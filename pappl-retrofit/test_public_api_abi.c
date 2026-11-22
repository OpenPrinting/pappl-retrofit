//
// =============================================================================
//  test_public_api_abi.c — Compile-time guard that pappl-retrofit's public,
//                          PAPPL-version-INDEPENDENT functions keep the
//                          exact signature documented in pappl-retrofit.h,
//                          regardless of whether HAVE_PAPPL1 is defined.
// =============================================================================
//
//  Target header : pappl-retrofit/pappl-retrofit.h
//
//  Background:
//
//    pappl-retrofit builds against either PAPPL 1.4.x or PAPPL 2.x
//    (HAVE_PAPPL1 selects which), and several of PAPPL's OWN callback
//    typedefs legitimately differ in shape between the two (see
//    pappl1-private.h). pappl-retrofit's OWN exported functions are a
//    separate, higher-level contract: they are documented to keep one
//    fixed signature no matter which PAPPL is linked, with any version
//    differences absorbed internally (via pappl1-private.h shims or small
//    adapter/trampoline functions), never leaked into these signatures.
//
//    That contract was broken once, silently: an early attempt at the
//    PAPPL 2.x build fix added a HAVE_PAPPL1 conditional parameter
//    directly to prPrintFile()'s own prototype. Every CI leg still built
//    fine on its own (each leg is internally consistent in isolation), so
//    nothing caught it. This file exists so the next version-dependent
//    signature leak fails the build outright, in every leg, instead of
//    slipping through again.
//
//  Mechanism:
//
//    Each PR_ABI_CHECK() below is a file-scope _Static_assert comparing a
//    function's actual type (via __typeof__) against its exact,
//    non-conditional signature from pappl-retrofit.h. This is a hard
//    compile error on mismatch, independent of -Wall/-Werror — this
//    project does not enable -Werror by default (see configure.ac), so a
//    plain implicit-pointer-type warning alone would not have failed CI.
// =============================================================================
//

#include "test-internal.h"
#include "pappl-retrofit.h"

#define PR_ABI_CHECK(name, ...) \
  _Static_assert(__builtin_types_compatible_p(__typeof__(name), __VA_ARGS__), \
                 #name " no longer matches its documented, " \
                 "PAPPL-version-independent signature")

PR_ABI_CHECK(prGetSystem,
             pappl_system_t *(pr_printer_app_global_data_t *));
PR_ABI_CHECK(prRetroFitPrinterApp,
             int (pr_printer_app_config_t *, int, char **));
PR_ABI_CHECK(prBestMatchingPPD,
             const char *(const char *, pr_printer_app_global_data_t *));
PR_ABI_CHECK(prRegExMatchDevIDField,
             int (const char *, const char *, const char *,
                  pr_devid_regex_mode_t));
PR_ABI_CHECK(prSupportsPostScript, bool (const char *));
PR_ABI_CHECK(prSupportsPDF, bool (const char *));
PR_ABI_CHECK(prSupportsPCL5, bool (const char *));
PR_ABI_CHECK(prSupportsPCL5c, bool (const char *));
PR_ABI_CHECK(prSupportsPCLXL, bool (const char *));
PR_ABI_CHECK(prAutoAdd,
             const char *(const char *, const char *, const char *, void *));
PR_ABI_CHECK(prPSIdentify, void (pappl_printer_t *, pappl_device_t *));
PR_ABI_CHECK(prIdentify,
             void (pappl_printer_t *, pappl_identify_actions_t, const char *));
PR_ABI_CHECK(prTestPage,
             const char *(pappl_printer_t *, char *, size_t));
PR_ABI_CHECK(prPWGRasterEndJob,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *));
PR_ABI_CHECK(prPWGRasterEndPage,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned));
PR_ABI_CHECK(prPWGRasterStartJob,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *));
PR_ABI_CHECK(prPWGRasterStartPage,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned));
PR_ABI_CHECK(prPWGRasterWriteLine,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned, const unsigned char *));
PR_ABI_CHECK(prPSRasterEndJob,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *));
PR_ABI_CHECK(prPSRasterEndPage,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned));
PR_ABI_CHECK(prPSRasterStartJob,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *));
PR_ABI_CHECK(prPSRasterStartPage,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned));
PR_ABI_CHECK(prPSRasterWriteLine,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *,
                   unsigned, const unsigned char *));
PR_ABI_CHECK(prPrintFile,
             bool (pappl_job_t *, pappl_pr_options_t *, pappl_device_t *));
PR_ABI_CHECK(prSetupAddPPDFilesPage, void (void *));
PR_ABI_CHECK(prSetupDeviceSettingsPage, void (pappl_printer_t *, void *));


int
main(void)
{
  // Every check above already ran at compile time; reaching main() at all
  // means none of them fired, so there is nothing left to assert here.
  testBegin("Public API functions keep their PAPPL-version-independent "
            "signatures");
  testEnd(true);

  return (testsPassed ? 0 : 1);
}
