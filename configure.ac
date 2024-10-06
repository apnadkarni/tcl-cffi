#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during installation
dnl	to configure the system for the local environment.

#-----------------------------------------------------------------------
# Based on sample configure.ac for Tcl Extensions.  The only places you
# should need to modify this file are marked by the string __CHANGE__
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
# This will also define a special symbol for Windows (BUILD_<PACKAGE_NAME>
# so that we create the export library with the dll.
#-----------------------------------------------------------------------

AC_INIT([cffi],[2.0b2])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

TEA_INIT(3.13)

AC_CONFIG_AUX_DIR(tclconfig)

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

#TEA_PATH_TKCONFIG
#TEA_LOAD_TKCONFIG

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC and a few others to create the basic setup
# necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

TEA_ADD_SOURCES([generic/tclCffi.c \
                     generic/tclCffiAlias.c \
                     generic/tclCffiArena.c \
                     generic/tclCffiCallback.c \
                     generic/tclCffiEnum.c \
                     generic/tclCffiFunction.c \
                     generic/tclCffiHelp.c \
                     generic/tclCffiInterface.c \
                     generic/tclCffiLoad.c \
                     generic/tclCffiMemory.c \
                     generic/tclCffiNames.c \
                     generic/tclCffiPointer.c \
                     generic/tclCffiPrototype.c \
                     generic/tclCffiStruct.c \
                     generic/tclCffiTclh.c \
                     generic/tclCffiTypes.c \
                     generic/tclCffiWrapper.c])
TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([-I${srcdir}/generic -I${srcdir}/tclh/include])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([])


#--------------------------------------------------------------------
# __CHANGE__
#
# You can add more files to clean if your extension creates any extra
# files by extending CLEANFILES.
# Add pkgIndex.tcl if it is generated in the Makefile instead of ./configure
# and change Makefile.in to move it from CONFIG_CLEAN_FILES to BINARIES var.
#
# A few miscellaneous platform-specific items:
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

if test "${TEA_PLATFORM}" = "windows" ; then
    # Ensure no empty if clauses
    CLEANFILES="$CLEANFILES pkgIndex.tcl *.lib *.dll *.exp *.ilk *.pdb vc*.pch"
    #TEA_ADD_SOURCES([win/winFile.c])
    #TEA_ADD_INCLUDES([-I\"$(${CYGPATH} ${srcdir}/win)\"])
    TEA_ADD_LIBS([rpcrt4.lib])
else
    # Ensure no empty else clauses
    CLEANFILES="$CLEANFILES pkgIndex.tcl"
    if test "`uname -s`" = "Darwin" ; then
        TEA_ADD_LIBS([-framework CoreFoundation])
    else
        TEA_ADD_LIBS([-luuid])
    fi
fi

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
#TEA_PRIVATE_TCL_HEADERS

#TEA_PUBLIC_TK_HEADERS
#TEA_PRIVATE_TK_HEADERS
#TEA_PATH_X

#--------------------------------------------------------------------
# cffi specific configuration
#--------------------------------------------------------------------

AC_MSG_CHECKING([which library to use for foreign function calls])

AC_ARG_WITH([libffi],
            AS_HELP_STRING([--with-libffi],
                           [use libffi for foreign function calls [--with-libffi=yes]]),
            [], [with_libffi=default])

AC_ARG_ENABLE([staticffi],
              AS_HELP_STRING([--disable-staticffi],
                             [statically link ffi library [--enable-staticffi]]),
	      [], [enable_staticffi=yes])

AC_ARG_WITH([dyncall],
            AS_HELP_STRING([--with-dyncall],
                           [use dyncall for foreign function calls [--with-dyncall=no]]),
            [], [with_dyncall=default])


# Either of libffi or dyncall might be used.
AS_CASE([$with_libffi,$with_dyncall],
        [no,no],[AC_MSG_ERROR([Please, enable exactly one of --with-libffi or --with-dyncall.])],
        [default,default],[AS_VAR_SET([with_libffi], [yes])],
        [no,default],[AS_VAR_SET([with_dyncall], [yes])],
        [default,no],[AS_VAR_SET([with_libffi], [yes])],
        [default,yes],[], dnl continue
        [yes,default],[], dnl continue
        [no,yes],[], dnl continue
        [yes,no],[], dnl continue
        [AC_MSG_ERROR([Please, enable exactly one of --with-libffi or --with-dyncall.])])

