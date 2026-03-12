fontdir = $(datadir)/@PACKAGE@/
noinst_LTLIBRARIES = libjpeg.la
libjpeg_la_SOURCES = jcmaster.c jcdeflts.c jcarith.c jccolor.c jcexpand.c \
		    jchuff.c jcmcu.c jcpipe.c jcsample.c jfwddct.c jwrjfif.c \
		    jrdgif.c jrdppm.c jrdrle.c jrdtarga.c bitsource.c \
		    jdmaster.c jddeflts.c jbsmooth.c jdarith.c jdcolor.c \
		    jdhuff.c jdmcu.c jdpipe.c jdsample.c jquant1.c jquant2.c \
		    jrevdct.c jrdjfif.c jwrgif.c jwrppm.c jwrrle.c jwrtarga.c \
		    bitsink.c jutils.c jerror.c jmemmgr.c jmemsys.c \
		    bitsink.h bitsource.h jconfig.h jinclude.h jmemdos.h \
		    jmemsys.h jpegdata.h jversion.h
libjpeg_la_CPPFLAGS = -DSTEG_SUPPORTED -DDATADIR='"$(datadir)"' \
		      -DFONTDIR='"$(fontdir)"'
libjpeg_la_LDFLAGS = -static
