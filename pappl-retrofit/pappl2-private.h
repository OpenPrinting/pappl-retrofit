//
// PAPPL 2.x compatibility header for pappl-retrofit.
//
// pappl-retrofit is built against both PAPPL 1.4.x (libcups2) and PAPPL 2.x
// (libcups3).  PAPPL 2.x renamed/restructured a few pieces of the API that
// pappl-retrofit uses; the shims below let the same source compile against
// either generation.  configure defines HAVE_PAPPL2 when the pappl2 pkg-config
// module (PAPPL 2.x) is selected.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL2_PRIVATE_H_
#  define _PAPPL2_PRIVATE_H_

#  include <config.h>
#  include <pappl/pappl.h>

#  ifdef HAVE_PAPPL2

//   PAPPL 2.x dropped the "PWG" infix from the raster-type constants.

#    define PAPPL_PWG_RASTER_TYPE_BLACK_1  PAPPL_RASTER_TYPE_BLACK_1
#    define PAPPL_PWG_RASTER_TYPE_SGRAY_8  PAPPL_RASTER_TYPE_SGRAY_8
#    define PAPPL_PWG_RASTER_TYPE_SRGB_8   PAPPL_RASTER_TYPE_SRGB_8

//   The driver-data "finishings" field (the set of supported finishings) was
//   renamed to "finishings_supported" in PAPPL 2.x.  NB: this only applies to
//   pappl_pr_driver_data_t; pappl_pr_options_t still has a "finishings" field.

#    define pr_driver_finishings           finishings_supported

//   papplJobCreatePrintOptions() gained a leading document-number argument in
//   PAPPL 2.x (multi-document job support); pappl-retrofit always uses the
//   first document.

#    define prJobCreatePrintOptions(job, num_pages, color) \
              papplJobCreatePrintOptions((job), 1, (num_pages), (color))

#  else // PAPPL 1.4.x

#    define pr_driver_finishings           finishings

#    define prJobCreatePrintOptions(job, num_pages, color) \
              papplJobCreatePrintOptions((job), (num_pages), (color))

#  endif // HAVE_PAPPL2

#endif // !_PAPPL2_PRIVATE_H_
