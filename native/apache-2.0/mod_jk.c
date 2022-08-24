/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: Apache 2 plugin for Tomcat                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Henri Gomez <hgomez@apache.org>                            *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet related ramblings together.
 */

#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_file_info.h"
#include "apr_file_io.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "ap_mpm.h"

#if defined(AS400) && !defined(AS400_UTF8)
#include "ap_charset.h"
#include "util_charset.h"       /* ap_hdrs_from_ascii */
#endif

/* deprecated with apr 0.9.3 */

#include "apr_version.h"
#if (APR_MAJOR_VERSION == 0) && \
    (APR_MINOR_VERSION <= 9) && \
    (APR_PATCH_VERSION < 3)
#define apr_filepath_name_get apr_filename_of_pathname
#endif

#include "apr_strings.h"

/* Yes; sorta sucks - with luck we will clean this up before httpd-2.2
 * ships, leaving AP_NEED_SET_MUTEX_PERMS def'd as 1 or 0 on all platforms.
 */
#ifdef AP_NEED_SET_MUTEX_PERMS
# define JK_NEED_SET_MUTEX_PERMS AP_NEED_SET_MUTEX_PERMS
#else
  /* A special case for httpd-2.0 */
# if !defined(OS2) && !defined(WIN32) && !defined(BEOS) && !defined(AS400)
#  define JK_NEED_SET_MUTEX_PERMS 1
# else
#  define JK_NEED_SET_MUTEX_PERMS 0
# endif
#endif

#if JK_NEED_SET_MUTEX_PERMS
#include "unixd.h"      /* for unixd_set_global_mutex_perms */
#endif
/*
 * jk_ include files
 */
#include "jk_global.h"
#include "jk_ajp13.h"
#include "jk_logger.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_uri_worker_map.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_shm.h"
#include "jk_url.h"

#define JK_LOG_DEF_FILE             ("logs/mod_jk.log")
#define JK_SHM_DEF_FILE             ("logs/jk-runtime-status")
#define JK_ENV_REQUEST_ID           ("UNIQUE_ID")
#define JK_ENV_REMOTE_ADDR          ("JK_REMOTE_ADDR")
#define JK_ENV_REMOTE_PORT          ("JK_REMOTE_PORT")
#define JK_ENV_REMOTE_HOST          ("JK_REMOTE_HOST")
#define JK_ENV_REMOTE_USER          ("JK_REMOTE_USER")
#define JK_ENV_AUTH_TYPE            ("JK_AUTH_TYPE")
#define JK_ENV_LOCAL_NAME           ("JK_LOCAL_NAME")
#define JK_ENV_LOCAL_ADDR           ("JK_LOCAL_ADDR")
#define JK_ENV_LOCAL_PORT           ("JK_LOCAL_PORT")
#define JK_ENV_IGNORE_CL            ("JK_IGNORE_CL")
#define JK_ENV_HTTPS                ("HTTPS")
#define JK_ENV_SSL_PROTOCOL         ("SSL_PROTOCOL")
#define JK_ENV_CERTS                ("SSL_CLIENT_CERT")
#define JK_ENV_CIPHER               ("SSL_CIPHER")
#define JK_ENV_SESSION              ("SSL_SESSION_ID")
#define JK_ENV_KEY_SIZE             ("SSL_CIPHER_USEKEYSIZE")
#define JK_ENV_CERTCHAIN_PREFIX     ("SSL_CLIENT_CERT_CHAIN_")
#define JK_ENV_REPLY_TIMEOUT        ("JK_REPLY_TIMEOUT")
#define JK_ENV_STICKY_IGNORE        ("JK_STICKY_IGNORE")
#define JK_ENV_STATELESS            ("JK_STATELESS")
#define JK_ENV_ROUTE                ("JK_ROUTE")
#define JK_ENV_WORKER_NAME          ("JK_WORKER_NAME")
#define JK_NOTE_WORKER_NAME         ("JK_WORKER_NAME")
#define JK_NOTE_WORKER_TYPE         ("JK_WORKER_TYPE")
#define JK_NOTE_REQUEST_DURATION    ("JK_REQUEST_DURATION")
#define JK_NOTE_WORKER_ROUTE        ("JK_WORKER_ROUTE")
#define JK_HANDLER          ("jakarta-servlet")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet")
#define NULL_FOR_EMPTY(x)   ((x && !strlen(x)) ? NULL : x)
#define STRNULL_FOR_NULL(x) ((x) ? (x) : "(null)")
#define JK_LOG_LOCK_KEY     ("jk_log_lock_key")
#define JKLOG_MARK          __FILE__,__LINE__

/*
 * If you are not using SSL, comment out the following line. It will make
 * apache run faster.
 *
 * Personally, I (DM), think this may be a lie.
 */
#define ADD_SSL_INFO

/* Needed for Apache 2.3/2.4 per-module log config */
#ifdef APLOG_USE_MODULE
APLOG_USE_MODULE(jk);
#endif

/* module MODULE_VAR_EXPORT jk_module; */
AP_MODULE_DECLARE_DATA module jk_module;

/*
 * Environment variable forward object
 */
typedef struct
{
    int has_default;
    char *name;
    char *value;
} envvar_item;

/*
 * Configuration object for the mod_jk module.
 */
typedef struct
{

    /*
     * Log stuff
     */
    char *log_file;
    int log_level;
    jk_logger_t *log;

    /*
     * Mount stuff
     */
    char *mount_file;
    int mount_file_reload;
    jk_map_t *uri_to_context;

    int mountcopy;

    jk_uri_worker_map_t *uw_map;

    int was_initialized;

    /*
     * Automatic context path apache alias
     */
    char *alias_dir;

    /*
     * Request Logging
     */

    char *stamp_format_string;
    char *format_string;
    apr_array_header_t *format;

    /*
     * Setting target worker via environment
     */
    char *worker_indicator;

    /*
     * Configurable environment variables to overwrite
     * request information using meta data send by a
     * proxy in front of us.
     */
    char *request_id_indicator;
    char *remote_addr_indicator;
    char *remote_port_indicator;
    char *remote_host_indicator;
    char *remote_user_indicator;
    char *auth_type_indicator;
    char *local_name_indicator;
    char *local_addr_indicator;
    char *local_port_indicator;

    /*
     * Configurable environment variable to force
     * ignoring a request Content-Length header
     * (useful to make mod_deflate request inflation
     * compatible with mod_jk).
     */
    char *ignore_cl_indicator;

    /*
     * SSL Support
     */
    int ssl_enable;
    char *https_indicator;
    char *ssl_protocol_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;    /* Servlet API 2.3 requirement */
    char *key_size_indicator;   /* Servlet API 2.3 requirement */
    char *certchain_prefix;     /* Client certificate chain prefix */

    /*
     * Jk Options
     */
    int options;
    int exclude_options;

    int strip_session;
    char *strip_session_name;
    /*
     * Environment variables support
     */
    int envvars_has_own;
    apr_table_t *envvars;
    apr_table_t *envvars_def;
    apr_array_header_t *envvar_items;

    server_rec *s;
} jk_server_conf_t;

/*
 * Request specific configuration
 */
struct jk_request_conf
{
    rule_extension_t *rule_extensions;
    char *orig_uri;
    const char *request_id;
    int jk_handled;
};

typedef struct jk_request_conf jk_request_conf_t;

struct apache_private_data
{
    jk_pool_t p;

    int read_body_started;
    request_rec *r;
};
typedef struct apache_private_data apache_private_data_t;

static server_rec *main_server = NULL;
static jk_logger_t *main_log = NULL;
static apr_hash_t *jk_log_fps = NULL;
static jk_worker_env_t worker_env;
static apr_global_mutex_t *jk_log_lock = NULL;
static char *jk_shm_file = NULL;
static int jk_shm_size = 0;
static int jk_shm_size_set = 0;
static volatile int jk_watchdog_interval = 0;
static volatile int jk_watchdog_running  = 0;

/*
 * Worker stuff
*/
static jk_map_t *jk_worker_properties = NULL;
static char *jk_worker_file = NULL;
static int jk_mount_copy_all = JK_FALSE;

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned len, unsigned *actually_read);

static int init_jk(apr_pool_t * pconf, jk_server_conf_t * conf,
                    server_rec * s);

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l);

static void JK_METHOD ws_add_log_items(jk_ws_service_t *s,
                                       const char *const *log_names,
                                       const char *const *log_values,
                                       unsigned num_of_log_items);

static void * JK_METHOD ws_next_vhost(void *d);

static void JK_METHOD ws_vhost_to_text(void *d, char *buf, size_t len);

static jk_uri_worker_map_t * JK_METHOD ws_vhost_to_uw_map(void *d);

/* ========================================================================= */
/* JK Service step callbacks                                                 */
/* ========================================================================= */

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers)
{
    unsigned h;
    apache_private_data_t *p = s->ws_private;
    request_rec *r = p->r;
    jk_log_context_t *l = s->log_ctx;

    /* If we use proxy error pages, still pass
     * through context headers needed for special status codes.
     */
    if ((s->extension.use_server_error_pages &&
        status >= s->extension.use_server_error_pages) ||
	(!strcmp(s->method, "HEAD") && (status >= HTTP_BAD_REQUEST))) {
        if (status == HTTP_UNAUTHORIZED) {
            int found = JK_FALSE;
            for (h = 0; h < num_of_headers; h++) {
                if (!strcasecmp(header_names[h], "WWW-Authenticate")) {
                    char *tmp = apr_pstrdup(r->pool, header_values[h]);
                    apr_table_set(r->err_headers_out,
                                  "WWW-Authenticate", tmp);
                    found = JK_TRUE;
                }
            }
            if (found == JK_FALSE) {
                jk_log(l, JK_LOG_INFO,
                       "origin server sent 401 without"
                       " WWW-Authenticate header");
            }
        }
    }
    if (s->extension.use_server_error_pages &&
        status >= s->extension.use_server_error_pages) {
        return JK_TRUE;
    }

    /* If there is no reason given (or an empty one),
     * we'll try to guess a good one.
     */
    if (!reason || *reason == '\0') {
        /* We ask Apache httpd about a good reason phrase. */
        reason = ap_get_status_line(status);
        /* Unfortunately it returns with a 500 reason phrase,
         * whenever it does not know about the given status code,
         * e.g. in the case of custom status codes.
         */
        if (status != 500 && !strncmp(reason, "500 ", 4)) {
            reason = "Unknown Reason";
        }
        else {
            /* Apache httpd returns a full status line,
             * but we only want a reason phrase, so skip
             * the prepended status code.
             */
            reason = reason + 4;
        }
    }
    r->status = status;
    r->status_line = apr_psprintf(r->pool, "%d %s", status, reason);

    for (h = 0; h < num_of_headers; h++) {
        if (!strcasecmp(header_names[h], "Content-type")) {
            char *tmp = apr_pstrdup(r->pool, header_values[h]);
            ap_content_type_tolower(tmp);
            /* It should be done like this in Apache 2.0 */
            /* This way, Apache 2.0 will be able to set the output filter */
            /* and it make jk usable with deflate using */
            /* AddOutputFilterByType DEFLATE text/html */
            ap_set_content_type(r, tmp);
        }
        else if (!strcasecmp(header_names[h], "Location")) {
#if defined(AS400) && !defined(AS400_UTF8)
            /* Fix escapes in Location Header URL */
            ap_fixup_escapes((char *)header_values[h],
                             strlen(header_values[h]), ap_hdrs_from_ascii);
#endif
            apr_table_set(r->headers_out, header_names[h], header_values[h]);
        }
        else if (!strcasecmp(header_names[h], "Content-Length")) {
            ap_set_content_length(r, apr_atoi64(header_values[h]));
        }
        else if (!strcasecmp(header_names[h], "Transfer-Encoding")) {
            apr_table_set(r->headers_out, header_names[h], header_values[h]);
        }
        else if (!strcasecmp(header_names[h], "Last-Modified")) {
            /*
             * If the script gave us a Last-Modified header, we can't just
             * pass it on blindly because of restrictions on future values.
             */
            ap_update_mtime(r, apr_date_parse_http(header_values[h]));
            ap_set_last_modified(r);
        }
        else {
            apr_table_add(r->headers_out, header_names[h], header_values[h]);
        }
    }

    /* this NOP function was removed in apache 2.0 alpha14 */
    /* ap_send_http_header(r); */
    s->response_started = JK_TRUE;

    return JK_TRUE;
}

/*
 * Read a chunk of the request body into a buffer.  Attempt to read len
 * bytes into the buffer.  Write the number of bytes actually read into
 * actually_read.
 *
 * Think of this function as a method of the apache1.3-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */
static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned len, unsigned *actually_read)
{
    if (s && s->ws_private && b && actually_read) {
        apache_private_data_t *p = s->ws_private;
        if (!p->read_body_started) {
            if (ap_should_client_block(p->r)) {
                p->read_body_started = JK_TRUE;
            }
        }

        if (p->read_body_started) {
#if defined(AS400) && !defined(AS400_UTF8)
            int long rv = OK;
            if (rv = ap_change_request_body_xlate(p->r, 65535, 65535)) {        /* turn off request body translation */
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_CRIT, 0, NULL,
                             "mod_jk: Error on ap_change_request_body_xlate, rc=%d",
                             rv);
                return JK_FALSE;
            }
#else
            long rv;
#endif

            if ((rv = ap_get_client_block(p->r, b, len)) < 0) {
                return JK_FALSE;
            }
            else {
                *actually_read = (unsigned)rv;
            }
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

static void JK_METHOD ws_flush(jk_ws_service_t *s)
{
#if ! (defined(AS400) && !defined(AS400_UTF8))
    if (s && s->ws_private) {
        apache_private_data_t *p = s->ws_private;
        ap_rflush(p->r);
    }
#endif
}

static void JK_METHOD ws_done(jk_ws_service_t *s)
{
#if ! (defined(AS400) && !defined(AS400_UTF8))
    if (s && s->ws_private) {
        apache_private_data_t *p = s->ws_private;
        ap_finalize_request_protocol(p->r);
    }
#endif
}

/*
 * Write a chunk of response data back to the browser.  If the headers
 * haven't yet been sent over, send over default header values (Status =
 * 200, basically).
 *
 * Write len bytes from buffer b.
 *
 * Think of this function as a method of the apache1.3-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned int l)
{
#if defined(AS400) && !defined(AS400_UTF8)
    int rc;
#endif

    jk_log_context_t *log_ctx = s->log_ctx;

    if (s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if (l) {
            /* BUFF *bf = p->r->connection->client; */
            int r = 0;
            int ll = l;
            const char *bb = (const char *)b;

            if (!s->response_started) {
                jk_log(log_ctx, JK_LOG_INFO,
                       "Write without start, starting with defaults");
                if (!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }
            if (p->r->header_only) {
#if ! (defined(AS400) && !defined(AS400_UTF8))
                ap_rflush(p->r);
#endif
                return JK_TRUE;
            }
#if defined(AS400) && !defined(AS400_UTF8)
            /* turn off response body translation */
            rc = ap_change_response_body_xlate(p->r, 65535, 65535);
            if (rc) {
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_CRIT, 0, NULL,
                             "mod_jk: Error on ap_change_response_body_xlate, rc=%d",
                             rc);
                return JK_FALSE;
            }
#endif

            while (ll > 0 && !p->r->connection->aborted) {
                r = ap_rwrite(bb, ll, p->r);
                if (JK_IS_DEBUG_LEVEL(log_ctx))
                    jk_log(log_ctx, JK_LOG_DEBUG,
                           "written %d out of %d", r, ll);

                if (r < 0)
                    return JK_FALSE;
                ll -= r;
                bb += r;
            }
            if (ll && p->r->connection->aborted) {
                /* Fail if there is something left to send and
                 * the connection was aborted by the client
                 */
                return JK_FALSE;
            }
        }

        return JK_TRUE;
    }
    return JK_FALSE;
}

