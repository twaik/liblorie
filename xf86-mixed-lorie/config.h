/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

#include <stddef.h>
#ifndef __ANDROID__
#include <linux/signal.h>
#endif
#include "xorg-server.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* Name of package */
#define PACKAGE "xf86-video-lorie"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "https://bugs.freedesktop.org/enter_bug.cgi?product=xorg"

/* Define to the full name of this package. */
#define PACKAGE_NAME PACKAGE

/* Major version of this package */
#define PACKAGE_VERSION_MAJOR 0

/* Minor version of this package */
#define PACKAGE_VERSION_MINOR 3

/* Patch version of this package */
#define PACKAGE_VERSION_PATCHLEVEL 8

/* Version number of package */
#define VERSION STR(PACKAGE_VERSION_MAJOR) "." STR(PACKAGE_VERSION_MINOR) "." STR(PACKAGE_VERSION_PATCHLEVEL)

/* Define to the version of this package. */
#define PACKAGE_VERSION VERSION

/* Define to the full name and version of this package. */
#define PACKAGE_STRING PACKAGE " " PACKAGE_VERSION

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME PACKAGE

/* Define to the home page for this package. */
#define PACKAGE_URL ""
