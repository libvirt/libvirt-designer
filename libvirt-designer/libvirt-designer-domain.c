/*
 * libvirt-designer-domain.c: libvirt domain configuration
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#include <config.h>
#include <sys/utsname.h>

#include "libvirt-designer/libvirt-designer.h"

#define GVIR_DESIGNER_DOMAIN_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), GVIR_DESIGNER_TYPE_DOMAIN, GVirDesignerDomainPrivate))

struct _GVirDesignerDomainPrivate
{
    GVirConfigDomain *config;
    GVirConfigCapabilities *caps;
    OsinfoOs *os;
    OsinfoPlatform *platform;
};

G_DEFINE_TYPE(GVirDesignerDomain, gvir_designer_domain, G_TYPE_OBJECT);

#define GVIR_DESIGNER_DOMAIN_ERROR gvir_designer_domain_error_quark()

static GQuark
gvir_designer_domain_error_quark(void)
{
    return g_quark_from_static_string("gvir-designer-domain");
}

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_OS,
    PROP_PLATFORM,
    PROP_CAPS,
};

static void gvir_designer_domain_get_property(GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    g_return_if_fail(GVIR_DESIGNER_IS_DOMAIN(object));

    GVirDesignerDomain *design = GVIR_DESIGNER_DOMAIN(object);
    GVirDesignerDomainPrivate *priv = design->priv;

    switch (prop_id) {
    case PROP_CONFIG:
        g_value_set_object(value, priv->config);
        break;

    case PROP_OS:
        g_value_set_object(value, priv->os);
        break;

    case PROP_PLATFORM:
        g_value_set_object(value, priv->platform);
        break;

    case PROP_CAPS:
        g_value_set_object(value, priv->caps);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void gvir_designer_domain_set_property(GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    g_return_if_fail(GVIR_DESIGNER_IS_DOMAIN(object));

    GVirDesignerDomain *design = GVIR_DESIGNER_DOMAIN(object);
    GVirDesignerDomainPrivate *priv = design->priv;

    switch (prop_id) {
    case PROP_OS:
        if (priv->os)
            g_object_unref(priv->os);
        priv->os = g_value_dup_object(value);
        break;

    case PROP_PLATFORM:
        if (priv->platform)
            g_object_unref(priv->platform);
        priv->platform = g_value_dup_object(value);
        break;

    case PROP_CAPS:
        if (priv->caps)
            g_object_unref(priv->caps);
        priv->caps = g_value_dup_object(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void gvir_designer_domain_finalize(GObject *object)
{
    GVirDesignerDomain *conn = GVIR_DESIGNER_DOMAIN(object);
    GVirDesignerDomainPrivate *priv = conn->priv;

    g_object_unref(priv->config);
    g_object_unref(priv->os);
    g_object_unref(priv->platform);
    g_object_unref(priv->caps);

    G_OBJECT_CLASS(gvir_designer_domain_parent_class)->finalize(object);
}


static void gvir_designer_domain_class_init(GVirDesignerDomainClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gvir_designer_domain_finalize;

    object_class->get_property = gvir_designer_domain_get_property;
    object_class->set_property = gvir_designer_domain_set_property;

    g_object_class_install_property(object_class,
                                    PROP_CONFIG,
                                    g_param_spec_object("config",
                                                        "Config",
                                                        "Domain config",
                                                        GVIR_CONFIG_TYPE_DOMAIN,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_OS,
                                    g_param_spec_object("os",
                                                        "Os",
                                                        "Operating system",
                                                        OSINFO_TYPE_OS,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_PLATFORM,
                                    g_param_spec_object("platform",
                                                        "Platform",
                                                        "Platform",
                                                        OSINFO_TYPE_PLATFORM,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_CAPS,
                                    g_param_spec_object("capabilities",
                                                        "Capabilities",
                                                        "Capabilities",
                                                        GVIR_CONFIG_TYPE_CAPABILITIES,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

    g_type_class_add_private(klass, sizeof(GVirDesignerDomainPrivate));
}


static void gvir_designer_domain_init(GVirDesignerDomain *design)
{
    GVirDesignerDomainPrivate *priv;
    g_debug("Init GVirDesignerDomain=%p", design);

    priv = design->priv = GVIR_DESIGNER_DOMAIN_GET_PRIVATE(design);
    priv->config = gvir_config_domain_new();
}


GVirDesignerDomain *gvir_designer_domain_new(OsinfoOs *os,
                                             OsinfoPlatform *platform,
                                             GVirConfigCapabilities *caps)
{
    return GVIR_DESIGNER_DOMAIN(g_object_new(GVIR_DESIGNER_TYPE_DOMAIN,
                                             "os", os,
                                             "platform", platform,
                                             "capabilities", caps,
                                             NULL));
}


/**
 * gvir_designer_domain_get_os:
 * @design: (transfer none): the domain designer instance
 *
 * Retrieves the operating system object associated with the designer.
 * The object should not be modified by the caller.
 *
 * Returns: (transfer none): the operating system
 */
