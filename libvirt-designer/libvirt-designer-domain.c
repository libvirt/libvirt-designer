/*
 * libvirt-designer-domain.c: libvirt domain configuration
 *
 * Copyright (C) 2012-2013 Red Hat, Inc.
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
 *   Christophe Fergeau <cfergeau@redhat.com>
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
    GVIR_DESIGNER_DOMAIN_NIC_TYPE_BRIDGE,
    GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK,
    GVIR_DESIGNER_DOMAIN_NIC_TYPE_USER,
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

static const char GVIR_DESIGNER_SPICE_CHANNEL_NAME[] = "com.redhat.spice.0";
static const char GVIR_DESIGNER_SPICE_CHANNEL_DEVICE_ID[] = "http://pciids.sourceforge.net/v2.2/pci.ids/1af4/1003";
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


static GList *
gvir_designer_domain_get_device_by_type(GVirDesignerDomain *design, GType type)
{
    GList *devices;
    GList *it;
    GList *matched_devices = NULL;

    devices = gvir_config_domain_get_devices(design->priv->config);
    for (it = devices; it != NULL; it = it->next) {
        GType device_type = G_OBJECT_TYPE(it->data);

        if (g_type_is_a(device_type, type)) {
            matched_devices = g_list_prepend(matched_devices,
                                             g_object_ref(G_OBJECT(it->data)));
        }

    }
    g_list_free_full(devices, g_object_unref);

    return g_list_reverse(matched_devices);
}


static void gvir_designer_domain_add_clock(GVirDesignerDomain *design)
{
    GVirConfigDomainClock *clock;
    GVirConfigDomainTimer *timer;
    GVirConfigDomainClockOffset offset;

    clock = gvir_config_domain_clock_new();
    offset = GVIR_CONFIG_DOMAIN_CLOCK_UTC;
    if (design->priv->os != NULL) {
        const gchar *short_id;

        short_id = osinfo_product_get_short_id(OSINFO_PRODUCT(design->priv->os));
        if (short_id != NULL && g_str_has_suffix(short_id, "win")) {
            offset = GVIR_CONFIG_DOMAIN_CLOCK_LOCALTIME;
        }
    }
    gvir_config_domain_clock_set_offset(clock, offset);

    timer = GVIR_CONFIG_DOMAIN_TIMER(gvir_config_domain_timer_rtc_new());
    gvir_config_domain_timer_set_tick_policy(timer,
                                             GVIR_CONFIG_DOMAIN_TIMER_TICK_POLICY_CATCHUP);
    gvir_config_domain_clock_add_timer(clock, timer);
    g_object_unref(G_OBJECT(timer));

    timer = GVIR_CONFIG_DOMAIN_TIMER(gvir_config_domain_timer_pit_new());
    gvir_config_domain_timer_set_tick_policy(timer,
                                             GVIR_CONFIG_DOMAIN_TIMER_TICK_POLICY_DELAY);
    gvir_config_domain_clock_add_timer(clock, timer);
    g_object_unref(G_OBJECT(timer));

    gvir_config_domain_set_clock(design->priv->config, clock);
    g_object_unref(G_OBJECT(clock));
}


static gboolean
gvir_designer_domain_has_spice_channel(GVirDesignerDomain *design)
{
    GList *devices;
    GList *it;
    gboolean has_spice = FALSE;

    devices = gvir_designer_domain_get_device_by_type(design,
                                                      GVIR_CONFIG_TYPE_DOMAIN_CHANNEL);
    for (it = devices; it != NULL; it = it->next) {
        GVirConfigDomainChannel *channel;
        const char *target_name;
        channel = GVIR_CONFIG_DOMAIN_CHANNEL(it->data);
        target_name = gvir_config_domain_channel_get_target_name(channel);
        if (g_strcmp0(target_name, GVIR_DESIGNER_SPICE_CHANNEL_NAME) == 0) {
            /* FIXME could do more sanity checks (check if the channel
             * source has the 'spicevmc' type)
             */
            GVirConfigDomainChannelTargetType target_type;
            target_type = gvir_config_domain_channel_get_target_type(channel);
            if (target_type == GVIR_CONFIG_DOMAIN_CHANNEL_TARGET_VIRTIO) {
                has_spice = TRUE;
            } else {
                g_critical("Inconsistent SPICE channel, target type is wrong (%d)",
                           target_type);
                has_spice = FALSE;
            }

            break;
        }
    }
    g_list_free_full(devices, g_object_unref);

    return has_spice;
}


