#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

die_if_fails ${BUILDDIR}/test-http --version

gtest ${BUILDDIR}/test-http

exit 0
