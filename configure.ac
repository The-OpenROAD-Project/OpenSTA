# OpenSTA, Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Process this file with autoconf to produce a configure script.

AC_INIT(sta, 2.0.4)
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)

#--------------------------------------------------------------------
# Set default paths.
#--------------------------------------------------------------------

AC_ARG_ENABLE(32bit,
  [AS_HELP_STRING([--enable-32bit],[force 32 bit compile])],
  [case "${enableval}" in
    yes) force32bit=true ;;
    no)  force32bit=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-32bit) ;;
  esac],
  [force32bit=false])

HOST_ARCH=`uname -m`
HOST_OS=`uname -s`

if test x$force32bit = xtrue; then
  HOST_ARCH=i686
fi

echo -n "checking host... "
case "${HOST_OS}" in
  *Linux*)
    case "${HOST_ARCH}" in
      i?86)
	echo "Linux 32bit"
        INCLUDE_DIRS="/usr/include"
        LIB_DIRS="/usr/lib /usr/local/lib /usr/lib/i386-linux-gnu"
        LIB_EXTS="a so .so.1"
        ;;

      x86_64)
        echo "Linux 64bit"
        INCLUDE_DIRS="/usr/include"
        LIB_DIRS="/usr/lib64 /usr/lib /usr/lib/x86_64-linux-gnu"
        LIB_EXTS="a so"
        ;;

      *)
	echo "Linux unknown"
        INCLUDE_DIRS="/usr/include"
        LIB_DIRS="/usr/lib"
        LIB_EXTS="a so"
        ;;
    esac
    ;;

  CYGWIN*)
    echo "Cygwin"
    INCLUDE_DIRS="/usr/include"
    LIB_DIRS="/usr/lib"
    LIB_EXTS="a so"
    ;;

  Darwin)
    echo "Darwin"
    use_clang=true
    if test x$use_clang = xtrue; then
       # clang/gcc-apple llvm
       CXX=clang++
       CC=clang++
       INCLUDE_DIRS="/usr/local/include /usr/include"
       LIB_DIRS="/usr/local/lib /usr/lib"
    else
	# gcc
	CXX=g++
	CC=g++
	INCLUDE_DIRS="/usr/local/opt/tcl-tk/include /usr/local/opt/zlib/include /usr/include"
	LIB_DIRS="/usr/local/opt/tcl-tk/lib /usr/local/opt/zlib/lib /usr/local/lib"
    fi
    LIB_EXTS="dylib"
    ;;

  *)
    echo "Unknown"
    INCLUDE_DIRS="/usr/include /usr/local/include"
    LIB_DIRS="/usr/lib /usr/local/lib"
    LIB_EXTS="a so"
    ;;

esac

#--------------------------------------------------------------------

AC_ARG_ENABLE(debug,
  [AS_HELP_STRING([--enable-debug],[enable debug])],
  [case "${enableval}" in
    yes) debug=true ;;
    no)  debug=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
  esac],
  [debug=false])

AC_ARG_ENABLE(gprof,
  [AS_HELP_STRING([--enable-gprof],[enable gprof profiling])],
  [case "${enableval}" in
    yes) gprof=true ;;
    no)  gprof=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-gprof) ;;
  esac],
  [gprof=false])

AC_ARG_ENABLE(gcov,
  [AS_HELP_STRING([--enable-gcov],[enable gcov profiling])],
  [case "${enableval}" in
    yes) gcov=true ;;
    no)  gcov=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --with-gcov) ;;
  esac],
  [gcov=false])

AC_ARG_ENABLE(asan,
  [AS_HELP_STRING([--enable-asan],[enable AddressSanitizer])],
  [case "${enableval}" in
    yes) asan=true ;;
    no)  asan=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-asan) ;;
  esac],
  [asan=false])

AC_ARG_ENABLE(ssta,
  [AS_HELP_STRING([--enable-ssta],[enable ssta])],
  [case "${enableval}" in
    yes) ssta=true ;;
    no)  ssta=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-ssta) ;;
  esac],
  [ssta=false])