static gboolean
gvir_designer_domain_supports_spice_channel(GVirDesignerDomain *design)
{
    OsinfoDeviceList *devices;
    OsinfoFilter *filter;
    gboolean vioserial_found = FALSE;

    filter = osinfo_filter_new();
    osinfo_filter_add_constraint(filter,
                                 OSINFO_ENTITY_PROP_ID,
                                 GVIR_DESIGNER_SPICE_CHANNEL_DEVICE_ID);
    devices = gvir_designer_domain_get_supported_devices(design, filter);
    if (devices) {
        /* We only expect 0 or 1 virtio serial devices in that device list,
         * so warn if we get more than 1
         */
        g_warn_if_fail(osinfo_list_get_length(OSINFO_LIST(devices)) <= 1);
        if (osinfo_list_get_length(OSINFO_LIST(devices)) >= 1)
            vioserial_found = TRUE;
        g_object_unref(G_OBJECT(devices));
    }
    if (filter)
        g_object_unref(G_OBJECT(filter));

    return vioserial_found;
}


static gboolean gvir_designer_domain_add_spice_channel(GVirDesignerDomain *design,
                                                       GError **error)
{
    GVirConfigDomainChannel *channel;
    GVirConfigDomainChardevSourceSpiceVmc *vmc;

    if (!gvir_designer_domain_supports_spice_channel(design)) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "OS and/or hypervisor don't support virtio-serial"
                    " which is required by the SPICE channel");
        g_debug("SPICE channel not supported");
        return FALSE;
    }

    channel = gvir_config_domain_channel_new();
    gvir_config_domain_channel_set_target_type(channel,
                                               GVIR_CONFIG_DOMAIN_CHANNEL_TARGET_VIRTIO);
    gvir_config_domain_channel_set_target_name(channel,
                                               GVIR_DESIGNER_SPICE_CHANNEL_NAME);
    vmc = gvir_config_domain_chardev_source_spicevmc_new();
    gvir_config_domain_chardev_set_source(GVIR_CONFIG_DOMAIN_CHARDEV(channel),
                                          GVIR_CONFIG_DOMAIN_CHARDEV_SOURCE(vmc));
    g_object_unref(G_OBJECT(vmc));

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(channel));
    g_object_unref(G_OBJECT(channel));

    return TRUE;
}


static GVirConfigDomainGraphics *
gvir_designer_domain_create_graphics_desktop(GVirDesignerDomain *design,
                                             GError **error)
{
    int virt_type;

    virt_type = gvir_config_domain_get_virt_type(design->priv->config);

    switch (virt_type) {
    case GVIR_CONFIG_DOMAIN_VIRT_QEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KQEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KVM: {
        GVirConfigDomainGraphicsSdl *sdl;
        sdl = gvir_config_domain_graphics_sdl_new();
        return GVIR_CONFIG_DOMAIN_GRAPHICS(sdl);
    }
    case GVIR_CONFIG_DOMAIN_VIRT_VBOX: {
        GVirConfigDomainGraphicsDesktop *desktop;
        desktop = gvir_config_domain_graphics_desktop_new();
        return GVIR_CONFIG_DOMAIN_GRAPHICS(desktop);
    }
    default:
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Virt type %d does not support this graphics output",
                    virt_type);
        return NULL;
    }
}

/**
 * gvir_designer_domain_add_graphics:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add a new graphical framebuffer to @design. This allows
 * to see what the VM displays.
 * Remote display protocols will only be listening on localhost, and the
 * port will be automatically allocated when the VM starts (usually
 * starting at 5900). You can manipulate further the returned
 * #GVirConfigDomainGraphics if you want a different behaviour.
 * When setting up a SPICE display, the SPICE agent channel will be
 * automatically added to the VM if it's supported and not already
 * present.
 *
 * Returns: (transfer full): the pointer to the new graphical framebuffer
 * configuration object.
 */
