# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ====================
# Version informations
# ====================
AC_INIT([pappl-retrofit], [1.0b1], [https://github.com/OpenPrinting/pappl-retrofit/issues], [pappl-retrofit], [https://github.com/OpenPrinting/pappl-retrofit/])
pappl_retrofit_version="AC_PACKAGE_VERSION"
pappl_retrofit_version_major="`echo AC_PACKAGE_VERSION | awk -F. '{print $1}'`"
pappl_retrofit_version_minor="`echo AC_PACKAGE_VERSION | awk -F. '{printf("%d\n",$2);}'`"

# =============
# Automake init
# =============
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 gnu dist-xz dist-bzip2 subdir-objects foreign])
AM_SILENT_RULES([yes])
AC_LANG([C])
# Workaround to fix Snap build
m4_pattern_allow([AC_DEFINE])
AC_CONFIG_HEADERS([config.h])
# Extra defines for the config.h
AH_BOTTOM([
#ifdef HAVE_LONG_LONG
#  define CUPS_LLFMT   "%lld"
#  define CUPS_LLCAST  (long long)
#else
#  define CUPS_LLFMT   "%ld"
#  define CUPS_LLCAST  (long)
#endif /* HAVE_LONG_LONG */

#ifdef HAVE_ARC4RANDOM
#  define CUPS_RAND() arc4random()
#  define CUPS_SRAND(v) arc4random_stir()
#elif defined(HAVE_RANDOM)
#  define CUPS_RAND() random()
#  define CUPS_SRAND(v) srandom(v)
#elif defined(HAVE_LRAND48)
#  define CUPS_RAND() lrand48()
#  define CUPS_SRAND(v) srand48(v)
#else
#  define CUPS_RAND() rand()
#  define CUPS_SRAND(v) srand(v)
#endif /* HAVE_ARC4RANDOM */
])

# ===========================
# Find required base packages
# ===========================
AC_PROG_CC
#AC_PROG_CXX
#AX_CXX_COMPILE_STDCXX([11],[noext],[mandatory])
AM_PROG_CC_C_O
AM_ICONV
#AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG([0.20])

AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])

# ================================
# Find CUPS internals (no pc file)
# ================================
AC_ARG_WITH([cups-config],
	[AS_HELP_STRING([--with-cups-config=path], [Specify path to cups-config executable.])],
	[with_cups_config="$withval"],
	[with_cups_config=system]
)

AS_IF([test "x$with_cups_config" != "xsystem"], [
	CUPSCONFIG=$with_cups_config
], [
	AC_PATH_TOOL(CUPSCONFIG, [cups-config])
	AS_IF([test -z "$CUPSCONFIG"], [
		AC_MSG_ERROR([Required cups-config is missing. Please install CUPS developer packages.])
	])
])
CUPS_CFLAGS=`$CUPSCONFIG --cflags`
CUPS_LIBS=`$CUPSCONFIG --image --libs`
CUPS_VERSION=`$CUPSCONFIG --version`
AC_SUBST(CUPS_CFLAGS)
AC_SUBST(CUPS_LIBS)

CUPS_DATADIR="`$CUPSCONFIG --datadir`"
AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$CUPS_DATADIR", [CUPS datadir])
AC_SUBST(CUPS_DATADIR)

CUPS_SERVERROOT="`$CUPSCONFIG --serverroot`"
AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [CUPS serverroot])
AC_SUBST(CUPS_SERVERROOT)

CUPS_FONTPATH="$CUPS_DATADIR/fonts"
AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH", [Path to CUPS fonts dir])
AC_SUBST(CUPS_FONTPATH)

CUPS_SERVERBIN="`$CUPSCONFIG --serverbin`"
AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN", [Path to CUPS binaries dir])
AC_SUBST(CUPS_SERVERBIN)

AX_COMPARE_VERSION([$CUPS_VERSION],[gt],[2.2], [
	AC_DEFINE(CUPS_2_2, 1, [CUPS Version is 2.2 or newer])
])

# ======================
# Check system functions
# ======================
AC_CHECK_FUNCS(sigaction)
AC_CHECK_FUNCS(waitpid wait3)
AC_CHECK_FUNCS(strtoll)
AC_CHECK_FUNCS(open_memstream)
AC_CHECK_FUNCS(getline,[],AC_SUBST([GETLINE],['bannertopdf-getline.$(OBJEXT)']))
AC_CHECK_FUNCS(strcasestr,[],AC_SUBST([STRCASESTR],['pdftops-strcasestr.$(OBJEXT)']))
AC_SEARCH_LIBS(pow, m)
dnl Checks for string functions.
AC_CHECK_FUNCS(strdup strlcat strlcpy)
if test "$host_os_name" = "hp-ux" -a "$host_os_version" = "1020"; then
        echo Forcing snprintf emulation for HP-UX.
