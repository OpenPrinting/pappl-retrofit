//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// web-interface.c
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

#include <pappl-retrofit/web-interface-private.h>
#include <pappl-retrofit/pappl-retrofit-private.h>
#include <ppd/ppd.h>
#include <cups/cups.h>


//
// '_prPrinterWebDeviceConfig()' - Web interface page for
//                                 entering/polling the configuration
//                                 of printer add-ons ("Installable
//                                 Options" in PPD and polling default
//                                 option settings
//

void
_prPrinterWebDeviceConfig(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int          i, j, k, l, m;           // Looping variables
  const char   *status = NULL;		// Status text, if any
  const char   *uri = NULL;             // Client URI
  pappl_system_t *system;               // System
  pappl_pr_driver_data_t driver_data;
  ipp_t        *driver_attrs;
  pr_driver_extension_t *extension;
  pr_printer_app_global_data_t *global_data; // Global data
  ppd_file_t   *ppd = NULL;		// PPD file of the printer
  ppd_cache_t  *pc;
  ppd_group_t  *group;
  ppd_option_t *option;
  ppd_choice_t *choice;
  int          default_choice;
  int          num_options = 0;         // Number of polled options
  cups_option_t	*options = NULL;        // Polled options
  cups_option_t *opt;
  bool         polled_installables = false,
               polled_defaults = false;


  if (!papplClientHTMLAuthorize(client))
    return;

  system = papplPrinterGetSystem(printer);
  papplPrinterGetDriverData(printer, &driver_data);
  driver_attrs = papplPrinterGetDriverAttributes(printer);
  extension = (pr_driver_extension_t *)driver_data.extension;
  global_data = extension->global_data;
  ppd = extension->ppd;
  pc = ppd->cache;

  // Handle POSTs to set "Installable Options" and poll default settings...
  if (papplClientGetMethod(client) == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables
    int                 num_installables = 0; // Number of installable options 
    cups_option_t	*installables = NULL; // Set installable options
    int                 num_vendor = 0; // Number of vendor-specific options 
    cups_option_t	*vendor = NULL; // vendor-specific options
    ipp_attribute_t     *attr;
    const char		*action;	// Form action
    char                buf[1024];
    const char          *value;
    char                *ptr1, *ptr2;
    pwg_map_t           *pwg_map;
    pwg_size_t          *pwg_size;
    int                 polled_def_source = -1;
    const char          *polled_def_size = NULL,
                        *polled_def_type = NULL;
    int                 best = 0;
    char                ipp_supported[128],
                        ipp_default[128],
                        ipp_choice[80];
    int                 colormodel_pcm = -1,
                        default_in_presets = 0,
                        default_in_optimize_presets = 0,
                        presets_score[2][3],
                        optimize_presets_score[5],
                        best_score = 0,
                        best_pcm,
                        best_pq;


    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if ((action = cupsGetOption("action", num_form, form)) == NULL)
    {
      status = "Missing action.";
    }
    else if (!strcmp(action, "set-installable"))
    {
      status = "Installable accessory configuration saved.";
      buf[0] = '\0';
      for (i = num_form, opt = form; i > 0; i --, opt ++)
	if (opt->name[0] == '\t')
	{
	  if (opt->name[1] == '\t')
	  {
	    ptr1 = strdup(opt->name + 2);
	    ptr2 = strchr(ptr1, '\t');
	    *ptr2 = '\0';
	    ptr2 ++;
	    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
		     " %s=%s", ptr1, ptr2);
	    num_installables = cupsAddOption(ptr1, ptr2,
					     num_installables, &installables);
	    free(ptr1);
	  }
	  else
	  {
	    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
		     " %s=%s", opt->name + 1, opt->value);
	    num_installables = cupsAddOption(opt->name + 1, opt->value,
					     num_installables, &installables);
	  }
	}
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "\"Installable Options\" from web form:%s", buf);

      buf[0] = '\0';
      for (i = ppd->num_groups, group = ppd->groups;
	   i > 0;
	   i --, group ++)
      {

	// We are treating only the "Installable Options" group of options
	// in the PPD file here

	if (strncasecmp(group->name, "Installable", 11) != 0)
	  continue;

	for (j = group->num_options, option = group->options;
	     j > 0;
	     j --, option ++)
        {
	  // Does the option have less than 2 choices? Then it does not make
	  // sense to let it show in the web interface
	  if (option->num_choices < 2)
	    continue;

	  if ((value = cupsGetOption(option->keyword,
				     num_installables, installables)) == NULL)
	  {
	    // Unchecked check box option
	    if (!strcasecmp(option->choices[0].text, "false"))
	      value = option->choices[0].choice;
	    else if (!strcasecmp(option->choices[1].text, "false"))
	      value = option->choices[1].choice;
	  }
	  ppdMarkOption(ppd, option->keyword, value);
	  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
		   "%s=%s ", option->keyword, value);
	}
	if (buf[0])
	  buf[strlen(buf) - 1] = '\0';
      }
      cupsFreeOptions(num_installables, installables);

      // Update the driver data to only show options and choices which make
      // sense with the current installable accessory configuration
      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
		      "\"Installable Options\" marked in PPD: %s", buf);
      _prPrinterUpdateForInstallableOptions(printer, driver_data, buf);

      // Save the changes
      papplSystemSaveState(system, global_data->state_file);
    }
    else if (!strcmp(action, "poll-installable"))
    {
      // Poll installed options info
      num_options = _prPollDeviceOptionDefaults(printer, true, &options);
      if (num_options)
      {
	status = "Installable accessory configuration polled from printer.";
	polled_installables = true;

	// Join polled settings with current settings and mark them in the PPD
	for (i = num_options, opt = options; i > 0; i --, opt ++)
        {
	  ppdMarkOption(ppd, opt->name, opt->value);
	  extension->num_inst_options =
	    cupsAddOption(opt->name, opt->value,
			  extension->num_inst_options,
			  &extension->inst_options);
        }

	// Create new option string for saving in the state file
	buf[0] = '\0';
	for (i = extension->num_inst_options, opt = extension->inst_options;
	     i > 0; i --, opt ++)
	  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
		   "%s=%s ", opt->name, opt->value);
        if (buf[0])
	  buf[strlen(buf) - 1] = '\0';

	// Update the driver data to only show options and choices which make
	// sense with the current installable accessory configuration
	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			"\"Installable Options\" marked in PPD: %s", buf);
	_prPrinterUpdateForInstallableOptions(printer, driver_data, buf);

	// Save the changes
	papplSystemSaveState(system, global_data->state_file);
      }
      else
	status = "Could not poll installable accessory configuration from "
	         "printer.";
    }
    else if (!strcmp(action, "poll-defaults"))
    {
      // Poll default option values
      num_options = _prPollDeviceOptionDefaults(printer, false,
						&options);
      if (num_options)
      {
	// Read the polled option settings, mark them in the PPD, update them
	// in the printer data and create a summary for logging
	status = "Option defaults polled from printer.";
	polled_defaults = true;
	memset(presets_score, 0, sizeof(presets_score));
	memset(optimize_presets_score, 0, sizeof(optimize_presets_score));

	snprintf(buf, sizeof(buf) - 1, "Option defaults polled from printer:");
	for (i = num_options, opt = options; i > 0; i --, opt ++)
	{
	  ppdMarkOption(ppd, opt->name, opt->value);
	  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
		   " %s=%s", opt->name, opt->value);
	  // Switch color/grayscale
	  if (!strcasecmp(opt->name, "ColorModel") && ppd->color_device)
	  {
	    if (strcasestr(opt->value, "Gray") ||
		strcasestr(opt->value, "Mono") ||
		strcasestr(opt->value, "Black"))
	      colormodel_pcm = PAPPL_COLOR_MODE_MONOCHROME;
	    else if (strcasestr(opt->value, "Color") ||
		     strcasestr(opt->value, "RGB") ||
		     strcasestr(opt->value, "CMY"))
	      colormodel_pcm = PAPPL_COLOR_MODE_COLOR;
	    else
	      colormodel_pcm = PAPPL_COLOR_MODE_AUTO;
	  }
	  // General options
	  if (!strcasecmp(opt->name, "PageSize"))
	  {
	    for (j = 0, pwg_size = pc->sizes; j < pc->num_sizes;
		 j ++, pwg_size ++)
	      if (!strcasecmp(opt->value, pwg_size->map.ppd))
		break;
	    if (j < pc->num_sizes)
	    {
	      for (k = 0; k < driver_data.num_media; k ++)
		if (!strcasecmp(pwg_size->map.pwg, driver_data.media[k]))
		  break;
	      if (k < driver_data.num_media)
		polled_def_size = driver_data.media[k];
	    }
	  }
	  else if (!strcasecmp(opt->name, pc->source_option)) // InputSlot
	  {
	    for (j = 0, pwg_map = pc->sources; j < pc->num_sources;
		 j ++, pwg_map ++)
	      if (!strcasecmp(opt->value, pwg_map->ppd))
		break;
	    if (j < pc->num_sources)
	    {
	      for (k = 0; k < driver_data.num_source; k ++)
		if (!strcasecmp(pwg_map->pwg, driver_data.source[k]))
		  break;
	      if (k < driver_data.num_source)
		polled_def_source = k;
	    }
	  }
	  else if (!strcasecmp(opt->name, "MediaType"))
	  {
	    for (j = 0, pwg_map = pc->types; j < pc->num_types;
		 j ++, pwg_map ++)
	      if (!strcasecmp(opt->value, pwg_map->ppd))
		break;
	    if (j < pc->num_types)
	    {
	      for (k = 0; k < driver_data.num_type; k ++)
		if (!strcasecmp(pwg_map->pwg, driver_data.type[k]))
		  break;
	      if (k < driver_data.num_type)
		polled_def_type = driver_data.type[k];
	    }
	  }
	  else if (!strcasecmp(opt->name, "OutputBin"))
	  {
	    for (j = 0, pwg_map = pc->bins; j < pc->num_bins; j ++, pwg_map ++)
	      if (!strcasecmp(opt->value, pwg_map->ppd))
		break;
	    if (j < pc->num_bins)
	    {
	      for (k = 0; k < driver_data.num_bin; k ++)
		if (!strcasecmp(pwg_map->pwg, driver_data.bin[k]))
		  break;
	      if (k < driver_data.num_bin)
		driver_data.bin_default = k;
	    }
	  }
	  else if (!strcasecmp(opt->name, pc->sides_option)) // Duplex
	  {
	    if (!strcasecmp(opt->value, pc->sides_1sided))
	      driver_data.sides_default = PAPPL_SIDES_ONE_SIDED;
	    else if (!strcasecmp(opt->value, pc->sides_2sided_long))
	      driver_data.sides_default = PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
	    else if (!strcasecmp(opt->value, pc->sides_2sided_short))
	      driver_data.sides_default = PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
	  }
	  else if (strcasecmp(opt->name, "PageRegion"))
	  {
	    // Vendor options
	    for (j = 0; j < driver_data.num_vendor; j ++)
	    {
	      if (extension->vendor_ppd_options[j] &&
		  (!strcasecmp(opt->name, extension->vendor_ppd_options[j]) ||
		   (extension->vendor_ppd_options[j][0] == '/' &&
		    !strcasecmp(opt->name,
				extension->vendor_ppd_options[j] + 1))))
	      {
		if ((option = ppdFindOption(ppd, opt->name)) != NULL &&
		    (choice = ppdFindChoice(option, opt->value)) != NULL)
		{
		  snprintf(ipp_supported, sizeof(ipp_supported),
			   "%s-supported", driver_data.vendor[j]);
		  if ((attr = ippFindAttribute(driver_attrs, ipp_supported,
					       IPP_TAG_ZERO)) != NULL)
		  {
		    if (ippGetValueTag(attr) == IPP_TAG_BOOLEAN)
		    {
		      if (!strcasecmp(choice->text, "True"))
			num_vendor = cupsAddOption(driver_data.vendor[j],
						   "true", num_vendor, &vendor);
		      else if (!strcasecmp(choice->text, "False"))
			num_vendor = cupsAddOption(driver_data.vendor[j],
						   "false",
						   num_vendor, &vendor);
		    }
		    else
		    {
		      snprintf(ipp_default, sizeof(ipp_default),
			       "%s-default", driver_data.vendor[j]);
		      if ((attr = ippFindAttribute(driver_attrs, ipp_default,
						   IPP_TAG_ZERO)) != NULL &&
			  (ptr1 = (char *)ippGetString(attr, 0, NULL)) !=
			  NULL &&
			  strcasecmp(ptr1, "automatic-selection") == 0)
		      {
			// Do not switch a preset-conntrolled option to
			// manual (away from "automatic-selection")
			// Instead, search for the setting in the preset
			// and increase the score for each preset which
			// contains the setting. In the end we switch to the
			// preset with the highest score
			for (k = 0; k < 2; k ++)
			  for (l = 0; l < 3; l ++)
			    for (m = 0; m < pc->num_presets[k][l]; m ++)
			      if (strcasecmp(opt->name,
					     pc->presets[k][l][m].name) == 0 &&
				  strcasecmp(opt->value,
					     pc->presets[k][l][m].value) == 0)
			      {
				default_in_presets = 1;
				presets_score[k][l] ++;
				if (strcasecmp(opt->name, "Resolution") == 0)
				  presets_score[k][l] += 2;
				papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
						"%s=%s in preset [%d][%d] -> Score: %d",
						opt->name, opt->value, k, l,
						presets_score[k][l]);
			      }
			for (k = 0; k < 5; k ++)
			  for (m = 0; m < pc->num_optimize_presets[k]; m ++)
			    if (strcasecmp(opt->name,
					   pc->optimize_presets[k][m].name) ==
				0 &&
				strcasecmp(opt->value,
					   pc->optimize_presets[k][m].value) ==
				0)
			    {
			      default_in_optimize_presets = 1;
			      optimize_presets_score[k] ++;
			      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
					      "%s=%s in optimize preset [%d] -> Score: %d",
					      opt->name, opt->value, k,
					      optimize_presets_score[k]);
			    }
		      }
		      else
		      {
			ppdPwgUnppdizeName(choice->text, ipp_choice,
					   sizeof(ipp_choice), NULL);
			num_vendor = cupsAddOption(driver_data.vendor[j],
						   ipp_choice,
						   num_vendor, &vendor);
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}

	// Media Source
	if (polled_def_source < 0)
	{
	  if (polled_def_size || polled_def_type)
	    for (i = 0; i < driver_data.num_source; i ++)
	    {
	      j = 0;
	      if (polled_def_size &&
		  !strcasecmp(polled_def_size,
			      driver_data.media_ready[i].size_name))
		j += 2;
	      if (polled_def_type &&
		  !strcasecmp(polled_def_type,
			      driver_data.media_ready[i].type))
		j += 1;
	      if (j > best)
	      {
		best = j;
		driver_data.media_default = driver_data.media_ready[i];
	      }
	    }
	}
	else
	  driver_data.media_default =
	    driver_data.media_ready[polled_def_source];

	// print-color-mode and print-quality
	if (default_in_presets)
	{
	  // Find the highest score and take the first preset
	  // with this score. Go through the presets starting
	  // with the current default of print-color-mode and
	  // print-quality, to keep the current default if they
	  // are already with the highest score.
	  // This is the minimum change which best matches the
	  // polled settings.
	  for (i = 0; i < (ppd->color_device ? 2 : 1); i ++)
	  {
	    j = (!ppd->color_device ||
		 driver_data.color_default ==
		 PAPPL_COLOR_MODE_MONOCHROME ? i : 1 - i);
	    for (k = 0; k < 3; k ++)
	    {
	      if (k == 0)
		l = driver_data.quality_default - 3;
	      else if (k == 1)
	      {
		if (driver_data.quality_default != 4)
		  l = 1;
		else
		  l = 2;
	      }
	      else
	      {
		if (driver_data.quality_default != 3)
		  l = 0;
		else
		  l = 2;
	      }
	      if (presets_score[j][l] > best_score)
	      {
		best_score = presets_score[j][l];
		best_pcm = j;
		best_pq = l;
	      }
	    }
	  }
	  if (best_score > 0)
	  {
	    driver_data.color_default = (best_pcm ? PAPPL_COLOR_MODE_COLOR :
					 PAPPL_COLOR_MODE_MONOCHROME);
	    driver_data.quality_default = best_pq + 3;
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "To best match the polled default settings of the printer, set print-color-mode to %s and print-quality to %s",
			    (driver_data.color_default ==
			     PAPPL_COLOR_MODE_MONOCHROME ?
			     "Grayscale" : "Color"),
			    (driver_data.quality_default == IPP_QUALITY_DRAFT ?
			     "Draft" :
			     (driver_data.quality_default ==
			      IPP_QUALITY_NORMAL ?
			      "Normal" : "High")));
	  }
	}
	if ((!default_in_presets || !best_score) && colormodel_pcm > 0)
	{
	  driver_data.color_default = colormodel_pcm;
	  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			  "To best match the polled default setting of the ColorModel option, set print-color-mode to %s",
			  (driver_data.color_default ==
			   PAPPL_COLOR_MODE_MONOCHROME ? "Grayscale" :
			   (driver_data.color_default ==
			    PAPPL_COLOR_MODE_COLOR ? "Color" : "Auto")));
	}

	// print-content-optimize
	best_score = 0;
	if (default_in_optimize_presets)
	{
	  for (i = 0; i < 5; i ++)
	    if (optimize_presets_score[i] > best_score)
	    {
	      best_score = optimize_presets_score[i];
	      j = i;
	    }
	  if (best_score > 0)
	  {
	    driver_data.content_default =
	      (j == 0 ? PAPPL_CONTENT_AUTO :
	       (j == 1 ? PAPPL_CONTENT_PHOTO :
		(j == 2 ? PAPPL_CONTENT_GRAPHIC :
		 (j == 3 ? PAPPL_CONTENT_TEXT :
		  PAPPL_CONTENT_TEXT_AND_GRAPHIC))));
	    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG,
			    "To best match the polled default settings of the printer, set print-content-optimize to %s",
			    (j == 0 ? "Auto" :
			     (j == 1 ? "Photo" :
			      (j == 2 ? "Graphics" :
			       (j == 3 ? "Text" :
				"Text and graphics")))));
	  }
	}

	papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "%s", buf);

	// Submit the changed default values
	papplPrinterSetDriverDefaults(printer, &driver_data,
				      num_vendor, vendor);

	// Clean up
	if (num_vendor)
	  cupsFreeOptions(num_vendor, vendor);
      }
      else
	status = "Could not poll option defaults from printer.";
    }
    else
      status = "Unknown action.";

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, "Printer Device Settings", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  uri = papplClientGetURI(client);

  if (extension->installable_options)
  {
    papplClientHTMLPuts(client,
			"          <h3>Installable printer accessories</h3>\n");
    if (polled_installables)
      papplClientHTMLPuts(client,
			  "          <br>Settings obtained from polling the printer are marked with an asterisk (\"*\")</br>\n");

    papplClientHTMLStartForm(client, uri, false);

    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n");

    for (i = ppd->num_groups, group = ppd->groups;
	 i > 0;
	 i --, group ++)
    {

      // We are treating only the "Installable Options" group of options
      // in the PPD file here

      if (strncasecmp(group->name, "Installable", 11) != 0)
	continue;

      for (j = group->num_options, option = group->options;
	   j > 0;
	   j --, option ++)
      {
	// Does the option have less than 2 choices? Then it does not make
	// sense to let it show in the web interface
	if (option->num_choices < 2)
	  continue;

	papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>",
			      option->text);

	if (option->num_choices == 2 &&
	    ((!strcasecmp(option->choices[0].text, "true") &&
	      !strcasecmp(option->choices[1].text, "false")) ||
	     (!strcasecmp(option->choices[0].text, "false") &&
	      !strcasecmp(option->choices[1].text, "true"))))
	{
	  // Create a check box widget, as human-readable choices "true"
	  // and "false" are not very user-friendly
	  default_choice = 0;
	  for (k = 0; k < 2; k ++)
	    if (!strcasecmp(option->choices[k].text, "true"))
	    {
	      if (option->choices[k].marked)
		default_choice = 1;
	      // Stop here to make k be the index of the "True" value of this
	      // option so that we can extract its machine-readable value
	      break;
	    }
	  // We precede the option name with a two tabs to mark it as an
	  // option represented by a check box, we also add the machine-
	  // readable choice name for "True" (checked). This way we can treat
	  // the result correctly, taking into account that nothing for this
	  // option gets submitted when the box is unchecked.
	  papplClientHTMLPrintf(client,
				"<input type=\"checkbox\" name=\"\t\t%s\t%s\"%s>",
				option->keyword, option->choices[k].choice,
				default_choice == 1 ? " checked" : "");
	}
	else
	{
	  // Create a drop-down widget
	  // We precede the option name with a single tab to tell that this
	  // option comes from a drop-down. The drop-down choice always gets
	  // submitted, so the option name in the name field is enough for
	  // parsing the submitted result.
	  // The tab in the beginning also assures that the PPD option names
	  // never conflict with fixed option names of this function, like
	  // "action" or "session".
	  papplClientHTMLPrintf(client, "<select name=\"\t%s\">",
				option->keyword);
	  default_choice = 0;
	  for (k = 0; k < option->num_choices; k ++)
	    papplClientHTMLPrintf(client,
				  "<option value=\"%s\"%s>%s</option>",
				  option->choices[k].choice,
				  option->choices[k].marked ? " selected" : "",
				  option->choices[k].text);
	  papplClientHTMLPuts(client, "</select>");
	}

	// Mark options which got updated by polling from the printer with
	// an asterisk
	papplClientHTMLPrintf(client, "%s",
			      (polled_installables &&
			       cupsGetOption(option->keyword, num_options,
					     options) != NULL ? " *" : ""));

	papplClientHTMLPuts(client, "</td></tr>\n");
      }
    }
    papplClientHTMLPuts(client,
			"              <tr><th></th><td><button type=\"submit\" name=\"action\" value=\"set-installable\">Set</button>");
    if (extension->installable_pollable)
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "\n          &nbsp;<button type=\"submit\" name=\"action\" value=\"poll-installable\">Poll from printer</button>\n");
    }
    papplClientHTMLPuts(client,
			"</td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n");

  }

  if (extension->installable_options &&
      extension->defaults_pollable)
    papplClientHTMLPrintf(client, "          <hr>\n");

  if (extension->defaults_pollable)
  {
    papplClientHTMLPuts(client,
			"          <h3>Poll printing defaults from the printer</h3>\n");

    papplClientHTMLPuts(client,
			"          <p>Note that settings polled from the printer overwrite your original settings.</p>\n");
    if (polled_defaults)
      papplClientHTMLPuts(client,
			  "          <br>Polling results:</br>\n");

    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n");

    // If we have already polled and display the page again, show poll
    // results as options which got updated by polling with their
    // values (they are in list "options").
    if (polled_defaults && num_options)
      for (i = num_options, opt = options; i > 0; i --, opt ++)
	if ((option = ppdFindOption(ppd, opt->name)) != NULL &&
	    (choice = ppdFindChoice(option, opt->value)) != NULL)
	  papplClientHTMLPrintf(client,
				"              <tr><th>%s:</th><td>%s</td></tr>\n",
				option->text, choice->text);

    papplClientHTMLPrintf(client, "          <tr><th></th><td><input type=\"hidden\" name=\"action\" value=\"poll-defaults\"><input type=\"submit\" value=\"%s\"></td>\n",
			  (polled_defaults ? "Poll again" : "Poll"));

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n");
  }

  papplClientHTMLPrinterFooter(client);

  // Clean up
  ippDelete(driver_attrs);
  if (num_options)
    cupsFreeOptions(num_options, options);
}


