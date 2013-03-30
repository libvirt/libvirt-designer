AC_INIT(libvirt-designer, 0.0.2)
AC_CONFIG_SRCDIR(libvirt-designer/libvirt-designer.h)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
dnl Make automake keep quiet about wildcards & other GNUmake-isms
AM_INIT_AUTOMAKE([-Wno-portability])
AM_MAINTAINER_MODE([enable])
AC_CANONICAL_HOST

AM_SILENT_RULES([yes])

LIBOSINFO_REQUIRED=0.2.7
LIBVIRT_GCONFIG_REQUIRED=0.1.7
LIBVIRT_GOBJECT_REQUIRED=0.1.3
GOBJECT_INTROSPECTION_REQUIRED=0.10.8

AC_SUBST(LIBOSINFO_REQUIRED)
AC_SUBST(LIBVIRT_GCONFIG_REQUIRED)
AC_SUBST(LIBVIRT_GOBJECT_REQUIRED)

LIBVIRT_DESIGNER_MAJOR_VERSION=`echo $VERSION | awk -F. '{print $1}'`
LIBVIRT_DESIGNER_MINOR_VERSION=`echo $VERSION | awk -F. '{print $2}'`
LIBVIRT_DESIGNER_MICRO_VERSION=`echo $VERSION | awk -F. '{print $3}'`
LIBVIRT_DESIGNER_VERSION=$LIBVIRT_DESIGNER_MAJOR_VERSION.$LIBVIRT_DESIGNER_MINOR_VERSION.$LIBVIRT_DESIGNER_MICRO_VERSION$LIBVIRT_DESIGNER_MICRO_VERSION_SUFFIX
LIBVIRT_DESIGNER_VERSION_NUMBER=`expr $LIBVIRT_DESIGNER_MAJOR_VERSION \* 1000000 + $LIBVIRT_DESIGNER_MINOR_VERSION \* 1000 + $LIBVIRT_DESIGNER_MICRO_VERSION`

# In libtool terminology we need to figure out:
#
# CURRENT
#     The most recent interface number that this library implements.
#
# REVISION
#     The implementation number of the CURRENT interface.
#
# AGE
#     The difference between the newest and oldest interfaces that this
#     library implements.
#
# In other words, the library implements all the interface numbers
# in the range from number `CURRENT - AGE' to `CURRENT'.
#
# Libtool assigns the soname version from `CURRENT - AGE', and we
# don't want that to ever change, except at rare times where we
# need to break ABI in libvirt-designer.
#
# We would, however, like the libvirt-designer version number reflected
# in the so version'd symlinks, and this is based on AGE.REVISION
# eg  libvirt-designer.so.0.AGE.REVISION
#
# Assuming we do ever want to break soname version, this can
# toggled. But seriously, don't touch this, unless we explicitly
# agree to break ABI on the mailing list.
LIBVIRT_DESIGNER_SONUM=1

# The following examples show what libtool will do
#
# Input: 0.9.14 ->   libvirt-designer.so.0.9.14
# Input: 1.0.0  ->   libvirt-designer.so.0.1000.0
# Input: 2.5.8  ->   libvirt-designer.so.0.2005.8
#
AGE=`expr $LIBVIRT_DESIGNER_MAJOR_VERSION '*' 1000 + $LIBVIRT_DESIGNER_MINOR_VERSION`
REVISION=$LIBVIRT_DESIGNER_MICRO_VERSION
CURRENT=`expr $LIBVIRT_DESIGNER_SONUM + $AGE`
LIBVIRT_DESIGNER_VERSION_INFO=$CURRENT:$REVISION:$AGE

AC_SUBST([LIBVIRT_DESIGNER_MAJOR_VERSION])
AC_SUBST([LIBVIRT_DESIGNER_MINOR_VERSION])
AC_SUBST([LIBVIRT_DESIGNER_MICRO_VERSION])
AC_SUBST([LIBVIRT_DESIGNER_VERSION])
AC_SUBST([LIBVIRT_DESIGNER_VERSION_INFO])
AC_SUBST([LIBVIRT_DESIGNER_VERSION_NUMBER])