static void JK_METHOD ws_add_log_items(jk_ws_service_t *s,
                                       const char *const *log_names,
                                       const char *const *log_values,
                                       unsigned num_of_log_items)
{
    unsigned h;
    apache_private_data_t *p = s->ws_private;
    request_rec *r = p->r;

    for (h = 0; h < num_of_log_items; h++) {
        if (log_names[h] && log_values[h]) {
            apr_table_set(r->notes, log_names[h], log_values[h]);
        }
    }
}

static void * JK_METHOD ws_next_vhost(void *d)
{
    server_rec *s = (server_rec *)d;
    if (s == NULL)
        return main_server;
    return s->next;
}

static void JK_METHOD ws_vhost_to_text(void *d, char *buf, size_t len)
{
    server_rec *s = (server_rec *)d;
    size_t used = 0;

    if (s->server_hostname) {
        used += strlen(s->server_hostname);
    }
    if (!s->is_virtual) {
        if (s->port)
            used += strlen(":XXXXX");
    }
    else if (s->addrs) {
        used += strlen(" [");
        if (s->addrs->virthost)
            used += strlen(s->addrs->virthost);
        if (s->addrs->host_port)
            used += strlen(":XXXXX");
        used += strlen("]");
    }

    if (len < used && len > strlen("XXX")) {
        strcpy(buf, "XXX");
        return;
    }

    used = 0;

    if (s->server_hostname) {
        strcpy(buf + used, s->server_hostname);
        used += strlen(s->server_hostname);
    }
    if (!s->is_virtual) {
        if (s->port) {
            sprintf(buf + used, ":%hu", s->port);
            used = strlen(buf);
        }
    }
    else if (s->addrs) {
        strcpy(buf + used, " [");
        used += strlen(" [");
        if (s->addrs->virthost) {
            strcpy(buf + used, s->addrs->virthost);
            used += strlen(s->addrs->virthost);
        }
        if (s->addrs->host_port) {
            sprintf(buf + used, ":%hu", s->addrs->host_port);
            used = strlen(buf);
        }
        strcpy(buf + used, "]");
        used += strlen("]");
    }
}

static jk_uri_worker_map_t * JK_METHOD ws_vhost_to_uw_map(void *d)
{
    server_rec *s = (server_rec *)d;
    jk_server_conf_t *conf = NULL;
    if (s == NULL)
        return NULL;
    conf = (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                     &jk_module);
    return conf->uw_map;
}

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

static void dump_options(server_rec *srv, apr_pool_t *p)
{
    char server_name[80];
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    jk_server_conf_t *conf = (jk_server_conf_t *)ap_get_module_config(srv->module_config,
                                                                      &jk_module);
    int options = conf->options;

    l->logger = conf->log;
    l->id = "CONFIG";

    ws_vhost_to_text(srv, server_name, 80);
    if (options & JK_OPT_FWDURICOMPAT)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardURICompat", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDURICOMPAT ? " (default)" : "");
    if (options & JK_OPT_FWDURICOMPATUNPARSED)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardURICompatUnparsed", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDURICOMPATUNPARSED ? " (default)" : "");
    if (options & JK_OPT_FWDURIESCAPED)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardURIEscaped", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDURIESCAPED ? " (default)" : "");
    if (options & JK_OPT_FWDURIPROXY)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardURIProxy", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDURIPROXY ? " (default)" : "");
    if (options & JK_OPT_FWDDIRS)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardDirectories", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDDIRS ? " (default)" : "");
    if (options & JK_OPT_FWDLOCAL)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardLocalAddress", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDLOCAL ? " (default)" : "");
    if (options & JK_OPT_FWDPHYSICAL)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardPhysicalAddress", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDPHYSICAL ? " (default)" : "");
    if (options & JK_OPT_FWDCERTCHAIN)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardSSLCertChain", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDCERTCHAIN ? " (default)" : "");
    if (options & JK_OPT_FWDKEYSIZE)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "ForwardKeySize", server_name,
               JK_OPT_DEFAULT & JK_OPT_FWDKEYSIZE ? " (default)" : "");
    if (options & JK_OPT_FLUSHPACKETS)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "FlushPackets", server_name,
               JK_OPT_DEFAULT & JK_OPT_FLUSHPACKETS ? " (default)" : "");
    if (options & JK_OPT_FLUSHEADER)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "FlushHeader", server_name,
               JK_OPT_DEFAULT & JK_OPT_FLUSHEADER ? " (default)" : "");
    if (options & JK_OPT_DISABLEREUSE)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "DisableReuse", server_name,
               JK_OPT_DEFAULT & JK_OPT_DISABLEREUSE ? " (default)" : "");
    if (options & JK_OPT_REJECTUNSAFE)
        jk_log(l, JK_LOG_DEBUG, "JkOption '%s' set in server '%s'%s",
               "RejectUnsafeURI", server_name,
               JK_OPT_DEFAULT & JK_OPT_REJECTUNSAFE ? " (default)" : "");
}

/* ========================================================================= */
/* Log something to Jk log file then exit */
static void jk_error_exit(const char *file,
                          int line,
                          int level,
                          const server_rec * s,
                          apr_pool_t * p, const char *fmt, ...)
{
    va_list ap;
    char *res;
    char *ch;

    va_start(ap, fmt);
    res = apr_pvsprintf(s->process->pool, fmt, ap);
    va_end(ap);
    /* Replace all format characters in the resulting message */
    /* because we feed the message to ap_log_error(). */
    ch = res;
    while (*ch) {
        if (*ch == '%') {
            *ch = '#';
        }
        ch++;
    }

#if (MODULE_MAGIC_NUMBER_MAJOR >= 20100606)
    ap_log_error(file, line, APLOG_MODULE_INDEX, level, 0, s, "%s", res);
#else
    ap_log_error(file, line, level, 0, s, "%s", res);
#endif
    if (s) {
#if (MODULE_MAGIC_NUMBER_MAJOR >= 20100606)
        ap_log_error(file, line, APLOG_MODULE_INDEX, level, 0, NULL, "%s", res);
#else
        ap_log_error(file, line, level, 0, NULL, "%s", res);
#endif
    }

    /* Exit process */
    exit(1);
}

static jk_uint64_t get_content_length(request_rec * r)
{
    if (r->main == NULL || r->main == r) {
        char *lenp = (char *)apr_table_get(r->headers_in, "Content-Length");

        if (lenp) {
            jk_uint64_t rc = 0;
            if (sscanf(lenp, "%" JK_UINT64_T_FMT, &rc) > 0 && rc > 0) {
                return rc;
            }
        }
    }

    return 0;
}

/* Retrieve string value from env var, use default if env var does not exist. */
static const char *get_env_string(request_rec *r, const char *def,
                                  char *env, int null_for_empty)
{
    char *value = (char *)apr_table_get(r->subprocess_env, env);
    if (value)
        return null_for_empty ? NULL_FOR_EMPTY(value) : value;
    return null_for_empty ? NULL_FOR_EMPTY(def) : def;
}

/* Retrieve integer value from env var, use default if env var does not exist. */
static int get_env_int(request_rec *r, int def, char *env)
{
    char *value = (char *)apr_table_get(r->subprocess_env, env);
    if (value)
        return atoi(value);
    return def;
}

static int init_ws_service(apache_private_data_t * private_data,
                           jk_ws_service_t *s, jk_server_conf_t * conf)
{
    int size;
    request_rec *r = private_data->r;
    char *ssl_temp = NULL;
    char *uri = NULL;
    const char *reply_timeout = NULL;
    const char *sticky_ignore = NULL;
    const char *stateless = NULL;
    const char *route = NULL;
    rule_extension_t *e;
    jk_request_conf_t *rconf;
    jk_log_context_t *l = apr_pcalloc(r->pool, sizeof(jk_log_context_t));

    l->logger = conf->log;
    l->id = NULL;

    /* Copy in function pointers (which are really methods) */
    s->start_response = ws_start_response;
    s->read = ws_read;
    s->write = ws_write;
    s->flush = ws_flush;
    s->done = ws_done;
    s->add_log_items = ws_add_log_items;
    s->next_vhost = ws_next_vhost;
    s->vhost_to_text = ws_vhost_to_text;
    s->vhost_to_uw_map = ws_vhost_to_uw_map;

    rconf = (jk_request_conf_t *)ap_get_module_config(r->request_config, &jk_module);
    l->id = rconf->request_id;

    s->auth_type = get_env_string(r, r->ap_auth_type,
                                  conf->auth_type_indicator, 1);
    s->remote_user = get_env_string(r, r->user,
                                    conf->remote_user_indicator, 1);
    s->log_ctx = l;
    s->protocol = r->protocol;
    s->remote_host = (char *)ap_get_remote_host(r->connection,
                                                r->per_dir_config,
                                                REMOTE_HOST, NULL);
    s->remote_host = get_env_string(r, s->remote_host,
                                    conf->remote_host_indicator, 1);
    if (conf->options & JK_OPT_FWDLOCAL) {
        s->remote_addr = r->connection->local_ip;
        /* We don't know the client port of the backend connection. */
        s->remote_port = "0";
    }
    else {
#if (MODULE_MAGIC_NUMBER_MAJOR >= 20111130)
        if (conf->options & JK_OPT_FWDPHYSICAL) {
            s->remote_addr = r->connection->client_ip;
            s->remote_port = apr_itoa(r->pool, r->connection->client_addr->port);
        }
        else {
            s->remote_addr = r->useragent_ip;
            s->remote_port = apr_itoa(r->pool, r->useragent_addr->port);
        }
#else
        s->remote_addr = r->connection->remote_ip;
        s->remote_port = apr_itoa(r->pool, r->connection->remote_addr->port);
#endif
    }
    s->remote_addr = get_env_string(r, s->remote_addr,
                                    conf->remote_addr_indicator, 1);
    s->remote_port = get_env_string(r, s->remote_port,
                                    conf->remote_port_indicator, 1);

    if (conf->options & JK_OPT_FLUSHPACKETS)
        s->flush_packets = 1;
    if (conf->options & JK_OPT_FLUSHEADER)
        s->flush_header = 1;

    e = rconf->rule_extensions;
    if (e) {
        s->extension.reply_timeout = e->reply_timeout;
        s->extension.sticky_ignore = e->sticky_ignore;
        s->extension.stateless = e->stateless;
        s->extension.use_server_error_pages = e->use_server_error_pages;
        if (e->activation) {
            s->extension.activation = apr_palloc(r->pool, e->activation_size * sizeof(int));
            memcpy(s->extension.activation, e->activation, e->activation_size * sizeof(int));
        }
        if (e->fail_on_status_size > 0) {
            s->extension.fail_on_status_size = e->fail_on_status_size;
            s->extension.fail_on_status = apr_palloc(r->pool, e->fail_on_status_size * sizeof(int));
            memcpy(s->extension.fail_on_status, e->fail_on_status, e->fail_on_status_size * sizeof(int));
        }
        if (e->session_cookie) {
            s->extension.session_cookie = apr_pstrdup(r->pool, e->session_cookie);
        }
        if (e->session_path) {
            s->extension.session_path = apr_pstrdup(r->pool, e->session_path);
        }
        if (e->set_session_cookie) {
            s->extension.set_session_cookie = e->set_session_cookie;
        }
        if (e->session_cookie_path) {
            s->extension.session_cookie_path = apr_pstrdup(r->pool, e->session_cookie_path);
        }
    }
    reply_timeout = apr_table_get(r->subprocess_env, JK_ENV_REPLY_TIMEOUT);
    if (reply_timeout) {
        int r = atoi(reply_timeout);
        if (r >= 0)
            s->extension.reply_timeout = r;
    }

    sticky_ignore = apr_table_get(r->subprocess_env, JK_ENV_STICKY_IGNORE);
    if (sticky_ignore) {
        if (*sticky_ignore == '\0') {
            s->extension.sticky_ignore = JK_TRUE;
        }
        else {
            int r = atoi(sticky_ignore);
            if (r) {
                s->extension.sticky_ignore = JK_TRUE;
            }
            else {
                s->extension.sticky_ignore = JK_FALSE;
            }
        }
    }

    stateless = apr_table_get(r->subprocess_env, JK_ENV_STATELESS);
    if (stateless) {
        if (*stateless == '\0') {
            s->extension.stateless = JK_TRUE;
        }
        else {
            int r = atoi(stateless);
            if (r) {
                s->extension.stateless = JK_TRUE;
            }
            else {
                s->extension.stateless = JK_FALSE;
            }
        }
    }

    if (conf->options & JK_OPT_DISABLEREUSE)
        s->disable_reuse = 1;

    /* get route if known */
    route = apr_table_get(r->subprocess_env, JK_ENV_ROUTE);
    if (route && *route) {
        s->route = route;
    }

    /* get server name */
    s->server_name = get_env_string(r, (char *)ap_get_server_name(r),
                                    conf->local_name_indicator, 0);

    /* get the local IP address */
    s->local_addr = get_env_string(r, r->connection->local_ip,
                                   conf->local_addr_indicator, 0);

    /* get the real port (otherwise redirect failed) */
    /* XXX: use apache API for getting server port
     *
     * Pre 1.2.7 versions used:
     * s->server_port = r->connection->local_addr->port;
     */
    s->server_port = get_env_int(r, ap_get_server_port(r),
                                 conf->local_port_indicator);

#if ((AP_MODULE_MAGIC_AT_LEAST(20051115,4)) && !defined(API_COMPATIBILITY)) || (MODULE_MAGIC_NUMBER_MAJOR >= 20060905)
    s->server_software = (char *)ap_get_server_description();
#else
    s->server_software = (char *)ap_get_server_version();
#endif
    s->method = (char *)r->method;
    s->content_length = get_content_length(r);
    s->is_chunked = r->read_chunked;
    if (s->content_length > 0 &&
        get_env_string(r, NULL, conf->ignore_cl_indicator, 0) != NULL) {
        s->content_length = 0;
        s->is_chunked = 1;
    }
    s->no_more_chunks = 0;
#if defined(AS400) && !defined(AS400_UTF8)
    /* Get the query string that is not translated to EBCDIC  */
    s->query_string = ap_get_original_query_string(r);
#else
    s->query_string = r->args;
#endif

    /*
     * The 2.2 servlet spec errata says the uri from
     * HttpServletRequest.getRequestURI() should remain encoded.
     * [http://java.sun.com/products/servlet/errata_042700.html]
     *
     * We use JkOptions to determine which method to be used
     *
     * ap_escape_uri is the latest recommended but require
     *               some java decoding (in TC 3.3 rc2)
     *
     * unparsed_uri is used for strict compliance with spec and
     *              old Tomcat (3.2.3 for example)
     *
     * uri is use for compatibility with mod_rewrite with old Tomcats
     */

    uri = rconf->orig_uri ? rconf->orig_uri : r->uri;

    switch (conf->options & JK_OPT_FWDURIMASK) {

    case JK_OPT_FWDURICOMPATUNPARSED:
        s->req_uri = r->unparsed_uri;
        if (s->req_uri != NULL) {
            char *query_str = strchr(s->req_uri, '?');
            if (query_str != NULL) {
                *query_str = 0;
            }
        }

        break;

    case JK_OPT_FWDURICOMPAT:
        s->req_uri = uri;
        break;

    case JK_OPT_FWDURIPROXY:
        size = 3 * (int)strlen(uri) + 1;
        s->req_uri = apr_palloc(r->pool, size);
        jk_canonenc(uri, s->req_uri, size);
        break;

    case JK_OPT_FWDURIESCAPED:
        s->req_uri = ap_escape_uri(r->pool, uri);
        break;

    default:
        return JK_FALSE;
    }

    if (conf->ssl_enable || conf->envvars) {
        ap_add_common_vars(r);

        if (conf->ssl_enable) {
            ssl_temp =
                (char *)apr_table_get(r->subprocess_env,
                                      conf->https_indicator);
            if (ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl = JK_TRUE;
                s->ssl_cert =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->certs_indicator);

                if (conf->options & JK_OPT_FWDCERTCHAIN) {
                    const apr_array_header_t *t = apr_table_elts(r->subprocess_env);
                    if (t && t->nelts) {
                        int i;
                        const apr_table_entry_t *elts = (const apr_table_entry_t *) t->elts;
                        apr_array_header_t *certs = apr_array_make(r->pool, 1, sizeof(char *));
                        *(const char **)apr_array_push(certs) = s->ssl_cert;
                        for (i = 0; i < t->nelts; i++) {
                            if (!elts[i].key)
                                continue;
                            if (!strncasecmp(elts[i].key, conf->certchain_prefix,
                                             strlen(conf->certchain_prefix)))
                                *(const char **)apr_array_push(certs) = elts[i].val;
                        }
                        s->ssl_cert = apr_array_pstrcat(r->pool, certs, '\0');
                    }
                }

                if (s->ssl_cert) {
                    s->ssl_cert_len = (unsigned int)strlen(s->ssl_cert);
                    if (JK_IS_DEBUG_LEVEL(l)) {
                        jk_log(l, JK_LOG_DEBUG,
                               "SSL client certificate (%d bytes): %s",
                               s->ssl_cert_len, s->ssl_cert);
                    }
                }
                s->ssl_protocol =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->ssl_protocol_indicator);
                /* Servlet 2.3 API */
                s->ssl_cipher =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->cipher_indicator);
                s->ssl_session =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->session_indicator);

                if (conf->options & JK_OPT_FWDKEYSIZE) {
                    /* Servlet 2.3 API */
                    ssl_temp = (char *)apr_table_get(r->subprocess_env,
                                                     conf->
                                                     key_size_indicator);
                    if (ssl_temp)
                        s->ssl_key_size = atoi(ssl_temp);
                }


            }
        }

        if (conf->envvars) {
            const apr_array_header_t *t = conf->envvar_items;
            if (t && t->nelts) {
                int i;
                int j = 0;
                envvar_item *elts = (envvar_item *) t->elts;
                s->attributes_names = apr_palloc(r->pool,
                                                 sizeof(char *) * t->nelts);
                s->attributes_values = apr_palloc(r->pool,
                                                  sizeof(char *) * t->nelts);

                for (i = 0; i < t->nelts; i++) {
                    s->attributes_names[i - j] = elts[i].name;
                    s->attributes_values[i - j] =
                        (char *)apr_table_get(r->subprocess_env, elts[i].name);
                    if (!s->attributes_values[i - j]) {
                        if (elts[i].has_default) {
                            s->attributes_values[i - j] = elts[i].value;
                        }
                        else {
                            s->attributes_values[i - j] = "";
                            s->attributes_names[i - j] = "";
                            j++;
                        }
                    }
                }

                s->num_attributes = t->nelts - j;
            }
        }
    }

    if (r->headers_in && apr_table_elts(r->headers_in)) {
        int need_content_length_header = (!s->is_chunked
                                          && s->content_length ==
                                          0) ? JK_TRUE : JK_FALSE;
        const apr_array_header_t *t = apr_table_elts(r->headers_in);
        if (t && t->nelts) {
            int i;
            int off = 0;
            apr_table_entry_t *elts = (apr_table_entry_t *) t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            for (i = 0; i < t->nelts; i++) {
                char *hname = apr_pstrdup(r->pool, elts[i].key);
                if (!strcasecmp(hname, "content-length")) {
                    if (need_content_length_header) {
                        need_content_length_header = JK_FALSE;
                    } else if (s->is_chunked) {
                        s->num_headers--;
                        off++;
                        continue;
                    }
                }
                s->headers_values[i - off] = apr_pstrdup(r->pool, elts[i].val);
                s->headers_names[i - off] = hname;
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if (need_content_length_header) {
                s->headers_names[s->num_headers] = "content-length";
                s->headers_values[s->num_headers] = "0";
                s->num_headers++;
            }
        }
        /* Add a content-length = 0 header if needed. */
        else if (need_content_length_header) {
            s->headers_names = apr_palloc(r->pool, sizeof(char *));
            s->headers_values = apr_palloc(r->pool, sizeof(char *));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            s->headers_names[0] = "content-length";
            s->headers_values[0] = "0";
            s->num_headers++;
        }
    }
    s->uw_map = conf->uw_map;

    /* Dump all connection param so we can trace what's going to
     * the remote tomcat
     */
    if (JK_IS_DEBUG_LEVEL(l)) {
        jk_log(l, JK_LOG_DEBUG,
               "Service protocol=%s method=%s ssl=%s host=%s addr=%s name=%s port=%d auth=%s user=%s laddr=%s raddr=%s uaddr=%s uri=%s",
               STRNULL_FOR_NULL(s->protocol),
               STRNULL_FOR_NULL(s->method),
               s->is_ssl ? "true" : "false",
               STRNULL_FOR_NULL(s->remote_host),
               STRNULL_FOR_NULL(s->remote_addr),
               STRNULL_FOR_NULL(s->server_name),
               s->server_port,
               STRNULL_FOR_NULL(s->auth_type),
               STRNULL_FOR_NULL(s->remote_user),
               STRNULL_FOR_NULL(r->connection->local_ip),
#if (MODULE_MAGIC_NUMBER_MAJOR >= 20111130)
               STRNULL_FOR_NULL(r->connection->client_ip),
               STRNULL_FOR_NULL(r->useragent_ip),
#else
               STRNULL_FOR_NULL(r->connection->remote_ip),
               STRNULL_FOR_NULL(r->connection->remote_ip),
#endif
               STRNULL_FOR_NULL(s->req_uri));
    }

    return JK_TRUE;
}

