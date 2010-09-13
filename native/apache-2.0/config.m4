dnl -------------------------------------------------------- -*- autoconf -*-
dnl Licensed to the Apache Software Foundation (ASF) under one or more
dnl contributor license agreements.  See the NOTICE file distributed with
dnl this work for additional information regarding copyright ownership.
dnl The ASF licenses this file to You under the Apache License, Version 2.0
dnl (the "License"); you may not use this file except in compliance with
dnl the License.  You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.

AC_MSG_CHECKING(for mod_jk module)
AC_ARG_WITH(mod_jk,
  [  --with-mod_jk  Include the mod_jk (static).],
  [
    modpath_current=modules/jk
    module=jk
    libname=lib_jk.la
    BUILTIN_LIBS="$BUILTIN_LIBS $modpath_current/$libname"
    cat >>$modpath_current/modules.mk<<EOF
DISTCLEAN_TARGETS = modules.mk
static = $libname
shared =
EOF
  MODLIST="$MODLIST $module"
  AC_MSG_RESULT(added $withval)
  ],
  [ AC_MSG_RESULT(no mod_jk added)
  ])
