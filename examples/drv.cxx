/*
 * *.drv file support for pappl-retrofit.
 *
 * This program handles listing and installing PPD files
 * created from driver information files (*.drv).
 *
 * Copyright © 2021 by Till Kamppeter
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */


/*
 * Derived from cups-driverd.
 *
 * To be compiled with
 * 
 *    g++ -o drv drv.cxx $CUPS_SOURCE/ppdc/libcupsppdc.a \
 *        -DCUPS_DATADIR='"/usr/share/cups"' -I $CUPS_SOURCE -lcups -lppd
 *
 * Needs the source code of CUPS 2.4.x or older. Needs cups-filters 2.x
 * or newer.
 *
 * To be installed in /usr/share/ppd, NOT in
 * /usr/lib/cups/driver, so that pappl-retrofit-based Printer Applications
 * find and execute it but not CUPS (to avoid duplicate PPD listings).
 */


/*
 * Include necessary headers...
 */

#include <cups/dir.h>
#include <ppd/ppd.h>
#include <ppdc/ppdc.h>
#include <ctype.h>
#include <errno.h>


/*
 * Globals...
 */

static cups_array_t	*Inodes = NULL,	/* Inodes of directories we've visited */
			*PPDsByMakeModel = NULL;
					/* PPD files sorted by make and model */


/*
 * Local functions...
 */

static ppd_info_t	*add_ppd(const char *filename, const char *name,
			         const char *language, const char *make,
				 const char *make_and_model,
				 const char *device_id, const char *product,
				 const char *psversion, time_t mtime,
				 size_t size, int model_number, int type,
				 const char *scheme);
static int		cat_drv(const char *name);
static void		cat_ppd(const char *name);
static int		compare_inodes(struct stat *a, struct stat *b);
static int		compare_ppds(const ppd_info_t *p0,
			             const ppd_info_t *p1);
static void		free_array(cups_array_t *a);
static cups_file_t	*get_file(const char *name,
			          const char *subdir, char *buffer,
			          size_t bufsize, char **subfile);
static void		list_ppds();
static int		load_drv(const char *filename, const char *name,
			         cups_file_t *fp, time_t mtime, off_t size);
static int		load_ppds(const char *d, const char *p, int descend);


/*
 * 'main()' - Scan for PPDs available via *.drv files and build the PPDs
 *            on-demand.
 *
 * Usage:
 *
 *    drv list
 *    drv cat PPD_URI
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
 /*
  * Install or list PPDs...
  */

  if (argc == 3 && !strcmp(argv[1], "cat"))
    cat_ppd(argv[2]);
  else if (argc == 2 && !strcmp(argv[1], "list"))
    list_ppds();
  else
  {
    fputs("Usage: drv cat ppd-name\n", stderr);
    fputs("Usage: drv list\n", stderr);
    return (1);
  }
}


/*
 * 'add_ppd()' - Add a PPD file.
 */

static ppd_info_t *			/* O - PPD */
add_ppd(const char *filename,		/* I - PPD filename */
        const char *name,		/* I - PPD name */
        const char *language,		/* I - LanguageVersion */
        const char *make,		/* I - Manufacturer */
	const char *make_and_model,	/* I - NickName/ModelName */
	const char *device_id,		/* I - 1284DeviceID */
	const char *product,		/* I - Product */
	const char *psversion,		/* I - PSVersion */
        time_t     mtime,		/* I - Modification time */
	size_t     size,		/* I - File size */
	int        model_number,	/* I - Model number */
	int        type,		/* I - Driver type */
	const char *scheme)		/* I - PPD scheme */
{
  ppd_info_t	*ppd;			/* PPD */
  char		*recommended;		/* Foomatic driver string */


 /*
  * Add a new PPD file...
  */

  if ((ppd = (ppd_info_t *)calloc(1, sizeof(ppd_info_t))) == NULL)
  {
    fprintf(stderr,
	    "ERROR: [drv] Ran out of memory for %d PPD files!\n",
	    cupsArrayCount(PPDsByMakeModel));
    return (NULL);
  }

 /*
  * Zero-out the PPD data and copy the values over...
  */

  ppd->found               = 1;
  ppd->record.mtime        = mtime;
  ppd->record.size         = (off_t)size;
  ppd->record.model_number = model_number;
  ppd->record.type         = type;

  strncpy(ppd->record.filename, filename, sizeof(ppd->record.filename));
  strncpy(ppd->record.name, name, sizeof(ppd->record.name));
  strncpy(ppd->record.languages[0], language,
          sizeof(ppd->record.languages[0]));
  strncpy(ppd->record.products[0], product, sizeof(ppd->record.products[0]));
  strncpy(ppd->record.psversions[0], psversion,
          sizeof(ppd->record.psversions[0]));
  strncpy(ppd->record.make, make, sizeof(ppd->record.make));
  strncpy(ppd->record.make_and_model, make_and_model,
          sizeof(ppd->record.make_and_model));
  strncpy(ppd->record.device_id, device_id, sizeof(ppd->record.device_id));
  strncpy(ppd->record.scheme, scheme, sizeof(ppd->record.scheme));

 /*
  * Strip confusing (and often wrong) "recommended" suffix added by
  * Foomatic drivers...
  */

  if ((recommended = strstr(ppd->record.make_and_model,
                            " (recommended)")) != NULL)
    *recommended = '\0';

 /*
  * Add the PPD to the PPD array...
  */

  cupsArrayAdd(PPDsByMakeModel, ppd);

 /*
  * Return the new PPD pointer...
  */

  return (ppd);
}


