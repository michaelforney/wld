# wld: Makefile.am

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = $(fontconfig_CFLAGS) $(freetype_CFLAGS) $(pixman_CFLAGS)

lib_LTLIBRARIES = libwld.la
pkginclude_HEADERS = wld.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wld.pc

libwld_la_SOURCES = color.c drawable.c font.c
libwld_la_LIBADD = $(fontconfig_LIBS) $(freetype_LIBS) $(pixman_LIBS)

if WITH_WAYLAND
    AM_CFLAGS += $(wayland_client_CFLAGS)
    libwld_la_SOURCES += wayland.c
    libwld_la_LIBADD += $(wayland_client_LIBS)
    pkginclude_HEADERS += wayland.h
endif

if ENABLE_WAYLAND_DRM
    AM_CPPFLAGS = -I$(top_builddir)/protocol
    libwld_la_SOURCES += wayland-drm.c protocol/wayland-drm-protocol.c
    pkginclude_HEADERS += wayland-drm.h
endif

if ENABLE_WAYLAND_SHM
    libwld_la_SOURCES += wayland-shm.c
    pkginclude_HEADERS += wayland-shm.h
endif

if ENABLE_INTEL
    AM_CFLAGS += $(drm_intel_CFLAGS) $(intelbatch_CFLAGS)
    libwld_la_SOURCES += intel.c
    libwld_la_LIBADD += $(drm_intel_LIBS) $(intelbatch_LIBS)
    pkginclude_HEADERS += intel.h
endif

if ENABLE_PIXMAN
    libwld_la_SOURCES += pixman.c
    pkginclude_HEADERS += pixman.h
endif

SUBDIRS = protocol

