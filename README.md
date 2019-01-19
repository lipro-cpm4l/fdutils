Debian 10 (Buster) - `fdutils-5.5-20060227`
===========================================

This branch contains sorce code changes as made by Debian 10 (Buster) w/o `debian` folder,
[dists/buster/main/source/Sources.xz](http://debian.inet.de/debian/dists/buster/main/source/Sources.xz):

```
Package: fdutils
Binary: fdutils
Version: 5.5-20060227-8
Maintainer: Matteo Cypriani <mcy@lm7.fr>
Build-Depends: debhelper (>= 12), autoconf, automake, flex, texlive, texinfo, po-debconf
Architecture: alpha amd64 hppa ia64 i386 m68k powerpc powerpcspe ppc64 sparc sparc64
Standards-Version: 4.3.0
Format: 3.0 (quilt)
Files:
 32f7b13567b9f13dc9be35c259d6b82d 2033 fdutils_5.5-20060227-8.dsc
 cccab0f580659b91810a590436f688ee 222915 fdutils_5.5-20060227.orig.tar.gz
 7b5d2c27f8312790f973d3d66dea667f 48480 fdutils_5.5-20060227-8.debian.tar.xz
Vcs-Browser: https://salsa.debian.org/debian/fdutils
Vcs-Git: https://salsa.debian.org/debian/fdutils.git
Checksums-Sha256:
 38e1f1415a5fae907ce3b87819bb632cb777ad822d624f8d447612ffb57fb50c 2033 fdutils_5.5-20060227-8.dsc
 a867b381adc3596ca9a0c9139773bef18a38ceb5fa0e7401af46813c3a4b8d58 222915 fdutils_5.5-20060227.orig.tar.gz
 44afd8460cc386b74ced9fcaf6ed57f651904425a85972beea7189d57b6b550e 48480 fdutils_5.5-20060227-8.debian.tar.xz
Homepage: https://fdutils.linux.lu/
Package-List: 
 fdutils deb utils optional arch=alpha,amd64,hppa,ia64,i386,m68k,powerpc,powerpcspe,ppc64,sparc,sparc64
Directory: pool/main/f/fdutils
Priority: source
Section: utils
```

* snapshot.debian.org: https://snapshot.debian.org/package/fdutils/5.5-20060227-8/

  * [fdutils_5.5-20060227-8.debian.tar.xz](https://snapshot.debian.org/archive/debian/20190119T211302Z/pool/main/f/fdutils/fdutils_5.5-20060227-8.debian.tar.xz)
    SHA1:368cb0bb73983da0db9637d83329cb5452bee607
  * [fdutils_5.5-20060227-8.dsc](https://snapshot.debian.org/archive/debian/20190119T211302Z/pool/main/f/fdutils/fdutils_5.5-20060227-8.dsc)
    SHA1:c015d636a85c49019e11d8e44fe91f85b64086d0
  * [fdutils_5.5-20060227.orig.tar.gz](https://snapshot.debian.org/archive/debian-archive/20110127T084257Z/debian/pool/main/f/fdutils/fdutils_5.5-20060227.orig.tar.gz)
    SHA1:821ec7c8d489e0f34b450342ac7de7b95d9521a4

* sources.debian.org/src: https://sources.debian.org/src/fdutils/5.5-20060227-8/
* sources.debian.org/patches: https://sources.debian.org/patches/fdutils/5.5-20060227-8/

  * [series](https://sources.debian.org/src/fdutils/5.5-20060227-8/debian/patches/series/)
    [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/series)

    1. [makefile-destdir.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/makefile-destdir.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/makefile-destdir.patch)
       : Tune src/Makefile.in and doc/Makefile.in to use DESTDIR variable.
    1. [documentation-typos.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-typos.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-typos.patch)
       : fix many typos in the documentation
    1. [documentation-spelling_errors.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-spelling_errors.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-spelling_errors.patch)
       : quick spell-check against all the documentation
    1. [documentation-man_undefined_macros.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-man_undefined_macros.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-man_undefined_macros.patch)
       : Delete all occurrences of ".lp" and ".iX" in manpages to get rid of this lintian warnings.
    1. [documentation-man_hyphens.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-man_hyphens.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-man_hyphens.patch)
       : Get rid of lintian minor warning "hyphen-used-as-minus-sign".
    1. [documentation-info_metadata.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-info_metadata.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-info_metadata.patch)
       : The fdutils.texi file was missing the dircategory declaration, and the direntry declaration was wrong. This patch sets dircategory to "Utilities" and fixes direntry.
    1. [documentation-superformat_print_drive_deviation.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-superformat_print_drive_deviation.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-superformat_print_drive_deviation.patch)
       : document --print-drive-deviation in superformat(1)
    1. [documentation-section_media_description.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-section_media_description.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-section_media_description.patch)
       : The manpages setfdprm(1) and superformat(1) make a reference to an inexistant "Media description" section. That is because there is no generated manpage from mediaprm.texi.
         This patch adds the missing "Media description" sections, that cross-reference the Texinfo (or HTML, or DVI) documentation.
    1. [mediaprm-fix_etc-fdprm.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/mediaprm-fix_etc-fdprm.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/mediaprm-fix_etc-fdprm.patch)
       : This patch replaces occurrences of the old name /etc/fdprm by the new one /etc/mediaprm, in the file itself and in the fdprm manpage. It also fixes an indentation typo.
    1. [floppymeter-confirmation_message.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/floppymeter-confirmation_message.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/floppymeter-confirmation_message.patch)
       : [floppymeter] improve initial confirmation message
    1. [MAKEFLOPPIES-chown_separator.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/MAKEFLOPPIES-chown_separator.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/MAKEFLOPPIES-chown_separator.patch)
       : [makefloppies] fix the chown calls to use ':' instead of '.'
    1. [MAKEFLOPPIES-xsiisms.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/MAKEFLOPPIES-xsiisms.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/MAKEFLOPPIES-xsiisms.patch)
       : [makefloppies] remove xsiisms and replace use of /bin/bash with /bin/sh
    1. [MAKEFLOPPIES-devfs.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/MAKEFLOPPIES-devfs.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/MAKEFLOPPIES-devfs.patch)
       : [makefloppies] exit if devfs is detected
    1. [MAKEFLOPPIES-usage.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/MAKEFLOPPIES-usage.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/MAKEFLOPPIES-usage.patch)
       : [makefloppies] fix usage printing to fit the manpage
    1. [superformat-devfs_floppy.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/superformat-devfs_floppy.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/superformat-devfs_floppy.patch)
       : [superformat] use /dev/floppy/%d instead of /dev/fd%d on devfs-systems
    1. [superformat-env_variables.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/superformat-env_variables.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/superformat-env_variables.patch)
       : [superformat] inactivate setting of variables out of environment
    1. [superformat-verify_later.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/superformat-verify_later.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/superformat-verify_later.patch)
       : [superformat] The superformat option --verify_later requires an argument, but the variable verify_later is always used as a switch (0/1). This patch suppresses the required argument.
    1. [fdmount-compilation_linux_2.6.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/fdmount-compilation_linux_2.6.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/fdmount-compilation_linux_2.6.patch)
       : [fdmount] Allow compilation of fdutils with linux-kernel-headers 2.5.999-test7-bk-9. This patch is still needed with Linux 2.6.
    1. [dont_strip_binaries.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/dont_strip_binaries.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/dont_strip_binaries.patch)
       : Don't strip binaries as this is handled by dh_strip according to wether nostrip is set in DH_BUILD_OPTIONS or not.
    1. [help_messages.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/help_messages.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/help_messages.patch)
       : This patch contains several fixes related to the usage and help messages
    1. [remove_texi2html_dependency.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/remove_texi2html_dependency.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/remove_texi2html_dependency.patch)
       : remove dependency to obsolete texi2html
    1. [floppymeter-makefile_typo.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/floppymeter-makefile_typo.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/floppymeter-makefile_typo.patch)
       : [floppymeter] fix a typo in makefile to enable ldflags
    1. [documentation-faq_cleanup.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/documentation-faq_cleanup.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/documentation-faq_cleanup.patch)
       : [documentation] faq.html cleanup
    1. [config-ftbfs.patch](https://sources.debian.org/patches/fdutils/5.5-20060227-8/config-ftbfs.patch/)
       [(download)](https://sources.debian.org/data/main/f/fdutils/5.5-20060227-8/debian/patches/config-ftbfs.patch)
       : fix ftbfs with glibc 2.28
