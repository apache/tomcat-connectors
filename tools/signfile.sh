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

# Try to locate a SHA512 binary
sha512_bin="`which sha512sum 2>/dev/null || type sha512sum 2>&1`"
if [ -x "$sha512_bin" ]; then
    SHA512SUM="$sha512_bin --binary "
else
    SHA512SUM="echo 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 "
fi

for o
do
    echo "Signing $o"
    gpg $gpgopts $o
    $SHA512SUM $o > $o.sha512
done

