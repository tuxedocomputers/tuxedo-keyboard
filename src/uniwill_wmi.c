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
#include <linux/delay.h>
#include "uniwill_interfaces.h"

#define UNIWILL_EC_REG_LDAT	0x8a
#define UNIWILL_EC_REG_HDAT	0x8b
#define UNIWILL_EC_REG_FLAGS	0x8c
#define UNIWILL_EC_REG_CMDL	0x8d
#define UNIWILL_EC_REG_CMDH	0x8e

#define UNIWILL_EC_BIT_RFLG	0
#define UNIWILL_EC_BIT_WFLG	1
#define UNIWILL_EC_BIT_BFLG	2
#define UNIWILL_EC_BIT_CFLG	3
#define UNIWILL_EC_BIT_DRDY	7

#define UW_EC_BUSY_WAIT_CYCLES	30
#define UW_EC_BUSY_WAIT_DELAY	15

static bool uniwill_ec_direct = true;

DEFINE_MUTEX(uniwill_ec_lock);

static int uw_wmi_ec_evaluate(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, u8 read_flag, u32 *return_buffer)
{
	acpi_status status;
	union acpi_object *out_acpi;
	int e_result = 0;

	// Kernel buffer for input argument
	u32 *wmi_arg = (u32 *) kmalloc(sizeof(u32)*10, GFP_KERNEL);
	// Byte reference to the input buffer
	u8 *wmi_arg_bytes = (u8 *) wmi_arg;

	u8 wmi_instance = 0x00;
	u32 wmi_method_id = 0x04;
	struct acpi_buffer wmi_in = { (acpi_size) sizeof(wmi_arg), wmi_arg};
	struct acpi_buffer wmi_out = { ACPI_ALLOCATE_BUFFER, NULL };

	mutex_lock(&uniwill_ec_lock);

	// Zero input buffer
	memset(wmi_arg, 0x00, 10 * sizeof(u32));

	// Configure the input buffer
	wmi_arg_bytes[0] = addr_low;
	wmi_arg_bytes[1] = addr_high;
	wmi_arg_bytes[2] = data_low;
	wmi_arg_bytes[3] = data_high;

	if (read_flag != 0) {
		wmi_arg_bytes[5] = 0x01;
	}
	
	status = wmi_evaluate_method(UNIWILL_WMI_MGMT_GUID_BC, wmi_instance, wmi_method_id, &wmi_in, &wmi_out);
	out_acpi = (union acpi_object *) wmi_out.pointer;

	if (out_acpi && out_acpi->type == ACPI_TYPE_BUFFER) {
		memcpy(return_buffer, out_acpi->buffer.pointer, out_acpi->buffer.length);
	} /* else if (out_acpi && out_acpi->type == ACPI_TYPE_INTEGER) {
		e_result = (u32) out_acpi->integer.value;
	}*/
	if (ACPI_FAILURE(status)) {
		pr_err("uniwill_wmi.h: Error evaluating method\n");
		e_result = -EIO;
	}

	kfree(out_acpi);
	kfree(wmi_arg);

	mutex_unlock(&uniwill_ec_lock);

	return e_result;
}

/**
 * EC address read through WMI
 */
static int uw_ec_read_addr_wmi(u8 addr_low, u8 addr_high, union uw_ec_read_return *output)
{
	u32 uw_data[10];
	int ret = uw_wmi_ec_evaluate(addr_low, addr_high, 0x00, 0x00, 1, uw_data);
	output->dword = uw_data[0];
	// pr_debug("addr: 0x%02x%02x value: %0#4x (high: %0#4x) result: %d\n", addr_high, addr_low, output->bytes.data_low, output->bytes.data_high, ret);
	return ret;
}

/**
 * EC address write through WMI
 */
static int uw_ec_write_addr_wmi(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, union uw_ec_write_return *output)
{
	u32 uw_data[10];
	int ret = uw_wmi_ec_evaluate(addr_low, addr_high, data_low, data_high, 0, uw_data);
	output->dword = uw_data[0];
	return ret;
}

