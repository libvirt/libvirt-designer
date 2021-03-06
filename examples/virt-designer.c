/*
 * virt-designer.c: produce an domain XML
 *
 * Copyright (C) 2012, 2014 Red Hat, Inc.
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
 * License along with this library; If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Michal Privoznik <mprivozn@redhat.com>
 */

#include <config.h>
#include <libvirt-designer/libvirt-designer.h>
#include <libvirt-gobject/libvirt-gobject.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gprintf.h>

GList *cdrom_str_list = NULL;
GList *disk_str_list = NULL;
GList *floppy_str_list = NULL;
GList *iface_str_list = NULL;
OsinfoDb *db = NULL;

#define print_error(...) \
    print_error_impl(__FUNCTION__, __LINE__, __VA_ARGS__)

static void
print_error_impl(const char *funcname,
                 size_t linenr,
                 const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

static void
print_error_impl(const char *funcname,
                 size_t linenr,
                 const char *fmt, ...)
{
    va_list args;

    fprintf(stderr, "Error in %s:%zu ", funcname, linenr);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr,"\n");
}

static gboolean
load_osinfo(void)
{
    GError *err = NULL;
    gboolean ret = FALSE;
    OsinfoLoader *loader = NULL;

    loader = osinfo_loader_new();
    osinfo_loader_process_default_path(loader, &err);
    if (err) {
        print_error("Unable to load default libosinfo DB: %s", err->message);
        g_clear_error(&err);
    }

    db = osinfo_loader_get_db(loader);
    g_object_ref(db);
    ret = TRUE;

    g_object_unref(loader);
    return ret;
}

static gint
entity_compare(gconstpointer a, gconstpointer b)
{
    const gchar *id_a = osinfo_entity_get_id(OSINFO_ENTITY(a));
    const gchar *id_b = osinfo_entity_get_id(OSINFO_ENTITY(b));
    return g_strcmp0(id_a, id_b);
}

