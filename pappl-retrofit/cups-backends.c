//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// cups-backends.c
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

#include <pappl-retrofit/cups-backends.h>
#include <pappl-retrofit/base.h>
#include <cupsfilters/ieee1284.h>
#include <cups/dir.h>
#include <poll.h>


//
// Globals...
//

// Pointer to global data for CUPS backends ("cups" scheme)
// This is the only one global variable needed as papplDeviceAddScheme()
// has no user data pointer
void *pr_cups_device_user_data;


//
// Callback function to make papplDeviceList() initialize PAPPL's standard
// themes but not actually list anything
//

bool
pr_dummy_device(const char *device_info,
		const char *device_uri,
		const char *device_id,
		void *data)
{
  return true;
}


//
// 'pr_get_current_time()' - Get the current time as a double value in seconds
//

double				/* O - Time in seconds */
pr_get_current_time(void)
{
  struct timeval	curtime;	/* Current time */


  gettimeofday(&curtime, NULL);

  return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
}


//
// 'pr_cups_devlog()' - Logging function for pr_cups_devlist(), logs
//                      on the system for everything which is not an
//                      error and on the device for errors (here only
//                      errors are supported, only when a device error
//                      callback is provided). Control messages are
//                      considered as debug messages.
//

void
pr_cups_devlog(void *data,
	       filter_loglevel_t level,
	       const char *message,
	       ...)
{
  va_list       ap;                     // Pointer to additional args
  char          buffer[8192];           // Formatted message
  pr_cups_devlog_data_t *devlog_data = (pr_cups_devlog_data_t *)data;


  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  switch(level)
  {
    case FILTER_LOGLEVEL_CONTROL:
      buffer[sizeof(buffer) - 18] = '\0';
      memmove(buffer + 17, buffer, strlen(buffer) + 1);
      memcpy(buffer, "Control message: ", 17);
      level = FILTER_LOGLEVEL_DEBUG;
    default:
    case FILTER_LOGLEVEL_UNSPEC:
    case FILTER_LOGLEVEL_DEBUG:
    case FILTER_LOGLEVEL_INFO:
    case FILTER_LOGLEVEL_WARN:
      papplLog(devlog_data->system, level, "%s", buffer);
      break;
    case FILTER_LOGLEVEL_ERROR:
    case FILTER_LOGLEVEL_FATAL:
      if (devlog_data->err_cb)
	(*(devlog_data->err_cb))(buffer, devlog_data->err_data);
      else
	papplLog(devlog_data->system, level, "%s", buffer);
      break;
  }
}


//
// 'pr_cups_compare_devices()' - Compare device names to eliminate duplicates
//                               with the help of a sorted CUPS array
//

int				        // O - Result of comparison
pr_cups_compare_devices(pr_backend_device_t *d0,// I - First device
			pr_backend_device_t *d1)// I - Second device
{
  char          buf0[1024], buf1[1024]; // Buffers for normalizing strings
  int		diff;			// Difference between strings


  // Sort devices by device-info, device-class, and device-uri...
  if ((diff =
       strcasecmp(ieee1284NormalizeMakeAndModel(d0->device_info, NULL,
						IEEE1284_NORMALIZE_COMPARE |
						IEEE1284_NORMALIZE_LOWERCASE |
						IEEE1284_NORMALIZE_SEPARATOR_SPACE |
						IEEE1284_NORMALIZE_PAD_NUMBERS,
						NULL, buf0, sizeof(buf0),
						NULL, NULL, NULL),
		  ieee1284NormalizeMakeAndModel(d1->device_info, NULL,
						IEEE1284_NORMALIZE_COMPARE |
						IEEE1284_NORMALIZE_LOWERCASE |
						IEEE1284_NORMALIZE_SEPARATOR_SPACE |
						IEEE1284_NORMALIZE_PAD_NUMBERS,
						NULL, buf1, sizeof(buf1),
						NULL, NULL, NULL))) != 0)
    return (diff);
  else if ((diff = strcasecmp(d0->device_class, d1->device_class)) != 0)
    return (diff);
  else
    return (strcasecmp(d0->device_uri, d1->device_uri));
}


//
// 'pr_cups_sigchld_sigaction()' - Handle 'child' signals from finished
//                                 CUPS backend processes
//