/*
 * The JK module command processors
 *
 * The below are all installed so that Apache calls them while it is
 * processing its config files.  This allows configuration info to be
 * copied into a jk_server_conf_t object, which is then used for request
 * filtering/processing.
 *
 * See jk_cmds definition below for explanations of these options.
 */

/*
 * JkMountCopy directive handling
 *
 * JkMountCopy On/Off/All
 */

static const char *jk_set_mountcopy(cmd_parms * cmd,
                                    void *dummy, const char *mount_copy)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    if (! strcasecmp(mount_copy, "all")) {
        const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
        if (err_string != NULL) {
            return err_string;
        }
        jk_mount_copy_all = JK_TRUE;
    }
    else if (strcasecmp(mount_copy, "on") && strcasecmp(mount_copy, "off")) {
        return "JkMountCopy must be All, On or Off";
    }
    else {
        conf->mountcopy = strcasecmp(mount_copy, "off") ? JK_TRUE : JK_FALSE;
    }

    return NULL;
}

/*
 * JkMount directive handling
 *
 * JkMount URI(context) worker
 */

static const char *jk_mount_context(cmd_parms * cmd,
                                    void *dummy,
                                    const char *context,
                                    const char *worker)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    const char *c, *w;

    if (worker != NULL && cmd->path == NULL) {
        c = context;
        w = worker;
    }
    else if (worker == NULL && cmd->path != NULL) {
        c = cmd->path;
        w = context;
    }
    else {
        if (worker == NULL)
            return "JkMount needs a path when not defined in a location";
        else
            return "JkMount can not have a path when defined in a location";
    }

    if (c[0] != '/')
        return "JkMount context should start with /";

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkMount Memory error";
        }
    }
    /*
     * Add the new worker to the alias map.
     */
    jk_map_put(conf->uri_to_context, c, w, NULL);
    return NULL;
}

/*
 * JkUnMount directive handling
 *
 * JkUnMount URI(context) worker
 */

static const char *jk_unmount_context(cmd_parms * cmd,
                                      void *dummy,
                                      const char *context,
                                      const char *worker)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    char *uri;
    const char *c, *w;

    if (worker != NULL && cmd->path == NULL) {
        c = context;
        w = worker;
    }
    else if (worker == NULL && cmd->path != NULL) {
        c = cmd->path;
        w = context;
    }
    else {
        if (worker == NULL)
            return "JkUnMount needs a path when not defined in a location";
        else
            return "JkUnMount can not have a path when defined in a location";
    }

    if (c[0] != '/')
        return "JkUnMount context should start with /";

    uri = apr_pstrcat(cmd->temp_pool, "!", c, NULL);

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkUnMount Memory error";
        }
    }
    /*
     * Add the new worker to the alias map.
     */
    jk_map_put(conf->uri_to_context, uri, w, NULL);
    return NULL;
}


/*
 * JkWorkersFile Directive Handling
 *
 * JkWorkersFile file
 */

static const char *jk_set_worker_file(cmd_parms * cmd,
                                      void *dummy, const char *worker_file)
{
    const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err_string != NULL) {
        return err_string;
    }

    if (jk_worker_file != NULL)
        return "JkWorkersFile only allowed once";

    /* we need an absolute path (ap_server_root_relative does the ap_pstrdup) */
    jk_worker_file = ap_server_root_relative(cmd->pool, worker_file);

    if (jk_worker_file == NULL)
        return "JkWorkersFile file name invalid";

    if (jk_file_exists(jk_worker_file) != JK_TRUE)
        return "JkWorkersFile: Can't find the workers file specified";

    return NULL;
}

/*
 * JkMountFile Directive Handling
 *
 * JkMountFile file
 */

static const char *jk_set_mount_file(cmd_parms * cmd,
                                     void *dummy, const char *mount_file)
{
    server_rec *s = cmd->server;

    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolute path (ap_server_root_relative does the ap_pstrdup) */
    conf->mount_file = ap_server_root_relative(cmd->pool, mount_file);

    if (conf->mount_file == NULL)
        return "JkMountFile file name invalid";

    if (jk_file_exists(conf->mount_file) != JK_TRUE)
        return "JkMountFile: Can't find the mount file specified";

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkMountFile Memory error";
        }
    }

    return NULL;
}

/*
 * JkMountFileReload Directive Handling
 *
 * JkMountFileReload seconds
 */

static const char *jk_set_mount_file_reload(cmd_parms * cmd,
                                            void *dummy, const char *mount_file_reload)
{
    server_rec *s = cmd->server;
    int interval;

    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    interval = atoi(mount_file_reload);
    if (interval < 0) {
        interval = 0;
    }

    conf->mount_file_reload = interval;

    return NULL;
}

/*
 * JkWatchdogInterval Directive Handling
 *
 * JkWatchdogInterval seconds
 */

static const char *jk_set_watchdog_interval(cmd_parms * cmd,
                                            void *dummy, const char *watchdog_interval)
{
    const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err_string != NULL) {
        return err_string;
    }

#if APR_HAS_THREADS
    jk_watchdog_interval = atoi(watchdog_interval);
    if (jk_watchdog_interval < 0) {
        jk_watchdog_interval = 0;
    }
    return NULL;
#else
    return "JkWatchdogInterval: APR was compiled without threading support. Cannot create watchdog thread";
#endif
}

/*
 * JkLogFile Directive Handling
 *
 * JkLogFile file
 */

static const char *jk_set_log_file(cmd_parms * cmd,
                                   void *dummy, const char *log_file)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolute path */
    if (*log_file != '|')
        conf->log_file = ap_server_root_relative(cmd->pool, log_file);
    else
        conf->log_file = apr_pstrdup(cmd->pool, log_file);

    if (conf->log_file == NULL)
        return "JkLogFile file name invalid";

    return NULL;
}

/*
 * JkShmFile Directive Handling
 *
 * JkShmFile file
 */

static const char *jk_set_shm_file(cmd_parms * cmd,
                                   void *dummy, const char *shm_file)
{
    const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err_string != NULL) {
        return err_string;
    }

    /* we need an absolute path */
    jk_shm_file = ap_server_root_relative(cmd->pool, shm_file);
    if (jk_shm_file == NULL)
        return "JkShmFile file name invalid";

    return NULL;
}

/*
 * JkShmSize Directive Handling
 *
 * JkShmSize size in kilobytes
 */

static const char *jk_set_shm_size(cmd_parms * cmd,
                                   void *dummy, const char *shm_size)
{
    int sz = 0;
    const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err_string != NULL) {
        return err_string;
    }

    sz = atoi(shm_size) * 1024;
    if (sz < JK_SHM_MIN_SIZE)
        sz = JK_SHM_MIN_SIZE;
    else
        sz = JK_SHM_ALIGN(sz);
    jk_shm_size = sz;
    if (jk_shm_size)
        jk_shm_size_set = 1;
    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/error/emerg
 */

static const char *jk_set_log_level(cmd_parms * cmd,
                                    void *dummy, const char *log_level)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->log_level = jk_parse_log_level(log_level);

    return NULL;
}

/*
 * JkLogStampFormat Directive Handling
 *
 * JkLogStampFormat "[%a %b %d %H:%M:%S %Y] "
 */

static const char *jk_set_log_fmt(cmd_parms * cmd,
                                  void *dummy, const char *log_format)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->stamp_format_string = apr_pstrdup(cmd->pool, log_format);

    return NULL;
}


/*
 * JkAutoAlias Directive Handling
 *
 * JkAutoAlias application directory
 */

static const char *jk_set_auto_alias(cmd_parms * cmd,
                                     void *dummy, const char *directory)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->alias_dir = apr_pstrdup(cmd->pool, directory);

    if (conf->alias_dir == NULL)
        return "JkAutoAlias directory invalid";

    return NULL;
}

/*
 * JkStripSession directive handling
 *
 * JkStripSession On/Off [session path identifier]
 */

static const char *jk_set_strip_session(cmd_parms * cmd, void *dummy,
                                        const char *flag, const char *name)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    if (strcasecmp(flag, "on") && strcasecmp(flag, "off")) {
        return "JkStripSession must be On or Off";
    }
    else {
        conf->strip_session = strcasecmp(flag, "off") ? JK_TRUE : JK_FALSE;
    }

    /* Check for optional path value */
    if (name)
        conf->strip_session_name = apr_pstrdup(cmd->pool, name);
    else
        conf->strip_session_name = apr_pstrdup(cmd->pool, JK_PATH_SESSION_IDENTIFIER);

    return NULL;
}

/*****************************************************************
 *
 * Actually logging.
 */

typedef const char *(*item_key_func) (request_rec *, char *);

typedef struct
{
    item_key_func func;
    char *arg;
} request_log_format_item;

static const char *process_item(request_rec * r,
                                request_log_format_item * item)
{
    const char *cp;

    cp = (*item->func) (r, item->arg);
    return cp ? cp : "-";
}

