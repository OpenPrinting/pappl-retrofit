//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// pappl-retrofit.c
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

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <pappl-retrofit/pappl-retrofit.h>


//
// 'pr_retrofit_printer_app()' - Run the driver-retro-fitting printer
//                               application with a given configuration
//

int
pr_retrofit_printer_app(pr_printer_app_config_t *printer_app_config,
			int  argc,	 // I - Number of command-line arguments
			char *argv[])    // I - Command-line arguments
{
  int ret;

  // Blank global variable array with above config hooked in
  pr_printer_app_global_data_t global_data;
  memset(&global_data, 0, sizeof(global_data));
  global_data.config = printer_app_config;

  // Run PAPPL main loop with PAPPL retro-fit framework
  ret = papplMainloop(argc, argv,      // Command line arguments
		      global_data.config->version, // Version number
		      NULL,            // HTML Footer for web interface
		      0,               // Number of drivers for built-in setup
		      NULL,            // Driver list for built-in setup
		      NULL,            // Printer auto-addition callback
		                       // (will be set later)
		      NULL,            // Setup callback for selected driver
		      NULL,            // Sub-command name
		      NULL,            // Callback for sub-command
		      pr_system_cb,    // System creation callback
		      NULL,            // Usage info output callback
		      &global_data);    // Global data

  // Clean up
  cupsArrayDelete(global_data.config->spooling_conversions);
  cupsArrayDelete(global_data.config->stream_formats);
  if (global_data.config->driver_selection_regex_list)
    cupsArrayDelete(global_data.config->driver_selection_regex_list);
  
  return (ret);
}


//
// 'pr_best_matching_ppd()' - Find the PPD which best matches the
//                            given device ID. Highest weight has
//                            matching make and model against the make
//                            and model of the PPD's device ID. After
//                            that we normalize the device ID to IPP
//                            name format and match against the driver
//                            name, which is the PPD's make, model,
//                            and language in IPP name
//                            format. User-added PPDs always have
//                            priority. If for the given device ID
//                            there are several matching PPDs which
//                            differ only by their UI language,
//                            English is currently preferred. When
//                            PAPPL gets internationalization later,
//                            we eill also support auto-selecting PPDs
//                            in the user's language.
//