GVirConfigDomainGraphics *
gvir_designer_domain_add_graphics(GVirDesignerDomain *design,
                                  GVirDesignerDomainGraphics type,
                                  GError **error)
{
    GVirConfigDomainGraphics *graphics;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    switch (type) {
    case GVIR_DESIGNER_DOMAIN_GRAPHICS_DESKTOP: {
        graphics = gvir_designer_domain_create_graphics_desktop(design, error);
        if (graphics == NULL)
            return NULL;
    }

    case GVIR_DESIGNER_DOMAIN_GRAPHICS_RDP: {
        GVirConfigDomainGraphicsRdp *rdp;

        rdp = gvir_config_domain_graphics_rdp_new();
        gvir_config_domain_graphics_rdp_set_autoport(rdp, TRUE);
        graphics = GVIR_CONFIG_DOMAIN_GRAPHICS(rdp);

        break;
    }

    case GVIR_DESIGNER_DOMAIN_GRAPHICS_SPICE: {
        GVirConfigDomainGraphicsSpice *spice;

        spice = gvir_config_domain_graphics_spice_new();
        gvir_config_domain_graphics_spice_set_autoport(spice, TRUE);
        /* FIXME: Should only be done for local domains */
        gvir_config_domain_graphics_spice_set_image_compression(spice,
                                                                GVIR_CONFIG_DOMAIN_GRAPHICS_SPICE_IMAGE_COMPRESSION_OFF);
        graphics = GVIR_CONFIG_DOMAIN_GRAPHICS(spice);
        if (!gvir_designer_domain_has_spice_channel(design))
            gvir_designer_domain_add_spice_channel(design, error);
        if (error_is_set(error)) {
            g_object_unref(graphics);
            return NULL;
        }

        break;
    }

    case GVIR_DESIGNER_DOMAIN_GRAPHICS_VNC: {
        GVirConfigDomainGraphicsVnc *vnc;

        vnc = gvir_config_domain_graphics_vnc_new();
        gvir_config_domain_graphics_vnc_set_autoport(vnc, TRUE);
        graphics = GVIR_CONFIG_DOMAIN_GRAPHICS(vnc);

        break;
    }

    default:
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "Unknown graphics type: %d", type);
        g_return_val_if_reached(NULL);
    }

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(graphics));

    return graphics;
}


static gboolean
gvir_designer_domain_supports_usb(GVirDesignerDomain *design)
{
    GList *devices;
    devices = gvir_designer_domain_get_device_by_type(design,
                                                      GVIR_CONFIG_TYPE_DOMAIN_CONTROLLER_USB);
    g_list_free_full(devices, g_object_unref);

    return (devices != NULL);
}


static GVirConfigDomainControllerUsb *
gvir_designer_domain_create_usb_controller(GVirDesignerDomain *design,
                                           GVirConfigDomainControllerUsbModel model,
                                           guint indx,
                                           GVirConfigDomainControllerUsb *master,
                                           guint start_port)
{
    GVirConfigDomainControllerUsb *controller;

    controller = gvir_config_domain_controller_usb_new();
    gvir_config_domain_controller_usb_set_model(controller, model);
    gvir_config_domain_controller_set_index(GVIR_CONFIG_DOMAIN_CONTROLLER(controller), indx);
    if (master)
        gvir_config_domain_controller_usb_set_master(controller, master, start_port);

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(controller));

    return controller;
}


