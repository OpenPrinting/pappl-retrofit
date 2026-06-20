ACLOCAL_AMFLAGS = -I m4

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = \
	libpappl-retrofit.pc

doc_DATA = \
	README.md \
	AUTHORS \
	COPYING \
	NOTICE \
	LICENSE \
	DEVELOPING.md \
	CONTRIBUTING.md

EXTRA_DIST = \
	$(doc_DATA) \
	autogen.sh \
	config.rpath \
	libpappl-retrofit.pc.in

EXTRA_DIST += \
	pappl-retrofit/test-internal.h

# ==================================
# CUPS Driver legacy support library
# ==================================
pkgpappl_retrofitincludedir = $(includedir)
pkgpappl_retrofitinclude_DATA = \
	pappl-retrofit/pappl-retrofit.h

lib_LTLIBRARIES = libpappl-retrofit.la

libpappl_retrofit_la_SOURCES = \
	pappl-retrofit/pappl-retrofit.c \
	pappl-retrofit/pappl-retrofit-private.h \
	pappl-retrofit/print-job.c \
	pappl-retrofit/print-job-private.h \
	pappl-retrofit/cups-backends.c \
	pappl-retrofit/cups-backends-private.h \
	pappl-retrofit/cups-side-back-channel.c \
	pappl-retrofit/cups-side-back-channel-private.h \
	pappl-retrofit/web-interface.c \
	pappl-retrofit/web-interface-private.h \
	pappl-retrofit/libcups2-private.h \
	pappl-retrofit/pappl2-private.h
	$(pkgpappl_retrofitinclude_DATA)
libpappl_retrofit_la_LIBADD = \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	$(PAPPL_LIBS)
libpappl_retrofit_la_CFLAGS = \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	$(PAPPL_CFLAGS)
libpappl_retrofit_la_LDFLAGS = \
	-no-undefined \
	-version-info 1

EXTRA_DIST += \
	$(pkgpappl_retrofitinclude_DATA)

# =====
# Tests
# =====
#
# NOTE: The test CFLAGS deliberately add BOTH `-I$(srcdir)` and
# `-I$(srcdir)/pappl-retrofit/`.  The latter lets each test file
# include its target private header directly ("cups-backends-private.h",
# "print-job-private.h", etc.); the former lets those private headers'
# own `<pappl-retrofit/pappl-retrofit-private.h>` includes resolve.
#
check_PROGRAMS = \
	test_backend_parse \
	test_ascii85 \
	test_devid_match
TESTS = \
	test_backend_parse \
	test_ascii85 \
	test_devid_match

test_backend_parse_SOURCES = pappl-retrofit/test_backend_parse.c
test_backend_parse_LDADD = \
	libpappl-retrofit.la \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	$(PAPPL_LIBS)
test_backend_parse_CFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/pappl-retrofit/ \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	$(PAPPL_CFLAGS)

test_ascii85_SOURCES = pappl-retrofit/test_ascii85.c
test_ascii85_LDADD = \
	libpappl-retrofit.la \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	$(PAPPL_LIBS)
test_ascii85_CFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/pappl-retrofit/ \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	$(PAPPL_CFLAGS)

test_devid_match_SOURCES = pappl-retrofit/test_devid_match.c
test_devid_match_LDADD = \
	libpappl-retrofit.la \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	$(PAPPL_LIBS)
test_devid_match_CFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/pappl-retrofit/ \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	$(PAPPL_CFLAGS)

# ==========================
# Legacy Printer Application
# ==========================
if LEGACY_PRINTER_APP_AS_DAEMON
sbin_PROGRAMS = \
	legacy-printer-app
else
bin_PROGRAMS = \
	legacy-printer-app
endif

legacy_printer_app_SOURCES = \
	legacy/legacy-printer-app.c
legacy_printer_app_LDADD = \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	libpappl-retrofit.la
legacy_printer_app_CFLAGS = \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	$(PAPPL_CFLAGS) \
	-I$(srcdir)/pappl-retrofit/

EXTRA_DIST += \
	legacy/legacy-printer-app.service

# =========
# Man pages
# =========

man_MANS =

legacy_printer_app_manpage1 = \
        legacy/legacy-printer-app.1
legacy_printer_app_manpage8 = \
        legacy/legacy-printer-app.8

if LEGACY_PRINTER_APP_AS_DAEMON
$(legacy_printer_app_manpage8): $(legacy_printer_app_manpage1)
	cp $(legacy_printer_app_manpage1) $(legacy_printer_app_manpage8)
	sed -i -e 's/\.TH legacy-printer-app 1 "/\.TH legacy-printer-app 8 "/' $(legacy_printer_app_manpage8)
legacy_printer_app_manpages = \
        $(legacy_printer_app_manpage8)
