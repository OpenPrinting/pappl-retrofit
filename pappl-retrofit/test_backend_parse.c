//
// =============================================================================
//  test_backend_parse.c — Hermetic unit tests for pappl-retrofit's
//                        CUPS-backend pure-logic helpers
//                        (pappl-retrofit/cups-backends.c)
// =============================================================================
//
//  Target source : pappl-retrofit/cups-backends.c
//  Target header : pappl-retrofit/cups-backends-private.h
//
//  Public surface exercised:
//
//    bool   _prDummyDevice    (const char *, const char *, const char *,
//                              void *)
//    double _prGetCurrentTime (void)
//    int    _prCUPSCompareDevices (pr_backend_device_t *, pr_backend_device_t *)
//
//  NOT covered by this file (and why):
//
//    _prCUPSDevList()       — drives the actual line-by-line tokeniser for
//                             CUPS backend discovery output, but is
//                             entwined with fork(2) of every backend in
//                             /usr/lib/cups/backend, poll(2) on their
//                             pipes, and SIGCHLD handling.  The
//                             tokenisation logic at lines 463-568 of
//                             cups-backends.c is inline and cannot be
//                             reached without a real backend subprocess,
//                             so it is excluded from this hermetic file.
//                             A follow-up refactor that lifts the parser
//                             into a static helper would let us cover it
//                             here.
//
//    _prCUPSDevLog()        — depends on a live pappl_system_t and PAPPL's
//                             internal log dispatcher; not hermetic.
//
//    _prCUPSDevOpen/Close/Read/Write/Status/ID, _prCUPSDevLaunchBackend,
//    _prCUPSDevStopBackend, _prCUPSSigchldSigAction — all require real
//    backend processes and PAPPL device objects.
//
//  Test groups in this file (3 groups, 15 assertions):
//
//    G1 (T01-T02) ─ _prDummyDevice
//                   The "do-nothing" callback that papplDeviceList() uses
//                   when we only want PAPPL to initialise its standard
//                   themes without actually enumerating devices.  Must
//                   return true under every argument combination.
//
//    G2 (T03-T05) ─ _prGetCurrentTime
//                   Wall-clock-in-seconds helper used to time CUPS
//                   backend execution against MAX_BACKEND_TIME.  Must
//                   return a strictly positive double, must be
//                   monotonically non-decreasing across back-to-back
//                   calls within the same process, and must move forward
//                   across a short sleep.
//
//    G3 (T06-T15) ─ _prCUPSCompareDevices
//                   Three-tier comparator for cupsArrayNew() that
//                   de-duplicates raw backend output.  The tiers are:
//                     1. cfIEEE1284NormalizeMakeModel(device_info) under
//                        CF_IEEE1284_NORMALIZE_COMPARE | LOWERCASE |
//                        SEPARATOR_SPACE | PAD_NUMBERS, then strcasecmp
//                     2. strcasecmp(device_class)
//                     3. strcasecmp(device_uri)
//                   Every tier is exercised in isolation: identical
//                   inputs → 0; only info differs → info tier; identical
//                   info but different class → class tier; identical
//                   info and class but different URI → URI tier; and
//                   the case-insensitive guarantee at every tier.
//
//  Hermetic guarantees:
//    - No subprocess, no socket, no filesystem (apart from stdout/stderr
//      via the test framework).  No CUPS server, no PAPPL system, no
//      live device.  Every assertion is reproducible offline.
//    - Build dependencies are exactly what the library itself needs:
//      libpappl-retrofit.la + CUPS + cupsfilters + PPD + PAPPL.
// =============================================================================
//

#include "test-internal.h"
#include "cups-backends-private.h"

#include <cupsfilters/ieee1284.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>


// ==========================================================================
//  Helper: stamp a pr_backend_device_t with three string fields in one go.
//
//  pr_backend_device_t is a flat C struct of fixed-size char arrays
//  (device_class[128], device_info[128], device_uri[1024] — see
//  cups-backends-private.h:73-78).  All three fields are populated by
//  _prCUPSDevList() before any comparison ever happens, so the
//  comparator is allowed to assume non-NULL, NUL-terminated buffers.
//  This helper just keeps the test bodies tidy.
// ==========================================================================
static void
mkdev(pr_backend_device_t *d,
      const char          *cls,
      const char          *info,
      const char          *uri)
{
  // strncpy + force NUL at the last byte so we cannot accidentally
  // create a non-terminated buffer if a future test passes an
  // oversize string.  The function under test calls strcasecmp(),
  // which requires NUL-terminated input.
  memset(d, 0, sizeof(*d));
  strncpy(d->device_class, cls, sizeof(d->device_class) - 1);
  strncpy(d->device_info,  info, sizeof(d->device_info)  - 1);
  strncpy(d->device_uri,   uri,  sizeof(d->device_uri)   - 1);
}


