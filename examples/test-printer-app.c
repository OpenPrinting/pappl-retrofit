//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// Test Printer Application
//
// Copyright © 2020 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include <pappl-retrofit/base.h>


//
// Constants...
//

// Name and version

#define SYSTEM_NAME "CUPS Driver Retro-Fit Test Printer Application"
#define SYSTEM_PACKAGE_NAME "test-printer-app"
#define SYSTEM_VERSION_STR "1.0"
#define SYSTEM_VERSION_ARR_0 1
#define SYSTEM_VERSION_ARR_1 0
#define SYSTEM_VERSION_ARR_2 0
#define SYSTEM_VERSION_ARR_3 0
#define SYSTEM_WEB_IF_FOOTER "Copyright &copy; 2020 by Till Kamppeter. Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>."

// Test page

#define TESTPAGE "testpage.ps"


//
// 'test_autoadd()' - Auto-add printers.
//

const char *			        // O - Driver name or `NULL` for none
test_autoadd(const char *device_info,	// I - Device name (unused)
	     const char *device_uri,	// I - Device URI (unused)
	     const char *device_id,	// I - IEEE-1284 device ID
	     void       *data)          // I - Global data
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  const char	*ret = NULL;		// Return value


  (void)device_info;
  (void)device_uri;

  if (device_id == NULL || global_data == NULL)
    return (NULL);

  // Look at the COMMAND SET (CMD) key for the list of printer languages...
  if (1 || pr_supports_postscript(device_id))
    // Printer supports our PDL, so find the best-matching PPD file
    ret = pr_best_matching_ppd(device_id, global_data);
  else
    // Printer does not support our PDL, it is not supported by this
    // Printer Application
    ret = NULL;

  return (ret);
}


//
// 'main()' - Main entry for the test-printer-app.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  cups_array_t *spooling_conversions,
               *stream_formats;

  // Array of spooling conversions, most desirables first
  //
  // Here we prefer not converting into another format
  // Keeping vector formats (like PS -> PDF) is usually more desirable
  // but as many printers have buggy PS interpreters we prefer converting
  // PDF to Raster and not to PS
  spooling_conversions = cupsArrayNew(NULL, NULL);
  cupsArrayAdd(spooling_conversions, &pr_convert_pdf_to_pdf);
  cupsArrayAdd(spooling_conversions, &pr_convert_pdf_to_raster);
  cupsArrayAdd(spooling_conversions, &pr_convert_pdf_to_ps);
  cupsArrayAdd(spooling_conversions, &pr_convert_ps_to_ps);
  cupsArrayAdd(spooling_conversions, &pr_convert_ps_to_pdf);
  cupsArrayAdd(spooling_conversions, &pr_convert_ps_to_raster);

  // Array of stream formats, most desirables first
  //
  // PDF comes last because it is generally not streamable.
  // PostScript comes second as it is Ghostscript's streamable
  // input format.
  stream_formats = cupsArrayNew(NULL, NULL);
  cupsArrayAdd(stream_formats, &pr_stream_cups_raster);
  cupsArrayAdd(stream_formats, &pr_stream_postscript);
  cupsArrayAdd(stream_formats, &pr_stream_pdf);

  // Configuration record of the Printer Application
  pr_printer_app_config_t printer_app_config =
  {
    SYSTEM_NAME,
    SYSTEM_PACKAGE_NAME,
    SYSTEM_VERSION_STR,
    {
      SYSTEM_VERSION_ARR_0,
      SYSTEM_VERSION_ARR_1,
      SYSTEM_VERSION_ARR_2,
      SYSTEM_VERSION_ARR_3
    },
    SYSTEM_WEB_IF_FOOTER,
    PR_COPTIONS_QUERY_PS_DEFAULTS | PR_COPTIONS_WEB_ADD_PPDS |
      PR_COPTIONS_CUPS_BACKENDS,
    test_autoadd,
    pr_identify,
    pr_testpage,
    spooling_conversions,
    stream_formats,
    "driverless, driverless-fax, dnssd, ipp, ipps, http, https",
    "", //"hp, gutenprint53+usb",
    TESTPAGE
  };

  return (pr_retrofit_printer_app(&printer_app_config, argc, argv));
}