/*
 * 'cat_drv()' - Generate a PPD from a driver info file.
 */

static int				/* O - Exit code */
cat_drv(const char *name)		/* I - PPD name */
{
  cups_file_t	*fp;			// File pointer
  ppdcSource	*src;			// PPD source file data
  ppdcDriver	*d;			// Current driver
  cups_file_t	*out;			// Stdout via CUPS file API
  char		message[2048],		// status-message
		filename[1024],		// Full path to .drv file(s)
		scheme[32],		// URI scheme ("drv")
		userpass[256],		// User/password info (unused)
		host[2],		// Hostname (unused)
		resource[1024],		// Resource path (/dir/to/filename.drv)
		*pc_file_name;		// Filename portion of URI
  int		port;			// Port number (unused)


  // Pull out the path to the .drv file...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, name, scheme, sizeof(scheme),
                      userpass, sizeof(userpass), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_OK)
  {
    fprintf(stderr, "ERROR: Bad PPD name \"%s\".\n", name);
    return (1);
  }

  if ((fp = get_file(resource, "drv", filename, sizeof(filename), &pc_file_name)) == NULL || !pc_file_name)
    return (1);

  src = new ppdcSource(filename, fp);

  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
    if (!strcmp(pc_file_name, d->pc_file_name->value) ||
        (d->file_name && !strcmp(pc_file_name, d->file_name->value)))
      break;

  if (d)
  {
    ppdcArray	*locales;		// Locale names
    ppdcCatalog	*catalog;		// Message catalog in .drv file


    fprintf(stderr, "DEBUG2: [drv] %u locales defined in \"%s\"...\n", (unsigned)src->po_files->count, filename);

    locales = new ppdcArray();
    for (catalog = (ppdcCatalog *)src->po_files->first();
         catalog;
	 catalog = (ppdcCatalog *)src->po_files->next())
    {
      fprintf(stderr, "DEBUG2: [drv] Adding locale \"%s\"...\n",
              catalog->locale->value);
      catalog->locale->retain();
      locales->add(catalog->locale);
    }

    out = cupsFileStdout();
    d->write_ppd_file(out, NULL, locales, src, PPDC_LFONLY);
    cupsFileClose(out);

    locales->release();
  }
  else
    fprintf(stderr, "ERROR: PPD \"%s\" not found.\n", name);

  src->release();
  cupsFileClose(fp);

  return (!d);
}


/*
 * 'cat_ppd()' - Copy a PPD file to stdout.
 */

static void
cat_ppd(const char *name)		/* I - PPD name */
{
  char		scheme[256],		/* Scheme from PPD name */
		*sptr,			/* Pointer into scheme */
		line[1024],		/* Line/filename */
		message[2048];		/* status-message */


 /*
  * Figure out if this is a static or dynamic PPD file...
  */

  if (strstr(name, "../"))
  {
    fputs("ERROR: Invalid PPD name.\n", stderr);
    exit(1);
  }

  strncpy(scheme, name, sizeof(scheme));
  if ((sptr = strchr(scheme, ':')) != NULL)
    *sptr = '\0';
  else
    scheme[0] = '\0';

  if (!strcmp(scheme, "drv"))
    exit(cat_drv(name));

 /*
  * Exit
  */

  exit(1);
}


