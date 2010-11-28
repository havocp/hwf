#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-idle --version
log "Checking we don't fail"
gtest ${BUILDDIR}/test-idle


exit 0
