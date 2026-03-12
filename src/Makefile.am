SUBDIRS = jpeg-v4
lib_LTLIBRARIES = libpsteg2.la
libpsteg2_la_SOURCES =jsteg.c jsteg.h jpeg.c jpeg.h \
		      interface.c interface.h
libpsteg2_la_CPPFLAGS = -DDATADIR='"$(datadir)"'
libpsteg2_la_LIBADD = jpeg-v4/libjpeg.la

EXTRA_DIST = steg.i \
	     VeraMono.ttf

if HAVE_SWIG
CLEANFILES = php_steganography.h \
	     steganography.php \
	     steg_wrap.c

BUILT_SOURCES = $(srcdir)/steg_wrap.c
SWIG_INTERFACES = steg.i
php_pstegdir = `php-config --extension-dir`
php_psteg_LTLIBRARIES = steganography2.la
nodist_steganography2_la_SOURCES = $(BUILT_SOURCES)
steganography2_la_CPPFLAGS = -DSTEG_SUPPORTED `php-config --includes`
steganography2_la_LDFLAGS = -module -shared -avoid-version
steganography2_la_LIBADD = libpsteg2.la

$(srcdir)/steg_wrap.c: $(SWIG_INTERFACES)
	$(SWIG) -php -o $@ $<
	sed -i 's/zend_error_noreturn/zend_error/' steg_wrap.c

endif

noinst_PROGRAMS = testlib
bin_PROGRAMS = decode
testlib_CPPFLAGS = -DSTEG_SUPPORTED @freetype2_CFLAGS@ @zlib_CFLAGS@
testlib_SOURCES = tests.c
testlib_LDADD = libpsteg2.la

decode_CPPFLAGS = -DSTEG_SUPPORTED @freetype2_CFLAGS@ @zlib_CFLAGS@
decode_SOURCES = decode.c
decode_LDADD = libpsteg2.la

fontdir = $(datadir)/@PACKAGE@
font_DATA = VeraMono.ttf

createdirs:
	mkdir -p $(fontdir)

uninstall-local:
	rm -rf $(fontdir)
