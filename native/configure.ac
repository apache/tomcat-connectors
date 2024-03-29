dnl  Licensed to the Apache Software Foundation (ASF) under one or more
dnl  contributor license agreements.  See the NOTICE file distributed with
dnl  this work for additional information regarding copyright ownership.
dnl  The ASF licenses this file to You under the Apache License, Version 2.0
dnl  (the "License"); you may not use this file except in compliance with
dnl  the License.  You may obtain a copy of the License at
dnl
dnl      http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl  Unless required by applicable law or agreed to in writing, software
dnl  distributed under the License is distributed on an "AS IS" BASIS,
dnl  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl  See the License for the specific language governing permissions and
dnl  limitations under the License.

dnl
dnl Process this file with autoconf to produce a configure script
dnl

dnl Minimum autoconf version
AC_PREREQ(2.59)

dnl package and version.
dnl synchronization with common/jk_version.h ?
AC_INIT([mod_jk], [1.2.50])

AC_CONFIG_SRCDIR([common/jk_worker.h])
AC_CONFIG_HEADER(common/config.h)
AC_CONFIG_AUX_DIR(scripts/build/unix)
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE

dnl
dnl Include our own M4 macros
dnl
sinclude(scripts/build/jk_common.m4)

dnl Generate ./config.nice for reproducing runs of configure
dnl
JK_CONFIG_NICE(config.nice)

AC_PATH_PROG(TEST,test,$PATH)dnl
AC_SUBST(TEST)

AC_PATH_PROG(GREP,grep,$PATH)dnl
AC_SUBST(GREP)

AC_PATH_PROG(ECHO,echo,echo,$PATH)dnl
AC_SUBST(ECHO)

AC_PATH_PROG(SED,sed,$PATH)dnl
AC_SUBST(SED)

AC_PATH_PROG(CP,cp,$PATH)dnl
AC_SUBST(CP)

AC_PATH_PROG(MKDIR,mkdir,$PATH)dnl
AC_SUBST(MKDIR)

dnl Mark that we have generated config.h
CFLAGS="${CFLAGS} -DHAVE_CONFIG_H"
AC_SUBST(CFLAGS)

configure_dir=`dirname $0`
configure_dir=`cd $configure_dir; pwd`
APACHE_CONFIG_VARS=$configure_dir/scripts/build/config_vars.mk
WEBSERVER=""
apache_dir=""
apache_include=""
APXS="apxs"
AC_ARG_WITH(apxs,
[[  --with-apxs[=FILE]      Build shared Apache module.
                          FILE is the optional pathname to the apxs tool;
                          defaults to finding apxs in your PATH.]],
[
    case "${withval}" in
        y | yes | true) find_apxs=true ;;
        n | no | false) find_apxs= ;;
        *) find_apxs=${withval} ;;
    esac

    if ${TEST} ${find_apxs} ; then
        AC_MSG_RESULT([need to check for Perl first, apxs depends on it...])
        AC_PATH_PROG(PERL,perl,$PATH)dnl

        if ${TEST} ${find_apxs} = true ; then
            AC_PATH_PROG(APXS,apxs,$PATH)dnl
        else
            APXS=${find_apxs}
        fi

        if ${TEST} -n "${APXS}" ; then
            dnl Seems that we have it, but have to check if it is OK first
            if ${TEST} ! -x "${APXS}" ; then
                AC_MSG_ERROR(Invalid location for apxs: '${APXS}')
            fi

            ${APXS} -q PREFIX >/dev/null 2>/dev/null || apxs_support=false

            if ${TEST} "${apxs_support}" = "false" ; then
                AC_MSG_RESULT(could not find ${APXS})
                AC_MSG_ERROR(You must specify a valid --with-apxs path)
            fi

            dnl apache_dir and apache_include are also needed.
            apache_dir=`$APXS -q PREFIX`
            apache_include="-I`$APXS -q INCLUDEDIR`"

            dnl test apache version
            APA=`${GREP} STANDARD20 ${APXS}`
            if ${TEST} -z "$APA" ; then
                AC_MSG_ERROR(No Apache 2.x found)
            fi
            WEBSERVER="apache-2.0"
            APRINCLUDEDIR=""
            INCTEMP="`$APXS -q APR_INCLUDEDIR` `$APXS -q APU_INCLUDEDIR`"
            for INC in ${INCTEMP}; do
                APRINCLUDEDIR="${APRINCLUDEDIR} -I${INC}"
            done
            AC_MSG_RESULT([APRINCLUDEDIR is $APRINCLUDEDIR])
            APXSCC="`$APXS -q CC`"
            APXSCFLAGS="`${APXS} -q CFLAGS` `${APXS} -q EXTRA_CFLAGS` -DHAVE_APR ${APRINCLUDEDIR}"
            APXSCPPFLAGS="`${APXS} -q EXTRA_CPPFLAGS`"
            APXSLDFLAGS="`$APXS -q LDFLAGS`"
            APACHE_CONFIG_VARS="`${APXS} -q exp_installbuilddir`/config_vars.mk"
            LIBTOOL=`$APXS -q LIBTOOL`
            AC_MSG_RESULT([building connector for \"$WEBSERVER\"])
            if ${TEST} -z "${CC}" ; then
                CC="${APXSCC}"
            else
                if ${TEST} "${CC}" != "$APXSCC" ; then
                    WARN_CC=1
                fi
            fi

            AC_SUBST(APXS)
        fi
    fi
],
[
    AC_MSG_RESULT(no apxs given)
])

