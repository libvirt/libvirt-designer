INCLUDES = \
		-I$(top_builddir)/libvirt-designer	\
		-I$(top_srcdir)

virtxml_LDADD = \
		$(top_builddir)/libvirt-designer/libvirt-designer-1.0.la

virtxml_CFLAGS = \
		$(COVERAGE_CFLAGS) \
		$(LIBOSINFO_CFLAGS) \
		$(LIBVIRT_GCONFIG_CFLAGS) \
		$(WARN_CFLAGS) \
		$(LIBVIRT_CFLAGS) \
		$(NULL)

virtxml_LDFLAGS = \
		$(LIBOSINFO_LIBS) \
		$(LIBVIRT_GCONFIG_LIBS) \
		$(LIBVIRT_LIBS)

bin_PROGRAMS = virtxml
