//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// base.h
//
// Copyright © 2020 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_RETROFIT_BASE_H_
#  define _PAPPL_RETROFIT_BASE_H_

//
// Include necessary headers...
//

#include <pappl/pappl.h>
#include <cupsfilters/filter.h>


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef enum pr_devid_regex_mode_e           // Modes to match a regular
                                             // expression to the value of
                                             // a field in the device ID
{
  PR_DEVID_REGEX_MATCH_ITEM = 0,             // Match one of the
                                             // comma-separated items (like
                                             // PDLs)
  PR_DEVID_REGEX_MATCH_WHOLE_VALUE           // Match the whole value
} pr_devid_regex_mode_t;

typedef struct pr_spooling_conversion_s
{
  const char               *srctype;           // Input data type
  const char               *dsttype;           // Output data type
  int                      num_filters;        // Number of filters
  filter_filter_in_chain_t filters[];          // List of filters with
                                               // parameters
} pr_spooling_conversion_t;

typedef struct pr_stream_format_s
{
  const char               *dsttype;           // Output data type
  pappl_pr_rendjob_cb_t    rendjob_cb;         // End raster job callback
  pappl_pr_rendpage_cb_t   rendpage_cb;        // End raster page callback
  pappl_pr_rstartjob_cb_t  rstartjob_cb;       // Start raster job callback
  pappl_pr_rstartpage_cb_t rstartpage_cb;      // Start raster page callback
  pappl_pr_rwriteline_cb_t rwriteline_cb;      // Write raster line callback
  int                      num_filters;        // Number of filters
  filter_filter_in_chain_t filters[];          // List of filters with
                                               // parameters
} pr_stream_format_t;

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

// Options for components of the retro-fit Printer Application framework to
// be used
enum pr_coptions_e                           // Component option bits
{
  PR_COPTIONS_NONE = 0x0000,                 // No options
  PR_COPTIONS_USE_ONLY_MATCHING_NICKNAMES = 0x0001, // Skip PPD files where the
                                             // *NickName does not match the
                                             // regular expression
                                             // driver_display_regex
  PR_COPTIONS_PPD_NO_EXTRA_PRODUCTS = 0x0002,// Do not generate extra PPD list
                                             // entries by the *Product lines
                                             // in the PPD files
  PR_COPTIONS_NO_GENERIC_DRIVER = 0x0004,    // Do not create a "generic"
                                             // fallback driver entry
  PR_COPTIONS_QUERY_PS_DEFAULTS = 0x0008,    // Support query code in PPDs
  PR_COPTIONS_WEB_ADD_PPDS = 0x0010,         // Support user adding PPDs
  PR_COPTIONS_CUPS_BACKENDS = 0x0020,        // Also use CUPS backends
  PR_COPTIONS_NO_PAPPL_BACKENDS = 0x0040     // Only use CUPS backends
};
typedef unsigned int pr_coptions_t;          // Bitfield for component options

typedef void (*pr_extra_setup_cb_t)(void *data);