/**
 * Direct EC address read
 */
static int uw_ec_read_addr_direct(u8 addr_low, u8 addr_high, union uw_ec_read_return *output)
{
	int result;
	u8 tmp, count, flags;
	bool ready;
	bool bflag = false;

	mutex_lock(&uniwill_ec_lock);

	ec_read(UNIWILL_EC_REG_FLAGS, &flags);
	if ((flags & (1 << UNIWILL_EC_BIT_BFLG)) > 0) {
		pr_debug("read: BFLG set\n");
		bflag = true;
	}

	flags |= (1 << UNIWILL_EC_BIT_BFLG);
	ec_write(UNIWILL_EC_REG_FLAGS, flags);

	ec_write(UNIWILL_EC_REG_LDAT, addr_low);
	ec_write(UNIWILL_EC_REG_HDAT, addr_high);

	flags &= ~(1 << UNIWILL_EC_BIT_DRDY);
	flags |= (1 << UNIWILL_EC_BIT_RFLG);
	ec_write(UNIWILL_EC_REG_FLAGS, flags);

	// Wait for ready flag
	count = UW_EC_BUSY_WAIT_CYCLES;
	ready = false;
	while (!ready && count != 0) {
		msleep(UW_EC_BUSY_WAIT_DELAY);
		ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
		ready = (tmp & (1 << UNIWILL_EC_BIT_DRDY)) != 0;
		count -= 1;
	}

	if (count != 0) {
		output->dword = 0;
		ec_read(UNIWILL_EC_REG_CMDL, &tmp);
		output->bytes.data_low = tmp;
		ec_read(UNIWILL_EC_REG_CMDH, &tmp);
		output->bytes.data_high = tmp;
		result = 0;
	} else {
		output->dword = 0xfefefefe;
		result = -EIO;
	}

	ec_write(UNIWILL_EC_REG_FLAGS, 0x00);

	mutex_unlock(&uniwill_ec_lock);

	if (bflag)
		pr_debug("addr: 0x%02x%02x value: %0#4x result: %d\n", addr_high, addr_low, output->bytes.data_low, result);

	// pr_debug("addr: 0x%02x%02x value: %0#4x result: %d\n", addr_high, addr_low, output->bytes.data_low, result);

	return result;
}

static int uw_ec_write_addr_direct(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, union uw_ec_write_return *output)
{
	int result = 0;
	u8 tmp, count, flags;
	bool ready;
	bool bflag = false;

	mutex_lock(&uniwill_ec_lock);

	ec_read(UNIWILL_EC_REG_FLAGS, &flags);
	if ((flags & (1 << UNIWILL_EC_BIT_BFLG)) > 0) {
		pr_debug("write: BFLG set\n");
		bflag = true;
	}

	flags |= (1 << UNIWILL_EC_BIT_BFLG);
	ec_write(UNIWILL_EC_REG_FLAGS, flags);

	ec_write(UNIWILL_EC_REG_LDAT, addr_low);
	ec_write(UNIWILL_EC_REG_HDAT, addr_high);
	ec_write(UNIWILL_EC_REG_CMDL, data_low);
	ec_write(UNIWILL_EC_REG_CMDH, data_high);

	flags &= ~(1 << UNIWILL_EC_BIT_DRDY);
	flags |= (1 << UNIWILL_EC_BIT_WFLG);
	ec_write(UNIWILL_EC_REG_FLAGS, flags);

	// Wait for ready flag
	count = UW_EC_BUSY_WAIT_CYCLES;
	ready = false;
	while (!ready && count != 0) {
		msleep(UW_EC_BUSY_WAIT_DELAY);
		ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
		ready = (tmp & (1 << UNIWILL_EC_BIT_DRDY)) != 0;
		count -= 1;
	}

	// Replicate wmi output depending on success
	if (count != 0) {
		output->bytes.addr_low = addr_low;
		output->bytes.addr_high = addr_high;
		output->bytes.data_low = data_low;
		output->bytes.data_high = data_high;
		result = 0;
	} else {
		output->dword = 0xfefefefe;
		result = -EIO;
	}

	ec_write(UNIWILL_EC_REG_FLAGS, 0x00);

	if (bflag)
		pr_debug("addr: 0x%02x%02x value: %0#4x result: %d\n", addr_high, addr_low, data_low, result);

	mutex_unlock(&uniwill_ec_lock);

	return result;
}

