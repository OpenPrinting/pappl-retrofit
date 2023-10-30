//
// Side-channel API definitions for CUPS.
//
// Copyright © 2023 by Till Kamppeter.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_RETROFIT_CUPS_SIDE_BACK_CHANNEL_H_
#  define _PAPPL_RETROFIT_CUPS_SIDE_BACK_CHANNEL_H_

//
// Include necessary headers...
//

#  include <sys/types.h>
#  if defined(_WIN32) && !defined(__CUPS_SSIZE_T_DEFINED)
#    define __CUPS_SSIZE_T_DEFINED
#    include <stddef.h>
// Windows does not support the ssize_t type, so map it to __int64...
typedef __int64 ssize_t;			// @private@
#  endif // _WIN32 && !__CUPS_SSIZE_T_DEFINED


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

#define _PR_SC_FD	4		// File descriptor for select/poll


//
// Enumerations...
//

enum pr_sc_bidi_e			// **** Bidirectional capability
                                        //      values ****
{
  _PR_SC_BIDI_NOT_SUPPORTED = 0,	// Bidirectional I/O is not supported
  _PR_SC_BIDI_SUPPORTED = 1		// Bidirectional I/O is supported
};
typedef enum pr_sc_bidi_e pr_sc_bidi_t;
					// **** Bidirectional capabilities ****

enum pr_sc_command_e			// **** Request command codes ****
{
  _PR_SC_CMD_NONE = 0,			// No command @private@
  _PR_SC_CMD_SOFT_RESET = 1,		// Do a soft reset
  _PR_SC_CMD_DRAIN_OUTPUT = 2,		// Drain all pending output
  _PR_SC_CMD_GET_BIDI = 3,		// Return bidirectional capabilities
  _PR_SC_CMD_GET_DEVICE_ID = 4,		// Return the IEEE-1284 device ID
  _PR_SC_CMD_GET_STATE = 5,		// Return the device state
  _PR_SC_CMD_SNMP_GET = 6,		// Query an SNMP OID
  _PR_SC_CMD_SNMP_GET_NEXT = 7,		// Query the next SNMP OID
  _PR_SC_CMD_GET_CONNECTED = 8,		// Return whether the backend is
                                        // "connected" to the printer
  _PR_SC_CMD_MAX			// End of valid values @private@
};
typedef enum pr_sc_command_e pr_sc_command_t;
					// **** Request command codes ****

enum pr_sc_connected_e			// **** Connectivity values ****
{
  _PR_SC_NOT_CONNECTED = 0,		// Backend is not "connected" to printer
  _PR_SC_CONNECTED = 1			// Backend is "connected" to printer
};
typedef enum pr_sc_connected_e pr_sc_connected_t;
					// **** Connectivity values ****


enum pr_sc_state_e			// **** Printer state bits ****
{
  _PR_SC_STATE_OFFLINE = 0,		// Device is offline
  _PR_SC_STATE_ONLINE = 1,		// Device is online
  _PR_SC_STATE_BUSY = 2,		// Device is busy
  _PR_SC_STATE_ERROR = 4,		// Other error condition
  _PR_SC_STATE_MEDIA_LOW = 16,		// Paper low condition
  _PR_SC_STATE_MEDIA_EMPTY = 32,	// Paper out condition
  _PR_SC_STATE_MARKER_LOW = 64,		// Toner/ink low condition
  _PR_SC_STATE_MARKER_EMPTY = 128	// Toner/ink out condition
};
typedef enum pr_sc_state_e pr_sc_state_t;
					// **** Printer state bits ****

enum pr_sc_status_e			// **** Response status codes ****
{
  _PR_SC_STATUS_NONE,			// No status
  _PR_SC_STATUS_OK,			// Operation succeeded
  _PR_SC_STATUS_IO_ERROR,		// An I/O error occurred
  _PR_SC_STATUS_TIMEOUT,		// The backend did not respond
  _PR_SC_STATUS_NO_RESPONSE,		// The device did not respond
  _PR_SC_STATUS_BAD_MESSAGE,		// The command/response message was
                                        // invalid
  _PR_SC_STATUS_TOO_BIG,		// Response too big
  _PR_SC_STATUS_NOT_IMPLEMENTED		// Command not implemented
};
typedef enum pr_sc_status_e pr_sc_status_t;
					// **** Response status codes ****

typedef void (*pr_sc_walk_func_t)(const char *oid, const char *data,
				  int datalen, void *context);
					// **** SNMP walk callback ****


//
// Prototypes...
//

extern ssize_t		_prBackChannelRead(char *buffer, size_t bytes,
					   double timeout);
extern ssize_t		_prBackChannelWrite(const char *buffer, size_t bytes,
					    double timeout);
extern pr_sc_status_t	_prSideChannelDoRequest(pr_sc_command_t command,
						char *data, int *datalen,
						double timeout);
extern int		_prSideChannelRead(pr_sc_command_t *command,
					   pr_sc_status_t *status,
					   char *data, int *datalen,
					   double timeout);
extern int		_prSideChannelWrite(pr_sc_command_t command,
					    pr_sc_status_t status,
					    const char *data, int datalen,
					    double timeout);
extern pr_sc_status_t	_prSideChannelSNMPGet(const char *oid, char *data,
					      int *datalen, double timeout);
extern pr_sc_status_t	_prSideChannelSNMPWalk(const char *oid, double timeout,
					       pr_sc_walk_func_t cb,
					       void *context);


#  ifdef __cplusplus
}
#  endif // __cplusplus

#endif // !_PAPPL_RETROFIT_CUPS_SIDE_BACK_CHANNEL_H_
