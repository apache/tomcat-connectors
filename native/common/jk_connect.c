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

/*
 * Description: Socket/Naming manipulation functions
 * Based on:    Various Jserv files
 */
/**
 * @package jk_connect
 * @author      Gal Shachor <shachor@il.ibm.com>
 * @author      Mladen Turk <mturk@apache.org>
 * @version     $Revision$
 */


#include "jk_connect.h"
#include "jk_util.h"

#ifdef HAVE_APR
#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_pools.h"
static apr_pool_t *jk_apr_pool = NULL;
#endif

#ifdef HAVE_SYS_FILIO_H
/* FIONREAD on Solaris et al. */
#include <sys/filio.h>
#endif
#ifdef HAVE_POLL_H
/* Use poll instead select */
#include <poll.h>
#endif

#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
#define JK_IS_SOCKET_ERROR(x) ((x) == SOCKET_ERROR)
#define JK_GET_SOCKET_ERRNO() errno = WSAGetLastError() - WSABASEERR
#else
#define JK_IS_SOCKET_ERROR(x) ((x) == -1)
#define JK_GET_SOCKET_ERRNO() ((void)0)
#endif /* WIN32 */

#if defined(NETWARE) && defined(__NOVELL_LIBC__)
#define USE_SOCK_CLOEXEC
#endif

#ifndef INET6_ADDRSTRLEN
/* Maximum size of an IPv6 address in ASCII */
#define INET6_ADDRSTRLEN 46
#endif

/* 2 IPv6 adresses of length (INET6_ADDRSTRLEN-1)
 * each suffixed with a ":" and a port (5 digits)
 * plus " -> " plus terminating "\0"
 */
#define DUMP_SINFO_BUF_SZ (2 * (INET6_ADDRSTRLEN - 1 + 1 + 5) + 4 + 1)

#ifndef IN6ADDRSZ
#define IN6ADDRSZ   16
#endif

#ifndef INT16SZ
#define INT16SZ     sizeof(short)
#endif

#if !defined(EAFNOSUPPORT) && defined(WSAEAFNOSUPPORT)
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

/* our compiler cant deal with char* <-> const char* ... */
#if defined(NETWARE) && !defined(__NOVELL_LIBC__)
typedef char* SET_TYPE;
#else
typedef const char* SET_TYPE;
#endif

/** Set socket to blocking
 * @param sd  socket to manipulate
 * @return    errno: fcntl returns -1 (!WIN32)
 *            pseudo errno: ioctlsocket returns SOCKET_ERROR (WIN32)
 *            0: success
 */
static int soblock(jk_sock_t sd)
{
/* BeOS uses setsockopt at present for non blocking... */
#if defined (WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    u_long on = 0;
    if (JK_IS_SOCKET_ERROR(ioctlsocket(sd, FIONBIO, &on))) {
        JK_GET_SOCKET_ERRNO();
        return errno;
    }
#else
    int fd_flags;

    fd_flags = fcntl(sd, F_GETFL, 0);
#if defined(O_NONBLOCK)
    fd_flags &= ~O_NONBLOCK;
#elif defined(O_NDELAY)
    fd_flags &= ~O_NDELAY;
#elif defined(FNDELAY)
    fd_flags &= ~FNDELAY;
#else
#error Please teach JK how to make sockets blocking on your platform.
#endif
    if (fcntl(sd, F_SETFL, fd_flags) == -1) {
        return errno;
    }
#endif /* WIN32 || (NETWARE && __NOVELL_LIBC__) */
    return 0;
}

/** Set socket to non-blocking
 * @param sd  socket to manipulate
 * @return    errno: fcntl returns -1 (!WIN32)
 *            pseudo errno: ioctlsocket returns SOCKET_ERROR (WIN32)
 *            0: success
 */
static int sononblock(jk_sock_t sd)
{
#if defined (WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    u_long on = 1;
    if (JK_IS_SOCKET_ERROR(ioctlsocket(sd, FIONBIO, &on))) {
        JK_GET_SOCKET_ERRNO();
        return errno;
    }
#else
    int fd_flags;

    fd_flags = fcntl(sd, F_GETFL, 0);
#if defined(O_NONBLOCK)
    fd_flags |= O_NONBLOCK;
#elif defined(O_NDELAY)
    fd_flags |= O_NDELAY;
#elif defined(FNDELAY)
    fd_flags |= FNDELAY;
#else
#error Please teach JK how to make sockets non-blocking on your platform.
#endif
    if (fcntl(sd, F_SETFL, fd_flags) == -1) {
        return errno;
    }
#endif /* WIN32 || (NETWARE && __NOVELL_LIBC__) */
    return 0;
}

