#!/bin/bash
#
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


# Make sure to set your path so that we can find
# the following binaries:
# cd, mkdir, cp, rm, find
# svn
# ant
# libtoolize, aclocal, autoheader, automake, autoconf
# tar, zip, gzip
# gpg
# And any one of: w3m, elinks, links (links2)

SVNROOT="http://svn.apache.org/repos/asf"
SVNPROJ="tomcat/jk"
JK_CVST="tomcat-connectors"
JK_OWNER="root"
JK_GROUP="bin"
JK_TOOLS="`pwd`"

COPY_JK="README.txt HOWTO-RELEASE.txt .gitignore native jkstatus support tools xdocs"
COPY_NATIVE="LICENSE NOTICE"
COPY_BUILD="docs"
COPY_CONF="httpd-jk.conf uriworkermap.properties workers.properties"
SIGN_OPTS=""

#################### NO CHANGE BELOW THIS LINE ##############

#################### FUNCTIONS ##############

usage() {
    echo "Usage:: $0 -v VERSION [-f] [-r revision] [-t tag | -b BRANCH | -T | -d DIR]"
    echo "        -v: version to package"
    echo "        -f: force, do not validate tag against version"
    echo "        -h: create text documentation for html"
    echo "        -t: tag to use if different from version"
    echo "        -r: revision to package"
    echo "        -b: package from branch BRANCH"
    echo "        -T: package from trunk"
    echo "        -d: package from local directory"
    echo "        -o: owner used for creating tar archive"
    echo "        -g: group used for creating tar archive"
    echo "        -p: GNU PG passphrrase used for signing"
    echo "        -k: ID of GNU PG key to use for signing"
}

copy_files() {
    src=$1
    target=$2
    list="$3"

    mkdir -p $target
    for item in $list
    do
        echo "Copying $item from $src ..."
        cp -pr $src/$item $target/
    done
}

#################### MAIN ##############

txtgen=n
conflict=0
while getopts :v:t:r:b:d:p:k:o:g:Tfh c
do
    case $c in
    v)         version=$OPTARG;;
    t)         tag=$OPTARG
               conflict=$(($conflict+1));;
    r)         revision=$OPTARG;;
    k)         SIGN_OPTS="--default-key=$OPTARG $SIGN_OPTS";;
    p)         SIGN_OPTS="--passphrase=$OPTARG $SIGN_OPTS";;
    o)         JK_OWNER=$OPTARG;;
    g)         JK_GROUP=$OPTARG;;
    b)         branch=$OPTARG
               conflict=$(($conflict+1));;
    T)         trunk=trunk
               conflict=$(($conflict+1));;
    d)         local_dir=$OPTARG
               conflict=$(($conflict+1));;
    f)         force='y';;
    h)         txtgen='y';;
    \:)        usage
               exit 2;;
    \?)        usage
               exit 2;;
    esac
done
shift `expr $OPTIND - 1`

if [ $conflict -gt 1 ]
then
    usage
    echo "Only one of the options '-t', '-b', '-T'  and '-d' is allowed."
    exit 2
fi

if [ -n "$local_dir" ]
then
    echo "Caution: Packaging from directory!"
    echo "Make sure the directory is committed."
    answer="x"
    while [ "$answer" != "y" -a "$answer" != "n" ]
    do
        echo "Do you want to proceed? [y/n]"
        read answer
    done
    if [ "$answer" != "y" ]
    then
        echo "Aborting."
        exit 4
    fi
fi

if [ -z "$version" ]
then
    usage
    exit 2
fi
if [ -n "$revision" ]
then
    revision="-r $revision"
fi
if [ -n "$trunk" ]
then
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/trunk"
    svn_url_info="`svn help info | grep URL`"
    if [ -n "$svn_url_info" ]
    then
	JK_SVN_INFO="${JK_SVN_URL}"
    else
	JK_SVN_INFO=.
    fi
    JK_REV=`svn info $revision $JK_SVN_INFO | awk '$1 == "Revision:" {print $2}'`
    if [ -z "$JK_REV" ]
    then
       echo "No Revision found at '$JK_SVN_URL'"
       exit 3
    fi
    JK_SUFFIX=-${JK_REV}
    JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
elif [ -n "$branch" ]
then
    JK_BRANCH=`echo $branch | sed -e 's#/#__#g'`
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/branches/$branch"
    JK_REV=`svn info $revision ${JK_SVN_URL} | awk '$1 == "Revision:" {print $2}'`
    if [ -z "$JK_REV" ]
    then
       echo "No Revision found at '$JK_SVN_URL'"
       exit 3
    fi
    JK_SUFFIX=-${JK_BRANCH}-${JK_REV}
    JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
elif [ -n "$local_dir" ]
then
    JK_SVN_URL="$local_dir"
    JK_REV=`svn info $revision ${JK_SVN_URL} | awk '$1 == "Revision:" {print $2}'`
    if [ -z "$JK_REV" ]
    then
       echo "No Revision found at '$JK_SVN_URL'"
       exit 3
    fi
    JK_SUFFIX=-local-`date +%Y%m%d%H%M%S`-${JK_REV}
    JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
