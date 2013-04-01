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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Daniel P. Berrange <berrange@redhat.com>
 *   Michal Privoznik <mprivozn@redhat.com>
 */

#include <config.h>
#include <sys/utsname.h>

#include "libvirt-designer/libvirt-designer.h"
#include "libvirt-designer/libvirt-designer-internal.h"

#define GVIR_DESIGNER_DOMAIN_GET_PRIVATE(obj)                         \
        (G_TYPE_INSTANCE_GET_PRIVATE((obj), GVIR_DESIGNER_TYPE_DOMAIN, GVirDesignerDomainPrivate))

struct _GVirDesignerDomainPrivate
{
    GVirConfigDomain *config;
    GVirConfigCapabilities *caps;
    OsinfoDb *osinfo_db;
    OsinfoOs *os;
    OsinfoPlatform *platform;

    OsinfoDeployment *deployment;
    OsinfoDeviceDriverList *drivers;

    /* next disk targets */
    unsigned int ide;
    unsigned int virtio;
    unsigned int sata;
};

G_DEFINE_TYPE(GVirDesignerDomain, gvir_designer_domain, G_TYPE_OBJECT);

#define GVIR_DESIGNER_DOMAIN_ERROR gvir_designer_domain_error_quark()

typedef enum {
    GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK,
    /* add new type here */
} GVirDesignerDomainNICType;

static GQuark
gvir_designer_domain_error_quark(void)
{
    return g_quark_from_static_string("gvir-designer-domain");
}

static gboolean error_is_set(GError **error)
{
    return ((error != NULL) && (*error != NULL));
}

static const char GVIR_DESIGNER_VIRTIO_BLOCK_DEVICE_ID[] = "http://pciids.sourceforge.net/v2.2/pci.ids/1af4/1001";

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_OS,
    PROP_PLATFORM,
    PROP_CAPS,
    PROP_OSINFO_DB,
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

    case PROP_OSINFO_DB:
        g_value_set_object(value, priv->osinfo_db);
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
    case PROP_OSINFO_DB:
        if (priv->osinfo_db)
            g_object_unref(priv->osinfo_db);
        priv->osinfo_db = g_value_dup_object(value);
        break;
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
    if (priv->deployment)
        g_object_unref(priv->deployment);
    if (priv->osinfo_db)
        g_object_unref(priv->osinfo_db);
    if (priv->drivers)
        g_object_unref(priv->drivers);

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
                                    PROP_OSINFO_DB,
                                    g_param_spec_object("osinfo-db",
                                                        "Osinfo Database",
                                                        "libosinfo database",
                                                        OSINFO_TYPE_DB,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
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


static OsinfoDeviceList *
gvir_designer_domain_get_devices_from_drivers(GVirDesignerDomain *design,
                                              OsinfoFilter *filter)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    OsinfoDeviceList *devices;
    unsigned int i;


    devices = osinfo_devicelist_new();

    for (i = 0; i < osinfo_list_get_length(OSINFO_LIST(priv->drivers)); i++) {
        OsinfoDeviceDriver *driver;
        OsinfoDeviceList *driver_devices;

        driver = OSINFO_DEVICE_DRIVER(osinfo_list_get_nth(OSINFO_LIST(priv->drivers), i));
        driver_devices = osinfo_device_driver_get_devices(driver);
        osinfo_list_add_filtered(OSINFO_LIST(devices),
                                 OSINFO_LIST(driver_devices),
                                 filter);
    }

    return devices;
}


/* Gets the list of devices matching filter that are natively supported
 * by (OS) and (platform), or that are supported by (OS with a driver) and
 * (platform).
 * Drivers are added through gvir_designer_domain_add_driver()
 */
static OsinfoDeviceList *
gvir_designer_domain_get_supported_devices(GVirDesignerDomain *design,
                                           OsinfoFilter *filter)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    OsinfoDeviceList *os_devices;
    OsinfoDeviceList *platform_devices;
    OsinfoDeviceList *driver_devices;
    OsinfoDeviceList *devices;

    os_devices = osinfo_os_get_all_devices(priv->os, filter);
    platform_devices = osinfo_platform_get_all_devices(priv->platform, filter);
    driver_devices = gvir_designer_domain_get_devices_from_drivers(design, filter);

    devices = osinfo_devicelist_new();

    if (platform_devices == NULL)
        goto end;

    if (os_devices != NULL)
        osinfo_list_add_intersection(OSINFO_LIST(devices),
                                     OSINFO_LIST(os_devices),
                                     OSINFO_LIST(platform_devices));

    if (driver_devices != NULL)
        osinfo_list_add_intersection(OSINFO_LIST(devices),
                                     OSINFO_LIST(driver_devices),
                                     OSINFO_LIST(platform_devices));