const char *			// O - Driver name or `NULL` for none
pr_best_matching_ppd(const char *device_id,	// I - IEEE-1284 device ID
		     pr_printer_app_global_data_t *global_data)
{
  int           i, j;
  const char	*ret = NULL;		// Return value
  int		num_did;		// Number of device ID key/value pairs
  cups_option_t	*did = NULL;		// Device ID key/value pairs
  int		num_ddid;		// Device ID of driver list entry
  cups_option_t	*ddid = NULL;		// Device ID of driver list entry
  const char	*mfg, *dmfg, *mdl, *dmdl; // Device ID fields
  char          buf[1024];
  int           score, best_score = 0,
                best = -1;
  cups_array_t  *compiled_re_list = NULL;
  regex_t       *re;
  const char    *regex;
  int           num_drivers = global_data->num_drivers;
  pappl_pr_driver_t *drivers = global_data->drivers;


  if (device_id == NULL || num_drivers == 0 || drivers == NULL)
    return (NULL);

  // Parse the IEEE-1284 device ID to see if this is a printer we support...
  num_did = papplDeviceParseID(device_id, &did);
  if (num_did == 0 || did == NULL)
    return (NULL);

  // Make and model
  if ((mfg = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
    mfg = cupsGetOption("MFG", num_did, did);
  if ((mdl = cupsGetOption("MODEL", num_did, did)) == NULL)
    mdl = cupsGetOption("MDL", num_did, did);

  if (mfg && mdl)
  {
    // Normalize device ID to format of driver name and match
    ieee1284NormalizeMakeAndModel(device_id, NULL,
				  IEEE1284_NORMALIZE_IPP, NULL,
				  buf, sizeof(buf),
				  NULL, NULL, NULL);

    // Compile regular expressions to prioritize drivers
    if (global_data->config->driver_selection_regex_list)
    {
      compiled_re_list = cupsArrayNew(NULL, NULL);
      for (regex = (const char *)
	     cupsArrayFirst(global_data->config->driver_selection_regex_list);
	   regex;
	   regex = (const char *)
	     cupsArrayNext(global_data->config->driver_selection_regex_list))
      {
	if ((re = (regex_t *)calloc(1, sizeof(regex_t))) != NULL)
        {
	  if (regcomp(re, regex, REG_ICASE | REG_EXTENDED | REG_NOSUB))
	  {
	    regfree(re);
	    papplLog(global_data->system, PAPPL_LOGLEVEL_ERROR,
		     "Invalid regular expression: %s", regex);
	    continue;
	  }
	}
	else
	{
	  papplLog(global_data->system, PAPPL_LOGLEVEL_ERROR,
		   "Out of memory, cannot add more regular expressions to driver priorization list, first not included one is: %s", regex);
	  break;
	}
	cupsArrayAdd(compiled_re_list, re);
      }
    }

    // Match make and model with device ID of driver list entry
    for (i = 1; i < num_drivers; i ++)
    {
      score = 0;

      // Match make and model with device ID of driver list entry
      if (drivers[i].device_id[0] &&
	  (num_ddid = papplDeviceParseID(drivers[i].device_id, &ddid)) > 0 &&
	  ddid != NULL)
      {
	if ((dmfg = cupsGetOption("MANUFACTURER", num_ddid, ddid)) == NULL)
	  dmfg = cupsGetOption("MFG", num_ddid, ddid);
	if ((dmdl = cupsGetOption("MODEL", num_ddid, ddid)) == NULL)
	  dmdl = cupsGetOption("MDL", num_ddid, ddid);
	if (dmfg && dmdl &&
	    strcasecmp(mfg, dmfg) == 0 &&
	    strcasecmp(mdl, dmdl) == 0)
	  // Match
	  score += 2000;
	cupsFreeOptions(num_ddid, ddid);
      }

      // Match normalized device ID with driver name
      if (score == 0 && strncmp(buf, drivers[i].name, strlen(buf)) == 0)
	// Match
	score += 1000;

      // PPD must at least match make and model to get considered
      if (score == 0)
	continue;

      // User-added? Prioritize, as if the user adds something, he wants
      // to use it
      if (strstr(drivers[i].name, "-user-added"))
	score += 32000;

      // PPD matches user's/system's language?
      // To be added when PAPPL supports internationalization (TODO)
      // score + 8000 for 2-char language
      // score + 16000 for 5-char language/country

      // PPD is English language version?
      if (!strcmp(drivers[i].name + strlen(drivers[i].name) - 3, "-en") ||
	  !strncmp(drivers[i].name + strlen(drivers[i].name) - 6, "-en-", 4))
	score += 4000;

      // Match the regular expressions on the driver name
      if (compiled_re_list)
      {
	for (j = 0, re = (regex_t *)cupsArrayFirst(compiled_re_list);
	     re; j ++, re = (regex_t *)cupsArrayNext(compiled_re_list))
	{
	  if (!regexec(re, drivers[i].name, 0, NULL, 0))
	  {
	    // Regular expression matches
	    score += (500 - j);
	    papplLog(global_data->system, PAPPL_LOGLEVEL_DEBUG,
		     "Driver %s matched driver priority regular expression %d: \"%s\"",
		     drivers[i].name, j + 1,
		     (char *)cupsArrayIndex(
		       global_data->config->driver_selection_regex_list, j));
	    break;
	  }
	}
      }

      // Better match than the previous one?
      if (score > best_score)
      {
	best_score = score;
	best = i;
      }
    }

    if (compiled_re_list)
    {
      for (re = (regex_t *)cupsArrayFirst(compiled_re_list);
	   re; re = (regex_t *)cupsArrayNext(compiled_re_list))
	regfree(re);
      cupsArrayDelete(compiled_re_list);
    }
  }

  // Found at least one match? Take the best one
  if (best >= 0)
    ret = drivers[best].name;
  // None of the PPDs matches? Assign the generic PPD if we have one
  else if (!strcasecmp(drivers[0].name, "generic"))
    ret = "generic";
  else
    ret = NULL;

  // Clean up
  cupsFreeOptions(num_did, did);

  return (ret);
}


//
// 'pr_regex_match_devid_field()' - This function receives a device
//                                  ID, the name of one of the device
//                                  ID's fields, a regular expression
//                                  (POSIX-extended,
//                                  case-insensitive), and a mode bit.
//                                  The regular expression is matched
//                                  against the value of the selected
//                                  field, depending on the mode bit
//                                  against the whole value string or
//                                  against each of the
//                                  comma-separated items. Return
//                                  value is -4 if one of the input
//                                  strings is NULL or empty, -3 on an
//                                  invalid regular expression, -2 on
//                                  an invalid device ID, -1 on the
//                                  requested field not in the device
//                                  ID, 0 on no match, and the number
//                                  of matching items otherwise.
//

int                                        // O - >  0: Match(es) found
                                                  //     =  0: No match
                                                  //     = -1: Field not found
                                                  //     < -1: Error
pr_regex_match_devid_field(const char *device_id, // I - Device ID to search in
			   const char *key,       // I - Name of the field to
			                          // I - match the regexp on
			   const char *value_regex, // I - Regular expression
			   pr_devid_regex_mode_t mode) // I - Matching mode
{
  int	        ret = 0;		// Return value
  int		num_did;		// Number of device ID key/value pairs
  cups_option_t	*did = NULL;		// Device ID key/value pairs
  const char    *value;                 // String to match the regexp on
  char          buf[2048];              // Buffer to manipulate the value
                                        // string in
  regex_t	*re;                    // Compiled regular expression
  char          *ptr1, *ptr2;

  if (!device_id || !device_id[0] || !key || !key[0] ||
      !value_regex || !value_regex[0])
    return (-4);

  // Parse the IEEE-1284 device ID to find the field we are looking after
  num_did = papplDeviceParseID(device_id, &did);
  if (num_did == 0 || did == NULL)
    return (-2);

  // Does the device ID contain the requested field?
  if ((value = cupsGetOption(key, num_did, did)) == NULL)
  {
    ret = -1;
    goto out;
  }

  if ((re = (regex_t *)calloc(1, sizeof(regex_t))) != NULL)
  {
    // Compile the regular expression
    if (regcomp(re, value_regex, REG_ICASE | REG_EXTENDED | REG_NOSUB))
    {
      regfree(re);
      ret = -3;
      goto out;
    }

    // Copy the value string
    strncpy(buf, value, sizeof(buf) - 1);

    // Go through the value's comma-separated items and match the
    // regular expression
    for (ptr1 = buf; *ptr1;)
    {
      // Find and mark end of item
      if (mode == PR_DEVID_REGEX_MATCH_ITEM &&
	  (ptr2 = strchr(ptr1, ',')) != NULL)
      {
	*ptr2 = '\0';
	ptr2 ++;
      }
      else
	ptr2 = ptr1 + strlen(ptr1);

      // Match the regular expression on the item
      if (!regexec(re, ptr1, 0, NULL, 0))
	ret ++;

      // Next item
      ptr1 = ptr2;
    }

    regfree(re);
  }
  else
    ret = -5;

 out:
  // Clean up
  cupsFreeOptions(num_did, did);

  return (ret);
}


//
// 'pr_supports_postscript()' - Check by the device ID whether a printer
//                              supports PostScript
//

bool
pr_supports_postscript(const char *device_id)
{
  const char *regexp = "^(POSTSCRIPT|BRSCRIPT|PS$|PS2$|PS3$)";
  return(pr_regex_match_devid_field(device_id, "CMD",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0 ||
	 pr_regex_match_devid_field(device_id, "COMMAND SET",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0);
}


//
// 'pr_supports_pdf()' - Check by the device ID whether a printer
//                       supports PDF
//

bool
pr_supports_pdf(const char *device_id)
{
  const char *regexp = "^(PDF)";
  return(pr_regex_match_devid_field(device_id, "CMD",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0 ||
	 pr_regex_match_devid_field(device_id, "COMMAND SET",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0);
}


//
// 'pr_supports_pcl5()' - Check by the device ID whether a printer
//                        supports PCL 5(c/e)
//

bool
pr_supports_pcl5(const char *device_id)
{
  const char *regexp = "^(PCL([ -]?5([ -]?[ce])?)?)$";
  return(pr_regex_match_devid_field(device_id, "CMD",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0 ||
	 pr_regex_match_devid_field(device_id, "COMMAND SET",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0);
}


//
// 'pr_supports_pcl5c()' - Check by the device ID whether a printer
//                         supports PCL 5c (color)
//

bool
pr_supports_pcl5c(const char *device_id)
{
  const char *regexp = "^(PCL[ -]?5[ -]?c)$";
  return(pr_regex_match_devid_field(device_id, "CMD",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0 ||
	 pr_regex_match_devid_field(device_id, "COMMAND SET",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0);
}


//
// 'pr_supports_pclxl()' - Check by the device ID whether a printer
//                         supports PCL-XL
//

bool
pr_supports_pclxl(const char *device_id)
{
  const char *regexp = "^(PCL[ -]?XL|PXL|PCL[ -]?6)$";
  return(pr_regex_match_devid_field(device_id, "CMD",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0 ||
	 pr_regex_match_devid_field(device_id, "COMMAND SET",
				    regexp, PR_DEVID_REGEX_MATCH_ITEM) > 0);
}


//
// 'pr_autoadd()' - Auto-add printer simply by the best-matching PPD file
//

const char *			// O - Driver name or `NULL` for none
pr_autoadd(const char *device_info,	// I - Device name (unused)
	   const char *device_uri,	// I - Device URI (unused)
	   const char *device_id,	// I - IEEE-1284 device ID
	   void       *data)            // I - Global data
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;


  (void)device_info;
  (void)device_uri;

  if (device_id == NULL || global_data == NULL)
    return (NULL);

  // Simply find best-matching PPD
  return (pr_best_matching_ppd(device_id, global_data));
}


//
// 'pr_ps_identify()' - Identify a PostScript printer by sending a
//                      zero-page job. This should cause the printer
//                      to make some noise and/or light up its
//                      display.
//

void
pr_ps_identify(
    pappl_printer_t          *printer,	// I - Printer
    pappl_device_t           *device)
{
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;
  ppd_file_t             *ppd = NULL;	// PPD file of the printer


  // Identify the printer by sending a zero-page PostScript job to
  // make the display of the printer light up and depending on
  // hardware mechanics move and/or signal sounds play

  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  ppd = extension->ppd;

  // Note: We directly output to the printer device without using
  //       pr_print_filter_function() as only use printf()/puts() and
  //       not any PPD-related function of libppd for the output to
  //       the printer

  //
  // Put the printer in PostScript mode and initiate a PostScript
  // file...
  //

  if (ppd->jcl_begin)
  {
    papplDevicePuts(device, ppd->jcl_begin);
    papplDevicePuts(device, ppd->jcl_ps);
  }

  papplDevicePuts(device, "%!\n");
  papplDeviceFlush(device);

  //
  // Delay...
  //

  sleep(3);

  //
  // Finish the job...
  //

  if (ppd->jcl_end)
    papplDevicePuts(device, ppd->jcl_end);
  else
    papplDevicePuts(device, "\004");
  papplDeviceFlush(device);
}


//
// 'pr_identify()' - Identify the printer. As there is no standard way
//                   for an arbitrary printer to identify itself (make
//                   noise, light up display or any LEDs, ... without
//                   printing something and so wasting paper and
//                   ink/toner) we need to try different approaches
//                   and hope that one of them does the
//                   trick. Currently we send a zero-page PostScript
//                   job (if printer understands PostScript), a soft
//                   reset command, a status check command, and a
//                   request for the device ID. Not all printers react
//                   visibly or audibly to these operations, also not
//                   all print backends (both of PAPPL and CUPS
//                   actally support all these operations. So it is
//                   not assured whether this function actually works
//                   with your printer.
//

void
pr_identify(
    pappl_printer_t          *printer,	// I - Printer
    pappl_identify_actions_t actions, 	// I - Actions to take
    const char               *message)	// I - Message, if any
{
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;
  pappl_device_t         *device;       // PAPPL output device
  pappl_preason_t        reasons;
  char                   buffer[2048];
  char                   *device_id;
  cups_sc_status_t sc_status;
  int datalen;
  pr_cups_device_data_t *device_data;


  (void)actions;
  (void)message;

  if ((device = papplPrinterOpenDevice(printer)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR,
		    "Unable to open device for printer %s",
		    papplPrinterGetName(printer));
    return;
  }

  // Try to find out whether the printer understands PostScript and
  // send a zero-page PostScript job (without any
  // filtering/converting) then First check whether we have a pure
  // PostScript PPD (no CUPS filter), also look at the COMMAND SET
  // (CMD) key of the device ID whether PostScript is supported by the
  // printer. We do this independently on how we actually print. So if
  // we have for example chosen a PCL driver but the printer also
  // understands PostScript, we do this identifivation, too.
  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  if (extension->filterless_ps ||
      pr_supports_postscript(papplPrinterGetDeviceID(printer)))
  {
    // Printer supports PostScript, so we can send the zero-page PostScript
    // job
    pr_ps_identify(printer, device);
  }
  

  // If we have a CUPS backend, try a soft reset on the printer via side
  // channel
  if (strncmp(papplPrinterGetDeviceURI(printer), "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Start backend if not yet done so (first access is not by PAPPL device
    // API function)
    if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
      return;

    // Standard FD for the side channel is 4, the CUPS library
    // functions use always FD 4. Therefore we redirect our side
    // channel pipe end to FD 4
    dup2(device_data->sidefd, 4);

    datalen = 0;
    if ((sc_status = cupsSideChannelDoRequest(CUPS_SC_CMD_SOFT_RESET, NULL,
					      &datalen,
					      device_data->side_timeout)) !=
	CUPS_SC_STATUS_OK)
	papplDeviceError(device, "Side channel error status: %s",
			 pr_cups_sc_status_str[sc_status]);
    else if (datalen > 0)
      papplLog(device_data->global_data->system, PAPPL_LOGLEVEL_DEBUG,
	       "Soft reset sent");
  }

  // Identify the printer by doing a device status request
  reasons = papplDeviceGetStatus(device);
  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		  "Status of printer %s: %d",
		  papplPrinterGetName(printer), reasons);

  // Try also to poll the device ID
  device_id = papplDeviceGetID(device, buffer, sizeof(buffer));
  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		  "Device ID of printer %s: %s",
		  papplPrinterGetName(printer), device_id);

  papplPrinterCloseDevice(printer);
  device = NULL;
}


//
// 'pr_compare_ppd_paths()' - Compare function for sorting PPD path array
//

int
pr_compare_ppd_paths(void *a,
		     void *b,
		     void *data)
{
  pr_ppd_path_t *aa = (pr_ppd_path_t *)a;
  pr_ppd_path_t *bb = (pr_ppd_path_t *)b;

  (void)data;
  return (strcmp(aa->driver_name, bb->driver_name));
}


//
// 'pr_driver_delete()' - Free dynamic data structures of the driver when
//                        removing a printer.
//

void
pr_driver_delete(
    pappl_printer_t *printer,              // I - Printer to be removed
    pappl_pr_driver_data_t *driver_data)   // I - Printer's driver data
{
  int                   i;
  pr_driver_extension_t *extension;
  ipp_name_lookup_t     *opt_name;


  if (printer)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "Freeing memory from driver data");

  extension = (pr_driver_extension_t *)driver_data->extension;

  // PPD file

  // We do the removal of the PPD cache separately to assure that the
  // function of libppd (and not of libcups) is used, as in libppd the
  // PPD cache data structure is different (Content optimize presets
  // added).
  ppdCacheDestroy(extension->ppd->cache);
  extension->ppd->cache = NULL;
  ppdClose(extension->ppd);

  // Media source
  for (i = 0; i < driver_data->num_source; i ++)
    if (driver_data->source[i])
      free((char *)(driver_data->source[i]));

  // Media type
  for (i = 0; i < driver_data->num_type; i ++)
    if (driver_data->type[i])
      free((char *)(driver_data->type[i]));

  // Media size
  for (i = 0; i < driver_data->num_media; i ++)
    if (driver_data->media[i])
      free((char *)(driver_data->media[i]));

  // Output bins
  for (i = 0; i < driver_data->num_bin; i ++)
    if (driver_data->bin[i])
      free((char *)(driver_data->bin[i]));

  // Vendor options
  for (i = 0; i < driver_data->num_vendor; i ++)
  {
    if (driver_data->vendor[i])
      free((char *)(driver_data->vendor[i]));
    if (extension->vendor_ppd_options[i])
      free((char *)(extension->vendor_ppd_options[i]));
  }

  // Extension
  for (opt_name =
	 (ipp_name_lookup_t *)cupsArrayFirst(extension->ipp_name_lookup);
       opt_name;
       opt_name =
	 (ipp_name_lookup_t *)cupsArrayNext(extension->ipp_name_lookup))
  {
    free(opt_name->ipp);
    free(opt_name);
  }
  cupsArrayDelete(extension->ipp_name_lookup);
  free(extension->stream_filter);
  if (extension->temp_ppd_name)
  {
    unlink(extension->temp_ppd_name);
    free(extension->temp_ppd_name);
  }
  free(extension);
}


//
// 'pr_cups_filter_path()' - Check whether a CUPS filter is present
//                           and if so return its absolute path,
//                           otherwise NULL
//

char *                               // O - Executable path of filter,
                                            //     NULL if filter not found or
                                            //     not executable
pr_cups_filter_path(const char *filter,     // I - CUPS filter name
		    const char *filter_dir) // I - Filter directory
{
  char		*filter_path;      /* Path to filter executable */

  if (!filter || !filter[0] || !filter_dir[0])
    return (NULL);

  if (filter[0] == '/')
  {
    if ((filter_path = (char *)calloc(strlen(filter) + 1,
				      sizeof(char))) == NULL)
      return (NULL);
    snprintf(filter_path, strlen(filter) + 1, "%s", filter);
  }
  else
  {
    if ((filter_path = (char *)calloc(strlen(filter_dir) + strlen(filter) + 2,
				      sizeof(char))) == NULL)
      return (NULL);
    snprintf(filter_path, strlen(filter_dir) + strlen(filter) + 2,
	     "%s/%s", filter_dir, filter);
  }

  if (access(filter_path, X_OK) == 0)
    return (filter_path);

  free(filter_path);
  return (NULL);
}


//
// 'pr_ppd_find_cups_filter()' - Check the strings of the
//                               "*cupsFilter(2):" lines in a PPD file
//                               whether there is a suitable filter
//                               applying to a given input
//                               format. Return the path to call the
//                               filter or NULL if there is no
//                               suitable one.
//

char *                           // O - Executable path of filter,
                                        //     NULL if filter not found or
                                        //     not executable
pr_ppd_find_cups_filter(const char *input_format, // I - Input data format
			int num_filters,          // I - Number of filter
			                          //     entries in PPD
			char **filters,           // I - Pointer to PPD's
                                                  //     filter list entries
			const char *filter_dir)   // I - Filter directory
{
  int i,
      cost,
      lowest_cost = 9999999;
  char *filter_str,
       *filter_name,
       *filter_cost,
       *filter_path = NULL,
       *filter_selected = NULL;

  if (num_filters == 0)
  {
    // PostScript output with native PostScript PPD
    if (strcmp(input_format, "application/vnd.cups-postscript") == 0)
      return strdup(".");
    else
      return (NULL);
  }

  for (i = 0; i < num_filters; i++)
  {
    // String of the "*cupsfilter:" or "*cupsfilter2:"
    filter_str = filters[i];

    // First word of the filter entry string is the input format of the filter
    if (strncmp(filter_str, input_format, strlen(input_format)) == 0 &&
	isspace(filter_str[strlen(input_format)]))
    {
      // This filter takes the desired input data format

      // The name of the filter executable is the last word
      filter_name = filter_str + strlen(filter_str) - 1;
      while (!isspace(*filter_name) && filter_name > filter_str)
	filter_name --;
      if (filter_name == filter_str)
	continue;

      // The cost value of the filter is the second last word
      filter_cost = filter_name;
      filter_name ++;
      if (!filter_name[0])
	continue;
      while (isspace(*filter_cost) && filter_cost > filter_str)
	filter_cost --;
      while (!isspace(*filter_cost) && filter_cost > filter_str)
	filter_cost --;
      if (filter_cost == filter_str)
	continue;
      filter_cost ++;
      if (!isdigit(*filter_cost))
	continue;
      cost = atoi(filter_cost);

      if (strcmp(filter_name, "-") == 0)
	// Null filter ("-")
	filter_path = strdup("-");
      else if ((filter_path = pr_cups_filter_path(filter_name, filter_dir)) ==
	       NULL)
      {
	// Filter is not installed
	if (num_filters == 1 &&
	    strcmp(input_format, "application/vnd.cups-postscript") == 0)
	  // PostScript PPD with filter, but filter not installed. Usually
	  // most options work. So accept but mark as such.
	  filter_path = strdup(".");
	else
	  // Filter missing, ignore.
	  continue;
      }

      // If there is more than one suitable filter entry, take the one with
      // the lowest cost value
      if (cost < lowest_cost)
      {
	if (filter_selected)
	  free(filter_selected);
	filter_selected = filter_path;
	lowest_cost = cost;
	if (cost == 0)
	  break;
      }
      else
	free(filter_path);
    }
  }

  return (filter_selected);
}


//
// 'pr_ppd_missing_filters()' - Check the strings of the
//                              "*cupsFilter(2):" lines in a PPD file
//                              whether all the CUPS filters defined
//                              in them are actually installed. List
//                              the filters which are missing.
//

char *                           // O - Executable path of filter,
                                        //     NULL if filter not found or
                                        //     not executable
pr_ppd_missing_filters(int num_filters,          // I - Number of filter
		                                 //     entries in PPD
		       char **filters,           // I - Pointer to PPD's
                                                 //     filter list entries
		       const char *filter_dir)   // I - Filter directory
{
  int i;
  char *filter_str,
       *filter_name,
       *filter_path = NULL,
       buf[2048];

  if (num_filters == 0)
    return NULL;

  buf[0] = '\0';
  for (i = 0; i < num_filters; i++)
  {
    // String of the "*cupsfilter:" or "*cupsfilter2:"
    filter_str = filters[i];

    // The name of the filter executable is the last word
    filter_name = filter_str + strlen(filter_str) - 1;
    while (!isspace(*filter_name) && filter_name > filter_str)
      filter_name --;
    if (filter_name == filter_str)
      continue;

    filter_name ++;
    if (!filter_name[0])
      continue;

    if (!filter_name[0] || strcmp(filter_name, "-") == 0)
      // Null filter ("-")
      continue;
    else if ((filter_path = pr_cups_filter_path(filter_name, filter_dir)) ==
	     NULL)
    {
      // Filter is not installed, add it to the list
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
	       "%s, ", filter_name);
    }
    else
      free(filter_path);
  }

  if (strlen(buf) > 1)
  {
    buf[strlen(buf) - 2] = '\0';
    return strdup(buf);
  }

  return (NULL);
}


//
// 'pr_str_has_code()' - Check a string whether it contains active PostScript
//                       or PJL code and not only whitespace and comments
//

bool
pr_str_has_code(
    const char *str)   // I - String to check
{
  const char *ptr;
  bool       incomment = false;


  if (!str)
    return(false);

  for (ptr = str; *ptr; ptr ++)
  {
    if (*ptr == '%' && *(ptr + 1) == '%')  // PostScript comment up to end of
                                           // line
      incomment = true;
    else if (*ptr == '\n' || *ptr == '\r') // New line
      incomment = false;
    else if (!incomment && !isspace(*ptr)) // Active code character
      return (true);
  }
  return (false);
}


// 'pr_option_has_code()' - Check a PPD option whether it has active
//                          PostScript or PJL code in enough choices
//                          for the option and all its choices making
//                          sense.
//                          This means that at maximum one choice can
//                          be without code.
//

bool
pr_option_has_code(
    pappl_system_t *system, // I - System (for logging)
    ppd_file_t     *ppd,    // I - PPD file
    ppd_option_t   *option) // I - Option to check
{
  int i;
  int codeless_choices = 0;


  if (option->ui == PPD_UI_PICKONE || option->ui == PPD_UI_BOOLEAN)
  {
    for (i = 0; i < option->num_choices; i ++)
      if (!pr_str_has_code(option->choices[i].code))
	codeless_choices ++;
    if (codeless_choices > 1)
    {
      papplLog(system, PAPPL_LOGLEVEL_WARN,
	       "Skipping option \"%s\", the PPD file does not provide PostScript/PJL code for all its choices.",
	       option->keyword);
      if (ppd->num_filters)
	papplLog(system, PAPPL_LOGLEVEL_WARN,
		 "This option most probably needs a CUPS filter to work. Is this a PostScript PPD?");
      else
	papplLog(system, PAPPL_LOGLEVEL_WARN,
		 "The PPD file is probably broken.");
      return (false);
    }
  }
  return (true);
}


//
// 'pr_default_paper_size()' - Determine default paper size
//                             (A4/Letter) based on the location,
//                             Letter for US and Canada, A4 for the
//                             rest of the world. Use the locale
//                             environment variables ("LC_...") for
//                             that.
//

const char *pr_default_paper_size()
{
  static char result[128];            // Resulting default paper size
  char        *val;                   // Paper size/locale value
  int         i;
  const char  * const lc_env_vars[] = // Environment variables with suitable
  {                                   // locale information
    "LC_PAPER",
    "LC_CTYPE",
    "LC_ALL",
    "LANG",
  };


  memset(result, 0, sizeof(result));
  for (i = 0; i < sizeof(lc_env_vars) / sizeof(lc_env_vars[0]); i ++)
  {
    if ((val = getenv(lc_env_vars[i])) == NULL)
      continue;
    if (!strcmp(val, "C") ||
	!strcmp(val, "POSIX"))
      continue;
    if (!strcmp(val, "en") ||
	!strncmp(val, "en.", 3) ||
	!strncmp(val, "en_US", 5) ||
	!strncmp(val, "en_CA", 5) ||
	!strncmp(val, "fr_CA", 5))
      // These are the only locales that will default to "Letter" size...
      strcpy(result, "Letter");
    else
      // Rest of the world is A4
      strcpy(result, "A4");
    if (result[0])
      break;
  }
  return (result[0] ? result : NULL);
}


//
// 'pr_driver_setup()' - PostScript driver setup callback.
//
//                       Runs in two modes: Init and Update
//
//                       It runs in Init mode when the
//                       driver_data->extension is still NULL, meaning
//                       that the extension structure is not yet
//                       defined. This is the case when the printer
//                       data structure is created on startup or on
//                       adding a printer. Then we load and read the
//                       PPD and enter the properties into the driver
//                       data structure, not taking into account any
//                       user defaults or accessory settings.
//
//                       When called again with the data structure
//                       already present, it runs in Update mode,
//                       applying user defaults and modifying the data
//                       structure if the user changed the
//                       configuration of installable accessories.
//                       This mode is triggered when called by the
//                       pr_status() callback which in turn is called
//                       after completely loading the printer's state
//                       file entry or when doing changes on the
//                       "Device Settings" web interface page.
//

bool				   // O - `true` on success, `false`
                                           //     on failure
pr_driver_setup(
    pappl_system_t       *system,	   // I - System
    const char           *driver_name,     // I - Driver name
    const char           *device_uri,	   // I - Device URI
    const char           *device_id,	   // I - Device ID
    pappl_pr_driver_data_t *driver_data,   // O - Driver data
    ipp_t                **driver_attrs,   // O - Driver attributes
    void                 *data)            // I - Global data
{
  int          i, j, k, l, m;              // Looping variables
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  bool         update;                     // Are we updating the data
                                           // structure and not freshly
                                           // creating it?
  pr_driver_extension_t *extension;
  cups_array_t *ppd_paths = global_data->ppd_paths;
  pr_ppd_path_t *ppd_path,
               search_ppd_path;
  ppd_file_t   *ppd = NULL;		   // PPD file loaded from collection
  ppd_cache_t  *pc;
  cups_file_t  *tempfp;
  int          tempfd,
               bytes;
  char         tempfile[1024];
  pr_stream_format_t *stream_format;
  cups_page_header2_t header,              // CUPS raster headers to investigate
                      optheader;           // PPD with ppdRasterInterpretPPD()

  ipp_attribute_t *attr;
  int          num_inst_options = 0;
  cups_option_t *inst_options = NULL,
               *opt;
  const char   *def_source = NULL,
               *def_type = NULL;
  char         *def_bin = NULL;
  pwg_size_t   *def_media;
  int          def_left, def_right, def_top, def_bottom;
  int          res[3][2];
  char         *p, *q = NULL;
  ppd_group_t  *group;
  ppd_option_t *option;
  ppd_choice_t *choice = NULL;
  ppd_attr_t   *ppd_attr;
  pwg_map_t    *pwg_map;
  pwg_size_t   *pwg_size;
  ppd_pwg_finishings_t *finishings;
  ppd_coption_t *coption;
  ppd_cparam_t *cparam;
  int          num_cparams;
  pappl_media_col_t tmp_col;
  int          count;
  int          controlled_by_presets;
  ipp_name_lookup_t *opt_name;
  bool         pollable;
  char         buf[1024],
               ipp_opt[80],
               ipp_supported[256],
               ipp_default[256],
               ipp_choice[80],
               ipp_custom_opt[192],
               ipp_param[80];
  char         **choice_list;
  int          default_choice,
               first_choice;
  char         *ptr = NULL;
  const char * const pappl_handled_options[] =
  {
   "PageSize",
   "PageRegion",
   "InputSlot",
   "MediaType",
   "OutputBin",
   "Duplex",
   NULL
  };
  const char * const standard_ipp_names[] =
  {
   "media",
   "media-size",
   "media-source",
   "media-type",
   "printer-resolution",
   "output-bin",
   "sides",
   "color",
   "print-color-mode",
   "print-quality",
   "print-content-optimize",
   "copies",
   "finishings",
   "finishings-col",
   "job-pages-per-set",
   "orientation-requested",
   "media-col",
   "output-mode",
   "ipp-attribute-fidelity",
   "job-name",
   "page-ranges",
   "multiple-document-handling",
   "job-mandatory-attributes",
   "overrides",
   "print-rendering-intent",
   "print-scaling",
   NULL
  };


  if (!driver_data || !driver_attrs)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Driver callback called without required information.");
    return (false);
  }

  if (driver_data->extension == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Initializing driver data for driver \"%s\"", driver_name);

    if (!ppd_paths || cupsArrayCount(ppd_paths) == 0)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Driver callback did not find PPD indices.");
      return (false);
    }

    //
    // Load assigned PPD file from the PPD collection, mark defaults, create
    // cache
    //

  retry:
    if (strcasecmp(driver_name, "auto") == 0)
    {
      // Auto-select driver
      papplLog(system, PAPPL_LOGLEVEL_INFO,
	       "Automatic printer driver selection for device with URI \"%s\" "
	       "and device ID \"%s\" ...", device_uri, device_id);
      search_ppd_path.driver_name =
	(global_data->config->autoadd_cb)(NULL, device_uri, device_id,
					 global_data);
      if (search_ppd_path.driver_name)
	papplLog(system, PAPPL_LOGLEVEL_INFO,
		 "Automatically selected driver \"%s\".",
		 search_ppd_path.driver_name);
      else
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Automatic printer driver selection for printer "
		 "\"%s\" with device ID \"%s\" failed.",
		 device_uri, device_id);
	return (false);
      }
    }
    else
      search_ppd_path.driver_name = driver_name;

    ppd_path = (pr_ppd_path_t *)cupsArrayFind(ppd_paths, &search_ppd_path);

    if (ppd_path == NULL)
    {
      if (strcasecmp(driver_name, "auto") == 0)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "For the printer driver \"%s\" got auto-selected which does "
		 "not exist in this Printer Application.",
		 search_ppd_path.driver_name);
	return (false);
      }
      else
      {
	papplLog(system, PAPPL_LOGLEVEL_WARN,
		 "Printer uses driver \"%s\" which does not exist in this "
		 "Printer Application, switching to \"auto\".", driver_name);
	driver_name = "auto";
	goto retry;
      }
    }

    if ((ppd = ppdOpen2(ppdCollectionGetPPD(ppd_path->ppd_path, NULL,
					    (filter_logfunc_t)papplLog,
					    system))) == NULL)
    {
      ppd_status_t	err;		// Last error in file
      int		line;		// Line number in file

      err = ppdLastError(&line);
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "PPD %s: %s on line %d", ppd_path->ppd_path,
	       ppdErrorString(err), line);
      return (false);
    }

    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Using PPD %s: %s", ppd_path->ppd_path, ppd->nickname);

    ppdMarkDefaults(ppd);

    if ((pc = ppdCacheCreateWithPPD(ppd)) != NULL)
      ppd->cache = pc;

    for (i = 0; i < 2; i ++)
    {
      for (j = 0; j < 3; j ++)
      {
	snprintf(buf, sizeof(buf), "Presets for %s, %s:",
		 i == 1 ? "color" : "gray",
		 j == 0 ? "draft" : (j == 1 ? "normal" : "high"));
	for (k = 0; k < pc->num_presets[i][j]; k ++)
	  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=%s",
		  pc->presets[i][j][k].name, pc->presets[i][j][k].value);
	papplLog(system, PAPPL_LOGLEVEL_DEBUG, "%s", buf);
      }
    }
    for (i = 0; i < 5; i ++)
    {
      snprintf(buf, sizeof(buf), "Optimize presets %s:",
	      (i == 0 ? "automatic" :
	       (i == 1 ? "photo" :
		(i == 2 ? "graphics" :
		 (i == 3 ? "text" :
		  "text and graphics")))));
      for (k = 0; k < pc->num_optimize_presets[i]; k ++)
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=%s",
		 pc->optimize_presets[i][k].name,
		 pc->optimize_presets[i][k].value);
      papplLog(system, PAPPL_LOGLEVEL_DEBUG, "%s", buf);
    }

    //
    // Populate driver data record
    //

    // Callback functions end general properties
    driver_data->extension =
      (pr_driver_extension_t *)calloc(1, sizeof(pr_driver_extension_t));
    extension = (pr_driver_extension_t *)driver_data->extension;
    extension->ppd                  = ppd;
    extension->ipp_name_lookup      = NULL;
    extension->defaults_pollable    = false;
    extension->installable_options  = false;
    extension->installable_pollable = false;
    extension->filterless_ps        = false;
    extension->updated              = false;
    extension->temp_ppd_name        = NULL;
    extension->global_data          = global_data;
    driver_data->delete_cb          = pr_driver_delete;
    driver_data->identify_cb        = global_data->config->identify_cb;
    driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
    driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_DISPLAY |
                                      PAPPL_IDENTIFY_ACTIONS_SOUND;
    driver_data->printfile_cb       = NULL;
    driver_data->rendjob_cb         = NULL;
    driver_data->rendpage_cb        = NULL;
    driver_data->rstartjob_cb       = NULL;
    driver_data->rstartpage_cb      = NULL;
    driver_data->rwriteline_cb      = NULL;
    driver_data->status_cb          = pr_status;
    driver_data->testpage_cb        = global_data->config->testpage_cb;
    driver_data->format             = "application/vnd.printer-specific";
    driver_data->orient_default     = IPP_ORIENT_NONE;

    // Make and model
    strncpy(driver_data->make_and_model,
	    ppd->nickname,
	    sizeof(driver_data->make_and_model) - 1);

    // In case of a PPD for a PostScript printer is a filter defined
    // (in a "*cupsFilter(2): ..." line) which is not installed or no
    // filter at all (native PS PPD without "*cupsFilter(2): ..."
    // lines)?
    //
    // If we are outputting PostScript without filter we cannot accept
    // options or choices without PostScript or JCL code as these do
    // not make sense. Such options are probably for use with a filter
    // which is not installed and therefore they will not be displayed
    // in the web interface. We also will not create a physical copy
    // of the PPD file for use by CUPS filters.
    if (ppd->num_filters == 0)
      extension->filterless_ps = true;
    else
    {
      ptr = pr_ppd_find_cups_filter("application/vnd.cups-postscript",
				    ppd->num_filters, ppd->filters,
				    global_data->filter_dir);
      if (ptr && ptr[0] == '.')
	extension->filterless_ps = true;
      else
	extension->filterless_ps = false;
      free(ptr);
    }

    // Create a physical copy of the PPD file in a temporary file so that
    // the CUPS filter defined in the PPD file can read it.
    if (!extension->filterless_ps)
    {
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "CUPS filter to be applied defined in the PPD file");
      tempfp = ppdCollectionGetPPD(ppd_path->ppd_path, NULL,
				   (filter_logfunc_t)papplLog,
				   system);
      if ((tempfd = cupsTempFd(tempfile, sizeof(tempfile))) >= 0)
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Creating physical PPD file for the CUPS filter: %s",
		 tempfile);
	while ((bytes = cupsFileRead(tempfp, buf, sizeof(buf))) > 0)
	  bytes = write(tempfd, buf, bytes);
	cupsFileClose(tempfp);
	close(tempfd);
	extension->temp_ppd_name = strdup(tempfile);
      }
      else
	papplLog(system, PAPPL_LOGLEVEL_WARN,
		 "Unable to create physical PPD file for the CUPS filter, filter may not work correctly.");
    }
    else
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Sending PostScript output directly to the printer without CUPS filter");

    //
    // Find filters to use for this job
    //
  
    for (stream_format =
	   (pr_stream_format_t *)
	   cupsArrayFirst(global_data->config->stream_formats);
	 stream_format;
	 stream_format =
	   (pr_stream_format_t *)
	   cupsArrayNext(global_data->config->stream_formats))
      if ((ptr =
	   pr_ppd_find_cups_filter(stream_format->dsttype,
				   ppd->num_filters, ppd->filters,
				   global_data->filter_dir)) != NULL)
	break;

    if (stream_format == NULL || ptr == NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "No format found for printing in streaming mode");
      free(ptr);
      // We do the removal of the PPD cache separately to assure that
      // the function of libppd (and not of libcups) is used, as in
      // libppd the PPD cache data structure is different (Content
      // optimize presets added).
      ppdCacheDestroy(extension->ppd->cache);
      extension->ppd->cache = NULL;
      ppdClose(extension->ppd);
      free(extension);
      return (false);
    }

    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Converting raster input to format: %s", stream_format->dsttype);
    if (ptr[0] == '.')
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Passing on PostScript directly to printer");
    else if (ptr[0] == '-')
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Passing on %s directly to printer", stream_format->dsttype);
    else
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Using CUPS filter (printer driver): %s", ptr);

    extension->stream_filter = ptr;
    extension->stream_format = stream_format;
    driver_data->rendjob_cb    = stream_format->rendjob_cb;
    driver_data->rendpage_cb   = stream_format->rendpage_cb;
    driver_data->rstartjob_cb  = stream_format->rstartjob_cb;
    driver_data->rstartpage_cb = stream_format->rstartpage_cb;
    driver_data->rwriteline_cb = stream_format->rwriteline_cb;

    // We are in Init mode
    update = false;
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Updating driver data for %s", driver_data->make_and_model);
    extension = (pr_driver_extension_t *)driver_data->extension;
    ppd = extension->ppd;
    pc = ppd->cache;
    extension->updated = true;

    // We are in Update mode
    update = true;
  }

  // Note that we take into account option choice conflicts with the
  // configuration of installable accessories only in Update mode,
  // this way all options and choices are available after first
  // initialization (Init mode) so that all user defaults loaded from
  // the state file get accepted.
  //
  // Only at the end of the printer entry in the state file the
  // accessory configuration gets read. After that we re-run in Update
  // mode to correct the options and choices for the actual accessory
  // configuration.

  // Get settings of the "Installable Options" from the previous session
  if (*driver_attrs &&
      (attr = ippFindAttribute(*driver_attrs, "installable-options-default",
			       IPP_TAG_ZERO)) != NULL &&
      ippAttributeString(attr, buf, sizeof(buf)) > 0)
  {
    inst_options = NULL;
    num_inst_options = cupsParseOptions(buf, 0, &inst_options);
    ppdMarkOptions(ppd, num_inst_options, inst_options);
  }

  // Investigate PPD's/printer's basic properties by interpreting
  // the PostScript snippets of the default settings of the options
  ppdRasterInterpretPPD(&header, ppd, 0, NULL, NULL);

  // Resolution

  // Apply each preset to the PPD file in turn and find out which resolution
  // would get used

  memset(res, 0, sizeof(res));
  for (i = 0; i < 2; i ++)
  {
    for (j = 0; j < 3; j ++)
    {
      for (k = 0; k < 5; k ++)
      {
	if (j < 2 && k > 0)
	  // Consider resolution increases by content optimization only on
	  // high quality
	  continue;
	l = 0;
	optheader = header;
	ppdMarkDefaults(ppd);
	ppdMarkOptions(ppd, num_inst_options, inst_options);
	ppdMarkOptions(ppd, pc->num_presets[i][j], pc->presets[i][j]);
	if (k > 0)
	  ppdMarkOptions(ppd, pc->num_optimize_presets[k],
			 pc->optimize_presets[k]);
	// Check influence on the Resolution by (pseudo) PostScript code
	// assigned to the option choices
	ppdRasterInterpretPPD(&optheader, ppd, 0, NULL, NULL);
	if (optheader.HWResolution[0] != 100 || optheader.HWResolution[1] != 100)
        {
	  l = 1;
	  if (optheader.HWResolution[0] > res[j][0])
	    res[j][0] = optheader.HWResolution[0];
	  if (optheader.HWResolution[1] > res[j][1])
	    res[j][1] = optheader.HWResolution[1];
	}
	else
        {
	  // Check influence on the Resolution by JCL/PJL code
	  // assigned to the option choices
	  p = ppdEmitString(ppd, PPD_ORDER_JCL, 0);
	  if (p)
	  {
	    q = p;
	    while ((q = strstr(q, "RESOLUTION=")))
	    {
	      q += 11;
	      if ((l = sscanf(q, "%dX%d", &(res[j][0]), &(res[j][1]))) == 1)
		res[j][1] = res[j][0];
	    }
	    free(p);
	  }
	}
        if (l == 0)
	{
	  // Check choice names whether they suggest that the setting influences
	  // the resolution
	  m = pc->num_presets[i][j];
	  for (l = 0; l < m + (k > 0 ? pc->num_optimize_presets[k] : 0); l ++)
	  {
	    if (l < m)
	      q = pc->presets[i][j][l].value;
	    else if (k > 0)
	      q = pc->optimize_presets[k][l - m].value;
	    if ((p = strcasestr(q, "dpi")) != NULL)
	    {
	      if (p > q)
	      {
		p --;
		while (p > q && isspace(*p))
		  p --;
		if (p > q && isdigit(*p))
	        {
		  char x;
		  while (p > q && isdigit(*p))
		    p --;
		  if (p > q && (*p == 'x' || *p == 'X'))
		    p --;
		  while (p > q && isdigit(*p))
		    p --;
		  while (!isdigit(*p))
		    p ++;
		  if (sscanf(p, "%d%c%d",
			     &(res[j][0]), &x, &(res[j][1])) == 2)
		    res[j][1] = res[j][0];
		}
	      }
	    }
	  }
	}
      }
    }
  }
  ppdMarkDefaults(ppd);
  ppdMarkOptions(ppd, num_inst_options, inst_options);

  if (res[1][0] == 0 || res[1][1] == 0) // Normal quality resolution
  {
    // Normal quality resolution not defined by presets, find base resolution
    if (header.HWResolution[0] != 100 || header.HWResolution[1] != 100)
    {
      res[1][0] = header.HWResolution[0];
      res[1][1] = header.HWResolution[1];
    }
    else if ((ppd_attr = ppdFindAttr(ppd, "DefaultResolution", NULL)) != NULL)
    {
      // Use the PPD-defined default resolution...
      if (sscanf(ppd_attr->value, "%dx%d", &(res[1][0]), &(res[1][1])) == 1)
	res[1][1] = res[1][0];
    }
    else
      // Resort to 300 dpi
      res[1][0] = res[1][1] = 300;
  }
    
  if (res[0][0] == 0 || res[0][1] == 0) // Draft quality resolution
  {
    // Draft quality resolution not defined by presets, use normal quality
    // or base resolution
    res[0][0] = res[1][0];
    res[0][1] = res[1][1];
  }
  
  if (res[2][0] == 0 || res[2][1] == 0) // High quality resolution
  {
    // High quality resolution not defined by presets, use normal quality
    // or base resolution
    res[2][0] = res[1][0];
    res[2][1] = res[1][1];
  }

  // The resolutions here are actually only used for Apple/PWG Raster
  // and image input data. As in case of Apple/PWG Raster jobs the
  // client needs to provide the data in the given resolution, a
  // client can get easily overloaded by having extremely high
  // resolutions here. Therefore we limit these resolutions to a
  // maximum of 1440 dpi for high, 720 dpi for normal, and 360 dpi for
  // draft quality.
  //
  // The resolutions in the PPD files are the hardware resolutions of
  // the printers, which are used for high-quality dithering. For the
  // input data usually resolutions of 1440 dpi for drawings/lines and
  // 720 dpi for photos/images are good enough.
  //
  // Higher resolutions we reduce by halving them until they are below
  // the limit. This way the input resolution and the device
  // resolution stay multiples of 2 and so the pwgtoraster() filter
  // function of cups-filters is able to convert the resolution if
  // needed.
  while (res[0][0] >  360) res[0][0] /= 2;
  while (res[0][1] >  360) res[0][1] /= 2;
  while (res[1][0] >  720) res[1][0] /= 2;
  while (res[1][1] >  720) res[1][1] /= 2;
  while (res[2][0] > 1440) res[2][0] /= 2;
  while (res[2][1] > 1440) res[2][1] /= 2;

  // Default resolution (In update mode we keep the current default)
  if (!update ||
      driver_data->x_default <= 0 || driver_data->y_default <= 0)
  {
    // Default resolution is the one for normal quality
    driver_data->x_default = res[1][0];
    driver_data->y_default = res[1][1];
  }

  if (res[2][0] != res[1][0] || res[2][1] != res[1][1])
  {
    // High quality resolution differs from normal quality resolution

    // Either all three differ or draft resolution and normal resolution
    // are equal.

    // We create three resolution entries

    // If draft and normal quality have the same resolution and high quality
    // a higher one and we create only 2 resolution entries, the
    // papplJobCreatePrintOptions() function in the pappl/job-process.c
    // file takes the first entry for draft as default resolution and for
    // both normal and high quality it takes the second entry, the higher
    // resolution we observed from the high quality presets.

    // As we want the normal quality resolution as default for normal quality
    // we create three resolution entries with the first two being the lower
    // and the third being the higher resolution, as with three entries the
    // second entry is taken as default resolution for normal quality.

    for (i = 0; i < 3; i ++)
    {
      driver_data->x_resolution[i] = res[i][0];
      driver_data->y_resolution[i] = res[i][1];
    }
    driver_data->num_resolution = 3;
  }
  else if (res[0][0] != res[1][0] || res[0][1] != res[1][1])
  {
    // Draft quality resolution differs from normal quality resolution
    
    // We have a lower resolution for draft and the same resolution for both
    // normal and high quality

    // We create two resolution entries

    // In case of two resolution entries the
    // papplJobCreatePrintOptions() function in the
    // pappl/job-process.c file takes as the default resolution the
    // first for draft and the second for both normal and high
    // quality.

    for (i = 0; i < 2; i ++)
    {
      driver_data->x_resolution[i] = res[i][0];
      driver_data->y_resolution[i] = res[i][1];
    }
    driver_data->num_resolution = 2;
  }
  else
  {
    // All the three resolutions are the same
    
    // We have only a single resolution on this printer

    // We create one resolution entry

    driver_data->x_resolution[0] = res[1][0];
    driver_data->y_resolution[0] = res[1][1];
    driver_data->num_resolution = 1;
  }

  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Resolutions from presets (missing ones filled with defaults): Draft: %dx%ddpi, Normal: %dx%ddpi, High: %dx%ddpi",
	   res[0][0], res[0][1], res[1][0], res[1][1], res[2][0], res[2][1]);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Default resolution: %dx%ddpi",
	   driver_data->x_default, driver_data->y_default);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Resolution entries:");
  for (i = 0; i < driver_data->num_resolution; i ++)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  %dx%ddpi",
	     driver_data->x_resolution[i], driver_data->y_resolution[i]);
  }

  // Print speed in pages per minute (PPDs do not show different values for
  // Grayscale and Color)
  driver_data->ppm = ppd->throughput;
  if (driver_data->ppm <= 1)
    driver_data->ppm = 1;
  if (ppd->color_device)
    driver_data->ppm_color = driver_data->ppm;
  else
    driver_data->ppm_color = 0;

  // Properties not supported by the PPD
  driver_data->has_supplies = false;
  driver_data->input_face_up = false;

  // Pages face-up or face-down in output bin?
  if (pc->num_bins > 0)
    driver_data->output_face_up =
      (strstr(pc->bins->pwg, "face-up") != NULL);
  else
    driver_data->output_face_up = false;

  // No orientation requested by default
  if (!update) driver_data->orient_default = IPP_ORIENT_NONE;

  // Supported color modes
  if (ppd->color_device)
  {
    driver_data->color_supported =
      PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_COLOR |
      PAPPL_COLOR_MODE_MONOCHROME;
    if (!update) driver_data->color_default = PAPPL_COLOR_MODE_AUTO;
  }
  else
  {
    driver_data->color_supported = PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default = PAPPL_COLOR_MODE_MONOCHROME;
  }

  // These parameters are usually not defined in PPDs but standard IPP
  // options settable in the web interface
  if (!update)
  {
    driver_data->content_default = PAPPL_CONTENT_AUTO;
    driver_data->quality_default = IPP_QUALITY_NORMAL;
    driver_data->scaling_default = PAPPL_SCALING_AUTO;
  }

  // Raster graphics modes fo PWG Raster input
  if (ppd->color_device)
    driver_data->raster_types =
      PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 |
      PAPPL_PWG_RASTER_TYPE_SRGB_8;
  else
    driver_data->raster_types =
      PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
  driver_data->force_raster_type = 0;

  // Duplex
  driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  driver_data->duplex = PAPPL_DUPLEX_NONE;
  if (!update) driver_data->sides_default = PAPPL_SIDES_ONE_SIDED;
  if (pc->sides_option &&
      (option = ppdFindOption(ppd, pc->sides_option)) != NULL &&
      (!extension->filterless_ps ||
       pr_option_has_code(system, ppd, option)))
  {
    if (pc->sides_2sided_long &&
	!(update && ppdInstallableConflict(ppd, pc->sides_option,
					   pc->sides_2sided_long)))
    {
      driver_data->sides_supported |= PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
      driver_data->duplex = PAPPL_DUPLEX_NORMAL;
      if (!update &&
	  (choice = ppdFindMarkedChoice(ppd, pc->sides_option)) != NULL &&
	  strcmp(choice->choice, pc->sides_2sided_long) == 0)
	driver_data->sides_default = PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
    }
    if (pc->sides_2sided_short &&
	!(update && ppdInstallableConflict(ppd, pc->sides_option,
					   pc->sides_2sided_short)))
    {
      driver_data->sides_supported |= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
      driver_data->duplex = PAPPL_DUPLEX_NORMAL;
      if (!update &&
	  (choice = ppdFindMarkedChoice(ppd, pc->sides_option)) != NULL &&
	  strcmp(choice->choice, pc->sides_2sided_short) == 0)
	driver_data->sides_default = PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    }
    if (driver_data->duplex == PAPPL_DUPLEX_NORMAL)
    {
      ppd_attr = ppdFindAttr(ppd, "cupsBackSide", NULL);
      if (ppd_attr != NULL && ppd_attr->value != NULL)
	ptr = ppd_attr->value;
      else if (ppd->flip_duplex)
	ptr = "Rotated";
      else
	ptr = NULL;
      if (ptr)
      {
	if (strcasecmp(ptr, "ManualTumble") == 0)
	  driver_data->duplex = PAPPL_DUPLEX_MANUAL_TUMBLE;
	else if (strcasecmp(ptr, "Rotated") == 0)
	  driver_data->duplex = PAPPL_DUPLEX_ROTATED;
	else if (strcasecmp(ptr, "Flipped") == 0)
	  driver_data->duplex = PAPPL_DUPLEX_FLIPPED;
      }
    }
  }
  if ((driver_data->sides_default & driver_data->sides_supported) == 0)
  {
    driver_data->sides_default = PAPPL_SIDES_ONE_SIDED;
    if (pc->sides_option)
      ppdMarkOption(ppd, pc->sides_option, pc->sides_1sided);
  }

  // Finishings
  driver_data->finishings = PAPPL_FINISHINGS_NONE;
  for (finishings = (ppd_pwg_finishings_t *)cupsArrayFirst(pc->finishings);
       finishings;
       finishings = (ppd_pwg_finishings_t *)cupsArrayNext(pc->finishings))
  {
    for (i = finishings->num_options, opt = finishings->options; i > 0;
	 i --, opt ++)
    {
      if (update && ppdInstallableConflict(ppd, opt->name, opt->value))
	break;
      if ((option = ppdFindOption(ppd, opt->name)) == NULL ||
	  (extension->filterless_ps &&
	   !pr_option_has_code(system, ppd, option)))
	break;
    }
    if (i > 0)
      continue;
    if (finishings->value == IPP_FINISHINGS_STAPLE)
      driver_data->finishings |= PAPPL_FINISHINGS_STAPLE;
    else  if (finishings->value == IPP_FINISHINGS_PUNCH)
      driver_data->finishings |= PAPPL_FINISHINGS_PUNCH;
    else if (finishings->value == IPP_FINISHINGS_TRIM)
      driver_data->finishings |= PAPPL_FINISHINGS_TRIM;
  }

  // For the options Media Source and Media Type we do not need to
  // assure that both have PostScript/PJL code to tell the printer
  // which choices are selected, as they depend on each other, by
  // which media is loaded in which tray.
  //
  // If Media Source has PostScript/PJL code and Media Type not, the
  // code of Media Source selects the tray with whatever is loaded in
  // it. The Media Type selection in this Printer Application only
  // serves as a reminder what we have loaded then.
  //
  // If Media Type has PostScript/PJL code and Media Source not, the
  // printer, guided by the code of Media Size and Media Type searches
  // for the tray which carries this combo, so our codeless Media
  // Source option only serves for easy selection in this Printer
  // Application.
  //
  // Note that Media Size is required to have PostScript/PJL code

  // Media source
  if ((count = pc->num_sources) > 0)
  {
    if (!update)
      choice = ppdFindMarkedChoice(ppd, pc->source_option);
    else
      for (i = 0; i < driver_data->num_source; i ++)
	free((char *)(driver_data->source[i]));
    def_source = NULL;
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Media source entries:");
    for (i = 0, j = 0, pwg_map = pc->sources;
	 i < count && j < PAPPL_MAX_SOURCE;
	 i ++, pwg_map ++)
      if (!(update &&
	    ppdInstallableConflict(ppd, pc->source_option, pwg_map->ppd)))
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "  PPD: %s PWG: %s", pwg_map->ppd, pwg_map->pwg);
	if (!pwg_map->pwg || !pwg_map->pwg[0])
	{
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "    -> Skipping source with undefined PWG name");
	  continue;
	}
	for (k = 0; k < j; k++)
	  if (strcmp(driver_data->source[k], pwg_map->pwg) == 0)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "    -> Skipping duplicate source");
	    break;
	  }
	if (k < j)
	  continue;
	driver_data->source[j] = strdup(pwg_map->pwg);
	if (j == 0 ||
	    (!update && choice && !strcmp(pwg_map->ppd, choice->choice)) ||
	    (update &&
	     !strcmp(pwg_map->pwg, driver_data->media_default.source)))
	{
	  def_source = driver_data->source[j];
	  ppdMarkOption(ppd, pc->source_option, pwg_map->ppd);
	}
	j ++;
      }
    driver_data->num_source = j;
  }
  if (count == 0 || driver_data->num_source == 0)
  {
    driver_data->num_source = 1;
    driver_data->source[0] = strdup("default");
    def_source = driver_data->source[0];
  }

  // Media type
  if ((count = pc->num_types) > 0)
  {
    if (!update)
      choice = ppdFindMarkedChoice(ppd, "MediaType");
    else
      for (i = 0; i < driver_data->num_type; i ++)
	free((char *)(driver_data->type[i]));
    def_type = NULL;
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Media type entries:");
    for (i = 0, j = 0, pwg_map = pc->types;
	 i < count && j < PAPPL_MAX_TYPE;
	 i ++, pwg_map ++)
      if (!(update && ppdInstallableConflict(ppd, "MediaType", pwg_map->ppd)))
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "  PPD: %s PWG: %s", pwg_map->ppd, pwg_map->pwg);
	if (!pwg_map->pwg || !pwg_map->pwg[0])
	{
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "    -> Skipping type with undefined PWG name");
	  continue;
	}
	for (k = 0; k < j; k++)
	  if (strcmp(driver_data->type[k], pwg_map->pwg) == 0)
	  {
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "    -> Skipping duplicate type");
	    break;
	  }
	if (k < j)
	  continue;
	driver_data->type[j] = strdup(pwg_map->pwg);
	if (j == 0 ||
	    (!update && choice && !strcmp(pwg_map->ppd, choice->choice)) ||
	    (update &&
	     !strcmp(pwg_map->pwg, driver_data->media_default.type)))
	{
	  def_type = driver_data->type[j];
	  ppdMarkOption(ppd, "MediaType", pwg_map->ppd);
	}
	j ++;
      }
    driver_data->num_type = j;
  }
  if (count == 0 || driver_data->num_type == 0)
  {
    driver_data->num_type = 1;
    driver_data->type[0] = strdup("auto");
    def_type = driver_data->type[0];
  }

  // Media size, margins
  if ((option = ppdFindOption(ppd, "PageSize")) == NULL ||
      (extension->filterless_ps &&
       !pr_option_has_code(system, ppd, option)))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "PPD does not have a \"PageSize\" option or the option is "
	     "missing PostScript/PJL code for selecting the page size.");
    pr_driver_delete(NULL, driver_data);
    if (num_inst_options)
      cupsFreeOptions(num_inst_options, inst_options);
    return (false);
  }
  def_left = def_right = def_top = def_bottom = 9999999;
  driver_data->borderless = false;
  count = pc->num_sizes;
  if (!update)
  {
    // If we can determine a default page size (Letter/A4) depending
    // on the user's location via pr_default_paper_size() and there is
    // either no default page size set or the default page size is A4
    // or Letter, we correct the default to the page size of the
    // user's location, but only if it is actually available in the
    // PPD. Otherwise we take the default page size from the PPD file.
    // Most PPDs have Letter as default but most places on the world
    // use A4, so this switches the deafult to A4 in most cases.  This
    // affects only new print queues or newly added media sources.
    const char *val;
    if ((val = pr_default_paper_size()) == NULL ||
	(option = ppdFindOption(ppd, "PageSize")) == NULL ||
	((choice = ppdFindMarkedChoice(ppd, "PageSize")) != NULL &&
	 strcasecmp(choice->choice, "Letter") &&
	 strcasecmp(choice->choice, "A4")) ||
	(choice = ppdFindChoice(option, val)) == NULL)
      choice = ppdFindMarkedChoice(ppd, "PageSize");
  }
  else
    for (i = 0; i < driver_data->num_media; i ++)
      free((char *)(driver_data->media[i]));
  def_media = NULL;
  j = 0;

  // Custom page size (if defined in PPD)
  if (pc->custom_min_keyword && pc->custom_max_keyword &&
      pc->custom_max_width > pc->custom_min_width &&
      pc->custom_max_length > pc->custom_min_length)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Adding custom page size:");
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  PWG keyword min dimensions: \"%s\"", pc->custom_min_keyword);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  PWG keyword max dimensions: \"%s\"", pc->custom_max_keyword);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  Minimum dimensions (width, length): %dx%d",
	     pc->custom_min_width, pc->custom_min_length);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  Maximum dimensions (width, length): %dx%d",
	     pc->custom_max_width, pc->custom_max_length);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  Margins (left, bottom, right, top): %d, %d, %d, %d",
	     pc->custom_size.left, pc->custom_size.bottom,
	     pc->custom_size.right, pc->custom_size.top);
    driver_data->media[j] = strdup(pc->custom_max_keyword);
    j ++;
    driver_data->media[j] = strdup(pc->custom_min_keyword);
    j ++;
  }

  // Standard page sizes
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Media size entries:");
  for (i = 0, pwg_size = pc->sizes;
       i < count && j < PAPPL_MAX_MEDIA;
       i ++, pwg_size ++)
    if (!(update && ppdInstallableConflict(ppd, "PageSize", pwg_size->map.ppd)))
    {
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "  PPD: %s PWG: %s", pwg_size->map.ppd, pwg_size->map.pwg);
      if (!pwg_size->map.pwg || !pwg_size->map.pwg[0])
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "    -> Skipping size with undefined PWG name");
	continue;
      }
      for (k = 0; k < j; k++)
	if (strcmp(driver_data->media[k], pwg_size->map.pwg) == 0)
	{
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "    -> Skipping duplicate size");
	  break;
	}
      if (k < j)
	continue;
      if (!strchr(pwg_size->map.ppd, '.') &&
	  (j == 0 ||
	   (!update && choice && !strcmp(pwg_size->map.ppd, choice->choice)) ||
	   (update &&
	    !strcmp(pwg_size->map.pwg, driver_data->media_default.size_name))))
      {
	def_media = pwg_size;
	ppdMarkOption(ppd, "PageSize", pwg_size->map.ppd);
      }
      if (pwg_size->left == 0 && pwg_size->right == 0 &&
	  pwg_size->top == 0 && pwg_size->bottom == 0)
	driver_data->borderless = true;
      else
      {
	if (pwg_size->left < def_left)
	  def_left = pwg_size->left;
	if (pwg_size->right < def_right)
	  def_right = pwg_size->right;
	if (pwg_size->top < def_top)
	  def_top = pwg_size->top;
	if (pwg_size->bottom < def_bottom)
	  def_bottom = pwg_size->bottom;
      }

      // Skip variants ("A4.Borderless" is variant of "A4") of page
      // sizes from the list to avoid clutter
      //
      // We can expect all variants being for paper of the same
      // physical size dimensions and PAPPL only considers the size
      // dimensions for page sizes.
      //
      // We only skip the variants here, after having looked up their
      // margins, to find the actually narrowest margins for the
      // printer and especially variants with zero margins indicating
      // that we have borderless printing support.
      // 
      // When a job is executed, the size/margins are looked up in the
      // PPD again by the pr_create_job_data() and this way the best
      // fitting size, including variants selected for the job.
      if (strchr(pwg_size->map.ppd, '.'))
	continue;

      // Add size to list
      driver_data->media[j] =
	strdup(pwg_size->map.pwg);
      j ++;
    }

  // Number of media entries (Note that custom page size uses 2 entries,
  // one holding the minimum, one the maximum dimensions)
  driver_data->num_media = j;

  // If margin info missing in the page size entries, use "HWMargins"
  // line of the PPD file, otherwise zero
  if (def_left >= 9999999)
    def_left = (ppd->custom_margins[0] ?
		(int)(ppd->custom_margins[0] / 72.0 * 2540.0) : 0);
  if (def_bottom >= 9999999)
    def_bottom = (ppd->custom_margins[1] ?
		  (int)(ppd->custom_margins[1] / 72.0 * 2540.0) : 0);
  if (def_right >= 9999999)
    def_right = (ppd->custom_margins[2] ?
		 (int)(ppd->custom_margins[2] / 72.0 * 2540.0) : 0);
  if (def_top >= 9999999)
    def_top = (ppd->custom_margins[3] ?
	       (int)(ppd->custom_margins[3] / 72.0 * 2540.0) : 0);

  // Set margin info
  driver_data->left_right = (def_left < def_right ? def_left : def_right);
  driver_data->bottom_top = (def_bottom < def_top ? def_bottom : def_top);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Margins: Left/Right: %d, Bottom/Top: %d",
	   driver_data->left_right, driver_data->bottom_top);
  if (driver_data->left_right == 0 && driver_data->bottom_top == 0)
    // Remove "Borderless" switch again as the paper sizes are
    // borderless anyway
    driver_data->borderless = false;

  // Set default for media
  if (def_media)
  {
    pr_media_col(def_media, def_source, def_type, 0, 0, 0,
		 &(driver_data->media_default));
    // We use the general margins of the driver data here and not the
    // individual, page-size-specific margins of the PPD, as PAPPL
    // does not allow registering individual margins per-page-size
    // with the available page sizes. To not have the Printer
    // Application behave different from being started until the first
    // page size change we change to the general margins also for the
    // default media entry.
    driver_data->media_default.left_margin = driver_data->left_right;
    driver_data->media_default.right_margin = driver_data->left_right;
    driver_data->media_default.top_margin = driver_data->bottom_top;
    driver_data->media_default.bottom_margin = driver_data->bottom_top;
  }

  // "media-ready" not defined in PPDs, also cannot be polled from printer
  // The user configures in the web interface what is loaded.
  //
  // The web interface shows only the input trays which are actually
  // installed on the printer, according to the configuration of
  // installable accessories on the "Device Settings" page.
  //
  // If the user accidentally removes a tray on the "Device Settings" page
  // and re-adds it while the Printer Application is still running, the
  // loaded media configuration gets restored.
  if (update)
  {
    for (i = 0, j = 0, pwg_map = pc->sources;
	 i < pc->num_sources && j < PAPPL_MAX_SOURCE;
	 i ++, pwg_map ++)
    {
      tmp_col.source[0] = '\0';
      // Go through all media sources of the PPD file, to keep the order
      if (!strcasecmp(pwg_map->pwg, driver_data->source[j]))
      {
	// Current PPD media source is available (installed)
	if (strcasecmp(pwg_map->pwg, driver_data->media_ready[j].source))
	{
	  // There is no media-col-ready item for the current media source,
	  // so first check whether we have it in the hidden "Undo" space
	  // beyond the actually used media items (it should be there when
	  // we had already set the source as installed earlier during this
	  // session of the Printer Application
	  for (k = j;
	       k < PAPPL_MAX_SOURCE && driver_data->media_ready[k].source[0] &&
		 strcasecmp(pwg_map->pwg, driver_data->media_ready[k].source);
	       k ++);
	  if (!strcasecmp(pwg_map->pwg, driver_data->media_ready[k].source))
	    // Found desired item in hidden "Undo" space beyond the actually
	    // used media-col-ready items
	    memcpy(&tmp_col, &(driver_data->media_ready[k]),
		   sizeof(pappl_media_col_t));
	  else if (k == PAPPL_MAX_SOURCE)
	    k --; // Do not push beyond the memory
	  else if (k < PAPPL_MAX_SOURCE - 1)
	    k ++; // Push up also the terminating zero item
	  // Move up the other items to make space for the new item
	  memmove(&(driver_data->media_ready[j + 1]),
		  &(driver_data->media_ready[j]),
		  (k - j) * sizeof(pappl_media_col_t));
	  if (tmp_col.source[0])
	    // Insert item from "Undo" space
	    memcpy(&(driver_data->media_ready[j]), &tmp_col,
		   sizeof(pappl_media_col_t));
	  else
	  {
	    // Create new item, as this was not in "Undo" space
	    memcpy(&(driver_data->media_ready[j]),
		   &(driver_data->media_default),
		   sizeof(pappl_media_col_t));
	    strncpy(driver_data->media_ready[j].source, driver_data->source[j],
		    sizeof(driver_data->media_ready[j].source) - 1);
	  }
	}

	// Check margins of the medis-ready entry as the PPD file can have
	// been changed and so data loaded from the state file can have wrong
	// margins
	if (!(driver_data->borderless &&
	      driver_data->media_ready[j].left_margin == 0 &&
	      driver_data->media_ready[j].right_margin == 0 &&
	      driver_data->media_ready[j].top_margin == 0 &&
	      driver_data->media_ready[j].bottom_margin == 0))
	{
	  driver_data->media_ready[j].left_margin = driver_data->left_right;
	  driver_data->media_ready[j].right_margin = driver_data->left_right;
	  driver_data->media_ready[j].top_margin = driver_data->bottom_top;
	  driver_data->media_ready[j].bottom_margin =driver_data->bottom_top;
	}

	// Check media size (name?) of the media-ready entry as the
	// PPD file can have been changed and so data loaded from the
	// state file can have a media size not available in this PPD
	for (k = 0; k < driver_data->num_media; k ++)
	  if (!strcasecmp(driver_data->media_ready[j].size_name,
			  driver_data->media[k]))
	    break;
	if (k == driver_data->num_media)
	  strncpy(driver_data->media_ready[j].size_name,
		  driver_data->media_default.size_name,
		  sizeof(driver_data->media_ready[j].size_name));

	// Check media type of the media-ready entry as the
	// PPD file can have been changed and so data loaded from the
	// state file can have a media size not available in this PPD
	for (k = 0; k < driver_data->num_type; k ++)
	  if (!strcasecmp(driver_data->media_ready[j].type,
			  driver_data->type[k]))
	    break;
	if (k == driver_data->num_type)
	  strncpy(driver_data->media_ready[j].type,
		  driver_data->media_default.type,
		  sizeof(driver_data->media_ready[j].type));

	// Did we now create the media-ready entry for the media source
	// which is the default? Then copy its content into the default media
	if (!strcasecmp(driver_data->media_ready[j].source,
			driver_data->media_default.source))
	  memcpy(&(driver_data->media_default), &(driver_data->media_ready[j]),
		 sizeof(pappl_media_col_t));

	// Go on with next media source
	j ++;
      }
      else
      {
	// Current PPD media source is unavailable (accessory not installed)
	if (!strcasecmp(pwg_map->pwg, driver_data->media_ready[j].source))
	{
	  // Current media-col-ready item is the unavailable media source,
	  // so move current media-col-ready away into the "Undo" space beyond
	  // the actually used media-col-ready items, so its configuration
	  // stays saved case we have removed the tray in the installable
	  // accessories by accident
	  memcpy(&tmp_col, &(driver_data->media_ready[j]),
		 sizeof(pappl_media_col_t));
	  // Pull down the rest
	  for (k = j + 1;
	       k < PAPPL_MAX_SOURCE && driver_data->media_ready[k].source[0];
	       k ++);
	  memmove(&(driver_data->media_ready[j]),
		  &(driver_data->media_ready[j + 1]),
		  (k - j - 1) * sizeof(pappl_media_col_t));
	  // Drop the saved item into the freed slot in the "Undo" space
	  memcpy(&(driver_data->media_ready[k - 1]), &tmp_col,
		 sizeof(pappl_media_col_t));
	}
      }
    }
    // If there is no InputSlot (media source) option in the PPD file,
    // use the single medis-ready entry of the initial setup or loaded
    // from the state file
    if (j == 0)
      j = 1;
    // During initial loading of the state file add a terminating zero
    // item to manage the "Undo" space when configuring available
    // media trays on the printer
    if (!papplSystemIsRunning(system) && j < PAPPL_MAX_SOURCE)
      driver_data->media_ready[j].source[0] = '\0';
  }
  else
  {
    // Create media-col-ready items for each media source
    for (i = 0; i < driver_data->num_source; i ++)
    {
      memcpy(&(driver_data->media_ready[i]), &(driver_data->media_default),
	     sizeof(pappl_media_col_t));
      strncpy(driver_data->media_ready[i].source, driver_data->source[i],
	      sizeof(driver_data->media_ready[i].source) - 1);
    }
    // Add a terminating zero item to manage the "Undo" space when configuring
    // available media trays on the printer
    if (i < PAPPL_MAX_SOURCE)
      driver_data->media_ready[i].source[0] = '\0';
  }

  // Log "media-ready" entries
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Entries for \"media-ready\" (numbers are 1/100 mm):");
  for (i = 0;
       i < PAPPL_MAX_SOURCE && driver_data->media_ready[i].source[0];
       i ++)
  {
    if (i == driver_data->num_source)
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Undo buffer for \"media-ready\":");
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "  %s: %s, %s%s, L=%d, B=%d, R=%d, T=%d",
	     driver_data->media_ready[i].source,
	     driver_data->media_ready[i].size_name,
	     driver_data->media_ready[i].type,
	     (driver_data->media_ready[i].bottom_margin ||
	      driver_data->media_ready[i].left_margin ||
	      driver_data->media_ready[i].right_margin ||
	      driver_data->media_ready[i].top_margin ? "" : ", Borderless"),
	     driver_data->media_ready[i].left_margin,
	     driver_data->media_ready[i].bottom_margin,
	     driver_data->media_ready[i].right_margin,
	     driver_data->media_ready[i].top_margin);
  }

  // Offsets not defined in PPDs
  driver_data->left_offset_supported[0] = 0;
  driver_data->left_offset_supported[1] = 0;
  driver_data->top_offset_supported[0] = 0;
  driver_data->top_offset_supported[1] = 0;

  // Media tracking not defined in PPDs
  driver_data->tracking_supported = 0;

  // Output bins
  if ((count = pc->num_bins) > 0 &&
      (option = ppdFindOption(ppd, "OutputBin")) != NULL &&
      (!extension->filterless_ps ||
       pr_option_has_code(system, ppd, option)))
  {
    if (!update)
      choice = ppdFindMarkedChoice(ppd, "OutputBin");
    else
    {
      def_bin = strdup(driver_data->bin[driver_data->bin_default]);
      for (i = 0; i < driver_data->num_bin; i ++)
	free((char *)(driver_data->bin[i]));
    }
    driver_data->bin_default = 0;
    for (i = 0, j = 0, pwg_map = pc->bins;
	 i < count && j < PAPPL_MAX_BIN;
	 i ++, pwg_map ++)
      if (!(update && ppdInstallableConflict(ppd, "OutputBin", pwg_map->ppd)))
      {
	driver_data->bin[j] = strdup(pwg_map->pwg);
	if ((!update && choice && !strcmp(pwg_map->ppd, choice->choice)) ||
	    (update && !strcmp(pwg_map->pwg, def_bin)))
	{
	  driver_data->bin_default = j;
	  ppdMarkOption(ppd, "OutputBin", pwg_map->ppd);
	}
	j ++;
      }
    driver_data->num_bin = j;
    if (update)
      free(def_bin);
  }
  else
  {
    driver_data->num_bin = 0;
    driver_data->bin_default = 0;
  }

  // Properties not defined in PPDs
  driver_data->mode_configured = 0;
  driver_data->mode_supported = 0;
  driver_data->tear_offset_configured = 0;
  driver_data->tear_offset_supported[0] = 0;
  driver_data->tear_offset_supported[1] = 0;
  driver_data->speed_supported[0] = 0;
  driver_data->speed_supported[1] = 0;
  driver_data->speed_default = 0;
  driver_data->darkness_default = 0;
  driver_data->darkness_configured = 0;
  driver_data->darkness_supported = 0;
  driver_data->num_features = 0;

  // For each PPD option which is not supported by PAPPL/IPP add a
  // vendor option, so that the default for the options can get set in
  // the web interface or settings of these options can be supplied on
  // the command line.

  // Clean up old option lists on update
  if (update)
    for (i = 0; i < driver_data->num_vendor; i ++)
    {
      free((char *)(driver_data->vendor[i]));
      if (extension->vendor_ppd_options[i])
	free((char *)(extension->vendor_ppd_options[i]));
    }

  // Go through all the options of the PPD file
  driver_data->num_vendor = 0;
  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {
    for (j = group->num_options, option = group->options;
         j > 0;
         j --, option ++)
    {
      // Does the option allow custom values?
      num_cparams = 0;
      if ((coption = ppdFindCustomOption(ppd, option->keyword)) != NULL)
	num_cparams = cupsArrayCount(coption->params);

      // Does the option have less than 2 choices and also does not
      // allow custom values? Then it does not make sense to let it
      // show in the web interface
      if (option->num_choices < 2 && num_cparams == 0)
	continue;

      // Can printer's default setting of this option be polled from the
      // printer?
      pollable = false;
      if (global_data->config->components & PR_COPTIONS_QUERY_PS_DEFAULTS)
      {
	snprintf(buf, sizeof(buf), "?%s", option->keyword);
	if ((ppd_attr = ppdFindAttr(ppd, buf, NULL)) != NULL &&
	    ppd_attr->value)
	{
	  pollable = true;
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "Default of option \"%s\" (\"%s\") can get queried from "
		   "printer.", option->keyword, option->text);
	}
      }

      // Skip the group for installable options here, as they should not
      // show on the "Printing Defaults" page nor be listed in the response
      // to a get-printer-atrributes IPP request from a client.
      // Only note the fact that we have such options in the PPD file
      if (strncasecmp(group->name, "Installable", 11) == 0)
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Installable accessory option: \"%s\" (\"%s\")",
		 option->keyword, option->text);
	extension->installable_options = true;
	if (pollable)
	  extension->installable_pollable = true;
	continue;
      }

      // Do we have a pollable option? Mark that we have one so that
      // we can show an appropriate poll button in the web interface
      if (pollable)
	extension->defaults_pollable = true;

      // Is this option already handled by PAPPL/IPP
      for (k = 0; pappl_handled_options[k]; k ++)
	if (!strcasecmp(option->keyword, pappl_handled_options[k]))
	  break;
      if (pappl_handled_options[k] ||
	  (pc->source_option &&
	   !strcasecmp(option->keyword, pc->source_option)) ||
	  (pc->sides_option &&
	   !strcasecmp(option->keyword, pc->sides_option)))
	continue;

      // Check whether there is not more than one at least all but one choice
      // without active PostScript or PJL code to inject into the job stream
      if (extension->filterless_ps &&
	  !pr_option_has_code(system, ppd, option))
	continue;

      // Stop and warn if we have no slots for vendor attributes any more
      // Note that we reserve one slot for saving the "Installable Options"
      // in the state file
      // We also take into account here that each custom parameter for this
      // option requires one additional vendor option
      if (driver_data->num_vendor >= PAPPL_MAX_VENDOR - 1 - num_cparams)
      {
	papplLog(system, PAPPL_LOGLEVEL_WARN,
		 "Too many options in PPD file, \"%s\" (\"%s\") will not be controllable!",
		 option->keyword, option->text);
	continue;
      }

      // Find an IPP-style option name

      // Check look-up table to see whether we already have an IPP name for
      // this PPD option
      opt_name = NULL;
      if (extension->ipp_name_lookup == NULL)
	extension->ipp_name_lookup = cupsArrayNew(NULL, NULL);
      else
	for (opt_name =
	       (ipp_name_lookup_t *)cupsArrayFirst(extension->ipp_name_lookup);
	     opt_name;
	     opt_name =
	       (ipp_name_lookup_t *)cupsArrayNext(extension->ipp_name_lookup))
	  if (strcmp(option->keyword, opt_name->ppd) == 0)
	    break;

      if (opt_name == NULL)
      {
	// No IPP name assigned yet to this option, generate one
	for (k = 0; k < 4; k ++)
	{
	  // Try different approaches to find a not-yet-taken IPP name
	  if (k == 0)
	    // Base it on human-readable option name
	    ppdPwgUnppdizeName(option->text, ipp_opt, sizeof(ipp_opt), NULL);
	  else if (k == 2)
	    // Base it on machine-readable option name
	    ppdPwgUnppdizeName(option->keyword, ipp_opt, sizeof(ipp_opt), NULL);
	  else if (k == 1 || k == 3)
	  {
	    // Remove common prefixes from previously tried name
	    //
	    // We could use memmove() here but this give Valgrind
	    // messages about memcpy() use with overlapping memory
	    // reasons. Probably a bug somewhere in the libraries or
	    // in Valgrind a memmove() is made for overlapping memory
	    // regions.
	    if (strncmp(ipp_opt, "print-", 6) == 0)
	    {
	      for (p = ipp_opt, q = ipp_opt + 6; *q; p ++, q ++)
		*p = *q;
	      *p = '\0';
	    }
	    else if (strncmp(ipp_opt, "printer-", 8) == 0)
	    {
	      for (p = ipp_opt, q = ipp_opt + 8; *q; p ++, q ++)
		*p = *q;
	      *p = '\0';
	    }
	    else
	      // No prefix to remove
	      continue;
	  }

	  // Is this IPP attribute name a common IPP name?
	  for (l = 0; standard_ipp_names[l]; l ++)
	    if (!strcasecmp(ipp_opt, standard_ipp_names[l]))
	      break;
	  if (standard_ipp_names[l])
	    // Standard name, not available for us
	    continue;

	  // Look up IPP name in look-up table
	  for (opt_name =
	        (ipp_name_lookup_t *)cupsArrayFirst(extension->ipp_name_lookup);
	       opt_name;
	       opt_name =
		 (ipp_name_lookup_t *)cupsArrayNext(extension->ipp_name_lookup))
	    if (strcmp(ipp_opt, opt_name->ipp) == 0)
	      break;
	  if (opt_name)
	    // Already exists
	    continue;

	  // Suitable IPP attribute name found
	  break;
	}

	if (k == 4)
	{
	  // All attempts to find an IPP attribute name have failed
	  papplLog(system, PAPPL_LOGLEVEL_WARN,
		   "Now suitable IPP attribute name found for PPD option \"%s\" (\"%s\")",
		   option->keyword, option->text);
	  continue;
	}

	// Add the new name assignment to the look-up table
	opt_name = (ipp_name_lookup_t *)calloc(1, sizeof(ipp_name_lookup_t));
	opt_name->ppd = option->keyword;
	opt_name->ipp = strdup(ipp_opt);
	cupsArrayAdd(extension->ipp_name_lookup, opt_name);
      }
      else
	// We have already an IPP name, take it from the look-up table
	strncpy(ipp_opt, opt_name->ipp, sizeof(ipp_opt) - 1);

      // IPP attribute names for available values and default value
      snprintf(ipp_supported, sizeof(ipp_supported), "%s-supported", ipp_opt);
      snprintf(ipp_default, sizeof(ipp_default), "%s-default", ipp_opt);

      // Check if this option is also controlled by the presets. If so,
      // add an extra "automatic-selection" choice and make this the default
      // This extra choice makes the preset being used for this option
      controlled_by_presets = 0;
      for (k = 0; k < 2; k ++)
	for (l = 0; l < 3; l ++)
	  for (m = 0; m < pc->num_presets[k][l]; m ++)
	    if (strcmp(option->keyword, pc->presets[k][l][m].name) == 0)
	      controlled_by_presets = 1;
      if (controlled_by_presets == 0)
	for (k = 0; k < 5; k ++)
	  for (l = 0; l < pc->num_optimize_presets[k]; l ++)
	    if (strcmp(option->keyword, pc->optimize_presets[k][l].name) == 0)
	      controlled_by_presets = 1;

      // Add vendor option and its choices to driver IPP attributes
      if (option->ui == PPD_UI_PICKONE || option->ui == PPD_UI_BOOLEAN)
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Adding vendor-specific option \"%s\" (\"%s\") as IPP option "
		 "\"%s\"", option->keyword, option->text, ipp_opt);
	if (*driver_attrs == NULL)
	  *driver_attrs = ippNew();
	if (option->num_choices == 2 && controlled_by_presets == 0 &&
	    ((!strcasecmp(option->choices[0].text, "true") &&
	      !strcasecmp(option->choices[1].text, "false")) ||
	     (!strcasecmp(option->choices[0].text, "false") &&
	      !strcasecmp(option->choices[1].text, "true"))))
	{
	  // Create a boolean IPP option, as human-readable choices "true"
	  // and "false" are not very user-friendly

	  // On update, remove IPP attributes, keep default
	  default_choice = 0;
	  if (update)
          {
	    ippDeleteAttribute(*driver_attrs,
			       ippFindAttribute(*driver_attrs, ipp_supported,
						IPP_TAG_ZERO));
	    attr = ippFindAttribute(*driver_attrs, ipp_default, IPP_TAG_ZERO);
	    if (attr)
	    {
	      default_choice = ippGetBoolean(attr, 0);
	      ippDeleteAttribute(*driver_attrs, attr);
	    }
	    else
	      default_choice = 0;
	    if (ppdInstallableConflict(ppd, option->keyword,
				       option->choices[0].choice))
	      default_choice = -1;
	    if (ppdInstallableConflict(ppd, option->keyword,
				       option->choices[1].choice))
	    {
	      if (default_choice >= 0)
		ppdMarkOption(ppd, option->keyword, option->choices[0].choice);
	      default_choice = -1;
	    }
	    else if (default_choice < 0)
	      ppdMarkOption(ppd, option->keyword, option->choices[1].choice);
	    if (default_choice < 0)
	    {
	      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		       "  -> Skipping - Boolean option does not make sense with current accessory configuration");
	      continue;
	    }
	  }
	  else
	  {
	    default_choice = 0;
	    for (k = 0; k < 2; k ++)
	      if (option->choices[k].marked &&
		  !strcasecmp(option->choices[k].text, "true"))
		default_choice = 1;
	  }
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "  Default: %s", (default_choice ? "true" : "false"));
	  ippAddBoolean(*driver_attrs, IPP_TAG_PRINTER, ipp_supported, 1);
	  ippAddBoolean(*driver_attrs, IPP_TAG_PRINTER, ipp_default,
			default_choice);
	}
	else
	{
	  // Create an enumerated-choice IPP option

	  // On update, remove IPP attributes, keep default
	  if (update)
          {
	    ippDeleteAttribute(*driver_attrs,
			       ippFindAttribute(*driver_attrs, ipp_supported,
						IPP_TAG_ZERO));
	    attr = ippFindAttribute(*driver_attrs, ipp_default, IPP_TAG_ZERO);
	    if (attr)
	    {
	      ippAttributeString(attr, buf, sizeof(buf));
	      ippDeleteAttribute(*driver_attrs, attr);
	    }
	    else
	      buf[0] = '\0';
	  }
	  choice_list =
	    (char **)calloc(option->num_choices + controlled_by_presets,
			    sizeof(char *));
	  first_choice = -2;
	  default_choice = -1;
	  l = 0;
	  if (controlled_by_presets == 1)
	  {
	    // Add "automatic-selection" choice as first one, for the presets
	    // being used for this option
	    strncpy(ipp_choice, "automatic-selection", sizeof(ipp_choice) - 1);
	    choice_list[0] = strdup(ipp_choice);
	    if (first_choice == -2)
	      first_choice = -1;
	    if ((!update) ||
		(update && buf[0] && !strcasecmp(choice_list[0], buf)))
	      default_choice = 0;
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "  Adding choice for auto-selection from presets as \"%s\"%s",
		     ipp_choice,
		     default_choice == 0 ? " (default)" : "");
	    l ++;
	  }
	  for (k = 0; k < option->num_choices; k ++)
	    if (!(update && ppdInstallableConflict(ppd, option->keyword,
						   option->choices[k].choice)))
	    {
	      // If we have custom parameters (we accept a custom value)
	      // the last choice of this option is "Custom". Only accept
	      // this choice if PAPPL supports all the parameters (only
	      // strings and integer numbers are supported)
	      if (k == option->num_choices - 1 && num_cparams > 0)
	      {
		for (m = 0; m < num_cparams; m++)
		{
		  cparam = (ppd_cparam_t *)cupsArrayIndex(coption->params, m);
		  if (cparam->type != PPD_CUSTOM_INT &&
		      cparam->type != PPD_CUSTOM_STRING &&
		      cparam->type != PPD_CUSTOM_PASSWORD &&
		      cparam->type != PPD_CUSTOM_PASSCODE)
		    break;
		}
		if (m < num_cparams)
		{
		  papplLog(system, PAPPL_LOGLEVEL_WARN,
			   "  Custom setting for this option not possible, as the parameter \"%s\" (\"%s\") is of a format not supported by PAPPL",
			   cparam->name, cparam->text);
		  num_cparams = 0;
		  continue;
		}
	      }
	      ppdPwgUnppdizeName(option->choices[k].text,
				 ipp_choice, sizeof(ipp_choice), NULL);
	      // "True" and "False" as boolean option choices is ugly in a
	      // user interface, replace by "Yes" and "No"
	      if (option->num_choices == 2)
	      {
		if (strcmp(ipp_choice, "true") == 0)
		  strncpy(ipp_choice, "yes", sizeof(ipp_choice) - 1);
		if (strcmp(ipp_choice, "false") == 0)
		  strncpy(ipp_choice, "no", sizeof(ipp_choice) - 1);
	      }
	      // Check whether we have a duplicate (PPD bug: 2 Choices have same
	      // Human-readable string)
	      for (m = 0; m < l; m ++)
		if (strcmp(ipp_choice, choice_list[m]) == 0)
		{
		  papplLog(system, PAPPL_LOGLEVEL_WARN,
			   "  Two choices with the same human-readable name in the PPD file (PPD file bug): Choice \"%s\" (\"%s\") giving the IPP choice name \"%s\"",
			   option->choices[k].choice,
			   option->choices[k].text, ipp_choice);
		  if (k == option->num_choices - 1 && num_cparams > 0)
		    num_cparams = 0;
		  break;
		}
	      if (m < l)
		continue;
	      // Choice is valid, add it
	      choice_list[l] = strdup(ipp_choice);
	      if (first_choice == -2)
		first_choice = k;
	      if ((!update && controlled_by_presets == 0 &&
		   option->choices[k].marked) ||
		  (update && buf[0] && !strcasecmp(ipp_choice, buf)))
	      {
		default_choice = l;
		ppdMarkOption(ppd, option->keyword, option->choices[k].choice);
	      }
	      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		       "  Adding choice \"%s\" (\"%s\") as \"%s\"%s",
		       option->choices[k].choice, option->choices[k].text,
		       ipp_choice,
		       default_choice == l ? " (default)" : "");
	      l ++;
	    }
	    else if (k == option->num_choices - 1 && num_cparams > 0)
	      // Last choice is the "custom" choice if an option allows custom
	      // values, if it is dropped, this option does not allow custom
	      // values any more
	      num_cparams = 0;
	  if (l > 0 && default_choice < 0)
	  {
	    default_choice = 0;
	    if (controlled_by_presets == 0)
	      ppdMarkOption(ppd, option->keyword,
			    option->choices[first_choice].choice);
	  }
	  if (l >= 2 + controlled_by_presets)
	  {
	    ippAddStrings(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
			  ipp_supported, l, NULL,
			  (const char * const *)choice_list);
	    ippAddString(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
			 ipp_default, NULL, choice_list[default_choice]);
	  }
	  for (k = 0; k < l; k ++)
	    free(choice_list[k]);
	  free(choice_list);
	  if (l == controlled_by_presets ||
	      (l == 1 + controlled_by_presets && num_cparams == 0))
	  {
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "   -> Skipping - Option does not make sense with current accessory configuration");
	    continue;
	  }
	}
      }
      else
	continue;

      // Add vendor option to lookup lists
      driver_data->vendor[driver_data->num_vendor] = strdup(ipp_opt);
      snprintf(buf, sizeof(buf), "%s%s", controlled_by_presets ? "/" : "",
	       option->keyword);
      extension->vendor_ppd_options[driver_data->num_vendor] = strdup(buf);

      // Next entry ...
      driver_data->num_vendor ++;

      // Does the option allow a custom value?
      if (num_cparams == 0)
	continue;

      // Go through all custom parameters of the option
      for (k = 0; k < num_cparams; k++)
      {
	cparam = (ppd_cparam_t *)cupsArrayIndex(coption->params, k);
	// Name for extra vendor option to set this parameter
	if (num_cparams == 1)
	  snprintf(ipp_custom_opt, sizeof(ipp_custom_opt), "custom-%s", ipp_opt);
	else
	{
	  ppdPwgUnppdizeName(cparam->text, ipp_param, sizeof(ipp_param), NULL);
	  snprintf(ipp_custom_opt, sizeof(ipp_custom_opt), "custom-%s-for-%s",
		   ipp_param, ipp_opt);
	}
	snprintf(ipp_supported, sizeof(ipp_supported), "%s-supported",
		 ipp_custom_opt);
	snprintf(ipp_default, sizeof(ipp_default), "%s-default",
		 ipp_custom_opt);
	// Create extra vendor option for each custom parameter, according to
	// the data type
	switch (cparam->type)
	{
	case PPD_CUSTOM_INT:
	  if (!ippFindAttribute(*driver_attrs, ipp_default, IPP_TAG_ZERO))
	    ippAddInteger(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
			  ipp_default,
			  (cparam->minimum.custom_int <= 0 &&
			   cparam->maximum.custom_int >= 0 ? 0 :
			   (cparam->maximum.custom_int < 0 ?
			    cparam->maximum.custom_int :
			    cparam->minimum.custom_int)));
	  if (!ippFindAttribute(*driver_attrs, ipp_supported, IPP_TAG_ZERO))
	    ippAddRange(*driver_attrs, IPP_TAG_PRINTER, ipp_supported,
			cparam->minimum.custom_int, cparam->maximum.custom_int);
	  break;
	case PPD_CUSTOM_STRING:
	case PPD_CUSTOM_PASSCODE:
	case PPD_CUSTOM_PASSWORD:
	  if (!ippFindAttribute(*driver_attrs, ipp_default, IPP_TAG_ZERO))
	    ippAddString(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
			 ipp_default, NULL, "");
	  break;
	case PPD_CUSTOM_CURVE:
	case PPD_CUSTOM_INVCURVE:
	case PPD_CUSTOM_POINTS:
	case PPD_CUSTOM_REAL:
	case PPD_CUSTOM_UNKNOWN:
	default:
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "  Unsupported parameter \"%s\" (\"%s\") as IPP attribute \"%s\" -> This should never happen, \"Custom\" choice should have been rejected",
		   cparam->name, cparam->text, ipp_custom_opt);
	  break;
	}
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "  Adding custom parameter \"%s\" (\"%s\") as IPP attribute \"%s\"",
		 cparam->name, cparam->text, ipp_custom_opt);
	// Add parameter vendor option to lookup lists
	driver_data->vendor[driver_data->num_vendor] = strdup(ipp_custom_opt);
	snprintf(buf, sizeof(buf), "%s%s:%s", controlled_by_presets ? "/" : "",
		 option->keyword, cparam->name);
	extension->vendor_ppd_options[driver_data->num_vendor] = strdup(buf);
	// Next entry ...
	driver_data->num_vendor ++;
      }
    }
  }

  // Add a vendor option as placeholder for saving the settings for the
  // "Installable Options" in the state file. With no "...-supported" IPP
  // attribute and IPP_TAG_TEXT format it will not appear on the "Printing
  // Defaults" web interface page.
  if (extension->installable_options)
  {
    driver_data->vendor[driver_data->num_vendor] =
      strdup("installable-options");
    extension->vendor_ppd_options[driver_data->num_vendor] = NULL;
    driver_data->num_vendor ++;
    if (!update)
      ippAddString(*driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
		   "installable-options-default", NULL, "");
  }

  // Clean up
  if (num_inst_options)
    cupsFreeOptions(num_inst_options, inst_options);

  return (true);
}


