#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

die_if_fails ${BUILDDIR}/hwf-container --version

exit 0
