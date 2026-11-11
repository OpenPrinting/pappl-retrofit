# CHANGES - OpenPrinting pappl-retrofit v1.0b3 - 2026-11-11
## CHANGES IN V1.0b3 (11th November 2026)

- Support for building with libcups3 and PAPPL 2.x
  - Printer Applications are principally for CUPS 3.x environments. So we make
    sure we can build there without needing any parts of CUPS 2.x (to support
    PPD files we had created libppd earlier)
  - Code is now done based on libcups3 and PAPPL 2.x APIs and for
    libcups2/PAPPL 1.4.x builds conversions are defined in libcups2-private.h
    and pappl1-private.h
  - Thanks to Biswadeep Purkayastha for adding libcups3 support initially and
    to Rohit Kumar for updating it and adding support for PAPPL 2.x
  - Pull requests #15, #50

- Run uploaded PPD files through tests
  - When uploading a PPD file, it gets tested for conformity with the PPD
    specifications by the `ppdTest()` function of libppd, which is derived
    from the `cupstestppd` utility of CUPS 2.x.
  - Pull request #9

- Use PAPPL configuration options from file
  - Let legacy-printer-app use PAPPL configuration options via file or
    command line, so that legacy-printer-app and LPrint can use the same way
    of configuration
  - Pull request #27

- Raw printing support
  - Implemented prPrintFile() for raw printing
  - Pull request #58

- CI testing workflow for different architectures and CUPS versions
  - Static analysis with cppcheck and CodeQL
  - Build tests and unit tests on 4 architectures (x86 64-bit, ARM 32/64-bit,
    RISC-V 64-bit) and on CUPS 2.x/2.5.x with PAPPL 1.4.x and CUPS 3.x with
    PAPPL 2.x (12 combinations)
  - Use emulations for ARM 32-bit and RISC-V
  - C unit testing suite
  - Pull requests #43, #44, #45, #47, #48, #49, #50, #54, #55, #56

- Man page for the Legacy Printer Application (`legacy-printer-app`)
  - As the Legacy Printer Application is a useful utility for end users,
    allowing them to continue using proprietary CUPS drivers for their
    legacy printers after updating to CUPS 3.x or to the CUPS Snap, we add
    a man page for it now, as we have already man pages for the other
    Printer Applications: PostScript, Ghostscript, HPLIP, and Gutenprint
    Printer Application.
  - When we install `legacy-printer-app` as a daemon, the man page will
    get installed in section 8.

- Fixed bugs found in security audit by 7ASecurity
  - SECURITY: Reject PPD names that leave the upload directory, for both add
    and delete operations
    (CVE-2026-123747, GHSA-wxj6-x3wg-4vmc, OCU-01-020, pull request #66)
  - Don't crash on a check box form field with no choice
    (OCU-01-022, pull request #70)
  - Thanks to Dhirender Choudhary for posting the pull requests and
    implementing the fixes

- Don't pin whitespace collapsing in the device compare test
  (Pull request #68)

- Fix crash on error when uploading a PPD file
  (Pull request #60)

- Give the sidechannel and the backchannel its own file descriptor
  (Issue #32, pull request #51)

- Fix pthread rwlock deadlock in `_prStatus` callback
  (Pull request #52)

- Fix crash when filter chain is NULL
  (Issue #30, pull request #42)

- Fix bug where `O_NONBLOCK` flag is set with the wrong fcntl
  (Pull request #39)

- Fix memory leaks from `compiled_re_list`
  (Pull request #31)

- Fix issues reported by OpenScanHub
  - Fix potential memory leaks
  - Protect `_prASCII()` from negative lengths
  - `cups-backends.c`: Ensure read string is NULL-terminated
  - Fix possible unterminated string
  - Pull request #28

- Modify path of legacy printer app in `Makefile.am`
  (Pull request #13)

- Changed new files for back and side channel to `//` comment format

- Spell checked comments, log output, and `README.md` with CSpell
  (Pull request #12)

- Typo in URL in `README.md`
  (Pull request #33)

- Introduced `CHANGES.md` file


## CHANGES IN V1.0b2 (11th November 2026)

- Support for Legacy Printer Application as system daemon
  We have added a systemd *.service file and an install mode for installing
  the Legacy Printer Application as system daemon. The mode is triggered by
  the --enable-legacy-printer-app-as-daemon option for ./configure. Then the
  *.service file gets installed and the executable legacy-printer-app gets
  installed into /usr/(local/)sbin/.
- README.md: Updated TODOs and fixed minor glitches
  Especially the PAPPL development got already far enough for localization and
  ink level reporting support, so that we do not need to tell any more that we
  are waiting for further the devlopment of PAPPL.
- CONTRIBUTING.md: Updated text to reflect pappl-retrofit
- COPYING, NOTICE: Fixed copyright year 2023 and simplified the file based on
  the fact that one can assign the package's license to all
  autotools-generated files.
- Adapted DEVELOPING.md to the pappl-retrofit API, it was still reflecting the
  CUPS API (file was copied over from CUPS).
- Removed _prDefaultPaperSize() from pappl-retrofit-private.h. The function
  got removed when PAPPL had introduced papplLocGetDefaultMediaSizeName()
  which replaces this functionality.


## CHANGES IN V1.0b1 (9th December 2022)

This is the first beta release of the upcoming pappl-retrofit 1.0.0.

This is a library to convert classic CUPS drivers, consisting of PPD files,
CUPS filters, and sometimes also CUPS backends, into Printer Applications, the
new format of printer drivers, mainly for CUPS 3.x which goes all-IPP and does
not support the PPD/filter concept for printer drivers any more. Printer
Applications are emulations of driverless IPP printers which on their other
end pass on the jobs to the actual printer.

pappl-retrofit uses PAPPL, library for Printer Applications, as its base, and
so it does not need to care of the general functionality of Printer
Applications. So it only contains the code to adapt classic CUPS drivers and
PPD files into the Printer Application framework.

To support as many classic drivers as possible pappl-retrofit supports all
kinds of PPD files, with and without specification of a CUPS filter,
installable accessory settings, CUPS extension for custom option values, *.drv
PPD compiler files, PPD-file-generating executables, CUPS filters, CUPS
backends, side and back channels for filter/backend communication,
pre-filtering from the driverless-IPP-standard input formats to the input
formats of the driver filters.

pappl-retrofit also comes with the Legacy Printer Application, which when it
is classically (not as Snap or other container) installed sees all classically
installed CUPS drivers on the system and maps them into its IPP printer
emulation, so that CUPS 3.x can make use of all these drivers. This way the
user does not loose their old printer drivers on the transition to CUPS 3.x,
which is especially important for (often proprietary) drivers from printer
manufacturers.

As this library was developed along with the 4 retro-fitting Printer
Applications for PostScript, Ghostscript, HPLIP, and Gutenprint as the base
for them, it has grown with these applications and contains all functionality
they need. It has also grown with PAPPL, getting support for PAPPL's newest
features.

So we are not releasing now because we completed a pre-planned feature list
but rather to have releases of this package and the Printer Applications for
their easier adoption into Linux distributions. With this we will also version
the Printer Application Snaps in the Snap Store, so that when distributions
adopt them as their default printer drivers, they can also better manage their
customer support.

Feature-wise we are even not 100% complete. We will still add ink level
read-out from the printer (SNMP-based network printers, same as supported by
CUPS) and internationalization, but this will not cause any
compatibility-breaking API changes.