AC_SUBST(APACHE_CONFIG_VARS)

AC_PROG_CC
AC_PROG_LD

SAVE_LIBTOOL="$LIBTOOL"

dnl Not sure what it does, but the libtool manual seems to require this
dnl It should use the native platform dlopen ( if available )
AC_LIBTOOL_DLOPEN

dnl AM_PROG_LIBTOOL often causes problems.
dnl I have solved them once using aclocal --acdir=/usr/local/share/aclocal/
AM_PROG_LIBTOOL

if ${TEST} -n "${SAVE_LIBTOOL}" ; then
    LIBTOOL="$SAVE_LIBTOOL"
fi
AC_MSG_RESULT([LIBTOOL="$LIBTOOL"])

AC_SUBST(LIBTOOL)

dnl ----------------------------- Checks for standard typedefs

dnl Checks for integer size
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(long double, 12)
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(longlong, 8)

# Now we need to find what jk_uint32_t (sizeof == 4) will be.
# The first match is our preference.
if test "$ac_cv_sizeof_int" = "4"; then
    int32_t_fmt='#define JK_INT32_T_FMT "d"'
    uint32_t_fmt='#define JK_UINT32_T_FMT "u"'
    uint32_t_hex_fmt='#define JK_UINT32_T_HEX_FMT "x"'
    int32_value="int"
elif test "$ac_cv_sizeof_long" = "4"; then
    int32_t_fmt='#define JK_INT32_T_FMT "ld"'
    uint32_t_fmt='#define JK_UINT32_T_FMT "lu"'
    uint32_t_hex_fmt='#define JK_UINT32_T_HEX_FMT "lx"'
    int32_value="long"
else
    int32_t_fmt='#error could not detect a 32-bit integer type'
    uint32_t_fmt='#error could not detect a 32-bit integer type'
    uint32_t_hex_fmt='#error could not detect a 32-bit integer type'
    AC_ERROR([could not detect a 32-bit integer type])
fi

# Now we need to find what jk_uint64_t (sizeof == 8) will be.
# The first match is our preference.
if test "$ac_cv_sizeof_int" = "8"; then
    int64_t_fmt='#define JK_INT64_T_FMT "d"'
    uint64_t_fmt='#define JK_UINT64_T_FMT "u"'
    uint64_t_hex_fmt='#define JK_UINT64_T_HEX_FMT "x"'
    int64_value="int"
elif test "$ac_cv_sizeof_long" = "8"; then
    int64_t_fmt='#define JK_INT64_T_FMT "ld"'
    uint64_t_fmt='#define JK_UINT64_T_FMT "lu"'
    uint64_t_hex_fmt='#define JK_UINT64_T_HEX_FMT "lx"'
    int64_value="long"
elif test "$ac_cv_sizeof_long_long" = "8"; then
    # Linux, Solaris, FreeBSD all support ll with printf.
    # BSD 4.4 originated 'q'.  Solaris is more popular and
    # doesn't support 'q'.  Solaris wins.  Exceptions can
    # go to the OS-dependent section.
    int64_t_fmt='#define JK_INT64_T_FMT "lld"'
    uint64_t_fmt='#define JK_UINT64_T_FMT "llu"'
    uint64_t_hex_fmt='#define JK_UINT64_T_HEX_FMT "llx"'
    int64_value="long long"
