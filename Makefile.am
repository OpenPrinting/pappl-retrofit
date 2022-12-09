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
	pappl-retrofit/web-interface.c \
	pappl-retrofit/web-interface-private.h
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

# ==========================
# Legacy Printer Application
# ==========================
bin_PROGRAMS = \
	legacy-printer-app

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
	-I$(srcdir)/pappl-retrofit/

# ===========
# Directories
# ===========

legacy_printer_app_statedir=$(localstatedir)/lib/legacy-printer-app
legacy_printer_app_userppddir=$(legacy_printer_app_statedir)/ppd
legacy_printer_app_spooldir=$(localstatedir)/spool/legacy-printer-app
legacy_printer_app_resourcedir=$(datadir)/legacy-printer-app
legacy_printer_app_serverbin=$(libdir)/legacy-printer-app

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

uninstall-hook:

SUBDIRS =
