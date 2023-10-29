//
// Libcups2 header file for libcupsfilters.
//
// Copyright 2020-2022 by Till Kamppeter.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _LIBCUPS2_PRIVATE_H_
#  define _LIBCUPS2_PRIVATE_H_

#  include <config.h>

#  ifdef HAVE_LIBCUPS2 

//   These CUPS headers need to get applied before applying the
//   renaming "#define"s. Otherwise we get conflicting duplicate
//   declarations.

#    include "cups/http.h"
#    include "cups/array.h"
#    include "cups/cups.h"
#    include "cups/ipp.h"
#    include "cups/raster.h"

//   Functions renamed in libcups3

#    define cupsArrayGetCount      cupsArrayCount
#    define cupsArrayGetFirst      cupsArrayFirst
#    define cupsArrayGetNext       cupsArrayNext
#    define cupsArrayGetElement    cupsArrayIndex
#    define cupsArrayNew           cupsArrayNew3
#    define cupsGetDests           cupsGetDests2
#    define cupsGetError           cupsLastError
#    define cupsGetErrorString     cupsLastErrorString
#    define cupsRasterReadHeader   cupsRasterReadHeader2
#    define cupsRasterWriteHeader  cupsRasterWriteHeader2
#    define httpConnect            httpConnect2
#    define httpRead               httpRead2
#    define ippGetFirstAttribute   ippFirstAttribute
#    define ippGetNextAttribute    ippNextAttribute

//   Function replaced by a different function in libcups3

#    define cupsCreateTempFd(prefix,suffix,buffer,bufsize) cupsTempFd(buffer,bufsize)

//   Data types renamed in libcups3

#    define cups_acopy_cb_t       cups_acopy_func_t
#    define cups_afree_cb_t       cups_afree_func_t
#    define cups_array_cb_t       cups_array_func_t
#    define cups_page_header_t    cups_page_header2_t

//   For some functions' parameters in libcups3 size_t is used while
//   int was used in libcups2. We use this type in such a case.

#    define cups_len_t            int

//   Data type newly introduced in libcups3

enum http_resolve_e			// @link httpResolveURI@ options bit values
{
  HTTP_RESOLVE_DEFAULT = 0,		// Resolve with default options
  HTTP_RESOLVE_FQDN = 1,		// Resolve to a FQDN
  HTTP_RESOLVE_FAXOUT = 2		// Resolve FaxOut service instead of Print
};
typedef unsigned http_resolve_t;	// @link httpResolveURI@ options bitfield

#  else

#    define cups_len_t            size_t

#  endif // HAVE_LIBCUPS2

#endif // !_LIBCUPS2_PRIVATE_H_