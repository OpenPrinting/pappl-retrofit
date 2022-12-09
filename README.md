
# PPD/Classic CUPS driver retro-fit Printer Application Library


## INTRODUCTION

This library together with [PAPPL](https://www.msweet.org/pappl),
[libcupsfilters 2.x](https://github.com/OpenPrinting/libcupsfilters),
and [libppd](https://github.com/OpenPrinting/libppd) allows to convert
classic [CUPS](https://github.com/OpenPrinting/cups) printer drivers
into Printer Applications. This way the printer appears as an emulated
IPP printer and one can print on it from practically any operating
system, especially also mobile operating systems and IoT platforms,
without need any client-side driver.

It also makes printers needing a classic CUPS driver available to the
[CUPS Snap](https://github.com/OpenPrinting/cups-snap) and to CUPS 3.x
([library](https://github.com/OpenPrinting/libcups), [local
server](https://github.com/OpenPrinting/cups-local), [sharing
server](https://github.com/OpenPrinting/cups-sharing)) which do not
support installing and using classic CUPS drivers.

The basic framework for making up a Printer Application comes from
PAPPL, many resources to cope with PPD files, CUPS filters, job data
format conversions, and even CUPS backends are provided by
libcupsfilters, libppd, and cups-filters.

With this library a Printer Application is simply made up by defining
a configuration of basic properties and data conversion rules, and
also add some own functions for auto-assignment of drivers, test page,
... in a small C program stub, and packaging this together with the
PPD files, CUPS filters, and CUPS backend out of which the classic
CUPS filter is composed. The resulting Printer Application can also be
easily packaged in a Snap to be available for a wide range of Linux
distributions.

Your contributions are welcome. Please post [issues and pull
requests](https://github.com/OpenPrinting/pappl-retrofit).


### Properties

- A classic CUPS printer driver consists of PPD files, CUPS filters,
  and sometimes also CUPS backends. All this is supported by this
  library.

- The PPD files can be provided in all forms which CUPS supports:
  Individual files, compressed files, tar archives, driver information
  files (`.drv`), executables which generate PPDs on-the-fly.

- Even more complex filters and backends which use the side and back
  channel communication are supported.

- All user-settable options of the PPD file are made available in the
  web interface of the Printer Application, under "Printing
  Defaults". All paper input trays are listed under "Media" and one
  can assign the loaded paper size and type, and also whether
  borderless printing should be done (if the printer supports it).

- To be able to get best printing results from any client platform the
  PPD options are mapped to standard job IPP attributes, not only
  paper sizes, types, trays, duplex, but especially "print-color-mode"
  (color/monochrome), "print-quality" (draft/normal/high), and
  "print-content-optimize" (auto/photo/graphics/text/text&graphics)
  are auto-selecting the best possible PPD option settings, and that
  for ~10000 PPD files without any manually created database of option
  presets (no one will take the time for that).

- If you want to get the last bits out of your printer and therefore
  want to override the automatic settings done by the standard job IPP
  attributes you can always move the option settings on the "Printing
  Defaults" web interface page away from "Automatic Selection" and get
  all the choices from the original PPD. Remember to go back to
  "Automatic Selection" when done with the special job.

- If "print-content-optimize" is set to "auto" the job data is
  pre-checked for the content. Images (JPEG, PNG) are considered as
  photos, on PDF files it is determined by which application they were
  created and the content type selected by that.

- Some printers have hardware accessories which can be installed, like
  extra trays, a duplex unit, finishers, or more RAM. If the PPD file
  describes these accessories and what extra settings get available by
  them, a "Device Settings" web interface page will appear in the
  Printer Application. There you can configure what is installed amd
  the options under "Media" and "Printing Defaults" get updated
  appropriately.

- If the Printer is a PostScript printer and appropriate commands are
  defined in the PPD file, both option default settings and the
  configuration of installable accessories can be polled from the
  printer and the settings in the Printer Application get
  automatically adjusted. In this case you have a "Device Settings"
  web interface page for your printer and there are "Poll" buttons to
  do the polls.

- With this library any arbitrary amount of PPD files included in a
  single Printer Application is supported, it even does not slow down
  with ~10000 PPDs. Naturally only PPDs work for which the appropriate
  CUPS filters are available. Only PostScript printer PPDs can
  alternatively be used without the CUPS filter specified in them (but
  most PostScript PPDs do not specify a CUPS filter anyway).

- One can create Printer Applications which allow the user to add
  their own PPD files via "Add PPD file" web interface page.

- The full functionality of PAPPL is supported, including the
  emulation of a highly standards-conforming driverless IPP printer
  (IPP Everywhere, AirPrint) and streaming of Raster jobs (if the
  printer and driver support it), which saves resources on the server
  and allows even infinite jobs.

- Drivers can be assigned to the discovered printers automatically or
  manually by selecting the driver (a PPD file) from the menu on the
  "Add Printer" web interface page. The implementor of the Printer
  Application can use their own auto-assignment function. The library
  contains some functions for common operations, like finding the best
  PPD for a printer or checking which PDLs (Page Description
  Languages) the printer supports.

- The included Legacy Printer Application allows to easily check
  whether a CUPS driver works inside a Printer Application. Simply
  install the driver normally (into the system's conventionally
  installed CUPS) and the Legacy Printer Application looks for the
  driver's files in the usual CUPS directories. It can also be used to
  make classically installed printer drivers available to the CUPS
  Snap or to CUPS 3.x, especially proprietary legacy drivers which do
  not exist in a Snap in the Snap Store. In addition, it serves as
  example to create your own driver-retro-fitting Printer Application.


### Remark

- This library is derived from the [PostScript Printer
  Application](https://github.com/OpenPrinting/ps-printer-app), but
  note that when you are looking into its GIT repository now, you will
  only see a little code stub as it is now using this library. Go back
  in the GIT history to see the last pre-libpappl-retrofit state of
  it. The current state is also a nice example for using this library.


### To Do

- On the "Add PPD files" page in the list of already added user PPD
  files mark which ones are actually used by a printer which got set
  up in the Printer Application, to avoid that the user removes these
  files.

- Internationalization/Localization (Needs support by PAPPL: [Issue
  #58: Localization
  support](https://github.com/michaelrsweet/pappl/issues/58))

- SNMP Ink level check via ps_status() function (Needs support by
  PAPPL: [Issue #83: CUPS does IPP and SNMP ink level polls via
  backends, PAPPL should have functions for
  this](https://github.com/michaelrsweet/pappl/issues/83))

- In the C files some places can be marked with `TODO`. These are
  points to be improved or where functionality in PAPPL is still
  needed.

- Build options for cups-filters, to build without libqpdf and/or
  without libppd, the former will allow to create the Snap of some
  Printer Applications without downloading and building QPDF


## SNAP

This is a library and not a Printer Application, so there will be no
Snap for this, but Printer Applications created with the help of this
library are available in the [Snap
Store](https://snapcraft.io/search?q=OpenPrinting).

The included Legacy Printer Application is also not suitable to get
snapped, as it simply points its search directories to the PPD,
filter, and backend locations of a conventionally (not the Snap)
installed CUPS, to make all installed drivers available in a Printer
Application. Out of a fully constrained Snap you cannot simply access
the system's files and so this does not work (otherwise the CUPS Snap
would support claasic printer drivers).

Please have a look at the [PostScript Printer
Application](https://github.com/OpenPrinting/ps-printer-app), the
[Ghostscript Printer
Application](https://github.com/OpenPrinting/ghostscript-printer-app),
the [HPLIP Printer
Application](https://github.com/OpenPrinting/hplip-printer-app), and
the [Gutenprint Printer
Application](https://github.com/OpenPrinting/gutenprint-printer-app)
for examples on how Snaps of Printer Applications are created. All
these Printer Applications use this library, retro-fitting PostScript
printer PPD files, [Ghostscript](http://www.ghostscript.com/) drivers
with [Foomatic](https://github.com/OpenPrinting/foomatic-db) PPD files
(and many other printer drivers), HPLIP, and Gutenprint. Practically
every free software printer driver which is available as Debian
package is now also available as a Printer Application Snap. They can
be all installed from the [Snap
Store](https://snapcraft.io/search?q=OpenPrinting).

The HPLIP Printer Application is especially an example of how to add
driver-specific functionality which is beyond the pappl-retrofit
library and control this functionality through extra pages in the web
interface. Here a feature for downloading HP's proprietary plugin is
added.


## Installation

To install this library, you need libcups (of
[CUPS](https://github.com/OpenPrinting/cups) 2.2.x or newer),
[PAPPL](https://www.msweet.org/pappl) 1.3.x or newer,
[libcupsfilters](https://github.com/OpenPrinting/libcupsfilters) 2.0b1
or newer, and [libppd](https://github.com/OpenPrinting/libood) 2.0b1
or newer.

With this installed, you do the usual
```
./configure
make
sudo make install
```
Note that if you are using a GIT snapshot you have to run
```
./autogen.sh
```
before the above-mentioned commands.

Then have a look at
```
legacy/legacy-printer-app.c
```
and
```
pappl-retrofit/pappl-retrofit.h
```
to get a feeling how to create a Printer Application in your desired
configuration.


## Setting up

If you have your Printer Application, start it as a server, for
example (using the name of our Legacy Printer Application in the
examples):
```
sudo legacy-printer-app server &
```

Enter the web interface
```
http://localhost:8000/
```
Use the web interface to add a printer. Supply a name, select the
discovered printer, then select make and model. Also set the loaded
media and the option defaults.

Then print PDF, PostScript, JPEG, Apple Raster, or PWG Raster files
with
```
legacy-printer-app FILE
```
or print with CUPS, CUPS (and also cups-browsed) discover and treat
the printers set up with this Printer Application as driverless IPP
printers (IPP Everywhere and AirPrint).

You can also add PPD files, either by using the "Add PPD files" button
in the web interface or by manually copying PPD files:
```
sudo cp PPDFILE /var/lib/legacy-printer-app/ppd/
```

After manually copying (or removing) PPD files you need to restart the
server or in the web interface, on the "Add PPD files" page click the
"Refresh" button at the bottom. This adds the changes to the internal
driver list.

On the "Add Printer" page in the drop-down to select the driver,
user-added PPD files are marked "USER-ADDED". When setting up a
printer with automatic driver selection, user-added PPD files are
preferred.

`PPDFILE` in the command line above cannot only be a single PPD file
but any number of single PPD files, `.tar.gz` files containing PPDs
(in arbitrary directory structure), driver information files (`.drv`),
and PPD-generating executables which are usually put into
`/usr/lib/cups/driver`. You can also create arbitrary sub-directory
structures in `/var/lib/legacy-printer-app/ppd/` containing the
mentioned types of files. Only make sure to not put any executables
there which do anything else than listing and generating PPD files.

Note that with the web interface you can only manage individual PPDs
(uncompressed or compressed with `gzip`) in the
`/var/lib/legacy-printer-app/ppd/` directory itself. Archives, driver
information files, executables, or sub-directories are not shown and
appropriate uploads not accepted. This especially prevents adding
executables without root rights.

Any added PPD file must be for printers supported by the Printer
Application (its included filters and backends). The "Add PPD files"
page shows warnings if unsuitable files get uploaded.

See
```
legacy-printer-app --help
```
for more options.

Use the `--debug` argument for verbose logging in your terminal
window.


## LEGACY PRINTER APPLICATION

The Legacy Printer Application simply points to the directories of the
CUPS installed conventionally (not the CUPS Snap) on your system. So
it makes all of your installed printer drivers available in a Printer
Application. Some drivers may not work in the different environment
(for example if a filter or backend tries to communicate with the CUPS
daemon), but such cases are rare.

This especially makes the drivers available to the [CUPS
Snap](https://github.com/OpenPrinting/cups-snap) ([Snap
Store](https://snapcraft.io/cups)) and to CUPS 3.x
([library](https://github.com/OpenPrinting/libcups), [local
server](https://github.com/OpenPrinting/cups-local), [sharing
server](https://github.com/OpenPrinting/cups-sharing)) which do not
directly support classically installed printer drivers. So the CUPS
Snap/CUPS 3.x can be used as the system's standard printing
environment keeping the classically installed printer drivers. This
works but is generally not recommended, as most free software printer
drivers are already available as Printer Applications ([Snap
Store](https://snapcraft.io/search?q=OpenPrinting)), but there could
be some drivers which are not yet converted, especially many
proprietary legacy drivers from printer manufacturers. Here the Legacy
Printer Application comes in handy.

So the best way for a Linux distribution using the CUPS Snap or CUPS
3.x as its printing system it is recommended not to install the
standard drivers classically but use the appropriate Printer
Application (Snaps) instead, but also install the Legacy Printer
Application classically to catch any drivers for which we do not have
a Printer Application.

You can also use this Printer Application to test the driver which you
want to retro-fit before you start to configure your Printer
Application executable and package everything into a Snap (or other
packaging/containerization format). This library and the functions of
[libcupsfilters](https://github.com/OpenPrinting/libcupsfilters) and
[libppd](https://github.com/OpenPrinting/libppd) used by it try to
resemble the CUPS environment for the filters and backends as well as
possible: Environment variables, command lines, even side and back
channels. But it is always better to test whether the driver behaves
correctly, whether the PPD options are represented well on the "Device
Settings", "Media", and "Printing Defaults" pages of the web
interface, and whether the printer reacts correctly to IPP attributes
supplied with the job, especially `print-color-mode`, `print-quality`,
and `print-content-optimize`.

The Legacy Printer Application uses the (classically installed) CUPS
backends for communication with the printer hardware by default and
not the backends built into PAPPL, to get the best compatibility with
the CUPS drivers. To make the PAPPL backends also available, the
Legacy Printer Application has to be built with the
`--enable-pappl-backends-for-legacy-printer-app` for `./configure`.

The Legacy Printer Application searches for PPDs, PPD archives, driver
information files (`.drv`), and PPD-generating executables on

```
/usr/share/ppd/
/usr/share/cups/model/
/usr/lib/cups/driver/
/usr/share/cups/drv/
/var/lib/legacy-printer-app/ppd/
```
The last one is where the web interface drops user-uploaded PPD files.

It uses the following directories for its files:
```
/var/lib/legacy-printer-app
/var/spool/legacy-printer-app
/usr/share/legacy-printer-app
/usr/lib/legacy-printer-app
```
The last directory is linked to `/usr/lib/cups` so that the Printer
Application sees the filters and backends of CUPS.

The test page
```
/usr/share/legacy-printer-app/testpage.ps
```
is the good old 21-year-old PostScript test page of CUPS, but you can
easily use any other test page for your Printer Application.

Configured print queues and job history is saved in
```
/var/lib/legacy-printer-app/legacy-printer-app.state
```

You can set the `PPD_PATHS` environment variable to search other
places instead:
```
PPD_PATHS=/path/to/my/ppds:/my/second/place ./legacy-printer-app server
```

Simply put a colon-separated list of any amount of paths into the
variable, always the last being used by the "Add PPD files"
page. Creating a wrapper script is recommended.

For an alternative place for the test page use the TESTPAGE_DIR
environment variable:
```
TESTPAGE_DIR=`pwd` PPD_PATHS=/path/to/my/ppds:/my/second/place ./legacy-printer-app server
```
or for your own creation of a test page (PostScript, PDF, PNG, JPEG,
Apple Raster, PWG Raster):
```
TESTPAGE=/path/to/my/testpage/my_testpage.ps PPD_PATHS=/path/to/my/ppds:/my/second/place ./legacy-printer-app server
```

If anything behaves wrongly and you cannot get it working by modifying
the configuration of your Printer Application, your callbacks, regular
expressions, conversion rule selections, ... please report an [issue
on
libpappl-retrofit](https://github.com/OpenPrinting/pappl-retrofit/issues)
(we move it to cups-filters if it is actually there).


## ALREADY AVAILABLE PRINTER APPLICATIONS

Please have a look at the [PostScript Printer
Application](https://github.com/OpenPrinting/ps-printer-app), the
[Ghostscript Printer
Application](https://github.com/OpenPrinting/ghostscript-printer-app),
the [HPLIP Printer
Application](https://github.com/OpenPrinting/hplip-printer-app), and
the [Gutenprint Printer
Application](https://github.com/OpenPrinting/gutenprint-printer-app)
for examples on how Printer Applications and their Snaps are
created. All these Printer Applications use this library,
retro-fitting PostScript printer PPD files,
[Ghostscript](http://www.ghostscript.com/) drivers with
[Foomatic](https://github.com/OpenPrinting/foomatic-db) PPD files (and
many other printer drivers), HPLIP, and Gutenprint. Practically every
free software printer driver which is available as Debian package is
now also available as a Printer Application Snap. They can be all
installed from the [Snap
Store](https://snapcraft.io/search?q=OpenPrinting).

The HPLIP Printer Application is especially an example of how to add
driver-specific functionality which is beyond the pappl-retrofit
library and control this functionality through extra pages in the web
interface. Here a feature for downloading HP's proprietary plugin is
added.


## HISTORY

My work on making Printer Applications out of classic CUPS drivers
practically started when [PAPPL](https://www.msweet.org/pappl) got
born. I already saw the need for this before, especially for the [CUPS
Snap](https://github.com/OpenPrinting/cups-snap) and have opened GSoC
projects on that, but PAPPL was the real start for it.

I first started with PostScript printers, the simplest approach of
only PPD files without CUPS filters or backends, writing the
[PostScript Printer
Application](https://github.com/OpenPrinting/ps-printer-app) derived
from Michael Sweet's first working model, the [HP PCL Printer
Application](https://github.com/michaelrsweet/hp-printer-app). When I
saw the code of the PostScript Printer Application growing and also
when I added CUPS filter support as some PostScript PPDs use CUPS
filters to manage the password for secure printing options, I decided
on creating this library.

You can see the sections about the **PostScript Printer Application**
and about **Retro-fitting of CUPS printer drivers into Printer
ApplicationsPermalink** in my [monthly news
posts](https://openprinting.github.io/news/) on the OpenPrinting web
site.

I also wrote up my ideas on the design and the inner workings of this
library in the weeks of development before putting up a GitHub
repossitory for it on [this thread in the PostScript Printer
Application
GitHub](https://github.com/OpenPrinting/ps-printer-app/discussions/8).
The thread has ended now and further write-ups will appear in the
commit messages of this library's GIT, as I am already doing with the
[CUPS Snap](https://github.com/OpenPrinting/cups-snap/commits/master).


## LEGAL STUFF

The CUPS driver retro-fit library is Copyright © 2021-2022 by Till
Kamppeter.

It is derived from the PostScript Printer Application and this one
derived from the [HP PCL Printer
Application](https://github.com/michaelrsweet/hp-printer-app), a first
working model of a raster Printer Application using PAPPL.

The HP PCL Printer Application is Copyright © 2019-2020 by Michael R
Sweet.

This software is licensed under the Apache License Version 2.0 with an
exception to allow linking against GPL2/LGPL2 software (like older
versions of CUPS). See the files "LICENSE" and "NOTICE" for more
information.
