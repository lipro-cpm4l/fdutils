[![Build Status](https://travis-ci.org/lipro-cpm4l/fdutils.svg?branch=master)](https://travis-ci.org/lipro-cpm4l/fdutils)

fdutils - Floppy utilities
==========================

This package contains utilities for configuring and debugging the Linux
floppy driver, for formatting extra capacity disks, for sending raw
commands to the floppy controller, for automatic floppy disk mounting
and unmounting, etc.

The package includes the following items:

- `superformat`: formats high capacity disks (up to 1992k for
  high density disks or up to 3984k for extra density disks);
- `fdmount`: automatically mounts/unmounts disks when they
  are inserted/removed;
- `xdfcopy`: formats, reads and writes OS/2's XDF disks;
- `MAKEFLOPPIES`: creates the floppy devices in /dev;
- `getfdprm`: prints the current disk geometry (number of sectors,
  track and heads etc.);
- `setfdprm`: sets the current disk geometry;
- `fdrawcmd`: sends raw commands to the floppy driver;
- `floppycontrol`: configures the floppy driver;
- `floppymeter`: measure capacity and speed of floppy drive;
- `diskd`: daemon to executes a command on disk insertion;
- `diskseekd`: simulates Messy Dos' drive cleaning effect (read manpage);
- general documentation about the floppy driver.

## Compilation

### Requirements

You will need an ANSI standard C compiler, prefered is the GNU CC.
Fdutils was mainly developed in the mid-1990s under Linux 1.x/2.x but
should also be usable under all later and latest versions.  You will
never use it under Linux foreign systems such as Windows or MacOS.

### Get the Code

```bash
git clone https://github.com/lipro-cpm4l/fdutils.git
cd fdutils
```

### Build and install the binary

```bash
./configure
make all
sudo make install
```

## Documentation

- [Fdutils](http://www.fdutils.linux.lu/Fdutils.html)
- [Fdutils FAQ](http://www.fdutils.linux.lu/faq.html)
- [Linux Floppy Disk](https://www.kernel.org/doc/html/latest/admin-guide/blockdev/floppy.html)

  - [Driver](https://github.com/torvalds/linux/blob/master/drivers/block/floppy.c)
  - [Driver History](https://github.com/torvalds/linux/commits/master/drivers/block/floppy.c)
  - [Driver Orphaned](https://github.com/torvalds/linux/commit/47d6a76)

## End of Floppy Disk in Linux?

### Linus Torvalds Marks Floppy Disks ‘Orphaned’

On 18 Jul 2019 in a recent [merge commit](https://github.com/torvalds/linux/commit/47d6a76)
to the Linux Kernel, Linus Torvalds **marked the floppy disk drivers as
orphaned**. Could this be the beginning of the end of floppy disks in Linux?

### Lack of maintenance of the original source code

In addition, there has been no further development at Alain Knaff's
original sources for more than a decade. The last version
[5.5 from 2005](http://www.fdutils.linux.lu/fdutils-5.5.tar.gz)
received a first
[patch in 2006](//www.fdutils.linux.lu/fdutils-5.5-20060227.diff.gz)
and a last official
[patch in 2008](//www.fdutils.linux.lu/fdutils-5.5-20081027.diff.gz).

However, thanks to the Debian project, there are still adaptations to the
ongoing development in the Linux kernel, especially the established systems
around device management with `devfs` and `udev`:
https://tracker.debian.org/pkg/fdutils

---

This is an unofficial fork!
===========================

Original written by Alain Knaff <alain@knaff.lu> and distributed
under the GNU General Public License version 2.

*Primary-site*: http://www.fdutils.linux.lu/
[(alternate download)](http://ibiblio.org/pub/linux/utils/disk-management/)

*Mailing-list*: https://www.fdutils.linux.lu/mailman/listinfo/fdutils,
[(archives)](http://lll.lu/pipermail/fdutils/)

## License terms and liability

The author provides the software in accordance with the terms of
the GNU-GPL. The use of the software is free of charge and is
therefore only at your own risk! **No warranty or liability!**

**Any guarantee and liability is excluded!**

## Authorship

*Primary-site*: http://www.fdutils.linux.lu/

### Source code

**Alain Knaff is the originator of the C source code and**
as well as the associated scripts, descriptions and help files.
This part is released and distributed under the GNU General
Public License (GNU-GPL) Version 2.

**A few parts of the source code are based on the work of other
authors:**

> - **Matteo Cypriani**, **Andreas Henriksson**, **Anibal Monsalve Salazar**,
>   **Christian Perrier**, **Jochen Voss**, **David Weinehall**, **Taral**,
>   **Thomas Preud'homme**, **Anthony Towns**, **Vaidhyanathan G Mayilrangam**,
>   **Anthony Fok**, **Mark W. Eichin**:
>   Debian package maintenance with many many bug fixes and improvements.
> - **Heiko Schroeder**:
>   Wrote the fdpatches, which inspired the new floppy driver since 1.1.41.
> - **David Niemi**:
>   Provided many useful suggestions and general support.  Contributed to
>   `superformat.c` and mtools.  Wrote `fdc version` detection code and the
>   `hlt`/`hut`/`srt` code.  Tested ED code, found many minor bugs.  Wrote
>   the `floppy_format` file, and added a few default formats.
> - **Bill Broadhurst**:
>   Tested and corrected ED code.
> - **Thorsten Meinecke**, **Sam Chessman**:
>   Pointed me (Alain Knaff) to 2m.  Reported a compatibility bug with dosemu.
> - **Ciriaco Garcia de Celis**:
>   Wrote the 2m program.
> - **Frank Lofaro**:
>   Provided information about an FDC which supports the version command,
>   but no other command.
> - **Andre Schroeter**, **Francois Genolini**:
>   Noticed that the mutual exclusion between `mount` and `open`
>   is harmful for Lilo.  Reported a device size bug.
> - **Wolfram Rick**, **Francois Genolini**:
>   Reported the ram disk loading bug.
> - **Mitch Miers**, **Laurent Chemla**, **Derek Atkins**, **Kai Makisara**:
>   Found an incompatibility with PCI & Adaptec 1542CF, proposed fixes
>   for the problem, and did experiments to find the explanation for
>   the problem.  Helped debugging the 1.1.52 alpha patches.
> - **Ulrich Dessauer**:
>   Proposed the ability to read 256 sectors (used by OS-9).
> - **Bill Metzenthen**:
>   Found a fix for the bug which broke `e2fsck`.
> - **Uwe Bonnes**:
>   Suggested modifications to mmount which allow more flexibility.
>   Helped debugging the 1.1.52 alpha patches.
> - **Sven Verdoolaege**:
>   Suggested mtools support for disk serial numbers.
> - **Dario Ballabio**:
>   Discovered that under heavy disk load fsync may return before
>   all buffers are flushed.  Helped debugging the 1.1.52 and
>   later alpha patches.
> - **Frank van Maarseveen**:
>   Helped debugging a hardware incompatibility with drives that are
>   "a bit slow detecting a floppy change".
> - **Karl Eichwalder**:
>   Proposed some changes to the Makefile.
>   Suggested mtools should check for mounted disks and non-msdos disks.
> - **Martin Schulze**:
>   Actually took diskseekd seriously and proposed an improvement.
> - **Rainer Zimmermann**:
>   Contributed fdmount, a daemon which automatically handles
>   mounting and unmounting of disks.

*see*: [COPYING](COPYING), [CREDITS](CREDITS),
[debian/changelog](https://salsa.debian.org/debian/fdutils/blob/master/debian/changelog),
[debian/patches](https://salsa.debian.org/debian/fdutils/blob/master/debian/patches)
