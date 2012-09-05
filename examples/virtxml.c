/*
 * virtxml.c: produce an domain XML
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
 * License along with this library; If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Michal Privoznik <mprivozn@redhat.com>
 */

#include <config.h>
#include <libvirt-designer/libvirt-designer.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gprintf.h>

GList *disk_str_list = NULL;

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

static OsinfoDb *
get_default_osinfo_db(void)
{
    GError *err = NULL;
    OsinfoLoader *loader = NULL;
    OsinfoDb *ret = NULL;

    loader = osinfo_loader_new();
    osinfo_loader_process_default_path(loader, &err);
    if (err) {
        print_error("Unable to load default libosinfo DB: %s", err->message);
        goto cleanup;
    }

    ret = osinfo_loader_get_db(loader);
    g_object_ref(ret);

cleanup:
    g_object_unref(loader);
    return ret;
}

static gint
entity_compare(gconstpointer a, gconstpointer b)
{
    const gchar *id_a = osinfo_entity_get_param_value(OSINFO_ENTITY(a),
                                                      OSINFO_ENTITY_PROP_ID);
    const gchar *id_b = osinfo_entity_get_param_value(OSINFO_ENTITY(b),
                                                      OSINFO_ENTITY_PROP_ID);
    return g_strcmp0(id_a, id_b);
}

static gboolean
print_oses(const gchar *option_name,
           const gchar *value,
           gpointer data,
           GError **error)
{
    OsinfoDb *db = get_default_osinfo_db();
    OsinfoOsList *list;
    GList *oses = NULL;
    GList *os_iter;
    int ret = EXIT_FAILURE;

    if (!db)
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
        const char *id = osinfo_entity_get_param_value(OSINFO_ENTITY(os),
                                                       OSINFO_ENTITY_PROP_ID);

        printf("%s\n", id);
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (list)
        g_object_unref(list);
    if (db)
        g_object_unref(db);

    exit(ret);
    return TRUE;
}

static gboolean
print_platforms(const gchar *option_name,
                const gchar *value,
                gpointer data,
                GError **error)
{
    OsinfoDb *db = get_default_osinfo_db();
    OsinfoPlatformList *list;
    GList *platforms = NULL;
    GList *platform_iter;
    int ret = EXIT_FAILURE;

    if (!db)
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
        const char *id = osinfo_entity_get_param_value(OSINFO_ENTITY(platform),
                                                       OSINFO_ENTITY_PROP_ID);

        printf("%s\n", id);
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (list)
        g_object_unref(list);
    if (db)
        g_object_unref(db);

    exit(ret);
    return TRUE;
}

static void
add_disk(gpointer data,
         gpointer user_data)
{
    GVirDesignerDomain *domain = (GVirDesignerDomain *) user_data;
    char *path = (char *) data;
    char *format = NULL;
    struct stat buf;
    GError *error = NULL;

    format = strchr(path, ',');
    if (format) {
        *format = '\0';
        format++;
    }

    if (!path || !strlen(path)) {
        print_error("No path provided");
        exit(EXIT_FAILURE);
    }

    if (!stat(path, &buf) &&
        !S_ISREG(buf.st_mode)) {
        gvir_designer_domain_add_disk_device(domain, path, &error);
    } else {
        gvir_designer_domain_add_disk_file(domain, path, format, &error);
    }

    if (error) {
        print_error("%s", error->message);
        exit(EXIT_FAILURE);
    }
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
    virConnectPtr conn = NULL;
    char *caps_str = NULL;
    gchar *xml = NULL;
    static char *os_str = NULL;
    static char *platform_str = NULL;
    static char *arch_str = NULL;
    static char *connect_uri = NULL;
    GOptionContext *context;

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
        {"disk", 'd', 0, G_OPTION_ARG_CALLBACK, add_disk_str,
            "add disk to domain with PATH being source and FORMAT its format", "PATH[,FORMAT]"},
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
    if (!os_str) {
        print_error("Operating system was not specified");
        return EXIT_FAILURE;
    }
    if (!platform_str) {
        print_error("Platform was not specified");
        return EXIT_FAILURE;
    }

    conn = virConnectOpenAuth(connect_uri, virConnectAuthPtrDefault, VIR_CONNECT_RO);
    if (!conn) {
        print_error("Unable to connect to libvirt");
        return EXIT_FAILURE;
    }

    if ((caps_str = virConnectGetCapabilities(conn)) == NULL) {
        print_error("failed to get capabilities");
        goto cleanup;
    }

    os = osinfo_os_new(os_str);
    platform = osinfo_platform_new(platform_str);
    caps = gvir_config_capabilities_new_from_xml(caps_str, NULL);

    domain = gvir_designer_domain_new(os, platform, caps);

    gvir_designer_domain_setup_machine(domain, &error);
    CHECK_ERROR;

    if (arch_str) {
        gvir_designer_domain_setup_container_full(domain, arch_str, &error);
        CHECK_ERROR;
    }

    g_list_foreach(disk_str_list, add_disk, domain);

    config = gvir_designer_domain_get_config(domain);
    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));

    g_printf("%s\n", xml);

    ret = EXIT_SUCCESS;

cleanup:
    virConnectClose(conn);
    return ret;
}
