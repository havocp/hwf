#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-output --version
log "Checking we don't crash by default"
die_if_fails ${BUILDDIR}/test-output

log "HwfOutputStream looks good"

exit 0