void
pr_cups_sigchld_sigaction(int sig,		// I - Signal number (unused)
			  siginfo_t *info,	// I - Signal info
			  void *ucontext)	// I - Context info (unused)
{
  int i;
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)pr_cups_device_user_data;
  pr_backend_t *backend_list = global_data->backend_list;

  (void)sig;
  (void)ucontext;

  // One of the backends terminated, mark it as done and add the status
  // to its record
  for (i = 0; backend_list[i].name && i < MAX_BACKENDS; i ++)
    if (backend_list[i].pid == info->si_pid)
    {
      papplLog(global_data->system, PAPPL_LOGLEVEL_DEBUG,
	       "Backend '%s' triggered SIGCHLD", backend_list[i].name);
      backend_list[i].done = true;
      backend_list[i].status =
	(info->si_status == SIGTERM ? 0 : info->si_status);
    }
}


//
// 'pr_cups_devlist()' - List all devices which get discovered by the
//                       CUPS backends in our specified CUPS backend
//                       directory, taking into account include and
//                       exclude lists.  Resulting CUPS device URIs
//                       are prepended by "cups:" as this is the
//                       device list callback function of our custom
//                       "cups" scheme. The backends are always run as
//                       the same user as the Printer Application, so
//                       backends which require root are skipped when
//                       running as normal user (A Printer Application
//                       in a Snap runs as root). The backends are run
//                       in the filterExternalCUPS() filter function,
//                       so their environment is as close to CUPS as
//                       possible. For the implementation I mostly
//                       followed scheduler/cups-deviced.c from
//                       CUPS. It is rather complex, but this is to
//                       make the backends run in parallel, as
//                       especially the network backends take some
//                       time for their discovery run. Only this way
//                       we can keep the response time always
//                       reasonable.
//