// Items to configure the properties of this Printer Application
// These items do not change while the Printer Application is running
typedef struct pr_printer_app_config_s
{
  // Identification of the Printer Application
  const char        *system_name;        // Name of the system
  const char        *system_package_name;// Name of Printer Application
                                         // package/executable
  const char        *version;            // Program version number string
  unsigned short    numeric_version[4];  // Numeric program version
  const char        *web_if_footer;      // HTML Footer for web interface

  // Optional components used in this Printer Application
  pr_coptions_t     components;

  // Callback functions

  // Auto-add (automatic driver assignment) callback
  pappl_pr_autoadd_cb_t autoadd_cb;

  // Printer identify callback (Printer makes noise, lights up display, ...
  // without printing, to find printer under several others)
  pappl_pr_identify_cb_t identify_cb;

  // Print a test page (To check whether configuration is OK)
  pappl_pr_testpage_cb_t testpage_cb;

  // Additional setup steps for the system (like web interface buttons and/or
  // pages, not for particular print queue)
  pr_extra_setup_cb_t extra_setup_cb;

  // Additional setup steps for a print queue (like web interface buttons and/or
  // pages for this print queue)
  pappl_pr_create_cb_t printer_extra_setup_cb;

  // Spooling conversion paths (input and output mime type, filter function,
  // parameters), more desired (simpler) conversions first, less desired
  // later (first match in list gets used)
  cups_array_t      *spooling_conversions;

  // Supported data formats to get from streaning Raster input and the
  // needed callback functions (output mime type, 5 callback functions:
  // start/end job, start/emd page, output raster line), more desired formats
  // (streamability) first: CUPS Raster, PosdtScript, PDF (we will actually
  // send PCLm, so that at least some printers stream).
  cups_array_t      *stream_formats;

  // CUPS backends to be ignored (comman-separated list, empty or NULL
  // for allowing all backends)
  const char        *backends_ignore;

  // CUPS backends to use exclusively (comman-separated list, empty or
  // NULL for including all backends)
  const char        *backends_only;

  // Data for the test page callback function
  // For pr_testpage() this is simply the file name of the only one test
  // page without directory
  void              *testpage_data;

  // Regular expression to select the part of the PPD's *NickName
  // which is not the printer make/model name any more. This part
  // gives extra info about PostScript versions, drivers, ... If a
  // Printer Application includes more than one driver option for a
  // printer, this extra information is valuable and should be visible
  // in the model/driver list entries, so that the PPDs for different
  // drivers on the same model are not skipped as duplicate and the
  // driver name can also be used for both manual and automatic driver
  // selection.
  //
  // Thee regular expression must match the whole extra information,
  // beginning from the character right after the model name. If it
  // contains parantheses, the substring in the first matching pair of
  // parantheses will be considered the driver name and displayed
  // after the model name in the driver list entry. Otherwise the
  // whole extra information string, up to the end of the *Nickname
  // will be displayed.
  //
  // If the regular expression is " +Foomatic/(.+)$", the *Nickname
  //
  //   "Brother DCP-7020 Foomatic/hl1250 (recommended)"
  //
  // will appear in the model/driver list as
  //
  //   "Brother DCP-7020, hl1250 (recommended)"
  //
  // Use NULL for not using this facility
  const char        *driver_display_regex;

  // The function to automatically find the best PPD for a printer
  // given by its device ID, pr_best_matching_ppd(), to be used by the
  // auto-add callbacks, uses these regular expressions to prioritize
  // between PPD files if they are for the same printer model and the
  // same UI language. A matching PPD is prioritized against a
  // non-matching and between two matching the one where the earlier
  // regular expression in the list matches.
  //
  // The string to match the regular expression against is NOT the
  // human-readable *NickName, but the driver name, which is the
  // driver entry of the PPD list, converted into IPP attribute style.
  //
  // If the regular expression list is (note especially how Foomatic
  // driver names with numbers translate into PAPPL driver name
  // components)
  //
  //    "-recommended-"
  //    "-postscript-"
  //    "-pxlcolor-"
  //    "-pxlmono-"
  //    "-ljet-4-d-"
  //    "-ljet-4-"
  //
  // and the PPDs for our printer Acme LaserStar 100 got the following
  // driver names
  //
  //   "acme--laserstar-100--ljet4d-recommended-en"
  //   "acme--laserstar-100--pxlmono-en"
  //   "acme--laserstar-100--laserstar-en"
  //
  // The first entry has highest priority, the third entry lowest.
  //
  // Use NULL for not using this facility
  cups_array_t      *driver_selection_regex_list;
} pr_printer_app_config_t;

// Global variables for this Printer Application.
// Note that the Printer Application can only run one system at the same time
// Items adjustable by command line options and environment variables and also
// values obtained at run time
typedef struct pr_printer_app_global_data_s
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
                                         // via BACKENF_DIR environment
                                         // variable
  char              testpage_dir[1024];  // Test page directory, customizable
                                         // via TESTPAGE_DIR environment
                                         // variable
  // State file
  char              state_file[1024];    // State file, customizable via
                                         // STATE_FILE environment variable
} pr_printer_app_global_data_t;


//
// Functions...
//

extern int pr_retrofit_printer_app(pr_printer_app_config_t *printer_app_config,
				   int argc, char *argv[]);
extern const char *pr_best_matching_ppd(const char *device_id,
					pr_printer_app_global_data_t
					*global_data);
extern int  pr_regex_match_devid_field(const char *device_id,
				       const char *key,
				       const char *value_regex,
				       pr_devid_regex_mode_t mode);
extern bool pr_supports_postscript(const char *device_id);
extern bool pr_supports_pdf(const char *device_id);
extern bool pr_supports_pcl5(const char *device_id);
extern bool pr_supports_pcl5c(const char *device_id);
extern bool pr_supports_pclxl(const char *device_id);
extern const char *pr_autoadd(const char *device_info, const char *device_uri,
			      const char *device_id, void *data);
extern void pr_ps_identify(pappl_printer_t *printer, pappl_device_t *device);
extern void pr_identify(pappl_printer_t *printer,
			pappl_identify_actions_t actions,
			const char *message);
extern const char *pr_testpage(pappl_printer_t *printer, char *buffer,
			       size_t bufsize);
extern bool   pr_pwg_rendjob(pappl_job_t *job, pappl_pr_options_t *options,
			      pappl_device_t *device);
extern bool   pr_pwg_rendpage(pappl_job_t *job, pappl_pr_options_t *options,
			       pappl_device_t *device, unsigned page);
extern bool   pr_pwg_rstartjob(pappl_job_t *job, pappl_pr_options_t *options,
				pappl_device_t *device);
extern bool   pr_pwg_rstartpage(pappl_job_t *job, pappl_pr_options_t *options,
				 pappl_device_t *device, unsigned page);