end:
    if (os_devices != NULL)
        g_object_unref(os_devices);

    if (platform_devices != NULL)
        g_object_unref(platform_devices);

    if (driver_devices != NULL)
        g_object_unref(driver_devices);

    return devices;
}


static void gvir_designer_domain_init(GVirDesignerDomain *design)
{
    GVirDesignerDomainPrivate *priv;
    g_debug("Init GVirDesignerDomain=%p", design);

    priv = design->priv = GVIR_DESIGNER_DOMAIN_GET_PRIVATE(design);
    priv->config = gvir_config_domain_new();
    priv->drivers = osinfo_device_driverlist_new();
}


GVirDesignerDomain *gvir_designer_domain_new(OsinfoDb *db,
                                             OsinfoOs *os,
                                             OsinfoPlatform *platform,
                                             GVirConfigCapabilities *caps)
{
    return GVIR_DESIGNER_DOMAIN(g_object_new(GVIR_DESIGNER_TYPE_DOMAIN,
                                             "osinfo-db", db,
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
    gchar *arch_native;

    if (arch) {
        arch_native = gvir_designer_domain_get_arch_normalized(arch);
    } else {
        struct utsname ut;
        uname(&ut);
        arch_native = gvir_designer_domain_get_arch_normalized(ut.machine);
    }
    g_object_unref(G_OBJECT(cpu));
    g_object_unref(G_OBJECT(host));

    return arch_native;
}


static GVirConfigCapabilitiesGuest *
gvir_designer_domain_get_guest(GVirDesignerDomain *design,
                               const gchar *wantarch)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GList *guests = gvir_config_capabilities_get_guests(priv->caps);
    GList *tmp = guests;
    GVirConfigCapabilitiesGuest *ret = NULL;

    while (tmp && !ret) {
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
        }

        g_object_unref(G_OBJECT(arch));

        tmp = tmp->next;
    }

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
    GVirConfigCapabilitiesGuestDomain *domain = NULL;
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
    if (domain != NULL)
        g_object_unref(domain);
    g_object_unref(arch);
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


static OsinfoDeviceLink *
gvir_designer_domain_get_preferred_device(GVirDesignerDomain *design,
                                          const char *class,
                                          GError **error)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    OsinfoFilter *filter = osinfo_filter_new();
    OsinfoDeviceLinkFilter *filter_link = NULL;
    OsinfoDeployment *deployment = priv->deployment;
    OsinfoDeviceLink *dev_link = NULL;

    if (!deployment) {
        if (!priv->osinfo_db) {
            g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                        "Unable to find any deployment in libosinfo database");
            goto cleanup;
        }
        priv->deployment = deployment = osinfo_db_find_deployment(priv->osinfo_db,
                                                                  priv->os,
                                                                  priv->platform);
        if (!deployment) {
            g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                        "Unable to find any deployment in libosinfo database");
            goto cleanup;
        }
    }

    osinfo_filter_add_constraint(filter, "class", class);
    filter_link = osinfo_devicelinkfilter_new(filter);
    dev_link = osinfo_deployment_get_preferred_device_link(deployment, OSINFO_FILTER(filter_link));

cleanup:
    if (filter_link)
        g_object_unref(filter_link);
    if (filter)
        g_object_unref(filter);
    return dev_link;
}


static gchar *
gvir_designer_domain_next_disk_target(GVirDesignerDomain *design,
                                      GVirConfigDomainDiskBus bus)
{
    gchar *ret = NULL;
    GVirDesignerDomainPrivate *priv = design->priv;

    switch (bus) {
    case GVIR_CONFIG_DOMAIN_DISK_BUS_IDE:
        ret = g_strdup_printf("hd%c", 'a' + priv->ide++);
        break;
    case GVIR_CONFIG_DOMAIN_DISK_BUS_VIRTIO:
        ret = g_strdup_printf("vd%c", 'a' + priv->virtio++);
        break;
    case GVIR_CONFIG_DOMAIN_DISK_BUS_SATA:
        ret = g_strdup_printf("sd%c", 'a' + priv->sata++);
        break;
    case GVIR_CONFIG_DOMAIN_DISK_BUS_FDC:
    case GVIR_CONFIG_DOMAIN_DISK_BUS_SCSI:
    case GVIR_CONFIG_DOMAIN_DISK_BUS_XEN:
    case GVIR_CONFIG_DOMAIN_DISK_BUS_USB:
    case GVIR_CONFIG_DOMAIN_DISK_BUS_UML:
    default:
        /* not supported yet */
        /* XXX should we fallback to ide/virtio? */
        break;
    }

    return ret;
}

