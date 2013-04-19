/*
 * test-designer-domain.c: libvirt domain configuration test case
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
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#include <config.h>

#include <libvirt-designer/libvirt-designer.h>

static const gchar *capsqemuxml =
    "<capabilities>"
    "  <host>"
    "    <uuid>b9d70ef8-6756-4b51-8901-f0e65af0dcd8</uuid>"
    "    <cpu>"
    "      <arch>x86_64</arch>"
    "      <model>core2duo</model>"
    "      <vendor>Intel</vendor>"
    "      <topology sockets='1' cores='2' threads='1'/>"
    "    </cpu>"
    "  </host>"
    "  <guest>"
    "    <os_type>hvm</os_type>"
    "    <arch name='i686'>"
    "      <wordsize>32</wordsize>"
    "      <emulator>/usr/bin/qemu-system-x86_64</emulator>"
    "      <machine>pc-1.0</machine>"
    "      <machine canonical='pc-1.0'>pc</machine>"
    "      <domain type='qemu'>"
    "      </domain>"
    "      <domain type='kvm'>"
    "        <emulator>/usr/bin/qemu-kvm</emulator>"
    "        <machine>pc-1.0</machine>"
    "        <machine canonical='pc-1.0'>pc</machine>"
    "      </domain>"
    "    </arch>"
    "    <features>"
    "      <cpuselection/>"
    "      <deviceboot/>"
    "      <pae/>"
    "      <nonpae/>"
    "      <acpi default='on' toggle='yes'/>"
    "      <apic default='on' toggle='no'/>"
    "    </features>"
    "  </guest>"
    "  <guest>"
    "    <os_type>hvm</os_type>"
    "    <arch name='x86_64'>"
    "      <wordsize>64</wordsize>"
    "      <emulator>/usr/bin/qemu-system-x86_64</emulator>"
    "      <machine>pc-1.0</machine>"
    "      <machine canonical='pc-1.0'>pc</machine>"
    "      <domain type='qemu'>"
    "      </domain>"
    "      <domain type='kvm'>"
    "        <emulator>/usr/bin/qemu-kvm</emulator>"
    "        <machine>pc-1.0</machine>"
    "        <machine canonical='pc-1.0'>pc</machine>"
    "        <machine>isapc</machine>"
    "      </domain>"
    "    </arch>"
    "    <features>"
    "      <cpuselection/>"
    "      <deviceboot/>"
    "      <acpi default='on' toggle='yes'/>"
    "      <apic default='on' toggle='no'/>"
    "    </features>"
    "  </guest>"
    "  <guest>"
    "    <os_type>hvm</os_type>"
    "    <arch name='mipsel'>"
    "      <wordsize>32</wordsize>"
    "      <emulator>/usr/bin/qemu-system-mipsel</emulator>"
    "      <machine>malta</machine>"
    "      <domain type='qemu'>"
    "      </domain>"
    "    </arch>"
    "    <features>"
    "      <deviceboot/>"
    "    </features>"
    "  </guest>"
    "</capabilities>";

static const gchar *capslxcxml =
    "<capabilities>"
    "  <host>"
    "    <uuid>b9d70ef8-6756-4b51-8901-f0e65af0dcd8</uuid>"
    "    <cpu>"
    "      <arch>x86_64</arch>"
    "    </cpu>"
    "  </host>"
    "  <guest>"
    "    <os_type>exe</os_type>"
    "    <arch name='x86_64'>"
    "      <wordsize>64</wordsize>"
    "      <emulator>/usr/libexec/libvirt_lxc</emulator>"
    "      <domain type='lxc'>"
    "      </domain>"
    "    </arch>"
    "  </guest>"
    "  <guest>"
    "    <os_type>exe</os_type>"
    "    <arch name='i686'>"
    "      <wordsize>32</wordsize>"
    "      <emulator>/usr/libexec/libvirt_lxc</emulator>"
    "      <domain type='lxc'>"
    "      </domain>"
    "    </arch>"
    "  </guest>"
    "</capabilities>";

static const gchar *domain_machine_simple_iso_result =
    "<domain>\n"
    "  <devices>\n"
    "    <disk type=\"file\">\n"
    "      <source file=\"/foo/bar1\"/>\n"
    "      <driver name=\"qemu\" type=\"raw\"/>\n"
    "      <target bus=\"ide\" dev=\"hda\"/>\n"
    "    </disk>\n"
    "    <disk type=\"block\">\n"
    "      <source dev=\"/foo/bar2\"/>\n"
    "      <driver name=\"qemu\" type=\"raw\"/>\n"
    "      <target bus=\"ide\" dev=\"hdb\"/>\n"
    "    </disk>\n"
    "    <disk type=\"file\">\n"
    "      <source file=\"/foo/bar3\"/>\n"
    "      <driver name=\"qemu\" type=\"qcow2\"/>\n"
    "      <target bus=\"ide\" dev=\"hdc\"/>\n"
    "    </disk>\n"
    "    <disk type=\"block\">\n"
    "      <source dev=\"/foo/bar4\"/>\n"
    "      <driver name=\"qemu\" type=\"raw\"/>\n"
    "      <target bus=\"ide\" dev=\"hdd\"/>\n"
    "    </disk>\n"
    "    <disk type=\"file\">\n"
    "      <source file=\"/foo/bar5\"/>\n"
    "      <driver name=\"qemu\" type=\"bochs\"/>\n"
    "      <target bus=\"ide\" dev=\"hde\"/>\n"
    "    </disk>\n"
    "    <disk type=\"block\">\n"
    "      <source dev=\"/foo/bar6\"/>\n"
    "      <driver name=\"qemu\" type=\"raw\"/>\n"
    "      <target bus=\"ide\" dev=\"hdf\"/>\n"
    "    </disk>\n"
    "  </devices>\n"
    "</domain>";

static void test_domain_machine_setup(GVirDesignerDomain **design, gconstpointer opaque)
{
    OsinfoOs *os = osinfo_os_new("http://myoperatingsystem/amazing/4.2");
    OsinfoDb *db = osinfo_db_new();
    OsinfoPlatform *platform = osinfo_platform_new("http://myhypervisor.org/awesome/6.6.6");
    GVirConfigCapabilities *caps = gvir_config_capabilities_new_from_xml(capsqemuxml, NULL);

    *design = gvir_designer_domain_new(db, os, platform, caps);

    g_object_unref(os);
    g_object_unref(db);
    g_object_unref(platform);
    g_object_unref(caps);
}


static void test_domain_machine_simple_disk_setup(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    GVirConfigDomainDisk *disk;

    test_domain_machine_setup(design, opaque);
    g_assert(*design);

    disk = gvir_designer_domain_add_cdrom_file(*design, "/foo/bar1", "raw", &error);
    g_assert(disk);
    g_object_unref(disk);

    disk = gvir_designer_domain_add_cdrom_device(*design, "/foo/bar2", &error);
    g_assert(disk);
    g_object_unref(disk);

    disk = gvir_designer_domain_add_disk_file(*design, "/foo/bar3", "qcow2", &error);
    g_assert(disk);
    g_object_unref(disk);

    disk = gvir_designer_domain_add_disk_device(*design, "/foo/bar4", &error);
    g_assert(disk);
    g_object_unref(disk);

    disk = gvir_designer_domain_add_floppy_file(*design, "/foo/bar5", "bochs", &error);
    g_assert(disk);
    g_object_unref(disk);

    disk = gvir_designer_domain_add_disk_device(*design, "/foo/bar6", &error);
    g_assert(disk);
    g_object_unref(disk);
}


static void test_domain_container_setup(GVirDesignerDomain **design, gconstpointer opaque)
{
    OsinfoOs *os = osinfo_os_new("http://myoperatingsystem/amazing/4.2");
    OsinfoDb *db = osinfo_db_new();
    OsinfoPlatform *platform = osinfo_platform_new("http://myhypervisor.org/awesome/6.6.6");
    GVirConfigCapabilities *caps = gvir_config_capabilities_new_from_xml(capslxcxml, NULL);

    *design = gvir_designer_domain_new(db, os, platform, caps);

    g_object_unref(os);
    g_object_unref(db);
    g_object_unref(platform);
    g_object_unref(caps);
}


static void test_domain_machine_host_arch_run(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    gboolean ret;
    GVirConfigDomain *config;
    GVirConfigDomainOs *osconfig;
    gchar *xml;

    g_assert(gvir_designer_domain_supports_machine(*design));

    ret = gvir_designer_domain_setup_machine(*design, &error);
    if (!ret) {
        g_test_message("Failed %s", error->message);
        g_error_free(error);
    }
    g_assert(ret);

    config = gvir_designer_domain_get_config(*design);
    osconfig = gvir_config_domain_get_os(config);

    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));
    g_test_message("XML %s", xml);
    g_free(xml);

    g_assert_cmpint(gvir_config_domain_get_virt_type(config),
                    ==,
                    GVIR_CONFIG_DOMAIN_VIRT_KVM);
    g_assert_cmpint(gvir_config_domain_os_get_os_type(osconfig),
                    ==,
                    GVIR_CONFIG_DOMAIN_OS_TYPE_HVM);
    g_assert_cmpstr(gvir_config_domain_os_get_arch(osconfig),
                    ==,
                    "x86_64");

    g_object_unref(osconfig);
}

static void test_domain_machine_alt_arch_run(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    gboolean ret;
    GVirConfigDomain *config;
    GVirConfigDomainOs *osconfig;
    gchar *xml;

    g_assert(gvir_designer_domain_supports_machine_full(*design,
                                                        "x86_64",
                                                        GVIR_CONFIG_DOMAIN_OS_TYPE_HVM));
    g_assert(gvir_designer_domain_supports_machine_full(*design,
                                                        "i686",
                                                        GVIR_CONFIG_DOMAIN_OS_TYPE_HVM));
    g_assert(gvir_designer_domain_supports_machine_full(*design,
                                                        "mipsel",
                                                        GVIR_CONFIG_DOMAIN_OS_TYPE_HVM));

    ret = gvir_designer_domain_setup_machine_full(*design,
                                                  "mipsel",
                                                  GVIR_CONFIG_DOMAIN_OS_TYPE_HVM,
                                                  &error);
    if (!ret) {
        g_test_message("Failed %s", error->message);
        g_error_free(error);
    }
    g_assert(ret);

    config = gvir_designer_domain_get_config(*design);
    osconfig = gvir_config_domain_get_os(config);

    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));
    g_test_message("XML %s", xml);
    g_free(xml);

    g_assert_cmpint(gvir_config_domain_get_virt_type(config),
                    ==,
                    GVIR_CONFIG_DOMAIN_VIRT_QEMU);
    g_assert_cmpint(gvir_config_domain_os_get_os_type(osconfig),
                    ==,
                    GVIR_CONFIG_DOMAIN_OS_TYPE_HVM);
    g_assert_cmpstr(gvir_config_domain_os_get_arch(osconfig),
                    ==,
                    "mipsel");

    g_object_unref(osconfig);
}

static void test_domain_machine_simple_disk_run(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    gboolean ret;
    GVirConfigDomain *config;
    gchar *xml;

    config = gvir_designer_domain_get_config(*design);
    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));
    g_test_message("XML %s", xml);
    g_assert_cmpstr(xml,
                    ==,
                    domain_machine_simple_iso_result);
    g_free(xml);
}

static void test_domain_container_host_arch_run(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    gboolean ret;
    GVirConfigDomain *config;
    GVirConfigDomainOs *osconfig;
    gchar *xml;

    g_assert(gvir_designer_domain_supports_container(*design));

    ret = gvir_designer_domain_setup_container(*design, &error);
    if (!ret) {
        g_test_message("Failed %s", error->message);
        g_error_free(error);
    }
    g_assert(ret);

    config = gvir_designer_domain_get_config(*design);
    osconfig = gvir_config_domain_get_os(config);

    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));
    g_test_message("XML %s", xml);
    g_free(xml);

    g_assert_cmpint(gvir_config_domain_get_virt_type(config),
                    ==,
                    GVIR_CONFIG_DOMAIN_VIRT_LXC);
    g_assert_cmpint(gvir_config_domain_os_get_os_type(osconfig),
                    ==,
                    GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);
    g_assert_cmpstr(gvir_config_domain_os_get_arch(osconfig),
                    ==,
                    "x86_64");

    g_object_unref(osconfig);
}

static void test_domain_container_alt_arch_run(GVirDesignerDomain **design, gconstpointer opaque)
{
    GError *error = NULL;
    gboolean ret;
    GVirConfigDomain *config;
    GVirConfigDomainOs *osconfig;
    gchar *xml;

    g_assert(gvir_designer_domain_supports_container_full(*design,
                                                          "i686"));

    g_assert(gvir_designer_domain_supports_container_full(*design,
                                                          "x86_64"));

    ret = gvir_designer_domain_setup_container_full(*design,
                                                    "i686",
                                                    &error);
    if (!ret) {
        g_test_message("Failed %s", error->message);
        g_error_free(error);
    }
    g_assert(ret);

    config = gvir_designer_domain_get_config(*design);
    osconfig = gvir_config_domain_get_os(config);

    xml = gvir_config_object_to_xml(GVIR_CONFIG_OBJECT(config));
    g_test_message("XML %s", xml);
    g_free(xml);

    g_assert_cmpint(gvir_config_domain_get_virt_type(config),
                    ==,
                    GVIR_CONFIG_DOMAIN_VIRT_LXC);
    g_assert_cmpint(gvir_config_domain_os_get_os_type(osconfig),
                    ==,
                    GVIR_CONFIG_DOMAIN_OS_TYPE_EXE);
    g_assert_cmpstr(gvir_config_domain_os_get_arch(osconfig),
                    ==,
                    "i686");

    g_object_unref(osconfig);
}

static void test_domain_teardown(GVirDesignerDomain **design, gconstpointer opaque)
{
    if (*design)
        g_object_unref(*design);
    *design = NULL;
}

int main(int argc, char **argv)
{
    GVirDesignerDomain *domain = NULL;

    if (!gvir_designer_init_check(&argc, &argv, NULL))
        return EXIT_FAILURE;

    g_test_init(&argc, &argv, NULL);
    g_test_add("/TestDesignerDomain/MachineHostArch",
               GVirDesignerDomain *,
               &domain,
               test_domain_machine_setup,
               test_domain_machine_host_arch_run,
               test_domain_teardown);
    g_test_add("/TestDesignerDomain/MachineAltArch",
               GVirDesignerDomain *,
               &domain,
               test_domain_machine_setup,
               test_domain_machine_alt_arch_run,
               test_domain_teardown);
    g_test_add("/TestDesignerDomain/ContainerHostArch",
               GVirDesignerDomain *,
               &domain,
               test_domain_container_setup,
               test_domain_container_host_arch_run,
               test_domain_teardown);
    g_test_add("/TestDesignerDomain/ContainerAltArch",
               GVirDesignerDomain *,
               &domain,
               test_domain_container_setup,
               test_domain_container_alt_arch_run,
               test_domain_teardown);
    g_test_add("/TestDesignerDomain/SimpleDisk",
               GVirDesignerDomain *,
               &domain,
               test_domain_machine_simple_disk_setup,
               test_domain_machine_simple_disk_run,
               test_domain_teardown);

    return g_test_run();
}