bool
pr_cups_devlist(pappl_device_cb_t cb,
		void *data,
		pappl_deverror_cb_t err_cb,
		void *err_data)
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)pr_cups_device_user_data;
  pr_cups_devlog_data_t devlog_data;
  filter_data_t filter_data;
  filter_external_cups_t backend_params;
  char          buf[2048];
  bool          ret = false;
  int		num_backends = 0,
				// Total backends
		active_backends = 0;
				// Active backends
  pr_backend_t  backends[MAX_BACKENDS];
				// Array of backends
  struct pollfd	backend_fds[MAX_BACKENDS];
  				// Array for poll()
  cups_array_t	*devices = NULL;// Array of devices
  int		i;		// Looping var
  struct sigaction action;	// Actions for POSIX signals
  struct sigaction old_action;	// Backup of Actions for POSIX signals
  cups_dir_t	*dir;		// Directory pointer
  cups_dentry_t *dent;		// Directory entry
  int		timeout;	// Timeout in seconds
  int		status;		// Exit status of child
  int		pid;		// Process ID of child
  double	current_time,	// Current time
		end_time;	// Ending time
  pr_backend_t  *backend;	// Current backend
  const char	*name;		// Name of process
  char	        line[2048],	// Line from backend, for logging
	        *newline,       // Where in the buffer starts the next line?
	        *ptr1,		// Pointer into line
	        *ptr2,		// Pointer into line
	        *dclass,	// Device class
	        *uri,		// Device URI
	        *info,		// Device info
	        *device_id;	// 1284 device ID
  size_t        bytes;          // Bytes read from pipe
  pr_backend_device_t *device;  // New device

  // Common arguments amd parameters for calling the CUPS backends in
  // discovery mode
  memset(&devlog_data, 0, sizeof(devlog_data));
  devlog_data.err_cb = err_cb;
  devlog_data.err_data = err_data;
  devlog_data.system = global_data->system;
  memset(&filter_data, 0, sizeof(filter_data));
  filter_data.back_pipe[0] = -1;
  filter_data.back_pipe[1] = -1;
  filter_data.side_pipe[0] = -1;
  filter_data.side_pipe[1] = -1;
  filter_data.logfunc = pr_cups_devlog;
  filter_data.logdata = &devlog_data;

  // Initialize backends list and link with global data
  memset(backends, 0, sizeof(backends));
  global_data->backend_list = backends;

  // Listen to child signals to get note of backends which have finished or
  // errorred to take their status and remove them from the poll
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = pr_cups_sigchld_sigaction;
  sigaction(SIGCHLD, &action, &old_action);

  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
		 "Backend directory: %s; Ignoring backends: %s; Using only backends: %s",
		 global_data->backend_dir,
		 (global_data->config->backends_ignore &&
		  global_data->config->backends_ignore[0] ?
		  global_data->config->backends_ignore : "(none)"),
		 (global_data->config->backends_only &&
		  global_data->config->backends_only[0] ?
		  global_data->config->backends_only : "(all)"));

  // Open the backend directory and start the selected backends in
  // discovery mode (without arguments)

  if ((dir = cupsDirOpen(global_data->backend_dir)) == NULL)
  {
    pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
		   "Unable to open backend directory '%s': %s",
		   global_data->backend_dir, strerror(errno));
  }
  else 
  {
    // Setup the devices array...
    devices = cupsArrayNew3((cups_array_func_t)pr_cups_compare_devices,
			    NULL, NULL, 0, NULL, (cups_afree_func_t)free);

    // Go through the backends
    while ((dent = cupsDirRead(dir)) != NULL)
    {
      // Put together full path of the backend file
      snprintf(buf, sizeof(buf), "%s/%s",
	       global_data->backend_dir, dent->filename);

      // Skip entries that are not executable files...
      if (!S_ISREG(dent->fileinfo.st_mode) ||
	  !isalnum(dent->filename[0] & 255) ||
	  (dent->fileinfo.st_mode & (S_IRUSR | S_IXUSR)) !=
	  (S_IRUSR | S_IXUSR) ||
	  (getuid() &&
	   (dent->fileinfo.st_mode & (S_IRGRP | S_IXGRP)) !=
	   (S_IRGRP | S_IXGRP) &&
	   (dent->fileinfo.st_mode & (S_IROTH | S_IXOTH)) !=
	   (S_IROTH | S_IXOTH)))
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
		       "Backend '%s' not executable, skipping",
		       dent->filename);
	continue;
      }

      // Are backend file properties suitable for secure use by root?
      if (!geteuid() &&
	  (dent->fileinfo.st_uid ||              // 1. Must be owned by root
	   (dent->fileinfo.st_mode & S_IWGRP) || // 2. Must not be writable by
	                                         //    group
	   (dent->fileinfo.st_mode & S_ISUID) || // 3. Must not be setuid
	   (dent->fileinfo.st_mode & S_IWOTH)))  // 4. Must not be writable by
	                                         //    others
      {
      	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_WARN,
		       "Backend '%s' has unsafe permissions/ownership to be run as root, skipping",
		       dent->filename);
	continue;
      }

      // Skip excluded backends...
      if (global_data->config->backends_ignore &&
	  global_data->config->backends_ignore[0] &&
	  (ptr1 = strstr(global_data->config->backends_ignore,
			 dent->filename)) != NULL &&
	  (ptr1 == global_data->config->backends_ignore ||
	   !isalnum(*(ptr1 - 1))) &&
	  !isalnum(ptr1[strlen(dent->filename)]))
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
		       "Backend '%s' not considered as it is on the exclude list",
		       dent->filename);
	continue;
      }

      // Skip not included backends...
      if (global_data->config->backends_only &&
	  global_data->config->backends_only[0] &&
	  !((ptr1 = strstr(global_data->config->backends_only,
			   dent->filename)) != NULL &&
	    (ptr1 == global_data->config->backends_only ||
	     !isalnum(*(ptr1 - 1))) &&
	    !isalnum(ptr1[strlen(dent->filename)])))
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
		       "Backend '%s' not considered as it is not on the include list",
		       dent->filename);
	continue;
      }

      // Do not run too many backends, only as much as we have space in our
      // backend array
      if (num_backends >= MAX_BACKENDS)
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_WARN,
		       "Too many backends (%d)!\n", num_backends);
	break;
      }
      backend = backends + num_backends;

      // Prepare parameters of filterExternalCUPS() filter function call
      // for running the CUPS backend in discovery mode (without arguments)
      memset(&backend_params, 0, sizeof(backend_params));
      backend_params.filter = strdup(buf);
      backend_params.is_backend = 2; // Run backend in discovery mode, w/o args

      // Fill in the backend information...
      backend->name   = strdup(dent->filename);
      backend->status = 0;
      backend->count  = 0;
      backend->bytes  = 0;
      backend->done   = false;

      // Terminating zero mark for SIGCHLD handler
      if (num_backends + 1 < MAX_BACKENDS)
	backends[num_backends + 1].name = NULL;

      // Launch the backend with pipe providing backend's stdout
      if ((backend->pipe = filterPOpen(filterExternalCUPS,
				       open("/dev/null", O_RDWR), -1,
				       0, &filter_data, &backend_params,
				       &(backend->pid))) == 0)
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
		       "Unable to execute '%s' - %s\n",
		       backend_params.filter, strerror(errno));
	continue;
      }

      // Set the output pipes of the backends non-blocking so that if we read
      // over the end of an output line before the backend terminates that we
      // do not get blocked until the next output line or the end of this
      // backend
      if (fcntl(backend->pipe, F_SETFD,
		fcntl(backend->pipe, F_GETFD) | O_NONBLOCK))
      {
	pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
		       "Unable to set output pipe of '%s' to non-blocking- %s\n",
		       backend_params.filter, strerror(errno));
	filterPClose(backend->pipe, backend->pid, &filter_data);
	continue;
      }

      pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
		     "Started backend %s (PID %d)",
		     backend_params.filter, backend->pid);
      
      backend_fds[num_backends].fd     = backend->pipe;
      backend_fds[num_backends].events = POLLIN;

      active_backends ++;
      num_backends ++; 
    }
    cupsDirClose(dir);

    // Collect devices

    // Timeout of 15 seconds (same as CUPS)
    end_time = pr_get_current_time() + 15;

    while (!ret &&
	   active_backends > 0 &&
	   (current_time = pr_get_current_time()) < end_time)
    {
      // Collect the output from the backends, but only within the timeout
      //timeout = (int)(1000 * (end_time - current_time));
      timeout = 1000;
      if (poll(backend_fds, (nfds_t)num_backends, timeout) > 0)
      {
	for (i = 0; i < num_backends; i ++)
	  if (backend_fds[i].revents && backends[i].pipe)
	  {
	    while ((bytes =
		    read(backends[i].pipe,
			 backends[i].buf + backends[i].bytes,
			 sizeof(backends[i].buf) - backends[i].bytes)) > 0)
	    {
	      // Parse the output lines

	      //
	      // Each line is of the form:
	      //
	      //   class URI "make model" "name" ["1284 device ID"] ["location"]
	      //

	      backends[i].bytes += bytes;
	      while ((newline = strchr(backends[i].buf, '\n')) != NULL)
	      {
		// We have read at least one line
		*newline = '\0';
		newline ++;

		// Save the line for logging
		strncpy(line, backends[i].buf, sizeof(line) - 1);

		// device-class
		ptr1 = backends[i].buf;
		for (dclass = ptr1; *ptr1; ptr1 ++)
		  if (isspace(*ptr1 & 255))
		    break;
		while (isspace(*ptr1 & 255))
		  *ptr1++ = '\0';

		// device-uri
		if (!*ptr1)
		  goto error;
		for (uri = ptr1; *ptr1; ptr1 ++)
		  if (isspace(*ptr1 & 255))
		    break;
		while (isspace(*ptr1 & 255))
		  *ptr1++ = '\0';

		// Check whether we have discovered an actual device here
		// and not something like
		//
		//    network socket "Unknown" "AppSocket/HP JetDirect"
		//
		if (!strchr(uri, ':'))
		{
		  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
				 "Non-device output line from '%s': %s",
				 backends[i].name, line);
		  goto nextline;
		}

		// device-make-and-model
		if (*ptr1 != '\"')
		  goto error;
		for (ptr1 ++; *ptr1 && *ptr1 != '\"'; ptr1 ++)
		{
		  if (*ptr1 == '\\' && ptr1[1])
		    for (ptr2 = ptr1; *ptr2; ptr2 ++)
		      *ptr2 = *(ptr2 + 1);
		}
		if (*ptr1 != '\"')
		  goto error;
		for (*ptr1++ = '\0'; isspace(*ptr1 & 255); *ptr1++ = '\0');

		// device-info
		if (*ptr1 != '\"')
		  goto error;
		for (ptr1 ++, info = ptr1; *ptr1 && *ptr1 != '\"'; ptr1 ++)
		{
		  if (*ptr1 == '\\' && ptr1[1])
		    for (ptr2 = ptr1; *ptr2; ptr2 ++)
		      *ptr2 = *(ptr2 + 1);
		}
		if (*ptr1 != '\"')
		  goto error;
		for (*ptr1++ = '\0'; isspace(*ptr1 & 255); *ptr1++ = '\0');

		// device-id
		if (*ptr1 == '\"')
	        {
		  for (ptr1 ++, device_id = ptr1; *ptr1 && *ptr1 != '\"';
		       ptr1 ++)
		  {
		    if (*ptr1 == '\\' && ptr1[1])
		      for (ptr2 = ptr1; *ptr2; ptr2 ++)
			*ptr2 = *(ptr2 + 1);
		  }
		  if (*ptr1 != '\"')
		    goto error;
		  for (*ptr1++ = '\0'; isspace(*ptr1 & 255); *ptr1++ = '\0');

		  // device-location
		  if (*ptr1 == '\"')
		  {
		    for (ptr1 ++; *ptr1 && *ptr1 != '\"'; ptr1 ++)
		    {
		      if (*ptr1 == '\\' && ptr1[1])
			for (ptr2 = ptr1; *ptr2; ptr2 ++)
			  *ptr2 = *(ptr2 + 1);
		    }
		    if (*ptr1 != '\"')
		      goto error;
		    *ptr1 = '\0';
		  }
		}
		else
		  device_id = NULL;

		// Add device entry to list/submit (use list only to detect
		// duplicates)

		// Allocate memory for the device record...
		if ((device = calloc(1, sizeof(pr_backend_device_t))) == NULL)
		{
		  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
				 "Ran out of memory allocating a device!");
		  goto nextline;
		}

		// Copy the strings over...
		strncpy(device->device_class, dclass,
			sizeof(device->device_class) - 1);
		strncpy(device->device_info, info,
			sizeof(device->device_info) - 1);
		snprintf(device->device_uri, sizeof(device->device_uri),
			 "cups:%s", uri);

		// Check whether we have a duplicate and if so, skip it
		if (cupsArrayFind(devices, device))
		{
		  free(device);
		  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
				 "Duplicate device from backend '%s' skipped: %s (URI: %s Device ID: %s)",
				 backends[i].name, info, device->device_uri,
				 device_id);
		}
		else
		{
		  // Add new entry to the list
		  cupsArrayAdd(devices, device);

		  // If there is more than one backend, mark device info with
		  // with backend name
		  snprintf(buf, sizeof(buf), "%s%s%s%s", info,
			   (num_backends > 1 ? " (" : ""),
			   (num_backends > 1 ? backends[i].name : ""),
			   (num_backends > 1 ? ")" : ""));
		  if (num_backends > 1)
		    for (ptr1 = buf + strlen(info) + 2; *ptr1 != ')'; ptr1 ++)
		      *ptr1 = toupper(*ptr1);

		  // Submit device info...
		  ret = (*cb)(buf, device->device_uri, device_id, data);
		  backends[i].count ++;
		  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
				 "Device from backend '%s' added to list of available devices: %s (URI: %s Device ID: %s)",
				 backends[i].name, buf,
				 device->device_uri, device_id);
		  if (ret)
		    // Callback returned "true", stop process here
		    goto stop_process;
		}

		goto nextline;

	      error:
		// Bad format; strip trailing newline and write an
		// error message.
		pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
			       "Bad line from '%s': %s",
			       backends[i].name, line);

	      nextline:
		// Move rest of the buffer content to the beginning of
		// the buffer
		backends[i].bytes -= (newline - backends[i].buf);
	        memmove(backends[i].buf, newline, backends[i].bytes);
	      }
	    }
	    if (bytes == 0)
	    {
	      // Backend terminated
	      backends[i].done = true;
	    }
	    else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
	    {
	      // An error occured (not simply no further bytes due to the
	      // backend to take time to find the next device)
	      pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
			     "Read error from backend '%s' - %s",
			     backends[i].name, strerror(errno));
	      close(backends[i].pipe);
	      backends[i].pipe = 0;
	      kill(backends[i].pid, SIGTERM);
	      pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
			     "PID %d (%s) killed after read error!",
			     backends[i].pid, backends[i].name);
	    }
	  }
      }

    stop_process:
      // Log exit status from terminated children and close pipes

      for (i = num_backends, backend = backends; i > 0; i --, backend ++)
	if (backend->done && backend->pid)
	{
	  filterPClose(backend->pipe, backend->pid, &filter_data);
	  pid             = backend->pid;
	  name            = backend->name;
	  status          = backend->status;
	  if (status)
	  {
	    if (WIFEXITED(status))
	      pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
			     "PID %d (%s) stopped with status %d!",
			     pid, name, WEXITSTATUS(status));
	    else
	      pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_ERROR,
			     "PID %d (%s) crashed on signal %d!",
			     pid, name, WTERMSIG(status));
	  }
	  else
	    pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
			   "PID %d (%s) exited with no errors.",
			   pid, name);
	  if (backend->count)
	    pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
			   "Found %d devices using the '%s' backend",
			   backend->count, name);
	  backend->pid    = 0;
	  backend->pipe   = 0;
	  active_backends --;
	}
    }

    // Terminate any remaining backends and exit...

    if (active_backends > 0)
    {
      for (i = 0; i < num_backends; i ++)
	if (backends[i].pid)
	{
	  kill(backends[i].pid, SIGTERM);
	  pr_cups_devlog(&devlog_data, PAPPL_LOGLEVEL_DEBUG,
			 "PID %d (%s) killed after timeout!",
			 backends[i].pid, backends[i].name);
	}
      for (i = 0; i < num_backends; i ++)
	if (backends[i].pid)
	  filterPClose(backends[i].pipe, backends[i].pid, &filter_data);
    }
  }

  // Restore handling of SIGCHLD
  sigaction(SIGCHLD, &old_action, NULL);
  global_data->backend_list = NULL;

  // Clean up
  for (i = 0; i < num_backends; i ++)
    free(backends[i].name);
  if (devices)
    cupsArrayDelete(devices);

  return (ret);
}