static OsinfoDevice *
gvir_designer_domain_get_preferred_disk_controller(GVirDesignerDomain *design,
                                                   GError **error)
{
    OsinfoDevice *dev = NULL;
    OsinfoDeviceLink *dev_link = gvir_designer_domain_get_preferred_device(design,
                                                                           "block",
                                                                           error);
    if (dev_link == NULL)
        goto cleanup;

    dev = osinfo_devicelink_get_target(dev_link);

cleanup:
    if (dev_link != NULL)
        g_object_unref(dev_link);
    return dev;
}


static OsinfoDevice *
gvir_designer_domain_get_fallback_disk_controller(GVirDesignerDomain *design,
                                                  GError **error)
{
    OsinfoEntity *dev = NULL;
    OsinfoDeviceList *devices;
    OsinfoFilter *filter;
    int virt_type;

    filter = osinfo_filter_new();
    osinfo_filter_add_constraint(filter, OSINFO_DEVICE_PROP_CLASS, "block");
    devices = gvir_designer_domain_get_supported_devices(design, filter);
    g_object_unref(G_OBJECT(filter));

    if (devices == NULL ||
        osinfo_list_get_length(OSINFO_LIST(devices)) == 0) {
        goto cleanup;
    }

    virt_type = gvir_config_domain_get_virt_type(design->priv->config);
    if (virt_type == GVIR_CONFIG_DOMAIN_VIRT_QEMU ||
        virt_type == GVIR_CONFIG_DOMAIN_VIRT_KQEMU ||
        virt_type == GVIR_CONFIG_DOMAIN_VIRT_KVM) {
        /* If using QEMU; we favour using virtio-block */
        OsinfoList *tmp_devices;
        filter = osinfo_filter_new();
        osinfo_filter_add_constraint(filter,
                                     OSINFO_ENTITY_PROP_ID,
                                     GVIR_DESIGNER_VIRTIO_BLOCK_DEVICE_ID);
        tmp_devices = osinfo_list_new_filtered(OSINFO_LIST(devices), filter);
        if (tmp_devices != NULL &&
            osinfo_list_get_length(OSINFO_LIST(tmp_devices)) > 0) {
            g_object_unref(devices);
            devices = OSINFO_DEVICELIST(tmp_devices);
        }
        g_object_unref(G_OBJECT(filter));
    }

    dev = osinfo_list_get_nth(OSINFO_LIST(devices), 0);
    g_object_ref(G_OBJECT(dev));
    g_object_unref(G_OBJECT(devices));

cleanup:
    return OSINFO_DEVICE(dev);
}

static GVirConfigDomainDiskBus
gvir_designer_domain_get_bus_type_from_controller(GVirDesignerDomain *design,
                                                  OsinfoDevice *controller)
{
    const char *id;

    id = osinfo_entity_get_id(OSINFO_ENTITY(controller));
    if (g_str_equal(id, GVIR_DESIGNER_VIRTIO_BLOCK_DEVICE_ID) == 0)
        return GVIR_CONFIG_DOMAIN_DISK_BUS_VIRTIO;

    return GVIR_CONFIG_DOMAIN_DISK_BUS_IDE;
}

