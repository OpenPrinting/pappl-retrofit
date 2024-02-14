# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ===================
# Version information
# ===================
AC_INIT([pappl-retrofit], [1.0b2], [https://github.com/OpenPrinting/pappl-retrofit/issues], [pappl-retrofit], [https://github.com/OpenPrinting/pappl-retrofit/])
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
LT_INIT

# ==========
# pkg-config
# ==========
PKG_PROG_PKG_CONFIG([0.20])
AC_PATH_TOOL([PKGCONFIG], [pkg-config])

AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])

# ====
# CUPS
# ====
AC_MSG_CHECKING([for CUPS library v3.0 or higher])
AS_IF([$PKGCONFIG --exists cups3], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups3)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])
    CUPS_CFLAGS="$($PKGCONFIG --cflags cups3)"
    CUPS_LIBS="$LIBS $($PKGCONFIG --libs cups3)"
    AC_SUBST(CUPS_CFLAGS)
    AC_SUBST(CUPS_LIBS)
    CUPS_DATADIR="$($PKGCONFIG --variable=prefix cups3)/share/cups"
    AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$CUPS_DATADIR", [CUPS datadir])
    AC_SUBST(CUPS_DATADIR)
    AS_IF([test x$host_os_name = xdarwin], [
	CUPS_SERVERROOT="/private/etc/cups"
    ], [
	AS_IF([test "x$($PKGCONFIG --variable=prefix cups3)" = x/usr], [
	    CUPS_SERVERROOT="/etc/cups"
	], [
	    CUPS_SERVERROOT="$($PKGCONFIG --variable=prefix cups3)/etc/cups"
	])
    ])
    AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [CUPS serverroot])
    AC_SUBST(CUPS_SERVERROOT)
    CUPS_FONTPATH="$CUPS_DATADIR/fonts"
    AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH",
		       [Path to CUPS fonts dir])
    AC_SUBST(CUPS_FONTPATH)
    CUPS_SERVERBIN="$($PKGCONFIG --variable=prefix cups3)/lib/cups"
    AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN",
		       [Path to CUPS binaries dir])
    AC_SUBST(CUPS_SERVERBIN)
], [
    AC_MSG_RESULT(no)
    AC_ARG_WITH([cups-config],
		[AS_HELP_STRING([--with-cups-config=path], [Specify path to cups-config executable.])],
		[with_cups_config="$withval"],
		[with_cups_config=system])
    AS_IF([test "x$with_cups_config" != "xsystem"], [
	CUPSCONFIG=$with_cups_config
    ], [
	AC_PATH_TOOL(CUPSCONFIG, [cups-config])
    ])
    AC_MSG_CHECKING([for CUPS library v2.x])
    AS_IF([test -n "$CUPSCONFIG"], [
	CUPS_VERSION=`$CUPSCONFIG --version`
	AC_MSG_RESULT([yes, v$CUPS_VERSION])
	AC_DEFINE([HAVE_LIBCUPS2], [1], [Use libcups2])
	CUPS_CFLAGS=`$CUPSCONFIG --cflags`
	CUPS_LIBS=`$CUPSCONFIG --image --libs`
	AC_SUBST(CUPS_CFLAGS)
	AC_SUBST(CUPS_LIBS)
	CUPS_DATADIR="`$CUPSCONFIG --datadir`"
	AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$CUPS_DATADIR", [CUPS datadir])
	AC_SUBST(CUPS_DATADIR)
	CUPS_SERVERROOT="`$CUPSCONFIG --serverroot`"
	AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT",
			   [CUPS serverroot])
	AC_SUBST(CUPS_SERVERROOT)
	CUPS_FONTPATH="$CUPS_DATADIR/fonts"
	AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH",
			   [Path to CUPS fonts dir])
	AC_SUBST(CUPS_FONTPATH)
	CUPS_SERVERBIN="`$CUPSCONFIG --serverbin`"
	AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN",
			   [Path to CUPS binaries dir])
	AC_SUBST(CUPS_SERVERBIN)
    ], [
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([Required cups-config is missing. Please install CUPS developer packages.])
    ])
])
AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [Location of CUPS config files])

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
AC_CHECK_FUNC([poll], [
    AC_DEFINE([HAVE_POLL], [1], [Have poll function?])
])

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
PKG_CHECK_MODULES([CUPSFILTERS], [libcupsfilters])
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