else
legacy_printer_app_manpages = \
        $(legacy_printer_app_manpage1)
endif
man_MANS += $(legacy_printer_app_manpages)

EXTRA_DIST += \
        $(legacy_printer_app_manpage1)

# ===========
# Directories
# ===========

legacy_printer_app_statedir=$(localstatedir)/lib/legacy-printer-app
legacy_printer_app_userppddir=$(legacy_printer_app_statedir)/ppd
legacy_printer_app_spooldir=$(localstatedir)/spool/legacy-printer-app
legacy_printer_app_resourcedir=$(datadir)/legacy-printer-app
legacy_printer_app_serverbin=$(prefix)/lib/legacy-printer-app

# ==========
# Test pages
# ==========

testpagedir = $(legacy_printer_app_resourcedir)

testpagefiles = \
	data/testpage.pdf \
	data/testpage.ps

testpage_DATA = $(testpagefiles)

EXTRA_DIST += $(testpagefiles)



distclean-local:
	rm -rf *.cache *~ */*~

clean-local:
	rm -f $(legacy_printer_app_manpage8)
	rm -rf $(CIROOT)

install-data-hook:
	$(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_statedir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_userppddir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_spooldir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_resourcedir)
	if [ -d $(CUPS_SERVERBIN) ]; then \
	  $(LN_S) -f $(CUPS_SERVERBIN) $(DESTDIR)$(legacy_printer_app_serverbin);\
	else \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_serverbin); \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_serverbin)/filter; \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(legacy_printer_app_serverbin)/backend; \
	fi
if LEGACY_PRINTER_APP_AS_DAEMON
	$(INSTALL) -d -m 755 $(DESTDIR)$(SYSTEMD_UNITDIR)
	$(INSTALL) -m 644 legacy/legacy-printer-app.service $(DESTDIR)$(SYSTEMD_UNITDIR)
endif

uninstall-hook:
	-rmdir $(DESTDIR)$(legacy_printer_app_userppddir)
	-rmdir $(DESTDIR)$(legacy_printer_app_statedir)
	-rmdir $(DESTDIR)$(legacy_printer_app_spooldir)
	-rmdir $(DESTDIR)$(legacy_printer_app_resourcedir)
	-if [ -d $(CUPS_SERVERBIN) ]; then \
	  $(RM) $(DESTDIR)$(legacy_printer_app_serverbin);\
	else \
	  rmdir $(DESTDIR)$(legacy_printer_app_serverbin);\
	  rmdir $(DESTDIR)$(legacy_printer_app_serverbin)/filter;\
	  rmdir $(DESTDIR)$(legacy_printer_app_serverbin)/backend;\
	fi
	-rmdir $(DESTDIR)$(includedir)/pappl-retrofit
	-rmdir $(DESTDIR)$(docdir)
if LEGACY_PRINTER_APP_AS_DAEMON
	-rm $(DESTDIR)$(SYSTEMD_UNITDIR)/legacy-printer-app.service
	-rmdir $(DESTDIR)$(SYSTEMD_UNITDIR)
endif

uninstall-hook:

SUBDIRS =

# ============================================================
# Staged-install downstream autopkgtests (DESTDIR staging)
# ============================================================
CIROOT = $(abs_top_builddir)/_ciroot

EXTRA_DIST += \
	ci/autopkgtest/run.sh \
	ci/autopkgtest/debian-tests/control \
	ci/autopkgtest/debian-tests/pappl-retrofit-1-dev

# Stage a clean install of the just-built tree into $(CIROOT), then rewrite
# the staged libpappl-retrofit.pc prefix so a downstream consumer resolves
# THIS build (and not a system copy) while system dependency .pc files
# (libcupsfilters / libppd / pappl) keep resolving normally.
stage-ciroot: all
	rm -rf "$(CIROOT)"
	$(MAKE) $(AM_MAKEFLAGS) install DESTDIR="$(CIROOT)"
	@pc="$(CIROOT)$(libdir)/pkgconfig/libpappl-retrofit.pc"; \
	if [ -f "$$pc" ]; then \
	  sed -i.bak "s|^prefix=.*|prefix=$(CIROOT)$(prefix)|" "$$pc"; \
	  rm -f "$$pc.bak"; \
	  echo "stage-ciroot: rewrote prefix in $$pc"; \
	fi

# Run the unmodified downstream Debian autopkgtests against $(CIROOT).
test-autopkgtest: stage-ciroot
	CIROOT="$(CIROOT)" CIPREFIX="$(prefix)" TOP_BUILDDIR="$(abs_top_builddir)" \
	    $(SHELL) $(srcdir)/ci/autopkgtest/run.sh pappl-retrofit-1-dev

.PHONY: stage-ciroot test-autopkgtest