else
    JK_VER=$version
    JK_TAG=`echo $version | sed -e 's#^#JK_#' -e 's#\.#_#g'`
    if [ -n $tag ]
    then
        if [ -z $force ]
        then
            echo $tag | grep "^$JK_TAG" > /dev/null 2>&1
            if [ $? -gt 0 ]
            then
                echo "Tag '$tag' doesn't belong to version '$version'."
                echo "Force using '-f' if you are sure."
                exit 5
            fi
        fi
        JK_TAG=$tag
    fi
    JK_SVN_URL="${SVNROOT}/${SVNPROJ}/tags/${JK_TAG}"
    JK_REV=`svn info $revision ${JK_SVN_URL} | awk '$1 == "Revision:" {print $2}'`
    JK_SUFFIX=" ($JK_REV)"
    JK_DIST=${JK_CVST}-${JK_VER}-src
fi

echo "Using subversion URL: $JK_SVN_URL"
echo "Rolling into file $JK_DIST.*"
sleep 2

umask 022

rm -rf ${JK_DIST} 2>/dev/null || true
rm -rf ${JK_DIST}.* 2>/dev/null || true

mkdir -p ${JK_DIST}.tmp
svn export $revision "${JK_SVN_URL}" ${JK_DIST}.tmp/jk
if [ $? -ne 0 ]; then
  echo "svn export failed"
  exit 1
fi

# Build documentation.
cd ${JK_DIST}.tmp/jk/xdocs
ant
cd ../../..

# Copying things into the source distribution
copy_files ${JK_DIST}.tmp/jk $JK_DIST "$COPY_JK"
copy_files ${JK_DIST}.tmp/jk/native $JK_DIST "$COPY_NATIVE"
copy_files ${JK_DIST}.tmp/jk/build $JK_DIST "$COPY_BUILD"
copy_files ${JK_DIST}.tmp/jk/conf $JK_DIST/conf "$COPY_CONF"

# Remove extra directories and files
targetdir=${JK_DIST}
rm -rf ${targetdir}/xdocs/jk2
rm -f ${targetdir}/native/build.xml
rm -f ${targetdir}/native/NOTICE
rm -f ${targetdir}/native/LICENSE
find ${JK_DIST} -name .cvsignore -exec rm -rf \{\} \; 
find ${JK_DIST} -name CVS -exec rm -rf \{\} \; 
find ${JK_DIST} -name .svn -exec rm -rf \{\} \; 

cd ${JK_DIST}/native

if [ $txtgen = y ]
then
# Check for links, elinks or w3m
W3MOPTS="-dump -cols 80 -t 4 -S -O iso-8859-1 -T text/html"
ELNKOPTS="-dump -dump-width 80 -dump-charset iso-8859-1 -no-numbering -no-home -no-references"
LNKOPTS="-dump -width 80 -codepage iso-8859-1 -no-g -html-numbered-links 0"
LYXOPTS="-dump -width=80 -nolist -nostatus -noprint -assume_local_charset=iso-8859-1"
failed=true
for tool in `echo "w3m elinks links lynx"`
do
  found=false
  for dir in `echo ${PATH} | sed 's!^:!.:!;s!:$!:.!;s!::!:.:!g;s!:! !g'`
  do
    if [ -x ${dir}/${tool} ]
    then
      found=true
      break
    fi
  done

  # Try to run it
  if ${found}
  then
    case ${tool} in
      w3m)
        TOOL="w3m $W3MOPTS"
        ;;
      links)
        TOOL="links $LNKOPTS"
        ;;
      elinks)
        TOOL="elinks $ELNKOPTS"
        ;;
      lynx)
        TOOL="lynx $LYXOPTS"
        ;;
    esac
    rm -f CHANGES
    echo "Creating the CHANGES file using '$TOOL' ..."
    ${TOOL} ../docs/miscellaneous/printer/changelog.html > CHANGES 2>/dev/null
    if [ -f CHANGES -a -s CHANGES ]
    then
      failed=false
      break
    fi
  fi
done
if [ ${failed} = "true" ]
then
  echo "Can't convert html to text (CHANGES)"
  exit 1
fi

# Export text docs
echo "Creating the NEWS file using '$TOOL' ..."
rm -f NEWS
touch NEWS
for news in `ls -r ../xdocs/news/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].xml`
do
  print=`echo $news | sed -e 's#xdocs/news#docs/news/printer#' -e 's#\.xml#.html#'`
  echo "Adding $print to NEWS file ..."
  ${TOOL} $print >>NEWS
done
if [ ! -s NEWS ]
then
  echo "Can't convert html to text (NEWS)"
  exit 1
fi
fi

# Generate configure et. al.
./buildconf.sh
cd ../../

# Pack
tar cfz ${JK_DIST}.tar.gz --owner="${JK_OWNER}" --group="${JK_GROUP}" ${JK_DIST} || exit 1
perl ${JK_DIST}/tools/lineends.pl --ignore '.*/pcre/testdata/.*' --cr ${JK_DIST}
zip -9 -r ${JK_DIST}.zip ${JK_DIST}

# Create detached signature and verify it
archive=${JK_DIST}.tar.gz
. ${JK_TOOLS}/signfile.sh ${SIGN_OPTS} $archive
archive=${JK_DIST}.zip
. ${JK_TOOLS}/signfile.sh ${SIGN_OPTS} $archive
# Cleanup working dirs
rm -rf ${JK_DIST}.tmp
rm -rf ${JK_DIST}
