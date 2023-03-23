//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// print-job.c
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

#include <pappl-retrofit/print-job-private.h>
#include <pappl-retrofit/pappl-retrofit-private.h>


//
// '_prASCII85()' - Print binary data as a series of base-85 numbers.
//                  4 binary bytes are encoded into 5 printable
//                  characters. If the supplied data cannot be divided
//                  into groups of 4, the remaining 1, 2, or 3 bytes
//                  will be held by the function and on the next call
//                  the data will get preceded by these bytes. This
//                  way the data to be encoded can be supplied in
//                  arbitrary portions. On the last call the last_data
//                  bit has to be set to also encode a remainder of
//                  less than 4 bytes. A held remainder can get flushed
//                  out without needing to supply further data by calling
//                  with data set to NULL, length to 0 and last_data to 1.
//

void
_prASCII85(FILE                *outputfp,
	   const unsigned char *data,		// I - Data to encode
	   int                 length,		// I - Number of bytes to encode
	   int                 last_data)	// I - Last portion of data?
{
  unsigned              i;
  unsigned	        b;			// Binary data word */
  unsigned char	        c[5];			// ASCII85 encoded chars */
  static int	        col = 0;		// Current column */
  static unsigned char  remaining[3];           // Remaining bytes which do
                                                // not complete 4 to be encoded
                                                // Keep them for next call
  static unsigned int   num_remaining = 0;


  while (num_remaining + length > 0)
  {
    if (num_remaining > 0 || length < 4)
    {
      for (b = 0, i = 0; i < 4; i ++)
      {
	b <<= 8;
	if (i < num_remaining)
	  b |= remaining[i];
	else if (i - num_remaining < length) 
	  b |= data[i - num_remaining];
	else if (!last_data)
	{
	  if (length)
	    memcpy(remaining + num_remaining, data, length);
	  num_remaining = i;
	  return;
	}
      }
      i = 4 - num_remaining;
      if (length < i)
	i = length;
      num_remaining = 0;
    }
    else
    {
      b = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];
      i = 4;
    }

    if (b == 0)
    {
      fputc('z', outputfp);
      col ++;
    }
    else
    {
      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      fwrite(c, 5, 1, outputfp);
      col += 5;
    }

    if (data)
      data += i;
    length -= i;

    if (col >= 75)
    {
      fputc('\n', outputfp);
      col = 0;
    }
  }

  if (last_data)
  {
    fputs("~>\n", outputfp);
    col = 0;
    num_remaining = 0;
  }
}


//
// '_prGetFileContentType()' - Tries to find out what type of content
//                             the input of the given job is, by the
//                             data format and in case of PDF by the
//                             information about which application had
//                             created it in the metadata. Content
//                             types are the ones of the
//                             print-content-optimize IPP attribute:
//                             Photo, Text, Graphics, Text and
//                             Graphics.
//
//                             Needs one of the external utilities
//                             "pdfinfo" (from Poppler or XPDF) or
//                             "exiftool".
//

pappl_content_t
_prGetFileContentType(pappl_job_t *job)
{
  int        i, j, k;
  const char *informat,
             *filename,
             *found;
  char       command[512],
             line[256];
  char       *p, *q;
  int        creatorline_found = 0;
  pappl_content_t content_type;

  // In the fields "Creator", "Creator Tool", and/or "Producer" of the
  // metadata of a PDF file one usually find the name of the
  // application which created the file. We use thse names to find out
  // the type of content to expect in the file.
  const char *automatic[] =
  { // PAPPL_CONTENT_AUTO
    NULL
  };
  const char *graphics[] =
  { // PAPPL_CONTENT_GRAPHIC
    "Draw",              // LibreOffice
    "Charts",            // LibreOffice
    "Karbon",            // KDE Calligra
    "Flow",              // KDE Calligra
    "Inkscape",
    NULL
  };
  const char *photo[] =
  { // PAPPL_CONTENT_PHOTO
    "imagetopdf",        // CUPS
    "RawTherapee",
    "Darktable",
    "digiKam",
    "Geeqie",
    "GIMP",
    "eog",               // GNOME
    "Skia",              // Google Photos on Android 11 (tested with Pixel 5)
    "ImageMagick",
    "GraphicsMagick",
    "Krita",             // KDE
    "Photoshop",         // Adobe
    "Lightroom",         // Adobe
    "Camera Raw",        // Adobe
    "SilkyPix",
    "Capture One",
    "Photolab",
    "DxO",
    NULL
  };
  const char *text[] =
  { // PAPPL_CONTENT_TEXT
    "texttopdf",         // CUPS
    "GEdit",             // GNOME
    "Writer",            // LibreOffice
    "Word",              // Microsoft Office
    "Words",             // KDE Calligra
    "Kexi",              // KDE Calligra
    "Plan",              // KDE Calligra
    "Braindump",         // KDE Calligra
    "Author",            // KDE Calligra
    "Base",              // LibreOffice
    "Math",              // LibreOffice
    "Pages",             // Mac Office
    "Thunderbird",
    "Bluefish",          // IDEs
    "Geany",             // ...
    "KATE",
    "Eclipse",
    "Brackets",
    "Atom",
    "Sublime",
    "Visual Studio",
    "GNOME Builder",
    "Spacemacs",
    "Atom",
    "CodeLite",          // ...
    "KDevelop",          // IDEs
    "LaTeX",
    "TeX",
    NULL
  };
  const char *text_graphics[] =
  { // PAPPL_CONTENT_TEXT_AND_GRAPHIC
    "evince",            // GNOME
    "Okular",            // KDE
    "Chrome",
    "Chromium",
    "Firefox",
    "Impress",           // LibreOffice
    "Calc",              // LibreOffice
    "Calligra",          // KDE
    "QuarkXPress",
    "InDesign",          // Adobe
    "WPS Presentation",
    "Keynote",           // Mac Office
    "Numbers",           // Mac Office
    "Google",            // Google Docs
    "PowerPoint",        // Microsoft Office
    "Excel",             // Microsoft Office
    "Sheets",            // KDE Calligra
    "Stage",             // KDE Calligra
    NULL
  };

  const char **creating_apps[] =
  {
    automatic,
    graphics,
    photo,
    text,
    text_graphics,
    NULL
  };

  const char * const fields[] =
  {
    "Producer",
    "Creator",
    "Creator Tool",
    NULL
  };

  
  found = NULL;
  content_type = PAPPL_CONTENT_AUTO;
  informat = papplJobGetFormat(job);
  if (!strcmp(informat, "image/jpeg"))            // Photos
    content_type = PAPPL_CONTENT_PHOTO;
  else if (!strcmp(informat, "image/png"))        // Screen shots
    content_type = PAPPL_CONTENT_GRAPHIC;
  else if (!strcmp(informat, "application/pdf"))  // PDF, creating app in
                                                  // metadata
  {
    filename = papplJobGetFilename(job);
    // Run one of the command "pdfinfo" or "exiftool" with the input file,
    // use the first which gets found
    snprintf(command, sizeof(command),
	     "pdfinfo %s 2>/dev/null || exiftool %s 2>/dev/null",
	     filename, filename);
    FILE *pd = popen(command, "r");
    if (!pd)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_WARN,
		  "Unable to get PDF metadata from %s with both pdfinfo and exiftool",
		  filename);
    }
    else
    {
      while (fgets(line, sizeof(line), pd))
      {
	p = line;
	while (isspace(*p))
	  p ++;
	for (i = 0; fields[i]; i ++)
	  if (strncasecmp(p, fields[i], strlen(fields[i])) == 0 &&
	      (isspace(*(p + strlen(fields[i]))) ||
	       *(p + strlen(fields[i])) == ':'))
	    break;
	if (fields[i])
	{
	  p += strlen(fields[i]);
	  while (isspace(*p))
	    p ++;
	  if (*p == ':')
	  {
	    p ++;
	    while (isspace(*p))
	      p ++;
	    while ((q = p + strlen(p) - 1) && (*q == '\n' || *q == '\r'))
	      *q = '\0';
	    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
			"PDF metadata line: %s: %s", fields[i], p);
	    creatorline_found = 1;
	    for (j = 0; j < 5; j ++)
	    {
	      for (k = 0; creating_apps[j][k]; k ++)
	      {
		while ((q = strcasestr(p, creating_apps[j][k])))
		  if (!isalnum(*(q - 1)) &&
		      !isalnum(*(q + strlen(creating_apps[j][k]))))
		  {
		    found = creating_apps[j][k];
		    content_type = 1 << j;
		    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
				"  Found: %s", creating_apps[j][k]);
		    break;
		  }
		  else
		    p = q;
		if (found)
		  break;
	      }
	      if (found)
		break;
	    }
	  }
	}
	if (found)
	  break;
      }
      pclose(pd);
    }
    if (creatorline_found == 0)
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		  "No suitable PDF metadata line found");
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Input file format: %s%s%s%s -> Content optimization: %s",
	      informat,
	      found ? " (" : "", found ? found : "", found ? ")" : "", 
	      (content_type == PAPPL_CONTENT_AUTO ? "No optimization" :
	       (content_type == PAPPL_CONTENT_PHOTO ? "Photo" :
		(content_type == PAPPL_CONTENT_GRAPHIC ? "Graphics" :
		 (content_type == PAPPL_CONTENT_TEXT ? "Text" :
		  "Text and graphics")))));

  return (content_type);
}