AC_PROG_CC
AM_PROG_CC_C_O
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([uname])
AC_PROG_CXX
AC_TYPE_SIZE_T

AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

AC_CONFIG_LIBOBJ_DIR([libvirt-designer])

LIBVIRT_DESIGNER_COMPILE_WARNINGS

PKG_CHECK_MODULES(LIBOSINFO, libosinfo-1.0 >= $LIBOSINFO_REQUIRED)
PKG_CHECK_MODULES(LIBVIRT_GCONFIG, libvirt-gconfig-1.0 >= $LIBVIRT_GCONFIG_REQUIRED)

LIBVIRT_DESIGNER_GETTEXT
LIBVIRT_DESIGNER_GTK_MISC
LIBVIRT_DESIGNER_WIN32
LIBVIRT_DESIGNER_COVERAGE
LIBVIRT_DESIGNER_INTROSPECTION

AC_ARG_ENABLE([examples],
              AS_HELP_STRING([--enable-examples], [enable virtxml example. Default is check, meaning it is enabled as long as libvirt-gobject is installed]),
              [],[enable_examples=check])

if test "x$enable_examples" != "xno" ; then
    PKG_CHECK_MODULES([LIBVIRT_GOBJECT],
                      [libvirt-gobject-1.0 >= $LIBVIRT_GOBJECT_REQUIRED],
                      [enable_examples=yes],
                      [
                       if test "x$enable_examples" = "xcheck" ; then
                           enable_examples=no
                       else
                           AC_MSG_ERROR([Cannot enable examples because libvirt-gobject is not available])
                       fi
                      ])
fi
AM_CONDITIONAL(WITH_EXAMPLES, [test "x$enable_examples" = "xyes"])

AC_ARG_ENABLE([vala],
              AS_HELP_STRING([--enable-vala], [enable Vala binding generation]),
              [], [enable_vala=check])
if test "x$enable_introspection" = "xyes" ; then
        if test "x$enable_vala" != "xno" ; then
                AC_PATH_PROG(VAPIGEN, vapigen, no)
                if test "x$VAPIGEN" == "xno"; then
                        if test "x$enable_vala" == "xcheck" ; then
                                enable_vala=no
                        else
                                AC_MSG_ERROR([Cannot find the "vapigen" binary in your PATH])
                        fi
                fi
                AC_SUBST(VAPIGEN)
        fi
        if test "x$enable_vala" != "xno" ; then
                AC_MSG_CHECKING([$VAPIGEN support for --metadatadir])
                $VAPIGEN --help | grep metadatadir 1>/dev/null 2>&1
                if test $? != 0 ; then
                        if test "x$enable_vala" == "xcheck" ; then
                                enable_vala=no
                                AC_MSG_RESULT([no])
                        else
                                AC_MSG_ERROR(["vapigen" binary in your PATH is too old (< 0.13)])
                        fi
                else
                        AC_MSG_RESULT([no])
                fi
        fi
        if test "x$enable_vala" = "xcheck" ; then
                enable_vala=yes
        fi
fi
AM_CONDITIONAL([WITH_VALA], [test "x$enable_vala" = "xyes"])

AC_OUTPUT(Makefile
          README
          libvirt-designer/Makefile
          libvirt-designer.spec
          libvirt-designer-1.0.pc
          vapi/Makefile
          docs/Makefile
          examples/Makefile)

AC_MSG_NOTICE([])
AC_MSG_NOTICE([Configuration summary])
AC_MSG_NOTICE([=====================])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([        Vala API: $enable_vala])
AC_MSG_NOTICE([        examples: $enable_examples])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ Libraries:])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([       LIBOSINFO: $LIBOSINFO_CFLAGS $LIBOSINFO_LIBS])
AC_MSG_NOTICE([ LIBVIRT_GCONFIG: $LIBVIRT_GCONFIG_CFLAGS $LIBVIRT_GCONFIG_LIBS])
AC_MSG_NOTICE([])