//
// 'pr_cups_dev_launch_backend()' - This function starts the CUPS
//                                  backend for a PAPPL device using
//                                  the "cups" scheme. This function
//                                  is separate from the
//                                  pr_cups_deopen() callback function
//                                  to allow a delayed start of the
//                                  CUPS backend, on the first access
//                                  to the device at the lates.  This
//                                  way we can set up a job's filter
//                                  chain after PAPPL has opened the
//                                  device and before the backend gets
//                                  launched on sending the first job
//                                  data, we can supply the filter
//                                  chain's filter_data to the backend
//                                  and the backend gets started based
//                                  on this, making sure that it gets
//                                  all PPD options and has the same
//                                  side and back channel pipes as the
//                                  filters (so that the filters can
//                                  communicate with the backend).
//

bool
pr_cups_dev_launch_backend(pappl_device_t *device)
{
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);
  char buf[2048];


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return (false);
  }

  if (device_data->backend_pid)
  {
    if (device_data->filter_data)
    {
      papplDeviceError(device, "Backend is already running with PID %d!",
		       device_data->backend_pid);
      return (true);
    }
    else
    {
      papplDeviceError(device, "Backend PID is set but backend filter_data is not defined. This should not happen!");
      return (false);
    }
  }

  // Log function
  memset(&device_data->devlog_data, 0, sizeof(device_data->devlog_data));
  device_data->devlog_data.system = device_data->global_data->system;

  // If we do not have external filter data, for example if we open the device
  // only for administrative action without use of filters and not for a job
  // we create our filter data here
  if (device_data->filter_data == NULL)
  {
    if ((device_data->filter_data =
	 (filter_data_t *)calloc(1, sizeof(filter_data_t))) == NULL)
    {
      papplDeviceError(device, "Ran out of memory allocating a device!");
      return (false);
    }
    device_data->filter_data->back_pipe[0] = -1;
    device_data->filter_data->back_pipe[1] = -1;
    device_data->filter_data->side_pipe[0] = -1;
    device_data->filter_data->side_pipe[1] = -1;
    device_data->filter_data->logfunc = pr_cups_devlog;
    device_data->filter_data->logdata = &device_data->devlog_data;
    // Establish back/side channel pipes for CUPS backends
    filterOpenBackAndSidePipes(device_data->filter_data);
    // This is our filter_data we must free it
    device_data->internal_filter_data = true;
  }
  else
    device_data->internal_filter_data = false;

  // Put together full path of the backend file
  snprintf(buf, sizeof(buf), "%s/%s",
	   device_data->global_data->backend_dir, device_data->device_uri + 5);
  *(strchr(buf, ':')) = '\0';

  // Arguments amd parameters for the filterExternalCUPS() filter
  // function to run the CUPS backend in job execution mode. The backend
  // will be waiting for job data but also for commands from the side
  // channel. In addtion it can log status messages (control messages
  // staring with "STATE:")
  memset(&device_data->backend_params, 0, sizeof(device_data->backend_params));
  device_data->backend_params.filter = strdup(buf);
  device_data->backend_params.is_backend = 1; // Run backend in job execution
                                              // mode
  device_data->backend_params.device_uri = device_data->device_uri + 5;

  // Return the filter ends of the pipes
  device_data->backfd = device_data->filter_data->back_pipe[0];
  device_data->sidefd = device_data->filter_data->side_pipe[0];

  // Launch the backend with pipe providing backend's stdin
  if ((device_data->inputfd =
       filterPOpen(filterExternalCUPS,
		   -1, open("/dev/null", O_RDWR),
		   0, device_data->filter_data, &device_data->backend_params,
		   &device_data->backend_pid)) == 0)
  {
    papplDeviceError(device,
		     "Unable to execute '%s' - %s\n",
		     device_data->backend_params.filter, strerror(errno));
    device_data->backend_pid = 0;
    return (false);
  }

  return (true);
}


