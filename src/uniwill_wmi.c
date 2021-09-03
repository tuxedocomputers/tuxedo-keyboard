/*!
 * Copyright (c) 2021 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-keyboard.
 *
 * tuxedo-keyboard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/version.h>
#include "uniwill_interfaces.h"

/*u32 uniwill_wmi_interface_method_call(u8 cmd, u32 arg, u32 *result_value)
{
	return uniwill_wmi_evaluate(cmd, arg, result_value);
}

struct uniwill_interface_t uniwill_wmi_interface = {
	.string_id = "uniwill_wmi",
	.method_call = uniwill_wmi_interface_method_call,
};*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int uniwill_wmi_probe(struct wmi_device *wdev)
#else
static int uniwill_wmi_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	int status;

	// Look for for GUIDs used on uniwill devices
	status =
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_0) &&
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_1) &&
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_2) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BA) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BB) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BC);
	
	if (!status) {
		pr_debug("probe: At least one Uniwill GUID missing\n");
		return -ENODEV;
	}

	// TODO: Add interface and init

	pr_info("interface initialized\n");

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int uniwill_wmi_remove(struct wmi_device *wdev)
#else
static void uniwill_wmi_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("uniwill_wmi driver remove\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static void uniwill_wmi_notify(struct wmi_device *wdev, union acpi_object *dummy)
{
	//u32 event_value;
	pr_debug("uniwill_wmi notify\n");
	/*if (!IS_ERR_OR_NULL(uniwill_wmi_interface.event_callb)) {
		// Execute registered callback
		// TODO
	}*/
}

static const struct wmi_device_id uniwill_wmi_device_ids[] = {
	// Listing one should be enough, for a driver that "takes care of all anyways"
	// also prevents probe (and handling) per "device"
	{ .guid_string = UNIWILL_WMI_EVENT_GUID_2 },
	{ }
};

static struct wmi_driver uniwill_wmi_driver = {
	.driver = { .name = "uniwill_wmi", .owner = THIS_MODULE },
	.id_table = uniwill_wmi_device_ids,
	.probe = uniwill_wmi_probe,
	.remove = uniwill_wmi_remove,
	.notify = uniwill_wmi_notify,
};

module_wmi_driver(uniwill_wmi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for Uniwill WMI interface");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, uniwill_wmi_device_ids);
MODULE_ALIAS_UNIWILL_WMI();