static int request_log_transaction(request_rec * r)
{
    request_log_format_item *items;
    char *str, *s;
    int i;
    int len = 0;
    const char **strs;
    int *strl;
    jk_server_conf_t *conf;
    jk_request_conf_t *rconf;
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    apr_array_header_t *format;

    conf = (jk_server_conf_t *) ap_get_module_config(r->server->module_config,
                                                     &jk_module);
    format = conf->format;
    if (format == NULL) {
        return DECLINED;
    }
    rconf = (jk_request_conf_t *)ap_get_module_config(r->request_config,
                                                      &jk_module);
    if (rconf == NULL || rconf->jk_handled == JK_FALSE) {
        return DECLINED;
    }

    l->logger = conf->log;
    l->id = rconf->request_id;
    strs = apr_palloc(r->pool, sizeof(char *) * (format->nelts));
    strl = apr_palloc(r->pool, sizeof(int) * (format->nelts));
    items = (request_log_format_item *) format->elts;
    for (i = 0; i < format->nelts; ++i) {
        strs[i] = process_item(r, &items[i]);
    }
    for (i = 0; i < format->nelts; ++i) {
        len += strl[i] = (int)strlen(strs[i]);
    }
    str = apr_palloc(r->pool, len + 1);
    for (i = 0, s = str; i < format->nelts; ++i) {
        memcpy(s, strs[i], strl[i]);
        s += strl[i];
    }
    *s = 0;

    jk_log(l, JK_LOG_REQUEST, "%s", str);
    return OK;

}

/*****************************************************************
 *
 * Parsing the log format string
 */

static char *format_integer(apr_pool_t * p, int i)
{
    return apr_psprintf(p, "%d", i);
}

static char *pfmt(apr_pool_t * p, int i)
{
    if (i <= 0) {
        return "-";
    }
    else {
        return format_integer(p, i);
    }
}

static const char *constant_item(request_rec * dummy, char *stuff)
{
    return stuff;
}

static const char *log_worker_name(request_rec * r, char *a)
{
    return apr_table_get(r->notes, JK_NOTE_WORKER_NAME);
}

static const char *log_worker_route(request_rec * r, char *a)
{
    return apr_table_get(r->notes, JK_NOTE_WORKER_ROUTE);
}


static const char *log_request_duration(request_rec * r, char *a)
{
    return apr_table_get(r->notes, JK_NOTE_REQUEST_DURATION);
}

static const char *log_request_line(request_rec * r, char *a)
{
    /* NOTE: If the original request contained a password, we
     * re-write the request line here to contain XXXXXX instead:
     * (note the truncation before the protocol string for HTTP/0.9 requests)
     * (note also that r->the_request contains the unmodified request)
     */
    return (r->parsed_uri.password) ? apr_pstrcat(r->pool, r->method, " ",
                                                  apr_uri_unparse(r->pool,
                                                                  &r->
                                                                  parsed_uri,
                                                                  0),
                                                  r->
                                                  assbackwards ? NULL : " ",
                                                  r->protocol, NULL)
        : r->the_request;
}

/* These next two routines use the canonical name:port so that log
 * parsers don't need to duplicate all the vhost parsing crud.
 */
static const char *log_virtual_host(request_rec * r, char *a)
{
    return r->server->server_hostname;
}

static const char *log_server_port(request_rec * r, char *a)
{
    return apr_psprintf(r->pool, "%u",
                        r->server->port ? r->server->
                        port : ap_default_port(r));
}

/* This respects the setting of UseCanonicalName so that
 * the dynamic mass virtual hosting trick works better.
 */
static const char *log_server_name(request_rec * r, char *a)
{
    return ap_get_server_name(r);
}

static const char *log_request_uri(request_rec * r, char *a)
{
    return r->uri;
}
static const char *log_request_method(request_rec * r, char *a)
{
    return r->method;
}

static const char *log_request_protocol(request_rec * r, char *a)
{
    return r->protocol;
}
static const char *log_request_query(request_rec * r, char *a)
{
    return (r->args != NULL) ? apr_pstrcat(r->pool, "?", r->args, NULL)
        : "";
}
static const char *log_status(request_rec * r, char *a)
{
    return pfmt(r->pool, r->status);
}

static const char *clf_log_bytes_sent(request_rec * r, char *a)
{
    if (!r->sent_bodyct) {
        return "-";
    }
    else {
        return apr_off_t_toa(r->pool, r->bytes_sent);
    }
}

static const char *log_bytes_sent(request_rec * r, char *a)
{
    if (!r->sent_bodyct) {
        return "0";
    }
    else {
        return apr_psprintf(r->pool, "%" APR_OFF_T_FMT, r->bytes_sent);
    }
}

static struct log_item_list
{
    char ch;
    item_key_func func;
} log_item_keys[] = {
    { 'T', log_request_duration },
    { 'r', log_request_line },
    { 'U', log_request_uri },
    { 's', log_status },
    { 'b', clf_log_bytes_sent },
    { 'B', log_bytes_sent },
    { 'V', log_server_name },
    { 'v', log_virtual_host },
    { 'p', log_server_port },
    { 'H', log_request_protocol },
    { 'm', log_request_method },
    { 'q', log_request_query },
    { 'w', log_worker_name },
    { 'R', log_worker_route},
    { '\0', NULL }
};

static struct log_item_list *find_log_func(char k)
{
    int i;

    for (i = 0; log_item_keys[i].ch; ++i)
        if (k == log_item_keys[i].ch) {
            return &log_item_keys[i];
        }

    return NULL;
}

static char *parse_request_log_misc_string(apr_pool_t * p,
                                           request_log_format_item * it,
                                           const char **sa)
{
    const char *s;
    char *d;

    it->func = constant_item;

    s = *sa;
    while (*s && *s != '%') {
        s++;
    }
    /*
     * This might allocate a few chars extra if there's a backslash
     * escape in the format string.
     */
    it->arg = apr_palloc(p, s - *sa + 1);

    d = it->arg;
    s = *sa;
    while (*s && *s != '%') {
        if (*s != '\\') {
            *d++ = *s++;
        }
        else {
            s++;
            switch (*s) {
            case '\\':
                *d++ = '\\';
                s++;
                break;
            case 'n':
                *d++ = '\n';
                s++;
                break;
            case 't':
                *d++ = '\t';
                s++;
                break;
            default:
                /* copy verbatim */
                *d++ = '\\';
                /*
                 * Allow the loop to deal with this *s in the normal
                 * fashion so that it handles end of string etc.
                 * properly.
                 */
                break;
            }
        }
    }
    *d = '\0';

    *sa = s;
    return NULL;
}

static char *parse_request_log_item(apr_pool_t * p,
                                    request_log_format_item * it,
                                    const char **sa)
{
    const char *s = *sa;
    struct log_item_list *l;

    if (*s != '%') {
        return parse_request_log_misc_string(p, it, sa);
    }

    ++s;
    it->arg = "";               /* For safety's sake... */

    l = find_log_func(*s++);
    if (!l) {
        char dummy[2];

        dummy[0] = s[-1];
        dummy[1] = '\0';
        return apr_pstrcat(p, "Unrecognized JkRequestLogFormat directive %",
                           dummy, NULL);
    }
    it->func = l->func;
    *sa = s;
    return NULL;
}

static apr_array_header_t *parse_request_log_string(apr_pool_t * p,
                                                    const char *s,
                                                    const char **err)
{
    apr_array_header_t *a =
        apr_array_make(p, 0, sizeof(request_log_format_item));
    char *res;

    while (*s) {
        if ((res =
             parse_request_log_item(p,
                                    (request_log_format_item *)
                                    apr_array_push(a), &s))) {
            *err = res;
            return NULL;
        }
    }

    return a;
}

/*
 * JkRequestLogFormat Directive Handling
 *
 * JkRequestLogFormat format string
 *
 * %b - Bytes sent, excluding HTTP headers. In CLF format
 * %B - Bytes sent, excluding HTTP headers.
 * %H - The request protocol
 * %m - The request method
 * %p - The canonical Port of the server serving the request
 * %q - The query string (prepended with a ? if a query string exists,
 *      otherwise an empty string)
 * %r - First line of request
 * %s - request HTTP status code
 * %T - Request duration, elapsed time to handle request in seconds '.' micro seconds
 * %U - The URL path requested, not including any query string.
 * %v - The canonical ServerName of the server serving the request.
 * %V - The server name according to the UseCanonicalName setting.
 * %w - Tomcat worker name
 */

static const char *jk_set_request_log_format(cmd_parms * cmd,
                                             void *dummy, const char *format)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->format_string = apr_pstrdup(cmd->pool, format);

    return NULL;
}


/*
 * JkWorkerIndicator Directive Handling
 *
 * JkWorkerIndicator JkWorker
 */

