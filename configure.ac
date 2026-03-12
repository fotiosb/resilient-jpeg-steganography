#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT([psteg], [0.1], [fotios@fotios.org])
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/jsteg.c])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC([clang gcc cc])
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AX_PKG_SWIG(2.0.0, 
            [have_swig=true], 
            [ AC_MSG_ERROR([SWIG is required to build..]) ])
AM_CONDITIONAL([HAVE_SWIG], [test x$have_swig = xtrue])

LT_INIT
AM_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([gd], [gdImageCreate])
AC_CHECK_LIB([m], [sin])

PKG_CHECK_MODULES([freetype2], [freetype2 >= 2.4.0])
PKG_CHECK_MODULES([zlib], [zlib >= 1.2.0])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h memory.h stddef.h stdlib.h string.h strings.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strchr])

AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 src/jpeg-v4/Makefile
                 ])

AC_OUTPUT
