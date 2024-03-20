#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([TCMediaPlayback], [1.0.0], [jun@telechips.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP

# Checks PKG-CONFIG
PKG_CHECK_MODULES([TCMP], [glib-2.0 dbus-1 TcUtils  gstreamer-1.0])

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_join], , )
AC_CHECK_LIB([m], [floor], , )

AC_ARG_ENABLE([systemd],
				AC_HELP_STRING([--enable-systemd], [enable systemd notify]))

AS_IF([test "x$enable_systemd" = "xyes"], [LIBS+=-lsystemd MEDIAPLAYBACKDEF=-DUSE_SYSTEMD])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MMAP
AC_CHECK_FUNCS([memcpy])
# output

AC_SUBST(MEDIAPLAYBACKDEF)

AC_CONFIG_FILES([Makefile
				src/Makefile])
AC_OUTPUT