//
// 'pr_cups_dev_stop_backend()' - This function stops a CUPS backend
//                                started in
//                                pr_cups_dev_launch_backend() and
//                                closes the pipes. Being separate
//                                from pr_cups_devclose() it can get
//                                called manually earlier if needed,
//                                for example if it shares the
//                                filter_data with a filter chain, it
//                                can eb called before freeing the
//                                filter_data.
//

void
pr_cups_dev_stop_backend(pappl_device_t *device)
{
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return;
  }

  // Close the backend sub-process
  if (device_data->backend_pid)
  {
    filterPClose(device_data->inputfd, device_data->backend_pid,
		 device_data->filter_data);
    device_data->backend_pid = 0;
  }

  // Clean up
  if (device_data->internal_filter_data)
  {
    filterCloseBackAndSidePipes(device_data->filter_data);
    free(device_data->filter_data);
    device_data->filter_data = NULL;
  }

  free((char *)(device_data->backend_params.filter));
}


//
// 'pr_cups_devopen()' - Open device connection for devices under the
//                       "cups" scheme (based on CUPS backends). This
//                       function does not yet start the CUPS
//                       backend. It only prepares for it getting
//                       started on the first access. This way we can
//                       still set the filter_data externally,
//                       especially to the filter_data of the filter
//                       chain of a job. If the first access is by the
//                       CUPS library functions for the side channel
//                       (and not by a PAPPL "papplDevice...()" API
//                       function), pr_cups_dev_launch_backend() has
//                       to be called manually.
//                       device_data->filter_data == NULL
//                       means that the backend is not started yet.
//