# =============================================
# Run legacy-printer-app permanently as daemon?
# =============================================
AC_ARG_ENABLE([legacy-printer-app-as-daemon],
	[AS_HELP_STRING([--enable-legacy-printer-app-as-daemon], [Run legacy-printer-app permanently as daemon (to make classically installed CUPS drivers available as Printer Application).])],
        [enable_legacy_printer_app_as_daemon="$enableval"],
        [enable_legacy_printer_app_as_daemon=no]
)
SYSTEMD_UNITDIR=
AS_IF([test "x$enable_legacy_printer_app_as_daemon" != "xno"], [
    SYSTEMD_UNITDIR=`$PKG_CONFIG --variable=systemdsystemunitdir systemd`
])
AC_SUBST(SYSTEMD_UNITDIR)
AM_CONDITIONAL([LEGACY_PRINTER_APP_AS_DAEMON], test x$enable_legacy_printer_app_as_daemon != xno)

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

# ===========
# Admin group
# ===========
AC_ARG_WITH([admin_group], AS_HELP_STRING([--with-admin-group=[GROUP]], [Set group which users are authenticated for remote configuration, default=none]), [
    AS_IF([test $withval != yes -a $withval != no], [
	ADMINGROUP=$withval
    ], [
	ADMINGROUP="none"
    ])
], [
    ADMINGROUP="none"
])
AC_SUBST([ADMINGROUP])

# =============================
# PAM module for authentication
# =============================
AC_ARG_WITH([auth_service], AS_HELP_STRING([--with-auth-service=[SERVICE]], [Set PAM service to use for remote request authentication, default=none]), [
    AS_IF([test $withval != yes -a $withval != no], [
	AUTHSERV=$withval
    ], [
	AUTHSERV="none"
    ])
], [
    AUTHSERV="none"
])
AC_SUBST([AUTHSERV])

# =======================================
# Listening on specified address/hostname
# =======================================
AC_ARG_WITH([listen_hostname], AS_HELP_STRING([--with-listen-hostname=[HOSTNAME]], [Set address/hostname where listen for connection, default=local IPv4 and IPv6 addresses]), [
    AS_IF([test $withval != yes -a $withval != no], [
	LISTENHOST="-o listen-hostname=$withval "
    ], [
	LISTENHOST=""
    ])
], [
    LISTENHOST=""
])
AC_SUBST([LISTENHOST])

# ============
# Log location
# ============
AC_ARG_WITH([log_file], AS_HELP_STRING([--with-log-file=[LOCATION]], [Set location for logs: - (stderr), syslog (syslog service), FILENAME, default=-]), [
    AS_IF([test $withval != yes -a $withval != no], [
	LOGFILE="$withval"
    ], [
	LOGFILE="-"
    ])
], [
    LOGFILE="-"
])
AC_SUBST([LOGFILE])

# ==============
# Server options
# ==============
AC_ARG_WITH([server_options], AS_HELP_STRING([--with-server-options=[SOPTIONS]], [Set default server options, default=multi-queue,web-interface]), [
    AS_IF([test $withval != yes -a $withval != no], [
	SOPTIONS="$withval"
    ], [
	SOPTIONS="multi-queue,web-interface"
    ])
], [
    SOPTIONS="multi-queue,web-interface"
])
AC_SUBST([SOPTIONS])

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
	legacy/legacy-printer-app.service
	libpappl-retrofit.pc
	Makefile
])
AC_OUTPUT

# =============================================
# Display final information about configuration
# =============================================
AC_MSG_NOTICE([
==============================================================================
Environment settings:
	CFLAGS:          ${CFLAGS}
	LDFLAGS:         ${LDFLAGS}
Build configuration:
        libcups:         ${CUPS_VERSION}
	cups-config:     ${with_cups_config}
	legacy-printer-app as daemon: ${enable_legacy_printer_app_as_daemon}
	legacy-printer-app w/ PAPPL backends: ${enable_pappl_backends_for_legacy_printer_app}
	systemd unitdir: ${SYSTEMD_UNITDIR}
	log file: ${LOGFILE}
	server options: ${SOPTIONS}
	auth service: ${AUTHSERV}
	admin group: ${ADMINGROUP}
==============================================================================
])
