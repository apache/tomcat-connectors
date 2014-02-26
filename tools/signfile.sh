#!/bin/bash
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

gpgopts="-ba"
for o
do
    case "$o" in
    *=*) a=`echo "$o" | sed 's/^[-_a-zA-Z0-9]*=//'`
     ;;
    *) a=''
     ;;
    esac
    case "$o" in
        --default-key=*  )
            gpgopts="$gpgopts --default-key $a"
            shift
        ;;
        --passphrase=*  )
            gpgopts="$gpgopts --passphrase $a"
            shift
        ;;
        * )
            break
        ;;
    esac
done

# Try to locate a MD5 binary
md5_bin="`which md5sum 2>/dev/null || type md5sum 2>&1`"
if [ -x "$md5_bin" ]; then
    MD5SUM="$md5_bin --binary "
else
    MD5SUM="echo 00000000000000000000000000000000 "
fi
# Try to locate a SHA1 binary
sha1_bin="`which sha1sum 2>/dev/null || type sha1sum 2>&1`"
if [ -x "$sha1_bin" ]; then
    SHA1SUM="$sha1_bin --binary "
else
    SHA1SUM="echo 0000000000000000000000000000000000000000 "
fi

for o
do
    echo "Signing $o"
    gpg $gpgopts $o
    $MD5SUM $o > $o.md5 
    $SHA1SUM $o > $o.sha1
done