static void
gvir_designer_domain_add_usb_controllers(GVirDesignerDomain *design)
{
    GVirConfigDomainControllerUsb *master;
    GVirConfigDomainControllerUsb *controller;

    g_debug("Adding USB controllers");

    master = gvir_designer_domain_create_usb_controller(design,
                                                        GVIR_CONFIG_DOMAIN_CONTROLLER_USB_MODEL_ICH9_EHCI1,
                                                        0,
                                                        NULL,
                                                        0);
    controller = gvir_designer_domain_create_usb_controller(design,
                                                            GVIR_CONFIG_DOMAIN_CONTROLLER_USB_MODEL_ICH9_UHCI1,
                                                            0,
                                                            master,
                                                            0);
    g_object_unref(G_OBJECT(controller));
    controller = gvir_designer_domain_create_usb_controller(design,
                                                            GVIR_CONFIG_DOMAIN_CONTROLLER_USB_MODEL_ICH9_UHCI2,
                                                            0,
                                                            master,
                                                            2);
    g_object_unref(G_OBJECT(controller));
    controller = gvir_designer_domain_create_usb_controller(design,
                                                            GVIR_CONFIG_DOMAIN_CONTROLLER_USB_MODEL_ICH9_UHCI3,
                                                            0,
                                                            master,
                                                            4);
    g_object_unref(G_OBJECT(controller));
    g_object_unref(G_OBJECT(master));
}


/**
 * gvir_designer_domain_add_usb_redir:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add a new usb redirection channel into @design. This allows to redirect
 * an USB device from the SPICE client to the guest. One USB device
 * can be redirected per redirection channel, this function can
 * be called multiple times if you need to redirect multiple devices
 * simultaneously. An USB2 EHCI controller and USB1 UHCI controllers
 * will be automatically added to @design if @design does not have
 * USB controllers yet.
 *
 * Returns: (transfer full): the pointer to the new USB redir channel
 */
GVirConfigDomainRedirdev *
gvir_designer_domain_add_usb_redir(GVirDesignerDomain *design, GError **error)
{
    /* FIXME: check if OS/hypervisor support USB
     *        check if SPICE is being used
     */
    GVirConfigDomainRedirdev *redirdev;
    GVirConfigDomainChardevSourceSpiceVmc *vmc;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    redirdev = gvir_config_domain_redirdev_new();
    gvir_config_domain_redirdev_set_bus(redirdev,
                                        GVIR_CONFIG_DOMAIN_REDIRDEV_BUS_USB);
    vmc = gvir_config_domain_chardev_source_spicevmc_new();
    gvir_config_domain_chardev_set_source(GVIR_CONFIG_DOMAIN_CHARDEV(redirdev),
                                          GVIR_CONFIG_DOMAIN_CHARDEV_SOURCE(vmc));
    g_object_unref(G_OBJECT(vmc));

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(redirdev));

    if (!gvir_designer_domain_supports_usb(design)) {
        gvir_designer_domain_add_usb_controllers(design);
    } else {
        g_debug("USB controllers are already present");
    }

    return redirdev;
}


/**
 * gvir_designer_domain_add_smartcard:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add a new virtual smartcard reader to @design. This will allow to
 * share a smartcard reader between the guest and the host.
 *
 * Returns: (transfer full): the pointer to the new smartcard device
 */
GVirConfigDomainSmartcard *
gvir_designer_domain_add_smartcard(GVirDesignerDomain *design, GError **error)
{
    /* FIXME: check if OS/hypervisor support smartcard, might need
     *        libosinfo improvements
     */
    GVirConfigDomainSmartcardPassthrough *smartcard;
    GVirConfigDomainChardevSourceSpiceVmc *vmc;
    GVirConfigDomainChardevSource *source;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    smartcard = gvir_config_domain_smartcard_passthrough_new();
    vmc = gvir_config_domain_chardev_source_spicevmc_new();
    source = GVIR_CONFIG_DOMAIN_CHARDEV_SOURCE(vmc);
    gvir_config_domain_smartcard_passthrough_set_source(smartcard, source);
    g_object_unref(G_OBJECT(vmc));

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(smartcard));

    return GVIR_CONFIG_DOMAIN_SMARTCARD(smartcard);
}