#if defined (WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
/* WIN32 implementation */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param source   optional source address
 * @param timeout  connect timeout in seconds
 *                 (<=0: no timeout=blocking)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 SOCKET_ERROR: no timeout given and error
 *                               during blocking connect
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, jk_sockaddr_t *addr, jk_sockaddr_t *source,
                      int timeout, jk_logger_t *l)
{
    int rc;
    char buf[64];

    JK_TRACE_ENTER(l);

    if (source != NULL) {
        if (bind(sd, (const struct sockaddr *)&source->sa.sin, source->salen)) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "error during source bind on socket %d [%s] (errno=%d)",
                   sd, jk_dump_hinfo(source, buf, sizeof(buf)), errno);
        }
    }
    if (timeout <= 0) {
        rc = connect(sd, (const struct sockaddr *)&addr->sa.sin, addr->salen);
        JK_TRACE_EXIT(l);
        return rc;
    }

    if ((rc = sononblock(sd))) {
        JK_TRACE_EXIT(l);
        return -1;
    }
    if (JK_IS_SOCKET_ERROR(connect(sd, (const struct sockaddr *)&addr->sa.sin, addr->salen))) {
        struct timeval tv;
        fd_set wfdset, efdset;

        if ((rc = WSAGetLastError()) != WSAEWOULDBLOCK) {
            soblock(sd);
            WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
        /* wait for the connect to complete or timeout */
        FD_ZERO(&wfdset);
        FD_SET(sd, &wfdset);
        FD_ZERO(&efdset);
        FD_SET(sd, &efdset);

        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;

        rc = select((int)sd + 1, NULL, &wfdset, &efdset, &tv);
        if (JK_IS_SOCKET_ERROR(rc) || rc == 0) {
            rc = WSAGetLastError();
            soblock(sd);
            WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
        /* Evaluate the efdset */
        if (FD_ISSET(sd, &efdset)) {
            /* The connect failed. */
            int rclen = (int)sizeof(rc);
            if (getsockopt(sd, SOL_SOCKET, SO_ERROR, (char*) &rc, &rclen))
                rc = 0;
            soblock(sd);
            if (rc)
                WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
    }
    soblock(sd);
    JK_TRACE_EXIT(l);
    return 0;
}

#elif !defined(NETWARE)
/* POSIX implementation */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param source   optional source address
 * @param timeout  connect timeout in seconds
 *                 (<=0: no timeout=blocking)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, jk_sockaddr_t *addr, jk_sockaddr_t *source,
                      int timeout, jk_logger_t *l)
{
    int rc = 0;
    char buf[64];

    JK_TRACE_ENTER(l);

    if (source != NULL) {
        if (bind(sd, (const struct sockaddr *)&source->sa.sin, source->salen)) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "error during source bind on socket %d [%s] (errno=%d)",
                   sd, jk_dump_hinfo(source, buf, sizeof(buf)), errno);
        }
    }
    if (timeout > 0) {
        if (sononblock(sd)) {
            JK_TRACE_EXIT(l);
            return -1;
        }
    }
    do {
        rc = connect(sd, (const struct sockaddr *)&addr->sa.sin, addr->salen);
    } while (rc == -1 && errno == EINTR);

    if ((rc == -1) && (errno == EINPROGRESS || errno == EALREADY)
                   && (timeout > 0)) {
        fd_set wfdset;
        struct timeval tv;
        socklen_t rclen = (socklen_t)sizeof(rc);

        FD_ZERO(&wfdset);
        FD_SET(sd, &wfdset);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        rc = select(sd + 1, NULL, &wfdset, NULL, &tv);
        if (rc <= 0) {
            /* Save errno */
            int err = errno;
            soblock(sd);
            errno = err;
            JK_TRACE_EXIT(l);
            return -1;
        }
        rc = 0;
#ifdef SO_ERROR
        if (!FD_ISSET(sd, &wfdset) ||
            (getsockopt(sd, SOL_SOCKET, SO_ERROR,
                        (char *)&rc, &rclen) < 0) || rc) {
            if (rc)
                errno = rc;
            rc = -1;
        }
#endif /* SO_ERROR */
    }
    /* Not sure we can be already connected */
    if (rc == -1 && errno == EISCONN)
        rc = 0;
    soblock(sd);
    JK_TRACE_EXIT(l);
    return rc;
}
#else
/* NETWARE implementation - blocking for now */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param source   optional source address
 * @param timeout  connect timeout in seconds (ignored!)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, jk_sockaddr_t *addr, jk_sockaddr_t *source,
                      int timeout, jk_logger_t *l)
{
    int rc;
    char buf[64];

    JK_TRACE_ENTER(l);

    if (source != NULL) {
        if (bind(sd, (const struct sockaddr *)&source->sa.sin, source->salen)) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "error during source bind on socket %d [%s] (errno=%d)",
                   sd, jk_dump_hinfo(source, buf, sizeof(buf)), errno);
        }
    }
    rc = connect(sd, (const struct sockaddr *)&addr->sa.sin, addr->salen);
    JK_TRACE_EXIT(l);
    return rc;
}
#endif


#ifdef AS400_UTF8

/*
 *  i5/OS V5R4 need EBCDIC for its runtime calls but APR/APACHE works in UTF
 */
in_addr_t jk_inet_addr(const char * addrstr)
{
    in_addr_t addr;
    char *ptr;

    ptr = (char *)malloc(strlen(addrstr) + 1);
    jk_ascii2ebcdic((char *)addrstr, ptr);
    addr = inet_addr(ptr);
    free(ptr);

    return(addr);
}

#endif

/** Clone a jk_sockaddr_t
 * @param out     The source structure
 * @param in      The target structure
 */
void jk_clone_sockaddr(jk_sockaddr_t *out, jk_sockaddr_t *in)
{
    memcpy(out, in, sizeof(*in));
    /* The ipaddr_ptr member points to memory inside the struct.
     * Do not copy the pointer but use the same offset relative
     * to the struct start
     */
    out->ipaddr_ptr = (char *)out + ((char *)in->ipaddr_ptr - (char *)in);
}