bool
pr_cups_devopen(pappl_device_t *device,
		const char *device_uri,
		const char *name)
{
  pr_cups_device_data_t *device_data;


  if (papplDeviceGetData(device) != NULL)
  {
    papplDeviceError(device, "Device already opened!");
    return (false);
  }

  // Allocate memory for the device data record...
  if ((device_data = calloc(1, sizeof(pr_cups_device_data_t))) == NULL)
  {
    papplDeviceError(device, "Ran out of memory allocating a device!");
    return (false);
  }

  // Fill in the basic data
  device_data->device_uri = strdup(device_uri);
  device_data->back_timeout = 10.0;
  device_data->side_timeout =  5.0;
  device_data->global_data =
    (pr_printer_app_global_data_t *)pr_cups_device_user_data;
  // We do not yet start the backend
  device_data->filter_data = NULL;
  device_data->backend_pid = 0;

  papplDeviceSetData(device, device_data);
  return (true);
}


//
// 'pr_cups_devclose()' - Close device connection for devices under
//                        the "cups" scheme (based on CUPS
//                        backends). This function stops a CUPS
//                        backend started in pr_cups_devopen() and
//                        closes the pipes, but it supports also to
//                        manually stop the backend earlier, by simply
//                        calling pr_cups_dev_stop_backend() before.
//