//
// 'pr_have_force_gray()' - Check PPD file whether there is an option setting
//                          which forces grayscale output. Return the first
//                          suitable one as pair of option name and value.
//

bool                                // O - True if suitable setting found
pr_have_force_gray(ppd_file_t *ppd,        // I - PPD file to check
		   const char **optstr,    // I - Option name of found option
		   const char **choicestr) // I - Choice name to force grayscale
{
  int i;
  char *p1, *p2;
  ppd_option_t *option;


  if ((option = ppdFindOption(ppd, "ColorModel")) != NULL &&
      ppdFindChoice(option, "Gray"))
  {
    if (optstr)
      *optstr = "ColorModel";
    if (choicestr)
      *choicestr = "Gray";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "ColorModel")) != NULL &&
	   ppdFindChoice(option, "Grayscale"))
  {
    if (optstr)
      *optstr = "ColorModel";
    if (choicestr)
      *choicestr = "Grayscale";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "ColorModel")) != NULL &&
	   ppdFindChoice(option, "KGray"))
  {
    if (optstr)
      *optstr = "ColorModel";
    if (choicestr)
      *choicestr = "KGray";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "HPColorMode")) != NULL &&
	   ppdFindChoice(option, "grayscale"))
  {
    if (optstr)
      *optstr = "HPColorMode";
    if (choicestr)
      *choicestr = "grayscale";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "BRMonoColor")) != NULL &&
	   ppdFindChoice(option, "Mono"))
  {
    if (optstr)
      *optstr = "BRMonoColor";
    if (choicestr)
      *choicestr = "Mono";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "CNIJSGrayScale")) != NULL &&
	   ppdFindChoice(option, "1"))
  {
    if (optstr)
      *optstr = "CNIJSGrayScale";
    if (choicestr)
      *choicestr = "1";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "HPColorAsGray")) != NULL &&
	   ppdFindChoice(option, "True"))
  {
    if (optstr)
      *optstr = "HPColorAsGray";
    if (choicestr)
      *choicestr = "True";
    return (true);
  }
  else if ((option = ppdFindOption(ppd, "ColorModel")) != NULL)
  {
    for (i = 0; i < option->num_choices; i ++)
    {
      p1 = option->choices[i].choice;
      if (strcasestr(p1, "Gray") ||
	  strcasestr(p1, "Grey") ||
	  strcasestr(p1, "Mono") ||
	  ((p2 = strcasestr(p1, "Black")) && strcasestr(p2, "White")) ||
	  (strncasecmp(p1, "BW", 2) == 0 && !isalpha(p1[2])))
      {
	if (optstr)
	  *optstr = "ColorModel";
	if (choicestr)
	  *choicestr = p1;
	return (true);
      }
    }
  }

  if (optstr)
    *optstr = NULL;
  if (choicestr)
    *choicestr = NULL;

  return (false);
}