//
// '_prCreateJobData()' - Load the printer's PPD file and set the PPD options
//                          according to the job options
//

pr_job_data_t *
_prCreateJobData(pappl_job_t *job,
		   pappl_pr_options_t *job_options)
{
  int                   i, j, k, count, intval = 0;
  pr_driver_extension_t *extension;
  pr_job_data_t         *job_data;      // PPD data for job
  ppd_cache_t           *pc;
  pappl_pr_driver_data_t driver_data;   // Printer driver data
  int		        num_options = 0;// Number of PPD print options
  cups_option_t	        *options = NULL;// PPD print options
  cups_option_t         *opt;
  ipp_t                 *driver_attrs;  // Printer (driver) IPP attributes
  char                  buf[1024];      // Buffer for building strings
  const char            *choicestr,     // Choice name from PPD option
                        *val;           // Value string from IPP option
  ipp_t                 *attrs;         // IPP Attributes structure
  ipp_t		        *media_col,	// media-col IPP structure
                        *media_size;	// media-size IPP structure
  ipp_attribute_t       *attr;
  int                   pcm;            // Print color mode: 0: mono,
                                        // 1: color (for presets)
  int                   pq;             // IPP Print quality value (for presets)
  int                   pco;            // IPP Content optimize (for presets)
  int		        num_presets;	// Number of presets
  cups_option_t	        *presets;       // Presets of PPD options
  ppd_option_t          *option = NULL; // PPD option
  pwg_map_t             *pwg_map;
  int                   controlled_by_presets;
  ppd_coption_t         *coption = NULL;
  char                  *param;
  int                   num_cparams = 0;
  char                  paramstr[1024];
  time_t                t;
  cf_filter_data_t      *filter_data;
  ppd_filter_data_ext_t *filter_data_ext;
  const char * const extra_attributes[] =
  {
   "job-uuid",
   "job-originating-user-name",
   "job-originating-host-name",
   NULL
  };
  pappl_printer_t       *printer = papplJobGetPrinter(job);

  //
  // Load the printer's assigned PPD file, mark the defaults, and create the
  // cache
  //

  job_data = (pr_job_data_t *)calloc(1, sizeof(pr_job_data_t));

  papplPrinterGetDriverData(printer, &driver_data);
  extension = (pr_driver_extension_t *)driver_data.extension;
  job_data->global_data = extension->global_data;
  job_data->device_uri = (char *)papplPrinterGetDeviceURI(printer);
  job_data->ppd = extension->ppd;
  pc = job_data->ppd->cache;
  job_data->temp_ppd_name = extension->temp_ppd_name;
  job_data->stream_filter = extension->stream_filter;
  job_data->stream_format = extension->stream_format;

  driver_attrs = papplPrinterGetDriverAttributes(printer);

  //
  // Find the PPD (or filter) options corresponding to the job options
  //

  // Job options without PPD equivalent
  //  - print-darkness
  //  - darkness-configured
  //  - print-speed

  // page-ranges (filter option)
  if (job_options->first_page == 0)
    job_options->first_page = 1;
  if (job_options->last_page == 0)
    job_options->last_page = INT_MAX;
  if (job_options->first_page > 1 || job_options->last_page < INT_MAX)
  {
    snprintf(buf, sizeof(buf), "%d-%d",
	     job_options->first_page, job_options->last_page);
    num_options = cupsAddOption("page-ranges", buf, num_options, &(options));
  }

  // Finishings
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding options for finishings");
  if (job_options->finishings & PAPPL_FINISHINGS_PUNCH)
    num_options = ppdCacheGetFinishingOptions(pc, NULL, IPP_FINISHINGS_PUNCH,
					      num_options, &(options));
  if (job_options->finishings & PAPPL_FINISHINGS_STAPLE)
    num_options = ppdCacheGetFinishingOptions(pc, NULL, IPP_FINISHINGS_STAPLE,
					      num_options, &(options));
  if (job_options->finishings & PAPPL_FINISHINGS_TRIM)
    num_options = ppdCacheGetFinishingOptions(pc, NULL, IPP_FINISHINGS_TRIM,
					      num_options, &(options));

  // PageSize/media/media-size/media-size-name
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: PageSize");
  attrs = ippNew();
  media_col = ippNew();
  media_size = ippNew();
  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"x-dimension", job_options->media.size_width);
  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"y-dimension", job_options->media.size_length);
  ippAddCollection(media_col, IPP_TAG_PRINTER, "media-size", media_size);
  ippDelete(media_size);
  ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-size-name",
	       NULL, job_options->media.size_name);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"media-left-margin", job_options->media.left_margin);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"media-right-margin", job_options->media.right_margin);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"media-top-margin", job_options->media.top_margin);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
		"media-bottom-margin", job_options->media.bottom_margin);
  ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col", media_col);
  ippDelete(media_col);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "  Requesting size: W=%d H=%d L=%d R=%d T=%d B=%d (1/100 mm)",
	      job_options->media.size_width, job_options->media.size_length,
	      job_options->media.left_margin, job_options->media.right_margin,
	      job_options->media.top_margin, job_options->media.bottom_margin);
  if ((choicestr = ppdCacheGetPageSize(pc, attrs, NULL, NULL)) != NULL)
    num_options = cupsAddOption("PageSize", choicestr,
					  num_options,
					  &(options));
  ippDelete(attrs);

  // InputSlot/media-source
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: %s",
	      pc->source_option ? pc->source_option : "InputSlot");
  if ((choicestr = ppdCacheGetInputSlot(pc, NULL,
					job_options->media.source)) !=
      NULL)
    num_options = cupsAddOption(pc->source_option, choicestr,
				num_options, &(options));

  // MediaType/media-type
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: MediaType");
  if ((choicestr = ppdCacheGetMediaType(pc, NULL,
					job_options->media.type)) != NULL)
    num_options = cupsAddOption("MediaType", choicestr,
				num_options, &(options));

  // orientation-requested (filter option)
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Adding option: orientation-requested");
  if (job_options->orientation_requested >= IPP_ORIENT_PORTRAIT &&
      job_options->orientation_requested <  IPP_ORIENT_NONE)
  {
    snprintf(buf, sizeof(buf), "%d", job_options->orientation_requested);
    num_options = cupsAddOption("orientation-requested", buf,
				num_options, &(options));
  }

  // OutputBin/output-bin
  if ((count = pc->num_bins) > 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: OutputBin");
    val = job_options->output_bin;
    for (i = 0, pwg_map = pc->bins; i < count; i ++, pwg_map ++)
      if (!strcmp(pwg_map->pwg, val))
	choicestr = pwg_map->ppd;
    if (choicestr != NULL)
      num_options = cupsAddOption("OutputBin", choicestr,
				  num_options, &(options));
  }

  // Presets, selected by color/bw and print quality
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Adding option presets depending on requested print quality and color mode");
  if (job_data->ppd->color_device &&
      (job_options->print_color_mode &
       (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_COLOR)) != 0)
    pcm = 1;
  else
    pcm = 0;
  if (job_options->print_quality == IPP_QUALITY_DRAFT)
    pq = 0;
  else if (job_options->print_quality == IPP_QUALITY_HIGH)
    pq = 2;
  else
    pq = 1;
  num_presets = pc->num_presets[pcm][pq];
  presets     = pc->presets[pcm][pq];
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "%sresets for %s printing in %s quality%s",
	      num_presets ? "P" : "No p",
	      pcm == 1 ? "color" : "black and white",
	      pq == 0 ? "draft" : (pq == 1 ? "normal" : "high"),
	      num_presets ? ":" : "");
  if (num_presets > 0)
  {
    for (i = 0; i < num_presets; i ++)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		  "  Adding option: %s=%s", presets[i].name, presets[i].value);
      num_options = cupsAddOption(presets[i].name, presets[i].value,
				  num_options, &(options));
    }
  }

  // Optimize presets, selected by print content optimization
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Adding option presets depending on requested content optimization");

  // Find out about input file content type if not specified
  if (job_options->print_content_optimize == PAPPL_CONTENT_AUTO)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Automatic content type selection ...");
    job_options->print_content_optimize = _prGetFileContentType(job);
  }

  switch (job_options->print_content_optimize)
  {
    default:
    case PAPPL_CONTENT_AUTO:
        pco = 0;
	break;
    case PAPPL_CONTENT_PHOTO:
        pco = 1;
	break;
    case PAPPL_CONTENT_GRAPHIC:
        pco = 2;
	break;
    case PAPPL_CONTENT_TEXT:
        pco = 3;
	break;
    case PAPPL_CONTENT_TEXT_AND_GRAPHIC:
        pco = 4;
	break;
  }
  num_presets = pc->num_optimize_presets[pco];
  presets     = pc->optimize_presets[pco];
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "%sresets for %s printing%s",
	      num_presets ? "P" : "No p",
	      (pco == 0 ? "automatic" :
	       (pco == 1 ? "photo" :
		(pco == 2 ? "graphics" :
		 (pco == 3 ? "text" :
		  "text and graphics")))),
	      num_presets ? ":" : "");
  if (num_presets > 0)
  {
    for (i = 0; i < num_presets; i ++)
    {
      if (pq == 2 ||
	  cupsGetOption(presets[i].name, num_options,
			options) == NULL)
      {
	papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		    "  Adding option: %s=%s",
		    presets[i].name, presets[i].value);
	num_options = cupsAddOption(presets[i].name, presets[i].value,
				    num_options, &(options));
      }
      else
	papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		    "    Skipping option: %s=%s (This option would also switch to high-quality printing)",
		    presets[i].name, presets[i].value);
    }
  }

  // Add "ColorModel=Gray" to make filters converting color input to grayscale
  if (pcm == 0)
  {
    if (cupsGetOption("ColorModel", num_options,
		      options) == NULL)
      num_options = cupsAddOption("ColorModel", "Gray",
				  num_options, &(options));
  }

  // print-scaling (filter option)
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: print-scaling");
  if (job_options->print_scaling)
  {
    if (job_options->print_scaling & PAPPL_SCALING_AUTO)
      num_options = cupsAddOption("print-scaling", "auto",
				  num_options, &(options));
    if (job_options->print_scaling & PAPPL_SCALING_AUTO_FIT)
      num_options = cupsAddOption("print-scaling", "auto-fit",
				  num_options, &(options));
    if (job_options->print_scaling & PAPPL_SCALING_FILL)
      num_options = cupsAddOption("print-scaling", "fill",
				  num_options, &(options));
    if (job_options->print_scaling & PAPPL_SCALING_FIT)
      num_options = cupsAddOption("print-scaling", "fit",
				  num_options, &(options));
    if (job_options->print_scaling & PAPPL_SCALING_NONE)
      num_options = cupsAddOption("print-scaling", "none",
				  num_options, &(options));
  }

  // Duplex/sides
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: Duplex");
  if (job_options->sides && pc->sides_option)
  {
    if (job_options->sides & PAPPL_SIDES_ONE_SIDED &&
	pc->sides_1sided)
      num_options = cupsAddOption(pc->sides_option, pc->sides_1sided,
				  num_options, &(options));
    else if (job_options->sides & PAPPL_SIDES_TWO_SIDED_LONG_EDGE &&
	     pc->sides_2sided_long)
      num_options = cupsAddOption(pc->sides_option, pc->sides_2sided_long,
				  num_options, &(options));
    else if (job_options->sides & PAPPL_SIDES_TWO_SIDED_SHORT_EDGE &&
	     pc->sides_2sided_short)
      num_options = cupsAddOption(pc->sides_option, pc->sides_2sided_short,
				  num_options, &(options));
  }

  //
  // Add vendor-specific PPD options
  //

  k = 0;
  for (i = 0;
       i < driver_data.num_vendor;
       i ++)
  {
    controlled_by_presets = (extension->vendor_ppd_options[i][0] == '/' ?
			     1 : 0);
    if ((param = strchr(extension->vendor_ppd_options[i], ':')) == NULL)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: %s",
		  extension->vendor_ppd_options[i] + controlled_by_presets);
      coption = NULL;
      num_cparams = 0;
      k = 0;
    }
    else
    {
      param ++;
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "  Custom parameter: %s", param);
    }
    if ((attr = papplJobGetAttribute(job, driver_data.vendor[i])) == NULL ||
	ippGetString(attr, 0, NULL) == NULL)
    {
      snprintf(buf, sizeof(buf), "%s-default", driver_data.vendor[i]);
      attr = ippFindAttribute(driver_attrs, buf, IPP_TAG_ZERO);
    }

    choicestr = NULL;
    if (attr)
    {
      val = NULL;
      if (ippGetValueTag(attr) == IPP_TAG_BOOLEAN)
	val = ippGetBoolean(attr, 0) ? "True" : "False";
      else if (ippGetValueTag(attr) == IPP_TAG_INTEGER)
	intval = ippGetInteger(attr, 0);
      else
	val = ippGetString(attr, 0, NULL);
      if (param)
      {
	if (!option || !coption || num_cparams <= 0 || k >= num_cparams)
	  continue;
	if (num_cparams == 1)
	{
	  if (ippGetValueTag(attr) == IPP_TAG_INTEGER)
	    snprintf(paramstr, sizeof(paramstr) - 1, "Custom.%d", intval);
	  else
	    snprintf(paramstr, sizeof(paramstr) - 1, "Custom.%s", val);
	}
	else
	{
	  if (k == 0)
	  {
	    paramstr[0] = '{';
	    paramstr[1] = '\0';
	  }
	  if (ippGetValueTag(attr) == IPP_TAG_INTEGER)
	    snprintf(paramstr + strlen(paramstr),
		     sizeof(paramstr) - strlen(paramstr) - 1,
		     "%s=%d ", param, intval);
	  else
	    snprintf(paramstr + strlen(paramstr),
		     sizeof(paramstr) - strlen(paramstr) - 1,
		     "%s=%s ", param, val);
	  if (k == num_cparams - 1)
	    paramstr[strlen(paramstr) - 1] = '}';
	}
	if (k == num_cparams - 1)
	  num_options =
	    cupsAddOption(option->keyword, paramstr, num_options,
			  &(options));
	k ++;
      }
      else
      {
	option = ppdFindOption(job_data->ppd,
			       extension->vendor_ppd_options[i] +
			       controlled_by_presets);
	if (option == NULL)
        {
	  // Should never happen
	  papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		      "  PPD Option not correctly registered (bug), "
		      "skipping ...");
	  continue;
	}
	if (val == NULL)
        {
	  // Should never happen
	  papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		      "  PPD option not enumerated choice or boolean, "
		      "skipping ...");
	  continue;
	}
	if (controlled_by_presets && !strcasecmp(val, "automatic-selection"))
	{
	  // Option controlled by presets
	  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		      "  PPD option %s controlled by the presets",
		      option->keyword);
	  continue;
	}
	for (j = 0;
	     j < (ippGetValueTag(attr) == IPP_TAG_BOOLEAN ?
		  2 : option->num_choices);
	     j ++)
        {
	  ppdPwgUnppdizeName(option->choices[j].text, buf, sizeof(buf), NULL);
	  if (!strcasecmp(buf, val) ||
	      (option->num_choices == 2 &&
	       ((!strcasecmp(val, "yes") && !strcasecmp(buf, "true")) ||
		(!strcasecmp(val, "no") && !strcasecmp(buf, "false")))))
	  {
	    choicestr = option->choices[j].choice;
	    break;
	  }
	}
	if (choicestr != NULL &&
	    !ppdInstallableConflict(job_data->ppd, option->keyword,
				    choicestr) &&
	    (strcasecmp(choicestr, "Custom") ||
	     (coption =
	      ppdFindCustomOption(job_data->ppd, option->keyword)) == NULL ||
	     (num_cparams = cupsArrayCount(coption->params)) <= 0))
	    num_options =
	      cupsAddOption(option->keyword, choicestr, num_options,
			    &(options));
      }
    }
  }

  // Collate (will only be used with PDF or PostScript input)
  if ((attr =
       papplJobGetAttribute(job,
			    "multiple-document-handling")) != NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Adding option: Collate");
    val = ippGetString(attr, 0, NULL);
    if (strstr(val, "uncollate"))
      choicestr = "False";
    else if (strstr(val, "collate"))
      choicestr = "True";
    num_options = cupsAddOption("Collate", choicestr, num_options, &(options));
  }

  // Reset marked options in the PPD to defaults, to put options we do not
  // treat here into a defined state
  ppdMarkDefaults(job_data->ppd);


  // Mark options in the PPD file
  ppdMarkOptions(job_data->ppd, num_options, options);

  // Job attributes not handled by the PPD options which could be used by
  // some CUPS filters or filter functions
  for (i = 0; extra_attributes[i]; i ++)
    if ((attr = papplJobGetAttribute(job, extra_attributes[i])) != NULL &&
	(val = ippGetString(attr, 0, NULL)) != NULL)
      num_options = cupsAddOption(extra_attributes[i], val,
				  num_options, &(options));

  // Add options with time of creation and time of processing of the job
  if ((t = papplJobGetTimeCreated(job)) > 0)
  {
    snprintf(buf, sizeof(buf) - 1, "%ld", t);
    num_options = cupsAddOption("time-at-creation", buf,
				num_options, &(options));
  }
  if ((t = papplJobGetTimeProcessed(job)) > 0)
  {
    snprintf(buf, sizeof(buf) - 1, "%ld", t);
    num_options = cupsAddOption("time-at-processing", buf,
				num_options, &(options));
  }

  // Log the option settings which will get used
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "PPD options to be used:");
  for (i = num_options, opt = options; i > 0; i --, opt ++)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "  %s=%s", opt->name, opt->value);

  // Set environment variables for filters
  if ((val = papplPrinterGetName(printer)) != NULL && val[0])
    setenv("PRINTER", val, 1);
  else
    unsetenv("PRINTER");
  if ((val = papplPrinterGetLocation(printer, buf, sizeof(buf))) != NULL &&
      buf[0])
    setenv("PRINTER_LOCATION", val, 1);
  else
    unsetenv("PRINTER_LOCATION");

  // Clean up
  ippDelete(driver_attrs);

  // Prepare job data to be supplied to filter functions/CUPS filters
  // called during job execution
  filter_data = (cf_filter_data_t *)calloc(1, sizeof(cf_filter_data_t));
  job_data->filter_data = filter_data;
  filter_data->printer = strdup(papplPrinterGetName(printer));
  filter_data->job_id = papplJobGetID(job);
  filter_data->job_user = strdup(papplJobGetUsername(job));
  filter_data->job_title = strdup(papplJobGetName(job));
  filter_data->copies = job_options->copies;
  filter_data->job_attrs = NULL;     // We use PPD/filter options
  filter_data->printer_attrs = NULL; // We use the printer's PPD file
  filter_data->num_options = num_options;
  filter_data->options = options; // PPD/filter options
  filter_data->extension = NULL;
  filter_data->back_pipe[0] = -1;
  filter_data->back_pipe[1] = -1;
  filter_data->side_pipe[0] = -1;
  filter_data->side_pipe[1] = -1;
  filter_data->logfunc = _prJobLog; // Job log function catching page counts
                                    // ("PAGE: XX YY" messages)
  filter_data->logdata = job;
  filter_data->iscanceledfunc = _prJobIsCanceled; // Function to indicate
                                                  // whether the job got
                                                  // canceled
  filter_data->iscanceleddata = job;

  // Attach PPD file data as "libppd" filter data extension
  filter_data_ext =
    (ppd_filter_data_ext_t *)calloc(1, sizeof(ppd_filter_data_ext_t));
  filter_data_ext->ppdfile = job_data->temp_ppd_name; // PPD file name
  filter_data_ext->ppd = job_data->ppd;               // PPD data
  cfFilterDataAddExt(filter_data, PPD_FILTER_DATA_EXT, filter_data_ext);

  // Establish back/side channel pipes for CUPS backends
  if (job_data->global_data->config->components & PR_COPTIONS_CUPS_BACKENDS)
    cfFilterOpenBackAndSidePipes(filter_data);

  return (job_data);
}


