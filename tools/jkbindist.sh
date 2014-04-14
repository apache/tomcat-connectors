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

#
# Create windows binary distribution archive
#
prefix="tomcat-connectors"
tools="`pwd`"
sign=""

#################### NO CHANGE BELOW THIS LINE ##############

#################### FUNCTIONS ##############

usage() {
    echo "Usage:: $0 -v VERSION -w WEBSERVER -o OS -a ARCH <module file>"
    echo "        -v: version to package"
    echo "        -w: package for web server"
    echo "        -o: Operating System"
    echo "        -a: Architecture"
    echo "        -p: GNU PG passphrrase used for signing"
    echo "        -k: ID of GNU PG key to use for signing"
}

while getopts :v:w:o:a:p:k: c
do
    case $c in
    v)         version=$OPTARG;;
    w)         websrv=$OPTARG;;
    k)         sign="--default-key=$OPTARG $sign";;
    p)         sign="--passphrase=$OPTARG $sign";;
    o)         opsys=$OPTARG;;
    a)         arch=$OPTARG;;
    \:)        usage
               exit 2;;
    \?)        usage
               exit 2;;
    esac
done
shift `expr $OPTIND - 1`

if [ -z "$version" -o -z "$websrv" ]
then
    usage
    exit 2
fi
if [ -z "$opsys" ]
then
    opsys="`uname -s | tr [A-Z] [a-z]`"
    case "$opsys" in
        cygwin*)
            opsys=windows
        ;;
    esac
fi
if [ -z "$arch" ]
then
    arch="`uname -m`"
fi

if [ ! -f "$1" ]
then
    usage
    exit 2
fi

case "$websrv" in
    httpd*)
        webdesc="Apache HTTP Server"
    ;;
    iis*)
        webdesc="Microsoft IIS Web Server"
    ;;
    netscape*|nsapi*)
        webdesc="Oracle iPlanet Web Server"
    ;;
    *)
        echo "Unknown web server: $webserv"
        echo "   Supported are: httpd, iis, nsapi"
    ;;
esac

dist=${prefix}-${version}-${opsys}-${arch}-${websrv}
dtop=${tools}/..
copy="LICENSE NOTICE"

rm -f ${copy} 2>/dev/null
rm -f ${dist} 2>/dev/null
rm -f ${dist}.* 2>/dev/null
cat << EOF > README
Apache Tomcat Connectors - $version

Here you'll find module binaries for $webdesc.
Check the online documentation at http://tomcat.apache.org/connectors-doc/
for installation instructions.

The Apache Tomcat Project
http://tomcat.apache.org
EOF

umask 022
for i in ${copy}
do
    if [ -f ${dtop}/$i ]
    then
        cp ${dtop}/$i .
    else
        cp ${dtop}/native/$i .
    fi
    unix2dos $i
done
unix2dos README
#chmod 755 $1
# Pack
archive=${dist}.zip
zip -9 -j ${archive} $@ README ${copy}
# Sign
. ${tools}/signfile.sh ${sign} ${archive}

# Cleanup
rm -f README ${copy} 2>/dev/null
