# wld: config.mk

PREFIX              = /usr/local
LIBDIR              = $(PREFIX)/lib
INCLUDEDIR          = $(PREFIX)/include
PKGCONFIGDIR        = $(LIBDIR)/pkgconfig

CC                  = gcc
CFLAGS              = -pipe

ENABLE_DEBUG        = 1

ENABLE_PIXMAN       = 1
ENABLE_DRM          = 1
ENABLE_WAYLAND      = 0

DRM_DRIVERS         = intel
WAYLAND_INTERFACES  = shm

ifeq ($(ENABLE_DRM),1)
    WAYLAND_INTERFACES += drm
endif