//
// 'pr_media_col()' - Create a media-col entry
//

void
pr_media_col(pwg_size_t *pwg_size,            // I - Media size entry from PPD
	                                      //     cache
	     const char *def_source,          // I - Default media source
	     const char *def_type,            // I - Default media type
	     int left_offset,                 // I - Left offset
	     int top_offset,                  // I - Top offset
	     pappl_media_tracking_t tracking, // I - Media tracking
	     pappl_media_col_t *col)          // O - PAPPL media col entry
{
  strncpy(col->size_name, pwg_size->map.pwg, sizeof(col->size_name) - 1);
  col->size_width = pwg_size->width;
  col->size_length = pwg_size->length;
  col->left_margin = pwg_size->left;
  col->right_margin = pwg_size->right;
  col->top_margin = pwg_size->top;
  col->bottom_margin = pwg_size->bottom;
  strncpy(col->source, def_source, sizeof(col->source) - 1);
  strncpy(col->type, def_type, sizeof(col->type) - 1);
  col->left_offset = left_offset;
  col->top_offset = top_offset;
  col->tracking = tracking;
}


//
// 'pr_poll_device_option_defaults()' - This function uses query PostScript
//                                      code from the PPD file to poll
//                                      default option settings from the
//                                      printer
//

int                      // O - Number of polled default settings
                                //     0: Error