/*
 * 'copy_static()' - Copy a static PPD file to stdout.
 */

static int				/* O - Exit code */
cat_static(const char *name)		/* I - PPD name */
{
  cups_file_t	*fp;			/* PPD file */
  char		filename[1024],		/* PPD filename */
		line[1024];		/* Line buffer */


  if ((fp = get_file(name, "model", filename, sizeof(filename),
                     NULL)) == NULL)
    return (1);

 /*
  * Now copy the file to stdout...
  */

  while (cupsFileGets(fp, line, sizeof(line)))
    puts(line);

  cupsFileClose(fp);

  return (0);
}


/*
 * 'compare_inodes()' - Compare two inodes.
 */

static int				/* O - Result of comparison */
compare_inodes(struct stat *a,		/* I - First inode */
               struct stat *b)		/* I - Second inode */
{
  if (a->st_dev != b->st_dev)
    return (a->st_dev - b->st_dev);
  else
    return (a->st_ino - b->st_ino);
}


/*
 * 'compare_ppds()' - Compare PPD file make and model names for sorting.
 */

static int				/* O - Result of comparison */
compare_ppds(const ppd_info_t *p0,	/* I - First PPD file */
             const ppd_info_t *p1)	/* I - Second PPD file */
{
  int	diff;				/* Difference between strings */


 /*
  * First compare manufacturers...
  */

  if ((diff = strcasecmp(p0->record.make, p1->record.make)) != 0)
    return (diff);
  else if ((diff = strcasecmp(p0->record.make_and_model,
			      p1->record.make_and_model)) != 0)
    return (diff);
  else if ((diff = strcmp(p0->record.languages[0],
                          p1->record.languages[0])) != 0)
    return (diff);
  else if ((diff = strcmp(p0->record.filename, p1->record.filename)) != 0)
    return (diff);
  else
    return (strcmp(p0->record.name, p1->record.name));
}


/*
 * 'free_array()' - Free an array of strings.
 */

static void
free_array(cups_array_t *a)		/* I - Array to free */
{
  char	*ptr;				/* Pointer to string */


  for (ptr = (char *)cupsArrayFirst(a);
       ptr;
       ptr = (char *)cupsArrayNext(a))
    free(ptr);

  cupsArrayDelete(a);
}


/*
 * 'get_file()' - Get the filename associated with a request.
 */

static cups_file_t *			/* O - File pointer or NULL */
get_file(const char *name,		/* I - Name */
	 const char *subdir,		/* I - Subdirectory for file */
	 char       *buffer,		/* I - Filename buffer */
	 size_t     bufsize,		/* I - Size of filename buffer */
	 char       **subfile)		/* O - Sub-filename */
{
  cups_file_t	*fp;			/* File pointer */
  const char	*datadir;		/* CUPS_DATADIR env var */
  char		*bufptr,		/* Pointer into filename buffer */
		message[2048];		/* status-message */


  if (subfile)
    *subfile = NULL;

  while (*name == '/')
    name ++;

  if (strstr(name, "../") || strstr(name, "/.."))
  {
   /*
    * Bad name...
    */

    fprintf(stderr, "ERROR: [drv] Bad PPD name \"%s\".\n", name);

    return (NULL);
  }

 /*
  * Try opening the file...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
      datadir = CUPS_DATADIR;

  snprintf(buffer, bufsize, "%s/%s/%s", datadir, subdir, name);

 /*
  * Strip anything after ".drv/", ".drv.gz/"...
  */

  if (subfile)
  {
    if ((bufptr = strstr(buffer, ".drv/")) != NULL)
      bufptr += 4;
    else if ((bufptr = strstr(buffer, ".drv.gz/")) != NULL)
      bufptr += 7;

    if (bufptr)
    {
      *bufptr++ = '\0';
      *subfile  = bufptr;
    }
  }

 /*
  * Try opening the file...
  */

  if ((fp = cupsFileOpen(buffer, "r")) == NULL)
  {
    fprintf(stderr, "ERROR: [drv] Unable to open \"%s\" - %s\n",
	    buffer, strerror(errno));

    return (NULL);
  }

  return (fp);
}


/*
 * 'list_ppds()' - List PPD files.
 */

