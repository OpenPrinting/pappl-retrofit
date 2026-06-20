#!/bin/sh
# ci/ci-setup.sh
#
# CI helper for building pappl-retrofit against several CUPS releases (and the
# PAPPL version that pairs with each) on both native and QEMU-emulated runners.
#
# pappl-retrofit sits on top of the deepest dependency stack of the OpenPrinting
# repositories: PAPPL + libppd + libcupsfilters + libcups, all of which must be
# built against the SAME CUPS for a given leg.  PAPPL's ABI tracks CUPS:
#
#   CUPS 2.4.x  ->  PAPPL 1.4.x  (libcups2)
#   CUPS 2.5.x  ->  PAPPL 1.4.x  (libcups2)
#   CUPS 3.x    ->  PAPPL 2.x    (libcups3)
#
# The same pappl-retrofit source compiles against all of them; this script
# provides each CUPS build, a matching libcupsfilters / libppd / PAPPL, and then
# builds and tests pappl-retrofit against it.
#
# Subcommands:
#   deps                  install build dependencies
#   cups <kind>           provide libcups; <kind> is one of:
#                           system-2x    distro libcups2-dev  (CUPS 2.4.x)
#                           source-2.5.x OpenPrinting/cups@master    (CUPS 2.5.x)
#                           source-3.x   OpenPrinting/libcups@master (libcups3)
#   pdfio                 build/install pdfio (required by libcupsfilters/PAPPL)
#   libcupsfilters <kind> provide libcupsfilters matching the active CUPS
#   libppd <kind>         provide libppd matching the active CUPS
#   pappl <kind>          provide the PAPPL that pairs with the active CUPS
#   build-retrofit        autogen + configure + make + make check + smoke test
#
# Environment knobs honoured by build-retrofit:
#   CUPS_KIND   the <kind> above (controls test XFAILs for source CUPS)
#   EMULATED    "1" when running under QEMU emulation (controls test XFAILs)
#
# Override knobs (optional):
#   LIBCUPSFILTERS_URL/REF  git URL/ref for the source libcupsfilters build
#   LIBPPD_URL/REF          git URL/ref for the source libppd build
#   PAPPL_URL               git URL for the PAPPL build
#   PAPPL_REF_14            git ref for the PAPPL 1.4.x build (CUPS 2.4/2.5)
#   PAPPL_REF_2X            git ref for the PAPPL 2.x build   (CUPS 3.x)
#
# The script runs as root inside emulation containers and via sudo on native
# runners; it detects which automatically.
set -eu

PDFIO_VER=1.6.4
LIBCUPSFILTERS_URL="${LIBCUPSFILTERS_URL:-https://github.com/OpenPrinting/libcupsfilters.git}"
LIBCUPSFILTERS_REF="${LIBCUPSFILTERS_REF:-master}"
LIBPPD_URL="${LIBPPD_URL:-https://github.com/OpenPrinting/libppd.git}"
LIBPPD_REF="${LIBPPD_REF:-master}"
PAPPL_URL="${PAPPL_URL:-https://github.com/michaelrsweet/pappl.git}"
PAPPL_REF_14="${PAPPL_REF_14:-v1.4.x}"
PAPPL_REF_2X="${PAPPL_REF_2X:-master}"

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

# Make apt completely non-interactive.  Native GitHub runners ship needrestart,
# whose service-restart prompt otherwise hangs the job forever; the emulated
# containers do not have it, which is why only the native legs stalled.
export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a
export NEEDRESTART_SUSPEND=1

# Source-built CUPS / libcupsfilters / libppd / PAPPL install their .pc files
# under $prefix/lib[/<multiarch>]/pkgconfig; make sure pkg-config (and therefore
# every configure in the stack) can find them.
ma=$(gcc -dumpmachine 2>/dev/null || echo "")
PKG_CONFIG_PATH="/usr/lib/pkgconfig${ma:+:/usr/lib/$ma/pkgconfig}:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_PATH