pr_poll_device_option_defaults(
    pappl_printer_t *printer,   // I - Printer to be polled
    bool installable,           // I - Poll installable accessory configuration?
    cups_option_t **defaults)   // O - Option list of polled default settings
{
  int                    i, j, k;       // Looping variables
  pr_cups_device_data_t  *device_data = NULL;
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;
  ppd_file_t             *ppd = NULL;	// PPD file of the printer
  int                    num_defaults;  // Number of polled default settings
  pappl_device_t         *device;       // PAPPL output device
  int                    datalen;
  int		         status = 0;	// Exit status
  ppd_group_t            *group;
  ppd_option_t	         *option;	// Current option in PPD
  ppd_attr_t	         *attr;		// Query command attribute
  const char	         *valptr;	// Pointer into attribute value
  char		         buf[1024],	// String buffer
                         *bufptr;	// Pointer into buffer
  ssize_t	         bytes;		// Number of bytes read


  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  ppd = extension->ppd;

  *defaults = NULL;
  num_defaults = 0;

  //
  // Open access to printer device...
  //

  if ((device = papplPrinterOpenDevice(printer)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "Cannot access printer: Busy or otherwise not reachable");
    return (0);
  }

  // We have a CUPS backend, make use of the side channel to issue
  // control commands
  if (strncmp(papplPrinterGetDeviceURI(printer), "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Start backend if not yet done so (first access is not by PAPPL device
    // API function)
    if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
      return (0);

    // Standard FD for the side channel is 4, the CUPS library
    // functions use always FD 4. Therefore we redirect our side
    // channel pipe end to FD 4
    dup2(device_data->sidefd, 4);

    // See if the backend supports bidirectional I/O...
    datalen = 1;
    if (cupsSideChannelDoRequest(CUPS_SC_CMD_GET_BIDI, buf, &datalen,
				 5.0) != CUPS_SC_STATUS_OK ||
	buf[0] != CUPS_SC_BIDI_SUPPORTED)
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "Unable to query defaults from printer - no "
		      "bidirectional I/O available!");
      return (0);
    }
  }

  // Note: We directly output to the printer device without using
  //       pr_print_filter_function() as the original code
  //       (commandtops filter of CUPS) only uses printf()/puts() and
  //       not any PPD-related function of libcups (eq. libppd)
  //       for the output to the printer

  //
  // Put the printer in PostScript mode...
  //

  if (ppd->jcl_begin)
  {
    papplDevicePuts(device, ppd->jcl_begin);
    papplDevicePuts(device, ppd->jcl_ps);
  }

  papplDevicePuts(device, "%!\n");
  papplDevicePuts(device,
		  "userdict dup(\\004)cvn{}put (\\004\\004)cvn{}put\n");

  papplDeviceFlush(device);

  //
  // https://github.com/apple/cups/issues/4028
  //
  // As a lot of PPDs contain bad PostScript query code, we need to prevent one
  // bad query sequence from affecting all auto-configuration.  The following
  // error handler allows us to log PostScript errors to cupsd.
  //

  papplDevicePuts(device,
    "/cups_handleerror {\n"
    "  $error /newerror false put\n"
    "  (:PostScript error in \") print cups_query_keyword print (\": ) "
    "print\n"
    "  $error /errorname get 128 string cvs print\n"
    "  (; offending command:) print $error /command get 128 string cvs "
    "print (\n) print flush\n"
    "} bind def\n"
    "errordict /timeout {} put\n"
		  "/cups_query_keyword (?Unknown) def\n");
  papplDeviceFlush(device);

  if (device_data)
  {
    // Wait for the printer to become connected...
    do
    {
      sleep(1);
      datalen = 1;
    }
    while (cupsSideChannelDoRequest(CUPS_SC_CMD_GET_CONNECTED, buf, &datalen,
				    device_data->side_timeout) ==
	   CUPS_SC_STATUS_OK && !buf[0]);
  }

  //
  // Loop through every option in the PPD file and ask for the current
  // value...
  //

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		  "Reading printer-internal default settings...");

  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {

    // When "installable" is true, We are treating only the
    // "Installable Options" group of options in the PPD file here
    // otherwise only the other options

    if (strncasecmp(group->name, "Installable", 11) == 0)
    {
      if (!installable)
	continue;
    }
    else if (installable)
      continue;

    for (j = group->num_options, option = group->options;
	 j > 0;
	 j --, option ++)
    {
      // Does the option have less than 2 choices? Then it does not make
      // sense to query its default value
      if (option->num_choices < 2)
	continue;

      //
      // See if we have a query command for this option...
      //

      snprintf(buf, sizeof(buf), "?%s", option->keyword);

      if ((attr = ppdFindAttr(ppd, buf, NULL)) == NULL || !attr->value)
      {
	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			"Skipping %s option...", option->keyword);
	continue;
      }

      //
      // Send the query code to the printer...
      //

      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "Querying %s...", option->keyword);

      for (bufptr = buf, valptr = attr->value; *valptr; valptr ++)
      {
	//
	// Log the query code, breaking at newlines...
	//

	if (*valptr == '\n')
	{
	  *bufptr = '\0';
	  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			  "%s\\n", buf);
	  bufptr = buf;
	}
	else if (*valptr < ' ')
        {
	  if (bufptr >= (buf + sizeof(buf) - 4))
          {
	    *bufptr = '\0';
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "%s", buf);
	    bufptr = buf;
	  }

	  if (*valptr == '\r')
          {
	    *bufptr++ = '\\';
	    *bufptr++ = 'r';
	  }
	  else if (*valptr == '\t')
          {
	    *bufptr++ = '\\';
	    *bufptr++ = 't';
          }
	  else
          {
	    *bufptr++ = '\\';
	    *bufptr++ = '0' + ((*valptr / 64) & 7);
	    *bufptr++ = '0' + ((*valptr / 8) & 7);
	    *bufptr++ = '0' + (*valptr & 7);
	  }
	}
	else
        {
	  if (bufptr >= (buf + sizeof(buf) - 1))
          {
	    *bufptr = '\0';
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "%s", buf);
	    bufptr = buf;
	  }

	  *bufptr++ = *valptr;
	}
      }

      if (bufptr > buf)
      {
	*bufptr = '\0';
	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			"%s", buf);
      }

      papplDevicePrintf(device, "/cups_query_keyword (?%s) def\n",
			option->keyword); // Set keyword for error reporting
      papplDevicePuts(device, "{ (");
      for (valptr = attr->value; *valptr; valptr ++)
      {
	if (*valptr == '(' || *valptr == ')' || *valptr == '\\')
	  papplDevicePuts(device, "\\");
	papplDeviceWrite(device, valptr, 1);
      }
      papplDevicePuts(device,
			") cvx exec } stopped { cups_handleerror } if clear\n");
      // Send query code
      papplDeviceFlush(device);

      if (device_data)
      {
	// Flush the data from the backend into the printer
	datalen = 0;
	cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buf, &datalen,
				 device_data->side_timeout);
      }
      
      //
      // Read the response data...
      //

      bufptr    = buf;
      buf[0] = '\0';
      // When we use a PAPPL-native backend then if no bytes get read
      // (bytes <= 0), we repeat up to 100 times in 100 msec intervals
      // (10 sec timeout), for a CUPS backend we use the built-in
      // timeout handling of cupsBackChannelRead() (which is called by
      // pr_cups_devread(), called by papplDeviceRead().
      for (k = 0; k < 100; k ++)
      {
	//
	// Read answer from device ...
	//

	bytes = papplDeviceRead(device, bufptr,
				sizeof(buf) - (size_t)(bufptr - buf) - 1);

	//
	// No bytes of the answer arrived yet? Retry ...
	//

	if (bytes <= 0)
        {
	  if (device_data)
	  {
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "Back channel read timed out after 10 sec.");
	    status = 1;
	    break;
	  }
	  else
	  {
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "Answer not ready yet, retrying in 100 ms.");
	    usleep(100000);
	    continue;
	  }
        }

	//
	// No newline at the end? Go on reading ...
	//

	bufptr += bytes;
	*bufptr = '\0';

	if (bytes == 0 ||
	    (bufptr > buf && bufptr[-1] != '\r' && bufptr[-1] != '\n'))
	  continue;

	//
	// Trim whitespace and control characters from both ends...
	//

	bytes = bufptr - buf;

	for (bufptr --; bufptr >= buf; bufptr --)
	  if (isspace(*bufptr & 255) || iscntrl(*bufptr & 255))
	    *bufptr = '\0';
	  else
	    break;

	for (bufptr = buf; isspace(*bufptr & 255) || iscntrl(*bufptr & 255);
	     bufptr ++);

	if (bufptr > buf)
        {
	  memmove(buf, bufptr, strlen(bufptr) + 1);
	  bufptr = buf;
	}

	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			"Got %d bytes.", (int)bytes);

	//
	// Skip blank lines...
	//

	if (!buf[0])
	  continue;

	//
	// Check the response...
	//

	if ((bufptr = strchr(buf, ':')) != NULL)
        {
	  //
	  // PostScript code for this option in the PPD is broken; show the
	  // interpreter's error message that came back...
	  //

	  papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN,
			  "%s", bufptr + 1);
	  status = 1;
	  break;
	}

	//
	// Verify the result is a valid option choice...
	//

	if (!ppdFindChoice(option, buf))
        {
	  if (!strcasecmp(buf, "Unknown"))
	  {
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN,
			    "Unknown default setting for option \"%s\"",
			    option->keyword);
	    status = 1;
	    break;
	  }

	  bufptr    = buf;
	  buf[0] = '\0';
	  continue;
	}

        //
        // Write out the result and move on to the next option...
	//

	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			"Read default setting for \"%s\": \"%s\"",
			option->keyword, buf);
	num_defaults = cupsAddOption(option->keyword, buf, num_defaults,
				     defaults);
	break;
      }

      //
      // Printer did not answer this option's query
      //

      if (bytes <= 0)
      {
	papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN,
			"No answer to query for option %s within 10 sec "
			"timeout.", option->keyword);
	status = 1;
      }
    }
  }

  //
  // Finish the job...
  //

  papplDeviceFlush(device);
  if (ppd->jcl_end)
    papplDevicePuts(device, ppd->jcl_end);
  else
    papplDevicePuts(device, "\004");
  papplDeviceFlush(device);

  //
  // Close connection to the printer device...
  //

  papplPrinterCloseDevice(printer);

  //
  // Return...
  //

  if (status)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN,
		    "Unable to configure some printer options.");

  return (num_defaults);
}