else
        AC_CHECK_FUNCS(snprintf vsnprintf)
fi

# ========================
# Check for system headers
# ========================
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([sys/stat.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([zlib.h])
AC_CHECK_HEADERS([endian.h])
AC_CHECK_HEADERS([dirent.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADER(string.h,AC_DEFINE(HAVE_STRING_H))
AC_CHECK_HEADER(strings.h,AC_DEFINE(HAVE_STRINGS_H))

# ==========================
# Check for needed libraries
# ==========================
PKG_CHECK_MODULES([PAPPL], [pappl])
AC_SUBST(PAPPL_CFLAGS)
AC_SUBST(PAPPL_LIBS)
PKG_CHECK_MODULES([LIBCUPSFILTERS], [libcupsfilters])
AC_SUBST(CUPSFILTERS_CFLAGS)
AC_SUBST(CUPSFILTERS_LIBS)
PKG_CHECK_MODULES([PPD], [libppd])
AC_SUBST(PPD_CFLAGS)
AC_SUBST(PPD_LIBS)

# ===================================
# Check for large files and long long
# ===================================
AC_SYS_LARGEFILE
LARGEFILE=""
AS_IF([test x"$enable_largefile" != "xno"], [
	LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"
	AS_IF([test x"$ac_cv_sys_large_files" = "x1"], [LARGEFILE="$LARGEFILE -D_LARGE_FILES"])
	AS_IF([test x"$ac_cv_sys_file_offset_bits" = "x64"], [LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"])
])
AC_SUBST(LARGEFILE)

AC_CHECK_TYPE(long long, [long_long_found=yes], [long_long_found=no])
AS_IF([test x"$long_long_found" = "xyes"], [
	AC_DEFINE([HAVE_LONG_LONG], [], [Platform supports long long type])
])

# ================
# Check for cflags
# ================
AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror], [Treat all warnings as errors, useful for development.])],
	[enable_werror="$enableval"],
	[enable_werror=no]
)
AS_IF([test x"$enable_werror" = "xyes"], [
	CFLAGS="$CFLAGS -Werror"
])
AS_IF([test x"$GCC" = "xyes"], [
	# Be tough with warnings and produce less careless code
	CFLAGS="$CFLAGS -Wall -std=gnu11"
	CXXFLAGS="$CXXFLAGS -Wall " # -Weffc++" # TODO: enable when it does not print 1MB of warnings
])
CFLAGS="$CFLAGS -D_GNU_SOURCE"
CXXFLAGS="$CXXFLAGS -D_GNU_SOURCE"

# =========================================================
# Select a different shell instead of the default /bin/bash
# =========================================================
AC_ARG_WITH([shell],
	[AS_HELP_STRING([--with-shell=path], [Specify path for a modern shell.])],
	[with_shell="$withval"],
	[with_shell="/bin/bash"]
)
AC_DEFINE_UNQUOTED([SHELL], "$with_shell", [Path for a modern shell])

# ============================================
# Enable PAPPL backends in legacy-printer-app?
# ============================================
AC_ARG_ENABLE([pappl-backends-for-legacy-printer-app],
	[AS_HELP_STRING([--enable-pappl-backends-for-legacy-printer-app], [Enable PAPPL backends in legacy-printer-app (mainly for development).])],
        [enable_pappl_backends_for_legacy_printer_app="$enableval"],
        [enable_pappl_backends_for_legacy_printer_app=no]
)
AS_IF([test "x$enable_pappl_backends_for_legacy_printer_app" != "xno"], [
    AC_DEFINE(ENABLE_PAPPL_BACKENDS, 1, [Build legacy-printer-app with PAPPL backend support])
])

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
	libpappl-retrofit.pc
	Makefile
])
AC_OUTPUT

# ==============================================
# Display final informations about configuration
# ==============================================
AC_MSG_NOTICE([
==============================================================================
Environment settings:
	CFLAGS:          ${CFLAGS}
	LDFLAGS:         ${LDFLAGS}
Build configuration:
	cups-config:     ${with_cups_config}
	legacy-printer-app w/ PAPPL backends: ${enable_pappl_backends_for_legacy_printer_app}
==============================================================================
])
