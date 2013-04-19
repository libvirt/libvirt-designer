/*
 * libvirt-designer-main.c: libvirt designer integration
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
 * Authors:
 *   Daniel P. Berrange <berrange@redhat.com>
 *   Michal Privoznik <mprivozn@redhat.com>
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <libvirt-designer/libvirt-designer.h>
#include <libvirt-gconfig/libvirt-gconfig.h>

/**
 * gvir_designer_init:
 * @argc: (inout): pointer to application's argc
 * @argv: (inout) (array length=argc) (allow-none) (transfer none): Address of the
 *     <parameter>argv</parameter> parameter of main(), or %NULL. Any options
 *     understood by GTK+ are stripped before return.
 */
void gvir_designer_init(int *argc,
                        char ***argv)
{
    GError *err = NULL;
    if (!gvir_designer_init_check(argc, argv, &err)) {
        g_error("Could not initialize libvirt-designer: %s\n",
                err->message);
    }
}

static void gvir_log_handler(const gchar *log_domain G_GNUC_UNUSED,
                             GLogLevelFlags log_level G_GNUC_UNUSED,
                             const gchar *message,
                             gpointer user_data)
{
    if (user_data)
        fprintf(stderr, "%s\n", message);
}

/**
 * gvir_designer_init_check:
 * @argc: (inout): pointer to application's argc
 * @argv: (inout) (array length=argc) (allow-none) (transfer none): Address of the
 *     <parameter>argv</parameter> parameter of main(), or %NULL. Any options
 *     understood by GTK+ are stripped before return.
 * @err: pointer to a #GError to which a message will be posted on error
 */
gboolean gvir_designer_init_check(int *argc,
                                  char ***argv,
                                  GError **err)
{
    if (!gvir_config_init_check(argc, argv, err))
        return FALSE;

    /* GLib >= 2.31.0 debug is off by default, so we need to
     * enable it. Older versions are on by default, so we need
     * to disable it.
     */
#if GLIB_CHECK_VERSION(2, 31, 0)
    if (getenv("LIBVIRT_DESIGNER_DEBUG"))
        g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          gvir_log_handler, (void*)0x1);
#else
    if (!getenv("LIBVIRT_DESIGNER_DEBUG"))
        g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          gvir_log_handler, NULL);
#endif

    return TRUE;
}