apt_install() {
	$SUDO apt-get update --fix-missing -y
	$SUDO apt-get install -y "$@"
}

cmd_deps() {
	# Union of pappl-retrofit's own build deps and the deps needed to build
	# libcupsfilters, libppd, pdfio and PAPPL from source on the source legs.
	apt_install \
		build-essential autoconf automake libtool libtool-bin pkg-config \
		gettext autopoint autotools-dev cmake git wget tar make gcc g++ \
		file dbus \
		libavahi-client-dev libssl-dev libpam-dev libusb-1.0-0-dev \
		zlib1g-dev libqpdf-dev libexif-dev liblcms2-dev libfontconfig1-dev \
		libfreetype6-dev libcairo2-dev libjpeg-dev libpng-dev libtiff-dev \
		libjxl-dev libpoppler-dev libpoppler-cpp-dev libdbus-1-dev \
		libopenjp2-7-dev mupdf-tools poppler-utils ghostscript
}

# build_autoconf <url> <ref> <submodule-flag> [configure-args...]
build_autoconf() {
	url="$1"; ref="$2"; sub="$3"; shift 3
	echo "ci-setup: building $url @ $ref"
	src="$(mktemp -d)"
	git clone --depth 1 --branch "$ref" $sub "$url" "$src"
	( cd "$src"
	  [ -x ./configure ] || ./autogen.sh
	  ./configure --prefix=/usr "$@" || ./configure --prefix=/usr
	  make -j"$(nproc)"
	  $SUDO make install )
	$SUDO ldconfig || true
}

# CUPS 2.5 (OpenPrinting/cups master) ships cups.pc but has dropped cups-config.
# PAPPL 1.4.x's configure still calls cups-config, so install a thin shim that
# answers from pkg-config.  Harmless if a real cups-config is already present.
install_cups_config_shim() {
	command -v cups-config >/dev/null 2>&1 && return 0
	echo "ci-setup: installing cups-config shim (CUPS 2.5 has no cups-config)"
	shim="/usr/local/bin/cups-config"
	$SUDO mkdir -p /usr/local/bin
	$SUDO tee "$shim" >/dev/null <<'EOF'
#!/bin/sh
# Minimal cups-config shim backed by pkg-config (for CUPS 2.5, no cups-config).
prefix=$(pkg-config --variable=prefix cups 2>/dev/null)
[ -n "$prefix" ] || prefix=/usr
out=""
add() { out="$out $1"; }
while [ $# -gt 0 ]; do
	case "$1" in
		--cflags)              add "$(pkg-config --cflags cups)" ;;
		--libs|--image)        add "$(pkg-config --libs cups)" ;;
		--ldflags)             : ;;
		--datadir)             add "$prefix/share/cups" ;;
		--serverbin)           add "$prefix/lib/cups" ;;
		--serverroot)          add "$prefix/etc/cups" ;;
		--version)             add "$(pkg-config --modversion cups)" ;;
		--api-version)         add "$(pkg-config --modversion cups | cut -d. -f1,2)" ;;
		--help|--build|--prefix|--static) : ;;
		*)                     : ;;
	esac
	shift
done
echo $out
EOF
	$SUDO chmod +x "$shim"
}

cmd_cups() {
	kind="$1"
	case "$kind" in
		system-2x)
			apt_install libcups2-dev
			;;
		source-2.5.x)
			# CUPS 2.5 (OpenPrinting/cups master) ships cups.pc and has
			# dropped cups-config.  Force the multiarch libdir so libcups
			# lands on the default linker search path for transitive linking.
			build_autoconf https://github.com/OpenPrinting/cups.git master "" \
				--disable-systemd ${ma:+--libdir=/usr/lib/$ma}
			install_cups_config_shim
			;;
		source-3.x)
			build_autoconf https://github.com/OpenPrinting/libcups.git master \
				"--recurse-submodules"
			;;
		*)
			echo "ci-setup: unknown cups kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_pdfio() {
	echo "ci-setup: building pdfio $PDFIO_VER"
	src="$(mktemp -d)"
	( cd "$src"
	  wget -q "https://github.com/michaelrsweet/pdfio/releases/download/v$PDFIO_VER/pdfio-$PDFIO_VER.tar.gz"
	  tar -xzf "pdfio-$PDFIO_VER.tar.gz"
	  cd "pdfio-$PDFIO_VER"
	  ./configure --prefix=/usr --enable-shared
	  make all
	  $SUDO make install )
	$SUDO ldconfig || true
}

