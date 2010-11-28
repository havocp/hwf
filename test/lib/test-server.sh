#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

die_if_fails ${BUILDDIR}/test-server --version

gtest ${BUILDDIR}/test-server

exit 0