static void gvir_designer_domain_add_power_management(GVirDesignerDomain *design)
{
    GVirConfigDomainPowerManagement *pm;

    pm = gvir_config_domain_power_management_new();
    gvir_config_domain_power_management_set_mem_suspend_enabled(pm, FALSE);
    gvir_config_domain_power_management_set_disk_suspend_enabled(pm, FALSE);

    gvir_config_domain_set_power_management(design->priv->config, pm);
    g_object_unref(G_OBJECT(pm));
}

static void gvir_designer_domain_set_lifecycle(GVirDesignerDomain *design)
{
    gvir_config_domain_set_lifecycle(design->priv->config,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_ON_POWEROFF,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_DESTROY);
    gvir_config_domain_set_lifecycle(design->priv->config,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_ON_REBOOT,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_RESTART);
    gvir_config_domain_set_lifecycle(design->priv->config,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_ON_CRASH,
                                     GVIR_CONFIG_DOMAIN_LIFECYCLE_DESTROY);
}

static void gvir_designer_domain_add_console(GVirDesignerDomain *design)
{
    GVirConfigDomainConsole *console;
    GVirConfigDomainChardevSourcePty *pty;

    console = gvir_config_domain_console_new();
    pty = gvir_config_domain_chardev_source_pty_new();
    gvir_config_domain_chardev_set_source(GVIR_CONFIG_DOMAIN_CHARDEV(console),
                                          GVIR_CONFIG_DOMAIN_CHARDEV_SOURCE(pty));
    g_object_unref(G_OBJECT(pty));

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(console));
    g_object_unref(G_OBJECT(console));
}


static void gvir_designer_domain_add_input(GVirDesignerDomain *design)
{
    GVirConfigDomainInput *input;

    input = gvir_config_domain_input_new();
    gvir_config_domain_input_set_device_type(input,
                                             GVIR_CONFIG_DOMAIN_INPUT_DEVICE_TABLET);

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(input));
    g_object_unref(G_OBJECT(input));
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
            g_object_unref(G_OBJECT(arch));
            ret = g_object_ref(guest);
            goto cleanup;
        }

        g_object_unref(G_OBJECT(arch));
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

    gvir_designer_domain_add_clock(design);
    gvir_designer_domain_add_power_management(design);
    gvir_designer_domain_set_lifecycle(design);
    gvir_designer_domain_add_console(design);
    gvir_designer_domain_add_input(design);

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


static OsinfoDevice *
gvir_designer_domain_get_preferred_soundcard(GVirDesignerDomain *design,
                                             GError **error)
{
    OsinfoDevice *device = NULL;
    OsinfoDeviceLink *dev_link;

    dev_link = gvir_designer_domain_get_preferred_device(design,
                                                         "audio",
                                                         error);
    if (dev_link == NULL)
        goto cleanup;

    device = osinfo_devicelink_get_target(dev_link);

cleanup:
    if (dev_link != NULL)
        g_object_unref(dev_link);

    return device;
}

static OsinfoDeviceList *
gvir_designer_domain_get_fallback_devices(GVirDesignerDomain *design,
                                          const char *class,
                                          GError **error)
{
    OsinfoDeviceList *devices = NULL;
    OsinfoFilter *filter;

    filter = osinfo_filter_new();
    osinfo_filter_add_constraint(filter, OSINFO_DEVICE_PROP_CLASS, class);
    devices = gvir_designer_domain_get_supported_devices(design, filter);
    g_object_unref(G_OBJECT(filter));

    if (devices == NULL ||
        osinfo_list_get_length(OSINFO_LIST(devices)) == 0) {
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "No '%s' fallback devices found", class);
        goto cleanup;
    }

    return devices;

cleanup:
    if (devices != NULL)
        g_object_unref(devices);

    return NULL;
}


static OsinfoDevice *
gvir_designer_domain_get_fallback_soundcard(GVirDesignerDomain *domain,
                                            GError **error)
{
    OsinfoEntity *dev = NULL;
    OsinfoDeviceList *devices = NULL;

    devices = gvir_designer_domain_get_fallback_devices(domain, "audio", error);
    if (devices == NULL)
        goto cleanup;

    dev = osinfo_list_get_nth(OSINFO_LIST(devices), 0);
    g_object_ref(G_OBJECT(dev));

cleanup:
    if (devices != NULL)
        g_object_unref(G_OBJECT(devices));

    return OSINFO_DEVICE(dev);
}


