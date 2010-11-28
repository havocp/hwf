#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-args --version
log "Checking we don't crash by default"
die_if_fails ${BUILDDIR}/test-args

exit 0
