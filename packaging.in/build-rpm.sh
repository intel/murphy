#!/bin/bash

set -e

PKG=murphy
VERSION=`date +'%Y%m%d'`
TMPDIR=/tmp/$USER
TMP=$TMPDIR/$PKG-$VERSION
RPMDIR=$(basename $(pwd))

# roll a tarball by hand
function manual_tarball() {
    echo "* Building RPM from *working copy*, hit Ctrl-C to stop..."; sleep 1
    mkdir -p $TMP && pushd .. > /dev/null && \
        tar -cf - . | tar -C $TMP -xvf - && \
        tar -C $TMPDIR -cvzf $PKG-$VERSION.tar.gz $PKG-$VERSION && \
        mv $PKG-$VERSION.tar.gz $RPMDIR && \
    popd > /dev/null
}

# roll a tarball by git
function git_tarball() {
    local _version="${1:-HEAD}"

    echo "* Building RPM from *git $_version*, hit Ctrl-C to stop..."; sleep 1
    pushd .. > /dev/null && \
        git archive --prefix=$PKG-$VERSION/ $_version > $PKG-$VERSION.tar && \
        gzip $PKG-$VERSION.tar && \
        mv $PKG-$VERSION.tar.gz $RPMDIR && \
    popd > /dev/null
}

# roll a tarball
function make_tarball() {
    if [ -d .git -o -d ../.git -a "$1" != "current" ]; then
        git_tarball $1
    else
        manual_tarball
    fi
}


# set up defaults
GIT=""

# parse the command line
while [ -n "$1" -a "${1#-}" != "$1" ]; do
    case $1 in
        --git|-g)
            shift
            if [ -n "$1" -a "${1#-}" = "$1" ]; then
                GIT="$1"
                shift
            else
                GIT="HEAD"
            fi
            ;;
        --help|-h)
            echo "$0 [--git <git-version>]"
            exit 0
            ;;
    esac
done


if [ -n "$GIT" ]; then
    git_tarball $GIT
else
    manual_tarball
fi

# patch up spec file
sed "s/@VERSION@/$VERSION/g" $PKG.spec.in > $PKG.spec

# put it all in place and try to build rpm(s)
mv $PKG-$VERSION.tar.gz ~/rpmbuild/SOURCES
mv $PKG.spec ~/rpmbuild/SPECS

rpmbuild $* -bb ~/rpmbuild/SPECS/$PKG.spec
mv -v ~/rpmbuild/RPMS/*/$PKG*$VERSION* .
