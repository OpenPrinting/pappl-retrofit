//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// cups-backends.h
//
// Copyright © 2020 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_RETROFIT_CUPS_BACKENDS_H_
#  define _PAPPL_RETROFIT_CUPS_BACKENDS_H_

//
// Include necessary headers...
//

#include <pappl-retrofit/pappl-retrofit.h>
#include <pappl/pappl.h>
#include <cupsfilters/log.h>
#include <cupsfilters/filter.h>
#include <signal.h>


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

// Maximum of CUPS backends to run simultaneously for device discovery

#define MAX_BACKENDS	200		// Maximum number of backends we'll run

// Error messages for side channel of CUPS backends
static const char * const pr_cups_sc_status_str[] =
{
  "None",
  "OK",
  "IO Error",
  "Timeout",
  "No response",
  "Bad message",
  "Response to large",
  "Command not implemented",
};


//
// Types...
//

// Data for logging function for CUPS-backend-based device support
typedef struct pr_cups_devlog_data_s
{
  pappl_deverror_cb_t err_cb;
  void                *err_data;
  pappl_system_t      *system;         // For debug logging
} pr_cups_devlog_data_t;

// Device information structure to discover duplicate device reported
// by CUPS backends
typedef struct pr_backend_device_s
{
  char	device_class[128],		// Device class
        device_info[128],		// Device info/description
        device_uri[1024];		// Device URI
} pr_backend_device_t;

// Device data structure to keep a running CUPS backend available as
// PAPPL device
typedef struct pr_cups_device_data_s
{
  char                         *device_uri;  // Device URI
  int                          inputfd,      // FD for job data input
                               backfd,       // FD for back channel
                               sidefd;       // FD for side channel
  int                          backend_pid;  // PID of CUPS backend
  double                       back_timeout, // Timeout back channel (sec)
                               side_timeout; // Timeout side channel (sec)
  pr_printer_app_global_data_t *global_data; // Global data
  pr_cups_devlog_data_t        devlog_data;  // Data for log function
  cf_filter_data_t                *filter_data; // Common data for filter functions
  cf_filter_external_t       backend_params;// Parameters for launching
                                             // backend via ppdFilterExternalCUPS()
  bool                         internal_filter_data; // Is filter_data
                                             // internal?
} pr_cups_device_data_t;


//
// Globals...
//

// Pointer to global data for CUPS backends ("cups" scheme)
// This is the only one global variable needed as papplDeviceAddScheme()
// has no user data pointer
extern void *_PRCUPSDeviceUserData;


//
// Functions...
//

extern bool   _prDummyDevice(const char *device_info, const char *device_uri,
			      const char *device_id, void *data);
extern double _prGetCurrentTime(void);
extern void   _prCUPSDevLog(void *data, cf_loglevel_t level,
			     const char *message, ...);
extern int    _prCUPSCompareDevices(pr_backend_device_t *d0,
				      pr_backend_device_t *d1);
extern void   _prCUPSSigchldSigAction(int sig, siginfo_t *info,
					void *ucontext);
extern bool   _prCUPSDevList(pappl_device_cb_t cb, void *data,
			      pappl_deverror_cb_t err_cb, void *err_data);
extern bool   _prCUPSDevLaunchBackend(pappl_device_t *device);
extern void   _prCUPSDevStopBackend(pappl_device_t *device);
extern bool   _prCUPSDevOpen(pappl_device_t *device, const char *device_uri,
			      const char *name);
extern void   _prCUPSDevClose(pappl_device_t *device);
extern ssize_t _prCUPSDevRead(pappl_device_t *device, void *buffer,
			       size_t bytes);
extern ssize_t _prCUPSDevWrite(pappl_device_t *device, const void *buffer,
				size_t bytes);
extern pappl_preason_t _prCUPSDevStatus(pappl_device_t *device);
extern char   *_prCUPSDevID(pappl_device_t *device, char *buffer,
			     size_t bufsize);


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_RETROFIT_CUPS_BACKENDS_H_
