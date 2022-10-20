AM_CFLAGS  = -I$(top_builddir)/include -I$(top_builddir)/libmetrics -I$(top_builddir)/lib 

if STATIC_BUILD
noinst_LTLIBRARIES    = libmodferraris.la
libmodferraris_la_SOURCES = mod_ferraris.c 
libmodferraris_la_LDFLAGS = -export-all-symbols
else
pkglib_LTLIBRARIES    = modferraris.la
modferraris_la_SOURCES = mod_ferraris.c 
modferraris_la_LDFLAGS = -lpigpiod_if2 -module -avoid-version

EXTRA_DIST = ../conf.d/mod_ferraris.conf
endif

pkglibdir = $(libdir)/ganglia
