/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include "clevo_interfaces.h"

static int clevo_wmi_evaluate(u32 wmi_method_id, u32 wmi_arg, u32 *result)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)sizeof(wmi_arg),
					      &wmi_arg };
	struct acpi_buffer acpi_buffer_out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_result;
	acpi_status status_acpi;
	u32 return_status = 0;

	status_acpi =
		wmi_evaluate_method(CLEVO_WMI_METHOD_GUID, 0x00, wmi_method_id,
				    &acpi_buffer_in, &acpi_buffer_out);

	if (unlikely(ACPI_FAILURE(status_acpi))) {
		pr_err("failed to evaluate wmi method\n");
		return -EIO;
	}

	acpi_result = (union acpi_object *)acpi_buffer_out.pointer;
	if (!acpi_result) {
		pr_err("failed to evaluate WMI method\n");
		return_status = -1;
	} else {
		if (acpi_result->type == ACPI_TYPE_INTEGER) {
			if (!IS_ERR_OR_NULL(result)) {
				*result = (u32)acpi_result->integer.value;
				pr_debug(
					"evaluate wmi cmd: %0#4x arg: %0#10x\n",
					wmi_method_id, wmi_arg);
			}
		} else {
			pr_err("unknown output from wmi method\n");
			return_status = -EIO;
		}
	}

	kfree(acpi_result);

	return return_status;
}

u32 clevo_wmi_interface_method_call(u8 cmd, u32 arg, u32 *result_value)
{
	return clevo_wmi_evaluate(cmd, arg, result_value);
}

struct clevo_interface_t clevo_wmi_interface = {
	.string_id = "clevo_wmi",
	.method_call = clevo_wmi_interface_method_call,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int clevo_wmi_probe(struct wmi_device *wdev)
#else
static int clevo_wmi_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	u32 status, ret;

	pr_debug("clevo_wmi driver probe\n");

	if (!wmi_has_guid(CLEVO_WMI_EVENT_GUID)) {
		pr_debug("probe: Clevo event guid missing\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(CLEVO_WMI_METHOD_GUID)) {
		pr_debug("probe: Clevo method guid missing\n");
		return -ENODEV;
	}

	// Since the WMI GUIDs aren't unique let's (at least)
	// check the return of some "known existing general" method
	status = clevo_wmi_evaluate(0x52, 0, &ret);
	if (status < 0) {
		pr_debug("probe: Clevo GUIDs present but method call failed\n");
		return -ENODEV;
	}
	if (ret == 0xffffffff) {
		pr_debug(
			"probe: Clevo GUIDs present but method returned unexpected value\n");
		return -ENODEV;
	}

	// Add this interface
	clevo_keyboard_add_interface(&clevo_wmi_interface);

	// Initiate clevo keyboard, if not already loaded by other interface driver
	clevo_keyboard_init();

	pr_info("interface initialized\n");

	return 0;
}

static int clevo_wmi_remove(struct wmi_device *wdev)
{
	pr_debug("clevo_wmi driver remove\n");
	return 0;
}

static void clevo_wmi_notify(struct wmi_device *wdev, union acpi_object *dummy)
{
	u32 event_value;
	clevo_wmi_evaluate(0x01, 0, &event_value);
	pr_debug("clevo_wmi notify\n");
	if (!IS_ERR_OR_NULL(clevo_wmi_interface.event_callb)) {
		// Execute registered callback
		clevo_wmi_interface.event_callb(event_value);
	}
}

static const struct wmi_device_id clevo_wmi_device_ids[] = {
	// Listing one should be enough, for a driver that "takes care of all anyways"
	// also prevents probe (and handling) per "device"
	{ .guid_string = CLEVO_WMI_EVENT_GUID },
	{ }
};

static struct wmi_driver clevo_wmi_driver = {
	.driver = { .name = "clevo_wmi", .owner = THIS_MODULE },
	.id_table = clevo_wmi_device_ids,
	.probe = clevo_wmi_probe,
	.remove = clevo_wmi_remove,
	.notify = clevo_wmi_notify,
};

module_wmi_driver(clevo_wmi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for Clevo WMI interface");
MODULE_VERSION("0.0.2");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, clevo_wmi_device_ids);
MODULE_ALIAS_CLEVO_WMI();