void
pr_cups_devclose(pappl_device_t *device)
{
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return;
  }
  
  // Close the backend sub-process
  pr_cups_dev_stop_backend(device);

  // Clean up
  free(device_data->device_uri);
  free(device_data);
  papplDeviceSetData(device, NULL);
}


//
// 'pr_cups_devread()' - Read data from devices under the "cups"
//                       scheme (based on CUPS backends). Dummy
//                       function. We do not read directly from these
//                       devices in these Printer Applications, and
//                       filters use the channel of the CUPS backends.
//

ssize_t
pr_cups_devread(pappl_device_t *device,
		void *buffer,
		size_t bytes)
{
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return (-1);
  }

  // Start backend if not yet done so
  if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
    return (-1);

  // Standard FD for back channel is 3, the CUPS library functions use
  // always FD 3. Therefore we redirect our back channel pipe end to
  // FD 3
  dup2(device_data->backfd, 3);

  // Read from the back channel of the backend
  return (cupsBackChannelRead(buffer, bytes, device_data->back_timeout));
}


//
// 'pr_cups_devwrite()' - Write data (print) on devices under the
//                        "cups" scheme (based on CUPS
//                        backends). Dummy function. We run the CUPS
//                        backend already as part of the filter chain,
//                        so that the filter functions and the CUPS
//                        backend use the same filter_data structure
//                        and so back channel and side channel will
//                        work.
//