//
// 'pr_printer_update_for_installable_options() - Update printer's driver
//                                                data and driver IPP
//                                                attributes for changes
//                                                in the "Installable Options"
//                                                settings.
//

void
pr_printer_update_for_installable_options(
    pappl_printer_t *printer,           // I - Printer
    pappl_pr_driver_data_t driver_data, // I - Driver data
    const char *instoptstr)             // I - Installable options in a string 
                                        //     of key=value pairs or NULL for
                                        //     keeping current settings
{
  int                    i;
  pappl_system_t         *system;       // System
  ipp_t                  *driver_attrs,
                         *vendor_attrs;
  ipp_attribute_t        *attr;
  char                   buf[1024];
  pr_driver_extension_t  *extension =
    (pr_driver_extension_t *)driver_data.extension;

  // Get system...
  system = papplPrinterGetSystem(printer);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		  "Updating printer's driver data and attributes to the \"Installable Options\" settings.");
  if (instoptstr)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "New \"Installable Options\" settings: %s", instoptstr);

  // Get a copy of the driver IPP attributes to save the vendor option settings
  driver_attrs = papplPrinterGetDriverAttributes(printer);
  if ((attr = ippFindAttribute(driver_attrs, "installable-options-default",
			       IPP_TAG_ZERO)) != NULL &&
      ippAttributeString(attr, buf, sizeof(buf)) > 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "Previous installable accessories settings: %s", buf);
    if (!instoptstr)
      instoptstr = buf;
  }
  else
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "Installable Options settings not found");

  // If we have new installable options settings update them in driver_attrs
  if (instoptstr != buf)
  {
    if ((attr = ippFindAttribute(driver_attrs, "installable-options-default",
				 IPP_TAG_ZERO)) != NULL)
      ippDeleteAttribute(driver_attrs, attr);
    ippAddString(driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
		 "installable-options-default", NULL, instoptstr);
  }

  // Update the driver data to correspond with the printer hardware
  // accessory configuration ("Installable Options" in the PPD)
  pr_driver_setup(system, NULL, NULL, NULL, &driver_data, &driver_attrs,
		  extension->global_data);

  // Data structure for vendor option IPP attributes
  vendor_attrs = ippNew();

  // Copy the vendor option IPP attributes
  for (i = 0; i < driver_data.num_vendor; i ++)
  {
    snprintf(buf, sizeof(buf), "%s-default", driver_data.vendor[i]);
    attr = ippFindAttribute(driver_attrs, buf, IPP_TAG_ZERO);
    if (attr)
      ippCopyAttribute(vendor_attrs, attr, 0);
    else
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "Default setting for vendor option \"%s\" not found",
		      driver_data.vendor[i]);
    if (!strcmp(driver_data.vendor[i], "installable-options"))
      continue;
    snprintf(buf, sizeof(buf), "%s-supported", driver_data.vendor[i]);
    attr = ippFindAttribute(driver_attrs, buf, IPP_TAG_ZERO);
    if (attr)
      ippCopyAttribute(vendor_attrs, attr, 0);
    else
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "Supported choices for vendor option \"%s\" not found",
		      driver_data.vendor[i]);
  }

  // Save the updated driver data back to the printer
  papplPrinterSetDriverData(printer, &driver_data, vendor_attrs);

  // Clean up
  ippDelete(driver_attrs);
  ippDelete(vendor_attrs);
}


