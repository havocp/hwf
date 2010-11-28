#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-io-scheduling --version
log "Checking we don't fail"
gtest ${BUILDDIR}/test-io-scheduling


exit 0