AC_ARG_WITH(include,
  [AS_HELP_STRING([--with-include=dirs],[directories to search for include files])],
  [INCLUDE_DIRS="${withval}"],
  [])

AC_ARG_WITH(lib,
  [AS_HELP_STRING([--with-lib=dirs],[directories to search for libraries])],
  [LIB_DIRS="${withval}"],
  [])

AC_ARG_WITH(cudd,
  [AS_HELP_STRING([--with-cudd=path],[use CUDD BDD package, defaults to $CUDD])],
  [CUDD_ARG="${withval}"],
  [CUDD_ARG=$CUDD])

AC_ARG_WITH(visualstudio,
  [AS_HELP_STRING([--with-visualstudio],[use Microcruft Visual Studio C++ compiler])],
  [case "${withval}" in
    yes) visualstudio=true ;;
    no)  visualstudio=false ;;
    *) AC_MSG_ERROR(bad value ${withval} for --with-visualstudio) ;;
  esac],
  [visualstudio=false])

if test x$visualstudio = xtrue; then
  CXX=cl
  CC=cl
  LIB_EXTS="lib"
fi

AC_ARG_WITH(optimize,
  [AS_HELP_STRING([--with-optimize],[obsolete; optimization is on by default])],
  [],
  [])

#--------------------------------------------------------------------

if test x$ssta = xtrue; then
   AC_DEFINE(SSTA, 1, "SSTA support")
else
   AC_DEFINE(SSTA, 0, "SSTA support")
fi

#--------------------------------------------------------------------
# Locate the Tcl package
#--------------------------------------------------------------------

TCL_INCLUDE=""
tcl_major="8"
tcl_minors="6 5 4 3 2"
for tcl_minor in $tcl_minors; do
  AC_MSG_CHECKING(for Tcl $tcl_major.$tcl_minor header file)
  found_tcl_header=false
  for dir in $INCLUDE_DIRS ; do
    tcl_header=$dir/tcl.h
    if test -r $tcl_header; then
      AC_MSG_RESULT($tcl_header)
      TCL_INCLUDE=$dir
      found_tcl_header=true
      break
    fi
  
    dir2=$dir/tcl$tcl_major.$tcl_minor
    tcl_header=$dir2/tcl.h
    if test -r $tcl_header; then
      AC_MSG_RESULT($tcl_header)
      TCL_INCLUDE=$dir2
      found_tcl_header=true
      break
    fi
  done
  if test -z "$TCL_INCLUDE"; then
    AC_MSG_RESULT(not found)
  fi

  AC_MSG_CHECKING(for Tcl $tcl_major.$tcl_minor library)
  TCL_LIB=""
  for dir in $LIB_DIRS ; do
    for ext in $LIB_EXTS; do
      # With period between tcl_major/tcl_minor.
      lib=tcl$tcl_major.$tcl_minor
      lib_path="$dir/lib$lib.$ext"
      if test -r $lib_path; then
        AC_MSG_RESULT($lib_path)
        TCL_LIB_DIR=$dir
        TCL_LIB=$lib
        break 2
      fi
      # Without period between major/minor.
      lib=tcl$tcl_major$tcl_minor
      lib_path="$dir/lib$lib.$ext"
      if test -r $lib_path; then
        AC_MSG_RESULT($lib_path)
        TCL_LIB_DIR=$dir
        TCL_LIB=$lib
        break 2
      fi
      # Without leading lib or period between major/minor (windoz).
      lib=tcl$tcl_major$tcl_minor
      lib_path="$dir/$lib.$ext"
      if test -r $lib_path; then
        AC_MSG_RESULT($lib_path)
        TCL_LIB_DIR=$dir
        TCL_LIB=$lib
        break 2
      fi
    done
  done
  if test -z "$TCL_LIB"; then
    AC_MSG_RESULT(not found)
  fi

  if test $found_tcl_header = true &&
     test -n "$TCL_LIB"; then
    break
  fi

done

#--------------------------------------------------------------------
# Locate the Zlib package
#--------------------------------------------------------------------