//
// '_prFilter()' - PAPPL generic filter function wrapper for printing
//                 in spooling mode
//

bool
_prFilter(
    pappl_job_t    *job,		// I - Job
    pappl_device_t *device,		// I - Device
    void *data)                         // I - Global data
{
  int                   i;
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pr_cups_device_data_t *device_data = NULL;
  pr_job_data_t         *job_data;      // PPD data for job
  ppd_filter_data_ext_t *filter_data_ext;
  ppd_file_t            *ppd;           // PPD of the printer
  const char            *informat;
  const char		*filename;	// Input filename
  int			fd;		// Input file descriptor
  pr_spooling_conversion_t *conversion; // Spooling conversion to use
                                        // for pre-filtering
  char                  *filter_path = NULL; // Filter from PPD to use for
                                        // this job
  int                   nullfd;         // File descriptor for /dev/null
  pappl_pr_options_t	*job_options;	// Job options
  bool			ret = false;	// Return value
  cf_filter_external_t* ppd_filter_params = NULL; // Parameters for CUPS
                                        // filter defined in the PPD
  pr_print_filter_function_data_t *print_params; // Parameters for
                                        // _prPrintFilterFunction()
  cf_filter_filter_in_chain_t banner_filter = // cfFilterBannerToPDF() filter
                                        // function in filter chain, mainly for
  {                                     // PDF test pages
    cfFilterBannerToPDF,
    NULL,
    "bannertopdf"
  };
  int                   is_banner = 0;  // Do we have cfFilterBannerToPDF()
                                        // instructions in our PDF input file


  //
  // Load the printer's assigned PPD file, and find out which PPD option
  // seetings correspond to our job options
  //

  job_options = papplJobCreatePrintOptions(job, INT_MAX, 1);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Printing job in spooling mode");

  job_data = _prCreateJobData(job, job_options);
  filter_data_ext =
    (ppd_filter_data_ext_t *)cfFilterDataGetExt(job_data->filter_data,
						PPD_FILTER_DATA_EXT);
  ppd = filter_data_ext->ppd;

  //
  // Open the input file...
  //

  filename = papplJobGetFilename(job);
  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open input file '%s' for printing: %s",
		filename, strerror(errno));
    return (false);
  }

  //
  // Get input file format
  //

  informat = papplJobGetFormat(job);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Input file format: %s", informat);

  //
  // Find filters to use for this job
  //
  
  for (conversion =
	 (pr_spooling_conversion_t *)
	 cupsArrayFirst(global_data->config->spooling_conversions);
       conversion;
       conversion =
	 (pr_spooling_conversion_t *)
	 cupsArrayNext(global_data->config->spooling_conversions))
  {
    if (strcmp(conversion->srctype, informat) != 0)
      continue;
    if ((filter_path =
	 _prPPDFindCUPSFilter(conversion->dsttype,
			      ppd->num_filters, ppd->filters,
			      global_data->filter_dir)) != NULL)
      break;
  }

  if (conversion == NULL || filter_path == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"No pre-filter found for input format %s",
		informat);
    return (false);
  }

  // Set input and output formats for the filter chain
  job_data->filter_data->content_type = conversion->srctype;
  job_data->filter_data->final_content_type = conversion->dsttype;

  // Convert PPD file data into printer IPP attributes and options,
  // for the filter functions being able to use it
  ppdFilterLoadPPD(job_data->filter_data);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Converting input file to format: %s", conversion->dsttype);
  if (filter_path[0] == '.')
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Passing on PostScript directly to printer");
  else if (filter_path[0] == '-')
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Passing on %s directly to printer", conversion->dsttype);
  else
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Using CUPS filter (printer driver): %s", filter_path);

  //
  // Connect the job's filter_data to the backend
  //

  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Connect the filter_data
    device_data->filter_data = job_data->filter_data;
  }

  //
  // Check whether the PDF input is a banner or test page
  //

  if (strcmp(informat, "application/pdf") == 0 ||
      strcmp(informat, "application/vnd.cups-pdf") == 0)
  {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    fp = fdopen(fd, "r");
    while (getline(&line, &len, fp) != -1)
      if (strncmp(line, "%%#PDF-BANNER", 13) == 0 ||
	  strncmp(line, "%%PDF-BANNER", 12) == 0)
      {
	papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		    "Input PDF file is banner or test page file, calling bannertopdf to add printer and job information");
	is_banner = 1;
	job_data->filter_data->content_type = "application/vnd.cups-pdf-banner";
	break;
      }
    rewind(fp);
    free(line);
  }

  //
  // Set up filter function chain
  //

  job_data->chain = cupsArrayNew(NULL, NULL);
  if (is_banner)
    cupsArrayAdd(job_data->chain, &banner_filter);
  for (i = 0; i < conversion->num_filters; i ++)
    cupsArrayAdd(job_data->chain, &(conversion->filters[i]));
  if (strlen(filter_path) > 1) // A null filter is a single char, '-'
                               // or '.', whereas an actual filter has
                               // a path starting with '/', so at
                               // least 2 chars.
  {
    ppd_filter_params =
      (cf_filter_external_t *)calloc(1, sizeof(cf_filter_external_t));
    ppd_filter_params->filter = filter_path;
    job_data->ppd_filter =
      (cf_filter_filter_in_chain_t *)calloc(1,
			                sizeof(cf_filter_filter_in_chain_t));
    job_data->ppd_filter->function = ppdFilterExternalCUPS;
    job_data->ppd_filter->parameters = ppd_filter_params;
    job_data->ppd_filter->name = strrchr(filter_path, '/') + 1;
    cupsArrayAdd(job_data->chain, job_data->ppd_filter);
  } else
    job_data->ppd_filter = NULL;
  job_data->print =
    (cf_filter_filter_in_chain_t *)calloc(1,
					  sizeof(cf_filter_filter_in_chain_t));
  // Put filter function to send data to PAPPL's built-in backend at the end
  // of the chain
  print_params =
    (pr_print_filter_function_data_t *)
    calloc(1, sizeof(pr_print_filter_function_data_t));
  print_params->device = device;
  print_params->device_uri = job_data->device_uri;
  print_params->job = job;
  print_params->global_data = global_data;
  job_data->print->function = _prPrintFilterFunction;
  job_data->print->parameters = print_params;
  job_data->print->name = "Backend";
  cupsArrayAdd(job_data->chain, job_data->print);

  //
  // Update status
  //

  _prUpdateStatus(papplJobGetPrinter(job), device);

  //
  // Fire up the filter functions
  //

  papplJobSetImpressions(job, 1);

  // The filter chain has no output, data is going to the device
  nullfd = open("/dev/null", O_RDWR);

  if (cfFilterChain(fd, nullfd, 1, job_data->filter_data, job_data->chain) == 0)
    ret = true;

  //
  // Update status
  //

  _prUpdateStatus(papplJobGetPrinter(job), device);

  //
  // Stop the backend and disconnect the job's filter_data from the backend
  //

  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Shutting down CUPS backend");

    // Stop the backend
    // We stop it here explicitly as we will free the filter_data structure
    // and without it the backend shutdoen will not have access to the log
    // function any more.
    _prCUPSDevStopBackend(device);

    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Disconnect the filter_data
    device_data->filter_data = NULL;
  }

  //
  // Clean up
  //

  if (filter_path)
    free(filter_path);
  if (ppd_filter_params)
    free(ppd_filter_params);
  papplJobDeletePrintOptions(job_options);
  _prFreeJobData(job_data);
  close(fd);
  close(nullfd);

  return (ret);
}


