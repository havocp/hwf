#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-immediate --version
log "Checking we don't fail"
gtest ${BUILDDIR}/test-immediate


exit 0
