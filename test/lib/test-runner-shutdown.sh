#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-runner-shutdown --version
log "Checking we don't fail"
gtest ${BUILDDIR}/test-runner-shutdown


exit 0