static const char *jk_set_worker_indicator(cmd_parms * cmd,
                                           void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->worker_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * Directives Handling for setting various environment names
 * used to overwrite the following request information:
 * - request_id
 * - remote_addr
 * - remote_port
 * - remote_host
 * - remote_user
 * - auth_type
 * - server_name
 * - server_port
 */
static const char *jk_set_request_id_indicator(cmd_parms * cmd,
                                               void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->request_id_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_remote_addr_indicator(cmd_parms * cmd,
                                                void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->remote_addr_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_remote_port_indicator(cmd_parms * cmd,
                                                void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->remote_port_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_remote_host_indicator(cmd_parms * cmd,
                                                void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->remote_host_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_remote_user_indicator(cmd_parms * cmd,
                                                void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->remote_user_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_auth_type_indicator(cmd_parms * cmd,
                                              void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->auth_type_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_local_name_indicator(cmd_parms * cmd,
                                               void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->local_name_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_local_addr_indicator(cmd_parms * cmd,
                                               void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->local_addr_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_local_port_indicator(cmd_parms * cmd,
                                               void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->local_port_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

static const char *jk_set_ignore_cl_indicator(cmd_parms * cmd,
                                              void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config, &jk_module);
    conf->ignore_cl_indicator = apr_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkExtractSSL Directive Handling
 *
 * JkExtractSSL On/Off
 */

static const char *jk_set_enable_ssl(cmd_parms * cmd, void *dummy, int flag)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* Set up our value */
    conf->ssl_enable = flag ? JK_TRUE : JK_FALSE;

    return NULL;
}

/*
 * JkHTTPSIndicator Directive Handling
 *
 * JkHTTPSIndicator HTTPS
 */

static const char *jk_set_https_indicator(cmd_parms * cmd,
                                          void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->https_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkSSLPROTOCOLIndicator Directive Handling
 *
 * JkSSLPROTOCOLIndicator SSL_PROTOCOL
 */

static const char *jk_set_ssl_protocol_indicator(cmd_parms * cmd,
                                                 void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->ssl_protocol_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkCERTSIndicator Directive Handling
 *
 * JkCERTSIndicator SSL_CLIENT_CERT
 */

static const char *jk_set_certs_indicator(cmd_parms * cmd,
                                          void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->certs_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkCIPHERIndicator Directive Handling
 *
 * JkCIPHERIndicator SSL_CIPHER
 */

static const char *jk_set_cipher_indicator(cmd_parms * cmd,
                                           void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->cipher_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkCERTCHAINPrefix Directive Handling
 *
 * JkCERTCHAINPrefix SSL_CLIENT_CERT_CHAIN_
 */

static const char *jk_set_certchain_prefix(cmd_parms * cmd,
                                           void *dummy, const char *prefix)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->certchain_prefix = apr_pstrdup(cmd->pool, prefix);

    return NULL;
}

/*
 * JkSESSIONIndicator Directive Handling
 *
 * JkSESSIONIndicator SSL_SESSION_ID
 */

static const char *jk_set_session_indicator(cmd_parms * cmd,
                                            void *dummy,
                                            const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->session_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkKEYSIZEIndicator Directive Handling
 *
 * JkKEYSIZEIndicator SSL_CIPHER_USEKEYSIZE
 */

static const char *jk_set_key_size_indicator(cmd_parms * cmd,
                                             void *dummy,
                                             const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->key_size_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkOptions Directive Handling
 *
 *
 * +ForwardSSLKeySize        => Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2
 * -ForwardSSLKeySize        => Don't Forward SSL Key Size, will make mod_jk works with all TC release
 *  ForwardURICompat         => Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)
 *  ForwardURICompatUnparsed => Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)
 *  ForwardURIEscaped        => Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part
 *  ForwardDirectories       => Forward all directory requests with no index files to Tomcat
 * +ForwardSSLCertChain      => Forward SSL Cert Chain
 * -ForwardSSLCertChain      => Don't Forward SSL Cert Chain (default)
 */

static const char *jk_set_options(cmd_parms * cmd, void *dummy,
                                  const char *line)
{
    int opt = 0;
    int mask = 0;
    char action;
    char *w;

    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    while (line[0] != 0) {
        w = ap_getword_conf(cmd->pool, &line);
        action = 0;

        if (*w == '+' || *w == '-') {
            action = *(w++);
        }

        mask = 0;

        if (action == '-' &&
            (!strncasecmp(w, "ForwardURI", strlen("ForwardURI")))) {
            return apr_pstrcat(cmd->pool, "JkOptions: Illegal option '-", w,
                               "': option can not be disabled", NULL);
        }
        if (!strcasecmp(w, "ForwardURICompat")) {
            opt = JK_OPT_FWDURICOMPAT;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURICompatUnparsed")) {
            opt = JK_OPT_FWDURICOMPATUNPARSED;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURIEscaped")) {
            opt = JK_OPT_FWDURIESCAPED;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURIProxy")) {
            opt = JK_OPT_FWDURIPROXY;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "CollapseSlashesAll")) {
            opt = JK_OPT_COLLAPSEALL;
            mask = JK_OPT_COLLAPSEMASK;
        }
        else if (!strcasecmp(w, "CollapseSlashesNone")) {
            opt = JK_OPT_COLLAPSENONE;
            mask = JK_OPT_COLLAPSEMASK;
        }
        else if (!strcasecmp(w, "CollapseSlashesUnmount")) {
            opt = JK_OPT_COLLAPSEUNMOUNT;
            mask = JK_OPT_COLLAPSEMASK;
        }
        else if (!strcasecmp(w, "ForwardDirectories")) {
            opt = JK_OPT_FWDDIRS;
        }
        else if (!strcasecmp(w, "ForwardLocalAddress")) {
            opt = JK_OPT_FWDLOCAL;
            mask = JK_OPT_FWDADDRMASK;
        }
        else if (!strcasecmp(w, "ForwardPhysicalAddress")) {
            opt = JK_OPT_FWDPHYSICAL;
            mask = JK_OPT_FWDADDRMASK;
        }
        else if (!strcasecmp(w, "FlushPackets")) {
            opt = JK_OPT_FLUSHPACKETS;
        }
        else if (!strcasecmp(w, "FlushHeader")) {
            opt = JK_OPT_FLUSHEADER;
        }
        else if (!strcasecmp(w, "DisableReuse")) {
            opt = JK_OPT_DISABLEREUSE;
        }
        else if (!strcasecmp(w, "ForwardSSLCertChain")) {
            opt = JK_OPT_FWDCERTCHAIN;
        }
        else if (!strcasecmp(w, "ForwardKeySize")) {
            opt = JK_OPT_FWDKEYSIZE;
        }
        else if (!strcasecmp(w, "RejectUnsafeURI")) {
            opt = JK_OPT_REJECTUNSAFE;
        }
        else
            return apr_pstrcat(cmd->pool, "JkOptions: Illegal option '", w,
                               "'", NULL);

        conf->options &= ~mask;

        if (action == '-') {
            conf->exclude_options |= opt;
        }
        else if (action == '+') {
            conf->options |= opt;
        }
        else {                  /* for now +Opt == Opt */
            conf->options |= opt;
        }
    }
    return NULL;
}

/*
 * JkEnvVar Directive Handling
 *
 * JkEnvVar MYOWNDIR
 */

static const char *jk_add_env_var(cmd_parms * cmd,
                                  void *dummy,
                                  const char *env_name,
                                  const char *default_value)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->envvars_has_own = JK_TRUE;
    if (!conf->envvars) {
        conf->envvars      = apr_table_make(cmd->pool, 0);
        conf->envvars_def  = apr_table_make(cmd->pool, 0);
        conf->envvar_items = apr_array_make(cmd->pool, 0,
                                            sizeof(envvar_item));
    }

    /* env_name is mandatory, default_value is optional.
     * No value means send the attribute only, if the env var is set during runtime.
     */
    apr_table_setn(conf->envvars, env_name, default_value ? default_value : "");
    apr_table_setn(conf->envvars_def, env_name, default_value ? "1" : "0");

    return NULL;
}

/*
 * JkWorkerProperty Directive Handling
 *
 * JkWorkerProperty name=value
 */

static const char *jk_set_worker_property(cmd_parms * cmd,
                                          void *dummy,
                                          const char *line)
{
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    const char *err_string = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err_string != NULL) {
        return err_string;
    }

    l->logger = conf->log;
    l->id = "CONFIG";
    if (!jk_worker_properties)
        jk_map_alloc(&jk_worker_properties);
    if (jk_map_read_property(jk_worker_properties, NULL, line,
                             JK_MAP_HANDLE_DUPLICATES, l) == JK_FALSE)
        return apr_pstrcat(cmd->temp_pool, "Invalid JkWorkerProperty ", line, NULL);

    return NULL;
}

static const command_rec jk_cmds[] = {
    /*
     * JkWorkersFile specifies a full path to the location of the worker
     * properties file.
     *
     * This file defines the different workers used by apache to redirect
     * servlet requests.
     */
    AP_INIT_TAKE1("JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF,
                  "The name of a worker file for the Tomcat servlet containers"),

    /*
     * JkMountFile specifies a full path to the location of the
     * uriworker properties file.
     *
     * This file defines the different mapping for workers used by apache
     * to redirect servlet requests.
     */
    AP_INIT_TAKE1("JkMountFile", jk_set_mount_file, NULL, RSRC_CONF,
                  "The name of a mount file for the Tomcat servlet uri mapping"),

    /*
     * JkMountFileReload specifies the reload check interval for the
     * uriworker properties file.
     *
     * Default value is: JK_URIMAP_DEF_RELOAD
     */
    AP_INIT_TAKE1("JkMountFileReload", jk_set_mount_file_reload, NULL, RSRC_CONF,
                  "The reload check interval of the mount file"),

    /*
     * JkWatchdogInterval specifies the maintain interval for the
     * watchdog thread.
     *
     * Default value is: 0 meaning watchdog thread will not be created
     */
    AP_INIT_TAKE1("JkWatchdogInterval", jk_set_watchdog_interval, NULL, RSRC_CONF,
                  "The maintain interval of the watchdog thread"),

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    AP_INIT_TAKE12("JkMount", jk_mount_context, NULL, RSRC_CONF|ACCESS_CONF,
                   "A mount point from a context to a Tomcat worker"),

    /*
     * JkUnMount unmounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    AP_INIT_TAKE12("JkUnMount", jk_unmount_context, NULL, RSRC_CONF|ACCESS_CONF,
                   "A no mount point from a context to a Tomcat worker"),

    /*
     * JkMountCopy specifies if mod_jk should copy the mount points
     * from the main server to the virtual servers.
     */
    AP_INIT_TAKE1("JkMountCopy", jk_set_mountcopy, NULL, RSRC_CONF,
                  "Should the base server mounts be copied to the virtual server"),

    /*
     * JkStripSession specifies if mod_jk should strip the ;jsessionid
     * from the unmapped urls
     */
    AP_INIT_TAKE12("JkStripSession", jk_set_strip_session, NULL, RSRC_CONF,
                   "Should the server strip the jsessionid from unmapped URLs"),

    /*
     * JkLogFile & JkLogLevel specifies to where should the plugin log
     * its information and how much.
     * JkLogStampFormat specify the time-stamp to be used on log
     */
    AP_INIT_TAKE1("JkLogFile", jk_set_log_file, NULL, RSRC_CONF,
                  "Full path to the Tomcat module log file"),

    AP_INIT_TAKE1("JkShmFile", jk_set_shm_file, NULL, RSRC_CONF,
                  "Full path to the Tomcat module shared memory file"),

    AP_INIT_TAKE1("JkShmSize", jk_set_shm_size, NULL, RSRC_CONF,
                  "Size of the shared memory file in KBytes"),

    AP_INIT_TAKE1("JkLogLevel", jk_set_log_level, NULL, RSRC_CONF,
                  "The Tomcat module log level, can be debug, "
                  "info, error or emerg"),
    AP_INIT_TAKE1("JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF,
                  "The Tomcat module log format, follow strftime syntax"),
    AP_INIT_TAKE1("JkRequestLogFormat", jk_set_request_log_format, NULL,
                  RSRC_CONF,
                  "The mod_jk module request log format string"),

    /*
     * Automatically Alias webapp context directories into the Apache
     * document space.
     */
    AP_INIT_TAKE1("JkAutoAlias", jk_set_auto_alias, NULL, RSRC_CONF,
                  "The mod_jk module automatic context apache alias directory"),

    /*
     * Enable worker name to be set in an environment variable.
     * This way one can use LocationMatch together with mod_env,
     * mod_setenvif and mod_rewrite to set the target worker.
     * Use this in combination with SetHandler jakarta-servlet to
     * make mod_jk the handler for the request.
     *
     */
    AP_INIT_TAKE1("JkWorkerIndicator", jk_set_worker_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the worker name"),

    /*
     * Environment variables used to overwrite the following
     * request information which gets forwarded:
     * - request_id
     * - remote_addr
     * - remote_port
     * - remote_host
     * - remote_user
     * - auth_type
     * - server_name
     * - server_port
     */
    AP_INIT_TAKE1("JkRequestIdIndicator", jk_set_request_id_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the request id."),
    AP_INIT_TAKE1("JkRemoteAddrIndicator", jk_set_remote_addr_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the remote address"),
    AP_INIT_TAKE1("JkRemotePortIndicator", jk_set_remote_port_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the remote port"),
    AP_INIT_TAKE1("JkRemoteHostIndicator", jk_set_remote_host_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the remote host name"),
    AP_INIT_TAKE1("JkRemoteUserIndicator", jk_set_remote_user_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the remote user name"),
    AP_INIT_TAKE1("JkAuthTypeIndicator", jk_set_auth_type_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the type of authentication"),
    AP_INIT_TAKE1("JkLocalNameIndicator", jk_set_local_name_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the local name"),
    AP_INIT_TAKE1("JkLocalAddrIndicator", jk_set_local_addr_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the local IP address"),
    AP_INIT_TAKE1("JkLocalPortIndicator", jk_set_local_port_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the local port"),
    AP_INIT_TAKE1("JkIgnoreCLIndicator", jk_set_ignore_cl_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that forces to ignore a request "
                  "Content-Length header"),

    /*
     * Apache has multiple SSL modules (for example apache_ssl, stronghold
     * IHS ...). Each of these can have a different SSL environment names
     * The following properties let the administrator specify the envoiroment
     * variables names.
     *
     * HTTPS - indication for SSL
     * CERTS - Base64-Der-encoded client certificates.
     * CIPHER - A string specifing the ciphers suite in use.
     * KEYSIZE - Size of Key used in dialogue (#bits are secure)
     * SESSION - A string specifing the current SSL session.
     */
    AP_INIT_TAKE1("JkHTTPSIndicator", jk_set_https_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains SSL indication"),
    AP_INIT_TAKE1("JkSSLPROTOCOLIndicator", jk_set_ssl_protocol_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains the SSL protocol name"),
    AP_INIT_TAKE1("JkCERTSIndicator", jk_set_certs_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains SSL client certificates"),
    AP_INIT_TAKE1("JkCIPHERIndicator", jk_set_cipher_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL client cipher"),
    AP_INIT_TAKE1("JkSESSIONIndicator", jk_set_session_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL session"),
    AP_INIT_TAKE1("JkKEYSIZEIndicator", jk_set_key_size_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL key size in use"),
    AP_INIT_TAKE1("JkCERTCHAINPrefix", jk_set_certchain_prefix, NULL, RSRC_CONF,
                  "Name of the Apache environment (prefix) that contains SSL client chain certificates"),
    AP_INIT_FLAG("JkExtractSSL", jk_set_enable_ssl, NULL, RSRC_CONF,
                 "Turns on SSL processing and information gathering by mod_jk"),

    /*
     * Options to tune mod_jk configuration
     * for now we understand :
     * +ForwardSSLKeySize        => Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2
     * -ForwardSSLKeySize        => Don't Forward SSL Key Size, will make mod_jk works with all TC release
     *  ForwardURICompat         => Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)
     *  ForwardURICompatUnparsed => Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)
     *  ForwardURIEscaped        => Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part
     * +ForwardSSLCertChain      => Forward SSL certificate chain
     * -ForwardSSLCertChain      => Don't forward SSL certificate chain
     */
    AP_INIT_RAW_ARGS("JkOptions", jk_set_options, NULL, RSRC_CONF,
                     "Set one of more options to configure the mod_jk module"),

    /*
     * JkEnvVar let user defines envs var passed from WebServer to
     * Servlet Engine
     */
    AP_INIT_TAKE12("JkEnvVar", jk_add_env_var, NULL, RSRC_CONF,
                   "Adds a name of environment variable and an optional value "
                   "that should be sent to servlet-engine"),

    AP_INIT_RAW_ARGS("JkWorkerProperty", jk_set_worker_property,
                     NULL, RSRC_CONF,
                     "Set workers.properties formated directive"),

    {NULL}
};

/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Util - cleanup for all processes.
 */
static apr_status_t jk_cleanup_proc(void *data)
{
    /* If the main log is piped, we need to make sure
     * it is no longer used. The external log process
     * (e.g. rotatelogs) will be gone now and the pipe will
     * block, once the buffer gets full. NULLing
     * jklogfp makes logging switch to error log.
     */
    jk_logger_t *l = (jk_logger_t *)data;
    jk_log_context_t log_ctx;
    log_ctx.logger = l;
    log_ctx.id = "CLEANUP";
    if (l && l->logger_private) {
        jk_file_logger_t *p = l->logger_private;
        if (p && p->is_piped == JK_TRUE) {
            p->jklogfp = NULL;
            p->is_piped = JK_FALSE;
        }
    }
    jk_shm_close(&log_ctx);
    return APR_SUCCESS;
}

/** Util - cleanup for child processes.
 */
static apr_status_t jk_cleanup_child(void *data)
{
    /* If the main log is piped, we need to make sure
     * it is no longer used. The external log process
     * (e.g. rotatelogs) will be gone now and the pipe will
     * block, once the buffer gets full. NULLing
     * jklogfp makes logging switch to error log.
     */
    jk_logger_t *l = (jk_logger_t *)data;
    jk_log_context_t log_ctx;
    log_ctx.logger = l;
    log_ctx.id = "CLEANUP";
    if (l && l->logger_private) {
        jk_file_logger_t *p = l->logger_private;
        if (p && p->is_piped == JK_TRUE) {
            p->jklogfp = NULL;
            p->is_piped = JK_FALSE;
        }
    }
    /* Force the watchdog thread exit */
    if (jk_watchdog_interval > 0) {
        jk_watchdog_interval = 0;
        while (jk_watchdog_running)
            apr_sleep(apr_time_from_sec(1));
    }
    wc_shutdown(&log_ctx);
    return jk_cleanup_proc(data);
}

/** Main service method, called to forward a request to tomcat
 */
static int jk_handler(request_rec * r)
{
    const char *worker_name;
    jk_server_conf_t *xconf;
    jk_request_conf_t *rconf;
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    int rc, dmt = 1;
    int worker_name_extension = JK_FALSE;

    /* We do DIR_MAGIC_TYPE here to make sure TC gets all requests, even
     * if they are directory requests, in case there are no static files
     * visible to Apache and/or DirectoryIndex was not used. This is only
     * used when JkOptions has ForwardDirectories set. */
    /* Not for me, try next handler */
    if (strcmp(r->handler, JK_HANDLER)
        && (dmt = strcmp(r->handler, DIR_MAGIC_TYPE)))
        return DECLINED;

    xconf = (jk_server_conf_t *) ap_get_module_config(r->server->module_config,
                                                      &jk_module);
    l->logger = xconf->log;
    l->id = NULL;
    JK_TRACE_ENTER(l);
    if (apr_table_get(r->subprocess_env, "no-jk")) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Into handler no-jk env var detected for uri=%s, declined",
                   r->uri);

        JK_TRACE_EXIT(l);
        return DECLINED;
    }

    /* Was the option to forward directories to Tomcat set? */
    if (!dmt && !(xconf->options & JK_OPT_FWDDIRS)) {
        JK_TRACE_EXIT(l);
        return DECLINED;
    }

    rconf = (jk_request_conf_t *)ap_get_module_config(r->request_config, &jk_module);
    if (rconf == NULL) {
        rconf = apr_palloc(r->pool, sizeof(jk_request_conf_t));
        rconf->jk_handled = JK_FALSE;
        rconf->rule_extensions = NULL;
        rconf->orig_uri = NULL;
        rconf->request_id = get_env_string(r, NULL,
                                           xconf->request_id_indicator, 1);
        if (rconf->request_id == NULL) {
            rconf->request_id = get_env_string(r, NULL, JK_ENV_REQUEST_ID, 1);
        }
        ap_set_module_config(r->request_config, &jk_module, rconf);
    }
    l->id = rconf->request_id;

    worker_name = apr_table_get(r->notes, JK_NOTE_WORKER_NAME);

    if (worker_name == NULL) {
        /* we may be here because of a manual directive (that overrides
           translate and
           sets the handler directly). We still need to know the worker.
         */
        worker_name = apr_table_get(r->subprocess_env, xconf->worker_indicator);
        if (worker_name) {
          /* The JkWorkerIndicator environment variable has
           * been used to explicitly set the worker without JkMount.
           * This is useful in combination with LocationMatch or mod_rewrite.
           */
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Retrieved worker (%s) from env %s for %s",
                       worker_name, xconf->worker_indicator, r->uri);
            if (ap_strchr_c(worker_name, ';')) {
                rule_extension_t *e = apr_palloc(r->pool, sizeof(rule_extension_t));
                char *w = apr_pstrdup(r->pool, worker_name);
                worker_name_extension = JK_TRUE;
                parse_rule_extensions(w, e, l);
                worker_name = w;
                rconf->rule_extensions = e;
            }
        }
        else if (worker_env.num_of_workers == 1) {
          /** We have a single worker (the common case).
              (lb is a bit special, it should count as a single worker but
              I'm not sure how). We also have a manual config directive that
              explicitly give control to us. */
            worker_name = worker_env.worker_list[0];
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Single worker (%s) configuration for %s",
                       worker_name, r->uri);
        }
        else {
            if (!xconf->uw_map) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "missing uri map for %s:%s",
                           xconf->s->server_hostname ? xconf->s->server_hostname : "_default_",
                           r->uri);
            }
            else {
                rule_extension_t *e;
                char *clean_uri;
                clean_uri = apr_pstrdup(r->pool, r->uri);
                rc = jk_servlet_normalize(clean_uri, l);
                if (rc != 0) {
                    JK_TRACE_EXIT(l);
                    return HTTP_BAD_REQUEST;
                }

                worker_name = map_uri_to_worker_ext(xconf->uw_map, clean_uri,
                                                    NULL, &e, NULL, l);
                if (worker_name) {
                    rconf->rule_extensions = e;
                    rconf->orig_uri = r->uri;
                    r->uri = clean_uri;
                }
            }

            if (worker_name == NULL && worker_env.num_of_workers) {
                worker_name = worker_env.worker_list[0];
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Using first worker (%s) from %d workers for %s",
                           worker_name, worker_env.num_of_workers, r->uri);
            }
        }
        if (worker_name)
            apr_table_setn(r->notes, JK_NOTE_WORKER_NAME, worker_name);
    }

    if (JK_IS_DEBUG_LEVEL(l))
       jk_log(l, JK_LOG_DEBUG, "Into handler %s worker=%s"
              " r->proxyreq=%d",
              r->handler, STRNULL_FOR_NULL(worker_name), r->proxyreq);

    rconf->jk_handled = JK_TRUE;

    /* If this is a proxy request, we'll notify an error */
    if (r->proxyreq) {
        jk_log(l, JK_LOG_INFO, "Proxy request for worker=%s"
               " is not allowed",
               STRNULL_FOR_NULL(worker_name));
        JK_TRACE_EXIT(l);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if ((rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) != APR_SUCCESS) {
        JK_TRACE_EXIT(l);
        return rc;
    }

    if (worker_name) {
        jk_worker_t *worker = wc_get_worker_for_name(worker_name, l);

        /* If the remote client has aborted, just ignore the request */
        if (r->connection->aborted) {
            jk_log(l, JK_LOG_INFO, "Client connection aborted for"
                   " worker=%s",
                   worker_name);
            JK_TRACE_EXIT(l);
            return OK;
        }

        if (worker) {
            long micro, seconds;
            char *duration = NULL;
            apr_time_t rd;
            apr_time_t request_begin = 0;
            int is_error = HTTP_INTERNAL_SERVER_ERROR;
            int rc = JK_FALSE;
            apache_private_data_t private_data;
            jk_ws_service_t s;
            jk_pool_atom_t buf[SMALL_POOL_SIZE];
            jk_open_pool(&private_data.p, buf, sizeof(buf));

            private_data.read_body_started = JK_FALSE;
            private_data.r = r;

            if (worker_name_extension == JK_TRUE) {
                extension_fix(&private_data.p, worker_name,
                              rconf->rule_extensions, l);
            }

            /* Maintain will be done by watchdog thread */
            if (!jk_watchdog_interval)
                wc_maintain(l);
            jk_init_ws_service(&s);
            s.ws_private = &private_data;
            s.pool = &private_data.p;
            apr_table_setn(r->notes, JK_NOTE_WORKER_TYPE,
                           wc_get_name_for_type(worker->type, l));

            request_begin = apr_time_now();

            if (init_ws_service(&private_data, &s, xconf)) {
                jk_endpoint_t *end = NULL;

                /* Use per/thread pool (or "context") to reuse the
                   endpoint. It's a bit faster, but I don't know
                   how to deal with load balancing - but it's usefull for JNI
                 */

                /* worker->get_endpoint might fail if we are out of memory so check */
                /* and handle it */
                if (worker->get_endpoint(worker, &end, l)) {
                    rc = end->service(end, &s, l,
                                      &is_error);
                    end->done(&end, l);
                    if ((s.content_read < s.content_length ||
                        (s.is_chunked && !s.no_more_chunks)) &&
                        /* This case aborts the connection below and typically
                         * means the request body reading already timed out,
                         * so lets not try to read again here. */
                        !(rc == JK_CLIENT_ERROR && is_error == HTTP_BAD_REQUEST)) {
                        /*
                         * If the servlet engine didn't consume all of the
                         * request data, consume and discard all further
                         * characters left to read from client
                         */
                        char *buff = apr_palloc(r->pool, 2048);
                        int consumed = 0;
                        if (buff != NULL) {
                            int rd;
                            while ((rd =
                                    ap_get_client_block(r, buff, 2048)) > 0) {
                                s.content_read += rd;
                                consumed += rd;
                            }
                        }
                        if (JK_IS_DEBUG_LEVEL(l)) {
                           jk_log(l, JK_LOG_DEBUG,
                                  "Consumed %d bytes of remaining request data for worker=%s",
                                  consumed, STRNULL_FOR_NULL(worker_name));
                        }
                    }
                }
                else {            /* this means we couldn't get an endpoint */
                    jk_log(l, JK_LOG_ERROR, "Could not get endpoint"
                           " for worker=%s",
                           worker_name);
                    rc = 0;       /* just to make sure that we know we've failed */
                    is_error = HTTP_SERVICE_UNAVAILABLE;
                }
            }
            else {
                jk_log(l, JK_LOG_ERROR, "Could not init service"
                       " for worker=%s",
                       worker_name);
                jk_close_pool(&private_data.p);
                JK_TRACE_EXIT(l);
                return HTTP_INTERNAL_SERVER_ERROR;
            }
            rd = apr_time_now() - request_begin;
            seconds = (long)apr_time_sec(rd);
            micro = (long)(rd - apr_time_from_sec(seconds));

            duration = apr_psprintf(r->pool, "%.1ld.%.6ld", seconds, micro);
            apr_table_setn(r->notes, JK_NOTE_REQUEST_DURATION, duration);
            if (s.route && *s.route)
                apr_table_set(r->notes, JK_NOTE_WORKER_ROUTE, s.route);

            jk_close_pool(&private_data.p);

            if (rc > 0) {
                if (s.extension.use_server_error_pages &&
                    s.http_response_status >= s.extension.use_server_error_pages) {
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG, "Forwarding status=%d"
                               " for worker=%s",
                               s.http_response_status, worker_name);
                    JK_TRACE_EXIT(l);
                    return s.http_response_status;
                }
                /* If tomcat returned no body and the status is not OK,
                   let apache handle the error code */

                if (!r->sent_bodyct && r->status >= HTTP_BAD_REQUEST) {
                    jk_log(l, JK_LOG_INFO, "No body with status=%d"
                           " for worker=%s",
                           r->status, worker_name);
                    JK_TRACE_EXIT(l);
                    return r->status;
                }
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG, "Service finished"
                           " with status=%d for worker=%s",
                           r->status, worker_name);
                JK_TRACE_EXIT(l);
                return OK;      /* NOT r->status, even if it has changed. */
            }
            else if (rc == JK_CLIENT_ERROR) {
                if (is_error != HTTP_REQUEST_ENTITY_TOO_LARGE)
                    r->connection->aborted = 1;
                jk_log(l, JK_LOG_INFO, "Aborting connection"
                       " for worker=%s",
                       worker_name);
                JK_TRACE_EXIT(l);
                return is_error;
            }
            else {
                jk_log(l, JK_LOG_INFO, "Service error=%d"
                       " for worker=%s",
                       rc, worker_name);
                JK_TRACE_EXIT(l);
                return is_error;
            }
        }
        else {
            jk_log(l, JK_LOG_INFO, "Could not find a worker"
                   " for worker name=%s",
                   worker_name);
            JK_TRACE_EXIT(l);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    rconf->jk_handled = JK_FALSE;
    JK_TRACE_EXIT(l);
    return DECLINED;
}

/** Standard apache hook, cleanup jk
 */
static apr_status_t jk_apr_pool_cleanup(void *data)
{
    server_rec *s = data;

    if (jk_worker_properties) {
        jk_map_free(&jk_worker_properties);
        jk_worker_properties = NULL;
        jk_worker_file = NULL;
        jk_mount_copy_all = JK_FALSE;
    }

    while (NULL != s) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                      &jk_module);

        if (conf && conf->was_initialized == JK_TRUE) {
            /* On pool cleanup pass NULL for the jk_logger to
               prevent segmentation faults on Windows because
               we can't guarantee what order pools get cleaned
               up between APR implementations. */
            wc_close(NULL);
            if (conf->uri_to_context) {
                jk_map_free(&conf->uri_to_context);
                /* We cannot have allocated uw_map
                 * unless we've allocated uri_to_context
                 */
                if (conf->uw_map)
                    uri_worker_map_free(&conf->uw_map, NULL);
            }
            conf->was_initialized = JK_FALSE;
        }
        s = s->next;
    }
    return APR_SUCCESS;
}

/** Create default jk_config. XXX This is mostly server-independent,
    all servers are using something similar - should go to common.
 */
static void *create_jk_config(apr_pool_t * p, server_rec * s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) apr_pcalloc(p, sizeof(jk_server_conf_t));

    c->was_initialized = JK_FALSE;

    if (s->is_virtual) {
        c->mountcopy = JK_UNSET;
        c->mount_file_reload = JK_UNSET;
        c->log_level = JK_UNSET;
        c->ssl_enable = JK_UNSET;
        c->strip_session = JK_UNSET;
    }
    else {
        if (!jk_map_alloc(&(c->uri_to_context))) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, "Memory error");
        }
        c->mountcopy = JK_FALSE;
        c->mount_file_reload = JK_URIMAP_DEF_RELOAD;
        c->log_level = JK_LOG_DEF_LEVEL;
        c->options = JK_OPT_DEFAULT;
        c->worker_indicator = JK_ENV_WORKER_NAME;

        /*
         * Configurable environment variables to overwrite
         * request information using meta data send by a
         * proxy in front of us.
         */
        c->request_id_indicator = JK_ENV_REQUEST_ID;
        c->remote_addr_indicator = JK_ENV_REMOTE_ADDR;
        c->remote_port_indicator = JK_ENV_REMOTE_PORT;
        c->remote_host_indicator = JK_ENV_REMOTE_HOST;
        c->remote_user_indicator = JK_ENV_REMOTE_USER;
        c->auth_type_indicator = JK_ENV_AUTH_TYPE;
        c->local_name_indicator = JK_ENV_LOCAL_NAME;
        c->local_addr_indicator = JK_ENV_LOCAL_ADDR;
        c->local_port_indicator = JK_ENV_LOCAL_PORT;

        c->ignore_cl_indicator = JK_ENV_IGNORE_CL;

        /*
         * By default we will try to gather SSL info.
         * Disable this functionality through JkExtractSSL
         */
        c->ssl_enable = JK_TRUE;
        /*
         * The defaults ssl indicators match those in mod_ssl (seems
         * to be in more use).
         */
        c->https_indicator = JK_ENV_HTTPS;
        c->ssl_protocol_indicator = JK_ENV_SSL_PROTOCOL;
        c->certs_indicator = JK_ENV_CERTS;
        c->cipher_indicator = JK_ENV_CIPHER;
        c->certchain_prefix = JK_ENV_CERTCHAIN_PREFIX;
        c->session_indicator = JK_ENV_SESSION;
        c->key_size_indicator = JK_ENV_KEY_SIZE;
        c->strip_session = JK_FALSE;
    }
    c->envvars_has_own = JK_FALSE;

    c->s = s;
    apr_pool_cleanup_register(p, s, jk_apr_pool_cleanup, apr_pool_cleanup_null);
    return c;
}


/*
 * Utility - copy items from apr table src to dst,
 * for keys that exist in src but not in dst.
 */
static void merge_apr_table(apr_table_t *src, apr_table_t *dst)
{
    int i;
    const apr_array_header_t *arr;
    const apr_table_entry_t *elts;

    arr = apr_table_elts(src);
    elts = (const apr_table_entry_t *)arr->elts;
    for (i = 0; i < arr->nelts; ++i) {
        if (!apr_table_get(dst, elts[i].key)) {
            apr_table_setn(dst, elts[i].key, elts[i].val);
        }
    }
}


/** Standard apache callback, merge jk options specified in <Directory>
    context or <Host>.
 */
static void *merge_jk_config(apr_pool_t * p, void *basev, void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *) overridesv;
    int mask = 0;

    if (!overrides->log_file)
        overrides->log_file = base->log_file;
    if (overrides->log_level == JK_UNSET)
        overrides->log_level = base->log_level;

    if (!overrides->stamp_format_string)
        overrides->stamp_format_string = base->stamp_format_string;
    if (!overrides->format_string)
        overrides->format_string = base->format_string;

    if (!overrides->worker_indicator)
        overrides->worker_indicator = base->worker_indicator;

    if (!overrides->request_id_indicator)
        overrides->request_id_indicator = base->request_id_indicator;
    if (!overrides->remote_addr_indicator)
        overrides->remote_addr_indicator = base->remote_addr_indicator;
    if (!overrides->remote_port_indicator)
        overrides->remote_port_indicator = base->remote_port_indicator;
    if (!overrides->remote_host_indicator)
        overrides->remote_host_indicator = base->remote_host_indicator;
    if (!overrides->remote_user_indicator)
        overrides->remote_user_indicator = base->remote_user_indicator;
    if (!overrides->auth_type_indicator)
        overrides->auth_type_indicator = base->auth_type_indicator;
    if (!overrides->local_name_indicator)
        overrides->local_name_indicator = base->local_name_indicator;
    if (!overrides->local_port_indicator)
        overrides->local_port_indicator = base->local_port_indicator;

    if (!overrides->ignore_cl_indicator)
        overrides->ignore_cl_indicator = base->ignore_cl_indicator;

    if (overrides->ssl_enable == JK_UNSET)
        overrides->ssl_enable = base->ssl_enable;
    if (!overrides->https_indicator)
        overrides->https_indicator = base->https_indicator;
    if (!overrides->ssl_protocol_indicator)
        overrides->ssl_protocol_indicator = base->ssl_protocol_indicator;
    if (!overrides->certs_indicator)
        overrides->certs_indicator = base->certs_indicator;
    if (!overrides->cipher_indicator)
        overrides->cipher_indicator = base->cipher_indicator;
    if (!overrides->certchain_prefix)
        overrides->certchain_prefix = base->certchain_prefix;
    if (!overrides->session_indicator)
        overrides->session_indicator = base->session_indicator;
    if (!overrides->key_size_indicator)
        overrides->key_size_indicator = base->key_size_indicator;

/* Don't simply accumulate bits in the JK_OPT_FWDURIMASK or
 * JK_OPT_COLLAPSEMASK region, because those are multi-bit values. */
    if (overrides->options & JK_OPT_FWDURIMASK)
        mask |= JK_OPT_FWDURIMASK;
    if (overrides->options & JK_OPT_COLLAPSEMASK)
        mask |= JK_OPT_COLLAPSEMASK;
    overrides->options |= (base->options & ~base->exclude_options) & ~mask;

    if (base->envvars) {
        if (overrides->envvars && overrides->envvars_has_own) {
/* merge_apr_table() preserves existing entries in overrides table */
            merge_apr_table(base->envvars, overrides->envvars);
            merge_apr_table(base->envvars_def, overrides->envvars_def);
        }
        else {
            overrides->envvars = base->envvars;
            overrides->envvars_def = base->envvars_def;
            overrides->envvar_items = base->envvar_items;
        }
    }

    if (overrides->mountcopy == JK_UNSET && jk_mount_copy_all == JK_TRUE) {
        overrides->mountcopy = JK_TRUE;
    }
    if (overrides->uri_to_context && overrides->mountcopy == JK_TRUE) {
/* jk_map_copy() preserves existing entries in overrides map */
        if (jk_map_copy(base->uri_to_context, overrides->uri_to_context) == JK_FALSE) {
                jk_error_exit(JKLOG_MARK, APLOG_EMERG, overrides->s, p, "Memory error");
        }
        if (!overrides->mount_file)
            overrides->mount_file = base->mount_file;
    }
    if (overrides->mountcopy == JK_TRUE) {
        if (!overrides->alias_dir)
            overrides->alias_dir = base->alias_dir;
    }
    if (overrides->mount_file_reload == JK_UNSET)
        overrides->mount_file_reload = base->mount_file_reload;
    if (overrides->strip_session == JK_UNSET) {
        overrides->strip_session = base->strip_session;
        overrides->strip_session_name = base->strip_session_name;
    }
    return overrides;
}

static int JK_METHOD jk_log_to_file(jk_logger_t *l, int level,
                                    int used, char *what)
{
    if (l &&
        (l->level <= level || level == JK_LOG_REQUEST_LEVEL) &&
        l->logger_private && what && used > 0) {
        jk_file_logger_t *p = l->logger_private;
        if (p->jklogfp) {
            apr_status_t rv;
            apr_size_t wrote;
#if defined(WIN32)
            what[used++] = '\r';
#endif
            what[used++] = '\n';
            wrote = used;
            rv = apr_global_mutex_lock(jk_log_lock);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ERR, rv, NULL,
                             "apr_global_mutex_lock(jk_log_lock) failed");
                /* XXX: Maybe this should be fatal? */
            }
            rv = apr_file_write(p->jklogfp, what, &wrote);
            if (rv != APR_SUCCESS) {
                char error[256];
                apr_strerror(rv, error, 254);
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                             "mod_jk: jk_log_to_file %.*s failed: %s",
                             used, what, error);
            }
            rv = apr_global_mutex_unlock(jk_log_lock);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ERR, rv, NULL,
                             "apr_global_mutex_unlock(jk_log_lock) failed");
                /* XXX: Maybe this should be fatal? */
            }
        }
        else {
            /* Can't use mod_jk log any more, log to error log instead.
             * Choose APLOG_ERR, since we already checked above, that if
             * the mod_jk log file would still be open, we would have
             * actually logged the message there
             */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                         "%.*s", used, what);
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

/*
** +-------------------------------------------------------+
** |                                                       |
** |              jk logfile support                       |
** |                                                       |
** +-------------------------------------------------------+
*/

static apr_status_t jklog_cleanup(void *d)
{
    /* hgomez@20070425 */
    /* Clean up pointer content */
    if (d != NULL)
        *(jk_logger_t **)d = NULL;

    return APR_SUCCESS;
}

static int open_jklog(server_rec * s, apr_pool_t * p)
{
    jk_server_conf_t *conf;
    const char *fname;
    apr_status_t rc;
    apr_file_t *jklogfp;
    piped_log *pl;
    jk_logger_t *jkl;
    jk_file_logger_t *flp;
    int jklog_flags = (APR_WRITE | APR_APPEND | APR_CREATE);
    apr_fileperms_t jklog_mode =
        (APR_UREAD | APR_UWRITE | APR_GREAD | APR_WREAD);

    conf = ap_get_module_config(s->module_config, &jk_module);

    if (conf->log_file == NULL) {
        conf->log_file = ap_server_root_relative(p, JK_LOG_DEF_FILE);
        if (conf->log_file)
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                         "No JkLogFile defined in httpd.conf. "
                         "Using default %s", conf->log_file);
    }
    if (*(conf->log_file) == '\0') {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_EBADPATH, s,
                     "mod_jk: Invalid JkLogFile EMPTY");
        conf->log = main_log;
        return 0;
    }

    jklogfp = apr_hash_get(jk_log_fps, conf->log_file, APR_HASH_KEY_STRING);
    if (!jklogfp) {
        if (*conf->log_file == '|') {
            if ((pl = ap_open_piped_log(p, conf->log_file + 1)) == NULL) {
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                             "mod_jk: could not open reliable pipe "
                             "to jk log %s", conf->log_file + 1);
                return -1;
            }
            jklogfp = (void *)ap_piped_log_write_fd(pl);
        }
        else {
            fname = ap_server_root_relative(p, conf->log_file);
            if (!fname) {
                ap_log_error(APLOG_MARK, APLOG_ERR, APR_EBADPATH, s,
                             "mod_jk: Invalid JkLog " "path %s", conf->log_file);
                return -1;
            }
            if ((rc = apr_file_open(&jklogfp, fname,
                                    jklog_flags, jklog_mode, p))
                != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ERR, rc, s,
                             "mod_jk: could not open JkLog " "file %s", fname);
                return -1;
            }
        }
        apr_file_inherit_set(jklogfp);
        apr_hash_set(jk_log_fps, conf->log_file, APR_HASH_KEY_STRING, jklogfp);
    }
    jkl = (jk_logger_t *)apr_palloc(p, sizeof(jk_logger_t));
    flp = (jk_file_logger_t *) apr_palloc(p, sizeof(jk_file_logger_t));
    if (jkl && flp) {
        jkl->log = jk_log_to_file;
        jkl->level = conf->log_level;
        jkl->logger_private = flp;
        flp->jklogfp = jklogfp;
        if (*conf->log_file == '|')
            flp->is_piped = JK_TRUE;
        else
            flp->is_piped = JK_FALSE;
        conf->log = jkl;
        jk_set_time_fmt(conf->log, conf->stamp_format_string);
        if (main_log == NULL) {
            main_log = conf->log;

            /* hgomez@20070425 */
            /* Shouldn't we clean both conf->log and main_log ?                   */
            /* Also should we pass pointer (ie: main_log) or handle (*main_log) ? */
            apr_pool_cleanup_register(p, &main_log,
                                      jklog_cleanup,
                                      apr_pool_cleanup_null);
        }

        return 0;
    }

    return -1;
}

#if APR_HAS_THREADS

static void * APR_THREAD_FUNC jk_watchdog_func(apr_thread_t *thd, void *data)
{
    int i;
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    jk_server_conf_t *conf = (jk_server_conf_t *)data;
    l->logger = conf->log;
    l->id = "WATCHDOG";

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Watchdog thread initialized with %d second interval",
               jk_watchdog_interval);
    for (;;) {
        for (i = 0; i < (jk_watchdog_interval * 10); i++) {
            /* apr_sleep() uses microseconds */
            apr_sleep((apr_time_t)(100000));
            if (!jk_watchdog_interval)
                break;
        }
        if (!jk_watchdog_interval)
            break;
        if (JK_IS_DEBUG_LEVEL(l))
           jk_log(l, JK_LOG_DEBUG,
                  "Watchdog thread running");
        jk_watchdog_running = 1;
        wc_maintain(l);
        if (!jk_watchdog_interval)
            break;
    }
    jk_watchdog_running = 0;
    return NULL;
}

#endif

/** Standard apache callback, initialize jk.
 */
static void jk_child_init(apr_pool_t * pconf, server_rec * s)
{
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    jk_server_conf_t *conf;
    apr_status_t rv;
    int rc;
    apr_thread_t *wdt;

    conf = ap_get_module_config(s->module_config, &jk_module);
    l->logger = conf->log;
    l->id = "CHILD_INIT";

    rv = apr_global_mutex_child_init(&jk_log_lock, NULL, pconf);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                     "mod_jk: could not init JK log lock in child");
    }

    JK_TRACE_ENTER(l);

    if (jk_watchdog_interval) {
#if APR_HAS_THREADS
        rv = apr_thread_create(&wdt, NULL, jk_watchdog_func, conf, pconf);
        if (rv != APR_SUCCESS) {
            jk_log(l, JK_LOG_ERROR,
                   "Could not init watchdog thread, error=%d", rv);
            jk_watchdog_interval = 0;
        }
        apr_thread_detach(wdt);
#endif
    }

    if ((rc = jk_shm_attach(jk_shm_file, jk_shm_size, l)) == 0) {
        apr_pool_cleanup_register(pconf, l->logger, jk_cleanup_child,
                                  apr_pool_cleanup_null);
    }
    else {
        jk_log(l, JK_LOG_ERROR, "Attaching shm:%s errno=%d",
               jk_shm_name(), rc);
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "Initialized %s", JK_FULL_EXPOSED_VERSION);
    JK_TRACE_EXIT(l);
}

/** Initialize jk, using worker.properties.
    We also use apache commands (JkWorker, etc), but this use is
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.

    Apache config will only be used for manual override, using
    SetHandler and normal apache directives (but minimal jk-specific
    stuff)
*/
static int init_jk(apr_pool_t * pconf, jk_server_conf_t * conf,
                    server_rec * s)
{
    int rc;
    int is_threaded;
    int mpm_threads = 1;
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    l->logger = conf->log;
    l->id = "JK_INIT";

    if (!jk_worker_properties)
        jk_map_alloc(&jk_worker_properties);
    jk_map_put(jk_worker_properties, "ServerRoot", ap_server_root, NULL);

    /* Set default connection cache size for multi-threaded MPMs */
    if (ap_mpm_query(AP_MPMQ_IS_THREADED, &is_threaded) == APR_SUCCESS &&
        is_threaded != AP_MPMQ_NOT_SUPPORTED) {
        if (ap_mpm_query(AP_MPMQ_MAX_THREADS, &mpm_threads) != APR_SUCCESS)
            mpm_threads = 1;
    }
    if (mpm_threads > 1) {
#if _MT_CODE
        /* _MT_CODE  */
        if (JK_IS_DEBUG_LEVEL(l)) {
#if !defined(WIN32)
#if USE_FLOCK_LK
            jk_log(l, JK_LOG_DEBUG,
                   "Using flock() for locking.");
#else
            jk_log(l, JK_LOG_DEBUG,
                   "Using fcntl() for locking.");
#endif /* USE_FLOCK_LK */
#else  /* WIN32 */
            jk_log(l, JK_LOG_DEBUG,
                   "Not using locking.");
#endif
        }
#else
        /* !_MT_CODE */
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "Cannot run prefork mod_jk on threaded server.");
        return JK_FALSE;
#endif
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Setting default connection pool max size to %d",
               mpm_threads);
    jk_set_worker_def_cache_size(mpm_threads);

    if ((jk_worker_file != NULL) &&
        !jk_map_read_properties(jk_worker_properties, NULL, jk_worker_file, NULL,
                                JK_MAP_HANDLE_DUPLICATES, l)) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "Error in reading worker properties from '%s'",
                     jk_worker_file);
        return JK_FALSE;
    }

    if (jk_map_resolve_references(jk_worker_properties, "worker.",
                                  1, 1, l) == JK_FALSE) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "Error in resolving configuration references");
        return JK_FALSE;
    }

