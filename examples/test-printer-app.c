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

#define TESTPAGE "testpage.pdf"
//#define TESTPAGE "testpage.ps"


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
               *stream_formats,
               *driver_selection_regex_list;

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

  // Array of regular expressions for driver priorization
  driver_selection_regex_list = cupsArrayNew(NULL, NULL);
  cupsArrayAdd(driver_selection_regex_list, "-recommended-");
  cupsArrayAdd(driver_selection_regex_list, "-postscript-");
  cupsArrayAdd(driver_selection_regex_list, "-hl-1250-");
  cupsArrayAdd(driver_selection_regex_list, "-hl-7-x-0-");
  cupsArrayAdd(driver_selection_regex_list, "-pxlcolor-");
  cupsArrayAdd(driver_selection_regex_list, "-pxlmono-");
  cupsArrayAdd(driver_selection_regex_list, "-ljet-4-d-");
  cupsArrayAdd(driver_selection_regex_list, "-ljet-4-");
  cupsArrayAdd(driver_selection_regex_list, "-gutenprint-");

  // Configuration record of the Printer Application
  pr_printer_app_config_t printer_app_config =
  {
    SYSTEM_NAME,              // Display name for Printer Application
    SYSTEM_PACKAGE_NAME,      // Package/executable name
    SYSTEM_VERSION_STR,       // Version as a string
    {
      SYSTEM_VERSION_ARR_0,   // Version 1st number
      SYSTEM_VERSION_ARR_1,   //         2nd
      SYSTEM_VERSION_ARR_2,   //         3rd
      SYSTEM_VERSION_ARR_3    //         4th
    },
    SYSTEM_WEB_IF_FOOTER,     // Foother for web interface (in HTML)
    PR_COPTIONS_QUERY_PS_DEFAULTS | // pappl-retrofit special features to be
    PR_COPTIONS_WEB_ADD_PPDS |      // used
    PR_COPTIONS_CUPS_BACKENDS |
    PR_COPTIONS_NO_GENERIC_DRIVER,
    test_autoadd,             // Auto-add (driver assignment) callback
    pr_identify,              // Printer identify callback
    pr_testpage,              // Test page print callback
    pr_setup_add_ppd_files_page, // Set up "Add PPD Files" web interface page
    pr_setup_device_settings_page, // Set up "Device Settings" printer web
                              // interface page
    spooling_conversions,     // Array of data format conversion rules for
                              // printing in spooling mode
    stream_formats,           // Arrray for stream formats to be generated
                              // when printing in streaming mode
    "driverless, driverless-fax, dnssd, ipp, ipps, http, https",
                              // CUPS backends to be ignored
    "", //"hp, gutenprint53+usb",
                              // CUPS backends to be used exclusively
                              // If empty all but the ignored backends are used
    TESTPAGE,                 // Test page (printable file), used by the
                              // standard test print callback pr_testpage()
    " +Foomatic/(.+)$| +- +CUPS\\+(Gutenprint)",
                              // Regular expression to separate the
                              // extra information after make/model in
                              // the PPD's *NickName. Also extracts a
                              // contained driver name (by using
                              // parentheses)
    driver_selection_regex_list
                              // Regular expression for the driver
                              // auto-selection to prioritize a driver
                              // when there is more than one for a
                              // given printer. If a regular
                              // expression matches on the driver
                              // name, the driver gets priority. If
                              // there is more than one matching
                              // driver, the driver name on which the
                              // earlier regular expression in the
                              // list matches, gets the priority.
  };

  // If the "driverless" utility is under the CUPS backends or under
  // the PPD-denerating executables, tell it to not browse the network
  // for supported (driverless) printers but exit immediately, as this
  // Printer Application is for using printers with installed CUPS
  // drivers.
  putenv("NO_DRIVERLESS_PPDS=1");

  return (pr_retrofit_printer_app(&printer_app_config, argc, argv));
}