/** Resolve the host IP
 * @param host     host or ip address
 * @param port     port
 * @param rc       return value pointer
 * @param l        logger
 * @return         JK_FALSE: some kind of error occured
 *                 JK_TRUE: success
 */
int jk_resolve(const char *host, int port, jk_sockaddr_t *saddr,
               void *pool, int prefer_ipv6, jk_logger_t *l)
{
    int family = JK_INET;
    struct in_addr iaddr;

    JK_TRACE_ENTER(l);

    memset(saddr, 0, sizeof(jk_sockaddr_t));
    if (*host >= '0' && *host <= '9' && strspn(host, "0123456789.") == strlen(host)) {

        /* If we found only digits we use inet_addr() */
        iaddr.s_addr = jk_inet_addr(host);
        memcpy(&(saddr->sa.sin.sin_addr), &iaddr, sizeof(struct in_addr));
    }
    else {
#ifdef HAVE_APR
        apr_sockaddr_t *remote_sa, *temp_sa;

        if (!jk_apr_pool) {
            if (apr_pool_create(&jk_apr_pool, (apr_pool_t *)pool) != APR_SUCCESS) {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
        }
        apr_pool_clear(jk_apr_pool);
        if (apr_sockaddr_info_get(&remote_sa, host, APR_UNSPEC, (apr_port_t)port,
                                  0, jk_apr_pool) != APR_SUCCESS) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        /* Check if we have multiple address matches
         */
        if (remote_sa->next) {
            /* Since we are only handling JK_INET (IPV4) address (in_addr_t) */
            /* make sure we find one of those.                               */
            temp_sa = remote_sa;
#if APR_HAVE_IPV6
            if (prefer_ipv6) {
                while ((NULL != temp_sa) && (APR_INET6 != temp_sa->family))
                    temp_sa = temp_sa->next;
            }
#endif
            if (NULL != temp_sa) {
                remote_sa = temp_sa;
            }
            else {
                while ((NULL != temp_sa) && (APR_INET != temp_sa->family))
                    temp_sa = temp_sa->next;
#if APR_HAVE_IPV6
                if (NULL == temp_sa) {
                    temp_sa = remote_sa;
                    while ((NULL != temp_sa) && (APR_INET6 != temp_sa->family))
                        temp_sa = temp_sa->next;
                }
#endif
            }
            /* if temp_sa is set, we have a valid address otherwise, just return */
            if (NULL != temp_sa) {
                remote_sa = temp_sa;
            }
            else {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
        }
        if (remote_sa->family == APR_INET) {
            saddr->sa.sin = remote_sa->sa.sin;
            family = JK_INET;
        }
#if APR_HAVE_IPV6
        else {
            saddr->sa.sin6 = remote_sa->sa.sin6;
            family = JK_INET6;
        }
#endif
#else /* HAVE_APR */
        /* Without APR go the classic way.
         */
#if defined(HAVE_GETADDRINFO)
        /* TODO:
         * 1. Check for numeric IPV6 addresses
         * 2. Do we need to set service name for getaddrinfo?
         */
        struct addrinfo hints, *ai_list, *ai = NULL;
        int error;
        char  pbuf[12];
        char *pbufptr = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        
#if JK_HAVE_IPV6
        if (strchr(host, ':')) {
            /* If host name contains collon this must be IPV6 address.
             * Set prefer_ipv6 flag in this case if it wasn't set already
             */
            prefer_ipv6 = 1;            
        }
        if (prefer_ipv6)
            hints.ai_family = JK_INET6;
        else
#endif
            hints.ai_family = JK_INET;
        if (port > 0) {
            snprintf(pbuf, sizeof(pbuf), "%d", port);
            pbufptr = pbuf;
        }
        error = getaddrinfo(host, pbufptr, &hints, &ai_list);
#if JK_HAVE_IPV6
        /* XXX:
         * Is the check for EAI_FAMILY/WSAEAFNOSUPPORT correct
         * way to retry the IPv4 address?
         */
        if (error == EAI_FAMILY && prefer_ipv6) {
            hints.ai_family = JK_INET;
            error = getaddrinfo(host, pbufptr, &hints, &ai_list);
        }
#endif
        if (error) {
            JK_TRACE_EXIT(l);
            errno = error;
            return JK_FALSE;
        }
#if JK_HAVE_IPV6
        if (prefer_ipv6) {
            ai = ai_list;
            while (ai) {
                if (ai->ai_family == JK_INET6) {
                    /* ignore elements without required address info */
                    if((ai->ai_addr != NULL) && (ai->ai_addrlen > 0)) {                        
                        family = JK_INET6;
                        break;
                    }
                }
                ai = ai->ai_next;
            }
        }
#endif
        if (ai == NULL) {
            ai = ai_list;
            while (ai) {
                if (ai->ai_family == JK_INET) {
                    /* ignore elements without required address info */
                    if((ai->ai_addr != NULL) && (ai->ai_addrlen > 0)) {                        
                        family = JK_INET;
                        break;
                    }
                }
                ai = ai->ai_next;
            }
        }
        if (ai == NULL) {
            /* No address found
             * XXX: Use better error code?
             */
            freeaddrinfo(ai_list);
            JK_TRACE_EXIT(l);
            errno = ENOENT;
            return JK_FALSE;
        }
        memcpy(&(saddr->sa), ai->ai_addr, ai->ai_addrlen);
        freeaddrinfo(ai_list);
#else /* HAVE_GETADDRINFO */
        struct hostent *hoste;

        /* XXX : WARNING : We should really use gethostbyname_r in multi-threaded env */
        /* Fortunatly when APR is available, ie under Apache 2.0, we use it */
#if defined(NETWARE) && !defined(__NOVELL_LIBC__)
        hoste = gethostbyname((char*)host);
#else
        hoste = gethostbyname(host);
#endif
        if (!hoste) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        iaddr = *((struct in_addr *)hoste->h_addr_list[0]);
        memcpy(&(saddr->sa.sin.sin_addr), &iaddr, sizeof(struct in_addr));
#endif /* HAVE_GETADDRINFO */
#endif /* HAVE_APR */
    }

    if (family == JK_INET) {
        saddr->ipaddr_ptr = &(saddr->sa.sin.sin_addr);
        saddr->ipaddr_len = (int)sizeof(struct in_addr);
        saddr->salen      = (int)sizeof(struct sockaddr_in);
    }
#if JK_HAVE_IPV6
    else {
        saddr->ipaddr_ptr = &(saddr->sa.sin6.sin6_addr);
        saddr->ipaddr_len = (int)sizeof(struct in6_addr);
        saddr->salen      = (int)sizeof(struct sockaddr_in6);
    }
#endif
    saddr->sa.sin.sin_family = family;
    /* XXX IPv6: assumes sin_port and sin6_port at same offset */
    saddr->sa.sin.sin_port = htons(port);
    saddr->port   = port;
    saddr->family = family;

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

/** Connect to Tomcat
 * @param addr      address to connect to
 * @param keepalive should we set SO_KEEPALIVE (if !=0)
 * @param timeout   connect timeout in seconds
 *                  (<=0: no timeout=blocking)
 * @param sock_buf  size of send and recv buffer
 *                  (<=0: use default)
 * @param l         logger
 * @return          JK_INVALID_SOCKET: some kind of error occured
 *                  created socket: success
 * @remark          Cares about errno
 */
jk_sock_t jk_open_socket(jk_sockaddr_t *addr, jk_sockaddr_t *source,
                         int keepalive,
                         int timeout, int connect_timeout,
                         int sock_buf, jk_logger_t *l)
{
    char buf[DUMP_SINFO_BUF_SZ];
    jk_sock_t sd;
    int set = 1;
    int ret = 0;
    int flags = 0;
#ifdef SO_LINGER
    struct linger li;
#endif

    JK_TRACE_ENTER(l);

    errno = 0;
#if defined(SOCK_CLOEXEC) && defined(USE_SOCK_CLOEXEC)
    flags |= SOCK_CLOEXEC;
#endif
    sd = socket(addr->family, SOCK_STREAM | flags, 0);
    if (!IS_VALID_SOCKET(sd)) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
               "socket() failed (errno=%d)", errno);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#if defined(FD_CLOEXEC) && !defined(USE_SOCK_CLOEXEC)
    if ((flags = fcntl(sd, F_GETFD)) == -1) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
               "fcntl() failed (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
    flags |= FD_CLOEXEC;
    if (fcntl(sd, F_SETFD, flags) == -1) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
               "fcntl() failed (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#endif

    /* Disable Nagle algorithm */
    if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (SET_TYPE)&set,
                   sizeof(set))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting TCP_NODELAY (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "socket TCP_NODELAY set to On");
    if (keepalive) {
#if defined(WIN32) && !defined(NETWARE)
        DWORD dw;
        struct tcp_keepalive ka = { 0 }, ks = { 0 };
        if (timeout)
            ka.keepalivetime = timeout * 10000;
        else
            ka.keepalivetime = 60 * 10000; /* 10 minutes */
        ka.keepaliveinterval = 1000;
        ka.onoff = 1;
        if (WSAIoctl(sd, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
                     &ks, sizeof(ks), &dw, NULL, NULL)) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "failed setting SIO_KEEPALIVE_VALS (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_KEEPALIVE set to %d seconds",
                   ka.keepalivetime / 1000);
#else
        set = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (SET_TYPE)&set,
                       sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "failed setting SO_KEEPALIVE (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_KEEPALIVE set to On");
#endif
    }

    if (sock_buf > 0) {
        set = sock_buf;
        /* Set socket send buffer size */
        if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (SET_TYPE)&set,
                        sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_SNDBUF (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        set = sock_buf;
        /* Set socket receive buffer size */
        if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (SET_TYPE)&set,
                                sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_RCVBUF (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_SNDBUF and SO_RCVBUF set to %d",
                   sock_buf);
    }

    if (timeout > 0) {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        int tmout = timeout * 1000;
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *) &tmout, sizeof(int));
        setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                   (const char *) &tmout, sizeof(int));
        JK_GET_SOCKET_ERRNO();