static void
list_ppds()
{
  int		i;			/* Looping vars */
  int		count;			/* Number of PPDs to send */
  ppd_info_t	*ppd;			/* Current PPD file */
  cups_file_t	*fp;			/* ppds.dat file */
  char		filename[1024],		/* ppds.dat filename */
		model[1024];		/* Model directory */
  const char	*cups_datadir;		/* CUPS_DATADIR environment variable */
  int		model_number,		/* ppd-model-number value */
		type;			/* ppd-type value */
  size_t	make_and_model_len,	/* Length of ppd-make-and-model */
		product_len;		/* Length of ppd-product */


 /*
  * Load all PPDs in the specified directory and below...
  */

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

  Inodes          = cupsArrayNew((cups_array_func_t)compare_inodes, NULL);
  PPDsByMakeModel = cupsArrayNew((cups_array_func_t)compare_ppds, NULL);

  snprintf(model, sizeof(model), "%s/drv", cups_datadir);
  load_ppds(model, "", 1);

  count = cupsArrayCount(PPDsByMakeModel);

  for (ppd = (ppd_info_t *)cupsArrayFirst(PPDsByMakeModel);
       count > 0 && ppd;
       ppd = (ppd_info_t *)cupsArrayNext(PPDsByMakeModel))
  {
   /*
    * Skip invalid PPDs...
    */

    if (ppd->record.type < PPD_TYPE_POSTSCRIPT ||
        ppd->record.type >= PPD_TYPE_DRV)
      continue;

   /*
    * Send this PPD...
    */

    count --;

    printf("\"%s\" %s \"%s\" \"%s\" \"%s\"\n", ppd->record.name, ppd->record.languages[0], ppd->record.make, ppd->record.make_and_model, ppd->record.device_id);
  }

  exit(0);
}


/*
 * 'load_drv()' - Load the PPDs from a driver information file.
 */