static GVirConfigDomainDisk *
gvir_designer_domain_add_disk_full(GVirDesignerDomain *design,
                                   GVirConfigDomainDiskType type,
                                   GVirConfigDomainDiskGuestDeviceType guest_type,
                                   const char *path,
                                   const char *format,
                                   gchar *target,
                                   GError **error)
{
    GVirDesignerDomainPrivate *priv = design->priv;
    GVirConfigDomainDisk *disk = NULL;
    GVirConfigDomainDiskBus bus;
    gchar *target_gen = NULL;
    const char *driver_name;
    int virt_type;

    OsinfoDevice *controller;

    virt_type = gvir_config_domain_get_virt_type(priv->config);
    switch (virt_type) {
    case GVIR_CONFIG_DOMAIN_VIRT_QEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KQEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KVM:
        driver_name = "qemu";
        break;
        /* add new virt type here */
    default:
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unsupported virt type %d", virt_type);
        goto error;
        break;
    }

    disk = gvir_config_domain_disk_new();
    gvir_config_domain_disk_set_type(disk, type);
    gvir_config_domain_disk_set_source(disk, path);
    gvir_config_domain_disk_set_driver_name(disk, driver_name);
    if (format)
        gvir_config_domain_disk_set_driver_type(disk, format);

    controller = gvir_designer_domain_get_preferred_disk_controller(design, NULL);
    if (controller == NULL)
        controller = gvir_designer_domain_get_fallback_disk_controller(design, NULL);

    if (controller != NULL) {
        bus = gvir_designer_domain_get_bus_type_from_controller(design, controller);
    } else {
        bus = GVIR_CONFIG_DOMAIN_DISK_BUS_IDE;
    }
    gvir_config_domain_disk_set_target_bus(disk, bus);

    if (!target) {
        target = target_gen = gvir_designer_domain_next_disk_target(design, bus);
        if (!target_gen) {
            g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                        "unable to generate target name for bus '%d'", bus);
            goto error;
        }
    }
    gvir_config_domain_disk_set_target_dev(disk, target);

    g_free(target_gen);

    gvir_config_domain_add_device(priv->config, GVIR_CONFIG_DOMAIN_DEVICE(disk));

    return disk;

error:
    g_free(target_gen);
    if (disk)
        g_object_unref(disk);
    return NULL;
}