AS_VAR_IF([with_libffi], [yes], [AC_MSG_RESULT([libffi])], [AC_MSG_RESULT([dyncall])])

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
# This auto-enables if Tcl was compiled threaded.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

TEA_ADD_CFLAGS([-DTCL_NO_DEPRECATED])

AC_CONFIG_COMMANDS_PRE(
    [# Must define -DFFI_BUILDING when linking statically, see commentary of FFI_API
     # in ffi.h for details.
     AS_VAR_IF([with_libffi], [yes],
        [AS_VAR_IF([enable_staticffi], [yes],
            [AS_VAR_APPEND([DEFS], [" -DFFI_BUILDING=static"])])])])

AS_VAR_IF([with_libffi], [yes],
    [AC_DEFINE([CFFI_USE_LIBFFI], 1, [Use libffi for foreign function calls])
     dnl Do not try to guess static linking absolute path if LIBFFI_LIBS was
     dnl explicitely given.
     AS_VAR_SET_IF([LIBFFI_LIBS], [],
                   [AS_VAR_IF([enable_staticffi], [yes],
                              [AS_VAR_SET([LIBFFI_LIBS], ["-Wl,-Bstatic -lffi -Wl,-Bdynamic"])],
                              [AS_VAR_SET([LIBFFI_LIBS], ["-lffi"])]
                             )
                   ])
     TEA_ADD_INCLUDES([$LIBFFI_CFLAGS])
     TEA_ADD_SOURCES([generic/tclCffiLibffi.c])
     TEA_ADD_LIBS([$LIBFFI_LIBS])])

dnl See with_libffi comments above.
AS_VAR_IF([with_dyncall], [yes],
    [AC_DEFINE([CFFI_USE_DYNCALL], 1, [Use dyncalls for foreign function calls])
     dnl When DYNCALL_LIBS
     TEA_ADD_CFLAGS([-DCFFI_USE_DYNCALL])
     TEA_ADD_INCLUDES([$DYNCALL_CFLAGS])
     TEA_ADD_SOURCES([generic/tclCffiDyncall.c])
     TEA_ADD_LIBS([-ldynload_s -ldyncall_s -ldyncallback_s])])




#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Determine the name of the tclsh and/or wish executables in the
# Tcl and Tk build directories or the location they were installed
# into. These paths are used to support running test cases only,
# the Makefile should not be making use of these paths to generate
# a pkgIndex.tcl file or anything else at extension build time.
#--------------------------------------------------------------------

TEA_PROG_TCLSH
#TEA_PROG_WISH

#--------------------------------------------------------------------
# Setup a *Config.sh.in configuration file.
#--------------------------------------------------------------------

#TEA_EXPORT_CONFIG([sample])
#AC_SUBST(SAMPLE_VAR)

#--------------------------------------------------------------------
# Needed for test DLL suffix depending on platform
#--------------------------------------------------------------------
AC_SUBST(SHLIB_SUFFIX)

#--------------------------------------------------------------------
# Needed for linking against tommath on Windows. TCL_TOMMATH_LIB_NAME
# will be set in tclConfig.sh when required (Windows, Tcl 9)
#--------------------------------------------------------------------
# No longer needed
#if test "X${TCL_TOMMATH_LIB_NAME}" != "X" ; then
    #TCL_TOMMATH_LIB=\${libdir}/${TCL_TOMMATH_LIB_NAME}
#fi
AC_SUBST(TCL_TOMMATH_LIB)

#--------------------------------------------------------------------
# Specify files to substitute AC variables in. You may alternatively
# have a special pkgIndex.tcl.in or other files which require
# substituting the AC variables in. Include these here.
#--------------------------------------------------------------------

AC_CONFIG_FILES([Makefile pkgIndex.tcl])

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the files
# specified with AC_CONFIG_FILES.
#--------------------------------------------------------------------

AC_OUTPUT