#if !defined(WIN32)
    if (!jk_shm_file) {
        jk_shm_file = ap_server_root_relative(pconf, JK_SHM_DEF_FILE);
        if (jk_shm_file)
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                         "No JkShmFile defined in httpd.conf. "
                         "Using default %s", jk_shm_file);
    }
#endif
    if (jk_shm_size == 0) {
        jk_shm_size = jk_shm_calculate_size(jk_worker_properties, l);
    }
    else if (jk_shm_size_set) {
        jk_log(l, JK_LOG_WARNING,
               "The optimal shared memory size can now be determined automatically.");
        jk_log(l, JK_LOG_WARNING,
               "You can remove the JkShmSize directive if you want to use the optimal size.");
    }
    if ((rc = jk_shm_open(jk_shm_file, jk_shm_size, l)) == 0) {
        apr_pool_cleanup_register(pconf, l->logger,
                                  jk_cleanup_proc,
                                  apr_pool_cleanup_null);
    }
    else {
        jk_error_exit(JKLOG_MARK, APLOG_EMERG, s, s->process->pool,
                      "Initializing shm:%s errno=%d. Unable to start due to shared memory failure.",
                      jk_shm_name(), rc);
    }
    /* we add the URI->WORKER MAP since workers using AJP14
       will feed it */
    worker_env.uri_to_worker = conf->uw_map;
    worker_env.virtual = "*";   /* for now */