#elif defined(SO_RCVTIMEO) && defined(USE_SO_RCVTIMEO) && defined(SO_SNDTIMEO) && defined(USE_SO_SNDTIMEO)
        struct timeval tv;
        tv.tv_sec  = timeout;
        tv.tv_usec = 0;
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                   (const void *) &tv, sizeof(tv));
        setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                   (const void *) &tv, sizeof(tv));
#endif
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "timeout %d set for socket=%d",
                   timeout, sd);
    }
#ifdef SO_NOSIGPIPE
    /* The preferred method on Mac OS X (10.2 and later) to prevent SIGPIPEs when
     * sending data to a dead peer. Possibly also existing and in use on other BSD
     * systems?
    */
    set = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&set,
                   sizeof(int))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting SO_NOSIGPIPE (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#endif
#ifdef SO_LINGER
    /* Make hard closesocket by disabling lingering */
    li.l_linger = li.l_onoff = 0;
    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (SET_TYPE)&li,
                   sizeof(li))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting SO_LINGER (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#endif
    /* Tries to connect to Tomcat (continues trying while error is EINTR) */
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
                "trying to connect socket %d to %s", sd,
                jk_dump_hinfo(addr, buf, sizeof(buf)));

/* Need more infos for BSD 4.4 and Unix 98 defines, for now only
iSeries when Unix98 is required at compile time */
#if (_XOPEN_SOURCE >= 520) && defined(AS400)
    ((struct sockaddr *)addr)->sa.sin.sa_len = sizeof(struct sockaddr_in);