static gboolean
print_oses(const gchar *option_name,
           const gchar *value,
           gpointer data,
           GError **error)
{
    OsinfoOsList *list = NULL;
    GList *oses = NULL;
    GList *os_iter;
    int ret = EXIT_FAILURE;

    if (!db && !load_osinfo())
        goto cleanup;

    printf("  Operating System ID\n"
           "-----------------------\n");

    list = osinfo_db_get_os_list(db);
    if (!list)
        goto cleanup;
    oses = osinfo_list_get_elements(OSINFO_LIST(list));
    oses = g_list_sort(oses, entity_compare);
    for (os_iter = oses; os_iter; os_iter = os_iter->next) {
        OsinfoOs *os = OSINFO_OS(os_iter->data);
        const char *id = osinfo_entity_get_id(OSINFO_ENTITY(os));

        printf("%s\n", id);
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (list)
        g_object_unref(list);
    g_list_free(oses);

    exit(ret);
    return TRUE;
}

static gboolean
print_platforms(const gchar *option_name,
                const gchar *value,
                gpointer data,
                GError **error)
{
    OsinfoPlatformList *list = NULL;
    GList *platforms = NULL;
    GList *platform_iter;
    int ret = EXIT_FAILURE;

    if (!db && !load_osinfo())
        goto cleanup;

    printf("  Platform ID\n"
           "---------------\n");

    list = osinfo_db_get_platform_list(db);
    if (!list)
        goto cleanup;
    platforms = osinfo_list_get_elements(OSINFO_LIST(list));
    platforms = g_list_sort(platforms, entity_compare);
    for (platform_iter = platforms; platform_iter; platform_iter = platform_iter->next) {
        OsinfoPlatform *platform = OSINFO_PLATFORM(platform_iter->data);
        const char *id = osinfo_entity_get_id(OSINFO_ENTITY(platform));

        printf("%s\n", id);
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (list)
        g_object_unref(list);
    g_list_free(platforms);

    exit(ret);
    return TRUE;
}


static void
add_disk_generic(GVirDesignerDomain *domain,
                 const char *path,
                 GVirConfigDomainDiskGuestDeviceType type)
{
    GVirConfigDomainDisk *disk;
    char *format = NULL;
    struct stat buf;
    GError *error = NULL;
    gboolean is_device;

    format = strchr(path, ',');
    if (format) {
        *format = '\0';
        format++;
    }

    if (!path || !strlen(path)) {
        print_error("No path provided");
        exit(EXIT_FAILURE);
    }

    is_device = (!stat(path, &buf) && !S_ISREG(buf.st_mode));
    switch(type) {
        case GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_CDROM:
            if (is_device)
                disk = gvir_designer_domain_add_cdrom_device(domain, path, &error);
            else
                disk = gvir_designer_domain_add_cdrom_file(domain, path, format, &error);
            break;
        case GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_DISK:
            if (is_device)
                disk = gvir_designer_domain_add_disk_device(domain, path, &error);
            else
                disk = gvir_designer_domain_add_disk_file(domain, path, format, &error);
            break;
        case GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_FLOPPY:
            if (is_device)
                disk = gvir_designer_domain_add_floppy_device(domain, path, &error);
            else
                disk = gvir_designer_domain_add_floppy_file(domain, path, format, &error);
            break;
        default:
            g_return_if_reached();
    }

    if (disk)
        g_object_unref(G_OBJECT(disk));

    if (error) {
        print_error("%s", error->message);
        exit(EXIT_FAILURE);
    }
}


static void add_cdrom(gpointer data, gpointer user_data)
{
    add_disk_generic(GVIR_DESIGNER_DOMAIN(user_data),
                     (const char *)data,
                     GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_CDROM);
}


static void add_disk(gpointer data, gpointer user_data)
{
    add_disk_generic(GVIR_DESIGNER_DOMAIN(user_data),
                     (const char *)data,
                     GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_DISK);
}


static void add_floppy(gpointer data, gpointer user_data)
{
    add_disk_generic(GVIR_DESIGNER_DOMAIN(user_data),
                     (const char *)data,
                     GVIR_CONFIG_DOMAIN_DISK_GUEST_DEVICE_FLOPPY);
}


static gboolean
add_cdrom_str(const gchar *option_name,
              const gchar *value,
              gpointer data,
              GError **error)
{
    cdrom_str_list = g_list_append(cdrom_str_list, g_strdup(value));
    return TRUE;
}


static gboolean
add_disk_str(const gchar *option_name,
             const gchar *value,
             gpointer data,
             GError **error)
{
    disk_str_list = g_list_append(disk_str_list, g_strdup(value));
    return TRUE;
}


static gboolean
add_floppy_str(const gchar *option_name,
               const gchar *value,
               gpointer data,
               GError **error)
{
    floppy_str_list = g_list_append(floppy_str_list, g_strdup(value));
    return TRUE;
}


static void
add_iface(gpointer data,
          gpointer user_data)
{
    GVirDesignerDomain *domain = (GVirDesignerDomain *) user_data;
    char *network = (char *) data;
    char *param = NULL;
    GVirConfigDomainInterface *iface = NULL;
    GError *error = NULL;

    param = strchr(network, ',');
    if (param) {
        *param = '\0';
        param++;
    }

    iface = gvir_designer_domain_add_interface_network(domain, network, &error);
    if (error) {
        print_error("%s", error->message);
        exit(EXIT_FAILURE);
    }

    while (param && *param) {
        char *key = param;
        char *val;
        GVirConfigDomainInterfaceLinkState link;

        /* move to next token */
        param = strchr(param, ',');
        if (param) {
            *param = '\0';
            param++;
        }

        /* parse token */
        val = strchr(key, '=');
        if (!val) {
            print_error("Invalid format: %s", key);
            exit(EXIT_FAILURE);
        }

        *val = '\0';
        val++;

        if (g_str_equal(key, "mac")) {
            gvir_config_domain_interface_set_mac(iface, val);
        } else if (g_str_equal(key, "link")) {
            if (g_str_equal(val, "up")) {
                link = GVIR_CONFIG_DOMAIN_INTERFACE_LINK_STATE_UP;
            } else if (g_str_equal(val, "down")) {
                link = GVIR_CONFIG_DOMAIN_INTERFACE_LINK_STATE_DOWN;
            } else {
                print_error("Unknown value: %s", val);
                exit(EXIT_FAILURE);
            }
            gvir_config_domain_interface_set_link_state(iface, link);
        } else {
            print_error("Unknown key: %s", key);
            exit(EXIT_FAILURE);
        }
    }
    g_object_unref(iface);
}

static gboolean
add_iface_str(const gchar *option_name,
             const gchar *value,
             gpointer data,
             GError **error)
{
    iface_str_list = g_list_append(iface_str_list, g_strdup(value));
    return TRUE;
}

static OsinfoEntity *
find_entity_by_short_id(OsinfoList *ent_list,
                        const char *short_id)
{
    OsinfoEntity *ret = NULL;
    GList *list, *list_iterator;

    list = osinfo_list_get_elements(ent_list);
    for (list_iterator = list; list_iterator; list_iterator = list_iterator->next) {
        const char *id = osinfo_product_get_short_id(OSINFO_PRODUCT(list_iterator->data));

        if (!g_strcmp0(id, short_id)) {
            ret = list_iterator->data;
            g_object_ref(ret);
            break;
        }
    }
    g_list_free(list);

    return ret;
}

static OsinfoOs *
find_os(const gchar *os_str)
{
    OsinfoOs *ret = NULL;

    if (!db && !load_osinfo())
        return NULL;

    ret = osinfo_db_get_os(db, os_str);

    return ret;
}

static OsinfoOs *
find_os_by_short_id(const char *short_id)
{
    OsinfoOs *ret = NULL;
    OsinfoOsList *oses = NULL;
    OsinfoEntity *found = NULL;

    if (!db && !load_osinfo())
        return NULL;

    oses = osinfo_db_get_os_list(db);

    if (!oses)
        goto cleanup;

    found = find_entity_by_short_id(OSINFO_LIST(oses), short_id);

    if (!found)
        goto cleanup;
    ret = OSINFO_OS(found);

cleanup:
    if (oses)
        g_object_unref(oses);
    return ret;
}

static OsinfoOs *
guess_os_from_cdrom(GList *disk_list)
{
    OsinfoOs *ret = NULL;
    GList *list_it = NULL;

    if (!db && !load_osinfo())
        return NULL;

    for (list_it = disk_list; list_it; list_it = list_it->next) {
        char *path = (char *) list_it->data;
        char *sep = strchr(path, ',');
        OsinfoMedia *media = NULL;

        if (sep)
            path = g_strndup(path, sep-path);

        media = osinfo_media_create_from_location(path, NULL, NULL);

        if (sep)
            g_free(path);

        if (!media)
            continue;

        if (osinfo_db_identify_media(db, media)) {
            g_object_get(G_OBJECT(media), "os", &ret, NULL);
            break;
        }
    }

    return ret;
}

static OsinfoPlatform *
find_platform(const char *platform_str)
{
    OsinfoPlatform *ret = NULL;

    if (!db && !load_osinfo())
        return NULL;

    ret = osinfo_db_get_platform(db, platform_str);

    return ret;
}

static OsinfoPlatform *
find_platform_by_short_id(const char *short_id)
{
    OsinfoPlatform *ret = NULL;
    OsinfoPlatformList *platforms = NULL;
    OsinfoEntity *found = NULL;

    if (!db && !load_osinfo())
        goto cleanup;

    platforms = osinfo_db_get_platform_list(db);

    if (!platforms)
        goto cleanup;

    found = find_entity_by_short_id(OSINFO_LIST(platforms), short_id);

    if (!found)
        goto cleanup;

    ret = OSINFO_PLATFORM(found);

cleanup:
    if (platforms)
        g_object_unref(platforms);
    return ret;
}

static OsinfoPlatform *
guess_platform_from_connect(GVirConnection *conn)
{
    OsinfoPlatform *ret = NULL;
    gchar *hv_type = NULL;
    gulong ver, major, minor, release;
    char *short_id = NULL, *type = NULL;

    hv_type = gvir_connection_get_hypervisor_name(conn, NULL);
    ver = gvir_connection_get_version(conn, NULL);

    if (!hv_type || !ver) {
        print_error("Unable to get hypervisor and its version");
        exit(EXIT_FAILURE);
    }

    /* do some mappings:
     * QEMU -> qemu-kvm
     * Xen -> xen
     */
    type = g_ascii_strdown(hv_type, -1);
    if (g_str_equal(type, "qemu") && ver <= 1002000) {
        g_free(type);
        type = g_strdup("qemu-kvm");
    }

    major = ver / 1000000;
    ver %= 1000000;
    minor = ver / 1000;
    release = ver % 1000 ;

    short_id = g_strdup_printf("%s-%lu.%lu.%lu", type, major, minor, release);

    ret = find_platform_by_short_id(short_id);

    g_free(short_id);
    g_free(hv_type);
    g_free(type);
    return ret;
}

#define CHECK_ERROR \
    if (error) {                            \
        print_error("%s", error->message);  \
        goto cleanup;                       \
    }

int
main(int argc, char *argv[])
{
    int ret = EXIT_FAILURE;
    GError *error = NULL;
    OsinfoOs *os = NULL;
    OsinfoPlatform *platform = NULL;
    GVirConfigCapabilities *caps = NULL;
    GVirConfigDomain *config = NULL;
    GVirDesignerDomain *domain = NULL;
    GVirConnection *conn = NULL;
    gchar *xml = NULL;
    static char *os_str = NULL;
    static char *platform_str = NULL;
    static char *arch_str = NULL;
    static char *connect_uri = NULL;
    static char *graphics_str = NULL;
    GVirDesignerDomainGraphics graphics;
    static gboolean enable_smartcard;
    static gboolean enable_usb;
    static char *resources_str = NULL;
    GVirDesignerDomainResources resources;
    GOptionContext *context = NULL;
    unsigned int i;

    static GOptionEntry entries[] =
    {
        {"connect", 'c', 0, G_OPTION_ARG_STRING, &connect_uri,
            "libvirt connection URI used for querying capabilities", "URI"},
        {"list-os", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, print_oses,
            "list IDs of known OSes", NULL},
        {"list-platform", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, print_platforms,
            "list IDs of known hypervisors", NULL},
        {"os", 'o', 0, G_OPTION_ARG_STRING, &os_str,
            "set domain OS", "OS"},
        {"platform", 'p', 0, G_OPTION_ARG_STRING, &platform_str,
            "set hypervisor under which domain will be running", "PLATFORM"},
        {"architecture", 'a', 0, G_OPTION_ARG_STRING, &arch_str,
            "set domain architecture", "ARCH"},
        {"cdrom", 'C', 0, G_OPTION_ARG_CALLBACK, add_cdrom_str,
            "add CDROM to domain with PATH being source and FORMAT its format", "PATH[,FORMAT]"},
        {"disk", 'd', 0, G_OPTION_ARG_CALLBACK, add_disk_str,
            "add disk to domain with PATH being source and FORMAT its format", "PATH[,FORMAT]"},
        {"floppy", 'F', 0, G_OPTION_ARG_CALLBACK, add_floppy_str,
            "add floppy to domain with PATH being source and FORMAT its format", "PATH[,FORMAT]"},
        {"interface", 'i', 0, G_OPTION_ARG_CALLBACK, add_iface_str,
            "add interface with NETWORK source. Possible ARGs: mac, link={up,down}", "NETWORK[,ARG=VAL]"},
        {"graphics", 'g', 0, G_OPTION_ARG_STRING, &graphics_str,
            "add graphical output to the VM. Possible values are 'spice' or 'vnc'", "GRAPHICS"},
        {"smartcard", 's', 0, G_OPTION_ARG_NONE, &enable_smartcard,
            "add smartcard reader to the VM.", NULL},
        {"usb", 'u', 0, G_OPTION_ARG_NONE, &enable_usb,
            "add USB redirection to the VM.", NULL},
        {"resources", 'r', 0, G_OPTION_ARG_STRING, &resources_str,
            "Set minimal or recommended values for cpu count and RAM amount", "{minimal|recommended}"},
        {NULL}
    };

    if (!gvir_designer_init_check(&argc, &argv, NULL))
        return EXIT_FAILURE;

    context = g_option_context_new ("- test tree model performance");
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print ("option parsing failed: %s\n", error->message);
        return EXIT_FAILURE;
    }

    conn = gvir_connection_new(connect_uri);
    gvir_connection_open(conn, NULL, &error);
    CHECK_ERROR;

    caps = gvir_connection_get_capabilities(conn, &error);
    CHECK_ERROR;

    if (os_str) {
        os = find_os(os_str);
        if (!os)
            os = find_os_by_short_id(os_str);
    } else {
        os = guess_os_from_cdrom(cdrom_str_list);
    }

    if (!os) {
        print_error("Operating system could not be found or guessed");
        goto cleanup;
    }

    if (platform_str) {
        platform = find_platform(platform_str);
        if (!platform)
            platform = find_platform_by_short_id(platform_str);
    } else {
        platform = guess_platform_from_connect(conn);
    }

    if (!platform) {
        print_error("Platform was not specified or could not be guessed");
        goto cleanup;
    }

    domain = gvir_designer_domain_new(db, os, platform, caps);

    gvir_designer_domain_setup_machine(domain, &error);
    CHECK_ERROR;

    if (enable_usb) {
        for (i = 0; i < 4; i++) {
            /* 4 USB redir channels allow to redirect 4 USB devices at once */
            g_object_unref(gvir_designer_domain_add_usb_redir(domain, &error));
            CHECK_ERROR;
        }
    }

    if (enable_smartcard) {
        g_object_unref(gvir_designer_domain_add_smartcard(domain, &error));
        CHECK_ERROR;
    }

    g_object_unref(gvir_designer_domain_add_video(domain, &error));
    CHECK_ERROR;

    g_object_unref(gvir_designer_domain_add_sound(domain, &error));
    CHECK_ERROR;

    if (arch_str) {
        gvir_designer_domain_setup_container_full(domain, arch_str, &error);
        CHECK_ERROR;
    }

    if (resources_str) {
        if (g_str_equal(resources_str, "minimal") ||
            g_str_equal(resources_str, "min"))
            resources = GVIR_DESIGNER_DOMAIN_RESOURCES_MINIMAL;
        else if (g_str_equal(resources_str, "recommended") ||
                 g_str_equal(resources_str, "rec"))
            resources = GVIR_DESIGNER_DOMAIN_RESOURCES_RECOMMENDED;
        else {
            print_error("Unknown value '%s' for resources", resources_str);
            goto cleanup;
        }
        gvir_designer_domain_setup_resources(domain, resources, &error);
        CHECK_ERROR;
    } else {
        gvir_designer_domain_setup_resources(domain,
                                             GVIR_DESIGNER_DOMAIN_RESOURCES_RECOMMENDED,
                                             NULL);
    }

    if (graphics_str) {
        if (g_str_equal(graphics_str, "spice"))
            graphics = GVIR_DESIGNER_DOMAIN_GRAPHICS_SPICE;
        else if (g_str_equal(graphics_str, "vnc"))
            graphics = GVIR_DESIGNER_DOMAIN_GRAPHICS_VNC;
        else {
            print_error("Unknown value '%s' for graphics", graphics_str);
            goto cleanup;
        }
        g_object_unref(gvir_designer_domain_add_graphics(domain,
                                                         graphics,
                                                         &error));
        CHECK_ERROR;
    }

    g_list_foreach(cdrom_str_list, add_cdrom, domain);

    g_list_foreach(disk_str_list, add_disk, domain);

    g_list_foreach(floppy_str_list, add_floppy, domain);

    g_list_foreach(iface_str_list, add_iface, domain);

    config = gvir_designer_domain_get_config(domain);
    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));

    g_printf("%s\n", xml);
    g_free(xml);

    ret = EXIT_SUCCESS;

