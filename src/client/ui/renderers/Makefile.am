noinst_HEADERS = \
	QuetooRenderer.h

noinst_LTLIBRARIES = \
	librenderers.la

librenderers_la_SOURCES = \
	QuetooRenderer.c

librenderers_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/client/ui \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

librenderers_la_LIBADD = \
	$(top_builddir)/src/client/renderer/librenderer.la
