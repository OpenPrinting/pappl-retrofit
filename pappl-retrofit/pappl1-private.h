//
// PAPPL 1.4.x compatibility header for pappl-retrofit.
//
// pappl-retrofit's principal code targets PAPPL 2.x (libcups3).  PAPPL 1.4.x
// (libcups2) renamed/restructured a few of the API pieces pappl-retrofit uses;
// the shims below map the PAPPL 2.x names the code uses onto their PAPPL 1.4.x
// equivalents, so the same source compiles against the older PAPPL too.  This
// mirrors how libcups2-private.h shims the older CUPS for code written against
// libcups3.  configure defines HAVE_PAPPL1 when the pappl pkg-config module
// (PAPPL 1.4.x) is selected instead of pappl2.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL1_PRIVATE_H_
#  define _PAPPL1_PRIVATE_H_

#  include <config.h>

#  ifdef HAVE_PAPPL1

//   This PAPPL header needs to get applied before applying the renaming
//   "#define"s below.  Otherwise the function-like macros would rewrite the
//   declarations in the header itself.

#    include <pappl/pappl.h>

//   PAPPL 2.x dropped the "PWG" infix from the raster-type constants; map the
//   2.x names the code uses back to the PAPPL 1.4.x ones.

#    define PAPPL_RASTER_TYPE_BLACK_1  PAPPL_PWG_RASTER_TYPE_BLACK_1
#    define PAPPL_RASTER_TYPE_SGRAY_8  PAPPL_PWG_RASTER_TYPE_SGRAY_8
#    define PAPPL_RASTER_TYPE_SRGB_8   PAPPL_PWG_RASTER_TYPE_SRGB_8

//   The driver-data "finishings" field (the set of supported finishings) was
//   named "finishings" in PAPPL 1.4.x and renamed to "finishings_supported" in
//   PAPPL 2.x.  NB: pappl_pr_options_t keeps a "finishings" field in both, so
//   only the driver-data field name is remapped here.

#    define finishings_supported       finishings

//   papplJobCreatePrintOptions() gained a leading document-number argument in
//   PAPPL 2.x (multi-document jobs); drop it for PAPPL 1.4.x.

#    define papplJobCreatePrintOptions(job, doc, num_pages, color) \
              papplJobCreatePrintOptions((job), (num_pages), (color))

//   The single-document job accessors became document-indexed in PAPPL 2.x;
//   map them back onto the PAPPL 1.4.x single-document accessors.

#    define papplJobGetDocumentFilename(job, doc)  papplJobGetFilename(job)
#    define papplJobGetDocumentFormat(job, doc)    papplJobGetFormat(job)

#  endif // HAVE_PAPPL1

#endif // !_PAPPL1_PRIVATE_H_
