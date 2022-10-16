//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// pappl-retrofit.h
//
// Copyright © 2020 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_RETROFIT_PAPPL_RETROFIT_H_
#  define _PAPPL_RETROFIT_PAPPL_RETROFIT_H_

//
// Include necessary headers...
//

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <pappl-retrofit/base.h>
#include <pappl-retrofit/print-job.h>
#include <pappl-retrofit/cups-backends.h>
#include <pappl-retrofit/web-interface.h>
#include <pappl/pappl.h>
#include <ppd/ppd.h>
#include <cupsfilters/ieee1284.h>
#include <cups/cups.h>
#include <cups/dir.h>
#include <limits.h>
#include <poll.h>
#include <regex.h>


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef struct pr_ppd_path_s		// Driver-name/PPD-path pair
{
  const char *driver_name;              // Driver name
  const char *ppd_path;	                // PPD path in collections
} pr_ppd_path_t;

typedef struct ipp_name_lookup_s        // Entry for PPD/IPP option name
                                        // look-up table
{
  const char *ppd;                      // PPD option name
  char       *ipp;                      // Assigned IPP attribute name
} ipp_name_lookup_t;

// Additional driver data specific to the CUPS-driver retro-fitting
// printer applications
typedef struct pr_driver_extension_s	// Driver data extension
{
  ppd_file_t *ppd;                      // PPD file loaded from collection
  const char *vendor_ppd_options[PAPPL_MAX_VENDOR]; // Names of the PPD options
                                        // represented as vendor options;
  cups_array_t *ipp_name_lookup;        // Look-up table for the IPP names
                                        // assigned to vendor PPD options
  int        num_inst_options;          // PPD option settings representing 
  cups_option_t *inst_options;          // presence of installable accressories
  // Special properties taken from the PPD file
  bool       defaults_pollable,         // Are option defaults pollable? 
             installable_options,       // Is there an "Installable Options"
                                        // group?
             installable_pollable,      // "Installable Options" pollable?
             filterless_ps;             // In case of a native PostScript PPD
                                        // is a filter defined which is not
                                        // installed or no filter at all?
  char       *stream_filter;            // CUPS filter to use when printing
                                        // in streaming mode (Raster input)
  pr_stream_format_t *stream_format;    // Filter sequence for streaming
                                        // raster input
  char       *temp_ppd_name;            // File name of temporary copy of the
                                        // PPD file to be used by CUPS filters
  bool       updated;                   // Is the driver data updated for
                                        // "Installable Options" changes?
  pr_printer_app_global_data_t *global_data; // Global data
} pr_driver_extension_t;


//
// Functions...
//

extern int    pr_compare_ppd_paths(void *a, void *b, void *data); 
extern pappl_content_t pr_get_file_content_type(pappl_job_t *job);
extern void   pr_driver_delete(pappl_printer_t *printer,
			       pappl_pr_driver_data_t *driver_data);
extern char   *pr_cups_filter_path(const char *filter,
				   const char *filter_dir);
extern char   *pr_ppd_find_cups_filter(const char *input_format,
				       int num_filters, char **filters,
				       const char *filter_dir);
extern char   *pr_ppd_missing_filters(int num_filters, char **filters,
				      const char *filter_dir);
extern bool   pr_str_has_code(const char *str);
extern bool   pr_option_has_code(pappl_system_t *system, ppd_file_t *ppd,
				 ppd_option_t *option);
extern const char *pr_default_paper_size();
extern bool   pr_driver_setup(pappl_system_t *system, const char *driver_name,
			      const char *device_uri, const char *device_id,
			      pappl_pr_driver_data_t *driver_data,
			      ipp_t **driver_attrs, void *data);
extern bool   pr_have_force_gray(ppd_file_t *ppd,
				 const char **optstr, const char **choicestr);
extern void   pr_media_col(pwg_size_t *pwg_size, const char *def_source,
			   const char *def_type, int left_offset,
			   int top_offset, pappl_media_tracking_t tracking,
			   pappl_media_col_t *col);
extern int    pr_poll_device_option_defaults(pappl_printer_t *printer,
					     bool installable,
					     cups_option_t **defaults);
extern void   pr_printer_update_for_installable_options(
					   pappl_printer_t *printer,
					   pappl_pr_driver_data_t driver_data,
					   const char *instoptstr);
extern void   pr_setup_driver_list(pr_printer_app_global_data_t *global_data);
extern void   pr_setup(pr_printer_app_global_data_t *global_data);
extern void   pr_system_web_add_ppd(pappl_client_t *client, void *data);
extern bool   pr_status(pappl_printer_t *printer);
extern bool   pr_update_status(pappl_printer_t *printer,
			       pappl_device_t *device);
extern const char *pr_testpage(pappl_printer_t *printer, char *buffer,
			       size_t bufsize);
extern pappl_system_t *pr_system_cb(int num_options, cups_option_t *options,
				    void *data);


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_RETROFIT_PAPPL_RETROFIT_H_