//
// '_prFreeJobData()' - Clean up job data with PPD options.
//

void _prFreeJobData(pr_job_data_t *job_data)
{
  ppd_filter_data_ext_t *filter_data_ext =
    (ppd_filter_data_ext_t *)cfFilterDataRemoveExt(job_data->filter_data,
						   PPD_FILTER_DATA_EXT);

  unsetenv("PRINTER");
  unsetenv("PRINTER_LOCATION");

  if (job_data->global_data->config->components & PR_COPTIONS_CUPS_BACKENDS)
    cfFilterCloseBackAndSidePipes(job_data->filter_data);

  if (filter_data_ext)
    free(filter_data_ext);

  free(job_data->filter_data->printer);
  free(job_data->filter_data->job_user);
  free(job_data->filter_data->job_title);
  ppdFilterFreePPD(job_data->filter_data);
  cupsFreeOptions(job_data->filter_data->num_options,
		  job_data->filter_data->options);
  free(job_data->filter_data);
  
  if (job_data->ppd_filter)
    free(job_data->ppd_filter);
  if (job_data->print)
  {
    free(job_data->print->parameters);
    free(job_data->print);
  }
  if (job_data->chain)
    cupsArrayDelete(job_data->chain);
  free(job_data);
}


//
// '_prJobIsCanceled()' - Return 1 if the job is canceled, which is
//                        the case when papplJobIsCanceled() returns
//                        true.
//

