AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/external/jssy/jssy/ $(libcurl_CFLAGS) $(libfragmentzip_CFLAGS) $(libgeneral_CFLAGS)
AM_LDFLAGS = -L$(top_srcdir)/libfwkeyfetch $(libcurl_LIBS) $(libfragmentzip_LIBS) $(libgeneral_LIBS)

noinst_LTLIBRARIES = libjssy.la

lib_LTLIBRARIES = libfwkeyfetch.la

libjssy_la_CFLAGS = $(AM_CFLAGS)
libjssy_la_SOURCES = $(top_srcdir)/external/jssy/jssy/jssy.c

libfwkeyfetch_la_CXXFLAGS = $(AM_CFLAGS)
libfwkeyfetch_la_LIBADD = libjssy.la $(AM_LDFLAGS)
libfwkeyfetch_la_SOURCES = libfwkeyfetch.cpp