int
main(void)
{
  // Two scratch devices used by every G3 test.  Declared here, not at
  // file scope, so that valgrind / leak detectors see the lifetime end
  // at main()'s return.
  pr_backend_device_t a, b;

  // ========================================================================
  //  GROUP 1 — _prDummyDevice
  //  --------------------------------------------------------------------
  //  This callback is wired up as the device-list callback whenever we
  //  want papplDeviceList() to spin through its theme initialisation
  //  *without* actually enumerating any USB/network device.  It must
  //  unconditionally return true so that PAPPL keeps walking its
  //  internal list rather than halting on the first "device".
  // ========================================================================

  // ----------------------------------------------------------------------
  //  T01 — Happy-path: realistic-looking arguments.
  //  The function does not inspect any argument (see
  //  cups-backends.c:43-50), so the values we pass are irrelevant —
  //  what matters is the unconditional return value.
  // ----------------------------------------------------------------------
  testBegin("T01: _prDummyDevice() returns true with valid arguments");
  {
    bool r = _prDummyDevice("HP LaserJet 4000",
                            "usb://HP/LaserJet%204000",
                            "MFG:HP;MDL:LaserJet 4000;",
                            NULL);
    testEnd(r == true);
  }

  // ----------------------------------------------------------------------
  //  T02 — Stress: every pointer NULL.  Because the function is a
  //  literal "return (true);" with no dereference, NULL inputs must
  //  also be accepted.  This guards against any future refactor that
  //  accidentally adds a dereference and breaks the PAPPL theme-init
  //  path.
  // ----------------------------------------------------------------------
  testBegin("T02: _prDummyDevice() returns true with all NULL arguments");
  {
    bool r = _prDummyDevice(NULL, NULL, NULL, NULL);
    testEnd(r == true);
  }


  // ========================================================================
  //  GROUP 2 — _prGetCurrentTime
  //  --------------------------------------------------------------------
  //  Implementation is a single gettimeofday(2) wrapped into a double
  //  (cups-backends.c:57-66).  It is used to drive the 15-second
  //  per-backend timeout loop in _prCUPSDevList().  Three invariants:
  //    (a) the value is strictly positive (seconds since the epoch);
  //    (b) two successive calls are non-decreasing — wall-clock time
  //        does not run backwards inside a single process; and
  //    (c) across a short, blocking sleep the value strictly advances.
  // ========================================================================

  // ----------------------------------------------------------------------
  //  T03 — A single call must yield a strictly positive double.  A
  //  zero or negative value would mean gettimeofday(2) failed silently,
  //  which the implementation does not check for, so we have to.
  // ----------------------------------------------------------------------
  testBegin("T03: _prGetCurrentTime() returns strictly positive seconds");
  {
    double t = _prGetCurrentTime();
    testEnd(t > 0.0);
  }

  // ----------------------------------------------------------------------
  //  T04 — Back-to-back: time must NOT go backwards across consecutive
  //  calls in the same thread.  We allow equality because the call is
  //  so cheap that two calls in the same microsecond is plausible on
  //  modern hardware.
  // ----------------------------------------------------------------------
  testBegin("T04: _prGetCurrentTime() is monotonically non-decreasing");
  {
    double t1 = _prGetCurrentTime();
    double t2 = _prGetCurrentTime();
    testEnd(t2 >= t1);
  }

  // ----------------------------------------------------------------------
  //  T05 — After a 50-millisecond sleep, the clock MUST advance by
  //  some non-trivial amount.  50 ms is well above the resolution
  //  floor of gettimeofday(2) on any platform we ship to, but well
  //  below the per-backend timeout (15 s) so the test stays fast.
  // ----------------------------------------------------------------------
  testBegin("T05: _prGetCurrentTime() advances across a 50ms sleep");
  {
    double t1 = _prGetCurrentTime();
    struct timespec req = { 0, 50 * 1000 * 1000 }; // 50 ms
    nanosleep(&req, NULL);
    double t2 = _prGetCurrentTime();
    // Allow slop: anything > 1ms confirms forward motion past
    // gettimeofday's microsecond granularity.
    testEndMessage(t2 - t1 > 0.001,
                   "delta=%.6f s", t2 - t1);
  }


  // ========================================================================
  //  GROUP 3 — _prCUPSCompareDevices
  //  --------------------------------------------------------------------
  //  Three-tier sort comparator.  The first tier passes both device_info
  //  strings through cfIEEE1284NormalizeMakeModel() under
  //  CF_IEEE1284_NORMALIZE_COMPARE | LOWERCASE | SEPARATOR_SPACE |
  //  PAD_NUMBERS, then strcasecmps the results.  That single tier
  //  collapses cosmetic differences ("HP", "Hewlett-Packard",
  //  "Hewlett Packard", trailing whitespace, runs of internal
  //  whitespace, mixed case) into one canonical form so that a printer
  //  reported by two different backends is correctly recognised as the
  //  same device.  Subsequent tiers (class, URI) are plain
  //  strcasecmp() and only break ties when the normalised info tier
  //  returns equal.
  // ========================================================================

  // ----------------------------------------------------------------------
  //  T06 — Identical inputs → comparator returns 0.  This is the
  //  fundamental sort invariant: equal records must compare equal so
  //  that cupsArrayAdd() de-duplicates correctly.
  // ----------------------------------------------------------------------
  testBegin("T06: identical devices compare equal (return 0)");
  {
    mkdev(&a, "direct", "HP LaserJet 4000", "usb://HP/LaserJet%204000");
    mkdev(&b, "direct", "HP LaserJet 4000", "usb://HP/LaserJet%204000");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff == 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T07 — Different make+model → first tier (normalised info) MUST
  //  detect the difference, regardless of class/URI.  The exact sign
  //  of the result depends on cfIEEE1284NormalizeMakeModel()'s
  //  internal ordering ("brother..." vs "hp..."), so we only assert
  //  non-zero here.
  // ----------------------------------------------------------------------
  testBegin("T07: different make/model produces non-zero comparison");
  {
    mkdev(&a, "direct", "HP LaserJet 4000",     "usb://A");
    mkdev(&b, "direct", "Brother HL-L2300D",    "usb://A");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff != 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T08 — Case-insensitive equality of the normalised info string.
  //  Lower- and upper-case spellings of the same model MUST compare
  //  equal because CF_IEEE1284_NORMALIZE_LOWERCASE is part of the
  //  flag set passed to cfIEEE1284NormalizeMakeModel().  This is what
  //  prevents a printer from appearing twice in the picker because
  //  one backend wrote "HP LASERJET" and another wrote "Hp LaserJet".
  // ----------------------------------------------------------------------
  testBegin("T08: case differences in device_info collapse to equal");
  {
    mkdev(&a, "direct", "HP LASERJET 4000", "usb://A");
    mkdev(&b, "direct", "hp laserjet 4000", "usb://A");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff == 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T09 — Equal info, equal class, but different URI: tiebreak by
  //  URI MUST kick in and report non-zero.  This is what stops two
  //  physically distinct printers of the same model from being merged.
  // ----------------------------------------------------------------------
  testBegin("T09: equal info & class but different URI → URI tier fires");
  {
    mkdev(&a, "direct", "HP LaserJet 4000", "usb://HP/LaserJet%204000?serial=AAA");
    mkdev(&b, "direct", "HP LaserJet 4000", "usb://HP/LaserJet%204000?serial=BBB");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff != 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T10 — Equal info, different class, same URI: tiebreak by class
  //  MUST kick in.  Class strings are bare strcasecmp() (no
  //  normalisation), so we can predict the sign: "direct" < "network".
  // ----------------------------------------------------------------------
  testBegin("T10: equal info but different class → class tier fires");
  {
    mkdev(&a, "direct",  "HP LaserJet 4000", "usb://X");
    mkdev(&b, "network", "HP LaserJet 4000", "usb://X");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff < 0, "diff=%d (expected <0 since 'direct'<'network')",
                   diff);
  }

  // ----------------------------------------------------------------------
  //  T11 — Reverse of T10.  Confirms the sign flips when we swap the
  //  operands, i.e. the comparator is anti-symmetric, which cupsArray
  //  relies on.
  // ----------------------------------------------------------------------
  testBegin("T11: comparator is anti-symmetric on the class tier");
  {
    mkdev(&a, "network", "HP LaserJet 4000", "usb://X");
    mkdev(&b, "direct",  "HP LaserJet 4000", "usb://X");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff > 0, "diff=%d (expected >0 — operands swapped)",
                   diff);
  }

  // ----------------------------------------------------------------------
  //  T12 — Case-insensitive equality on the class tier.  Like info,
  //  class is compared via strcasecmp(), so "Direct" and "direct"
  //  must collide.  We use identical URI so the URI tier returns 0
  //  too, exposing the class tier in isolation.
  // ----------------------------------------------------------------------
  testBegin("T12: class tier is case-insensitive");
  {
    mkdev(&a, "Direct", "HP LaserJet 4000", "usb://X");
    mkdev(&b, "direct", "HP LaserJet 4000", "usb://X");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff == 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T13 — Case-insensitive equality on the URI tier.  Even though
  //  URIs are usually case-sensitive in their host component, the
  //  implementation uses strcasecmp() here for consistency with the
  //  other tiers.  We assert the documented behaviour.
  // ----------------------------------------------------------------------
  testBegin("T13: URI tier is case-insensitive");
  {
    mkdev(&a, "direct", "HP LaserJet 4000", "USB://HP/LASERJET%204000");
    mkdev(&b, "direct", "HP LaserJet 4000", "usb://hp/laserjet%204000");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff == 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T14 — Make-prefix normalisation.  cfIEEE1284NormalizeMakeModel()
  //  collapses "Hewlett-Packard" / "Hewlett Packard" / "HP" to the
  //  same canonical form.  This test asserts that two records that
  //  differ only in the spelling of the maker name compare equal —
  //  which is the whole point of running the field through the
  //  normaliser before strcasecmp().
  // ----------------------------------------------------------------------
  testBegin("T14: Hewlett-Packard / HP prefixes normalise to equal");
  {
    mkdev(&a, "direct", "Hewlett-Packard LaserJet 4000", "usb://X");
    mkdev(&b, "direct", "HP LaserJet 4000",              "usb://X");
    int diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage(diff == 0, "diff=%d", diff);
  }

  // ----------------------------------------------------------------------
  //  T15 — Whitespace in device_info. Collapsing runs of spaces is
  //  libcupsfilters' policy: distro CUPS 2.x leaves them, and the
  //  cups-filters that the 2.5 and 3 CI jobs build from source
  //  collapses them. The comparator has to agree with the copy it
  //  linked, so this checks that instead of pinning one policy.
  // ----------------------------------------------------------------------
  testBegin("T15: device_info comparison follows the IEEE 1284 normaliser");
  {
    static const cf_ieee1284_normalize_modes_t mode =
        CF_IEEE1284_NORMALIZE_COMPARE |
        CF_IEEE1284_NORMALIZE_LOWERCASE |
        CF_IEEE1284_NORMALIZE_SEPARATOR_SPACE |
        CF_IEEE1284_NORMALIZE_PAD_NUMBERS;
    char bufa[1024], bufb[1024];
    char *na, *nb;
    int want, diff;

    mkdev(&a, "direct", "  HP   LaserJet  4000  ", "usb://X");
    mkdev(&b, "direct", "HP LaserJet 4000",         "usb://X");
    na = cfIEEE1284NormalizeMakeModel(a.device_info, NULL, mode,
                                      NULL, bufa, sizeof(bufa),
                                      NULL, NULL, NULL);
    nb = cfIEEE1284NormalizeMakeModel(b.device_info, NULL, mode,
                                      NULL, bufb, sizeof(bufb),
                                      NULL, NULL, NULL);
    want = strcasecmp(na, nb);
    diff = _prCUPSCompareDevices(&a, &b);
    testEndMessage((diff == 0) == (want == 0),
                   "diff=%d normaliser=%d", diff, want);
  }


  // ========================================================================
  //  Suite epilogue.  The test-internal.h framework sets the global
  //  `testsPassed` to false the moment any testEnd*() reports a failure.
  //  Map that boolean to a POSIX exit code so `make check` picks up
  //  the result without parsing log lines.
  // ========================================================================
  return (testsPassed ? 0 : 1);
}