int
_prJobIsCanceled(void *data)
{
  pappl_job_t *job = (pappl_job_t *)data;


  return (papplJobIsCanceled(job) ? 1 : 0);
}


//
// '_prJobLog()' - Job log function which calls
//                 papplJobSetImpressionsCompleted() on page logs of
//                 filter functions
//

void
_prJobLog(void *data,
	  cf_loglevel_t level,
	  const char *message,
	  ...)
{
  va_list arglist;
  pappl_job_t *job = (pappl_job_t *)data;
  char buf[1024];
  int page, copies;


  va_start(arglist, message);
  vsnprintf(buf, sizeof(buf) - 1, message, arglist);
  fflush(stdout);
  va_end(arglist);
  if (level == CF_LOGLEVEL_CONTROL)
  {
    if (sscanf(buf, "PAGE: %d %d", &page, &copies) == 2)
    {
      papplJobSetImpressionsCompleted(job, copies);
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Printing page %d, %d copies",
		  page, copies);
    }
    else
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Unused control message: %s",
		  buf);
  }
  else
    papplLogJob(job, (pappl_loglevel_t)level, "%s", buf);
}


//
// '_prOneBitDitherOnDraft()' - If an image job is printed in
//                              grayscale in draft mode switch to
//                              1-bit dithering mode to get printing
//                              as fast as possible
//

void
_prOneBitDitherOnDraft(
    pappl_job_t        *job,     // I   - Job
    pappl_pr_options_t *options) // I/O - Job options
{
  pappl_pr_driver_data_t driver_data;


  if (!strcmp(papplJobGetFormat(job), "image/urf") ||
      !strcmp(papplJobGetFormat(job), "image/pwg-raster"))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Not changing Raster input color depth on PWG/Apple Raster input");
    return;
  }

  papplPrinterGetDriverData(papplJobGetPrinter(job), &driver_data);
  if (options->print_quality == IPP_QUALITY_DRAFT &&
      options->print_color_mode != PAPPL_COLOR_MODE_COLOR &&
      options->header.cupsNumColors == 1)
  {
    options->header.cupsBitsPerColor = 1;
    options->header.cupsBitsPerPixel = 1;
    options->header.cupsColorSpace = CUPS_CSPACE_K;
    options->header.cupsColorOrder = CUPS_ORDER_CHUNKED;
    options->header.cupsNumColors = 1;
    options->header.cupsBytesPerLine = (options->header.cupsWidth + 7) / 8;
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Monochrome draft quality job -> 1-bit dithering for speed-up");
    if (options->print_content_optimize == PAPPL_CONTENT_PHOTO ||
	!strcmp(papplJobGetFormat(job), "image/jpeg") ||
	!strcmp(papplJobGetFormat(job), "image/png"))
    {
      memcpy(options->dither, driver_data.pdither, sizeof(options->dither));
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		  "Photo/Image-optimized dither matrix");
    }
    else
    {
      memcpy(options->dither, driver_data.gdither, sizeof(options->dither));
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		  "General-purpose dither matrix");
    }
  }
  else
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Not in monochrome draft mode -> no color depth change applied");
}


//
// `_prCleanDebugCopies()' - Remove debug copies of jobs created by
//                           the _prPrintFilterFunction() function
//                           more than 24 hours ago. This avoids
//                           filling up the disk should the user have
//                           switched to debug logging for some reason
//                           and forgot to turn back after solving his
//                           problem.
//

