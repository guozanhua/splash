#											   -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.68)
AC_INIT(SPLASH, 0.3.10, emmanueldurand@gmail.com)
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_LIBTOOL
AM_INIT_AUTOMAKE([1.9 tar-ustar])
AC_CONFIG_HEADERS([include/config.h])

AC_SUBST([LIBSPLASH_SO_VERSION], [0:3:10])
AC_SUBST([LIBSPLASH_API_VERSION], [0.3])
AC_DEFINE(LIBSPLASH_API_VERSION, ["0.3"], [API version])

# Check for C and C++ compilers
AC_PROG_CXX
AC_PROG_CC_C_O
AC_PROG_OBJCXX

CC="clang"
CXX="clang++"
CXXFLAGS="$CXXFLAGS -std=c++11"

export PKG_CONFIG_PATH=$srcdir/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig

# Check for header files
AC_HEADER_STDC

# Check if we are trying to compile on OSX
case "$host" in *-darwin* | *-macos10*)
  LIBTOOL="glibtool"
  CXXFLAGS+=" -D__Darwin"
  LDFLAGS+=" -undefined dynamic_lookup -Wl,-headerpad_max_install_names"
  CPPFLAGS+=" -I/usr/local/include -I/usr/local/include/jsoncpp"
  CXXFLAGS+=" -I/usr/local/include -I/usr/local/include/jsoncpp"
  LDFLAGS+=" -L/usr/local/lib"
  if test -d /opt/local ; then
	CPPFLAGS+=" -I/opt/local/include"
	CXXFLAGS+=" -I/opt/local/include"
	LDFLAGS+=" -L/opt/local/lib"
  elif test -d /sw ; then
	CPPFLAGS+=" -I/sw/include"
	CXXFLAGS+=" -I/sw/include"
	LDFLAGS+=" -L/sw/lib"
  fi
  
  have_osx=true
  AM_CONDITIONAL([HAVE_OSX], true)
  AC_DEFINE(HAVE_OSX,[1],[Apple Mac OS X operating system detected])
  
  # OSX does not include /usr/local/lib/pkgconfig path, so we export it here
  # (for everyone, just in case)
  PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
  export PKG_CONFIG_PATH
  
  LDFLAGS+=" -fPIC -L/usr/local/lib -L/usr/local/lib64"
  ;;
*)
  have_osx=false
  AM_CONDITIONAL([HAVE_OSX], false)
  AC_DEFINE(HAVE_OSX,[0],[Apple Mac OS X operating system not detected, considering a Linux system])
  CXXFLAGS+=" -I/usr/include/jsoncpp -I/usr/local/include/jsoncpp"
  ;;
esac

# Check for SSE
AX_EXT

# GLFW
PKG_CHECK_MODULES([GLFW], [glfw3 >= 3.0.3])
if test "x${have_glfw}" = "xfalse" ; then
	AC_MSG_ERROR([Missing glfw])
fi

# OpenGL
#PKG_CHECK_MODULES([GL], [gl >= 2.3])
#if test "x${have_gl}" = "xfalse" ; then
#	AC_MSG_ERROR([Missing opengl])
#fi

# GLM
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([glm/glm.hpp],
	[HAVE_GLM=true],
	[HAVE_GLM=false])
AC_LANG_POP
if test "$HAVE_GLM" = "false" ; then
	AC_MSG_ERROR([Missing glm])
fi

# GPhoto
PKG_CHECK_MODULES([GPHOTO], [libgphoto2],
    AC_DEFINE([HAVE_GPHOTO], [1], [Defined to 1 if libgphoto2 is detected]),
    AC_DEFINE([HAVE_GPHOTO], [0], [Defined to 1 if libgphoto2 is detected]))
AM_CONDITIONAL(HAVE_GPHOTO, test -n "$GPHOTO_LIBS")

# OpenImageIO
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([OpenImageIO/imageio.h],
	[HAVE_OIIO=true],
	[HAVE_OIIO=false])
AC_LANG_POP
if test "$HAVE_OIIO" = "false" ; then
	AC_MSG_ERROR([Missing OpenImageIO])
fi

# Snappy
AC_LANG_PUSH([C])
AC_CHECK_HEADER([snappy-c.h],
	[HAVE_SNAPPY=true],
	[HAVE_SNAPPY=false])
