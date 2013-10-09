dnl wld: configure.ac

AC_PREREQ([2.59])

AC_INIT([wld], [0.0.1], [mforney@mforney.org])
AM_INIT_AUTOMAKE([foreign nostdinc subdir-objects -Wall -Werror])
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

# DRM
AC_ARG_ENABLE([drm],
              AS_HELP_STRING([--disable-drm],
                             [disable DRM support]),
              enable_drm=$enableval,
              enable_drm=yes)
AM_CONDITIONAL([ENABLE_DRM], [test "x$enable_drm" = "xyes"])

# DRM drivers
DEFAULT_DRM_DRIVERS=intel
AC_ARG_WITH([drm-drivers],
            AS_HELP_STRING([--with-drm-drivers=DRIVERS],
                           [specify DRM drivers (comma separated)]),
            with_drm_drivers=$withval,
            with_drm_drivers=$DEFAULT_DRM_DRIVERS)
case "x$with_drm_drivers" in
    xyes)   with_drm_drivers=$DEFAULT_DRM_DRIVERS ;;
    xno)    with_drm_drivers= ;;
esac
if test "x$drm_drivers" != "x" -a "x$enable_drm" != "xyes"; then
    AC_MSG_ERROR([--with-drm-drivers requires --enable-drm])
fi
drm_drivers=`IFS=','; echo $with_drm_drivers`
for driver in $drm_drivers ; do
    case "x$driver" in
        xintel)
            AC_DEFINE([WITH_DRM_INTEL], [1], [Build Intel DRM driver])
            PKG_CHECK_MODULES([drm_intel], [libdrm_intel],,
                AC_MSG_ERROR([Intel DRM driver requires libdrm_intel]))
            PKG_CHECK_MODULES([intelbatch], [intelbatch],,
                AC_MSG_ERROR([Intel DRM driver requires libintelbatch]))
            PKGCONFIG_REQUIRES_PRIVATE="libdrm_intel intelbatch \
$PKGCONFIG_REQUIRES_PRIVATE"
            with_drm_intel=yes
        ;;
        *)
            AC_MSG_ERROR([Unknown DRM driver: '$driver'])
        ;;
    esac
done
AM_CONDITIONAL([WITH_DRM_INTEL], [test "x$with_drm_intel" = "xyes"])

# Wayland
AC_ARG_ENABLE([wayland],
              AS_HELP_STRING([--disable-wayland],
                             [disable Wayland support]),
              enable_wayland=$enableval,
              enable_wayland=yes)
if test "x$enable_wayland" = "xyes" ; then
    PKG_CHECK_MODULES([wayland_client], [wayland-client],,
        AC_MSG_ERROR([Wayland client libraries not found. Specify \
--disable-wayland to build without wayland support]))
    PKGCONFIG_REQUIRES_PRIVATE="wayland-client $PKGCONFIG_REQUIRES_PRIVATE"
fi
AM_CONDITIONAL([ENABLE_WAYLAND], [test "x$enable_wayland" = "xyes"])

DEFAULT_WAYLAND_INTERFACES=drm,shm
AC_ARG_WITH([wayland-interfaces],
            AS_HELP_STRING([--with-wayland-interfaces=INTERFACES],
                           [specify Wayland interfaces (comma separated)]),
            with_wayland_interfaces=$withval,
            with_wayland_interfaces=$DEFAULT_WAYLAND_INTERFACES)
case "x$with_wayland_interfaces" in
    xyes)   with_wayland_interfaces=$DEFAULT_WAYLAND_INTERFACES ;;
    xno)    with_wayland_interfaces= ;;
esac
if test "x$wayland_interfaces" != "x" -a "x$enable_wayland" != "xyes"; then
    AC_MSG_ERROR([--with-wayland-interfaces requires --enable-wayland])
fi
wayland_interfaces=`IFS=','; echo $with_wayland_interfaces`
for interface in $wayland_interfaces ; do
    case "x$interface" in
        xdrm)
            PKG_CHECK_MODULES([libdrm], [libdrm],,
                AC_MSG_ERROR([Wayland DRM support requires libdrm]))
            PKGCONFIG_REQUIRES_PRIVATE="libdrm $PKGCONFIG_REQUIRES_PRIVATE"
            AC_DEFINE([WITH_WAYLAND_DRM], [1],
                      [Build support for wl_drm global])
            with_wayland_drm=yes
        ;;
        xshm)
            AC_DEFINE([WITH_WAYLAND_SHM], [1],
                      [Build support for wl_shm global])
            with_wayland_shm=yes
        ;;
        *)
            AC_MSG_ERROR([Unknown Wayland interface: '$interface'])
        ;;
    esac
done
AM_CONDITIONAL([WITH_WAYLAND_DRM], [test "x$with_wayland_drm" = "xyes"])
AM_CONDITIONAL([WITH_WAYLAND_SHM], [test "x$with_wayland_shm" = "xyes"])

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

