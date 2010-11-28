dnl -*- mode: m4 -*-

AC_PREREQ(2.61)
AC_INIT([hwf], [0.1.0])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([1.11 -Wno-portability])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/container/main.c])
AM_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)
CONFIGURE_TIME=`date "+%Y-%m-%d %H:%M"`
AC_DEFINE_UNQUOTED(CONFIGURE_TIME, "$CONFIGURE_TIME", [Time source tree was configured])

# using ACLOCAL_AMFLAGS would be better, but autoreconf can't handle
# variables in ACLOCAL_AMFLAGS, so we have to do this. This approach
# results in adding ACLOCAL_FLAGS multiple times as we reconfigure,
# unfortunately.
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

AM_MAINTAINER_MODE([enable])

## Args
AC_ARG_ENABLE(gcov, AS_HELP_STRING([--enable-gcov],[build with coverage flags to gcc]),enable_gcov=$enableval,enable_gcov=no)


if test x$enable_gcov = xyes; then
   GCOV_ENABLED=1
els
   GCOV_ENABLED=0
fi
AC_DEFINE_UNQUOTED(GCOV_ENABLED, "$GCOV_ENABLED", [config.h should change if gcov is toggled to force a full rebuild so we define this])

## don't rerun to this point if we abort
AC_CACHE_SAVE

AC_PROG_INSTALL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_ISC_POSIX
AC_HEADER_STDC

## don't rerun to this point if we abort
AC_CACHE_SAVE

## libev checks
m4_include([deps/libev/libev.m4])

## save the libev checks
AC_CACHE_SAVE

#### gcc warning flags

changequote(,)dnl
addCommonWarnings() {
  result="$@"

  case " $result " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) result="$result -Wall" ;;
  esac

  case " $result " in
  *[\ \	]-Wchar-subscripts[\ \	]*) ;;
  *) result="$result -Wchar-subscripts" ;;
  esac

  case " $result " in
  *[\ \	]-Wpointer-arith[\ \	]*) ;;
  *) result="$result -Wpointer-arith" ;;
  esac

  case " $result " in
  *[\ \	]-Wcast-align[\ \	]*) ;;
  *) result="$result -Wcast-align" ;;
  esac

# can't get rid of one of these in spidermonkey
#  case " $result " in
#  *[\ \	]-Wfloat-equal[\ \	]*) ;;
#  *) result="$result -Wfloat-equal" ;;
#  esac

  case " $result " in
  *[\ \	]-Wsign-compare[\ \	]*) ;;
  *) result="$result -Wsign-compare" ;;
  esac

  case " $result " in
  *[\ \	]-Wtype-limits[\ \	]*) ;;
  *) result="$result -Wtype-limits" ;;
  esac

  case " $result " in
  *[\ \	]-fno-strict-aliasing[\ \	]*) ;;
  *) result="$result -fno-strict-aliasing" ;;
  esac

  case " $result " in
  *[\ \	]-fdiagnostics-show-option[\ \	]*) ;;
  *) result="$result -fdiagnostics-show-option" ;;
  esac

  if test "x$enable_ansi" = "xyes"; then
    case " $result " in
    *[\ \	]-ansi[\ \	]*) ;;
    *) result="$result -ansi" ;;
    esac

    case " $result " in
    *[\ \	]-D_POSIX_C_SOURCE*) ;;
    *) result="$result -D_POSIX_C_SOURCE=199309L" ;;
    esac

    case " $result " in
    *[\ \	]-D_BSD_SOURCE[\ \	]*) ;;
    *) result="$result -D_BSD_SOURCE" ;;
    esac

    case " $result " in
    *[\ \	]-pedantic[\ \	]*) ;;
    *) result="$result -pedantic" ;;
    esac
  fi
  if test x$enable_gcov = xyes; then
    case " $result " in
    *[\ \	]-fprofile-arcs[\ \	]*) ;;
    *) result="$result -fprofile-arcs" ;;
    esac
    case " $result " in
    *[\ \	]-ftest-coverage[\ \	]*) ;;
    *) result="$result -ftest-coverage" ;;
    esac

    ## remove optimizations so we get better line number and block info
    result=`echo "$result" | sed -e 's/-O[0-9s]*//g'`
  fi

  echo $result
}

addCOnlyWarnings() {
  result="$@"

  case " $result " in
  *[\ \	]-Wdeclaration-after-statement[\ \	]*) ;;
  *) result="$result -Wdeclaration-after-statement" ;;
  esac

  case " $result " in
  *[\ \	]-Wmissing-declarations[\ \	]*) ;;
  *) result="$result -Wmissing-declarations" ;;
  esac

  case " $result " in
  *[\ \	]-Wmissing-prototypes[\ \	]*) ;;
  *) result="$result -Wmissing-prototypes" ;;
  esac

  case " $result " in
  *[\ \	]-Wnested-externs[\ \	]*) ;;
  *) result="$result -Wnested-externs" ;;
  esac

  echo $result
}

addCXXOnlyWarnings() {
  result="$@"

  case " $result " in
  *[\ \	]-Wno-non-virtual-dtor[\ \	]*) ;;
  *) result="$result -Wno-non-virtual-dtor" ;;
  esac

  echo $result
}

changequote([,])dnl

if test "x$GCC" = "xyes"; then
  CFLAGS="`addCommonWarnings $CFLAGS`"
  CFLAGS="`addCOnlyWarnings $CFLAGS`"
  CXXFLAGS="`addCommonWarnings $CXXFLAGS`"
  CXXFLAGS="`addCXXOnlyWarnings $CXXFLAGS`"
fi


AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(LDFLAGS)

SHLIB_CFLAGS='-fPIC -DPIC'
SHLIB_LDFLAGS='-shared'

## Shared libraries
PKG_CHECK_MODULES(HRT, gobject-2.0 gthread-2.0)
HRT_LIBS="$SHLIB_LDFLAGS $HRT_LIBS"
HRT_CFLAGS="$SHLIB_CFLAGS $HRT_CFLAGS"

PKG_CHECK_MODULES(HIO, gobject-2.0 gthread-2.0)
HIO_LIBS="$SHLIB_LDFLAGS $HIO_LIBS"
HIO_CFLAGS="$SHLIB_CFLAGS $HIO_CFLAGS"

PKG_CHECK_MODULES(HJS, gobject-2.0 gthread-2.0)
HJS_LIBS="$SHLIB_LDFLAGS $HJS_LIBS"
HJS_CFLAGS="$SHLIB_CFLAGS $HJS_CFLAGS"

## non-test programs
PKG_CHECK_MODULES(CONTAINER, gobject-2.0 gthread-2.0)

## test programs
PKG_CHECK_MODULES(TEST_ARGS, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_BUFFER, gobject-2.0)
PKG_CHECK_MODULES(TEST_HTTP, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_IDLE, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_IMMEDIATE, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_IO, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_IO_SCHEDULING, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_JS, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_LOG, gobject-2.0)
PKG_CHECK_MODULES(TEST_OUTPUT, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_RUNNER_SHUTDOWN, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_SERVER, gio-2.0)
PKG_CHECK_MODULES(TEST_SUBTASK, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_THREAD_LOCAL, gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(TEST_THREAD_POOL, gobject-2.0 gthread-2.0)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

AC_CONFIG_FILES([
Makefile
])

AC_OUTPUT
