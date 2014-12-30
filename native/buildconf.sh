#!/bin/sh

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

scripts/build/unix/buildcheck.sh || exit 1

rm -rf autom4te.cache 2>/dev/null || true
echo "buildconf: ${LIBTOOLIZE:-libtoolize} --automake --copy"
${LIBTOOLIZE:-libtoolize} --automake --copy
echo "buildconf: aclocal"
#aclocal --acdir=`aclocal --print-ac-dir`
#aclocal --acdir=/usr/local/share/aclocal
aclocal
echo "buildconf: autoheader"
autoheader
echo "buildconf: automake -a --foreign --copy"
automake -a --foreign --copy
echo "buildconf: autoconf"
autoconf
rm -rf autom4te.cache