void
_prCleanDebugCopies(pr_printer_app_global_data_t *global_data)
{
  cups_dir_t	*dir;			// Directory pointer
  cups_dentry_t	*dent;			// Directory entry
  char		filename[2048];		// Name of PPD or directory
  time_t        outdated;               // Files older than this time
                                        // get deleted


  papplLog(global_data->system, PAPPL_LOGLEVEL_DEBUG,
	   "Checking for old debug copy files in the spool directory %s",
	   global_data->spool_dir);

  // Open spool directory
  if ((dir = cupsDirOpen(global_data->spool_dir)) == NULL)
  {
    papplLog(global_data->system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to open spool directory %s: %s",
	     global_data->spool_dir, strerror(errno));
    return;
  }

  // Files older than 24 hours are outdated and get deleted
  outdated = time(NULL) - 24 * 60 * 60;

  // Go through all files and remov ethe outdated debug copied
  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (S_ISDIR(dent->fileinfo.st_mode) || dent->filename[0] == '.' ||
	dent->fileinfo.st_mtime > outdated ||
	strncmp(dent->filename, "debug-jobdata-", 14))
      continue;
    snprintf(filename, sizeof(filename), "%s/%s",
	     global_data->spool_dir, dent->filename);
    unlink(filename);
    papplLog(global_data->system, PAPPL_LOGLEVEL_DEBUG,
	     "Deleted old debug copy file %s", dent->filename);
  }

  cupsDirClose(dir);
}


//
// '_prPrintFilterFunction()' - Print file.
//
//                              This function has the format of a
//                              filter function of libcupsfilters, so
//                              we can chain it with other filter
//                              functions using the special filter
//                              function cfFilterChain() and so we do
//                              not need to care with forking. As we
//                              send off the data to the device
//                              instead of filtering, it behaves more
//                              like a backend than a filter, and
//                              sends nothing to its output
//                              FD. Therefore it must always be in the
//                              end of a chain.  This function does
//                              not do any filtering or conversion,
//                              this has to be done by filters applied
//                              to the data before.  If we run the
//                              Printer Application in debug logging
//                              mode ("-o log-level=debug" or
//                              switching on the logging page of the
//                              web interface) from every job a copy
//                              of the data actually sent to the
//                              printer gets saved in a file
//                              (debug-copy-PRINTER-JOB.prn) in the
//                              spool directory. These files are kept
//                              for 24 hours (clean-up done with every
//                              new job, independent of log level).
//

int                                           // O - Error status
_prPrintFilterFunction(int inputfd,           // I - File descriptor input 
			                      //     stream
		       int outputfd,          // I - File descriptor output
			                      //     stream (unused)
		       int inputseekable,     // I - Is input stream
			                      //     seekable? (unused)
		       cf_filter_data_t *data,// I - Job and printer data
		       void *parameters)      // I - PAPPL output device
{
  ssize_t	       bytes;	              // Bytes read/written
  char	               buffer[65536];         // Read/write buffer
  cf_logfunc_t         log = data->logfunc;   // Log function
  void                 *ld = data->logdata;   // log function data
  pr_print_filter_function_data_t *params =
    (pr_print_filter_function_data_t *)parameters;
  pappl_device_t       *device = params->device; // PAPPL output device
  pappl_job_t          *job = params->job;
  pappl_printer_t      *printer;
  pr_printer_app_global_data_t *global_data = params->global_data;
  char                 filename[2048];        // Name for debug copy of the
                                              // job
  int                  debug_fd = -1;         // File descriptor for debug copy


  (void)inputseekable;

  // Remove debug copies of old jobs
  _prCleanDebugCopies(global_data);

  if (papplSystemGetLogLevel(global_data->system) == PAPPL_LOGLEVEL_DEBUG)
  {
    // We are in debug mode
    // Debug copy file name (in spool directory)
    printer = papplJobGetPrinter(job);
    snprintf(filename, sizeof(filename), "%s/debug-jobdata-%s-%d.prn",
	     global_data->spool_dir, papplPrinterGetName(printer),
	     papplJobGetID(job));
    if (log)
      log(ld, CF_LOGLEVEL_DEBUG,
	  "Backend: Creating debug copy of what goes to the printer: %s",
	  filename);
    // Open the file
    debug_fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  }

  while ((bytes = read(inputfd, buffer, sizeof(buffer))) > 0)
  {
    if (debug_fd >= 0)
      if (write(debug_fd, buffer, (size_t)bytes) != bytes)
      {
	if (log)
	  log(ld, CF_LOGLEVEL_ERROR,
	      "Backend: Debug copy: Unable to write %d bytes, stopping debug copy, continuing job output.",
	      (int)bytes);
	close(debug_fd);
	debug_fd = -1;
      }

    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      if (log)
	log(ld, CF_LOGLEVEL_ERROR,
	    "Backend: Output to device: Unable to send %d bytes to printer.",
	    (int)bytes);
      if (debug_fd >= 0)
	close(debug_fd);
      close(inputfd);
      close(outputfd);
      return (1);
    }
  }
  papplDeviceFlush(device);

  if (debug_fd >= 0)
    close(debug_fd);

  close(inputfd);
  close(outputfd);
  return (0);
}


//
// '_prRasterPrepareJob()' - Create job data record to carry through
//                           all the raster printing callbacks from
//                           the PPD and job attributes.  Also create
//                           the output pipe to the device, if needed
//                           passing the data through a CUPS filter
//                           defined in the "*cupsFilter(2): ..."
//                           lines of the PPD file
//

pr_job_data_t*                  // O - Job data, NULL on failure
_prRasterPrepareJob(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Job options
    pappl_device_t   *device,   // I - Device
    char             *starttype)// I - MIME type to feed into the filters
{
  int                    i;
  pr_job_data_t          *job_data;  // PPD data for job
  pr_cups_device_data_t  *device_data = NULL; // Device data of CUPS backend
                                     // device
  int                    nullfd;     // File descriptor pointing to /dev/null
  cf_filter_external_t   *ppd_filter_params = NULL;
                                     // Parameters for call of PPD's
                                     // CUPS filter via ppdFilterExternalCUPS()
  pr_print_filter_function_data_t *print_params; // Paramaters for
                                     // _prPrintFilterFunction()


  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Printing job in streaming mode");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Converting raster input to format %s for further filtering",
	      starttype);

  // Load PPD file and determine the PPD options equivalent to the job options
  job_data = _prCreateJobData(job, options);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Filtering data to get format %s to send off to the driver or device",
	      job_data->stream_format->dsttype);

  // Do not generate copies in post-filtering, for PWG/Apple Raster input
  // the client has to generate copies, for images PAPPL generates them
  job_data->filter_data->copies = 1;

  // Set streaming mode for the filters. This prevents them from pre-checks
  // (for input format or empty job) which require spooling the input and
  // also assumes a streamable format (usually PostScript instead of PDF)
  // as input
  job_data->filter_data->num_options =
    cupsAddOption("filter-streaming-mode", "true",
		  job_data->filter_data->num_options,
		  &job_data->filter_data->options);

  // Connect the job's filter_data to the backend
  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Connect the filter_data
    device_data->filter_data = job_data->filter_data;
  }

  // The filter chain has no output, data is going directly to the device
  nullfd = open("/dev/null", O_RDWR);

  // Create file descriptor/pipe to which print data can be sent

  // Create filter chain of the filter function for creating the stream
  // data format and/or call the CUPS filter defined in the PPD file, and the
  // print filter function
  job_data->chain = cupsArrayNew(NULL, NULL);
  for (i = 0; i < job_data->stream_format->num_filters; i ++)
    cupsArrayAdd(job_data->chain, &(job_data->stream_format->filters[i]));
  // Set input and output formats for the filter chain
  job_data->filter_data->content_type = starttype;
  job_data->filter_data->final_content_type = job_data->stream_format->dsttype;
  // Convert PPD file data into printer IPP attributes and options,
  // for the filter functions being able to use it
  ppdFilterLoadPPD(job_data->filter_data);
  // Filter from PPD?
  if (strlen(job_data->stream_filter) > 1) // A null filter is a
                                           // single char, '-' or '.',
                                           // whereas an actual filter
                                           // has a path starting with
                                           // '/', so at least 2
                                           // chars.
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Using CUPS filter (printer driver): %s",
		job_data->stream_filter);
    ppd_filter_params =
      (cf_filter_external_t *)calloc(1, sizeof(cf_filter_external_t));
    ppd_filter_params->filter = job_data->stream_filter;
    job_data->ppd_filter =
      (cf_filter_filter_in_chain_t *)calloc(1,
					  sizeof(cf_filter_filter_in_chain_t));
    job_data->ppd_filter->function = ppdFilterExternalCUPS;
    job_data->ppd_filter->parameters = ppd_filter_params;
    job_data->ppd_filter->name = strrchr(job_data->stream_filter, '/') + 1;
    cupsArrayAdd(job_data->chain, job_data->ppd_filter);
  } else
    job_data->ppd_filter = NULL;
  // Put filter function to send data to PAPPL's built-in backend at the end
  // of the chain
  print_params =
    (pr_print_filter_function_data_t *)
    calloc(1, sizeof(pr_print_filter_function_data_t));
  print_params->device = device;
  print_params->device_uri = job_data->device_uri;
  print_params->job = job;
  print_params->global_data = job_data->global_data;
  job_data->print =
    (cf_filter_filter_in_chain_t *)calloc(1,
					  sizeof(cf_filter_filter_in_chain_t));
  job_data->print->function = _prPrintFilterFunction;
  job_data->print->parameters = print_params;
  job_data->print->name = "Backend";
  cupsArrayAdd(job_data->chain, job_data->print);

  // Update status
  _prUpdateStatus(papplJobGetPrinter(job), device);

  // Call the filter chain and get the file descriptor to feed in the data
  job_data->device_fd = cfFilterPOpen(cfFilterChain, -1, nullfd,
				      0, job_data->filter_data, job_data->chain,
				      &(job_data->device_pid));

  if (job_data->device_fd < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"Unable to create pipe for filtering and sending off the job");
    if (strlen(job_data->stream_filter) > 1)
      free(ppd_filter_params);
    return (NULL);
  }

  // Save data for the other raster callback functions
  papplJobSetData(job, job_data);

  return (job_data);
}