/**
 * gvir_designer_domain_add_disk_file:
 * @design: (transfer none): the domain designer instance
 * @filepath: (transfer none): the path to a file
 * @format: (transfer none): disk format
 * @error: return location for a #GError, or NULL
 *
 * Add a new disk to the domain.
 *
 * Returns: (transfer full): the pointer to new disk.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_disk_file(GVirDesignerDomain *design,
                                                         const char *filepath,
                                                         const char *format,
                                                         GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_FILE,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_DISK,
                                             filepath,
                                             format,
                                             NULL,
                                             error);
    return ret;
}


/**
 * gvir_designer_domain_add_disk_device:
 * @design: (transfer none): the domain designer instance
 * @devpath: (transfer none): path to the device
 * @error: return location for a #GError, or NULL
 *
 * Add given device as a new disk to the domain designer instance.
 *
 * Returns: (transfer full): the pointer to the new disk.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_disk_device(GVirDesignerDomain *design,
                                                           const char *devpath,
                                                           GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_BLOCK,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_DISK,
                                             devpath,
                                             "raw",
                                             NULL,
                                             error);
    return ret;
}

/**
 * gvir_designer_domain_add_cdrom_file:
 * @design: (transfer none): the domain designer instance
 * @filepath: (transfer none): the path to a file
 * @format: (transfer none): file format
 * @error: return location for a #GError, or NULL
 *
 * Add a new disk to the domain.
 *
 * Returns: (transfer full): the pointer to new cdrom.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_cdrom_file(GVirDesignerDomain *design,
                                                          const char *filepath,
                                                          const char *format,
                                                          GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_FILE,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_CDROM,
                                             filepath,
                                             format,
                                             NULL,
                                             error);
    return ret;
}


/**
 * gvir_designer_domain_add_cdrom_device:
 * @design: (transfer none): the domain designer instance
 * @devpath: (transfer none): path to the device
 * @error: return location for a #GError, or NULL
 *
 * Add given device as a new cdrom to the domain designer instance.
 *
 * Returns: (transfer full): the pointer to the new cdrom.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_cdrom_device(GVirDesignerDomain *design,
                                                            const char *devpath,
                                                            GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_BLOCK,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_CDROM,
                                             devpath,
                                             "raw",
                                             NULL,
                                             error);
    return ret;
}


/**
 * gvir_designer_domain_add_floppy_file:
 * @design: (transfer none): the domain designer instance
 * @filepath: (transfer none): the path to a file
 * @format: (transfer none): file format
 * @error: return location for a #GError, or NULL
 *
 * Add a new disk to the domain.
 *
 * Returns: (transfer full): the pointer to new floppy.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_floppy_file(GVirDesignerDomain *design,
                                                           const char *filepath,
                                                           const char *format,
                                                           GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_FILE,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_FLOPPY,
                                             filepath,
                                             format,
                                             NULL,
                                             error);
    return ret;
}


/**
 * gvir_designer_domain_add_floppy_device:
 * @design: (transfer none): the domain designer instance
 * @devpath: (transfer none): path to the device
 * @error: return location for a #GError, or NULL
 *
 * Add given device as a new floppy to the domain designer instance.
 *
 * Returns: (transfer full): the pointer to the new floppy.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainDisk *gvir_designer_domain_add_floppy_device(GVirDesignerDomain *design,
                                                             const char *devpath,
                                                             GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainDisk *ret = NULL;

    ret = gvir_designer_domain_add_disk_full(design,
                                             GVIR_CONFIG_DOMAIN_DISK_BLOCK,
                                             GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_FLOPPY,
                                             devpath,
                                             "raw",
                                             NULL,
                                             error);
    return ret;
}



static const gchar *
gvir_designer_domain_get_preferred_nic_model(GVirDesignerDomain *design,
                                             GError **error)
{
    const gchar *ret = NULL;
    OsinfoDeviceLink *dev_link = NULL;

    dev_link = gvir_designer_domain_get_preferred_device(design, "network", error);
    if (!dev_link)
        goto cleanup;

    ret = osinfo_devicelink_get_driver(dev_link);

cleanup:
    if (dev_link)
        g_object_unref(dev_link);
    return ret;
}


static GVirConfigDomainInterface *
gvir_designer_domain_add_interface_full(GVirDesignerDomain *design,
                                        GVirDesignerDomainNICType type,
                                        const char *network,
                                        GError **error)
{
    GVirConfigDomainInterface *ret;
    const gchar *model = NULL;

    model = gvir_designer_domain_get_preferred_nic_model(design, error);

    switch (type) {
    case GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK:
        ret = GVIR_CONFIG_DOMAIN_INTERFACE(gvir_config_domain_interface_network_new());
        gvir_config_domain_interface_network_set_source(GVIR_CONFIG_DOMAIN_INTERFACE_NETWORK(ret),
                                                        network);
        break;
    default:
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unsupported interface type '%d'", type);
        goto cleanup;
    }

    if (model)
        gvir_config_domain_interface_set_model(ret, model);

    gvir_config_domain_add_device(design->priv->config, GVIR_CONFIG_DOMAIN_DEVICE(ret));

cleanup:
    return ret;
}


/**
 * gvir_designer_domain_add_interface_network:
 * @design: (transfer none): the domain designer instance
 * @network: (transfer none): network name
 * @error: return location for a #GError, or NULL
 *
 * Add new network interface card into @design. The interface is
 * of 'network' type with @network used as the source network.
 *
 * Returns: (transfer full): the pointer to the new interface.
 */
GVirConfigDomainInterface *
gvir_designer_domain_add_interface_network(GVirDesignerDomain *design,
                                           const char *network,
                                           GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    GVirConfigDomainInterface *ret = NULL;

    ret = gvir_designer_domain_add_interface_full(design,
                                                  GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK,
                                                  network,
                                                  error);

    return ret;
}


static void
gvir_designer_domain_get_resources(OsinfoResourcesList *res_list,
                                   const gchar *design_arch,
                                   gint *design_n_cpus,
                                   gint64 *design_ram)
{
    GList *elem_list, *elem_iterator;

    if (!res_list)
        return;

    elem_list = osinfo_list_get_elements(OSINFO_LIST(res_list));
    for (elem_iterator = elem_list; elem_iterator; elem_iterator = elem_iterator->next) {
        OsinfoResources *res = OSINFO_RESOURCES(elem_iterator->data);
        const char *arch = osinfo_resources_get_architecture(res);
        gint n_cpus = -1;
        gint64 ram = -1;

        if (g_str_equal(arch, "all") ||
            g_str_equal(arch, design_arch)) {
            n_cpus = osinfo_resources_get_n_cpus(res);
            ram = osinfo_resources_get_ram(res);
            if (n_cpus > 0) {
                *design_n_cpus = n_cpus;
            }
            if (ram > 0) {
                /* libosinfo returns RAM in B, libvirt-gconfig takes it in KB */
                *design_ram = ram / 1024;
            }
            break;
        }
    }
    g_list_free(elem_list);
}