static GVirConfigDomainSoundModel
gvir_designer_sound_model_from_soundcard(OsinfoDevice *soundcard)
{
    const char *name;

    name = osinfo_device_get_name(soundcard);
    if (g_strcmp0(name, "ac97") == 0) {
        return GVIR_CONFIG_DOMAIN_SOUND_MODEL_AC97;
    } else if (g_strcmp0(name, "ich6") == 0) {
        return GVIR_CONFIG_DOMAIN_SOUND_MODEL_ICH6;
    } else if (g_strcmp0(name, "es1370") == 0) {
        return GVIR_CONFIG_DOMAIN_SOUND_MODEL_ES1370;
    } else if (g_strcmp0(name, "sb16") == 0) {
        return GVIR_CONFIG_DOMAIN_SOUND_MODEL_SB16;
    } else {
        g_warning("Unknown soundcard %s, falling back to PC speaker", name);
        return GVIR_CONFIG_DOMAIN_SOUND_MODEL_PCSPK;
    }
}


/**
 * gvir_designer_domain_add_sound:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add a new soundcard to the domain.
 *
 * Returns: (transfer full): the pointer to the new soundcard.
 * If something fails NULL is returned and @error is set.
 */
GVirConfigDomainSound *
gvir_designer_domain_add_sound(GVirDesignerDomain *design, GError **error)
{
    GVirConfigDomainSound *sound;
    OsinfoDevice *soundcard;
    GVirConfigDomainSoundModel model;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);

    soundcard = gvir_designer_domain_get_preferred_soundcard(design, NULL);
    if (soundcard == NULL)
        soundcard = gvir_designer_domain_get_fallback_soundcard(design, error);

    if (soundcard == NULL)
        return NULL;

    sound = gvir_config_domain_sound_new();
    model = gvir_designer_sound_model_from_soundcard(soundcard);
    gvir_config_domain_sound_set_model(sound, model);

    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(sound));

    return sound;
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
    OsinfoDeviceList *devices = NULL;
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

