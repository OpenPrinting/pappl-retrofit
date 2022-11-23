//
// PPD/Classic CUPS driver retro-fit Printer Application Library
// (libpappl-retrofit) for the Printer Application Framework (PAPPL)
//
// web-interface-private.h
//
// Copyright © 2020 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_RETROFIT_WEB_INTERFACE_H_
#  define _PAPPL_RETROFIT_WEB_INTERFACE_H_

//
// Include necessary headers...
//

#include <pappl/pappl.h>


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Functions...
//

extern void   _prPrinterWebDeviceConfig(pappl_client_t *client,
					pappl_printer_t *printer);
extern void   _prSystemWebAddPPD(pappl_client_t *client, void *data);


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus


#endif // !_PAPPL_RETROFIT_WEB_INTERFACE_H_
