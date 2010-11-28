#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

log "Checking we don't crash --version"
die_if_fails ${BUILDDIR}/test-log --version
log "Checking we don't crash by default"
die_if_fails ${BUILDDIR}/test-log
log "Checking we don't crash --debug"
die_if_fails ${BUILDDIR}/test-log --debug

log "Checking debug not on by default"
die_if_outputs ${BUILDDIR}/test-log 'A debug message'
log "Checking debug enable"
die_if_does_not_output "${BUILDDIR}/test-log --debug" 'A debug message'

log "Checking plain messages print with debug enabled"
die_if_does_not_output "${BUILDDIR}/test-log --debug" 'A regular message'
log "Checking plain messages print with debug disabled"
die_if_does_not_output "${BUILDDIR}/test-log" 'A regular message'

log "logging looks good!"

exit 0