elif test "$ac_cv_sizeof_long_double" = "8"; then
    int64_t_fmt='#define JK_INT64_T_FMT "Ld"'
    uint64_t_fmt='#define JK_UINT64_T_FMT "Lu"'
    uint64_t_hex_fmt='#define JK_UINT64_T_HEX_FMT "Lx"'
    int64_value="long double"
elif test "$ac_cv_sizeof_longlong" = "8"; then
    int64_t_fmt='#define JK_INT64_T_FMT "qd"'
    uint64_t_fmt='#define JK_UINT64_T_FMT "qu"'
    uint64_t_hex_fmt='#define JK_UINT64_T_HEX_FMT "qx"'
    int64_value="__int64"
else
    int64_t_fmt='#error could not detect a 64-bit integer type'
    uint64_t_fmt='#error could not detect a 64-bit integer type'
    uint64_t_hex_fmt='#error could not detect a 64-bit integer type'
    AC_ERROR([could not detect a 64-bit integer type])
fi

JK_CHECK_SIZEOF_EXTENDED([#include <sys/types.h>], pid_t, 8)

if test "$ac_cv_sizeof_pid_t" = "$ac_cv_sizeof_short"; then
    pid_t_fmt='#define JK_PID_T_FMT "hd"'
elif test "$ac_cv_sizeof_pid_t" = "$ac_cv_sizeof_int"; then
    pid_t_fmt='#define JK_PID_T_FMT "d"'
elif test "$ac_cv_sizeof_pid_t" = "$ac_cv_sizeof_long"; then
    pid_t_fmt='#define JK_PID_T_FMT "ld"'
elif test "$ac_cv_sizeof_pid_t" = "$ac_cv_sizeof_long_long"; then
    pid_t_fmt='#define JK_PID_T_FMT JK_INT64_T_FMT'
else
    pid_t_fmt='#error Can not determine the proper size for pid_t'
fi

JK_CHECK_SIZEOF_EXTENDED([#include <pthread.h>], pthread_t, 8)

if test "$ac_cv_sizeof_pthread_t" = "$ac_cv_sizeof_short"; then
    pthread_t_fmt='#define JK_PTHREAD_T_FMT "hu"'
    pthread_t_value="short"
elif test "$ac_cv_sizeof_pthread_t" = "$ac_cv_sizeof_int"; then
    pthread_t_fmt='#define JK_PTHREAD_T_FMT "u"'
    pthread_t_value="int"
elif test "$ac_cv_sizeof_pthread_t" = "$ac_cv_sizeof_long"; then
    pthread_t_fmt='#define JK_PTHREAD_T_FMT "lu"'
    pthread_t_value="long"
elif test "$ac_cv_sizeof_pthread_t" = "$ac_cv_sizeof_long_long"; then
    pthread_t_fmt='#define JK_PTHREAD_T_FMT "llu"'
    pthread_t_value="long long"
else
    pthread_t_fmt='#error Can not determine the proper size for pthread_t'
fi

# Basically, we have tried to figure out the correct format strings
# for pid_t which varies between platforms, but we don't always get
# it right.  If you find that we don't get it right for your platform,
# you can override our decision below.
case $host in
   *-solaris*)
       if test "$ac_cv_sizeof_pid_t" = "$ac_cv_sizeof_long"; then
           pid_t_fmt='#define JK_PID_T_FMT "ld"'
       fi
       ;;
esac

AC_SUBST(int32_value)
AC_SUBST(int32_t_fmt)
AC_SUBST(uint32_t_fmt)
AC_SUBST(uint32_t_hex_fmt)
AC_SUBST(int64_value)
AC_SUBST(int64_t_fmt)
AC_SUBST(uint64_t_fmt)
AC_SUBST(uint64_t_hex_fmt)
AC_SUBST(pid_t_fmt)
AC_SUBST(pthread_t_fmt)
AC_SUBST(pthread_t_value)

dnl check for snprintf and vsnprintf.
AC_CHECK_FUNC(snprintf, AC_DEFINE(HAVE_SNPRINTF,1,[Have snprintf()]))
AC_CHECK_FUNC(vsnprintf, AC_DEFINE(HAVE_VSNPRINTF,1,[Have vsnprintf()]))
dnl check for flock function.
AC_CHECK_FUNC(flock, AC_DEFINE(HAVE_FLOCK,1,[Have flock()]))

dnl check for -lsocket library
AC_CHECK_LIB(socket, setsockopt, [LIBS="$LIBS -lsocket"])

dnl check for filio.h used on Solaris to define FIONREAD ioctl.
AC_CHECK_HEADERS(sys/filio.h)

AC_DEFUN([JK_CHECK_SETSOCKOPT], [
AC_MSG_CHECKING(whether to use $1 with setsockopt())
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>

int main(void)
{
    int s;
    struct timeval tv;
    tv.tv_sec  = 3;
    tv.tv_usec = 0;

#ifndef $1
    exit(3);
#else
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        exit(2);

    /* fails on Solaris 2.6,8,9,10 and some Linuxes because
       SO_RCVTIMEO|SO_SNDTIMEO are defined but not implemented */
    if (setsockopt(s, SOL_SOCKET, $1, (const void *)&tv, sizeof(tv)) == -1)
        exit(1);

    exit(0);
#endif
}
]
, [ AC_MSG_RESULT([yes]) AC_DEFINE(USE_$1, 1, [Define to use $1 with setsockopt()]) ]
, [ AC_MSG_RESULT([no]) ]
)
])dnl

dnl check for SO_RCVTIMEO and SO_SNDTIMEO
JK_CHECK_SETSOCKOPT(SO_RCVTIMEO)
JK_CHECK_SETSOCKOPT(SO_SNDTIMEO)

AC_DEFUN([JK_CHECK_SOCKOPT], [
AC_MSG_CHECKING(whether to use $1 with socket())
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>

int main(void)
{
    int s;

#ifndef $1
    exit(3);
#else
    if ((s = socket(AF_INET, SOCK_STREAM | $1, 0)) == -1)
        exit(2);

    exit(0);
#endif
}
]
, [ AC_MSG_RESULT([yes]) AC_DEFINE(USE_$1, 1, [Define to use $1 with socket()]) ]
, [ AC_MSG_RESULT([no]) ]
)
])dnl