ssize_t
pr_cups_devwrite(pappl_device_t *device,
		 const void *buffer,
		 size_t bytes)
{
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return (-1);
  }

  // Start backend if not yet done so
  if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
    return (-1);

  // Write the data to the input of the backend
  return (write(device_data->inputfd, buffer, bytes));
}


//
// 'pr_cups_devstatus()' - Get status information from devices under
//                         the "cups" scheme (based on CUPS
//                         backends). We query the backend for the
//                         printer status using the side channel. Not
//                         all backends support this. If the backend
//                         does not support it, we do not have access
//                         to the status. We will always give an "OK"
//                         result then.
//

pappl_preason_t
pr_cups_devstatus(pappl_device_t *device)
{
  cups_sc_status_t sc_status;
  char pr_status;
  int datalen;
  pappl_preason_t reason = PAPPL_PREASON_NONE;
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return (reason);
  }

  // Start backend if not yet done so
  if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
    return (reason);

  // Query status via side channel

  // Standard FD for side channel is 4, the CUPS library functions
  // always use FD 4. Therefore we redirect our side channel pipe end
  // to FD 4
  dup2(device_data->sidefd, 4);

  datalen = 1;
  if ((sc_status = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_STATE, &pr_status,
					    &datalen,
					    device_data->side_timeout)) !=
      CUPS_SC_STATUS_OK)
  {
    papplDeviceError(device, "Side channel error status: %s",
		     pr_cups_sc_status_str[sc_status]);
  }
  else if (datalen > 0)
  {
    papplLog(device_data->global_data->system, PAPPL_LOGLEVEL_DEBUG,
	     "Printer status: %d", pr_status);
    if (pr_status & CUPS_SC_STATE_ONLINE)
      reason |= PAPPL_PREASON_NONE;
    if (pr_status & CUPS_SC_STATE_BUSY)
      reason |= PAPPL_PREASON_NONE;
    if (pr_status & CUPS_SC_STATE_ERROR)
      reason |= PAPPL_PREASON_OTHER;
    if (pr_status & CUPS_SC_STATE_MEDIA_LOW)
      reason |= PAPPL_PREASON_MEDIA_LOW;
    if (pr_status & CUPS_SC_STATE_MEDIA_EMPTY)
      reason |= PAPPL_PREASON_MEDIA_EMPTY;
    if (pr_status & CUPS_SC_STATE_MARKER_LOW)
      reason |= PAPPL_PREASON_MARKER_SUPPLY_LOW;
    if (pr_status & CUPS_SC_STATE_MARKER_EMPTY)
      reason |= PAPPL_PREASON_MARKER_SUPPLY_EMPTY;
  }

  return (reason);
}


//
// 'pr_cups_devid()' - Get the IEEE-1284 device ID from devices under
//                     the "cups" scheme (based on CUPS backends). We
//                     query the backend for the device ID using the
//                     side channel, which makes the backend polling
//                     the ID from the printer (so no cached ID of the
//                     print queue). Not all backends support this. If
//                     the backend does not support it, we do not have
//                     access to the printer ID (at least not
//                     separately, outside of discovery mode). We will
//                     return NULL (not accessible) then.
//

char *
pr_cups_devid(pappl_device_t *device,
	      char *buffer,
	      size_t bufsize)
{
  cups_sc_status_t sc_status;
  int datalen;
  pr_cups_device_data_t *device_data =
    (pr_cups_device_data_t *)papplDeviceGetData(device);


  if (!device_data)
  {
    papplDeviceError(device, "Device did not get opened!");
    return (NULL);
  }

  // Start backend if not yet done so
  if (!device_data->backend_pid && !pr_cups_dev_launch_backend(device))
    return (NULL);

  // Query device ID via side channel

  // Standard FD for side channel is 4, the CUPS library functions
  // always use FD 4. Therefore we redirect our side channel pipe end
  // to FD 4
  dup2(device_data->sidefd, 4);

  datalen = bufsize;
  if ((sc_status = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_DEVICE_ID,
					    buffer, &datalen,
					    device_data->side_timeout)) !=
      CUPS_SC_STATUS_OK)
  {
    papplDeviceError(device, "Side channel error status: %s",
		     pr_cups_sc_status_str[sc_status]);
  }
  else if (datalen > 0)
  {
    papplLog(device_data->global_data->system, PAPPL_LOGLEVEL_DEBUG,
	     "Device ID: %s", buffer);
  }

  return (datalen > 0 ? buffer : NULL);
}