#endif
    ret = nb_connect(sd, addr, source, connect_timeout, l);
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    if (JK_IS_SOCKET_ERROR(ret)) {
        JK_GET_SOCKET_ERRNO();
    }
#endif /* WIN32 */

    /* Check if we are connected */
    if (ret) {
        jk_log(l, JK_LOG_INFO,
               "connect to %s failed (errno=%d)",
               jk_dump_hinfo(addr, buf, sizeof(buf)), errno);
        jk_close_socket(sd, l);
        sd = JK_INVALID_SOCKET;
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "socket %d [%s] connected",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)));
    }
    JK_TRACE_EXIT(l);
    return sd;
}

/** Close the socket
 * @param sd        socket to close
 * @param l         logger
 * @return          -1: some kind of error occured (!WIN32)
 *                  SOCKET_ERROR: some kind of error occured  (WIN32)
 *                  0: success
 * @remark          Does not change errno
 */
int jk_close_socket(jk_sock_t sd, jk_logger_t *l)
{
    int rc;
    int save_errno;

    JK_TRACE_ENTER(l);

    if (!IS_VALID_SOCKET(sd)) {
        JK_TRACE_EXIT(l);
        return -1;
    }

    save_errno = errno;
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    rc = closesocket(sd) ? -1 : 0;
#else
    do {
        rc = close(sd);
    } while (JK_IS_SOCKET_ERROR(rc) && (errno == EINTR || errno == EAGAIN));
#endif
    JK_TRACE_EXIT(l);
    errno = save_errno;
    return rc;
}

#ifndef MAX_SECS_TO_LINGER
#define MAX_SECS_TO_LINGER 2
#endif

#ifndef MS_TO_LINGER
#define MS_TO_LINGER  100
#endif

#ifndef MS_TO_LINGER_LAST
#define MS_TO_LINGER_LAST 20
#endif

#ifndef MAX_READ_RETRY
#define MAX_READ_RETRY 10
#endif

#ifndef MAX_LINGER_BYTES
#define MAX_LINGER_BYTES 32768
#endif

#ifndef SHUT_WR
#ifdef SD_SEND
#define SHUT_WR SD_SEND
#else
#define SHUT_WR 0x01
#endif
#endif

#ifndef SHUT_RD
#ifdef SD_RECEIVE
#define SHUT_RD SD_RECEIVE
#else
#define SHUT_RD 0x00
#endif
#endif

/** Drain and close the socket
 * @param sd        socket to close
 * @param l         logger
 * @return          -1: socket to close is invalid
 *                  -1: some kind of error occured (!WIN32)
 *                  SOCKET_ERROR: some kind of error occured  (WIN32)
 *                  0: success
 * @remark          Does not change errno
 */
int jk_shutdown_socket(jk_sock_t sd, jk_logger_t *l)
{
    char dummy[512];
    char buf[DUMP_SINFO_BUF_SZ];
    char *sb = NULL;
    int rc = 0;
    size_t rd = 0;
    size_t rp = 0;
    int save_errno;
    int timeout = MS_TO_LINGER;
    time_t start = time(NULL);

    JK_TRACE_ENTER(l);

    if (!IS_VALID_SOCKET(sd)) {
        JK_TRACE_EXIT(l);
        return -1;
    }

    save_errno = errno;
    if (JK_IS_DEBUG_LEVEL(l)) {
        sb = jk_dump_sinfo(sd, buf, sizeof(buf));
        jk_log(l, JK_LOG_DEBUG, "About to shutdown socket %d [%s]",
               sd, sb);
    }
    /* Shut down the socket for write, which will send a FIN
     * to the peer.
     */
    if (shutdown(sd, SHUT_WR)) {
        rc = jk_close_socket(sd, l);
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Failed sending SHUT_WR for socket %d [%s]",
                   sd, sb);
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return rc;
    }

    do {
        rp = 0;
        if (jk_is_input_event(sd, timeout, l)) {
            /* Do a restartable read on the socket
             * draining out all the data currently in the socket buffer.
             */
            int num = 0;
            do {
                num++;
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
                rc = recv(sd, &dummy[0], sizeof(dummy), 0);
                if (JK_IS_SOCKET_ERROR(rc))
                    JK_GET_SOCKET_ERRNO();
#else
                rc = read(sd, &dummy[0], sizeof(dummy));
#endif
                if (rc > 0)
                    rp += rc;
            } while (JK_IS_SOCKET_ERROR(rc) && (errno == EINTR || errno == EAGAIN) && num < MAX_READ_RETRY);

            if (rc < 0) {
                /* Read failed.
                 * Bail out from the loop.
                 */
                break;
            }
        }
        else {
            /* Error or timeout (reason is logged within jk_is_input_event)
             * Exit the drain loop
             */
            break;
        }
        rd += rp;
        if (rp < sizeof(dummy)) {
            if (timeout > MS_TO_LINGER_LAST) {
                /* Try one last time with a short timeout
                */
                timeout = MS_TO_LINGER_LAST;
                continue;
            }
            /* We have read less then size of buffer
             * It's a good chance there will be no more data
             * to read.
             */
            if ((rc = sononblock(sd))) {
                rc = jk_close_socket(sd, l);
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "error setting socket %d [%s] to nonblocking",
                           sd, sb);
                errno = save_errno;
                JK_TRACE_EXIT(l);
                return rc;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "shutting down the read side of socket %d [%s]",
                       sd, sb);
            shutdown(sd, SHUT_RD);
            break;
        }
        timeout = MS_TO_LINGER;
    } while ((rd < MAX_LINGER_BYTES) && (difftime(time(NULL), start) < MAX_SECS_TO_LINGER));

    rc = jk_close_socket(sd, l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Shutdown socket %d [%s] and read %d lingering bytes in %d sec.",
               sd, sb, rd, (int)difftime(time(NULL), start));
    errno = save_errno;
    JK_TRACE_EXIT(l);
    return rc;
}

