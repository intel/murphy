AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

# murphy breedline test
noinst_PROGRAMS = breedline-murphy-test

breedline_murphy_test_SOURCES = breedline-murphy-test.c
breedline_murphy_test_CFLAGS  = $(AM_CFLAGS)
breedline_murphy_test_LDADD   = ../../libbreedline-murphy.la	\
				../../libbreedline.la		\
				../../libmurphy-common.la

# basic (blocking) breedline test
noinst_PROGRAMS += breedline-test

breedline_test_SOURCES = breedline-test.c
breedline_test_CFLAGS  = $(AM_CFLAGS)
breedline_test_LDADD   = ../../libbreedline.la

if GLIB_ENABLED
# breedline glib test
noinst_PROGRAMS += breedline-glib-test

breedline_glib_test_SOURCES = breedline-glib-test.c
breedline_glib_test_CFLAGS  = $(AM_CFLAGS) $(GLIB_CFLAGS)
breedline_glib_test_LDADD   = ../../libbreedline-glib.la	\
			      ../../libbreedline.la		\
			      $(GLIB_LIBS)
endif