AC_LANG_POP
if test "$HAVE_SNAPPY" = "false" ; then
	AC_MSG_ERROR([Missing Snappy])
fi

# JSoncpp
# Not tested through pkgconfig for OSX / Macports compatibility
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([json/reader.h],
	[HAVE_JSONCPP=true],
	[HAVE_JSONCPP=false])
AC_LANG_POP
if test "$HAVE_JSONCPP" = "false" ; then
	AC_MSG_ERROR([Missing JsonCPP])
fi

# Shmdata
PKG_CHECK_MODULES([SHMDATA], [shmdata-1.0],
    AC_DEFINE([HAVE_SHMDATA], [1], [Defined to 1 if shmdata-1.0 is detected]),
    AC_DEFINE([HAVE_SHMDATA], [0], [Defined to 0 if shmdata-1.0 is detect]))
AM_CONDITIONAL(HAVE_SHMDATA, test -n "$SHMDATA_LIBS")

# GNU Scientific Library, also used for calibration
PKG_CHECK_MODULES([GSL], [gsl])
if test "x${have_gsl}" = "xfalse" ; then
	AC_MSG_ERROR([Missing gsl])
fi

# ZMQ, for communication between World and Scenes
PKG_CHECK_MODULES([ZMQ], [libzmq > 3.0])
if test "x${have_zmq}" = "xfalse" ; then
	AC_MSG_ERROR([Missing libzmq])
fi

# FFMPEG, for very simple internal file reading
PKG_CHECK_MODULES([FFMPEG], [libavformat libavcodec libavutil libswscale],
    AC_DEFINE([HAVE_FFMPEG], [1], [Defined to 1 if ffmpeg is detected]),
    AC_DEFINE([HAVE_FFMPEG], [0], [Defined to 1 if ffmpeg is detected]))
AM_CONDITIONAL(HAVE_FFMPEG, test -n "$FFMPEG_LIBS")

# PortAudio, to play sound
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0],
    AC_DEFINE([HAVE_PORTAUDIO], [1], [Defined to 1 if portaudio-2.0 is detected]),
    AC_DEFINE([HAVE_PORTAUDIO], [0], [Defined to 1 if portaudio-2.0 is detected]))
AM_CONDITIONAL(HAVE_PORTAUDIO, test -n "$PORTAUDIO_LIBS")

# OpenCV, for their hardware support
PKG_CHECK_MODULES([OPENCV], [opencv],
    AC_DEFINE([HAVE_OPENCV], [1], [Defined to 1 if OpenCV is detected]),
    AC_DEFINE([HAVE_OPENCV], [0], [Defined to 1 if OpenCV is detected]))
AM_CONDITIONAL(HAVE_OPENCV, test -n "$OPENCV_LIBS")

# Boost, for the HTTP server
BOOST_REQUIRE([1.50])
BOOST_ASIO

# LTC, for synchronization with a SMTPE clock through audio input
PKG_CHECK_MODULES([LTC], [ltc],
    AC_DEFINE([HAVE_LTC], [1], [Defined to 1 if ltc is detected]),
    AC_DEFINE([HAVE_LTC], [0], [Defined to 1 if ltc is detected]))
AM_CONDITIONAL(HAVE_LTC, test -n "$LTC_LIBS")

# Check for Markdown to generate documentation
AC_CHECK_PROGS([MARKDOWN], [markdown])
if test "x$MARKDOWN" = "xtrue" ; then
	AC_MSG_WARN([Markdown was not found - README.md will not be converted to HTML])
fi
AM_CONDITIONAL([HAVE_MARKDOWN], [test -n "$MARKDOWN"])

# Some options
AC_ARG_WITH([tests],
	AS_HELP_STRING([--with-tests], [If enabled, all tests are enabled]),
	[],
	[with_tests=no])
AM_CONDITIONAL(HAVE_TESTS, test "x$with_tests" != "xno")

AC_CONFIG_FILES([
	Makefile
    addons/Makefile
    addons/blender/Makefile
    addons/blender/splash/Makefile
    addons/blender/templates/Makefile
	src/Makefile
	data/Makefile
	data/templates/Makefile
    osx/Info.plist:osx/Info.plist.in
    osx/Makefile
	tests/Makefile
	])

    #tools/Makefile

AC_OUTPUT
