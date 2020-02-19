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
# svn or git
# ant
# libtoolize, aclocal, autoheader, automake, autoconf
# tar, zip, gzip
# gpg
# And any one of: w3m, elinks, links (links2)

SVN_REPOS="http://svn.apache.org/repos/asf/tomcat/jk"
GIT_REPOS="https://gitbox.apache.org/repos/asf/tomcat-connectors.git"
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
    echo "Usage:: $0 -R (git|svn) -v VERSION [-f] [-r revision_or_hash] [-t tag | -b BRANCH | -T | -d DIR]"
    echo "        -R: Use git or svn to check out from repos"
    echo "        -v: version to package"
    echo "        -f: force, do not validate tag against version"
    echo "        -h: create text documentation for html"
    echo "        -t: tag to use if different from version"
    echo "        -r: revision or hash to package, only allowed in"
    echo "            combination with '-b BRANCH', '-T' or '-d DIR'"
    echo "        -b: package from branch BRANCH"
    echo "        -T: package from trunk/master"
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
rev_allowed=0
while getopts :R:v:t:r:b:d:p:k:o:g:Tfh c
do
    case $c in
    R)         repos=$OPTARG;;
    v)         version=$OPTARG;;
    t)         tag=$OPTARG
               conflict=$(($conflict+1));;
    r)         revision=$OPTARG;;
    k)         SIGN_OPTS="--default-key=$OPTARG $SIGN_OPTS";;
    p)         SIGN_OPTS="--passphrase=$OPTARG $SIGN_OPTS";;
    o)         JK_OWNER=$OPTARG;;
    g)         JK_GROUP=$OPTARG;;
    b)         branch=$OPTARG
               conflict=$(($conflict+1))
               rev_allowed=1;;
    T)         trunk=trunk
               conflict=$(($conflict+1))
               rev_allowed=1;;
    d)         local_dir=$OPTARG
               conflict=$(($conflict+1))
               rev_allowed=1;;
    f)         force='y';;
    h)         txtgen='y';;
    \:)        usage
               exit 2;;
    \?)        usage
               exit 2;;
    esac
done
shift `expr $OPTIND - 1`

if [ "X$repos" == "Xgit" ]
then
    USE_GIT=1
    REPOS=$GIT_REPOS
    JK_REPOS_URL=$REPOS
elif [ "X$repos" == "Xsvn" ]
then
    USE_GIT=0
    REPOS=$SVN_REPOS
else
    usage
    echo "Option '-R git' or '-R svn' must be set."
    exit 2
fi

if [ $conflict -gt 1 ]
then
    usage
    echo "Only one of the options '-t', '-b', '-T'  and '-d' is allowed."
    exit 2
fi

if [ -n "$revision" ]
then
    if [ $rev_allowed -eq 0 ]
    then
        usage
        echo "Option '-r revision' only allowed in combination with '-b BRANCH', '-T' or '-d DIR'"
        exit 2
    fi
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
    if [ $USE_GIT -eq 0 ]
    then
        revision="-r $revision"
    fi
fi
if [ -n "$trunk" ]
then
    if [ $USE_GIT -eq 1 ]
    then
        JK_REV=`git ls-remote $REPOS refs/heads/master | awk '{print $1}'`
        if [ -z "$JK_REV" ]
        then
           echo "No git hash found via 'git ls-remote $REPOS refs/heads/master'"
           exit 3
        fi
        JK_SUFFIX=-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    else
        JK_REPOS_URL="${REPOS}/trunk"
        repos_use_url="`svn help info | grep URL`"
        if [ -n "$repos_use_url" ]
        then
            JK_REPOS_INFO_PATH="${JK_REPOS_URL}"
        else
            JK_REPOS_INFO_PATH=.
        fi
        JK_REV=`svn info $revision $JK_REPOS_INFO_PATH | awk '$1 == "Revision:" {print $2}'`
        if [ -z "$JK_REV" ]
        then
           echo "No svn revision found at '$JK_REPOS_URL'"
           exit 3
        fi
        JK_SUFFIX=-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    fi
elif [ -n "$branch" ]
then
    if [ $USE_GIT -eq 1 ]
    then
        JK_REV=`git ls-remote $REPOS refs/heads/$branch | awk '{print $1}'`
        if [ -z "$JK_REV" ]
        then
           echo "No git hash found via 'git ls-remote $REPOS refs/heads/$branch'"
           exit 3
        fi
        JK_SUFFIX=-${JK_BRANCH}-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    else
        JK_BRANCH=`echo $branch | sed -e 's#/#__#g'`
        JK_REPOS_URL="${REPOS}/branches/$branch"
        JK_REV=`svn info $revision ${JK_REPOS_URL} | awk '$1 == "Revision:" {print $2}'`
        if [ -z "$JK_REV" ]
        then
           echo "No svn revision found at '$JK_REPOS_URL'"
           exit 3
        fi
        JK_SUFFIX=-${JK_BRANCH}-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    fi