cmd_libcupsfilters() {
	kind="$1"
	case "$kind" in
		system-2x)
			apt_install libcupsfilters-dev
			;;
		source-*)
			# Never let a pre-shipped libcupsfilters/libppd shadow the source
			# builds under test on the source legs.
			$SUDO apt-get remove -y libcupsfilters-dev libppd-dev || true
			build_autoconf "$LIBCUPSFILTERS_URL" "$LIBCUPSFILTERS_REF" ""
			;;
		*)
			echo "ci-setup: unknown libcupsfilters kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_libppd() {
	kind="$1"
	case "$kind" in
		system-2x)
			apt_install libppd-dev
			;;
		source-*)
			# --enable-ppdc-utils: pappl-retrofit / legacy-printer-app need the
			# ppdc tooling at runtime to turn .drv sources into PPDs.
			build_autoconf "$LIBPPD_URL" "$LIBPPD_REF" "" --enable-ppdc-utils
			;;
		*)
			echo "ci-setup: unknown libppd kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_pappl() {
	kind="$1"
	case "$kind" in
		system-2x)
			# Distro PAPPL is 1.4.x (libcups2) - matches CUPS 2.4.x.
			apt_install libpappl-dev
			;;
		source-2.5.x)
			# CUPS 2.5.x pairs with PAPPL 1.4.x (libcups2) for now.
			$SUDO apt-get remove -y libpappl-dev || true
			build_autoconf "$PAPPL_URL" "$PAPPL_REF_14" "" --enable-shared
			;;
		source-3.x)
			# CUPS 3.x (libcups3) pairs with PAPPL 2.x.
			$SUDO apt-get remove -y libpappl-dev || true
			build_autoconf "$PAPPL_URL" "$PAPPL_REF_2X" "" --enable-shared
			;;
		*)
			echo "ci-setup: unknown pappl kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_build() {
	./autogen.sh
	./configure
	make -j"$(nproc)" V=1

	# Report which CUPS the configure step actually selected.
	echo "ci-setup: configured against:"
	grep -E "libcups:|cups-config:" config.log 2>/dev/null || true

	# Hook for tests that depend on environment quirks of a source-installed
	# or emulated CUPS/PAPPL.  Empty for now; add space-separated test names
	# here if a leg surfaces an environment-only failure.
	xfail=""

	if [ -n "$xfail" ]; then
		make check V=1 VERBOSE=1 XFAIL_TESTS="$xfail" \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	else
		make check V=1 VERBOSE=1 \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	fi

	# Smoke-test the fully-linked binary against the runtime PAPPL / libppd /
	# libcupsfilters stack.
	./legacy-printer-app --help
}

case "${1:-}" in
	deps)            cmd_deps ;;
	cups)            shift; cmd_cups "$@" ;;
	pdfio)           cmd_pdfio ;;
	libcupsfilters)  shift; cmd_libcupsfilters "$@" ;;
	libppd)          shift; cmd_libppd "$@" ;;
	pappl)           shift; cmd_pappl "$@" ;;
	build-retrofit)  cmd_build ;;
	*)
		echo "usage: ci-setup.sh {deps | cups <kind> | pdfio | libcupsfilters <kind> | libppd <kind> | pappl <kind> | build-retrofit}" >&2
		exit 2 ;;
esac