AC_MSG_CHECKING(for Zlib header file)
ZLIB_INCLUDE=""
for dir in $INCLUDE_DIRS ; do
  if test -r $dir/zlib.h; then
    AC_MSG_RESULT($dir)
    ZLIB_INCLUDE=$dir
    AC_DEFINE(ZLIB, 1, "Zlib compressed file reader")
    break
  fi
done
if test -z "$ZLIB_INCLUDE"; then
  AC_MSG_RESULT(not found)
fi

AC_MSG_CHECKING(for Zlib library)
ZLIB_LIB=""
for dir in $LIB_DIRS ; do
  for ext in $LIB_EXTS; do
    lib_path="$dir/libz.$ext"
    if test -r $lib_path; then
      AC_MSG_RESULT($lib_path)
      ZLIB_DIR=$dir
      ZLIB_LIB="z"
      break 2
    fi
  done
done
if test -z "$ZLIB_LIB"; then
  AC_MSG_RESULT(not found)
fi

AC_SUBST(ZLIB_INCLUDE)
AC_SUBST(ZLIB_LIB)

#--------------------------------------------------------------------
# Locate the Cudd package
#--------------------------------------------------------------------

CUDD_INCLUDE=""
CUDD_LIBS=""
if test "$CUDD_ARG"; then
  AC_MSG_RESULT($CUDD_ARG)
  AC_MSG_CHECKING(for CUDD header file)
  CUDD_INCLUDE_DIR="$CUDD_ARG/include"
  CUDD_HEADER="$CUDD_INCLUDE_DIR/cudd.h"
  if test -r "$CUDD_HEADER"; then
    AC_MSG_RESULT($CUDD_HEADER)
    CUDD_INCLUDE=$CUDD_INCLUDE_DIR
    AC_DEFINE(CUDD, 1, "CUDD bdd package")
  else
    AC_MSG_RESULT(not found)
  fi

  AC_MSG_CHECKING(for CUDD library)
  CUDD_LIB_EXT="a"
  CUDD_LIB="$CUDD_ARG/lib/libcudd.$CUDD_LIB_EXT"
  if test -r $CUDD_LIB; then
    AC_MSG_RESULT($CUDD_LIB)
    CUDD_LIBS="$CUDD_ARG/lib/libcudd.$CUDD_LIB_EXT"
  else
    AC_MSG_RESULT(not found)
  fi
fi

AC_SUBST(CUDD_LIBS)

#--------------------------------------------------------------------

# Checks for programs.
AC_PROG_CXX
# Disable shared libraries by default.
AC_DISABLE_SHARED
AC_PROG_AWK
AC_PROG_YACC
AM_PROG_LEX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for header files.
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(strings.h)
AC_CHECK_HEADERS(stddef.h)
AC_CHECK_HEADERS(inttypes.h)
AC_CHECK_HEADERS(float.h)
AC_CHECK_HEADERS(pthread.h, HAVE_PTHREAD=true, HAVE_PTHREAD=false)
AC_CHECK_HEADERS([sys/time.h])
AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_VPRINTF
AC_FUNC_STRTOD
AC_TYPE_SIZE_T

AC_CHECK_FUNC(strdup)
AC_CHECK_FUNC(strstr)
AC_CHECK_FUNC(strchr)
AC_CHECK_FUNC(strrchr)
AC_CHECK_FUNC(strpbrk)
AC_CHECK_FUNC(strtol)
AC_CHECK_FUNC(strtoul)
AC_CHECK_FUNC(strcasecmp)
AC_CHECK_FUNC(strncasecmp)
AC_CHECK_FUNC(gettimeofday)
AC_CHECK_FUNC(sqrt)
AC_CHECK_FUNC(memset)

AC_C_INLINE
AC_FUNC_ERROR_AT_LINE

#--------------------------------------------------------------------

