/*
 * libvirt-designer-domain.h: libvirt domain configuration
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

#if !defined(__LIBVIRT_DESIGNER_H__) && !defined(LIBVIRT_DESIGNER_BUILD)
#error "Only <libvirt-designer/libvirt-designer.h> can be included directly."
#endif

#ifndef __LIBVIRT_DESIGNER_DOMAIN_H__
#define __LIBVIRT_DESIGNER_DOMAIN_H__

#include <osinfo/osinfo.h>
#include <libvirt-gconfig/libvirt-gconfig.h>

G_BEGIN_DECLS

#define GVIR_DESIGNER_TYPE_DOMAIN            (gvir_designer_domain_get_type ())
#define GVIR_DESIGNER_DOMAIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GVIR_DESIGNER_TYPE_DOMAIN, GVirDesignerDomain))
#define GVIR_DESIGNER_DOMAIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GVIR_DESIGNER_TYPE_DOMAIN, GVirDesignerDomainClass))
#define GVIR_DESIGNER_IS_DOMAIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GVIR_DESIGNER_TYPE_DOMAIN))
#define GVIR_DESIGNER_IS_DOMAIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GVIR_DESIGNER_TYPE_DOMAIN))
#define GVIR_DESIGNER_DOMAIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GVIR_DESIGNER_TYPE_DOMAIN, GVirDesignerDomainClass))

typedef struct _GVirDesignerDomain GVirDesignerDomain;
typedef struct _GVirDesignerDomainPrivate GVirDesignerDomainPrivate;
typedef struct _GVirDesignerDomainClass GVirDesignerDomainClass;

struct _GVirDesignerDomain
{
    GObject parent;

    GVirDesignerDomainPrivate *priv;

    /* Do not add fields to this struct */
};

struct _GVirDesignerDomainClass
{
    GObjectClass parent_class;

    gpointer padding[20];
};

GType gvir_designer_domain_get_type(void);

GVirDesignerDomain *gvir_designer_domain_new(OsinfoOs *os,
                                             OsinfoPlatform *platform,
                                             GVirConfigCapabilities *caps);

OsinfoOs *gvir_designer_domain_get_os(GVirDesignerDomain *design);
OsinfoPlatform *gvir_designer_domain_get_platform(GVirDesignerDomain *design);
GVirConfigCapabilities *gvir_designer_domain_get_capabilities(GVirDesignerDomain *design);
GVirConfigDomain *gvir_designer_domain_get_config(GVirDesignerDomain *design);


gboolean gvir_designer_domain_supports_machine(GVirDesignerDomain *design);
gboolean gvir_designer_domain_supports_machine_full(GVirDesignerDomain *design,
                                                    const char *arch,
                                                    GVirConfigDomainOsType ostype);
gboolean gvir_designer_domain_supports_container(GVirDesignerDomain *design);
gboolean gvir_designer_domain_supports_container_full(GVirDesignerDomain *design,
                                                      const char *arch);


gboolean gvir_designer_domain_setup_machine(GVirDesignerDomain *design,
                                            GError **error);
gboolean gvir_designer_domain_setup_machine_full(GVirDesignerDomain *design,
                                                 const char *arch,
                                                 GVirConfigDomainOsType ostype,
                                                 GError **error);

gboolean gvir_designer_domain_setup_container(GVirDesignerDomain *design,
                                              GError **error);
gboolean gvir_designer_domain_setup_container_full(GVirDesignerDomain *design,
                                                   const char *arch,
                                                   GError **error);

GVirConfigDomainDisk *gvir_designer_domain_add_disk_file(GVirDesignerDomain *design,
                                                         const char *filepath,
                                                         const char *format,
                                                         GError **error);
GVirConfigDomainDisk *gvir_designer_domain_add_disk_device(GVirDesignerDomain *design,
                                                           const char *devpath,
                                                           GError **error);

G_END_DECLS

#endif /* __LIBVIRT_DESIGNER_DOMAIN_H__ */