#if ((AP_MODULE_MAGIC_AT_LEAST(20051115,4)) && !defined(API_COMPATIBILITY)) || (MODULE_MAGIC_NUMBER_MAJOR >= 20060905)
    worker_env.server_name = (char *)ap_get_server_description();
#else
    worker_env.server_name = (char *)ap_get_server_version();
#endif
    worker_env.pool = pconf;

    if (wc_open(jk_worker_properties, &worker_env, l)) {
        ap_add_version_component(pconf, JK_EXPOSED_VERSION);
        jk_log(l, JK_LOG_INFO,
               "%s initialized",
               JK_FULL_EXPOSED_VERSION);
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "Error in creating the workers."
                     " Please consult your mod_jk log file '%s'.", conf->log_file);
        return JK_FALSE;
    }
    return JK_TRUE;
}

static int jk_post_config(apr_pool_t * pconf,
                          apr_pool_t * plog,
                          apr_pool_t * ptemp, server_rec * s)
{
    apr_status_t rv;
    jk_server_conf_t *conf;
    jk_log_context_t log_ctx;
    jk_log_context_t *l = &log_ctx;
    server_rec *srv = s;
    const char *err_string = NULL;
    int remain;
    void *data = NULL;

    l->logger = NULL;
    l->id = "POST_CONFIG";
    remain = jk_check_buffer_size();
    if (remain < 0) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s,
                     "mod_jk: JK_MAX_ATTRIBUTE_NAME_LEN in jk_util.c is too small, "
                     "increase by %d", -1 * remain);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    apr_pool_userdata_get(&data, JK_LOG_LOCK_KEY, s->process->pool);
    if (data == NULL) {
        /* create the jk log lockfiles in the parent */
        if ((rv = apr_global_mutex_create(&jk_log_lock, NULL,
                                          APR_LOCK_DEFAULT,
                                          s->process->pool)) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                         "mod_jk: could not create jk_log_lock");
            return HTTP_INTERNAL_SERVER_ERROR;
        }

#if JK_NEED_SET_MUTEX_PERMS
#if (MODULE_MAGIC_NUMBER_MAJOR >= 20090208)
        rv = ap_unixd_set_global_mutex_perms(jk_log_lock);
#else
        rv = unixd_set_global_mutex_perms(jk_log_lock);
#endif
        if (rv != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                         "mod_jk: Could not set permissions on "
                         "jk_log_lock; check User and Group directives");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
