AC_PREREQ([2.69])
AC_INIT([libfwkeyfetch], m4_esyscmd([git rev-list --count HEAD | tr -d '\n']), [tihmstar@gmail.com])

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST
AM_PROG_LIBTOOL


AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_DEFINE([VERSION_COMMIT_COUNT], "m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])", [Git commit count])
AC_DEFINE([VERSION_COMMIT_SHA], "m4_esyscmd([git rev-parse HEAD | tr -d '\n'])", [Git commit sha])
AC_SUBST([VERSION_COMMIT_COUNT], ["m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_COMMIT_SHA], ["m4_esyscmd([git rev-parse HEAD | tr -d '\n'])"])


# Checks for programs.
AC_PROG_CXX([clang++])
AC_PROG_CC([clang])

# Check for operating system
AC_MSG_CHECKING([whether we need platform-specific build settings])
case $host_os in
  darwin* )
        AC_MSG_RESULT([${host_os}])
        CXXFLAGS+=" -stdlib=libc++"
        ;;
  *mingw32*|*cygwin*|*msys*)
        AC_MSG_RESULT([${host_os}])
        win32=true
        LDFLAGS+=" -no-undefined"
        ;;
esac

CXXFLAGS+=" -std=c++11"
CFLAGS+=" -std=c11 -fvisibility=hidden"

# Checks for libraries.
LIBCURL_REQUIRES_STR="libcurl >= 8.5.0"
LIBFRAGMENTZIP_REQUIRES_STR="libfragmentzip >= 70"
LIBGENERAL_REQUIRES_STR="libgeneral >= 83"

PKG_CHECK_MODULES(libcurl, $LIBCURL_REQUIRES_STR)
PKG_CHECK_MODULES(libfragmentzip, $LIBFRAGMENTZIP_REQUIRES_STR, have_libfragmentzip=yes, have_libfragmentzip=no)
PKG_CHECK_MODULES(libgeneral, $LIBGENERAL_REQUIRES_STR)

AC_SUBST([libcurl_requires], [$LIBCURL_REQUIRES_STR])
AC_SUBST([libfragmentzip_requires], [$LIBFRAGMENTZIP_REQUIRES_STR])
AC_SUBST([libgeneral_requires], [$LIBGENERAL_REQUIRES_STR])

AC_ARG_WITH([libfragmentzip],
            [AS_HELP_STRING([--without-libfragmentzip],
            [do not build with libfragmentzip support @<:@default=yes@:>@])],
            [with_libfragmentzip=no],
            [with_libfragmentzip=yes])

AC_ARG_WITH([remote-keys],
            [AS_HELP_STRING([--without-remote-keys],
            [do not request firmware keys from remote public sources @<:@default=no@:>@])],
            [with_remote_keys=no],
            [with_remote_keys=yes])

AC_ARG_ENABLE([debug],
            [AS_HELP_STRING([--enable-debug],
            [enable debug build(default is no)])],
            [debug_build=true],
            [debug_build=false])

AM_CONDITIONAL(WITH_REMOTE_KEYS, test "x$with_remote_keys" = "xyes")

if test "x$with_remote_keys" = "xyes"; then
  AC_DEFINE(WITH_REMOTE_KEYS, 1, [Define if you build with support for requesting keys from remote public database])
fi

if test "x$with_libfragmentzip" == "xyes"; then
  if test "x$have_libfragmentzip" != "xyes"; then
    AC_MSG_ERROR([requested building with libfragmentzip, but library could not be found])
  fi

  AC_DEFINE(HAVE_LIBFRAGMENTZIP, 1, [Define if you have libfragmentzip])

  AC_SUBST(libfragmentzip_CFLAGS)
  AC_SUBST(libfragmentzip_LIBS)
else
  echo "*** Note: libfragmentzip has been disabled ***"
fi

if test "$debug_build" = true; then
  echo "*** Note: debug build requested ***"
  CFLAGS+=" -g -O0 -DDEBUG=1"
  CXXFLAGS+=" -g -O0 -DDEBUG=1"
fi


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libfwkeyfetch/Makefile
		             libfwkeyfetch.pc])
AC_OUTPUT

echo "
Configuration for $PACKAGE-$VERSION:
-------------------------------------------

  install prefix ..........: $prefix
  with libfragmentzip .....: $with_libfragmentzip
  with remote keys ........: $with_remote_keys
  debug build  ............: $debug_build"

echo "  compiler ................: ${CC}

  Now type 'make' to build $PACKAGE-$VERSION,
  and then 'make install' for installation.
"