//
// 'pr_setup_add_ppd_files_page()' - Add web admin interface page for adding
//                                   PPD files.
//

void
pr_setup_add_ppd_files_page (void *data)  // I - Global data
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pappl_system_t   *system = global_data->system;

  if (global_data->config->components & PR_COPTIONS_WEB_ADD_PPDS)
  {
    papplSystemAddResourceCallback(system, "/addppd", "text/html",
				   (pappl_resource_cb_t)pr_system_web_add_ppd,
				   global_data);
    papplSystemAddLink(system, "Add PPD Files", "/addppd",
		       PAPPL_LOPTIONS_OTHER | PAPPL_LOPTIONS_HTTPS_REQUIRED);
  }
}


//
// 'pr_setup_device_settings_page()' - Add web admin interface page for
//                                     device settings: Installable
//                                     accessories and polling PostScript
//                                     option defaults
//

void
pr_setup_device_settings_page(pappl_printer_t *printer, // I - Printer
			      void *data)               // I - Global data
                                                        //     (unused)
{
  char                   path[256];     // Path to resource
  pappl_system_t         *system;	// System
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;


  (void)data;

  system = papplPrinterGetSystem(printer);

  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  if (extension->defaults_pollable ||
      extension->installable_options)
  {
    papplPrinterGetPath(printer, "device", path, sizeof(path));
    papplSystemAddResourceCallback(system, path, "text/html",
			     (pappl_resource_cb_t)pr_printer_web_device_config,
			     printer);
    papplPrinterAddLink(printer, "Device Settings", path,
			PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);
  }
}


//
// 'pr_setup_driver_list()' - Create a driver list of the available PPD files.
//

void
pr_setup_driver_list(pr_printer_app_global_data_t *global_data)
{
  int              i, j, k;
  char             *generic_ppd, *mfg_mdl, *dev_id;
  char             *end_model, *drv_name;
  char             *ppd_model_name;
  pr_ppd_path_t    *ppd_path;
  int              num_options = 0;
  cups_option_t    *options = NULL;
  cups_array_t     *ppds;
  ppd_info_t       *ppd;
  char             driver_info[1024];
  char             buf1[1024], buf2[1024];
  char             *ptr;
  int              pre_normalized;
  pappl_pr_driver_t swap;
  pappl_system_t   *system = global_data->system;
  int              num_drivers = global_data->num_drivers;
  pappl_pr_driver_t *drivers = global_data->drivers;
  cups_array_t     *ppd_paths = global_data->ppd_paths,
                   *ppd_collections = global_data->ppd_collections;
  regex_t          *driver_re = NULL;


  //
  // Create the list of all available PPD files
  //

  ppds = ppdCollectionListPPDs(ppd_collections, 0,
			       num_options, options,
			       (filter_logfunc_t)papplLog, system);

  //
  // Create driver list from the PPD list and submit it
  //
  
  if (ppds)
  {
    i = 0;
    num_drivers = cupsArrayCount(ppds);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Found %d PPD files.", num_drivers);
    generic_ppd = NULL;
    if (!(global_data->config->components & PR_COPTIONS_NO_GENERIC_DRIVER))
    {
      // Search for a generic PPD to use as generic PostScript driver
      for (ppd = (ppd_info_t *)cupsArrayFirst(ppds);
	   ppd;
	   ppd = (ppd_info_t *)cupsArrayNext(ppds))
      {
	if (!strcasecmp(ppd->record.make, "Generic") ||
	    !strncasecmp(ppd->record.make_and_model, "Generic", 7) ||
	    !strncasecmp(ppd->record.products[0], "Generic", 7))
	{
	  generic_ppd = ppd->record.name;
	  break;
	}
      }
      if (generic_ppd)
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Found generic PPD file: %s", generic_ppd);
      else
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "No generic PPD file found, "
		 "Printer Application will only support printers "
		 "explicitly supported by the PPD files");
    }
    // Create driver indices
    if (drivers)
      free(drivers);
    drivers = (pappl_pr_driver_t *)calloc(num_drivers + PPD_MAX_PROD,
					  sizeof(pappl_pr_driver_t));
    // Create list of PPD file paths
    if (ppd_paths)
      cupsArrayDelete(ppd_paths);
    ppd_paths = cupsArrayNew(pr_compare_ppd_paths, NULL);
    if (generic_ppd)
    {
      drivers[i].name = strdup("generic");
      drivers[i].description = strdup("Generic Printer");
      drivers[i].device_id = strdup("");
      drivers[i].extension = strdup(" generic");
      i ++;
      ppd_path = (pr_ppd_path_t *)calloc(1, sizeof(pr_ppd_path_t));
      ppd_path->driver_name = strdup("generic");
      ppd_path->ppd_path = strdup(generic_ppd);
      cupsArrayAdd(ppd_paths, ppd_path);
    }
    if (global_data->config->driver_display_regex)
    {
      // Compile the regular expression for separating the driver info
      // from the model name
      if ((driver_re = (regex_t *)calloc(1, sizeof(regex_t))) != NULL)
      {
	if (regcomp(driver_re, global_data->config->driver_display_regex,
		    REG_ICASE | REG_EXTENDED))
	{
	  regfree(driver_re);
	  driver_re = NULL;
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "Invalid regular expression: %s",
		   global_data->config->driver_display_regex);
	}
      }
    }
    for (ppd = (ppd_info_t *)cupsArrayFirst(ppds);
	 ppd;
	 ppd = (ppd_info_t *)cupsArrayNext(ppds))
    {
      if (!generic_ppd || strcmp(ppd->record.name, generic_ppd))
      {
	// If we have a regular expression to extract the extra info
	// (driver info) from the *NickName entries of the PPDs (for
	// example if the Printer Application contains more than one
	// driver for some printers) we separate this extra info (at
	// least the driver name in it) to combine it also with extra
	// make/model names from *Product entries.
	driver_info[0] = '\0';
	if (driver_re)
        {
	  // Get driver info from *NickName entry
	  ieee1284NormalizeMakeAndModel(ppd->record.make_and_model,
					NULL,
					IEEE1284_NORMALIZE_HUMAN,
				        driver_re,
					buf2, sizeof(buf2),
					NULL, &end_model, &drv_name);
	  if (end_model)
	  {
	    ppd->record.make_and_model[end_model - buf2 - strlen(buf2) +
				       strlen(ppd->record.make_and_model)] =
	      '\0';
	    if (drv_name)
	    {
	      if (drv_name[0])
	      {
		if (end_model[0] &&
		    !strncasecmp(drv_name, end_model, strlen(drv_name)))
		  snprintf(driver_info, sizeof(driver_info), "%s", drv_name);
		else
		  snprintf(driver_info, sizeof(driver_info), ", %s", drv_name);
	      }
	    }
	    else
	    {
	      if (end_model[0])
		snprintf(driver_info, sizeof(driver_info), "%s", end_model);
	    }
	  }
	  else if (global_data->config->components &
		   PR_COPTIONS_USE_ONLY_MATCHING_NICKNAMES)
	  {
	    free(ppd);
	    continue;
	  }
	}
	// Note: The last entry in the product list is the ModelName of the
	// PPD and not an actual Product entry. Therefore we ignore it as
	// a product name entry (Hidden feature of ppdCollectionListPPDs())
	for (j = 0; j < PPD_MAX_PROD; j ++)
	  if (!ppd->record.products[j][0])
	    break;
	ppd_model_name = (j > 0 ? ppd->record.products[j - 1] : NULL);
        for (j = -1;
	     j < (global_data->config->components &
		  PR_COPTIONS_PPD_NO_EXTRA_PRODUCTS ? 0 : PPD_MAX_PROD - 1);
	     j ++)
	{
	  // End of product list
          if (j >= 0 &&
	      (!ppd->record.products[j][0] || !ppd->record.products[j + 1][0]))
            break;
	  // If there is only 1 product, ignore it, it is either the
	  // model of the PPD itself or something weird
	  if (j == 0 &&
	      (!ppd->record.products[1][0] || !ppd->record.products[2][0]))
	    break;
	  pre_normalized = 0;
	  dev_id = NULL;
	  if (j < 0)
	  {
	    // Model of PPD itself
	    if (ppd->record.device_id[0] &&
		(strstr(ppd->record.device_id, "MFG:") ||
		 strstr(ppd->record.device_id, "MANUFACTURER:")) &&
		(strstr(ppd->record.device_id, "MDL:") ||
		 strstr(ppd->record.device_id, "MODEL:")) &&
		!strstr(ppd->record.device_id, "MDL:hp_") &&
		!strstr(ppd->record.device_id, "MDL:hp-") &&
		!strstr(ppd->record.device_id, "MDL:HP_") &&
		!strstr(ppd->record.device_id, "MODEL:hp2") &&
		!strstr(ppd->record.device_id, "MODEL:hp3") &&
		!strstr(ppd->record.device_id, "MODEL:hp9") &&
		!strstr(ppd->record.device_id, "MODEL:HP2"))
	    {
	      // To check whether the device ID is not something
	      // weird, unsuitable as a display string, we save the
	      // normalized NickName for comparison. Only if the first
	      // word (cleaned manufacturer name or part of it) is the
	      // same, we accept the data of the device ID as display
	      // string.
	      strncpy(buf1, buf2, sizeof(buf1));
	      if ((ptr = strchr(buf1, ' ')) != NULL)
		*ptr = '\0';
	      // Convert device ID to make/model string, so that we can add
	      // the language for building final index strings
	      mfg_mdl = ieee1284NormalizeMakeAndModel(ppd->record.device_id,
						      NULL,
						      IEEE1284_NORMALIZE_HUMAN,
						      NULL, buf2, sizeof(buf2),
						      NULL, NULL, NULL);
	      if (strncasecmp(mfg_mdl, buf1, strlen(buf1)) == 0)
		pre_normalized = 1;
	    }
	    if (pre_normalized == 0)
	    {
	      if (ppd->record.products[0][0] &&
		  ((ppd->record.products[1][0] &&
		    ppd->record.products[2][0]) ||
		   (!strncasecmp(ppd->record.products[0],
				 ppd->record.make_and_model,
				 strlen(ppd->record.products[0])))))
		mfg_mdl = ppd->record.products[0];
	      else if (ppd_model_name)
		mfg_mdl = ppd_model_name;
	      else
		mfg_mdl = ppd->record.make_and_model;
	    }
	    if (ppd->record.device_id[0])
	      dev_id = ppd->record.device_id;
	  }
	  else
	    // Extra models in list of products
	    mfg_mdl = ppd->record.products[j];
	  // Remove parantheses from model name if it came from a Product
	  // entry of the PPD
	  if (mfg_mdl[0] == '(' && mfg_mdl[strlen(mfg_mdl) - 1] == ')')
	  {
	    memmove(mfg_mdl, mfg_mdl + 1, strlen(mfg_mdl) - 2);
	    mfg_mdl[strlen(mfg_mdl) - 2] = '\0';
	  }
	  // We preferably register device IDs actually found in the PPD files,
	  // For PPDs without explicit device ID we try our best to fill the
	  // model field with only the model name, without driver specification
	  if (dev_id)
	    drivers[i].device_id = strdup(dev_id);
	  {
	    snprintf(buf1, sizeof(buf1) - 1, "MFG:%s;MDL:%s;",
		     ppd->record.make, mfg_mdl);
	    drivers[i].device_id = strdup(buf1);
	  }
	  // New entry for PPD lookup table
	  ppd_path = (pr_ppd_path_t *)calloc(1, sizeof(pr_ppd_path_t));
	  // Base make/model/language string to generate the needed index
	  // strings
	  snprintf(buf1, sizeof(buf1) - 1, "%s%s%s (%s)",
		   mfg_mdl, driver_info,
		   ((global_data->config->components &
		     PR_COPTIONS_WEB_ADD_PPDS) &&
		    !strncmp(ppd->record.name, global_data->user_ppd_dir,
			     strlen(global_data->user_ppd_dir)) &&
		    ppd->record.name[strlen(global_data->user_ppd_dir)] == '/' ?
		    " - USER-ADDED" : ""),
		   ppd->record.languages[0]);
	  // IPP-compatible string as driver name
	  drivers[i].name =
	    strdup(ieee1284NormalizeMakeAndModel(buf1, ppd->record.make,
						 IEEE1284_NORMALIZE_IPP,
						 NULL, buf2, sizeof(buf2),
						 NULL, NULL, NULL));
	  ppd_path->driver_name = strdup(drivers[i].name);
	  // Path to grab PPD from repositories
	  ppd_path->ppd_path = strdup(ppd->record.name);
	  cupsArrayAdd(ppd_paths, ppd_path);
	  // Human-readable string to appear in the driver drop-down
	  if (pre_normalized)
	    drivers[i].description = strdup(buf1);
	  else
	    drivers[i].description =
	      strdup(ieee1284NormalizeMakeAndModel(buf1, ppd->record.make,
						   IEEE1284_NORMALIZE_HUMAN,
						   NULL, buf2, sizeof(buf2),
						   NULL, NULL, NULL));
	  // List sorting index with padded numbers (typos in example intended)
	  // "LaserJet 3P" < "laserjet 4P" < "Laserjet3000P" < "LaserJet 4000P"
	  drivers[i].extension =
	    strdup(ieee1284NormalizeMakeAndModel(buf1, ppd->record.make,
					IEEE1284_NORMALIZE_COMPARE |
					IEEE1284_NORMALIZE_LOWERCASE |
					IEEE1284_NORMALIZE_SEPARATOR_SPACE |
					IEEE1284_NORMALIZE_PAD_NUMBERS,
					NULL, buf2, sizeof(buf2),
					NULL, NULL, NULL));
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "File: %s; Printer (%d): %s; --> Entry %d: Driver %s; "
		   "Description: %s; Device ID: %s; Sorting index: %s",
		   ppd_path->ppd_path, j, buf1, i, drivers[i].name,
		   drivers[i].description, drivers[i].device_id,
		   (char *)(drivers[i].extension));
	  // Sort the new entry into the list via the extension
	  for (k = i;
	       k > 0 &&
		 ((strncmp(drivers[k - 1].extension, "generic  ", 9) &&
		   !strncmp(drivers[k].extension, "generic  ", 9)) ||
		  strcmp((char *)(drivers[k - 1].extension),
			 (char *)(drivers[k].extension)) > 0);
	       k --)
	  {
	    swap = drivers[k - 1];
	    drivers[k - 1] = drivers[k];
	    drivers[k] = swap;
	  }
	  // Check for duplicates
	  if (k > 0 &&
	      (strcmp(drivers[k - 1].name, drivers[k].name) == 0 ||
	       strcasecmp(drivers[k - 1].description,
			  drivers[k].description) == 0))
	  {
	    // Remove the duplicate
	    // We do not count the freeable memory here as in the end
	    // we adjust the allocated memory anyway
	    memmove(&drivers[k], &drivers[k + 1],
		    (i - k) * sizeof(pappl_pr_driver_t));
	    i --;
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "DUPLICATE REMOVED!");
	  }
	  // Next position in the list
	  i ++;
	}
	// Add memory for PPD with multiple product entries
	num_drivers += j;
	drivers = (pappl_pr_driver_t *)reallocarray(drivers,
						    num_drivers +
						    PPD_MAX_PROD,
						    sizeof(pappl_pr_driver_t));
      }
      free(ppd);
    }

    // Free the compiled regular expression
    if (driver_re)
      regfree(driver_re);

    cupsArrayDelete(ppds);

    // Final adjustment of allocated memory
    drivers = (pappl_pr_driver_t *)reallocarray(drivers, i,
						sizeof(pappl_pr_driver_t));
    num_drivers = i;
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Created %d driver entries.", num_drivers);
    global_data->num_drivers = num_drivers;
    global_data->drivers = drivers;
    global_data->ppd_paths = ppd_paths;
  }
  else
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "No PPD files found.");

  papplSystemSetPrinterDrivers(system, num_drivers, drivers,
			       global_data->config->autoadd_cb,
			       global_data->config->printer_extra_setup_cb,
			       pr_driver_setup, global_data);
}