if test $CXX = clang++; then
  # suppress deprecated-register warnings in flex files.
  CXXFLAGS="-std=c++11 -Wall -Wcast-qual -Wunused-parameter -Wno-deprecated-register"
  CXX_OPT="-O3"
  CXX_DEBUG="-g"
  CXX_ASAN="-fsanitize=address -O1 -fno-omit-frame-pointer"
  CXX_LD=$CXX

  LDFLAGS="-stdlib=libc++"
  if test x$HAVE_PTHREAD = xtrue; then
    LDFLAGS="$LDFLAGS -lpthread"
  fi
  if test "$TCL_LIB"; then
    LDFLAGS="$LDFLAGS -L$TCL_LIB_DIR -l$TCL_LIB"
  fi
  if test "$ZLIB_LIB"; then
    LDFLAGS="$LDFLAGS -L$ZLIB_DIR -l$ZLIB_LIB"
  fi

elif test x$GXX = xyes; then
  # Use GCC_EXTRA_FLAGS for local flags.
  # -Wno-deprecated-register to suppress warnings in flex files
  CXXFLAGS="-std=c++11 -Wall -Wextra -Wpointer-arith -Wcast-qual -Wno-deprecated-register -pipe $GCC_EXTRA_FLAGS"
  if test x$force32bit = xtrue; then
    CXXFLAGS="$CXXCFLAGS -m32"
  fi
  CXX_OPT="-O3"
  CXX_DEBUG="-g"
  CXX_GPROF="-pg -g"
  CXX_GCOV="-fprofile-arcs -ftest-coverage"
  CXX_LD=$CXX

  LDFLAGS=""
  if test x$HAVE_PTHREAD = xtrue; then
    LDFLAGS="$LDFLAGS -lpthread"
  fi
  if test "$TCL_LIB"; then
    LDFLAGS="$LDFLAGS -L$TCL_LIB_DIR -l$TCL_LIB"
  fi
  if test "$ZLIB_LIB"; then
    LDFLAGS="$LDFLAGS -L$ZLIB_DIR -l$ZLIB_LIB"
  fi

elif test x$visualstudio = xtrue; then
  # /Zc:forScope force conformance in for Loop scope
  # /EHa use c++ exception handling
  # /W3 display level 3 warnings
  # /WL one line diagnostics
  # /c compiles without linking
  # /GR enable run time type info (RTTI)
  CXX_FLAGS="/nologo /Zc:forScope /EHa /W3 /WL /c /GR"
  # /MDd use debug version of vc runtime library.
  # /RTC1 run time error check use of uninitialized variable, initialize stack
  #       variables to non-zero value
  # /GS buffer security check
  # /Z7 symbolic debugging info
  # /Od disable optimization
  CXX_DEBUG="/MDd /RTC1 /GS /Z7 /Od"
  # /MD use optimized version of vc runtime library.
  # /Ox full optimization
  # /G7 ??
  # /Gy enable function level linking
  CXX_OPT="/MD /Ox /G7 /Gy"
  CXX_GPROF=""
  CXX_GCOV=""

  CXX_LD="link"
  LDFLAGS="/nologo /SUBSYSTEM:CONSOLE /OPT:REF /IGNORE:4089 /NODEFAULTLIB:MSVCRT user32.lib advapi32.lib /OUT:sta"
  if test "$TCL_LIB"; then
    LDFLAGS="$LDFLAGS `cygpath -m $TCL_LIB_DIR/$TCL_LIB.$LIB_EXTS`"
  fi
  if test "$ZLIB_LIB"; then
    LDFLAGS="$LDFLAGS `cygpath -m $ZLIB_DIR/$ZLIB_LIB.$LIB_EXTS`"
  fi

  if test "$TCL_INCLUDE"; then
    TCL_INCLUDE=`cygpath -m $TCL_INCLUDE`
  fi
  if test "$ZLIB_INCLUDE"; then
    ZLIB_INCLUDE=`cygpath -m $ZLIB_INCLUDE`
  fi
  if test "$CUDD_INCLUDE"; then
    CUDD_INCLUDE=`cygpath -m $CUDD_INCLUDE`
  fi
fi

if test x$debug = xtrue; then
  CXXFLAGS+=" $CXX_DEBUG"
elif test x$optimize = xtrue; then
  CXXFLAGS+=" $CXX_OPT"