static int				/* O - 1 on success, 0 on failure */
load_drv(const char  *filename,		/* I - Actual filename */
         const char  *name,		/* I - Name to the rest of the world */
         cups_file_t *fp,		/* I - File to read from */
	 time_t      mtime,		/* I - Mod time of driver info file */
	 off_t       size)		/* I - Size of driver info file */
{
  ppdcSource	*src;			// Driver information file
  ppdcDriver	*d;			// Current driver
  ppdcAttr	*device_id,		// 1284DeviceID attribute
		*product,		// Current product value
		*ps_version,		// PSVersion attribute
		*cups_fax,		// cupsFax attribute
		*nick_name;		// NickName attribute
  ppdcFilter	*filter;		// Current filter
  ppd_info_t	*ppd;			// Current PPD
  int		products_found;		// Number of products found
  char		uri[1024],		// Driver URI
		make_model[1024];	// Make and model
  int		type;			// Driver type


 /*
  * Load the driver info file...
  */

  src = new ppdcSource(filename, fp);

  if (src->drivers->count == 0)
  {
    fprintf(stderr,
            "ERROR: [drv] Bad driver information file \"%s\"!\n",
	    filename);
    src->release();
    return (0);
  }

 /*
  * Add a dummy entry for the file...
  */

  add_ppd(name, name, "", "", "", "", "", "", mtime, (size_t)size, 0, PPD_TYPE_DRV, "drv");

 /*
  * Then the drivers in the file...
  */

  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "drv", "", "", 0,
                     "/%s/%s", name,
		     d->file_name ? d->file_name->value :
		                    d->pc_file_name->value);

    device_id  = d->find_attr("1284DeviceID", NULL);
    ps_version = d->find_attr("PSVersion", NULL);
    nick_name  = d->find_attr("NickName", NULL);

    if (nick_name)
      strncpy(make_model, nick_name->value->value, sizeof(make_model));
    else if (strncasecmp(d->model_name->value, d->manufacturer->value,
                         strlen(d->manufacturer->value)))
      snprintf(make_model, sizeof(make_model), "%s %s, %s",
               d->manufacturer->value, d->model_name->value,
	       d->version->value);
    else
      snprintf(make_model, sizeof(make_model), "%s, %s", d->model_name->value,
               d->version->value);

    if ((cups_fax = d->find_attr("cupsFax", NULL)) != NULL &&
        !strcasecmp(cups_fax->value->value, "true"))
      type = PPD_TYPE_FAX;
    else if (d->type == PPDC_DRIVER_PS)
      type = PPD_TYPE_POSTSCRIPT;
    else if (d->type != PPDC_DRIVER_CUSTOM)
      type = PPD_TYPE_RASTER;
    else
    {
      for (filter = (ppdcFilter *)d->filters->first(),
               type = PPD_TYPE_POSTSCRIPT;
	   filter;
	   filter = (ppdcFilter *)d->filters->next())
        if (strcasecmp(filter->mime_type->value, "application/vnd.cups-raster"))
	  type = PPD_TYPE_RASTER;
        else if (strcasecmp(filter->mime_type->value,
	                    "application/vnd.cups-pdf"))
	  type = PPD_TYPE_PDF;
    }

    for (product = (ppdcAttr *)d->attrs->first(), products_found = 0,
             ppd = NULL;
         product;
	 product = (ppdcAttr *)d->attrs->next())
      if (!strcmp(product->name->value, "Product"))
      {
        if (!products_found)
	  ppd = add_ppd(name, uri, "en", d->manufacturer->value, make_model, device_id ? device_id->value->value : "", product->value->value,
		        ps_version ? ps_version->value->value : "(3010) 0", mtime, (size_t)size, d->model_number, type, "drv");
	else if (products_found < PPD_MAX_PROD)
	  strncpy(ppd->record.products[products_found], product->value->value, sizeof(ppd->record.products[0]));
	else
	  break;

	products_found ++;
      }

    if (!products_found)
      add_ppd(name, uri, "en", d->manufacturer->value, make_model, device_id ? device_id->value->value : "", d->model_name->value, ps_version ? ps_version->value->value : "(3010) 0", mtime, (size_t)size, d->model_number, type, "drv");
  }

  src->release();

  return (1);
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static int				/* O - 1 on success, 0 on failure */
load_ppds(const char *d,		/* I - Actual directory */
          const char *p,		/* I - Virtual path in name */
	  int        descend)		/* I - Descend into directories? */
{
  struct stat	dinfo,			/* Directory information */
		*dinfoptr;		/* Pointer to match */
  cups_file_t	*fp;			/* Pointer to file */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024],		/* Name of PPD or directory */
		line[256],		/* Line from file */
		*ptr,			/* Pointer into name */
		name[512];		/* Name of PPD file */
  ppd_info_t	*ppd,			/* New PPD file */
		key;			/* Search key */


 /*
  * See if we've loaded this directory before...
  */

  if (stat(d, &dinfo))
  {
    if (errno != ENOENT)
      fprintf(stderr, "ERROR: [drv] Unable to stat \"%s\": %s\n", d,
	      strerror(errno));

    return (0);
  }
  else if (cupsArrayFind(Inodes, &dinfo))
  {
    fprintf(stderr, "ERROR: [drv] Skipping \"%s\": loop detected!\n",
            d);
    return (1);
  }

 /*
  * Nope, add it to the Inodes array and continue...
  */

  dinfoptr = (struct stat *)malloc(sizeof(struct stat));
  memcpy(dinfoptr, &dinfo, sizeof(struct stat));
  cupsArrayAdd(Inodes, dinfoptr);

 /*
  * Check permissions...
  */

  if ((dir = cupsDirOpen(d)) == NULL)
  {
    if (errno != ENOENT)
      fprintf(stderr,
	      "ERROR: [drv] Unable to open PPD directory \"%s\": %s\n",
	      d, strerror(errno));

    return (0);
  }

  fprintf(stderr, "DEBUG: [drv] Loading \"%s\"...\n", d);

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Skip files/directories starting with "."...
    */

    if (dent->filename[0] == '.')
      continue;

   /*
    * See if this is a file...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->filename);

    if (p[0])
      snprintf(name, sizeof(name), "%s/%s", p, dent->filename);
    else
      strncpy(name, dent->filename, sizeof(name));

    if (S_ISDIR(dent->fileinfo.st_mode))
    {
     /*
      * Do subdirectory...
      */

      if (descend)
      {
	if (!load_ppds(filename, name, 1))
	{
	  cupsDirClose(dir);
	  return (1);
	}
	continue;
      }
    }

    fprintf(stderr, "DEBUG: [drv] File \"%s\"...\n", filename);
    
    if ((ptr = strstr(filename, ".drv")) != NULL && !strcmp(ptr, ".drv"))
      load_drv(filename, name, fp, dent->fileinfo.st_mtime,
	       dent->fileinfo.st_size);
  }

  cupsDirClose(dir);

  return (1);
}