/**
 * gvir_designer_domain_setup_resources:
 * @design: (transfer none): the domain designer instance
 * @req: (transfer none): requirements to set
 * @error: return location for a #GError, or NULL
 *
 * Set minimal or recommended resources on @design.
 *
 * Returns: (transfer none): TRUE when successfully set, FALSE otherwise.
 */
gboolean gvir_designer_domain_setup_resources(GVirDesignerDomain *design,
                                              GVirDesignerDomainResources req,
                                              GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);
    gboolean ret = FALSE;
    OsinfoResourcesList *res_list_min = NULL, *res_list_rec = NULL;
    GVirConfigDomainOs *os = gvir_config_domain_get_os(design->priv->config);
    const gchar *design_arch = os ? gvir_config_domain_os_get_arch(os) : "";
    gint n_cpus = -1;
    gint64 ram = -1;

    /* If user request recommended settings those may just override
     * only a few settings when compared to minimal. So we must implement
     * inheritance here. */
    res_list_min = osinfo_os_get_minimum_resources(design->priv->os);
    res_list_rec = osinfo_os_get_recommended_resources(design->priv->os);
    if (req ==GVIR_DESIGNER_DOMAIN_RESOURCES_MINIMAL) {
        gvir_designer_domain_get_resources(res_list_min, design_arch,
                                           &n_cpus, &ram);
    } else if (req == GVIR_DESIGNER_DOMAIN_RESOURCES_RECOMMENDED) {
        gvir_designer_domain_get_resources(res_list_min, design_arch,
                                           &n_cpus, &ram);
        gvir_designer_domain_get_resources(res_list_rec, design_arch,
                                           &n_cpus, &ram);
    } else {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unknown resources argument: '%d'", req);
        goto cleanup;
    }

    if ((n_cpus <= 0) && (ram <= 0)) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find resources in libosinfo database");
        goto cleanup;
    }

    if (n_cpus > 0)
        gvir_config_domain_set_vcpus(design->priv->config, n_cpus);
    if (ram > 0)
        gvir_config_domain_set_memory(design->priv->config, ram);

cleanup:
    if (res_list_min != NULL)
        g_object_unref(G_OBJECT(res_list_min));
    if (res_list_rec != NULL)
        g_object_unref(G_OBJECT(res_list_rec));
    g_object_unref(G_OBJECT(os));

    return ret;
}

/**
 * gvir_designer_domain_add_driver:
 * @design: the domain designer instance
 * @driver_id: OsInfo id of the driver to Add
 * @error: return location for a #GError, or NULL
 *
 * Instructs libvirt-designer to assume that the driver identified by
 * @driver_id is installed in the guest OS. This means that @design
 * can use the device associated to @driver_id if needed.
 *
 * Returns: (transfer none): TRUE when successfully set, FALSE otherwise.
 */
gboolean gvir_designer_domain_add_driver(GVirDesignerDomain *design,
                                         const char *driver_id,
                                         GError **error)
{
    OsinfoEntity *driver;
    OsinfoDeviceDriverList *drivers;
    gboolean driver_added = FALSE;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);
    g_return_val_if_fail(driver_id != NULL, FALSE);
    g_return_val_if_fail(!error_is_set(error), FALSE);

    if (design->priv->os == NULL) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0, "Unknown OS");
        goto end;
    }

    drivers = osinfo_os_get_device_drivers(design->priv->os);
    driver = osinfo_list_find_by_id(OSINFO_LIST(drivers), driver_id);
    g_return_val_if_fail(OSINFO_IS_DEVICE_DRIVER(driver), FALSE);
    if (driver == NULL) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unable to find driver %s in OS %s", driver_id,
                    osinfo_entity_get_id(OSINFO_ENTITY(design->priv->os)));
        goto end;
    }

    osinfo_list_add(OSINFO_LIST(design->priv->drivers), driver);
    driver_added = TRUE;

end:
    return driver_added;
}


/**
 * gvir_designer_domain_remove_all_drivers:
 * @design: the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Removes all drivers used in @design.
 *
 * Returns: (transfer none): TRUE when successfully set, FALSE otherwise.
 * @see_also gvir_designer_domain_add_driver()
 */
gboolean gvir_designer_domain_remove_all_drivers(GVirDesignerDomain *design,
                                                 GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), FALSE);
    g_return_val_if_fail(!error_is_set(error), FALSE);

    g_object_unref(design->priv->drivers);
    design->priv->drivers = NULL;

    return TRUE;
}