#endif
        apr_pool_userdata_set((const void *)jk_log_lock, JK_LOG_LOCK_KEY,
                              apr_pool_cleanup_null, s->process->pool);
    }
    else {
        jk_log_lock = (apr_global_mutex_t *)data;
    }

    main_server = s;
    jk_log_fps = apr_hash_make(pconf);

    if (!s->is_virtual) {
        conf = (jk_server_conf_t *)ap_get_module_config(s->module_config,
                                                        &jk_module);
        l->logger = conf->log;
        if (conf->was_initialized == JK_FALSE) {
            /* step through the servers and open each jk logfile
             * and do additional post config initialization.
             */
            for (; srv; srv = srv->next) {
                jk_server_conf_t *sconf = (jk_server_conf_t *)ap_get_module_config(srv->module_config,
                                                                                   &jk_module);

/*
 * If a virtual server contains no JK directive, httpd shares
 * the config structure. But we don't want to share some settings
 * by default, especially the JkMount rules.
 * Therefore we check, if this config structure really belongs to this
 * vhost, otherwise we create a new one and merge.
 */
                if (sconf && sconf->s != srv) {
                    jk_server_conf_t *srvconf = (jk_server_conf_t *)create_jk_config(pconf, srv);
                    sconf = (jk_server_conf_t *)merge_jk_config(pconf, sconf, srvconf);
                    ap_set_module_config(srv->module_config, &jk_module, sconf);
                }

                if (sconf && sconf->was_initialized == JK_FALSE) {
                    sconf->was_initialized = JK_TRUE;
                    if (open_jklog(srv, pconf))
                        return HTTP_INTERNAL_SERVER_ERROR;
                    l->logger = sconf->log;
                    sconf->options &= ~sconf->exclude_options;
                    dump_options(srv, pconf);
                    if (sconf->uri_to_context) {
                        if (!uri_worker_map_alloc(&(sconf->uw_map),
                                                  sconf->uri_to_context, l))
                            jk_error_exit(JKLOG_MARK, APLOG_EMERG, srv,
                                          srv->process->pool, "Memory error");
                        if (sconf->options & JK_OPT_REJECTUNSAFE)
                            sconf->uw_map->reject_unsafe = 1;
                        else
                            sconf->uw_map->reject_unsafe = 0;
                        if (sconf->mount_file) {
                            sconf->uw_map->fname = sconf->mount_file;
                            sconf->uw_map->reload = sconf->mount_file_reload;
                            uri_worker_map_switch(sconf->uw_map, l);
                            uri_worker_map_load(sconf->uw_map, l);
                        }
                        if (sconf->options & JK_OPT_COLLAPSEMASK) {
                            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                                         "Deprecated CollapseSlashes setting ignored");
                        }
                    }
                    else {
                        if (sconf->mountcopy == JK_TRUE) {
                            sconf->uw_map = conf->uw_map;
                        }
                    }
                    if (sconf->format_string) {
                        sconf->format =
                            parse_request_log_string(pconf, sconf->format_string, &err_string);
                        if (sconf->format == NULL)
                            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                                         "JkRequestLogFormat format array NULL");
                    }
                    if (sconf->envvars && sconf->envvars_has_own) {
                        int i;
                        const apr_array_header_t *arr;
                        const apr_table_entry_t *elts;
                        envvar_item *item;
                        const char *envvar_def;

                        arr = apr_table_elts(sconf->envvars);
                        if (arr) {
                            elts = (const apr_table_entry_t *)arr->elts;
                            for (i = 0; i < arr->nelts; ++i) {
                                item = (envvar_item *)apr_array_push(sconf->envvar_items);
                                if (!item)
                                    return HTTP_INTERNAL_SERVER_ERROR;
                                item->name = elts[i].key;
                                envvar_def = apr_table_get(sconf->envvars_def, elts[i].key);
                                if (envvar_def && !strcmp("1", envvar_def)) {
                                    item->value = elts[i].val;
                                    item->has_default = 1;
                                }
                                else {
                                    item->value = "";
                                    item->has_default = 0;
                                }
                            }
                        }
                    }
                }
            }
            conf->was_initialized = JK_TRUE;
            l->logger = conf->log;
            if (init_jk(pconf, conf, s) == JK_FALSE)
                return HTTP_INTERNAL_SERVER_ERROR;
            if (conf->uw_map) {
                uri_worker_map_ext(conf->uw_map, l);
                uri_worker_map_switch(conf->uw_map, l);
            }
            for (srv = s; srv; srv = srv->next) {
                jk_server_conf_t *sconf = (jk_server_conf_t *)ap_get_module_config(srv->module_config,
                                                                                   &jk_module);
                l->logger = sconf->log;
                if (conf->uw_map != sconf->uw_map && sconf->uw_map) {
                    uri_worker_map_ext(sconf->uw_map, l);
                    uri_worker_map_switch(sconf->uw_map, l);
                }
            }

        }
    }

    return OK;
}

/** Use the internal mod_jk mappings to find if this is a request for
 *    tomcat and what worker to use.
 */
static int jk_translate(request_rec * r)
{
    jk_request_conf_t *rconf = apr_palloc(r->pool, sizeof(jk_request_conf_t));
    rconf->jk_handled = JK_FALSE;
    rconf->rule_extensions = NULL;
    rconf->orig_uri = NULL;
    ap_set_module_config(r->request_config, &jk_module, rconf);

    if (!r->proxyreq) {
        jk_log_context_t log_ctx;
        jk_log_context_t *l = &log_ctx;
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(r->server->
                                                      module_config,
                                                      &jk_module);
        l->logger = conf->log;
        l->id = "JK_TRANSLATE";

        if (conf) {
            char *clean_uri;
            int rc;
            const char *worker;

            JK_TRACE_ENTER(l);

            rconf->request_id = get_env_string(r, NULL,
                                               conf->request_id_indicator, 1);
            if (rconf->request_id == NULL) {
                rconf->request_id = get_env_string(r, NULL, JK_ENV_REQUEST_ID, 1);
            }
            l->id = rconf->request_id;
            if ((r->handler != NULL) && (!strcmp(r->handler, JK_HANDLER))) {
                /* Somebody already set the handler, probably manual config
                 * or "native" configuration, no need for extra overhead
                 */
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Manually mapped, no need to call uri_to_worker");
                JK_TRACE_EXIT(l);
                return DECLINED;
            }

            if (apr_table_get(r->subprocess_env, "no-jk")) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Into translate no-jk env var detected for uri=%s, declined",
                           r->uri);

                JK_TRACE_EXIT(l);
                return DECLINED;
            }

            clean_uri = apr_pstrdup(r->pool, r->uri);
            rc = jk_servlet_normalize(clean_uri, l);
            if (rc != 0) {
                JK_TRACE_EXIT(l);
                return HTTP_BAD_REQUEST;
            }

            /* Special case to make sure that apache can serve a directory
               listing if there are no matches for the DirectoryIndex and
               Tomcat webapps are mapped into apache using JkAutoAlias. */
            if (r->main != NULL && r->main->handler != NULL &&
                (conf->alias_dir != NULL) &&
                !strcmp(r->main->handler, DIR_MAGIC_TYPE)) {

                /* Append the request uri to the JkAutoAlias directory and
                   determine if the file exists. */
                apr_finfo_t finfo;
                finfo.filetype = APR_NOFILE;
                /* Map uri to a context static file */
                if (strlen(clean_uri) > 1) {
                    char *context_path = NULL;

                    context_path = apr_pstrcat(r->pool, conf->alias_dir, clean_uri, NULL);
                    if (context_path != NULL) {
                        apr_stat(&finfo, context_path, APR_FINFO_TYPE,
                                 r->pool);
                    }
                }
                if (finfo.filetype != APR_REG) {
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "JkAutoAlias, no DirectoryIndex file for URI %s",
                               r->uri);
                    JK_TRACE_EXIT(l);
                    return DECLINED;
                }
            }
            if (!conf->uw_map) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "missing uri map for %s:%s",
                           conf->s->server_hostname ? conf->s->server_hostname : "_default_",
                           r->uri);
                JK_TRACE_EXIT(l);
                return DECLINED;
            }
            else {
                rule_extension_t *e;
                worker = map_uri_to_worker_ext(conf->uw_map, clean_uri,
                                               NULL, &e, NULL, l);
                if (worker) {
                    rconf->rule_extensions = e;
                    rconf->orig_uri = r->uri;
                    r->uri = clean_uri;
                }
            }

            if (worker) {
                r->handler = apr_pstrdup(r->pool, JK_HANDLER);
                apr_table_setn(r->notes, JK_NOTE_WORKER_NAME, worker);

                /* This could be a sub-request, possibly from mod_dir */
                /* Also add the the HANDLER to the main request */
                if (r->main) {
                    r->main->handler = apr_pstrdup(r->main->pool, JK_HANDLER);
                    apr_table_setn(r->main->notes, JK_NOTE_WORKER_NAME, worker);
                }

                JK_TRACE_EXIT(l);
                return OK;
            }
            else if (conf->alias_dir != NULL) {
                /* Automatically map uri to a context static file */
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "check alias_dir: %s",
                           conf->alias_dir);
                if (strlen(clean_uri) > 1) {
                    /* Get the context directory name */
                    char *context_dir = NULL;
                    char *context_path = NULL;
                    char *child_dir = NULL;
                    char *index = clean_uri;
                    char *suffix = strchr(index + 1, '/');
                    if (suffix != NULL) {
                        int size = (int)(suffix - index);
                        context_dir = apr_pstrndup(r->pool, index, size);
                        /* Get the context child directory name */
                        index = index + size + 1;
                        suffix = strchr(index, '/');
                        if (suffix != NULL) {
                            size = (int)(suffix - index);
                            child_dir = apr_pstrndup(r->pool, index, size);
                        }
                        else {
                            child_dir = index;
                        }
                        /* Deny access to WEB-INF and META-INF directories */
                        if (child_dir != NULL) {
                            if (JK_IS_DEBUG_LEVEL(l))
                                jk_log(l, JK_LOG_DEBUG,
                                       "AutoAlias child_dir: %s",
                                       child_dir);
                            if (!strcasecmp(child_dir, "WEB-INF")
                                || !strcasecmp(child_dir, "META-INF")) {
                                if (JK_IS_DEBUG_LEVEL(l))
                                    jk_log(l, JK_LOG_DEBUG,
                                           "AutoAlias HTTP_NOT_FOUND for URI: %s",
                                           r->uri);
                                JK_TRACE_EXIT(l);
                                return HTTP_NOT_FOUND;
                            }
                        }
                    }
                    else {
                        context_dir = apr_pstrdup(r->pool, index);
                    }

                    context_path = apr_pstrcat(r->pool, conf->alias_dir, context_dir, NULL);
                    if (context_path != NULL) {
                        apr_finfo_t finfo;
                        finfo.filetype = APR_NOFILE;
                        apr_stat(&finfo, context_path, APR_FINFO_TYPE,
                                 r->pool);
                        if (finfo.filetype == APR_DIR) {
                            char *ret = apr_pstrcat(r->pool, conf->alias_dir, clean_uri, NULL);
                            /* Add code to verify real path ap_os_canonical_name */
                            if (ret != NULL) {
                                if (JK_IS_DEBUG_LEVEL(l))
                                    jk_log(l, JK_LOG_DEBUG,
                                           "AutoAlias OK for file: %s",
                                           ret);
                                r->filename = ret;
                                JK_TRACE_EXIT(l);
                                return OK;
                            }
                        }
                        else {
                            /* Deny access to war files in web app directory */
                            int size = (int)strlen(context_dir);
                            if (size > 4
                                && !strcasecmp(context_dir + (size - 4),
                                               ".war")) {
                                if (JK_IS_DEBUG_LEVEL(l))
                                    jk_log(l, JK_LOG_DEBUG,
                                           "AutoAlias HTTP_FORBIDDEN for URI: %s",
                                           r->uri);
                                JK_TRACE_EXIT(l);
                                return HTTP_FORBIDDEN;
                            }
                        }
                    }
                }
            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "no match for %s found",
                           r->uri);
            }
        }
    }

    return DECLINED;
}

/* bypass the directory_walk and file_walk for non-file requests */
static int jk_map_to_storage(request_rec * r)
{

    jk_server_conf_t *conf;
    jk_request_conf_t *rconf = ap_get_module_config(r->request_config, &jk_module);
    if (rconf == NULL) {
        rconf = apr_palloc(r->pool, sizeof(jk_request_conf_t));
        rconf->jk_handled = JK_FALSE;
        rconf->rule_extensions = NULL;
        rconf->orig_uri = NULL;
        ap_set_module_config(r->request_config, &jk_module, rconf);
    }

    if (!r->proxyreq) {
        conf =
            (jk_server_conf_t *) ap_get_module_config(r->server->
                                                      module_config,
                                                      &jk_module);
        if (conf) {
            rconf->request_id = get_env_string(r, NULL,
                                               conf->request_id_indicator, 1);
            if (rconf->request_id == NULL) {
                rconf->request_id = get_env_string(r, NULL, JK_ENV_REQUEST_ID, 1);
            }
        }
    }

    if (!r->proxyreq && !apr_table_get(r->notes, JK_NOTE_WORKER_NAME)) {
        if (conf) {
            char *clean_uri;
            int rc;
            const char *worker;
            jk_log_context_t log_ctx;
            jk_log_context_t *l = &log_ctx;
            l->logger = conf->log;
            l->id = rconf->request_id;

            JK_TRACE_ENTER(l);

            if ((r->handler != NULL) && (!strcmp(r->handler, JK_HANDLER))) {
                /* Somebody already set the handler, probably manual config
                 * or "native" configuration, no need for extra overhead
                 */
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Manually mapped, no need to call uri_to_worker");
                JK_TRACE_EXIT(l);
                return DECLINED;
            }

            if (apr_table_get(r->subprocess_env, "no-jk")) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Into map_to_storage no-jk env var detected for uri=%s, declined",
                           r->uri);

                JK_TRACE_EXIT(l);
                return DECLINED;
            }

            // Not a URI based request - e.g. file based SSI include
            if (strlen(r->uri) == 0) {
                jk_log(l, JK_LOG_DEBUG,
                       "File based (sub-)request for file=%s. No URI to match.",
                       r->filename);
                JK_TRACE_EXIT(l);
                return DECLINED;
            }

            clean_uri = apr_pstrdup(r->pool, r->uri);
            rc = jk_servlet_normalize(clean_uri, l);
            if (rc != 0) {
                JK_TRACE_EXIT(l);
                return HTTP_BAD_REQUEST;
            }

            if (!conf->uw_map) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "missing uri map for %s:%s",
                           conf->s->server_hostname ? conf->s->server_hostname : "_default_",
                           r->uri);
                JK_TRACE_EXIT(l);
                return DECLINED;
            }
            else {
                rule_extension_t *e;
                worker = map_uri_to_worker_ext(conf->uw_map, clean_uri,
                                               NULL, &e, NULL, l);
                if (worker) {
                    rconf->rule_extensions = e;
                    rconf->orig_uri = r->uri;
                    r->uri = clean_uri;
                }
            }

            if (worker) {
                r->handler = apr_pstrdup(r->pool, JK_HANDLER);
                apr_table_setn(r->notes, JK_NOTE_WORKER_NAME, worker);

                /* This could be a sub-request, possibly from mod_dir */
                if (r->main)
                    apr_table_setn(r->main->notes, JK_NOTE_WORKER_NAME, worker);

            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "no match for %s found",
                           r->uri);
                if (conf->strip_session == JK_TRUE && conf->strip_session_name) {
                    if (r->uri) {
                        jk_strip_session_id(r->uri, conf->strip_session_name, l);
                    }
                    if (r->filename) {
                        jk_strip_session_id(r->filename, conf->strip_session_name, l);
                    }
                    JK_TRACE_EXIT(l);
                    return DECLINED;
                }
            }
            JK_TRACE_EXIT(l);
        }
    }

    if (apr_table_get(r->notes, JK_NOTE_WORKER_NAME)) {

        /* First find just the name of the file, no directory */
        r->filename = (char *)apr_filepath_name_get(r->uri);

        /* Only if sub-request for a directory, most likely from mod_dir */
        if (r->main && r->main->filename &&
            (!apr_filepath_name_get(r->main->filename) ||
             !strlen(apr_filepath_name_get(r->main->filename)))) {

            /* The filename from the main request will be set to what should
             * be picked up, aliases included. Tomcat will need to know about
             * those aliases or things won't work for them. Normal files should
             * be fine. */

            /* Need absolute path to stat */
            if (apr_filepath_merge(&r->filename,
                                   r->main->filename, r->filename,
                                   APR_FILEPATH_SECUREROOT |
                                   APR_FILEPATH_TRUENAME, r->pool)
                != APR_SUCCESS) {
                return DECLINED;        /* We should never get here, very bad */
            }

            /* Stat the file so that mod_dir knows it's there */
            apr_stat(&r->finfo, r->filename, APR_FINFO_TYPE, r->pool);
        }

        return OK;
    }
    return DECLINED;
}

static void jk_register_hooks(apr_pool_t * p)
{
    ap_hook_post_config(jk_post_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(jk_child_init, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk_translate, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_map_to_storage(jk_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_handler(jk_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_log_transaction(request_log_transaction, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA jk_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                       /* dir config creater */
    NULL,                       /* dir merger --- default is to override */
    create_jk_config,           /* server config */
    merge_jk_config,            /* merge server config */
    jk_cmds,                    /* command ap_table_t */
    jk_register_hooks           /* register hooks */
};
