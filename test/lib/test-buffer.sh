#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-buffer --version
log "Checking we don't crash by default"
die_if_fails ${BUILDDIR}/test-buffer

log "HwfBuffer looks good"

exit 0
