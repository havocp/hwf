set -e

export MALLOC_CHECK_=2

if test -z "${TEST_LOGDIR}" ; then
    echo "Must set TEST_LOGDIR" >&2
fi

if test -z "${BUILDDIR}" ; then
    echo "Must set BUILDDIR" >&2
fi

if test -z "${TOP_SRCDIR}" ; then
    echo "Must set TOP_SRCDIR" >&2
fi

## because we aren't using libtool we have to set this
export LD_LIBRARY_PATH=${BUILDDIR}:${LD_LIBRARY_PATH}


TESTNAME=$(basename $(dirname $0))_$(basename $0)
LOGFILE="${TEST_LOGDIR}/${TESTNAME}.log"

mkdir -p "${TEST_LOGDIR}"
/bin/rm -f ${LOGFILE} || true

function die() {
    STATUS=$?
    echo "exit status of last command: $STATUS" >>${LOGFILE}
    if (($STATUS > 128)) ; then
        SIGNAL=$(($STATUS-128))
        echo "last command received signal $SIGNAL" >>${LOGFILE}
    fi
    echo "========= Log ${LOGFILE} contains =========" >&2
    cat ${LOGFILE} >&2
    echo "========= End ${LOGFILE} contents =========" >&2
    echo ""
    exit 1
}

trap "die" ABRT TERM SEGV BUS INT ILL KILL FPE

function log() {
    echo $* >>${LOGFILE}
}

function die_if_fails() {
    $* >>${LOGFILE} 2>&1 || die
}

function die_if_succeeds() {
    $* >>${LOGFILE} 2>&1 && die || true
}

## relies on quoting the entire command line, and single-quoting the output string
function die_if_outputs() {
    $1 2>&1 | grep -q "$2" && die || true
}

## relies on quoting the entire command line, and single-quoting the output string
function die_if_does_not_output() {
    $1 2>&1 | grep -q "$2" || die
}

function gtest() {
    die_if_fails gtester ${GTESTER_OPTIONS} --verbose --keep-going $*
}

## some quick self-testing
die_if_fails /bin/true
die_if_succeeds /bin/false