elif test x$gprof = xtrue; then
  if test $CXX = clang++; then
    echo "clang does not support gprof"
    exit
  fi
  CXXFLAGS+=" $CXX_GPROF"
  LDFLAGS+=" $CXX_GPROF"
elif test x$asan = xtrue; then
  CXXFLAGS+=" $CXX_ASAN"
elif test x$gcov = xtrue; then
  CXXFLAGS+=" $CXX_GCOV"
  LDFLAGS+=" $CXX_GCOV"
else
  # optimize by default
  CXXFLAGS+=" $CXX_OPT"
fi

YFLAGS="-d -p \$*_"
AC_SUBST(YFLAGS)

SWIG=swig
AC_SUBST(SWIG)
SWIG_FLAGS="-tcl8 -c++"
AC_SUBST(SWIG_FLAGS)

# The subdirectory app has to follow all library subdirs.
STA_SUBDIRS="util liberty network graph sdc sdf parasitics dcalc search tcl doc etc verilog app"

STA_INCLUDE="-I.. -I../util -I../liberty -I../network -I../graph -I../sdc -I../sdf -I../parasitics -I../dcalc -I../search -I../verilog -I../app"

STA_LIBS="../search/libsearch.la ../sdf/libsdf.la ../graph/libgraph.la ../dcalc/libdcalc.la ../parasitics/libparasitics.la ../sdc/libsdc.la ../liberty/libliberty.la ../network/libnetwork.la ../util/libutil.la"

SWIG_DEPEND="../tcl/StaException.i ../tcl/StaTcl.i ../tcl/NetworkEdit.i ../sdf/Sdf.i ../dcalc/DelayCalc.i ../parasitics/Parasitics.i"

TCL_INIT_FILES="../tcl/Util.tcl ../dcalc/DelayCalc.tcl ../tcl/Graph.tcl ../tcl/Liberty.tcl ../tcl/Link.tcl ../tcl/Network.tcl ../tcl/NetworkEdit.tcl ../parasitics/Parasitics.tcl ../tcl/Sdc.tcl ../sdf/Sdf.tcl ../tcl/Search.tcl ../tcl/Cmds.tcl ../tcl/Variables.tcl ../tcl/Sta.tcl ../tcl/Power.tcl ../tcl/Splash.tcl"

if test "$TCL_INCLUDE"; then
  STA_INCLUDE="$STA_INCLUDE -I$TCL_INCLUDE"
fi

if test "$ZLIB_INCLUDE"; then
  STA_INCLUDE="$STA_INCLUDE -I$ZLIB_INCLUDE"
fi

if test "$CUDD_INCLUDE"; then
  STA_INCLUDE="$STA_INCLUDE -I$CUDD_INCLUDE"
fi

AC_SUBST(STA_INCLUDE)
AC_SUBST(STA_LIBS)
AC_SUBST(TCL_INIT_FILES)
AC_SUBST(SWIG_DEPEND)
AM_CPPFLAGS=$STA_INCLUDE
AC_SUBST(AM_CPPFLAGS)

# libtool generation has to be after AR/AR_FLAGS are set.
AC_PROG_LIBTOOL

AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(util/Makefile)
AC_CONFIG_FILES(liberty/Makefile)
AC_CONFIG_FILES(network/Makefile)
AC_CONFIG_FILES(verilog/Makefile)
AC_CONFIG_FILES(graph/Makefile)
AC_CONFIG_FILES(sdc/Makefile)
AC_CONFIG_FILES(sdf/Makefile)
AC_CONFIG_FILES(parasitics/Makefile)
AC_CONFIG_FILES(dcalc/Makefile)
AC_CONFIG_FILES(search/Makefile)
AC_CONFIG_FILES(tcl/Makefile)
AC_CONFIG_FILES(doc/Makefile)
AC_CONFIG_FILES(etc/Makefile)
AC_CONFIG_FILES(app/Makefile)

# Pass $STA_SUBDIRS to Makefile.am.
AC_SUBST(STA_SUBDIRS)

AC_OUTPUT