//
// '_prRasterCleanUpJob()' - Clean-up after finishing a job
//

void
_prRasterCleanUpJob(pappl_job_t      *job,      // I - Job
		    pappl_device_t   *device)   // I - Device
{
  pr_job_data_t *job_data = (pr_job_data_t *)papplJobGetData(job);
  pr_cups_device_data_t *device_data = NULL; // Device data of CUPS backend
                                             // device


  // Stop the filter chain
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Shutting down filter chain");
  cfFilterPClose(job_data->device_fd, job_data->device_pid,
	       job_data->filter_data);

  // Update status
  _prUpdateStatus(papplJobGetPrinter(job), device);

  // Stop the backend and disconnect the job's filter_data to the backend
  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Shutting down CUPS backend");

    // Stop the backend
    // We stop it here explicitly as we will free the filter_data structure
    // and without it the backend shutdoen will not have access to the log
    // function any more.
    _prCUPSDevStopBackend(device);

    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Disconnect the filter_data
    device_data->filter_data = NULL;
  }

  // Free the data structures
  if (strlen(job_data->stream_filter) > 1)
    free(job_data->ppd_filter->parameters);
  _prFreeJobData(job_data);
  papplJobSetData(job, NULL);
}


//
// Functions to stream Raster jobs into PWG Raster, used together with
// the pwgtoraster filter function for printers using classic CUPS
// Raster drivers
//

//
// 'prPWGRasterEndJob()' - End a raster-to-PWG-Raster job. (Only close
//                         the streams and free allocated memory, no
//                         further data to be sent)
//

bool                            // O - `true` on success, `false` on failure
prPWGRasterEndJob(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Options
    pappl_device_t   *device)   // I - Device
{
  pr_job_data_t      *job_data;      // PPD data for job
  cups_raster_t      *raster;        // PWG Raster output stream

  (void)options;
  (void)device;

  job_data = (pr_job_data_t *)papplJobGetData(job);
  raster = (cups_raster_t *)job_data->data;

  cupsRasterClose(raster);
  
  //
  // Clean up
  //

  _prRasterCleanUpJob(job, device);

  return (true);
}


//
// 'prPWGRasterEndPage()' - End a raster-to-PWG-Raster page. (Nothing
//                          needed to be send to finish the page, only
//                          flush the buffer to get the page ejected)
//

bool                            // O - `true` on success, `false` on failure
prPWGRasterEndPage(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Job options
    pappl_device_t   *device,   // I - Device
    unsigned         page)      // I - Page number
{
  (void)job;
  (void)options;
  (void)page;
  
  // Nothing more to send here, but flush the buffer to get the page ejected
  papplDeviceFlush(device);

  return (true);
}


//
// 'prPWGRasterStartJob()' - Start a raster-to-PWG-Raster job.
//                           (Prepare job and initiate PWG Raster job
//                           output, sending the 4-byte "Magic
//                           string", the cfFilterPWGToRaster() filter
//                           function will do all the dirty conversion
//                           work to get the CUPS Raster needed by the
//                           driver/filter defined in PPD)
//

bool                            // O - `true` on success, `false` on failure
prPWGRasterStartJob(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Job options
    pappl_device_t   *device)   // I - Device
{
  pr_job_data_t      *job_data; // PPD data for job
  cups_raster_t      *raster;

  // Create the job data record and the pipe to the device, with PPD's CUPS
  // filter if needed
  job_data = _prRasterPrepareJob(job, options, device, "image/pwg-raster");

  if (!job_data)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"Unable to create job metadata record");
    return (false);
  }

  // Initiate PWG Raster output
  raster = cupsRasterOpen(job_data->device_fd, CUPS_RASTER_WRITE_PWG);
  if (raster == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"Unable to open PWG Raster output stream");
    return(false);
  }
  job_data->data = raster;

  return (true);
}


//
// 'prPWGRasterStartPage()' - Start a raster-to-PWG-Raster page.
//                            (Send a PWG Raster header, in our case
//                            options->header)
//

bool                              // O - `true` on success, `false` on failure
prPWGRasterStartPage(
    pappl_job_t       *job,       // I - Job
    pappl_pr_options_t  *options, // I - Job options
    pappl_device_t    *device,    // I - Device
    unsigned          page)       // I - Page number
{
  pr_job_data_t      *job_data;   // PPD data for job
  cups_raster_t      *raster;     // PWG Raster output stream

  (void)device;
  
  job_data = (pr_job_data_t *)papplJobGetData(job);
  raster = (cups_raster_t *)job_data->data;
  job_data->line_count = 0;

  if (!cupsRasterWriteHeader2(raster, &(options->header)))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"Unable to output PWG Raster header for page %d", page);
    return(false);
  }

  return (true);
}


//
// 'prPWGRasterWriteLine()' - Write a raster-to-PWG-Raster pixel line.
//                            (Simply pass through the pixels as the
//                            cfFilterPWGToRaster() filter function
//                            does the needed conversion work)
//

bool				  // O - `true` on success, `false` on failure
prPWGRasterWriteLine(
    pappl_job_t         *job,	  // I - Job
    pappl_pr_options_t  *options, // I - Job options
    pappl_device_t      *device,  // I - Device
    unsigned            y,	  // I - Line number
    const unsigned char *pixels)  // I - Line
{
  pr_job_data_t      *job_data;   // PPD data for job
  cups_raster_t      *raster;     // PWG Raster output stream

  (void)device;
  
  job_data = (pr_job_data_t *)papplJobGetData(job);
  raster = (cups_raster_t *)job_data->data;

  if (job_data->line_count < options->header.cupsHeight)
    if (!cupsRasterWritePixels(raster, (unsigned char *)pixels,
			       options->header.cupsBytesPerLine))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		  "Unable to output PWG Raster pixel line %d", y);
      return(false);
    }
  job_data->line_count ++;

  return (true);
}


//
// Functions to stream Raster jobs into PostScript, for PostScript printers
// and Ghostscript drivers
//

//
// 'prPSRasterEndJob()' - End a raster-to-PostScript job.
//

bool                            // O - `true` on success, `false` on failure
prPSRasterEndJob(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Options
    pappl_device_t   *device)   // I - Device
{
  pr_job_data_t *job_data;      // PPD data for job
  FILE *devout;
  int num_pages;


  (void)options;
  job_data = (pr_job_data_t *)papplJobGetData(job);
  devout = job_data->device_file;

  fputs("%%Trailer\n", devout);
  if ((num_pages = papplJobGetImpressionsCompleted(job)) > 0)
    fprintf(devout,"%%%%Pages: %d\n", num_pages);
  fputs("%%EOF\n", devout);

  if (job_data->ppd->jcl_end)
    ppdEmitJCLEnd(job_data->ppd, devout);
  else
    fputc(0x04, devout);

  fclose(devout);

  //
  // Clean up
  //

  _prRasterCleanUpJob(job, device);

  return (true);
}


//
// 'prPSRasterEndPage()' - End a raster-to-PostScript page.
//