/** send a message
 * @param sd  socket to use
 * @param b   buffer containing the data
 * @param len length to send
 * @param l   logger
 * @return    negative errno: write returns a fatal -1 (!WIN32)
 *            negative pseudo errno: send returns SOCKET_ERROR (WIN32)
 *            JK_SOCKET_EOF: no bytes could be sent
 *            >0: success, provided number of bytes send
 * @remark    Always closes socket in case of error
 * @remark    Cares about errno
 * @bug       this fails on Unixes if len is too big for the underlying
 *            protocol
 */
int jk_tcp_socket_sendfull(jk_sock_t sd, const unsigned char *b, int len, jk_logger_t *l)
{
    int sent = 0;
    int wr;

    JK_TRACE_ENTER(l);

    errno = 0;
    while (sent < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            wr = send(sd, (const char*)(b + sent),
                      len - sent, 0);
            if (JK_IS_SOCKET_ERROR(wr))
                JK_GET_SOCKET_ERRNO();
#else
            wr = write(sd, b + sent, len - sent);
#endif
        } while (JK_IS_SOCKET_ERROR(wr) && (errno == EINTR || errno == EAGAIN));

        if (JK_IS_SOCKET_ERROR(wr)) {
            int err;
            jk_shutdown_socket(sd, l);
            err = (errno > 0) ? -errno : errno;
            JK_TRACE_EXIT(l);
            return err;
        }
        else if (wr == 0) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_SOCKET_EOF;
        }
        sent += wr;
    }

    JK_TRACE_EXIT(l);
    return sent;
}

/** receive a message
 * @param sd  socket to use
 * @param b   buffer to store the data
 * @param len length to receive
 * @param l   logger
 * @return    negative errno: read returns a fatal -1 (!WIN32)
 *            negative pseudo errno: recv returns SOCKET_ERROR (WIN32)
 *            JK_SOCKET_EOF: no bytes could be read
 *            >0: success, requested number of bytes received
 * @remark    Always closes socket in case of error
 * @remark    Cares about errno
 */
int jk_tcp_socket_recvfull(jk_sock_t sd, unsigned char *b, int len, jk_logger_t *l)
{
    int rdlen = 0;
    int rd;

    JK_TRACE_ENTER(l);

    errno = 0;
    while (rdlen < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            rd = recv(sd, (char *)b + rdlen,
                      len - rdlen, 0);
            if (JK_IS_SOCKET_ERROR(rd))
                JK_GET_SOCKET_ERRNO();
#else
            rd = read(sd, (char *)b + rdlen, len - rdlen);
#endif
        } while (JK_IS_SOCKET_ERROR(rd) && errno == EINTR);

        if (JK_IS_SOCKET_ERROR(rd)) {
            int err = (errno > 0) ? -errno : errno;
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return (err == 0) ? JK_SOCKET_EOF : err;
        }
        else if (rd == 0) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_SOCKET_EOF;
        }
        rdlen += rd;
    }

    JK_TRACE_EXIT(l);
    return rdlen;
}

/* const char *
 * inet_ntop4(src, dst, size)
 *  format an IPv4 address, more or less like inet_ntoa()
 * return:
 *  `dst' (as a const)
 * notes:
 *  (1) uses no statics
 *  (2) takes a u_char* not an in_addr as input
 * author:
 *  Paul Vixie, 1996.
 */
static const char *inet_ntop4(const unsigned char *src, char *dst, size_t size)
{
    const size_t MIN_SIZE = 16; /* space for 255.255.255.255\0 */
    int n = 0;
    char *next = dst;

    if (size < MIN_SIZE) {
        errno = ENOSPC;
        return NULL;
    }
    do {
        unsigned char u = *src++;
        if (u > 99) {
            *next++ = '0' + u/100;
             u %= 100;
            *next++ = '0' + u/10;
             u %= 10;
        }
        else if (u > 9) {
            *next++ = '0' + u/10;
             u %= 10;
        }
        *next++ = '0' + u;
        *next++ = '.';
        n++;
    } while (n < 4);
    *--next = '\0';
    return dst;
}

