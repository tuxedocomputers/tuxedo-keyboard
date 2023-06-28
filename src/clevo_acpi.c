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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/version.h>
#include "clevo_interfaces.h"

#define DRIVER_NAME			"clevo_acpi"

struct clevo_acpi_driver_data_t {
	struct acpi_device *adev;
	struct clevo_interface_t *clevo_interface;
};

static struct clevo_acpi_driver_data_t *active_driver_data = NULL;

static int clevo_acpi_evaluate(struct acpi_device *device, u8 cmd, u32 arg, union acpi_object **result)
{
	int status;
	acpi_handle handle;
	u64 dsm_rev_dummy = 0x00; // Dummy 0 value since not used
	u64 dsm_func = cmd;
	// Integer package data for argument
	union acpi_object dsm_argv4_package_data[] = {
		{
			.integer.type = ACPI_TYPE_INTEGER,
			.integer.value = arg
		}
	};

	// Package argument
	union acpi_object dsm_argv4 = {
		.package.type = ACPI_TYPE_PACKAGE,
		.package.count = 1,
		.package.elements = dsm_argv4_package_data
	};

	union acpi_object *out_obj;

	guid_t clevo_acpi_dsm_uuid;

	status = guid_parse(CLEVO_ACPI_DSM_UUID, &clevo_acpi_dsm_uuid);
	if (status < 0)
		return -ENOENT;

	handle = acpi_device_handle(device);
	if (handle == NULL)
		return -ENODEV;

	out_obj = acpi_evaluate_dsm(handle, &clevo_acpi_dsm_uuid, dsm_rev_dummy, dsm_func, &dsm_argv4);
	if (!out_obj) {
		pr_err("failed to evaluate _DSM\n");
		status = -1;
	}
	else {
		if (!IS_ERR_OR_NULL(result)) {
			*result = out_obj;
		}
	}

	return status;
}

int clevo_acpi_interface_method_call(u8 cmd, u32 arg, union acpi_object **result_value)
{
	int status = 0;

	if (!IS_ERR_OR_NULL(active_driver_data)) {
		status = clevo_acpi_evaluate(active_driver_data->adev, cmd, arg, result_value);
	} else {
		pr_err("acpi method call exec, no driver data found\n");
		pr_err("..for method_call: %0#4x arg: %0#10x\n", cmd, arg);
		status = -ENODATA;
	}
	// pr_debug("clevo_acpi method_call: %0#4x arg: %0#10x result: %0#10x\n", cmd, arg, !IS_ERR_OR_NULL(result_value) ? *result_value : 0);

	return status;
}

struct clevo_interface_t clevo_acpi_interface = {
	.string_id = CLEVO_INTERFACE_ACPI_STRID,
	.method_call = clevo_acpi_interface_method_call,
};

static int clevo_acpi_add(struct acpi_device *device)
{
	struct clevo_acpi_driver_data_t *driver_data;

	driver_data = devm_kzalloc(&device->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	driver_data->adev = device;
	driver_data->clevo_interface = &clevo_acpi_interface;

	active_driver_data = driver_data;

	pr_debug("clevo_acpi driver add\n");

	// Add this interface
	clevo_keyboard_add_interface(&clevo_acpi_interface);

	pr_info("interface initialized\n");

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int clevo_acpi_remove(struct acpi_device *device)
#else
static void clevo_acpi_remove(struct acpi_device *device)
#endif
{
	pr_debug("clevo_acpi driver remove\n");
	clevo_keyboard_remove_interface(&clevo_acpi_interface);
	active_driver_data = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
	return 0;
#endif
}

void clevo_acpi_notify(struct acpi_device *device, u32 event)
{
	u32 event_value;
	union acpi_object *out_obj;
	int status;
	// struct clevo_acpi_driver_data_t *clevo_acpi_driver_data;

	status = clevo_acpi_evaluate(device, 0x01, 0, &out_obj);
	if (!status) {
			if (out_obj->type == ACPI_TYPE_INTEGER) {
				event_value = (u32)out_obj->integer.value;
			} else {
				pr_err("return type not integer, use clevo_evaluate_method2\n");
			}
			ACPI_FREE(out_obj);
	}
	pr_debug("clevo_acpi event: %0#6x, clevo event value: %0#6x\n", event, event_value);

	// clevo_acpi_driver_data = container_of(&device, struct clevo_acpi_driver_data_t, adev);
	if (!IS_ERR_OR_NULL(clevo_acpi_interface.event_callb)) {
		// Execute registered callback
		clevo_acpi_interface.event_callb(event);
	}
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct device *dev)
{
	pr_debug("driver suspend\n");
	return 0;
}

static int driver_resume_callb(struct device *dev)
{
	pr_debug("driver resume\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(clevo_driver_pm_ops, driver_suspend_callb, driver_resume_callb);
#endif

static const struct acpi_device_id clevo_acpi_device_ids[] = {
	{CLEVO_ACPI_RESOURCE_HID, 0},
	{"", 0}
};

static struct acpi_driver clevo_acpi_driver = {
	.name = DRIVER_NAME,
	.class = DRIVER_NAME,
	.owner = THIS_MODULE,
	.ids = clevo_acpi_device_ids,
	.flags = ACPI_DRIVER_ALL_NOTIFY_EVENTS,
	.ops = {
		.add = clevo_acpi_add,
		.remove = clevo_acpi_remove,
		.notify = clevo_acpi_notify,
	},
#ifdef CONFIG_PM
	.drv.pm = &clevo_driver_pm_ops
#endif
};

module_acpi_driver(clevo_acpi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for Clevo ACPI interface");
MODULE_VERSION("0.1.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(acpi, clevo_acpi_device_ids);
