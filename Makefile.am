AUTOMAKE_OPTIONS = -Wno-portability
ACLOCAL_AMFLAGS = -I m4

# note that target-specific CPPFLAGS will REPLACE
# not augment these default flags by default,
# so we manually put AM_CPPFLAGS in the target flags
# if we set target_CPPFLAGS
AM_CPPFLAGS=						\
	-I$(top_builddir)/config/built/include		\
	-I$(top_srcdir)/src/lib

# again, if you have target_LDFLAGS this gets overridden
AM_LDFLAGS=					\
	-L$(top_builddir)

# These are added to using += in the included files
bin_PROGRAMS =
bin_SCRIPTS =
libexec_PROGRAMS =
shlibdir=$(libdir)
shlib_PROGRAMS =
check_PROGRAMS =
noinst_PROGRAMS =
noinst_LTLIBRARIES =
noinst_SCRIPTS =
nodist_noinst_DATA =
EXTRA_PROGRAMS =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
TESTS =
TEST_PROGS =
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

BUILT_SRC_DIR=$(top_builddir)/config/built
BUILT_INCLUDE_DIR=$(BUILT_SRC_DIR)/include

built-src-clean-local:
	$(RM) -rf $(BUILT_SRC_DIR)
distclean-local: built-src-clean-local

stale-files-clean-local:
	-$(RM) $(abs_top_builddir)/*.la $(abs_top_builddir)/*.gir $(abs_top_builddir)/*.typelib $(abs_top_builddir)/*.gcda $(abs_top_builddir)/*.gcno

clean-local: stale-files-clean-local test-logs-clean-local coverage-report-clean-local spidermonkey-clean-local

all-local: spidermonkey-all-local

install-data-hook: hrt-install-data-hook hio-install-data-hook hjs-install-data-hook

uninstall-hook: hrt-uninstall-hook hio-uninstall-hook hjs-uninstall-hook

### These should be ordered in the order you want to install, because
### automake will install e.g. lib_LTLIBRARIES in order, so the +=
### should be from lower to higher in dependency stack
include Makefile-m4.am
include Makefile-test.am
include Makefile-container.am
include Makefile-test-container.am
include Makefile-lib-hrt.am
include Makefile-lib-hjs.am
include Makefile-lib-hio.am
include Makefile-test-lib.am
include Makefile-deps-http-parser.am
include Makefile-test-http-parser.am
include Makefile-deps-libev.am
include Makefile-deps-spidermonkey.am
include Makefile-deps-trucov.am

EXTRA_DIST += deps/VERSIONS

EXTRA_DIST +=						\
	scripts/build-spidermonkey.sh			\
	scripts/update-spidermonkey-file-list.sh