bool                            // O - `true` on success, `false` on failure
prPSRasterEndPage(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Job options
    pappl_device_t   *device,   // I - Device
    unsigned         page)      // I - Page number
{
  pr_job_data_t      *job_data; // PPD data for job
  FILE *devout;
  unsigned char *pixels;


  job_data = (pr_job_data_t *)papplJobGetData(job);
  devout = job_data->device_file;

  // If we got too few raster lines pad with blank lines
  if (job_data->line_count < options->header.cupsHeight)
  {
    pixels = (unsigned char *)malloc(options->header.cupsBytesPerLine);
    if (options->header.cupsColorSpace == CUPS_CSPACE_K ||
	options->header.cupsColorSpace == CUPS_CSPACE_CMYK)
      memset(pixels, 0x00, options->header.cupsBytesPerLine);
    else
      memset(pixels, 0xff, options->header.cupsBytesPerLine);
    for (; job_data->line_count < options->header.cupsHeight;
	 job_data->line_count ++)
      _prASCII85(devout, pixels, options->header.cupsBytesPerLine, 0);
    free (pixels);
  }

  // Flush out remaining bytes of the bitmap 
  _prASCII85(devout, NULL, 0, 1);

  // Finish page and get it printed
  fprintf(devout, "grestore\n");
  fprintf(devout, "showpage\n");
  fprintf(devout, "%%%%PageTrailer\n");

  papplDeviceFlush(device);

  return (true);
}


//
// 'prPSRasterStartJob()' - Start a raster-to-PostScript job.
//

bool                            // O - `true` on success, `false` on failure
prPSRasterStartJob(
    pappl_job_t      *job,      // I - Job
    pappl_pr_options_t *options,// I - Job options
    pappl_device_t   *device)   // I - Device
{
  pr_job_data_t      *job_data; // PPD data for job
  const char	     *job_name; // Job name for header of PostScript file
  FILE               *devout;   // Output file pointer (pipe to device)
  pr_printer_app_global_data_t *global_data;


  // Create the job data record and the pipe to the device, with PPD's CUPS
  // filter if needed
  job_data = _prRasterPrepareJob(job, options, device,
				 "application/vnd.cups-postscript");

  if (!job_data)
    return (false);

  job_data->device_file = fdopen(job_data->device_fd, "w");

  global_data = job_data->global_data;
  devout = job_data->device_file;

  // Print 1 bit per pixel for monochrome draft printing
  _prOneBitDitherOnDraft(job, options);

  // DSC header
  job_name = papplJobGetName(job);

  ppdEmitJCL(job_data->ppd, devout, papplJobGetID(job),
	     papplJobGetUsername(job), job_name ? job_name : "Unknown");

  fputs("%!PS-Adobe-3.0\n", devout);
  fprintf(devout, "%%%%LanguageLevel: %d\n", job_data->ppd->language_level);
  fprintf(devout, "%%%%Creator: %s/%d.%d.%d.%d\n",
	  global_data->config->system_name,
	  global_data->config->numeric_version[0],
	  global_data->config->numeric_version[1],
	  global_data->config->numeric_version[2],
	  global_data->config->numeric_version[3]);
  if (job_name)
  {
    fputs("%%Title: ", devout);
    while (*job_name)
    {
      if (*job_name >= 0x20 && *job_name < 0x7f)
        fputc(*job_name, devout);
      else
        fputc('?', devout);

      job_name ++;
    }
    fputc('\n', devout);
  }
  fprintf(devout, "%%%%BoundingBox: 0 0 %d %d\n",
	  options->header.PageSize[0], options->header.PageSize[1]);
  fputs("%%Pages: (atend)\n", devout);
  fputs("%%EndComments\n", devout);

  fputs("%%BeginProlog\n", devout);

  // Number of copies (uncollated and hardware only due to job
  // not being spooled and infinite job supported
  if (job_data->ppd->language_level == 1)
    fprintf(devout, "/#copies %d def\n", options->copies);
  else
    fprintf(devout, "<</NumCopies %d>>setpagedevice\n", options->copies);

  if (job_data->ppd->patches)
  {
    fputs("%%BeginFeature: *JobPatchFile 1\n", devout);
    fputs(job_data->ppd->patches, devout);
    fputs("\n%%EndFeature\n", devout);
  }
  ppdEmit(job_data->ppd, devout, PPD_ORDER_PROLOG);
  fputs("%%EndProlog\n", devout);

  fputs("%%BeginSetup\n", devout);
  ppdEmit(job_data->ppd, devout, PPD_ORDER_DOCUMENT);
  ppdEmit(job_data->ppd, devout, PPD_ORDER_ANY);
  fputs("%%EndSetup\n", devout);

  return (true);
}


//
// 'prPSRasterStartPage()' - Start a raster-to-PostScript page.
//

bool                              // O - `true` on success, `false` on failure
prPSRasterStartPage(
    pappl_job_t       *job,       // I - Job
    pappl_pr_options_t  *options, // I - Job options
    pappl_device_t    *device,    // I - Device
    unsigned          page)       // I - Page number
{
  pr_job_data_t       *job_data;  // PPD data for job
  FILE                *devout;


  job_data = (pr_job_data_t *)papplJobGetData(job);
  devout = job_data->device_file;
  job_data->line_count = 0;

  // Print 1 bit per pixel for monochrome draft printing
  _prOneBitDitherOnDraft(job, options);

  // DSC header
  fprintf(devout, "%%%%Page: (%d) %d\n", page, page);
  fputs("%%BeginPageSetup\n", devout);
  ppdEmit(job_data->ppd, devout, PPD_ORDER_PAGE);
  fputs("%%EndPageSetup\n", devout);

  // Start raster image output
  fprintf(devout, "gsave\n");

  switch (options->header.cupsColorSpace)
  {
    case CUPS_CSPACE_RGB:
    case CUPS_CSPACE_SRGB:
    case CUPS_CSPACE_ADOBERGB:
        fprintf(devout, "/DeviceRGB setcolorspace\n");
	break;

    case CUPS_CSPACE_CMYK:
        fprintf(devout, "/DeviceCMYK setcolorspace\n");
	break;

    default:
    case CUPS_CSPACE_K:
    case CUPS_CSPACE_W:
    case CUPS_CSPACE_SW:
        fprintf(devout, "/DeviceGray setcolorspace\n");
	break;
  }

  fprintf(devout, "%d %d scale\n",
	  options->header.PageSize[0], options->header.PageSize[1]);
  fprintf(devout, "<< \n"
	 "/ImageType 1\n"
	 "/Width %d\n"
	 "/Height %d\n"
	 "/BitsPerComponent %d\n",
	  options->header.cupsWidth, options->header.cupsHeight,
	  options->header.cupsBitsPerColor);

  switch (options->header.cupsColorSpace)
  {
    case CUPS_CSPACE_RGB:
    case CUPS_CSPACE_SRGB:
    case CUPS_CSPACE_ADOBERGB:
        fprintf(devout, "/Decode [0 1 0 1 0 1]\n");
	break;

    case CUPS_CSPACE_CMYK:
        fprintf(devout, "/Decode [0 1 0 1 0 1 0 1]\n");
	break;

    case CUPS_CSPACE_SW:
        fprintf(devout, "/Decode [0 1]\n");
	break;

    default:
    case CUPS_CSPACE_K:
    case CUPS_CSPACE_W:
        fprintf(devout, "/Decode [1 0]\n");
	break;
  }

  fprintf(devout, "/DataSource currentfile /ASCII85Decode filter\n");

  fprintf(devout, "/ImageMatrix [%d 0 0 %d 0 %d]\n",
	  options->header.cupsWidth, -1 * options->header.cupsHeight,
	  options->header.cupsHeight);
  fprintf(devout, ">> image\n");

  return (true);
}


//
// 'prPSRasterWriteLine()' - Write a raster-to-PostScript pixel line.
//

bool				    // O - `true` on success, `false` on failure
prPSRasterWriteLine(
    pappl_job_t         *job,	    // I - Job
    pappl_pr_options_t  *options,   // I - Job options
    pappl_device_t      *device,    // I - Device
    unsigned            y,	    // I - Line number
    const unsigned char *pixels)    // I - Line
{
  pr_job_data_t         *job_data;  // PPD data for job
  FILE *devout;

  job_data = (pr_job_data_t *)papplJobGetData(job);
  devout = job_data->device_file;

  if (job_data->line_count < options->header.cupsHeight)
    _prASCII85(devout, pixels, options->header.cupsBytesPerLine, 0);
  job_data->line_count ++;

  return (true);
}