AC_ARG_ENABLE(sock-cloexec,
[AS_HELP_STRING([--disable-sock-cloexec],[Disable use of SOCK_CLOEXEC.
This ensures the built module can be used on systems that do not support SOCK_CLOEXEC])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
	dnl check for SOCK_CLOEXEC
	JK_CHECK_SOCKOPT(SOCK_CLOEXEC)
        ;;
esac
],
[
	dnl check for SOCK_CLOEXEC
	JK_CHECK_SOCKOPT(SOCK_CLOEXEC)
])

dnl check for poll.h header
AC_CHECK_HEADERS(poll.h)
dnl check for poll function
AC_CHECK_FUNC(poll, AC_DEFINE(HAVE_POLL,1,[Have poll()]))
dnl check for netinet/in.h header
AC_CHECK_HEADERS(netinet/in.h)
dnl check for netdb.h header
AC_CHECK_HEADERS(netdb.h)

AC_DEFUN([JK_HAVE_IPV6], [
AC_MSG_CHECKING(for IPv6 with socket())
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>

int main(void)
{
    int s;

    if ((s = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
        exit(2);
    exit(0);
}
]
, [ AC_MSG_RESULT([yes]) AC_DEFINE(HAVE_AF_INET6, 1, [Define to 1 if you have IPv6 support]) ]
, [ AC_MSG_RESULT([no]) ]
)
])dnl

AC_DEFUN([JK_CHECK_SASTORAGE], [
AC_MSG_CHECKING(for struct sockaddr_storage)
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

int main(void)
{
    struct sockaddr_storage sa;
    exit(sizeof(sa) == 0);
}
]
, [ AC_MSG_RESULT([yes]) AC_DEFINE(HAVE_SOCKADDR_STORAGE, 1, [Define to 1 if you have struct sockaddr_storage]) ]
, [ AC_MSG_RESULT([no]) ]
)
])dnl

JK_HAVE_IPV6
JK_CHECK_SASTORAGE
AC_CHECK_FUNC(getaddrinfo, AC_DEFINE(HAVE_GETADDRINFO,1,[Have getaddrinfo()]))
AC_CHECK_FUNC(gethostbyname_r, AC_DEFINE(HAVE_GETHOSTBYNAME_R,1,[Have gethostbyname_r()]))