//
// 'pr_setup()' - Setup CUPS driver(s).
//

void
pr_setup(pr_printer_app_global_data_t *global_data)  // I - Global data 
{
  pappl_system_t   *system = global_data->system;
  char             *ptr1, *ptr2;
  ppd_collection_t *col = NULL;
  pr_spooling_conversion_t *conversion;


  //
  // Clean up debug copy files of jobs in spool directory
  //

  pr_clean_debug_copies(global_data);

  //
  // Create PPD collection index data structure
  //

  global_data->num_drivers = 0;
  global_data->drivers = NULL;
  global_data->ppd_paths = cupsArrayNew(pr_compare_ppd_paths, NULL);
  global_data->ppd_collections = cupsArrayNew(NULL, NULL);

  //
  // Build PPD list from all repositories
  //

  if (global_data->ppd_dirs_list[0])
  {
    ptr1 = global_data->ppd_dirs_list;
    while (ptr1 && *ptr1)
    {
      ptr2 = strchr(ptr1, ':');
      if (ptr2)
	*ptr2 = '\0';
      col = (ppd_collection_t *)calloc(1, sizeof(ppd_collection_t));
      col->name = NULL;
      col->path = ptr1;
      cupsArrayAdd(global_data->ppd_collections, col);
      if (ptr2)
	ptr1 = ptr2 + 1;
      else
	ptr1 = NULL;
    }
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_FATAL, "No PPD file location defined.");
    return;
  }

  //
  // Last entry in the list is the directory for the user to drop
  // extra PPD files in via the web interface
  //

  if (global_data->config->components & PR_COPTIONS_WEB_ADD_PPDS &&
      col && !global_data->user_ppd_dir[0])
    strncpy(global_data->user_ppd_dir, col->path,
	    sizeof(global_data->user_ppd_dir) - 1);

  //
  // Create the list of all available PPD files
  //

  pr_setup_driver_list(global_data);

  //
  // Add filters for the different input data formats
  //

  for (conversion =
	 (pr_spooling_conversion_t *)
	 cupsArrayFirst(global_data->config->spooling_conversions);
       conversion;
       conversion =
	 (pr_spooling_conversion_t *)
	 cupsArrayNext(global_data->config->spooling_conversions))
    papplSystemAddMIMEFilter(system,
			     conversion->srctype,
			     "application/vnd.printer-specific",
			     pr_filter, global_data);

  //
  // Add "cups" scheme to use CUPS backends for devices
  //

  if (global_data->config->components & PR_COPTIONS_CUPS_BACKENDS)
  {
    if (!(global_data->config->components & PR_COPTIONS_NO_PAPPL_BACKENDS))
    {
      // Dummy operation on the PAPPL devices to trigger the creation of
      // PAPPL's standard schemes
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Triggering creation of PAPPL's standard schemes");
      papplDeviceList(PAPPL_DEVTYPE_ALL, pr_dummy_device, NULL, papplLogDevice,
		      system);
    }
    
    // We use this global pointer only for our "cups" scheme to have
    // access to info about backend dir and include/exclude lists as the
    // device API is missing a user data pointer.
    // We do not use this pointer elsewhere so that we can easily remove
    // it in case this API shortcoming gets fixed
    pr_cups_device_user_data = global_data;

    // Add the "cups" scheme for the CUPS backends we will include
    // We cannot add schemes named by the backends as the device list
    // callback does not have access to the scheme name to know for
    // which backend to list devices
    // URIs will be "cups:" followed by the original CUPS URI
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Adding \"cups\" device scheme for CUPS backends");
    papplDeviceAddScheme("cups", PAPPL_DEVTYPE_ALL, pr_cups_devlist,
			 pr_cups_devopen, pr_cups_devclose, pr_cups_devread,
			 pr_cups_devwrite, pr_cups_devstatus,
			 pr_cups_devid);
  }
}


//
// 'pr_status()' - Get printer status.
//

bool                   // O - `true` on success, `false` on failure
pr_status(
    pappl_printer_t *printer) // I - Printer
{
  pappl_system_t         *system;              // System
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;
  pr_printer_app_global_data_t *global_data;


  // Get system...
  system = papplPrinterGetSystem(printer);

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		  "Status callback called.");

  // Load the driver data
  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  global_data = extension->global_data;
  if (!extension->updated)
  {
    // Adjust the driver data according to the installed accessories
    pr_printer_update_for_installable_options(printer, driver_data, NULL);
    // Save new default settings (but only if system is running, to not
    // overwrite the state file when it is still loaded during startup)
    if (papplSystemIsRunning(system))
      papplSystemSaveState(system, global_data->state_file);
  }

  // Use commandtops CUPS filter code to check status here (ink levels, ...)
  // (TODO)

  // Do PostScript jobs for polling only once a minute or once every five
  // minutes, therefore save time of last call in a static variable. and
  // only poll again if last poll is older than given time.

  // Needs ink level support in PAPPL
  // (https://github.com/michaelrsweet/pappl/issues/83)

  return (true);
}


//
// 'pr_testpage()' - Return a test page file to print
//                   Simple function for Printer Applications which
//                   have one single test page for all printers
//

const char *			// O - Filename or `NULL`
pr_testpage(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - File Buffer
    size_t          bufsize)		// I - Buffer Size
{
  pappl_pr_driver_data_t driver_data;
  pr_driver_extension_t  *extension;
  pr_printer_app_global_data_t *global_data; // Global data


  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  global_data = extension->global_data;

  // Join directory and file
  snprintf(buffer, bufsize, "%s/%s", global_data->testpage_dir,
	   (char *)(global_data->config->testpage_data));

  // Does it actually exist?
  if (access(buffer, R_OK))
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR,
		    "Test page %s not found or not readable.", buffer);
    *buffer = '\0';
    return (NULL);
  }
  else
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		    "Using test page: %s", buffer);
    return (buffer);
  }
}


//
// 'pr_system_cb()' - System callback.
//

pappl_system_t *			// O - New system object
pr_system_cb(int           num_options,	// I - Number of options
	     cups_option_t *options,	// I - Options
	     void          *data)	// I - Callback data
{
  int                   i;
  pr_printer_app_global_data_t *global_data =
                                        (pr_printer_app_global_data_t *)data;
  pappl_system_t	*system;	// System object
  const char		*val,		// Current option value
			*hostname,	// Hostname, if any
			*logfile,	// Log file, if any
			*system_name;	// System name, if any
  pappl_loglevel_t	loglevel;	// Log level
  int			port = 0;	// Port number, if any
  char                  *ptr;
  char                  buf[1024];
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE |
                                   PAPPL_SOPTIONS_WEB_INTERFACE |
                                   PAPPL_SOPTIONS_WEB_LOG |
                                   PAPPL_SOPTIONS_WEB_NETWORK |
                                   PAPPL_SOPTIONS_WEB_SECURITY |
                                   PAPPL_SOPTIONS_WEB_TLS;
					// System options
  static pappl_version_t versions[1];   // Software versions

  // One single record for version information
  strncpy(versions[0].name, global_data->config->system_name,
	  sizeof(versions[0].name) - 1);
  versions[0].patches[0] = '\0';
  strncpy(versions[0].sversion, global_data->config->version,
	  sizeof(versions[0].sversion) - 1);
  for (i = 0; i < 4; i ++)
    versions[0].version[i] = global_data->config->numeric_version[i];

  // Parse options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "ps-printer-app: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile     = cupsGetOption("log-file", num_options, options);
  hostname    = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "ps-printer-app: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // PPD collection dirs list
  if ((val = cupsGetOption("ppd-directories", num_options, options)) != NULL ||
      (val = getenv("PPD_DIRS")) != NULL ||
      (val = getenv("PPD_PATHS")) != NULL)
    snprintf(global_data->ppd_dirs_list, sizeof(global_data->ppd_dirs_list),
	     "%s", val);
  else if (!global_data->ppd_dirs_list[0])
    snprintf(global_data->ppd_dirs_list, sizeof(global_data->ppd_dirs_list),
	     "/usr/share/ppd:/usr/share/cups/model:/usr/lib/cups/driver:/usr/share/cups/drv:/var/lib/%s/ppd",
	     global_data->config->system_package_name);

  // Dir for user-uploaded PPD files
  if (global_data->config->components & PR_COPTIONS_WEB_ADD_PPDS)
  {
    if ((val = cupsGetOption("user-ppd-directory", num_options, options)) !=
	NULL ||
	(val = getenv("USER_PPD_DIR")) != NULL)
      snprintf(global_data->user_ppd_dir, sizeof(global_data->user_ppd_dir),
	       "%s", val);
    else if (!global_data->user_ppd_dir[0])
    {
      if ((ptr = strrchr(global_data->ppd_dirs_list, ':')) != NULL)
	ptr ++;
      else
	ptr = global_data->ppd_dirs_list;
      strncpy(global_data->user_ppd_dir, ptr,
	      sizeof(global_data->user_ppd_dir) - 1);
    }
  }
  else
    global_data->user_ppd_dir[0] = '\0';

  // Spool dir
  if ((val = cupsGetOption("spool-directory", num_options, options)) != NULL ||
      (val = getenv("SPOOL_DIR")) != NULL)
    snprintf(global_data->spool_dir, sizeof(global_data->spool_dir), "%s", val);
  else if (!global_data->spool_dir[0])
    snprintf(global_data->spool_dir, sizeof(global_data->spool_dir),
	     "/var/spool/%s", global_data->config->system_package_name);

  // CUPS filter dir
  if ((val = cupsGetOption("filter-directory", num_options, options)) != NULL ||
      (val = getenv("FILTER_DIR")) != NULL)
    snprintf(global_data->filter_dir, sizeof(global_data->filter_dir), "%s",
	     val);
  else if (!global_data->filter_dir[0])
    snprintf(global_data->filter_dir, sizeof(global_data->filter_dir),
	     "/usr/lib/%s/filter", global_data->config->system_package_name);

  // Set CUPS_SERVERBIN (only if not already set and if FILTER_DIR ends
  // with "/filter"). This gives the best possible environment to the
  // CUPS filters when they are called out of the Printer Application.
  if (getenv("CUPS_SERVERBIN") == NULL && strlen(global_data->filter_dir) > 7)
  {
    strncpy(buf, global_data->filter_dir, sizeof(buf));
    ptr = buf + strlen(buf) - 7;
    if (strcmp(ptr, "/filter") == 0)
    {
      *ptr = '\0';
      setenv("CUPS_SERVERBIN", buf, 1);
    }
  }

  // CUPS Backend dir
  if (global_data->config->components & PR_COPTIONS_CUPS_BACKENDS)
  {
    if ((val = cupsGetOption("backend-directory", num_options, options)) !=
	NULL ||
	(val = getenv("BACKEND_DIR")) != NULL)
      snprintf(global_data->backend_dir, sizeof(global_data->backend_dir), "%s",
	       val);
    else if (!global_data->backend_dir[0])
      snprintf(global_data->backend_dir, sizeof(global_data->backend_dir),
	       "/usr/lib/%s/backend", global_data->config->system_package_name);
  }

  // Test page dir
  if ((val = cupsGetOption("testpage-directory", num_options, options)) !=
      NULL ||
      (val = getenv("TESTPAGE_DIR")) != NULL)
    snprintf(global_data->testpage_dir, sizeof(global_data->testpage_dir), "%s",
	     val);
  else if (!global_data->testpage_dir[0])
    snprintf(global_data->testpage_dir, sizeof(global_data->testpage_dir),
	     "/usr/share/%s", global_data->config->system_package_name);

  // State file
  if ((val = cupsGetOption("state-file", num_options, options)) != NULL ||
      (val = getenv("STATE_FILE")) != NULL)
    snprintf(global_data->state_file, sizeof(global_data->state_file), "%s",
	     val);
  else if (!global_data->state_file[0])
    snprintf(global_data->state_file, sizeof(global_data->state_file),
	     "/var/lib/%s/%s.state", global_data->config->system_package_name,
	     global_data->config->system_package_name);


  // Create the system object...
  if ((system =
       papplSystemCreate(soptions,
			 system_name ? system_name :
			 global_data->config->system_name,
			 port,
			 "_print,_universal",
			 global_data->spool_dir,
			 logfile ? logfile : "-",
			 loglevel,
			 cupsGetOption("auth-service", num_options, options),
			 /* tls_only */false)) ==
      NULL)
    return (NULL);

  global_data->system = system;
  
  papplSystemAddListeners(system, NULL);
  papplSystemSetHostName(system, hostname);
  pr_setup(global_data);

  // Extra setup steps for the system (like adding buttos/pages)
  if (global_data->config->extra_setup_cb)
    (global_data->config->extra_setup_cb)(global_data);

  papplSystemSetFooterHTML(system, global_data->config->web_if_footer);
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState,
			     (void *)(global_data->state_file));
  papplSystemSetVersions(system,
			 (int)(sizeof(versions) / sizeof(versions[0])),
			 versions);

  if (!papplSystemLoadState(system, global_data->state_file))
    papplSystemSetDNSSDName(system,
			    system_name ? system_name :
			    global_data->config->system_name);

  return (system);
}