extern bool   pr_pwg_rwriteline(pappl_job_t *job, pappl_pr_options_t *options,
				 pappl_device_t *device, unsigned y,
				 const unsigned char *pixels);
extern bool   pr_ps_rendjob(pappl_job_t *job, pappl_pr_options_t *options,
			    pappl_device_t *device);
extern bool   pr_ps_rendpage(pappl_job_t *job, pappl_pr_options_t *options,
			     pappl_device_t *device, unsigned page);
extern bool   pr_ps_rstartjob(pappl_job_t *job, pappl_pr_options_t *options,
			      pappl_device_t *device);
extern bool   pr_ps_rstartpage(pappl_job_t *job, pappl_pr_options_t *options,
			       pappl_device_t *device, unsigned page);
extern bool   pr_ps_rwriteline(pappl_job_t *job, pappl_pr_options_t *options,
			       pappl_device_t *device, unsigned y,
			       const unsigned char *pixels);
extern void   pr_setup_add_ppd_files_page(void *data);
extern void   pr_setup_device_settings_page(pappl_printer_t *printer,
					    void *data);


//
// Spooling conversions
//

static pr_spooling_conversion_t pr_convert_pdf_to_pdf =
{
  "application/pdf",
  "application/vnd.cups-pdf",
  1,
  {
    {
      pdftopdf,
      "application/vnd.cups-pdf",
      "pdftopdf"
    }
  }
};

static pr_spooling_conversion_t pr_convert_pdf_to_ps =
{
  "application/pdf",
  "application/vnd.cups-postscript",
  2,
  {
    {
      pdftopdf,
      "application/vnd.cups-postscript",
      "pdftopdf"
    },
    {
      pdftops,
      NULL,
      "pdftops"
    }
  }
};

static pr_spooling_conversion_t pr_convert_pdf_to_raster =
{
  "application/pdf",
  "application/vnd.cups-raster",
  2,
  {
    {
      pdftopdf,
      "application/vnd.cups-raster",
      "pdftopdf"
    },
    {
      ghostscript,
      &((filter_out_format_t){OUTPUT_FORMAT_CUPS_RASTER}),
      "ghostscript"
    }
  }
};

static pr_spooling_conversion_t pr_convert_pdf_to_raster_poppler =
{
  "application/pdf",
  "application/vnd.cups-raster",
  2,
  {
    {
      pdftopdf,
      "application/vnd.cups-raster",
      "pdftopdf"
    },
    {
      pdftoraster,
      &((filter_out_format_t){OUTPUT_FORMAT_CUPS_RASTER}),
      "pdftoraster"
    }
  }
};

static pr_spooling_conversion_t pr_convert_ps_to_ps =
{
  "application/postscript",
  "application/vnd.cups-postscript",
  1,
  {
    {
      pstops,
      NULL,
      "pstops"
    }
  }
};

static pr_spooling_conversion_t pr_convert_ps_to_pdf =
{
  "application/postscript",
  "application/vnd.cups-pdf",
  2,
  {
    {
      ghostscript,
      &((filter_out_format_t){OUTPUT_FORMAT_PDF}),
      "ghostscript"
    },
    {
      pdftopdf,
      "application/vnd.cups-pdf",
      "pdftopdf"
    }
  }
};

static pr_spooling_conversion_t pr_convert_ps_to_raster =
{
  "application/postscript",
  "application/vnd.cups-raster",
  2,
  {
    {
      pstops,
      NULL,
      "pstops"
    },
    {
      ghostscript,
      &((filter_out_format_t){OUTPUT_FORMAT_CUPS_RASTER}),
      "ghostscript"
    }
  }
};


//
// Stream formats
//

static pr_stream_format_t pr_stream_cups_raster =
{
  "application/vnd.cups-raster",
  pr_pwg_rendjob,
  pr_pwg_rendpage,
  pr_pwg_rstartjob,
  pr_pwg_rstartpage,
  pr_pwg_rwriteline,
  1,
  {
    {
      pwgtoraster,
      NULL,
      "pwgtoraster"
    }
  }
};

static pr_stream_format_t pr_stream_postscript =
{
  "application/vnd.cups-postscript",
  pr_ps_rendjob,
  pr_ps_rendpage,
  pr_ps_rstartjob,
  pr_ps_rstartpage,
  pr_ps_rwriteline,
  0
};

static pr_stream_format_t pr_stream_pdf =
{
  "application/vnd.cups-pdf",
  pr_ps_rendjob,
  pr_ps_rendpage,
  pr_ps_rstartjob,
  pr_ps_rstartpage,
  pr_ps_rwriteline,
  2,
  {
    {
      ghostscript,
      &((filter_out_format_t){OUTPUT_FORMAT_PDF_IMAGE}),
      "ghostscript"
    },
    {
      pdftopdf,
      "application/vnd.cups-pdf",
      "pdftopdf"
    }
  }
};


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_RETROFIT_BASE_H_