OsinfoOs *gvir_designer_domain_get_os(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirDesignerDomainPrivate *priv = design->priv;
    return priv->os;
}


/**
 * gvir_designer_domain_get_platform:
 * @design: (transfer none): the domain designer instance
 *
 * Retrieves the virtualization platform object associated with the designer.
 * The object should not be modified by the caller.
 *
 * Returns: (transfer none): the virtualization platform
 */
OsinfoPlatform *gvir_designer_domain_get_platform(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirDesignerDomainPrivate *priv = design->priv;
    return priv->platform;
}


/**
 * gvir_designer_domain_get_capabilities:
 * @design: (transfer none): the domain designer instance
 *
 * Retrieves the capabilities object associated with the designer
 * The object should not be modified by the caller.
 *
 * Returns: (transfer none): the capabilities
 */
GVirConfigCapabilities *gvir_designer_domain_get_capabilities(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirDesignerDomainPrivate *priv = design->priv;
    return priv->caps;
}


/**
 * gvir_designer_domain_get_config:
 * @design: (transfer none): the domain designer instance
 *
 * Retrieves the domain config object associated with the designer
 * The object may be modified by the caller at will, but should
 * not be freed.
 *
 * Returns: (transfer none): the domain config
 */
GVirConfigDomain *gvir_designer_domain_get_config(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirDesignerDomainPrivate *priv = design->priv;
    return priv->config;
}


static gchar *
gvir_designer_domain_get_arch_normalized(const gchar *arch)
{
    /* Squash i?86 to i686 */
    if (g_str_equal(arch, "i386") ||
        g_str_equal(arch, "i486") ||
        g_str_equal(arch, "i586") ||
        g_str_equal(arch, "i686"))
        return g_strdup("i686");

    /* XXX what about Debian amd64 vs Fedora x86_64 */
    /* XXX any other arch inconsistencies ? */

    return g_strdup(arch);
}


static gchar *
gvir_designer_domain_get_arch_native(GVirDesignerDomain *design)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GVirConfigCapabilitiesHost *host =
        gvir_config_capabilities_get_host(priv->caps);
    GVirConfigCapabilitiesCpu *cpu = host ?
        gvir_config_capabilities_host_get_cpu(host) : NULL;
    const gchar *arch = cpu ?
        gvir_config_capabilities_cpu_get_arch(cpu) : NULL;

    if (arch) {
        return gvir_designer_domain_get_arch_normalized(arch);
    } else {
        struct utsname ut;
        uname(&ut);
        return gvir_designer_domain_get_arch_normalized(ut.machine);
    }
}


