AC_DEFUN([LIBVIRT_DESIGNER_GETTEXT],[
    # Autoconf 2.61a.99 and earlier don't support linking a file only
    # in VPATH builds.  But since GNUmakefile is for maintainer use
    # only, it does not matter if we skip the link with older autoconf.
    # Automake 1.10.1 and earlier try to remove GNUmakefile in non-VPATH
    # builds, so use a shell variable to bypass this.
    GNUmakefile=GNUmakefile
    m4_if(m4_version_compare([2.61a.100],
        m4_defn([m4_PACKAGE_VERSION])), [1], [],
      [AC_CONFIG_LINKS([$GNUmakefile:$GNUmakefile], [],
        [GNUmakefile=$GNUmakefile])])

    GETTEXT_PACKAGE=libvirt-designer
    AC_SUBST(GETTEXT_PACKAGE)
    AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],"$GETTEXT_PACKAGE", [GETTEXT package name])

    dnl IT_PROG_INTLTOOL([0.35.0])
])
