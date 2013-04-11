NULL =

if WITH_VALA
vapidir = $(datadir)/vala/vapi
vapi_DATA =					\
	libvirt-designer-1.0.deps		\
	libvirt-designer-1.0.vapi		\
	$(NULL)

libvirt-designer-1.0.vapi: $(top_builddir)/libvirt-designer/LibvirtDesigner-1.0.gir
	$(AM_V_GEN)$(VAPIGEN)							 \
		--vapidir=$(builddir)						 \
		--pkg gio-2.0							 \
		--pkg libosinfo-1.0						 \
		--pkg libvirt-gconfig-1.0					 \
		--library libvirt-designer-1.0					 \
		--metadatadir=$(srcdir)						 \
		$<
endif

CLEANFILES =					\
	libvirt-designer-1.0.vapi		\
	$(NULL)

EXTRA_DIST =					\
	libvirt-designer-1.0.deps		\
	$(NULL)