elif [ -n "$local_dir" ]
then
    if [ ! -d "$local_dir" ]
    then
       echo "Directory '$local_dir' does not exist - Aborting!"
       exit 6
    fi
    if [ $USE_GIT -eq 1 ]
    then
        JK_REV=`git --git-dir=$local_dir rev-parse HEAD`
        if [ -z "$JK_REV" ]
        then
           echo "No git hash found via 'git rev-parse --short HEAD' in `pwd`"
           exit 3
        fi
        JK_SUFFIX=-local-`date +%Y%m%d%H%M%S`-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    else
        JK_REPOS_URL="$local_dir"
        JK_REV=`svn info $revision ${JK_REPOS_URL} | awk '$1 == "Revision:" {print $2}'`
        if [ -z "$JK_REV" ]
        then
           echo "No svn revision found at '$JK_REPOS_URL'"
           exit 3
        fi
        JK_SUFFIX=-local-`date +%Y%m%d%H%M%S`-${JK_REV}
        JK_DIST=${JK_CVST}-${version}-dev${JK_SUFFIX}-src
    fi
else
    JK_TAG=`echo $version | sed -e 's#^#JK_#' -e 's#\.#_#g'`
    if [ $USE_GIT -eq 1 ]
    then
        if [ -n "$tag" ]
        then
            if [ -z "$force" ]
            then
                echo $tag | grep "^$JK_TAG" > /dev/null 2>&1
                if [ "X$tag" != "X$JK_TAG" ]
                then
                    echo "Tag '$tag' doesn't belong to version '$version'."
                    echo "Force by using '-f' if you are sure."
                    exit 5
                fi
            fi
            JK_REV=`git ls-remote $REPOS refs/tags/$tag | awk '{print $1}'`
            if [ -z "$JK_REV" ]
            then
               echo "No git hash found via 'git ls-remote $REPOS refs/tags/$tag'"
               exit 3
            fi
            JK_SUFFIX=-tag-${tag}-${JK_REV}
        else
            JK_REV=`git ls-remote $REPOS refs/tags/$JK_TAG | awk '{print $1}'`
            if [ -z "$JK_REV" ]
            then
               echo "No git hash found via 'git ls-remote $REPOS refs/tags/$JK_TAG'"
               exit 3
            fi
            JK_SUFFIX=''
        fi
        JK_DIST=${JK_CVST}-${version}${JK_SUFFIX}-src
    else
        if [ -n "$tag" ]
        then
            if [ -z "$force" ]
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
        JK_REPOS_URL="${REPOS}/tags/${JK_TAG}"
        JK_DIST=${JK_CVST}-${version}-src
    fi
fi

echo "Using checkout URL: $JK_REPOS_URL"
echo "Rolling into file $JK_DIST.*"
sleep 2

umask 022

rm -rf ${JK_DIST} 2>/dev/null || true
rm -rf ${JK_DIST}.* 2>/dev/null || true

mkdir -p ${JK_DIST}.tmp
if [ $USE_GIT -eq 0 ]
then
    svn export $revision "${JK_REPOS_URL}" ${JK_DIST}.tmp/jk
    if [ $? -ne 0 ]
    then
      echo "svn export failed"
      exit 1
    fi
else
    if [ -n "$local_dir" ]
    then
        git --git-dir=$work_space/.git --work-tree=${JK_DIST}.tmp/jk checkout $JK_REV
        if [ $? -ne 0 ]
        then
          echo "git checkout for version $version hash '$JK_REV' from local '$work_space/.git' to directory '${JK_DIST}.tmp/jk' failed"
          exit 1
        fi
    else
        git clone --no-checkout "${JK_REPOS_URL}" ${JK_DIST}.tmp/jk
        if [ $? -ne 0 ]
        then
          echo "git clone '${JK_REPOS_URL}' to '${JK_DIST}.tmp/jk' failed"
          exit 1
        fi
        git --git-dir=${JK_DIST}.tmp/jk/.git --work-tree=${JK_DIST}.tmp/jk checkout $JK_REV
        if [ $? -ne 0 ]
        then
          echo "git checkout for version $version hash '$JK_REV' from cloned '${JK_REPOS_URL}' in directory '${JK_DIST}.tmp/jk' failed"
          exit 1
        fi
    fi
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
find ${JK_DIST} -name .git -exec rm -rf \{\} \; 

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
