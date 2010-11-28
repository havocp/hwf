#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-thread-pool --version
log "Checking we don't fail"
gtest ${BUILDDIR}/test-thread-pool

exit 0
