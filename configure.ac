dnl wld: configure.ac

AC_PREREQ([2.59])

AC_INIT([wld], [0.0.1], [mforney@mforney.org])
AM_INIT_AUTOMAKE([foreign nostdinc -Wall -Werror])
AC_LANG([C])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS

AM_PROG_AR
LT_INIT

AC_CONFIG_SRCDIR([wld.h])
AC_CONFIG_MACRO_DIR([m4])

PKG_PROG_PKG_CONFIG([0.9.0])

dnl Options {{{
# Debugging
AC_ARG_ENABLE([debug],
              AS_HELP_STRING([--enable-debug],
                             [enable debugging messages]),
              enable_debug=$enableval,
              enable_debug=no)
if test "x$enable_debug" = "xyes" ; then
    AC_DEFINE([ENABLE_DEBUG], [1], [Enable debug messages])
fi

# Wayland
AC_ARG_WITH([wayland],
            AS_HELP_STRING([--without-wayland],
                           [disable Wayland support]),
            with_wayland=$withval,
            with_wayland=yes)
if test "x$with_wayland" = "xyes" ; then
    PKG_CHECK_MODULES([wayland_client], [wayland-client],,
                      AC_MSG_ERROR([Wayland client libraries not found. Specify
                                    --without-wayland to build without wayland
                                    support]))
    AC_DEFINE([WITH_WAYLAND], [1], [Build Wayland support])
    PKGCONFIG_EXTRA_REQUIRES="wayland-client $PKGCONFIG_EXTRA_REQUIRES"

fi
AM_CONDITIONAL([WITH_WAYLAND], test "x$with_wayland" = "xyes")

# DRM
AC_ARG_ENABLE([drm],
              AS_HELP_STRING([--disable-drm],
                             [disable DRM support]),
              enable_drm=$enableval,
              enable_drm=yes)
AM_CONDITIONAL([ENABLE_DRM], [test "x$enable_drm" = "xyes"])

# Wayland DRM
AC_ARG_ENABLE([wayland-drm],
              AS_HELP_STRING([--disable-wayland-drm],
                             [disable Wayland DRM support]),
              enable_wayland_drm=$enableval,
              enable_wayland_drm=$with_wayland)
if test "x$enable_wayland_drm" = "xyes" ; then
    if test "x$with_wayland" != "xyes" ; then
        AC_MSG_ERROR([--enable-wayland-drm requires --with-wayland])
    fi
    AC_DEFINE([ENABLE_WAYLAND_DRM], [1], [Enable Wayland DRM support])
fi
AM_CONDITIONAL([ENABLE_WAYLAND_DRM], test "x$enable_wayland_drm" = "xyes")

# Wayland SHM
AC_ARG_ENABLE([wayland-shm],
              AS_HELP_STRING([--disable-wayland-shm],
                             [disable Wayland SHM support]),
              enable_shm=$enableval,
              enable_shm=$with_wayland)
if test "x$enable_wayland_shm" = "xyes" ; then
    if test "x$with_wayland" != "xyes" ; then
        AC_MSG_ERROR([--enable-wayland-shm requires --with-wayland])
    fi
    AC_DEFINE([ENABLE_WAYLAND_SHM], [1], [Enable Wayland SHM support])
fi
AM_CONDITIONAL([ENABLE_WAYLAND_SHM], test "x$enable_wayland_shm" = "xyes")

# Intel
AC_ARG_ENABLE([intel],
              AS_HELP_STRING([--disable-intel],
                             [disable Intel drawable support]),
              enable_intel=$enableval,
              enable_intel=auto)
if test "x$enable_intel" = "xyes" -o "x$enable_intel" = "xauto" ; then
    PKG_CHECK_MODULES([drm_intel], [libdrm_intel], have_drm_intel=yes,
        if test "x$enable_intel" = "xyes" ; then
            AC_MSG_ERROR([--enable-intel requires libdrm_intel])
        fi
    )
    PKG_CHECK_MODULES([intelbatch], [intelbatch], have_intelbatch=yes,
        if test "x$enable_intel" = "xyes" ; then
            AC_MSG_ERROR([--enable-intel requires libintelbatch])
        fi
    )

    if test "x$enable_intel" = "xauto" \
         -a "x$have_drm_intel" = "xyes" -a "x$have_intelbatch" = "xyes"
    then
        enable_intel=yes
    fi

    if test "x$enable_intel" = "xyes" ; then
        AC_DEFINE([ENABLE_INTEL], [1], [Enable Intel DRM support])
        PKGCONFIG_REQUIRES_PRIVATE="libdrm_intel $PKGCONFIG_REQUIRES_PRIVATE"
        PKGCONFIG_REQUIRES_PRIVATE="intelbatch $PKGCONFIG_REQUIRES_PRIVATE"
    fi
fi
AM_CONDITIONAL([ENABLE_INTEL], test "x$enable_intel" = "xyes")

# Pixman
AC_ARG_ENABLE([pixman],
              AS_HELP_STRING([--disable-pixman],
                             [disable pixman drawable support]),
              enable_pixman=$enableval,
              enable_pixman=yes)
if test "x$enable_pixman" = "xyes" ; then
    AC_DEFINE([ENABLE_PIXMAN], [1], [Enable Pixman support])
fi
AM_CONDITIONAL([ENABLE_PIXMAN], test "x$enable_pixman" = "xyes")
dnl }}}

dnl Check for libraries {{{
PKG_CHECK_MODULES([pixman], [pixman-1])
PKG_CHECK_MODULES([fontconfig], [fontconfig])
PKG_CHECK_MODULES([freetype], [freetype2])
dnl }}}

dnl Wayland protocol
WAYLAND_SCANNER_RULES(['$(top_srcdir)/protocol'])

dnl pkgconfig
AC_SUBST([PKGCONFIG_REQUIRES_PRIVATE])

AC_CONFIG_FILES([Makefile protocol/Makefile wld.pc])
AC_OUTPUT