AC_DEFUN([JK_CHECK_ATOMICS], [
AC_MSG_CHECKING(whether the compiler provides atomic builtins)
AC_TRY_RUN([
int main()
{
    unsigned long val = 1010;

    if (__sync_add_and_fetch(&val, 1010) != 2020 || val != 2020)
        return 1;

    if (__sync_sub_and_fetch(&val, 1010) != 1010 || val != 1010)
        return 1;

    return 0;
}
]
, [ AC_MSG_RESULT([yes]) AC_DEFINE(HAVE_ATOMIC_BUILTINS, 1, [Define to 1 if the compiler provides atomic builtins]) ]
, [ AC_MSG_RESULT([no]) ]
)
])dnl

JK_CHECK_ATOMICS

dnl Apache-2.0 needs the os subdirectory to include os.h
dnl this include is copy from os/config.m4
sinclude(../support/os_apache.m4)

dnl it is copied from the configure of JServ ;=)
dnl and adapted.

apache_dir_is_src="false"
AC_ARG_WITH(apache,
[AS_HELP_STRING([--with-apache=DIR],
[Build static Apache module. DIR is the pathname
to the Apache source directory.])],
[
    if ${TEST} ! -z "$WEBSERVER" ; then
        AC_MSG_ERROR([Sorry cannot use --with-apxs=${APXS} and --with-apache=${withval} together, please choose one of both])
    fi

    AC_MSG_CHECKING([for Apache source directory (assume static build)])

    if ${TEST} -n "${withval}" && ${TEST} -d "${withval}" ; then
        if ${TEST} -d "${withval}/src" ; then
           # handle the case where people use relative paths to
           # the apache source directory by pre-pending the current
           # build directory to the path. there are probably
           # errors with this if configure is run while in a
           # different directory than what you are in at the time
           if ${TEST} -n "`${ECHO} ${withval}|${GREP} \"^\.\.\"`" ; then
               withval=`pwd`/${withval}
           fi

           apache_dir=${withval}
           apache_dir_is_src="true"
           AC_MSG_RESULT(${apache_dir})

           AC_MSG_CHECKING(for Apache include directory)

           if ${TEST} -d "${withval}/src/include" ; then
               # read osdir from the existing apache.
               osdir=`${GREP} '^OSDIR=' ${withval}/src/Makefile.config | ${SED} -e 's:^OSDIR=.*/os:os:'`
               if ${TEST} -z "${osdir}" ; then
                   osdir=os/unix
               fi
               apache_include="-I${withval}/src/include \
                   -I${withval}/src/${osdir}"
               WEBSERVER="apache-1.3"
               LIB_JK_TYPE=mod_jk.a
               CFLAGS="${CFLAGS} -DJK_PREFORK"
               AC_MSG_RESULT([${apache_include}, version 1.3])
           else
               AC_MSG_ERROR([Sorry Apache 1.2.x is no longer supported.])
           fi
        else
           if ${TEST} -d "${withval}/include" ; then
              # osdir for Apache20.
              WEBSERVER="apache-2.0"
              apache_dir=${withval}
              apache_dir_is_src="true"
              LIB_JK_TYPE=lib_jk.la
              apache_include="-I${withval}/include -I${withval}/srclib/apr/include -I${withval}/os/${OS_APACHE_DIR} -I${withval}/srclib/apr-util/include"
              AC_MSG_RESULT(${apache_dir})
           fi
        fi
    fi

    dnl Make sure we have a result.
    if ${TEST} -z "$WEBSERVER" ; then
        AC_MSG_ERROR([Directory $apache_dir is not a valid Apache source distribution])
    fi

# VT: Now, which one I'm supposed to use? Let's figure it out later

    configure_apache=true
    configure_src=true

    AC_MSG_RESULT([building connector for \"$WEBSERVER\"])
],
[
    AC_MSG_RESULT(no apache given)
])

AC_SUBST(apache_include)
APACHE_DIR=${apache_dir}
AC_SUBST(APACHE_DIR)

dnl CFLAGS for EAPI mod_ssl (Apache 1.3)
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(EAPI,
[AS_HELP_STRING([--enable-EAPI],[Enable EAPI support (mod_ssl, Apache 1.3)])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DEAPI"
        AC_MSG_RESULT([...Enabling EAPI Support...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl CFLAGS for maintainer mode
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(maintainer-mode,
[AS_HELP_STRING([--enable-maintainer-mode],[Turn on debugging and compile time warnings])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DDEBUG -Wall"
        AC_MSG_RESULT([...Enabling Maintainer mode...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl CFLAGS for prefork mode
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(prefork,
[AS_HELP_STRING([--enable-prefork],[Turn on prefork web server mode (single-threaded)])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DJK_PREFORK"
        AC_MSG_RESULT([...Enabling Prefork mode...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl CFLAGS for stripping TRACE logs
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(trace,
[AS_HELP_STRING([--disable-trace],[Exclude trace log code from compilation])],
[
case "${enableval}" in
    no )
        CFLAGS="${CFLAGS} -DJK_PRODUCTION"
        AC_MSG_RESULT([...Exclude trace log code...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl CFLAGS for building against recent httpd but without
dnl using httpd API functions, which didn't exist in the first
dnl production releases. This ensures, that the resulting
dnl module binary is compatible with older httpd releases.
dnl Until now only relevant for httpd 2.2.x with x >= 4.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(api-compatibility,
[AS_HELP_STRING([--enable-api-compatibility],
[Only use httpd API functions available in all production releases.
This improves binary compatibility of module builds with httpd releases
older than the release against we build (only between minor versions).])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DAPI_COMPATIBILITY"
        AC_MSG_RESULT([...Only using compatible httpd API...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl CFLAGS for shared memory lock mode
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(flock,
[AS_HELP_STRING([--enable-flock],[Turn on flock for shared locking if present])],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DJK_USE_FLOCK"
        AC_MSG_RESULT([...Enabling flock() shared memory locking...])
        ;;
esac
])
AC_SUBST(CFLAGS)

dnl the APXSCFLAGS is given by apxs to the C compiler
if ${TEST} -n "${CFLAGS}" ; then
    APXSCFLAGS="${APXSCFLAGS} ${CFLAGS}"
fi
AC_SUBST(APXSCFLAGS)
AC_SUBST(APXSCPPFLAGS)

dnl the APXSLDFLAGS is given to the linker (for APRVARS).
if ${TEST} -n "${LDFLAGS}" ; then
    APXSLDFLAGS="${APXSLDFLAGS} ${LDFLAGS}"
fi

dnl Prefix all LDFLAGS with "-Wl,",
dnl because we pass them to libtool
JK_PREFIX_IF_MISSING(APXSLDFLAGS, [-Wl,])

AC_SUBST(APXSLDFLAGS)

dnl Check that  a WEBSERVER has been given
if ${TEST} -z "$WEBSERVER" ; then
    AC_MSG_ERROR(Cannot find the WebServer)
fi

dnl Add common to subdir list
WEBSERVER="common ${WEBSERVER}"

AC_SUBST(WEBSERVER)

AM_CONDITIONAL(MAKE_DYNAMIC_APACHE, ${TEST} "${apache_dir_is_src}" = "false")

if ${TEST} "${apache_dir_is_src}" = "false" ; then
    dnl normal apxs handling
    APACHE20_OEXT=.c
    LIB_JK_TYPE=mod_jk.so
    INSTALL_TYPE=install_dynamic
else
    dnl install static library in apache sources.
    APACHE20_OEXT=.lo
    INSTALL_TYPE=install_static
fi
AC_SUBST(APACHE20_OEXT)
AC_SUBST(LIB_JK_TYPE)
AC_SUBST(INSTALL_TYPE)

dnl automake needs the path it does not work with $WEBSERVER
dnl that why useless Makefiles are build.
AC_OUTPUT([
    Makefile
    apache-2.0/Makefile
    apache-2.0/Makefile.apxs
    common/Makefile
    common/list.mk
    common/jk_types.h
])

if ${TEST} -n "${WARN_CC}" ; then
    AC_MSG_WARN([===========================================])
    AC_MSG_WARN([Using CC from environment:])
    AC_MSG_WARN([    CC="$CC"])
    AC_MSG_WARN([instead of CC from apxs:])
    AC_MSG_WARN([    CC="$APXSCC"])
    AC_MSG_WARN([If "make" throws an error of the form])
    AC_MSG_WARN([    "libtool: compile: unable to infer tagged configuration"])
    AC_MSG_WARN([    "libtool: compile: specify a tag with `--tag'"])
    AC_MSG_WARN([try running configure without setting CC,])
    AC_MSG_WARN([or at least CC should start with "$APXSCC"])
    AC_MSG_WARN([===========================================])
fi