#if JK_HAVE_IPV6
/* const char *
 * inet_ntop6(src, dst, size)
 *  convert IPv6 binary address into presentation (printable) format
 * author:
 *  Paul Vixie, 1996.
 */
static const char *inet_ntop6(const unsigned char *src, char *dst, size_t size)
{
    /*
     * Note that int32_t and int16_t need only be "at least" large enough
     * to contain a value of the specified size.  On some systems, like
     * Crays, there is no such thing as an integer variable with 16 bits.
     * Keep this in mind if you think this function should have been coded
     * to use pointer overlays.  All the world's not a VAX.
     */
    char tmp[INET6_ADDRSTRLEN], *tp;
    struct { int base, len; } best = {-1, 0}, cur = {-1, 0};
    unsigned int words[IN6ADDRSZ / INT16SZ];
    int i;
    const unsigned char *next_src, *src_end;
    unsigned int *next_dest;

    /*
     * Preprocess:
     *  Copy the input (bytewise) array into a wordwise array.
     *  Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    next_src = src;
    src_end = src + IN6ADDRSZ;
    next_dest = words;
    i = 0;
    do {
        unsigned int next_word = (unsigned int)*next_src++;
        next_word <<= 8;
        next_word |= (unsigned int)*next_src++;
        *next_dest++ = next_word;

        if (next_word == 0) {
            if (cur.base == -1) {
                cur.base = i;
                cur.len = 1;
            }
            else {
                cur.len++;
            }
        } else {
            if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len) {
                    best = cur;
                }
                cur.base = -1;
            }
        }

        i++;
    } while (next_src < src_end);

    if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len) {
            best = cur;
        }
    }
    if (best.base != -1 && best.len < 2) {
        best.base = -1;
    }

    /*
     * Format the result.
     */
    tp = tmp;
    for (i = 0; i < (IN6ADDRSZ / INT16SZ);) {
        /* Are we inside the best run of 0x00's? */
        if (i == best.base) {
            *tp++ = ':';
            i += best.len;
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if (i != 0) {
            *tp++ = ':';
        }
        /* Is this address an encapsulated IPv4? */
        if (i == 6 && best.base == 0 &&
            (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
            if (!inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp))) {
                return (NULL);
            }
            tp += strlen(tp);
            break;
        }
        tp += sprintf(tp, "%x", words[i]);
        i++;
    }
    /* Was it a trailing run of 0x00's? */
    if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ)) {
        *tp++ = ':';
    }
    *tp++ = '\0';

    /*
     * Check for overflow, copy, and we're done.
     */
    if ((size_t)(tp - tmp) > size) {
        errno = ENOSPC;
        return NULL;
    }
    strcpy(dst, tmp);
    return dst;
}
#endif

/**
 * dump a jk_sockaddr_t in A.B.C.D:P in ASCII buffer
 *
 */
char *jk_dump_hinfo(jk_sockaddr_t *saddr, char *buf, size_t size)
{
    char pb[8];

    if (saddr->ipaddr_ptr == NULL) {
        strcpy(buf, "UnresolvedIP");
    } else {
        if (saddr->family == JK_INET) {
            inet_ntop4(saddr->ipaddr_ptr, buf, size);
        }
#if JK_HAVE_IPV6
        else {
            inet_ntop6(saddr->ipaddr_ptr, buf, size);
        }
#endif
    }
 
    sprintf(pb, ":%d", saddr->port);
    strncat(buf, pb, size - strlen(buf) - 1);

    return buf;
}

char *jk_dump_sinfo(jk_sock_t sd, char *buf, size_t size)
{
    struct sockaddr rsaddr;
    struct sockaddr lsaddr;
    socklen_t       salen;

    salen = sizeof(struct sockaddr);
    if (getsockname(sd, &lsaddr, &salen) == 0) {
        salen = sizeof(struct sockaddr);
        if (getpeername(sd, &rsaddr, &salen) == 0) {
            char   pb[8];
            size_t ps;
            if (lsaddr.sa_family == JK_INET) {
                struct sockaddr_in  *sa = (struct sockaddr_in  *)&lsaddr;
                inet_ntop4((unsigned char *)&sa->sin_addr, buf, size);
                sprintf(pb, ":%d", (unsigned int)htons(sa->sin_port));
            }
#if JK_HAVE_IPV6
            else {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&lsaddr;
                inet_ntop6((unsigned char *)&sa->sin6_addr, buf, size);
                sprintf(pb, ":%d", (unsigned int)htons(sa->sin6_port));
            }
#endif
            ps = strlen(buf);
            strncat(buf, pb, size - ps - 1);
            ps = strlen(buf);
            strncat(buf, " -> ", size - ps - 1);
            ps = strlen(buf);
            if (rsaddr.sa_family == JK_INET) {
                struct sockaddr_in  *sa = (struct sockaddr_in  *)&rsaddr;
                inet_ntop4((unsigned char *)&sa->sin_addr,  buf + ps, size - ps);
                sprintf(pb, ":%d", (unsigned int)htons(sa->sin_port));
            }
#if JK_HAVE_IPV6
            else {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&rsaddr;
                inet_ntop6((unsigned char *)&sa->sin6_addr, buf + ps, size - ps);
                sprintf(pb, ":%d", (unsigned int)htons(sa->sin6_port));
            }
#endif
            strncat(buf, pb, size - strlen(buf) - 1);
            return buf;
        }
    }
    snprintf(buf, size, "errno=%d", errno);
    return buf;
}

