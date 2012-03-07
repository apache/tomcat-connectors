#! /bin/sh

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

echo "buildconf: checking installation..."

# autoconf 2.59 or newer
ac_version=`${AUTOCONF:-autoconf} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;q'`
if test -z "$ac_version"; then
echo "buildconf: autoconf not found."
echo "           You need autoconf version 2.59 or newer installed"
echo "           to build mod_jk from SVN."
exit 1
fi
IFS=.; set $ac_version; IFS=' '
if test "$1" = "2" -a "$2" -lt "59" || test "$1" -lt "2"; then
echo "buildconf: autoconf version $ac_version found."
echo "           You need autoconf version 2.59 or newer installed"
echo "           to build mod_jk from SVN."
exit 1
else
echo "buildconf: autoconf version $ac_version (ok)"
fi

ac_version=`${LIBTOOL:-libtool} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;s/(.*//;q'`
if test -z "$ac_version"; then
echo "buildconf: libtool not found."
echo "           You need libtool version 1.4 or newer installed"
echo "           to build mod_jk from SVN."
exit 1
fi
IFS=.; set $ac_version; IFS=' '
if test "$1" = "1" -a "$2" -lt "4" || test "$1" -lt "1"; then
echo "buildconf: libtool version $ac_version found."
echo "           You need libtool version 1.4 or newer installed"
echo "           to build mod_jk from SVN."
exit 1
else
echo "buildconf: libtool  version $ac_version (ok)"
fi

exit 0