cleanup:
    if (devices != NULL)
        g_object_unref(G_OBJECT(devices));

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
    GVirConfigDomainDiskDriver *driver = NULL;
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

    driver = gvir_config_domain_disk_driver_new();
    gvir_config_domain_disk_driver_set_name(driver, driver_name);
    if (format) {
        int fmt;

        fmt = gvir_designer_genum_get_value(GVIR_CONFIG_TYPE_DOMAIN_DISK_FORMAT,
                                            format, -1);

        if (fmt == -1) {
            g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                        "Unknown disk format: %s", format);
            goto error;
        }

        gvir_config_domain_disk_driver_set_format(driver, fmt);
    }

    disk = gvir_config_domain_disk_new();
    gvir_config_domain_disk_set_type(disk, type);
    gvir_config_domain_disk_set_source(disk, path);
    gvir_config_domain_disk_set_driver(disk, driver);
    g_object_unref(driver);
    driver = NULL;

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
    if (driver != NULL)
        g_object_unref(driver);
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
                                        const char *source,
                                        GError **error)
{
    GVirConfigDomainInterface *ret = NULL;
    const gchar *model = NULL;

    model = gvir_designer_domain_get_preferred_nic_model(design, error);

    switch (type) {
    case GVIR_DESIGNER_DOMAIN_NIC_TYPE_BRIDGE:
        ret = GVIR_CONFIG_DOMAIN_INTERFACE(gvir_config_domain_interface_bridge_new());
        gvir_config_domain_interface_bridge_set_source(GVIR_CONFIG_DOMAIN_INTERFACE_BRIDGE(ret),
                                                       source);
        break;
    case GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK:
        ret = GVIR_CONFIG_DOMAIN_INTERFACE(gvir_config_domain_interface_network_new());
        gvir_config_domain_interface_network_set_source(GVIR_CONFIG_DOMAIN_INTERFACE_NETWORK(ret),
                                                        source);
        break;
    case GVIR_DESIGNER_DOMAIN_NIC_TYPE_USER:
        ret = GVIR_CONFIG_DOMAIN_INTERFACE(gvir_config_domain_interface_user_new());
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
 * gvir_designer_domain_add_interface_bridge:
 * @design: (transfer none): the domain designer instance
 * @bridge: (transfer none): bridge name
 * @error: return location for a #GError, or NULL
 *
 * Add new network interface card into @design. The interface is
 * of 'bridge' type and uses @bridge as the bridge device
 *
 * Returns: (transfer full): the pointer to the new interface.
 */
GVirConfigDomainInterface *
gvir_designer_domain_add_interface_bridge(GVirDesignerDomain *design,
                                          const char *bridge,
                                          GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(bridge != NULL, NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    GVirConfigDomainInterface *ret = NULL;

    ret = gvir_designer_domain_add_interface_full(design,
                                                  GVIR_DESIGNER_DOMAIN_NIC_TYPE_BRIDGE,
                                                  bridge,
                                                  error);

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
    g_return_val_if_fail(network != NULL, NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    GVirConfigDomainInterface *ret = NULL;

    ret = gvir_designer_domain_add_interface_full(design,
                                                  GVIR_DESIGNER_DOMAIN_NIC_TYPE_NETWORK,
                                                  network,
                                                  error);

    return ret;
}

/**
 * gvir_designer_domain_add_interface_user:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add new network interface card into @design. The interface is
 * of 'user' type.
 *
 * Returns: (transfer full): the pointer to the new interface.
 */
GVirConfigDomainInterface *
gvir_designer_domain_add_interface_user(GVirDesignerDomain *design,
                                        GError **error)
{
    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    GVirConfigDomainInterface *ret = NULL;

    ret = gvir_designer_domain_add_interface_full(design,
                                                  GVIR_DESIGNER_DOMAIN_NIC_TYPE_USER,
                                                  NULL,
                                                  error);

    return ret;
}

static GVirConfigDomainVideoModel
gvir_designer_domain_video_model_str_to_enum(const char *model_str,
                                             GError **error)
{
    GVirConfigDomainVideoModel model;

    if (g_str_equal(model_str, "vga")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VGA;
    } else if (g_str_equal(model_str, "cirrus")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_CIRRUS;
    } else if (g_str_equal(model_str, "vmvga")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VMVGA;
    } else if (g_str_equal(model_str, "xen")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_XEN;
    } else if (g_str_equal(model_str, "vbox")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VBOX;
    } else if (g_str_equal(model_str, "qxl")) {
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_QXL;
    } else {
        g_debug("unsupported video model type '%s'", model_str);
        g_set_error(error, GVIR_DESIGNER_DOMAIN_ERROR, 0,
                    "unsupported video model type '%s'", model_str);
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VGA;
    }

    return model;
}

/* FIXME: belongs in platform descriptions, maybe useful as a last resort */
static GVirConfigDomainVideoModel
gvir_designer_domain_video_model_from_virt_type(GVirDesignerDomain *design)
{
    GVirConfigDomainVirtType virt_type;
    GVirConfigDomainVideoModel model;

    virt_type = gvir_config_domain_get_virt_type(design->priv->config);
    switch (virt_type) {
    case GVIR_CONFIG_DOMAIN_VIRT_QEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KQEMU:
    case GVIR_CONFIG_DOMAIN_VIRT_KVM:
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_QXL;
        break;
    case GVIR_CONFIG_DOMAIN_VIRT_XEN:
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_XEN;
        break;
    case GVIR_CONFIG_DOMAIN_VIRT_VMWARE:
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VMVGA;
        break;
    case GVIR_CONFIG_DOMAIN_VIRT_VBOX:
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VBOX;
        break;
    case GVIR_CONFIG_DOMAIN_VIRT_LXC:
    case GVIR_CONFIG_DOMAIN_VIRT_UML:
    case GVIR_CONFIG_DOMAIN_VIRT_OPENVZ:
    case GVIR_CONFIG_DOMAIN_VIRT_VSERVER:
    case GVIR_CONFIG_DOMAIN_VIRT_LDOM:
    case GVIR_CONFIG_DOMAIN_VIRT_TEST:
    case GVIR_CONFIG_DOMAIN_VIRT_HYPERV:
    case GVIR_CONFIG_DOMAIN_VIRT_ONE:
    case GVIR_CONFIG_DOMAIN_VIRT_PHYP:
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VGA;
        break;
    default:
        g_warn_if_reached();
        model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VGA;
        break;
    }

    return model;
}

static const gchar *
gvir_designer_domain_get_preferred_video_model(GVirDesignerDomain *design,
                                               GError **error)
{
    const gchar *ret = NULL;
    OsinfoDeviceLink *dev_link = NULL;
    OsinfoDevice *dev;

    dev_link = gvir_designer_domain_get_preferred_device(design, "video", error);
    if (!dev_link)
        goto cleanup;

    dev = osinfo_devicelink_get_target(dev_link);
    if (dev)
        ret = osinfo_device_get_name(dev);

cleanup:
    if (dev_link)
        g_object_unref(dev_link);
    return ret;
}

static GVirConfigDomainVideoModel
gvir_designer_domain_get_fallback_video_model(GVirDesignerDomain *design)
{
    OsinfoDeviceList *supported_devices = NULL;
    OsinfoFilter *filter = NULL;
    OsinfoDevice *device = NULL;
    const char *model_str;
    GVirConfigDomainVideoModel model;
    GError *error = NULL;

    model = GVIR_CONFIG_DOMAIN_VIDEO_MODEL_VGA;

    filter = osinfo_filter_new();
    osinfo_filter_add_constraint(filter, OSINFO_DEVICE_PROP_CLASS, "video");
    supported_devices = gvir_designer_domain_get_supported_devices(design, filter);
    if (supported_devices == NULL || osinfo_list_get_length(OSINFO_LIST(supported_devices)) == 0)
        goto fallback;

    device = OSINFO_DEVICE(osinfo_list_get_nth(OSINFO_LIST(supported_devices), 0));
    model_str = osinfo_device_get_name(device);
    model = gvir_designer_domain_video_model_str_to_enum(model_str,
                                                         &error);
    if (error != NULL) {
        g_clear_error(&error);
        goto fallback;
    }
    goto end;

fallback:
    model = gvir_designer_domain_video_model_from_virt_type(design);

end:
    if (filter != NULL)
        g_object_unref(G_OBJECT(filter));
    if (supported_devices != NULL)
        g_object_unref(G_OBJECT(supported_devices));

    return model;
}

/**
 * gvir_designer_domain_add_video:
 * @design: (transfer none): the domain designer instance
 * @error: return location for a #GError, or NULL
 *
 * Add a new video device into @design.
 *
 * Returns: (transfer full): the pointer to the new video device.
 */
GVirConfigDomainVideo *
gvir_designer_domain_add_video(GVirDesignerDomain *design, GError **error)
{
    GVirConfigDomainVideo *video;
    const gchar *model_str = NULL;
    GVirConfigDomainVideoModel model;

    g_return_val_if_fail(GVIR_DESIGNER_IS_DOMAIN(design), NULL);
    g_return_val_if_fail(!error_is_set(error), NULL);

    model_str = gvir_designer_domain_get_preferred_video_model(design, NULL);
    if (model_str != NULL) {
        model = gvir_designer_domain_video_model_str_to_enum(model_str, error);
        if (error_is_set(error)) {
            g_clear_error(error);
            model = gvir_designer_domain_get_fallback_video_model(design);
        }
    } else {
        model = gvir_designer_domain_get_fallback_video_model(design);
    }

    video = gvir_config_domain_video_new();
    gvir_config_domain_video_set_model(video, model);
    gvir_config_domain_add_device(design->priv->config,
                                  GVIR_CONFIG_DOMAIN_DEVICE(video));

    return video;
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