cleanup:
    if (context)
        g_option_context_free(context);
    if (os)
        g_object_unref(G_OBJECT(os));
    if (platform)
        g_object_unref(G_OBJECT(platform));
    if (caps)
        g_object_unref(G_OBJECT(caps));
    if (domain)
        g_object_unref(G_OBJECT(domain));
    if (conn)
        gvir_connection_close(conn);

    return ret;
}

/*
=pod


=head1 NAME

virt-designer - Generate domain XML

=head1 SYNOPSIS

B<virt-designer> [I<OPTION>]...

=head1 DESCRIPTION

B<virt-designer> is a command line tool for generating XML documents for
libvirt domains. However, it cooperates with libosinfo database to guess
the correct combination of attributes (e.g. disk driver, NIC model).

B<virt-designer> does not feed libvirt with generated XML though. For now,
it's a proof of concept.

=head1 OPTIONS

The basic structure of arguments passed to B<virt-designer> is:

  virt-designer [-c URI] [OPTION] [OPTION] ...

However, arguments have no pre-defined order so users can type them
in any order they like.

=head2 General Options

=over 2

=item -c URI, --connect=URI

The libvirt connection URI which is used for querying capabilities of the
host.

=item --list-os

List IDs of operating systems known to libosinfo

=item --list-platform

List IDs of platforms known to libosinfo

=item -o OS, --os=OS

Specify operating system that will be run on the domain. I<OS> is an ID
which can be obtained via B<--list-os>.

=item -p PLATFORM, --platform=PLATFORM

Specify platform (hypervisor) under which the domain will run. I<PLATFORM>
is and ID which can be obtained via I<--list-platform>.

=item -a ARCH, --architecture=ARCH

Set domain's architecture

=item -C PATH[,FORMAT] --cdrom=PATH[,FORMAT]

Add I<PATH> as a CDROM to the domain. To specify its format (e.g. raw,
qcow2, phy) use I<FORMAT>.

=item -d PATH[,FORMAT] --disk=PATH[,FORMAT]

Add I<PATH> as a disk to the domain. To specify its format (e.g. raw,
qcow2, phy) use I<FORMAT>.

=item -i NETWORK[,ARG=VAL]

Add an interface of type network with I<NETWORK> source. Moreover, some
other configuration knobs can be set (possible I<ARG>s): I<mac>,
I<link>={up|down}

=item -g GRAPHICS

Add a graphics device of type I<GRAPHICS>. Valid values are I<spice>
or I<vnc>.

=item -s

Add smartcard reader to the VM conifguration

=item -u

Add USB controllers and setup USB redirection in the VM configuration.

=item -r RESOURCE, --resources=RESOURCES

Set I<minimal> or I<recommended> resources on the domain XML. By default,
the I<recommended> is used.

=back

Usually, both B<--os> and B<--platform> are required as they are needed to
make the right decision on driver, model, ...  when adding a new device.
However, when adding a disk which is an installation medium (e.g. a CD-ROM or
DVD), B<virt-designer> tries to guess the OS. Something similar is done with
platform. Usually, the platform is guessed from the connection URI.

=head1 EXAMPLES

Domain with Fedora 17 from locally stored ISO and one NIC with mac
00:11:22:33:44:55 and link set down:

  # virt-designer -C Fedora-17-x86_64-Live-KDE.iso \
                  -i default,mac=00:11:22:33:44:55,link=down

To add multiple devices just use appropriate argument multiple times:

  # virt-designer -d /tmp/home.img,qcow2 \
                  -d /var/lib/libvirt/images/f17.img,qcow2 \
                  -i default,mac=00:11:22:33:44:55,link=down \
                  -i blue_network \
                  -r minimal

=head1 AUTHORS

Written by Michal Privoznik, Daniel P. Berrange and team of other
contributors. See the AUTHORS file in the source distribution for the
complete list of credits.

=head1 BUGS

Report any bugs discovered to the libvirt community via the mailing
list C<http://libvirt.org/contact.html> or bug tracker C<http://libvirt.org/bugs.html>.
Alternatively report bugs to your software distributor / vendor.

=head1 COPYRIGHT

Copyright (C) 2012 Red Hat, Inc. and various contributors.
This is free software. You may redistribute copies of it under the terms of
the GNU General Public License C<http://www.gnu.org/licenses/gpl.html>. There
is NO WARRANTY, to the extent permitted by law.

=head1 SEE ALSO

C<virsh(1)>, C<virt-clone(1)>, C<virt-manager(1)>, the project website C<http://virt-manager.org>

=cut
*/