//
// '_prSystemWebAddPPD()' - Web interface page for adding/deleting PPD
//                          files by the user, to add support for
//                          printers not supported by the built-in PPD
//                          files
//

void
_prSystemWebAddPPD(
    pappl_client_t *client,		// I - Client
    void *data)                         // I - Global data
{
  int                 i, j;             // Looping variables
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pappl_system_t      *system = global_data->system; // System
  const char          *status = NULL;	// Status text, if any
  const char          *uri = NULL;      // Client URI
  pappl_version_t     version;
  cups_array_t        *uploaded,        // List of uploaded PPDs, to delete
                                        // them in certain error conditions
                      *accepted_report, // Report of accepted PPDs with warnings
                      *rejected_report; // Report of rejected PPDs with errors
  cups_dir_t          *dir;             // User PPD file directory
  cups_dentry_t       *dent;            // Entry in user PPD file directory
  cups_array_t        *user_ppd_files;  // List of user-uploaded PPD files


  if (!papplClientHTMLAuthorize(client))
    return;

  // Create arrays to log PPD file upload
  uploaded = cupsArrayNew3(NULL, NULL, NULL, 0, NULL,
			   (cups_afree_func_t)free);
  accepted_report = cupsArrayNew3(NULL, NULL, NULL, 0, NULL,
				  (cups_afree_func_t)free);
  rejected_report = cupsArrayNew3(NULL, NULL, NULL, 0, NULL,
				  (cups_afree_func_t)free);

  // Handle POSTs to add and delete PPD files...
  if (papplClientGetMethod(client) == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables
    cups_option_t	*opt;
    const char		*action;	// Form action
    char                strbuf[2048],
                        destpath[2048];	// File destination path
    const char	        *content_type;	// Content-Type header
    const char	        *boundary;	// boundary value for multi-part
    http_t              *http;
    bool                error = false;
    bool                ppd_repo_changed = false; // PPD(s) added or removed?
    char		*ptr;		// Pointer into string


    http = papplClientGetHTTP(client);
    content_type = httpGetField(http, HTTP_FIELD_CONTENT_TYPE);
    if (!strcmp(content_type, "application/x-www-form-urlencoded"))
    {
      // URL-encoded form data, PPD file uploads not possible in this format 
      // Use papplClientGetForm() to do the needed decoding
      error = true;
      if ((num_form = papplClientGetForm(client, &form)) == 0)
      {
	status = "Invalid form data.";
      }
      else if (!papplClientIsValidForm(client, num_form, form))
      {
	status = "Invalid form submission.";
      }
      else
	error = false;
    }
    else if (!strncmp(content_type, "multipart/form-data; ", 21) &&
	     (boundary = strstr(content_type, "boundary=")) != NULL)
    {
      // Multi-part form data, probably we have a PPD file upload
      // Use our own reading method to allow for submitting more than
      // one PPD file through the single input widget and for a larger
      // total amount of data
      char	buf[32768],		// Message buffer
		*bufinptr,		// Pointer end of incoming data
		*bufreadptr,		// Pointer for reading buffer
		*bufend;		// End of message buffer
      size_t	body_size = 0;		// Size of message body
      ssize_t	bytes;			// Bytes read
      http_state_t initial_state;	// Initial HTTP state
      char	name[1024],		// Form variable name
		filename[1024],		// Form filename
		bstring[256],		// Boundary string to look for
		*bend,			// End of value (boundary)
		*line;			// Start of line
      size_t	blen;			// Length of boundary string
      FILE      *fp = NULL;
      ppd_file_t *ppd = NULL;		// PPD file data for verification
      ppd_group_t *group;
      ppd_option_t *option;
      bool      codeless_option_found;
      bool      pagesize_option_ok;
      bool      check_options;
      char      *missing_filters = NULL;
      char      *warn_opt_part;


      // Read one buffer full of data, then search for \r only up to a
      // position in the buffer so that the boundary string still fits
      // after the \r, check for line end \r\n (in header) or boundary
      // string (after header, file/value), then save value into
      // "form" option list or file data into destination file, move
      // rest of buffer content (after line break/boundary) to
      // beginning of buffer, read rest of buffer space full,
      // continue. If no line end found, error (too long line), if no
      // boundary found, error in case of option value, write to
      // destination file in case of file. Move rest of buffer content
      // to the beginning of buffer, read buffer full, continue.
      initial_state = httpGetState(http);

      // Format the boundary string we are looking for...
      snprintf(bstring, sizeof(bstring), "\r\n--%s", boundary + 9);
      blen = strlen(bstring);
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
		     "Boundary string: \"%s\", %ld bytes",
		     bstring, (long)blen);

      // Parse lines in the message body...
      name[0] = '\0';
      filename[0] = '\0';

      for (bufinptr = buf, bufend = buf + sizeof(buf);
	   (bytes = httpRead2(http, bufinptr,
			      (size_t)(bufend - bufinptr))) > 0 ||
	     bufinptr > buf;)
      {
	body_size += (size_t)bytes;
	papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
		       "Bytes left over: %ld; Bytes read: %ld; Total bytes read: %ld",
		       (long)(bufinptr - buf), (long)bytes, (long)body_size);
	bufinptr += bytes;
	*bufinptr = '\0';

	for (bufreadptr = buf; bufreadptr < bufinptr;)
        {
	  if (fp == NULL)
	  {
	    // Split out a line...
	    for (line = bufreadptr; bufreadptr < bufinptr - 1; bufreadptr ++)
	    {
	      if (!memcmp(bufreadptr, "\r\n", 2))
	      {
		*bufreadptr = '\0';
		bufreadptr += 2;
		break;
	      }
	    }

	    if (bufreadptr >= bufinptr)
	      break;
	  }

	  if (!*line || fp)
          {
	    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			   "Data (value or file).");

	    // End of headers, grab value...
	    if (!name[0])
	    {
	      // No name value...
	      papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
			     "Invalid multipart form data: Form field name missing.");
	      // Stop here
	      status = "Invalid form data.";
	      error = true;
	      break;
	    }

	    for (bend = bufinptr - blen - 2,
		   ptr = memchr(bufreadptr, '\r', (size_t)(bend - bufreadptr));
		 ptr;
		 ptr = memchr(ptr + 1, '\r', (size_t)(bend - ptr - 1)))
	    {
	      // Check for boundary string...
	      if (!memcmp(ptr, bstring, blen))
		break;
	    }

	    if (!ptr && !filename[0])
	    {
	      // When reading a file, write out curremt data into destination
	      // file, when not reading a file, error out
	      // No boundary string, invalid data...
	      papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
			     "Invalid multipart form data: Form field %s: File without filename or excessively long value.",
			     name);
	      // Stop here
	      status = "Invalid form data.";
	      error = true;
	      break;
	    }

	    // Value/file data is in buffer range from ptr to bend,
	    // Reading continues at bufreadptr
	    if (ptr)
	    {
	      bend       = ptr;
	      ptr        = bufreadptr;
	      bufreadptr = bend + blen;
	    }
	    else
	    {
	      ptr        = bufreadptr;
	      bufreadptr = bend;
	    }

	    if (filename[0])
	    {
	      // Save data of an embedded file...

	      // New file
	      if (fp == NULL) // First data portion
	      {
		snprintf(destpath, sizeof(destpath), "%s/%s",
			 global_data->user_ppd_dir, filename);
		papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			       "Creating file: %s", destpath);
		if ((fp = fopen(destpath, "w+")) == NULL)
		{
		  papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
				 "Unable to create file: %s",
				 strerror(errno));
		  // Report error
		  snprintf(strbuf, sizeof(strbuf),
			   "%s: Cannot create file - %s",
			   filename, strerror(errno));
		  cupsArrayAdd(rejected_report, strdup(strbuf));
		  // Stop here
		  status = "Error uploading PPD file(s), uploading stopped.";
		  error = true;
		  break;
		}
	      }

	      if (fp)
	      {
		// Write the data
		while (bend > ptr) // We have data to write
		{
		  bytes = fwrite(ptr, 1, (size_t)(bend - ptr), fp);
		  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
				 "Bytes to write: %ld; %ld bytes written",
				 (long)(bend - ptr), (long)bytes);
		  if (errno)
		  {
		    papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
				   "Error writing into file %s: %s",
				   destpath, strerror(errno));
		    // Report error
		    snprintf(strbuf, sizeof(strbuf),
			     "%s: Cannot write file - %s",
			     filename, strerror(errno));
		    cupsArrayAdd(rejected_report, strdup(strbuf));
		    // PPD incomplete, close and delete it.
		    fclose(fp);
		    fp = NULL;
		    unlink(destpath);
		    // Stop here
		    status = "Error uploading PPD file(s), uploading stopped.";
		    error = true;
		    break;
		  }
		  ptr += bytes;
		}

		// Close the file and verify whether it is a usable PPD file
		if (bufreadptr > bend) // We have read the boundary string
		{
		  fclose(fp);
		  fp = NULL;
		  // Default is PPD_CONFORM_RELAXED, uncomment if not
		  // perfectly conforming PPD files cause problems
		  //ppdSetConformance(PPD_CONFORM_STRICT);
		  if ((ppd = ppdOpenFile(destpath)) == NULL)
		  {
		    ppd_status_t err;		// Last error in file
		    int		 linenum;	// Line number in file

		    // Log error
		    err = ppdLastError(&linenum);
		    papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
				   "PPD %s: %s on line %d", destpath,
				   ppdErrorString(err), linenum);
		    // PPD broken (or not a PPD), delete it.
		    unlink(destpath);
		    // Report error
		    snprintf(strbuf, sizeof(strbuf),
			     "%s: Not a PPD or file corrupted", filename);
		    cupsArrayAdd(rejected_report, strdup(strbuf));
		  }
		  else
		  {
		    // Check for cupsFilter(2) entries with filters
		    // which are not installed and warn id something
		    // is missing.
		    //
		    // If there is only one single cupsFilter(2) line
		    // with PostScript as input format or no
		    // cupsFilter(2) line at all, assume that this PPD
		    // is for a PostScript printer. If in this case
		    // the specified filter is missing or there is no
		    // cupsFilter(2) line also warn about options
		    // which do not have PostScript or PJL code in the
		    // PPD as they will not work without filter.
		    //
		    // We do not reject these PPDs right away, as there
		    // are some PostScript PPDs which use a filter only
		    // to implement some non-essential options. Therefore
		    // we issue a warning in such a case.
		    //
		    // If all the filters the PPD mentions in its
		    // cupsFilter(2) entries and/or if the PPD is not
		    // for a PostScript printer we do not worry about
		    // options with unsufficient PostScript or PJL
		    // code.
		    cups_array_t *report;
		    cups_array_t *file_array;
		    int result;

		    cupsArrayAdd(file_array, destpath);
		    result = ppdTest(0,0,NULL,0,0,0,0,0,0,1,file_array,&report);

                    if (report)
                    {
                      for (line = (char *)cupsArrayFirst(report); line; line = (char *)cupsArrayNext(report))
                      {
                        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, line);
			if (result == 1)
		          cupsArrayAdd(accepted_report, strdup(strbuf));
			else
			  cupsArrayAdd(rejected_report, strdup(strbuf));
                       }
                     }

			
		    check_options = false;
		    warn_opt_part = NULL;
		    if (ppd->num_filters)
		    {
		      missing_filters =
			_prPPDMissingFilters
			  (ppd->num_filters, ppd->filters,
			   global_data->filter_dir);
		      if (missing_filters)
		      {
			snprintf(strbuf, sizeof(strbuf),
				 "%s: WARNING: PPD needs filters which are not installed: %s",
				 filename, missing_filters);
			free(missing_filters);
			if (ppd->num_filters == 1 &&
			    strncmp(ppd->filters[0],
				    "application/vnd.cups-postscript",
				    strlen("application/vnd.cups-postscript"))
			    == 0)
			{
			  check_options = true;
			  warn_opt_part = strbuf + strlen(strbuf);
			  snprintf(strbuf + strlen(strbuf),
				   sizeof(strbuf) - strlen(strbuf) - 1,
				   "; Options which will possibly not work:");
			}
		      }
		    }
		    else
		    {  
		      check_options = true;
		      snprintf(strbuf, sizeof(strbuf),
			       "%s: WARNING: Options which will possibly not work:",
			       filename);
		    }
		    codeless_option_found = false;
		    pagesize_option_ok = false;
		    for (i = ppd->num_groups, group = ppd->groups;
			 i > 0;
			 i --, group ++)
		    {
		      // Skip "Installable Options" they do not need code
		      if (!strncasecmp(group->name, "Installable", 11) != 0)
			continue;
		      for (j = group->num_options, option = group->options;
			   j > 0;
			   j --, option ++)
		      {
			// Less than 2 choices, do not consider
			if (option->num_choices < 2)
			  continue;
			// Skip "PageRegion"
			if (!strcasecmp(option->keyword, "PageRegion"))
			  continue;
			// Do the choices provide code? If not, warn about
			// this option not to work
			// Don't worry if the filter is installed
			if (check_options &&
			    !_prOptionHasCode(system, ppd, option))
			{
			  codeless_option_found = true;
			  snprintf(strbuf + strlen(strbuf),
				   sizeof(strbuf) - strlen(strbuf) - 1,
				   " %s,",
				   option->text[0] ? option->text :
				   option->keyword);
			} else if (!strcasecmp(option->keyword, "PageSize"))
			  pagesize_option_ok = true;
		      }
		    }
		    if (codeless_option_found)
		      strbuf[strlen(strbuf) - 1] = '\0';
		    else if (warn_opt_part)
		    {
		      // Remove warning about options but keep part about
		      // missing filters
		      *warn_opt_part = '\0';
		    }
		    else
		    {
		      // Log success without warning
		      snprintf(strbuf, sizeof(strbuf), "%s: OK", filename);
		    }
		    ppdClose(ppd);
		    if (pagesize_option_ok)
		    {
		      cupsArrayAdd(accepted_report, strdup(strbuf));
		      // New PPD added, so driver list needs update
		      ppd_repo_changed = true;
		      // Log the addtion of the PPD file
		      cupsArrayAdd(uploaded, strdup(destpath));
		    }
		    else
		    {
		      // PPD for sure broken, delete it.
		      unlink(destpath);
		      // Report error
		      if (missing_filters)
			snprintf(strbuf + strlen(strbuf),
				 sizeof(strbuf) - strlen(strbuf) - 1,
				 "  \"PageSize\" option does not work without filter, PPD will not work");
		      else
			snprintf(strbuf, sizeof(strbuf),
				 "%s: No valid \"PageSize\" option, PPD will not work", filename);
		      cupsArrayAdd(rejected_report, strdup(strbuf));
		    }
		  }
		  //ppdSetConformance(PPD_CONFORM_RELAXED);
		}
	      }
	    }
	    else
	    {
	      // Save the form variable...
	      *bend = '\0';
	      num_form = cupsAddOption(name, ptr, num_form, &form);
	      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			     "Form variable: %s=%s", name, ptr);

	      // If we have found the session ID (comes before the first PPD
	      // file), verify it
	      if (!strcasecmp(name, "session") &&
		  !papplClientIsValidForm(client, num_form, form))
	      {
		papplLogClient(client, PAPPL_LOGLEVEL_ERROR,
			       "Invalid session ID: %s",
			       ptr);
		// Remove already uploaded PPD files ...
		while ((ptr = cupsArrayFirst(uploaded)))
		{
		  unlink(ptr);
		  cupsArrayRemove(uploaded, ptr);
		}
		// ... and the reports about them
		while (cupsArrayRemove(accepted_report,
				       cupsArrayFirst(accepted_report)));
		while (cupsArrayRemove(rejected_report,
				       cupsArrayFirst(rejected_report)));
		// Stop here
		status = "Invalid form submission.";
		error = true;
		break;
	      }
	    }

	    if (fp == NULL)
	    {
	      name[0]     = '\0';
	      filename[0] = '\0';

	      if (bufreadptr < (bufinptr - 1) &&
		  bufreadptr[0] == '\r' && bufreadptr[1] == '\n')
		bufreadptr += 2;
	    }

	    break;
	  }
	  else
	  {
	    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Line '%s'.", line);

	    if (!strncasecmp(line, "Content-Disposition:", 20))
            {
	      if ((ptr = strstr(line + 20, " name=\"")) != NULL)
	      {
		strncpy(name, ptr + 7, sizeof(name) - 1);

		if ((ptr = strchr(name, '\"')) != NULL)
		  *ptr = '\0';
	      }

	      if ((ptr = strstr(line + 20, " filename=\"")) != NULL)
	      {
		strncpy(filename, ptr + 11, sizeof(filename) - 1);

		if ((ptr = strchr(filename, '\"')) != NULL)
		  *ptr = '\0';
	      }
	      if (filename[0])
		papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			       "Found file from form field \"%s\" with file "
			       "name \"%s\"",
			       name, filename);
	      else
		papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			       "Found value for field \"%s\"", name);
	    }

	    break;
	  }
	}

	if (error)
	  break;

	if (bufinptr > bufreadptr)
	{
	  memmove(buf, bufreadptr, (size_t)(bufinptr - bufreadptr));
	  bufinptr = buf + (bufinptr - bufreadptr);
	}
	else
	  bufinptr = buf;
      }
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
		     "Read %ld bytes of form data (%s).",
		     (long)body_size, content_type);

      // Flush remaining data...
      if (httpGetState(http) == initial_state)
	httpFlush(http);
    }

    strbuf[0] = '\0';
    for (i = num_form, opt = form; i > 0; i --, opt ++)
      snprintf(strbuf + strlen(strbuf), sizeof(strbuf) - strlen(strbuf) - 1,
	       "%s=%s ", opt->name, opt->value);
    if (strbuf[0])
      strbuf[strlen(strbuf) - 1] = '\0';

    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
		   "Form variables: %s", strbuf);

    // Check non-file form input values
    if (!error)
    {
      if ((action = cupsGetOption("action", num_form, form)) == NULL)
      {
	status = "Missing action.";
	error = true;
      }
      else if (!strcmp(action, "add-ppdfiles"))
	status = "PPD file(s) uploaded.";
      else if (!strcmp(action, "delete-ppdfiles"))
      {
	for (i = num_form, opt = form; i > 0;
	     i --, opt ++)
	  if (opt->name[0] == '\t')
	  {
	    snprintf(destpath, sizeof(destpath), "%s/%s",
		     global_data->user_ppd_dir, opt->name + 1);
	    papplLogClient(client, PAPPL_LOGLEVEL_DEBUG,
			   "Deleting file: %s", destpath);
	    unlink(destpath);
	    ppd_repo_changed = true;
	  }
	if (ppd_repo_changed)
	  status = "PPD file(s) deleted.";
	else
	  status = "No PPD file selected for deletion.";
      }
      else if (!strcmp(action, "refresh-ppdfiles"))
      {
	ppd_repo_changed = true;
	status = "Driver list refreshed.";
      }
      else
      {
	status = "Unknown action.";
	error = true;
      }
      if (error)
      {
	// Remove already uploaded PPD files ...
	while ((ptr = cupsArrayFirst(uploaded)))
	{
	  unlink(ptr);
	  cupsArrayRemove(uploaded, ptr);
	}
	// ... and the reports about them
	while (cupsArrayRemove(accepted_report,
			       cupsArrayFirst(accepted_report)));
	while (cupsArrayRemove(rejected_report,
			       cupsArrayFirst(rejected_report)));
      }
    }

    // Refresh driver list (if at least 1 PPD got added or removed)
    if (ppd_repo_changed)
      _prSetupDriverList(global_data);

    cupsFreeOptions(num_form, form);
  }

  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    goto clean_up;
  papplClientHTMLHeader(client, "Add support for extra printers", 0);
  if (papplSystemGetVersions(system, 1, &version) > 0)
    papplClientHTMLPrintf(client,
                          "    <div class=\"header2\">\n"
                          "      <div class=\"row\">\n"
                          "        <div class=\"col-12 nav\">\n"
                          "          Version %s\n"
                          "        </div>\n"
                          "      </div>\n"
                          "    </div>\n", version.sversion);
  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-12\">\n"
			"          <h1 class=\"title\">Add support for extra printer models</h1>\n");

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  papplClientHTMLPuts(client,
		      "        <h3>Add the PPD file(s) of your printer(s)</h3>\n");
  papplClientHTMLPuts(client,
		      "        <p>If your printer is not already supported by this Printer Application, you can add support for it by uploading your printer's PPD file here.</p>\n");

  uri = papplClientGetURI(client);

  // Multi-part, as we want to upload a PPD file here
  papplClientHTMLStartForm(client, uri, true);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  if (cupsArrayCount(rejected_report))
  {
    for (i = 0; i < cupsArrayCount(rejected_report); i ++)
      papplClientHTMLPrintf(client,
			    (i == 0 ?
			     "              <tr><th>Upload&nbsp;failed:</th><td>%s</td></tr>\n" :
			     "              <tr><th></th><td>%s</td></tr>\n"),
			    (char *)cupsArrayIndex(rejected_report, i));
    papplClientHTMLPuts(client,
			"              <tr><th></th><td></td></tr>\n");
  }
  if (cupsArrayCount(accepted_report))
  {
    for (i = 0; i < cupsArrayCount(accepted_report); i ++)
      papplClientHTMLPrintf(client,
			    (i == 0 ?
			     "              <tr><th>Uploaded:</th><td>%s</td></tr>\n" :
			     "              <tr><th></th><td>%s</td></tr>\n"),
			    (char *)cupsArrayIndex(accepted_report, i));
    papplClientHTMLPuts(client,
			"              <tr><th></th><td></td></tr>\n");
  }
  papplClientHTMLPuts(client,
		      "              <tr><th><label for=\"ppdfiles\">PPD&nbsp;file(s):</label></th><td><input type=\"file\" name=\"ppdfiles\" accept=\".ppd,.PPD,.ppd.gz,.PPD.gz\" required multiple></td><td>(Only individual PPD files, no PPD-generating executables)</td></tr>\n");
  papplClientHTMLPuts(client,
		      "              <tr><th></th><td><button type=\"submit\" name=\"action\" value=\"add-ppdfiles\">Add PPDs</button></td><td></td></tr>\n");
  papplClientHTMLPuts(client,
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </form>\n");

  if ((dir = cupsDirOpen(global_data->user_ppd_dir)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_WARN,
	     "Unable to read user PPD directory '%s': %s",
	     global_data->user_ppd_dir, strerror(errno));
  }
  else
  {
    user_ppd_files = cupsArrayNew3((cups_array_func_t)strcasecmp,
				   NULL, NULL, 0, NULL,
				   (cups_afree_func_t)free);
    while ((dent = cupsDirRead(dir)) != NULL)
      if (!S_ISDIR(dent->fileinfo.st_mode) &&
	  dent->filename[0] && dent->filename[0] != '.' &&
	  (!strcasecmp(dent->filename + strlen(dent->filename) - 4, ".ppd") ||
	   !strcasecmp(dent->filename + strlen(dent->filename) - 7, ".ppd.gz")))
	cupsArrayAdd(user_ppd_files, strdup(dent->filename));

    cupsDirClose(dir);

    if (cupsArrayCount(user_ppd_files))
    {
      papplClientHTMLPrintf(client, "          <hr>\n");

      papplClientHTMLPuts(client,
			  "          <h3>Already uploaded PPD files</h3>\n");

      papplClientHTMLPuts(client,
			  "          <p>To remove files, mark them and click the \"Delete\" button</p>\n");

      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPuts(client,
			  "          <table class=\"form\">\n"
			  "            <tbody>\n");

      for (i = 0; i < cupsArrayCount(user_ppd_files); i ++)
	papplClientHTMLPrintf(client,
			      "              <tr><th><input type=\"checkbox\" name=\"\t%s\"></th><td>%s</td></tr>\n",
			      (char *)cupsArrayIndex(user_ppd_files, i),
			      (char *)cupsArrayIndex(user_ppd_files, i));

      papplClientHTMLPuts(client, "          <tr><th></th><td><input type=\"hidden\" name=\"action\" value=\"delete-ppdfiles\"><input type=\"submit\" value=\"Delete\"></td>\n");

      papplClientHTMLPuts(client,
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </form>\n");
    }

    cupsArrayDelete(user_ppd_files);
  }

  papplClientHTMLPrintf(client, "          <hr>\n");

  papplClientHTMLPuts(client,
		      "          <h3>Refresh driver list</h3>\n");

  papplClientHTMLPrintf(client,
			"          <p>If you have manually loaded PPD files into the user PPD file directory (%s) or deleted PPD files from there, please click the \"Refresh\" button to update the printer model list in this Printer Application.</p>\n",
			global_data->user_ppd_dir);

  papplClientHTMLStartForm(client, uri, false);
  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  papplClientHTMLPuts(client, "          <tr><th>&nbsp;&nbsp;&nbsp;&nbsp;</th><td><input type=\"hidden\" name=\"action\" value=\"refresh-ppdfiles\"><input type=\"submit\" value=\"Refresh\"></td>\n");

  papplClientHTMLPuts(client,
		      "            </tbody>\n"
		      "          </table>\n"
		      "        </form>\n");

  papplClientHTMLPuts(client,
                      "      </div>\n"
                      "    </div>\n");
  papplClientHTMLFooter(client);

 clean_up:
  // Clean up
  cupsArrayDelete(uploaded);
  cupsArrayDelete(accepted_report);
  cupsArrayDelete(rejected_report);
}