/** Wait for input event on socket until timeout
 * @param sd      socket to use
 * @param timeout wait timeout in milliseconds
 * @param l       logger
 * @return        JK_FALSE: Timeout expired without something to read
 *                JK_FALSE: Error during waiting
 *                JK_TRUE: success
 * @remark        Does not close socket in case of error
 *                to allow for iterative waiting
 * @remark        Cares about errno
 */
#ifdef HAVE_POLL
int jk_is_input_event(jk_sock_t sd, int timeout, jk_logger_t *l)
{
    struct pollfd fds;
    int rc;
    int save_errno;
    char buf[DUMP_SINFO_BUF_SZ];

    JK_TRACE_ENTER(l);

    errno = 0;
    fds.fd = sd;
    fds.events = POLLIN;
    fds.revents = 0;

    do {
        rc = poll(&fds, 1, timeout);
    } while (rc < 0 && errno == EINTR);

    if (rc == 0) {
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "timeout during poll on socket %d [%s] (timeout=%d)",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)), timeout);
        }
        /* Timeout. Set the errno to timeout */
        errno = ETIMEDOUT;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    else if (rc < 0) {
        save_errno = errno;
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "error during poll on socket %d [%s] (errno=%d)",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)), errno);
        }
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if ((fds.revents & (POLLERR | POLLHUP))) {
        save_errno = fds.revents & (POLLERR | POLLHUP);
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "error event during poll on socket %d [%s] (event=%d)",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)), save_errno);
        }
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    errno = 0;
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}
#else
int jk_is_input_event(jk_sock_t sd, int timeout, jk_logger_t *l)
{
    fd_set rset;
    struct timeval tv;
    int rc;
    int save_errno;
    char buf[DUMP_SINFO_BUF_SZ];

    JK_TRACE_ENTER(l);

    errno = 0;
    FD_ZERO(&rset);
    FD_SET(sd, &rset);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    do {
        rc = select((int)sd + 1, &rset, NULL, NULL, &tv);
    } while (rc < 0 && errno == EINTR);

    if (rc == 0) {
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "timeout during select on socket %d [%s] (timeout=%d)",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)), timeout);
        }
        /* Timeout. Set the errno to timeout */
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        errno = WSAETIMEDOUT - WSABASEERR;
#else
        errno = ETIMEDOUT;
#endif
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    else if (rc < 0) {
        save_errno = errno;
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "error during select on socket %d [%s] (errno=%d)",
                   sd, jk_dump_sinfo(sd, buf, sizeof(buf)), errno);
        }
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    errno = 0;
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}
#endif

/** Test if a socket is still connected
 * @param sd   socket to use
 * @param l    logger
 * @return     JK_FALSE: failure
 *             JK_TRUE: success
 * @remark     Always closes socket in case of error
 * @remark     Cares about errno
 */
#ifdef HAVE_POLL
int jk_is_socket_connected(jk_sock_t sd, jk_logger_t *l)
{
    struct pollfd fds;
    int rc;

    JK_TRACE_ENTER(l);

    errno = 0;
    fds.fd = sd;
    fds.events = POLLIN;

    do {
        rc = poll(&fds, 1, 0);
    } while (rc < 0 && errno == EINTR);

    if (rc == 0) {
        /* If we get a timeout, then we are still connected */
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else if (rc == 1 && fds.revents == POLLIN) {
        char buf;
        do {
            rc = (int)recvfrom(sd, &buf, 1, MSG_PEEK, NULL, NULL);
        } while (rc < 0 && errno == EINTR);
        if (rc == 1) {
            /* There is at least one byte to read. */
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
    }
    jk_shutdown_socket(sd, l);

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

#else
int jk_is_socket_connected(jk_sock_t sd, jk_logger_t *l)
{
    fd_set fd;
    struct timeval tv;
    int rc;

    JK_TRACE_ENTER(l);

    errno = 0;
    FD_ZERO(&fd);
    FD_SET(sd, &fd);

    /* Initially test the socket without any blocking.
     */
    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    do {
        rc = select((int)sd + 1, &fd, NULL, NULL, &tv);
        JK_GET_SOCKET_ERRNO();
        /* Wait one microsecond on next select, if EINTR */
        tv.tv_sec  = 0;
        tv.tv_usec = 1;
    } while (JK_IS_SOCKET_ERROR(rc) && errno == EINTR);

    errno = 0;
    if (rc == 0) {
        /* If we get a timeout, then we are still connected */
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else if (rc == 1) {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        u_long nr;
        rc = ioctlsocket(sd, FIONREAD, &nr);
        if (rc == 0) {
            if (WSAGetLastError() == 0)
                errno = 0;
            else
                JK_GET_SOCKET_ERRNO();
        }
#else
        int nr;
        rc = ioctl(sd, FIONREAD, (void*)&nr);
#endif
        if (rc == 0 && nr != 0) {
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        JK_GET_SOCKET_ERRNO();
    }
    jk_shutdown_socket(sd, l);

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}
#endif

