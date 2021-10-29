ACLOCAL_AMFLAGS = -I m4

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = \
	libpappl-retrofit.pc

doc_DATA = \
	README.md \
	AUTHORS \
	NOTICE \
	LICENSE

EXTRA_DIST = \
	$(doc_DATA) \
	autogen.sh \
	config.rpath \
	libpappl-retrofit.pc.in

# ==========================
# CUPS Driver legacy support library
# ==========================
pkgpappl_retrofitincludedir = $(includedir)/pappl-retrofit
pkgpappl_retrofitinclude_DATA = \
	pappl-retrofit/pappl-retrofit.h \
	pappl-retrofit/base.h \
	pappl-retrofit/print-job.h \
	pappl-retrofit/cups-backends.h \
	pappl-retrofit/web-interface.h

lib_LTLIBRARIES = libpappl-retrofit.la

libpappl_retrofit_la_SOURCES = \
	pappl-retrofit/pappl-retrofit.c \
	pappl-retrofit/print-job.c \
	pappl-retrofit/cups-backends.c \
	pappl-retrofit/web-interface.c
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

# ===========================
# Example Printer Application
# ===========================
bin_PROGRAMS = \
	test-printer-app

test_printer_app_SOURCES = \
	examples/test-printer-app.c
test_printer_app_LDADD = \
	$(CUPS_LIBS) \
	$(CUPSFILTERS_LIBS) \
	$(PPD_LIBS) \
	libpappl-retrofit.la
test_printer_app_CFLAGS = \
	$(CUPS_CFLAGS) \
	$(CUPSFILTERS_CFLAGS) \
	$(PPD_CFLAGS) \
	-I$(srcdir)/pappl-retrofit/

# ===========
# Directories
# ===========

test_printer_app_statedir=$(localstatedir)/lib/test-printer-app
test_printer_app_userppddir=$(test_printer_app_statedir)/ppd
test_printer_app_spooldir=$(localstatedir)/spool/test-printer-app
test_printer_app_resourcedir=$(datadir)/test-printer-app
test_printer_app_serverbin=$(libdir)/test-printer-app

# ==========
# Test pages
# ==========

testpagedir = $(test_printer_app_resourcedir)

testpagefiles = \
	data/testpage.pdf \
	data/testpage.ps

testpage_DATA = $(testpagefiles)

EXTRA_DIST += $(testpagefiles)



distclean-local:
	rm -rf *.cache *~ */*~

install-data-hook:
	$(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_statedir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_userppddir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_spooldir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_resourcedir)
	if [ -d $(CUPS_SERVERBIN) ]; then \
	  $(LN_S) -f $(CUPS_SERVERBIN) $(DESTDIR)$(test_printer_app_serverbin);\
	else \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_serverbin); \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_serverbin)/filter; \
	  $(INSTALL) -d -m 755 $(DESTDIR)$(test_printer_app_serverbin)/backend; \
	fi

uninstall-hook:
	-rmdir $(DESTDIR)$(test_printer_app_userppddir)
	-rmdir $(DESTDIR)$(test_printer_app_statedir)
	-rmdir $(DESTDIR)$(test_printer_app_spooldir)
	-rmdir $(DESTDIR)$(test_printer_app_resourcedir)
	-if [ -d $(CUPS_SERVERBIN) ]; then \
	  $(RM) $(DESTDIR)$(test_printer_app_serverbin);\
	else \
	  rmdir $(DESTDIR)$(test_printer_app_serverbin);\
	  rmdir $(DESTDIR)$(test_printer_app_serverbin)/filter;\
	  rmdir $(DESTDIR)$(test_printer_app_serverbin)/backend;\
	fi
	-rmdir $(DESTDIR)$(includedir)/pappl-retrofit
	-rmdir $(DESTDIR)$(docdir)

uninstall-hook:

SUBDIRS =
