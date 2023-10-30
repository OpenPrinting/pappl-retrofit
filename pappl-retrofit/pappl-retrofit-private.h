//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// pappl-retrofit-private.h
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

#include <pappl-retrofit/pappl-retrofit.h>
#include <pappl-retrofit/print-job-private.h>
#include <pappl-retrofit/cups-backends-private.h>
#include <pappl-retrofit/cups-side-back-channel-private.h>
#include <pappl-retrofit/web-interface-private.h>
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
  char       *human_strings;            // Table of human-readable strings
                                        // from the PPD file, for displaying
                                        // the vendor options in the web UI
  char       *human_strings_resource;   // Resource under which we registered
                                        // the human-readable strings
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

// Properties of CUPS backends running in discovery mode to find supported
// devices
typedef struct pr_backend_s
{
  char		*name;			// Name of backend
  int		pid,			// Process ID
                status;			// Exit status
  int		pipe;			// Pipe from backend stdout
  int		count;			// Number of devices found
  char          buf[4096];              // Buffer to hold backend output
  size_t        bytes;                  // Bytes in the buffer
  bool          done;                   // Sub-process finished?
} pr_backend_t;

// Global variables for this Printer Application.
// Note that the Printer Application can only run one system at the same time
// Items adjustable by command line options and environment variables and also
// values obtained at run time
struct pr_printer_app_global_data_s
{
  pr_printer_app_config_t *config;
  pappl_system_t          *system;
  int                     num_drivers;     // Number of drivers (from the PPDs)
  pappl_pr_driver_t       *drivers;        // Driver index (for menu and
                                           // auto-add)
  cups_array_t            *ppd_paths,      // List of the paths to each PPD
                          *ppd_collections;// List of all directories providing
                                           // PPD files
  pr_backend_t            *backend_list;   // Pointer to list of CUPS backends
                                           // running in discovery mode to find
                                           // devices, for access by SIGCHLD
                                           // handler
  // Directories for auxiliary files and components
  char              state_dir[1024];     // State/config file directory,
                                         // customizable via STATE_DIR
                                         // environment variable
  char              ppd_dirs_list[1023]; // Environment variable PPD_DIRS
                                         // with the PPD directories
  char              user_ppd_dir[1024];  // Directory where PPDs
                                         // added by the user are held
  char              spool_dir[1024];     // Spool directory, customizable via
                                         // SPOOL_DIR environment variable
  char              filter_dir[1024];    // Filter directory, customizable
                                         // via FILTER_DIR environment
                                         // variable
  char              backend_dir[1024];   // Backend directory, customizable
                                         // via BACKEND_DIR environment
                                         // variable
  char              testpage_dir[1024];  // Test page directory, customizable
                                         // via TESTPAGE_DIR environment
                                         // variable
  // State file
  char              state_file[1024];    // State file, customizable via
                                         // STATE_FILE environment variable
};


//
// Functions...
//

extern int    _prComparePPDPaths(void *a, void *b, void *data);
extern void   _prDriverDelete(pappl_printer_t *printer,
			      pappl_pr_driver_data_t *driver_data);
extern char   *_prCUPSFilterPath(const char *filter,
				 const char *filter_dir);
extern char   *_prPPDFindCUPSFilter(const char *input_format,
				    int num_filters, char **filters,
				    const char *filter_dir);
extern char   *_prPPDMissingFilters(int num_filters, char **filters,
				    const char *filter_dir);
extern bool   _prStrHasCode(const char *str);
extern bool   _prOptionHasCode(pappl_system_t *system, ppd_file_t *ppd,
			       ppd_option_t *option);
extern bool   _prDriverSetup(pappl_system_t *system, const char *driver_name,
			     const char *device_uri, const char *device_id,
			     pappl_pr_driver_data_t *driver_data,
			     ipp_t **driver_attrs, void *data);
extern bool   _prHaveForceGray(ppd_file_t *ppd,
			       const char **optstr, const char **choicestr);
extern void   _prMediaCol(pwg_size_t *pwg_size, const char *def_source,
			  const char *def_type, int left_offset,
			  int top_offset, pappl_media_tracking_t tracking,
			  pappl_media_col_t *col);
extern int    _prPollDeviceOptionDefaults(pappl_printer_t *printer,
					  bool installable,
					  cups_option_t **defaults);
extern void   _prPrinterUpdateForInstallableOptions(
					   pappl_printer_t *printer,
					   pappl_pr_driver_data_t driver_data,
					   const char *instoptstr);
extern void   _prSetupDriverList(pr_printer_app_global_data_t *global_data);
extern void   _prSetup(pr_printer_app_global_data_t *global_data);
extern bool   _prStatus(pappl_printer_t *printer);
extern bool   _prUpdateStatus(pappl_printer_t *printer,
			      pappl_device_t *device);
extern pappl_system_t *_prSystemCB(int num_options, cups_option_t *options,
				   void *data);


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_RETROFIT_PAPPL_RETROFIT_H_