static GVirConfigCapabilitiesGuest *
gvir_designer_domain_get_guest(GVirDesignerDomain *design,
                               const gchar *wantarch)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GList *guests = gvir_config_capabilities_get_guests(priv->caps);
    GList *tmp = guests;
    GVirConfigCapabilitiesGuest *ret = NULL;

    while (tmp) {
        GVirConfigCapabilitiesGuest *guest =
            GVIR_CONFIG_CAPABILITIES_GUEST(tmp->data);
        GVirConfigCapabilitiesGuestArch *arch =
            gvir_config_capabilities_guest_get_arch(guest);
        GVirConfigDomainOsType guestos =
            gvir_config_capabilities_guest_get_os_type(guest);
        const gchar *guestarch =
            gvir_config_capabilities_guest_arch_get_name(arch);

        if (g_str_equal(guestarch, wantarch) &&
            (guestos == GVIR_CONFIG_DOMAIN_OS_TYPE_HVM ||
             guestos == GVIR_CONFIG_DOMAIN_OS_TYPE_LINUX ||
             guestos == GVIR_CONFIG_DOMAIN_OS_TYPE_XEN ||
             guestos == GVIR_CONFIG_DOMAIN_OS_TYPE_UML)) {
            ret = g_object_ref(guest);
            goto cleanup;
        }

        tmp = tmp->next;
    }

cleanup:
    g_list_foreach(guests, (GFunc)g_object_unref, NULL);
    g_list_free(guests);
    return ret;
}


static GVirConfigCapabilitiesGuest *
gvir_designer_domain_get_guest_full(GVirDesignerDomain *design,
                                    const gchar *wantarch,
                                    GVirConfigDomainOsType ostype)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GList *guests = gvir_config_capabilities_get_guests(priv->caps);
    GList *tmp = guests;
    GVirConfigCapabilitiesGuest *ret = NULL;

    while (tmp) {
        GVirConfigCapabilitiesGuest *guest =
            GVIR_CONFIG_CAPABILITIES_GUEST(tmp->data);
        GVirConfigCapabilitiesGuestArch *arch =
            gvir_config_capabilities_guest_get_arch(guest);
        GVirConfigDomainOsType guestos =
            gvir_config_capabilities_guest_get_os_type(guest);
        const gchar *guestarch =
            gvir_config_capabilities_guest_arch_get_name(arch);

        if (g_str_equal(guestarch, wantarch) &&
            guestos == ostype) {
            ret = g_object_ref(guest);
            goto cleanup;
        }

        tmp = tmp->next;
    }

cleanup:
    g_list_foreach(guests, (GFunc)g_object_unref, NULL);
    g_list_free(guests);
    return ret;
}


gboolean gvir_designer_domain_supports_machine(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    gchar *hostarch = gvir_designer_domain_get_arch_native(design);
    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest(design, hostarch);

    g_free(hostarch);
    if (guest) {
        g_object_unref(guest);
        return TRUE;
    }

    return FALSE;
}


gboolean gvir_designer_domain_supports_machine_full(GVirDesignerDomain *design,
                                                    const char *arch,
                                                    GVirConfigDomainOsType ostype)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, arch, ostype);

    if (guest) {
        g_object_unref(guest);
        return TRUE;
    }

    return FALSE;
}

gboolean gvir_designer_domain_supports_container(GVirDesignerDomain *design)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    gchar *hostarch = gvir_designer_domain_get_arch_native(design);
    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, hostarch,
                                            GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);

    g_free(hostarch);
    if (guest) {
        g_object_unref(guest);
        return TRUE;
    }

    return FALSE;
}

gboolean gvir_designer_domain_supports_container_full(GVirDesignerDomain *design,
                                                      const char *arch)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, arch,
                                            GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);

    if (guest) {
        g_object_unref(guest);
        return TRUE;
    }

    return FALSE;
}


static GVirConfigCapabilitiesGuestDomain *
gvir_designer_domain_best_guest_domain(GVirConfigCapabilitiesGuestArch *arch,
                                       GError **error)
{
    GList *domains =
        gvir_config_capabilities_guest_arch_get_domains(arch);
    GList *tmp = domains;
    GVirConfigCapabilitiesGuestDomain *ret = NULL;


    /* At this time "best" basically means pick KVM first.
     * Other cleverness might be added later... */
    while (tmp) {
        GVirConfigCapabilitiesGuestDomain *dom =
            GVIR_CONFIG_CAPABILITIES_GUEST_DOMAIN(tmp->data);

        if (gvir_config_capabilities_guest_domain_get_virt_type(dom) ==
            GVIR_CONFIG_DOMAIN_VIRT_KVM) {
            ret = g_object_ref(dom);
            goto cleanup;
        }

        tmp = tmp->next;
    }

    if (domains) {
        GVirConfigCapabilitiesGuestDomain *dom =
            GVIR_CONFIG_CAPABILITIES_GUEST_DOMAIN(domains->data);

        ret = g_object_ref(dom);
        goto cleanup;
    }

    g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                "Unable to find any domain for guest arch %s",
                gvir_config_capabilities_guest_arch_get_name(arch));