int uw_wmi_read_ec_ram(u16 addr, u8 *data)
{
	int result;
	u8 addr_low, addr_high;
	union uw_ec_read_return output;

	if (IS_ERR_OR_NULL(data))
		return -EINVAL;

	addr_low = addr & 0xff;
	addr_high = (addr >> 8) & 0xff;

	if (uniwill_ec_direct) {
		result = uw_ec_read_addr_direct(addr_low, addr_high, &output);
	} else {
		result = uw_ec_read_addr_wmi(addr_low, addr_high, &output);
	}

	*data = output.bytes.data_low;
	return result;
}

int uw_wmi_write_ec_ram(u16 addr, u8 data)
{
	int result;
	u8 addr_low, addr_high, data_low, data_high;
	union uw_ec_write_return output;

	addr_low = addr & 0xff;
	addr_high = (addr >> 8) & 0xff;
	data_low = data;
	data_high = 0x00;

	if (uniwill_ec_direct)
		result = uw_ec_write_addr_direct(addr_low, addr_high, data_low, data_high, &output);
	else
		result = uw_ec_write_addr_wmi(addr_low, addr_high, data_low, data_high, &output);

	return result;
}

struct uniwill_interface_t uniwill_wmi_interface = {
	.string_id = UNIWILL_INTERFACE_WMI_STRID,
	.read_ec_ram = uw_wmi_read_ec_ram,
	.write_ec_ram = uw_wmi_write_ec_ram
};

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

	uniwill_add_interface(&uniwill_wmi_interface);

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
	uniwill_remove_interface(&uniwill_wmi_interface);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static void uniwill_wmi_notify(struct wmi_device *wdev, union acpi_object *obj)
{
	u32 code;

	if (!IS_ERR_OR_NULL(uniwill_wmi_interface.event_callb)) {
		if (obj) {
			if (obj->type == ACPI_TYPE_INTEGER) {
				code = obj->integer.value;
				// Execute registered callback
				uniwill_wmi_interface.event_callb(code);
			} else {
				pr_debug("unknown event type - %d (%0#6x)\n", obj->type, obj->type);
			}
		} else {
			pr_debug("expected ACPI object doesn't exist\n");
		}
	} else {
		pr_debug("no registered callback\n");
	}
}

static const struct wmi_device_id uniwill_wmi_device_ids[] = {
	// Listing one should be enough, for a driver that "takes care of all anyways"
	// also prevents probe (and handling) per "device"
	{ .guid_string = UNIWILL_WMI_EVENT_GUID_2 },
	{ }
};

static struct wmi_driver uniwill_wmi_driver = {
	.driver = {
		.name = UNIWILL_INTERFACE_WMI_STRID,
		.owner = THIS_MODULE
	},
	.id_table = uniwill_wmi_device_ids,
	.probe = uniwill_wmi_probe,
	.remove = uniwill_wmi_remove,
	.notify = uniwill_wmi_notify,
};

module_wmi_driver(uniwill_wmi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for Uniwill WMI interface");
MODULE_VERSION("0.0.3");
MODULE_LICENSE("GPL");

/*
 * If set to true, the module will use the replicated WMI functions
 * (direct ec_read/ec_write) to read and write to the EC RAM instead
 * of the original. Since the original functions, in all observed cases,
 * use excessive delays, they are not preferred.
 */
module_param_cb(ec_direct_io, &param_ops_bool, &uniwill_ec_direct, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ec_direct_io, "Do not use WMI methods to read/write EC RAM (default: true).");

MODULE_DEVICE_TABLE(wmi, uniwill_wmi_device_ids);
MODULE_ALIAS_UNIWILL_WMI();
