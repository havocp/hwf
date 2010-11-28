#! /bin/bash

. "${TOP_SRCDIR}"/test/testutil.sh

die_if_succeeds ${BUILDDIR}/hwf-container --nonexistent-option

exit 0
