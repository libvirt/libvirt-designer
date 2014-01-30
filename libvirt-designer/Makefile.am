
EXTRA_DIST = libvirt-designer.sym
CLEANFILES =

lib_LTLIBRARIES = libvirt-designer-1.0.la

DESIGNER_TEMPLATE_FILES = \
			libvirt-designer-enum-types.h.template \
			libvirt-designer-enum-types.c.template \
			$(NULL)

DESIGNER_GENERATED_ENUM_FILES = \
			libvirt-designer-enum-types.h \
			libvirt-designer-enum-types.c \
			$(NULL)

DESIGNER_GENERATED_FILES = \
			$(DESIGNER_GENERATED_ENUM_FILES) \
			$(NULL)

DESIGNER_HEADER_FILES = \
			libvirt-designer.h \
			libvirt-designer-internal.h \
			libvirt-designer-main.h \
			libvirt-designer-domain.h \
			$(NULL)
DESIGNER_SOURCE_FILES = \
			libvirt-designer-internal.c \
			libvirt-designer-main.c \
			libvirt-designer-domain.c \
			$(NULL)

libvirt_designer_1_0_ladir = $(includedir)/libvirt-designer-1.0/libvirt-designer
libvirt_designer_1_0_la_HEADERS = \
			$(DESIGNER_HEADER_FILES)
libvirt_designer_1_0_la_SOURCES = \
			$(DESIGNER_SOURCE_FILES)
nodist_libvirt_designer_1_0_la_HEADERS = \
			libvirt-designer-enum-types.h
			$(NULL)
nodist_libvirt_designer_1_0_la_SOURCES = \
			libvirt-designer-enum-types.c \
			$(NULL)
libvirt_designer_1_0_la_CFLAGS = \
			-DBINDIR="\"$(bindir)\"" \
			-DDATADIR="\"$(datadir)\"" \
			-DLIBVIRT_DESIGNER_BUILD \
			-DLIBEXECDIR="\"$(libexecdir)\"" \
			-DRUNDIR="\"$(rundir)\"" \
			$(COVERAGE_CFLAGS) \
			-I$(top_srcdir) \
			$(LIBOSINFO_CFLAGS) \
			$(LIBVIRT_GCONFIG_CFLAGS) \
			$(WARN_CFLAGS) \
			$(NULL)
libvirt_designer_1_0_la_LIBADD = \
			$(LIBOSINFO_LIBS) \
			$(LIBVIRT_GCONFIG_LIBS) \
			$(CYGWIN_EXTRA_LIBADD) \
			$(NULL)
libvirt_designer_1_0_la_DEPENDENCIES = \
                        libvirt-designer.sym
libvirt_designer_1_0_la_LDFLAGS = \
			$(WARN_CFLAGS) \
			$(COVERAGE_CFLAGS:-f%=-Wc,f%) \
			$(CYGWIN_EXTRA_LDFLAGS) \
			$(MINGW_EXTRA_LDFLAGS) \
			-Wl,--version-script=$(srcdir)/libvirt-designer.sym \
			-version-info $(LIBVIRT_DESIGNER_VERSION_INFO)


EXTRA_DIST += $(DESIGNER_TEMPLATE_FILES)

BUILT_SOURCES = $(DESIGNER_GENERATED_FILES)
CLEANFILES += $(DESIGNER_GENERATED_FILES)

libvirt-designer-enum-types.h: $(DESIGNER_HEADER_FILES) libvirt-designer-enum-types.h.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/libvirt-designer-enum-types.h.template $(DESIGNER_HEADER_FILES:%=$(srcdir)/%) ) | \
            sed -e "s/G_TYPE_VIR_CONFIG/GVIR_CONFIG_TYPE/" -e "s/g_vir/gvir/" > libvirt-designer-enum-types.h

libvirt-designer-enum-types.c: $(DESIGNER_HEADER_FILES) libvirt-designer-enum-types.c.template
	$(AM_V_GEN) ( $(GLIB_MKENUMS) --template $(srcdir)/libvirt-designer-enum-types.c.template $(DESIGNER_HEADER_FILES:%=$(srcdir)/%) ) | \
            sed -e "s/G_TYPE_VIR_CONFIG/GVIR_CONFIG_TYPE/" -e "s/g_vir/gvir/" > libvirt-designer-enum-types.c

check_PROGRAMS = test-designer-domain

TESTS = $(check_PROGRAMS)

test_designer_domain_CFLAGS = \
			-I$(top_srcdir) \
			$(COVERAGE_CFLAGS) \
			-I$(top_srcdir) \
			$(LIBOSINFO_CFLAGS) \
			$(LIBVIRT_GCONFIG_CFLAGS) \
			$(WARN_CFLAGS2) \
			$(NULL)
test_designer_domain_LDADD = \
			libvirt-designer-1.0.la
test_designer_domain_LDFLAGS = \
			$(LIBOSINFO_LIBS) \
			$(LIBVIRT_GCONFIG_LIBS) \
			$(COVERAGE_CFLAGS:-f%=-Wc,f%)

if WITH_INTROSPECTION

LibvirtDesigner-1.0.gir: libvirt-designer-1.0.la $(G_IR_SCANNER) Makefile.am
	$(AM_V_GEN) $(G_IR_SCANNER) \
                --quiet \
                --warn-all \
                --namespace LibvirtDesigner \
                --nsversion 1.0 \
                --include Libosinfo-1.0 \
                --include LibvirtGConfig-1.0 \
                --identifier-prefix=GVirDesigner \
                --symbol-prefix=gvir_designer \
                --library=$(builddir)/libvirt-designer-1.0.la \
                --output $@ \
                -I$(top_srcdir) \
                -I$(top_builddir) \
                --verbose \
                --pkg=libosinfo-1.0 \
                --pkg=libvirt-gconfig-1.0 \
                --c-include="libvirt-designer/libvirt-designer.h" \
                --pkg-export=libvirt-designer-1.0 \
                $(srcdir)/libvirt-designer.h \
                $(DESIGNER_SOURCE_FILES:%=$(srcdir)/%) \
                $(DESIGNER_HEADER_FILES:%=$(srcdir)/%) \
		$(DESIGNER_GENERATED_ENUM_FILES:%=%) \
		$(NULL)

girdir = $(datadir)/gir-1.0
gir_DATA = LibvirtDesigner-1.0.gir

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = LibvirtDesigner-1.0.typelib

%.typelib: %.gir
	$(AM_V_GEN)$(G_IR_COMPILER) \
                --includedir=$(builddir) \
                --includedir=$(girdir) \
                -o $@ $<

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # WITH_INTROSPECTION
