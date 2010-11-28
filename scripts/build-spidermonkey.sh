#! /bin/bash

set -e

SPIDERMONKEY_SRCDIR=$(cd "$1" && pwd)
BUILDDIR=$(cd "$2" && pwd)
SPIDERMONKEY_BUILDDIR="$BUILDDIR/build-spidermonkey"

function die() {
    echo "$*" 1>&2
    exit 1
}

function log() {
    #echo "$*"
    true
}

log "Configuring and compiling internal copy of SpiderMonkey"

if ! test -e "$SPIDERMONKEY_SRCDIR"/configure ; then
    AUTOCONF=autoconf2.13
    type $AUTOCONF || AUTOCONF=autoconf-2.13
    type $AUTOCONF || die "could not find ancient autoconf 2.13"

    (cd "$SPIDERMONKEY_SRCDIR" && $AUTOCONF) || die "could not autoconf"
fi

mkdir -p "$SPIDERMONKEY_BUILDDIR" || die "could not create $SPIDERMONKEY_BUILDDIR"
cd "$SPIDERMONKEY_BUILDDIR" || die "could not cd $SPIDERMONKEY_BUILDDIR"

CONFIGURE_CREATED_FILES="js-config.h js-config js-confdefs.h"
NEED_RECONFIGURE=false

for FILE in $CONFIGURE_CREATED_FILES ; do
    if ! test -e "$FILE" ; then
        log "$FILE does not exist, will have to configure SpiderMonkey"
        NEED_RECONFIGURE=true
    elif test "$SPIDERMONKEY_SRCDIR"/configure -nt "$FILE" ; then
        log "$SPIDERMONKEY_SRCDIR"/configure newer than "$FILE"
        NEED_RECONFIGURE=true
    fi
done

if $NEED_RECONFIGURE; then
    ## In a production build, we'll need to --disable-debug here.
    "$SPIDERMONKEY_SRCDIR"/configure --enable-threadsafe --with-system-nspr --enable-gczeal --enable-debug-symbols --enable-debug || die "failed to configure spidermonkey"


    # if js-config.h is unchanged then spidermonkey doesn't modify
    # its timestamp which means we keep rebuilding it over and over
    for FILE in $CONFIGURE_CREATED_FILES ; do
        if ! test -e "$FILE" ; then
            die "No $FILE was created"
        else
            log "$FILE created OK"
        fi
        touch "$FILE"
    done
    log "Configured SpiderMonkey"
else
    log "No need to reconfigure SpiderMonkey"
fi

## We have to build spidermonkey in BUILT_SOURCES to get js-config.h
## this leads to re-running make all the time and spidermonkey always
## does stuff on every make. So manually see if we can bail out
NEED_REBUILD=false
if ! test -e "$BUILDDIR"/libhwfjs.so ; then
    NEED_REBUILD=true
else
    for FILE in $CONFIGURE_CREATED_FILES "$SPIDERMONKEY_SRCDIR"/*.h "$SPIDERMONKEY_SRCDIR"/*.cpp ; do
        if test "$FILE" -nt "$BUILDDIR"/libhwfjs.so ; then
            NEED_REBUILD=true
        fi
    done
fi

if ! $NEED_REBUILD ; then
    log "SpiderMonkey up to date"
    exit 0
fi

# don't use log here so it shows up by default
echo "  SpiderMonkey needs building, may take a while..."
make 1>/dev/null || die "failed to build spidermonkey"
log "SpiderMonkey built successfully"

if test "$SPIDERMONKEY_BUILDDIR"/libmozjs.so -nt "$BUILDDIR"/libhwfjs.so ; then
    cp -f "$SPIDERMONKEY_BUILDDIR"/libmozjs.so "$BUILDDIR"/libhwfjs.so || die "could not copy .so into place"
fi

## this "hex edit with perl" hack has many issues, among them the new name of the lib
## has to be the same length as the old one. But, works for now. Without this,
## binaries pick up the name libmozjs.so as a dependency, even though they are
## linked to a file called libhwfjs.so
perl -pi -e 's/libmozjs\.so/libhwfjs\.so/g' "$BUILDDIR"/libhwfjs.so || die "could not rename libmozjs"

log "Done with SpiderMonkey"