cleanup:
    g_list_foreach(domains, (GFunc)g_object_unref, NULL);
    g_list_free(domains);
    return ret;
}


static gboolean
gvir_designer_domain_setup_guest(GVirDesignerDomain *design,
                                 GVirConfigCapabilitiesGuest *guest,
                                 GError **error)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GVirConfigDomainOs *os = gvir_config_domain_os_new();
    GVirConfigCapabilitiesGuestArch *arch =
        gvir_config_capabilities_guest_get_arch(guest);
    GVirConfigCapabilitiesGuestDomain *domain;
    gboolean ret = FALSE;

    if (!(domain = gvir_designer_domain_best_guest_domain(arch,
                                                          error)))
        goto cleanup;

    gvir_config_domain_os_set_os_type(
        os,
        gvir_config_capabilities_guest_get_os_type(guest));
    gvir_config_domain_os_set_arch(
        os,
        gvir_config_capabilities_guest_arch_get_name(arch));
    gvir_config_domain_set_virt_type(
        priv->config,
        gvir_config_capabilities_guest_domain_get_virt_type(domain));
    gvir_config_domain_set_os(priv->config, os);

    ret = TRUE;
cleanup:
    g_object_unref(os);
    return ret;
}


gboolean gvir_designer_domain_setup_machine(GVirDesignerDomain *design,
                                            GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    gchar *hostarch = gvir_designer_domain_get_arch_native(design);
    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest(design, hostarch);
    gboolean ret = FALSE;

    if (!guest) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find machine type for architecture %s",
                    hostarch);
        goto cleanup;
    }

    if (!gvir_designer_domain_setup_guest(design, guest, error))
        goto cleanup;

    ret = TRUE;
cleanup:
    if (guest)
        g_object_unref(guest);
    g_free(hostarch);
    return ret;
}


gboolean gvir_designer_domain_setup_machine_full(GVirDesignerDomain *design,
                                                 const char *arch,
                                                 GVirConfigDomainOsType ostype,
                                                 GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, arch, ostype);
    gboolean ret = FALSE;

    if (!guest) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find machine type for architecture %s and ostype %s",
                    arch, "ostype" /* XXX */);
        goto cleanup;
    }

    if (!gvir_designer_domain_setup_guest(design, guest, error))
        goto cleanup;

    ret = TRUE;
cleanup:
    if (guest)
        g_object_unref(guest);
    return ret;
}


gboolean gvir_designer_domain_setup_container(GVirDesignerDomain *design,
                                              GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    gchar *hostarch = gvir_designer_domain_get_arch_native(design);
    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, hostarch,
                                            GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);
    gboolean ret = FALSE;

    if (!guest) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find container type for architecture %s",
                    hostarch);
        goto cleanup;
    }

    if (!gvir_designer_domain_setup_guest(design, guest, error))
        goto cleanup;

    ret = TRUE;
cleanup:
    if (guest)
    g_object_unref(guest);
    g_free(hostarch);
    return ret;
}


gboolean gvir_designer_domain_setup_container_full(GVirDesignerDomain *design,
                                                   const char *arch,
                                                   GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);

    GVirConfigCapabilitiesGuest *guest =
        gvir_designer_domain_get_guest_full(design, arch,
                                            GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);
    gboolean ret = FALSE;

    if (!guest) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find container type for architecture %s",
                    arch);
        goto cleanup;
    }

    if (!gvir_designer_domain_setup_guest(design, guest, error))
        goto cleanup;

    ret = TRUE;
cleanup:
    if (guest)
        g_object_unref(guest);
    return ret;
}
