#! /bin/bash

set -e

OUTFILE=Makefile-spidermonkey-file-list.am

echo "Updating $OUTFILE"

function die() {
    echo "$*" 1>&2
    exit 1
}

if ! test -d deps/spidermonkey ; then
    die "don't see deps/spidermonkey"
fi

if ! test -e deps/spidermonkey/configure ; then
    die "You have to autoconf the spidermonkey srcdir first so we dist its configure"
fi

## we have to dump all the JS tests because they break automake distcheck (too many files)
echo "ALL_SPIDERMONKEY_FILES = \\" > ${OUTFILE}
find deps/spidermonkey deps/narcissus -name "*" -a -type f | grep -v -E '/tests/.*\.js' | sort | sed -e 's/$/ \\/g' | sed -e 's/^/	/g' >> ${OUTFILE} || die "Failed to find stuff for ${OUTFILE}"
echo '	$(NULL)' >> ${OUTFILE}

