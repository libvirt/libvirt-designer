INCLUDES = \
		-I$(top_builddir)/libvirt-designer	\
		-I$(top_srcdir)

virt_designer_LDADD = \
		$(top_builddir)/libvirt-designer/libvirt-designer-1.0.la

virt_designer_CFLAGS = \
		$(COVERAGE_CFLAGS) \
		$(LIBOSINFO_CFLAGS) \
		$(LIBVIRT_GCONFIG_CFLAGS) \
		$(WARN_CFLAGS) \
		$(LIBVIRT_GOBJECT_CFLAGS) \
		$(NULL)

virt_designer_LDFLAGS = \
		$(LIBOSINFO_LIBS) \
		$(LIBVIRT_GCONFIG_LIBS) \
		$(LIBVIRT_GOBJECT_LIBS)

POD2MAN = pod2man -c "Virtualization Support" -r "$(PACKAGE)-$(VERSION)"

%.1: %.c Makefile
	$(AM_V_GEN)$(POD2MAN) $< $@

if WITH_EXAMPLES
bin_PROGRAMS = virt-designer

man1_MANS = virt-designer.1

CLEANFILES = $(man1_MANS)
endif
