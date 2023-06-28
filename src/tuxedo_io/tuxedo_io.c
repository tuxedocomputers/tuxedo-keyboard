/*!
 * Copyright (c) 2019-2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-io.
 *
 * tuxedo-io is free software: you can redistribute it and/or modify
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
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/dmi.h>
#include "../clevo_interfaces.h"
#include "../uniwill_interfaces.h"
#include "tuxedo_io_ioctl.h"

MODULE_DESCRIPTION("Hardware interface for TUXEDO laptops");
MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_VERSION("0.3.6");
MODULE_LICENSE("GPL");

MODULE_ALIAS_CLEVO_INTERFACES();
MODULE_ALIAS("wmi:" CLEVO_WMI_METHOD_GUID);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BA);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BB);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BC);

// Initialized in module init, global for ioctl interface
static u32 id_check_clevo;
static u32 id_check_uniwill;

static struct uniwill_device_features_t *uw_feats;

/**
 * strstr version of dmi_match
 */
static bool __attribute__ ((unused)) dmi_string_in(enum dmi_field f, const char *str)
{
	const char *info = dmi_get_system_info(f);

	if (info == NULL || str == NULL)
		return info == str;

	return strstr(info, str) != NULL;
}

static u32 clevo_identify(void)
{
	return clevo_get_active_interface_id(NULL) == 0 ? 1 : 0;
}

/*
 * TDP boundary definitions per device
 */
static int tdp_min_ph4tux[] = { 0x05, 0x05, 0x00 };
static int tdp_max_ph4tux[] = { 0x26, 0x26, 0x00 };

static int tdp_min_ph4trx[] = { 0x05, 0x05, 0x00 };
static int tdp_max_ph4trx[] = { 0x32, 0x32, 0x00 };

static int tdp_min_ph4tqx[] = { 0x05, 0x05, 0x00 };
static int tdp_max_ph4tqx[] = { 0x32, 0x32, 0x00 };

static int tdp_min_ph4axx[] = { 0x05, 0x05, 0x00 };
static int tdp_max_ph4axx[] = { 0x2d, 0x3c, 0x00 };

static int tdp_min_phxpxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_phxpxx[] = { 0x2a, 0x32, 0x5a };

static int tdp_min_pfxluxg[] = { 0x05, 0x05, 0x05 };
static int tdp_max_pfxluxg[] = { 0x23, 0x23, 0x28 };

static int tdp_min_gmxngxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxngxx[] = { 0x50, 0x50, 0x5f };

static int tdp_min_gmxmgxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxmgxx[] = { 0x78, 0x78, 0xc8 };

static int tdp_min_gmxtgxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxtgxx[] = { 0x78, 0x78, 0xc8 };

static int tdp_min_gmxzgxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxzgxx[] = { 0x50, 0x50, 0x5f };

static int tdp_min_gmxagxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxagxx[] = { 0x78, 0x78, 0xd7 };

static int tdp_min_gmxrgxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxrgxx[] = { 0x64, 0x64, 0x6e };

static int tdp_min_gmxpxxx[] = { 0x05, 0x05, 0x05 };
static int tdp_max_gmxpxxx[] = { 0x82, 0x82, 0xc8 };

static int *tdp_min_defs = NULL;
static int *tdp_max_defs = NULL;

void uw_id_tdp(void)
{
	if (uw_feats->model == UW_MODEL_PH4TUX) {
		tdp_min_defs = tdp_min_ph4tux;
		tdp_max_defs = tdp_max_ph4tux;
	} else if (uw_feats->model == UW_MODEL_PH4TRX) {
		tdp_min_defs = tdp_min_ph4trx;
		tdp_max_defs = tdp_max_ph4trx;
	} else if (uw_feats->model == UW_MODEL_PH4TQF) {
		tdp_min_defs = tdp_min_ph4tqx;
		tdp_max_defs = tdp_max_ph4tqx;
	} else if (uw_feats->model == UW_MODEL_PH4AQF_ARX) {
		tdp_min_defs = tdp_min_ph4axx;
		tdp_max_defs = tdp_max_ph4axx;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	} else if (dmi_match(DMI_PRODUCT_SKU, "IBP1XI08MK1")) {
		tdp_min_defs = tdp_min_phxpxx;
		tdp_max_defs = tdp_max_phxpxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "PULSE1502")) {
		tdp_min_defs = tdp_min_pfxluxg;
		tdp_max_defs = tdp_max_pfxluxg;
	} else if (dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA02")) {
		tdp_min_defs = tdp_min_gmxngxx;
		tdp_max_defs = tdp_max_gmxngxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI02")) {
		tdp_min_defs = tdp_min_gmxmgxx;
		tdp_max_defs = tdp_max_gmxmgxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")) {
		tdp_min_defs = tdp_min_gmxtgxx;
		tdp_max_defs = tdp_max_gmxtgxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")) {
		tdp_min_defs = tdp_min_gmxzgxx;
		tdp_max_defs = tdp_max_gmxzgxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")) {
		tdp_min_defs = tdp_min_gmxagxx;
		tdp_max_defs = tdp_max_gmxagxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04")) {
		tdp_min_defs = tdp_min_gmxrgxx;
		tdp_max_defs = tdp_max_gmxrgxx;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05")) {
		tdp_min_defs = tdp_min_gmxpxxx;
		tdp_max_defs = tdp_max_gmxpxxx;
#endif
	} else {
		tdp_min_defs = NULL;
		tdp_max_defs = NULL;
	}
}

static u32 uniwill_identify(void)
{
	u32 result = uniwill_get_active_interface_id(NULL) == 0 ? 1 : 0;
	if (result) {
		uw_feats = uniwill_get_device_features();
		uw_id_tdp();
	}
	return result;
}

/*static int fop_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int fop_release(struct inode *inode, struct file *file)
{
	return 0;
}*/

static long clevo_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0, status;
	u32 copy_result;
	u32 argument = (u32) arg;

	u32 clevo_arg;

	const char str_no_if[] = "";
	char *str_clevo_if;
	
	switch (cmd) {
		case R_CL_HW_IF_STR:
			if (clevo_get_active_interface_id(&str_clevo_if) == 0) {
				copy_result = copy_to_user((char *) arg, str_clevo_if, strlen(str_clevo_if) + 1);
			} else {
				copy_result = copy_to_user((char *) arg, str_no_if, strlen(str_no_if) + 1);
			}
			break;
		case R_CL_FANINFO1:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO1, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FANINFO2:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO2, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FANINFO3:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO3, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		/*case R_CL_FANINFO4:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO4, 0);
			copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;*/
		case R_CL_WEBCAM_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_FLIGHTMODE_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_FLIGHTMODE_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
		case R_CL_TOUCHPAD_SW:
			status = clevo_evaluate_method(CLEVO_CMD_GET_TOUCHPAD_SW, 0, &result);
			copy_result = copy_to_user((int32_t *) arg, &result, sizeof(result));
			break;
	}

	switch (cmd) {
		case W_CL_FANSPEED:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_VALUE, argument, &result);
			// Note: Delay needed to let hardware catch up with the written value.
			// No known ready flag. If the value is read too soon, the old value
			// will still be read out.
			// (Theoretically needed for other methods as well.)
			// Can it be lower? 50ms is too low
			msleep(100);
			break;
		case W_CL_FANAUTO:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_AUTO, argument, &result);
			break;
		case W_CL_WEBCAM_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result);
			// Only set status if it isn't already the right value
			// (workaround for old and/or buggy WMI interfaces that toggle on write)
			if ((argument & 0x01) != (result & 0x01)) {
				clevo_evaluate_method(CLEVO_CMD_SET_WEBCAM_SW, argument, &result);
			}
			break;
		case W_CL_FLIGHTMODE_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_FLIGHTMODE_SW, argument, &result);
			break;
		case W_CL_TOUCHPAD_SW:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_evaluate_method(CLEVO_CMD_SET_TOUCHPAD_SW, argument, &result);
			break;
		case W_CL_PERF_PROFILE:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			clevo_arg = (CLEVO_CMD_OPT_SUB_SET_PERF_PROF << 0x18) | (argument & 0xff);
			clevo_evaluate_method(CLEVO_CMD_OPT, clevo_arg, &result);
			break;
	}

	return 0;
}

static int set_full_fan_mode(bool enable) {
	u8 mode_data;

	uniwill_read_ec_ram(0x0751, &mode_data);

	if (enable && !(mode_data & 0x40)) {
		// If not "full fan mode" (i.e. 0x40 bit not set) switch to it (required for old fancontrol)
		return uniwill_write_ec_ram(0x0751, mode_data | 0x40);
	}
	else if (mode_data & 0x40){
		// If "full fan mode" (i.e. 0x40 bit set) turn it off (required for new fancontrol)
		return uniwill_write_ec_ram(0x0751, mode_data & ~0x40);
	}

	return 0;
}

static bool fans_initialized = false;

static int uw_init_fan(void) {
	int i;

	u16 addr_use_custom_fan_table_0 = 0x07c5; // use different tables for both fans (0x0f00-0x0f2f and 0x0f30-0x0f5f respectivly)
	u16 addr_use_custom_fan_table_1 = 0x07c6; // enable 0x0fxx fantables
	u8 offset_use_custom_fan_table_0 = 7;
	u8 offset_use_custom_fan_table_1 = 2;
	u8 value_use_custom_fan_table_0;
	u8 value_use_custom_fan_table_1;
	u16 addr_cpu_custom_fan_table_end_temp = 0x0f00;
	u16 addr_cpu_custom_fan_table_start_temp = 0x0f10;
	u16 addr_cpu_custom_fan_table_fan_speed = 0x0f20;
	u16 addr_gpu_custom_fan_table_end_temp = 0x0f30;
	u16 addr_gpu_custom_fan_table_start_temp = 0x0f40;
	u16 addr_gpu_custom_fan_table_fan_speed = 0x0f50;

	if (!fans_initialized && uw_feats->uniwill_has_universal_ec_fan_control) {
		set_full_fan_mode(false);

		uniwill_read_ec_ram(addr_use_custom_fan_table_0, &value_use_custom_fan_table_0);
		if (!((value_use_custom_fan_table_0 >> offset_use_custom_fan_table_0) & 1)) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_0, value_use_custom_fan_table_0 + (1 << offset_use_custom_fan_table_0), 3);
		}

		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_end_temp, 0xff, 3);
		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_start_temp, 0x00, 3);
		uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_fan_speed, 0x00, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_end_temp, 0xff, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_start_temp, 0x00, 3);
		uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_fan_speed, 0x00, 3);
		for (i = 0x1; i <= 0xf; ++i) {
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_end_temp + i, 0xff, 3);
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_start_temp + i, 0xff, 3);
			uniwill_write_ec_ram_with_retry(addr_cpu_custom_fan_table_fan_speed + i, 0x00, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_end_temp + i, 0xff, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_start_temp + i, 0xff, 3);
			uniwill_write_ec_ram_with_retry(addr_gpu_custom_fan_table_fan_speed + i, 0x00, 3);
		}

		uniwill_read_ec_ram(addr_use_custom_fan_table_1, &value_use_custom_fan_table_1);
		if (!((value_use_custom_fan_table_1 >> offset_use_custom_fan_table_1) & 1)) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_1, value_use_custom_fan_table_1 + (1 << offset_use_custom_fan_table_1), 3);
		}
	}

	fans_initialized = true;

	return 0;
}

static u32 uw_set_fan(u32 fan_index, u8 fan_speed)
{
	u32 i;
	u8 mode_data;
	u16 addr_fan0 = 0x1804;
	u16 addr_fan1 = 0x1809;
	u16 addr_for_fan;

	u16 addr_cpu_custom_fan_table_fan_speed = 0x0f20;
	u16 addr_gpu_custom_fan_table_fan_speed = 0x0f50;

	if (uw_feats->uniwill_has_universal_ec_fan_control) {
		uw_init_fan();

		if (fan_index == 0)
			addr_for_fan = addr_cpu_custom_fan_table_fan_speed;
		else if (fan_index == 1)
			addr_for_fan = addr_gpu_custom_fan_table_fan_speed;
		else
			return -EINVAL;

		if (fan_speed == 0) {
			// Avoid hard coded EC behaviour: Setting fan speed = 0x00 spins the fan up
			// to 0x3c (30%) for 3 minutes before going to 0x00. Setting fan speed = 1
			// also causes the fan to stop since on 2020 or later TF devices the
			// microcontroller in the fan itself is intelligent enough to not try to
			// start up the motor when the speed is to slow. Older devices don't use
			// this fan controll anyway, but the else case below.
			fan_speed = 1;
		}

		uniwill_write_ec_ram(addr_for_fan, fan_speed & 0xff);
	}
	else { // old workaround using full fan mode
		if (fan_index == 0)
			addr_for_fan = addr_fan0;
		else if (fan_index == 1)
			addr_for_fan = addr_fan1;
		else
			return -EINVAL;

		// Check current mode
		uniwill_read_ec_ram(0x0751, &mode_data);
		if (!(mode_data & 0x40)) {
			// If not "full fan mode" (i.e. 0x40 bit set) switch to it (required for fancontrol)
			set_full_fan_mode(true);
			// Attempt to write both fans as quick as possible before complete ramp-up
			pr_debug("prevent ramp-up start\n");
			for (i = 0; i < 10; ++i) {
				uniwill_write_ec_ram(addr_fan0, fan_speed & 0xff);
				uniwill_write_ec_ram(addr_fan1, fan_speed & 0xff);
				msleep(10);
			}
			pr_debug("prevent ramp-up done\n");
		} else {
			// Otherwise just set the chosen fan
			uniwill_write_ec_ram(addr_for_fan, fan_speed & 0xff);
		}
	}

	return 0;
}

static u32 uw_set_fan_auto(void)
{
	u8 mode_data;

	if (uw_feats->uniwill_has_universal_ec_fan_control) {
		u16 addr_use_custom_fan_table_0 = 0x07c5; // use different tables for both fans (0x0f00-0x0f2f and 0x0f30-0x0f5f respectivly)
		u16 addr_use_custom_fan_table_1 = 0x07c6; // enable 0x0fxx fantables
		u8 offset_use_custom_fan_table_0 = 7;
		u8 offset_use_custom_fan_table_1 = 2;
		u8 value_use_custom_fan_table_0;
		u8 value_use_custom_fan_table_1;
		uniwill_read_ec_ram(addr_use_custom_fan_table_1, &value_use_custom_fan_table_1);
		if ((value_use_custom_fan_table_1 >> offset_use_custom_fan_table_1) & 1) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_1, value_use_custom_fan_table_1 - (1 << offset_use_custom_fan_table_1), 3);
		}
		uniwill_read_ec_ram(addr_use_custom_fan_table_0, &value_use_custom_fan_table_0);
		if ((value_use_custom_fan_table_0 >> offset_use_custom_fan_table_0) & 1) {
			uniwill_write_ec_ram_with_retry(addr_use_custom_fan_table_0, value_use_custom_fan_table_0 - (1 << offset_use_custom_fan_table_0), 3);
		}
		fans_initialized = false;
	}
	else {
		// Get current mode
		uniwill_read_ec_ram(0x0751, &mode_data);
		// Switch off "full fan mode" (i.e. unset 0x40 bit)
		uniwill_write_ec_ram(0x0751, mode_data & 0xbf);
	}

	return 0;
}

static int uw_get_tdp_min(u8 tdp_index)
{
	if (tdp_index > 2)
		return -EINVAL;

	if (tdp_min_defs == NULL)
		return -ENODEV;

	if (tdp_min_defs[tdp_index] <= 0) {
		return -ENODEV;
	}

	return tdp_min_defs[tdp_index];
}

static int uw_get_tdp_max(u8 tdp_index)
{
	if (tdp_index > 2)
		return -EINVAL;

	if (tdp_max_defs == NULL)
		return -ENODEV;

	if (tdp_max_defs[tdp_index] <= 0) {
		return -ENODEV;
	}

	return tdp_max_defs[tdp_index];
}

static int uw_get_tdp(u8 tdp_index)
{
	u8 tdp_data;
	u16 tdp_base_addr = 0x0783;
	u16 tdp_current_addr = tdp_base_addr + tdp_index;
	int status;

	// Use min tdp to detect support for chosen tdp parameter
	int min_tdp_status = uw_get_tdp_min(tdp_index);
	if (min_tdp_status < 0)
		return min_tdp_status;

	status = uniwill_read_ec_ram(tdp_current_addr, &tdp_data);
	if (status < 0)
		return status;

	return tdp_data;
}

static int uw_set_tdp(u8 tdp_index, u8 tdp_data)
{
	int tdp_min, tdp_max;
	u16 tdp_base_addr = 0x0783;
	u16 tdp_current_addr = tdp_base_addr + tdp_index;

	// Use min tdp to detect support for chosen tdp parameter
	int min_tdp_status = uw_get_tdp_min(tdp_index);
	if (min_tdp_status < 0)
		return min_tdp_status;

	tdp_min = uw_get_tdp_min(tdp_index);
	tdp_max = uw_get_tdp_max(tdp_index);
	if (tdp_data < tdp_min || tdp_data > tdp_max)
		return -EINVAL;

	uniwill_write_ec_ram(tdp_current_addr, tdp_data);

	return 0;
}

/**
 * Set profile 1-3 to 0xa0, 0x00 or 0x10 depending on
 * device support.
 */
static u32 uw_set_performance_profile_v1(u8 profile_index)
{
	u8 current_value = 0x00, next_value;
	u8 clear_bits = 0xa0 | 0x10;
	u32 result;
	result = uniwill_read_ec_ram(0x0751, &current_value);
	if (result >= 0) {
		next_value = current_value & ~clear_bits;
		switch (profile_index) {
		case 0x01:
			next_value |= 0xa0;
			break;
		case 0x02:
			next_value |= 0x00;
			break;
		case 0x03:
			next_value |= 0x10;
			break;
		default:
			result = -EINVAL;
			break;
		}

		if (result != -EINVAL) {
			result = uniwill_write_ec_ram(0x0751, next_value);
		}
	}

	return result;
}

static long uniwill_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0;
	u32 copy_result;
	u32 argument;
	u8 byte_data;
	const char str_no_if[] = "";
	char *str_uniwill_if;

#ifdef DEBUG
	union uw_ec_read_return reg_read_return;
	union uw_ec_write_return reg_write_return;
	u32 uw_arg[10];
	u32 uw_result[10];
	int i;
	for (i = 0; i < 10; ++i) {
		uw_result[i] = 0xdeadbeef;
	}
#endif

	switch (cmd) {
		case R_UW_HW_IF_STR:
			if (uniwill_get_active_interface_id(&str_uniwill_if) == 0) {
				copy_result = copy_to_user((char *) arg, str_uniwill_if, strlen(str_uniwill_if) + 1);
			} else {
				copy_result = copy_to_user((char *) arg, str_no_if, strlen(str_no_if) + 1);
			}
			break;
		case R_UW_MODEL_ID:
			result = uw_feats->model;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANSPEED:
			uniwill_read_ec_ram(0x1804, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANSPEED2:
			uniwill_read_ec_ram(0x1809, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP:
			uniwill_read_ec_ram(0x043e, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP2:
			uniwill_read_ec_ram(0x044f, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE:
			uniwill_read_ec_ram(0x0751, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE_ENABLE:
			uniwill_read_ec_ram(0x0741, &byte_data);
			result = byte_data;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANS_OFF_AVAILABLE:
			/*result = uw_feats->uniwill_has_universal_ec_fan_control ? 1 : 0;
			if (result == 1) {
				result = 0;
			}
			else if (result == 0) {
				result = 1;
			}*/
			result = 1;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANS_MIN_SPEED:
			/*result = uw_feats->uniwill_has_universal_ec_fan_control? 1 : 0;
			if (result == 1) {
				result = 20;
			}
			else if (result == 0) {
				result = 0;
			}*/
			result = 20;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP0:
			result = uw_get_tdp(0);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP1:
			result = uw_get_tdp(1);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP2:
			result = uw_get_tdp(2);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP0_MIN:
			result = uw_get_tdp_min(0);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP1_MIN:
			result = uw_get_tdp_min(1);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP2_MIN:
			result = uw_get_tdp_min(2);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP0_MAX:
			result = uw_get_tdp_max(0);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP1_MAX:
			result = uw_get_tdp_max(1);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_TDP2_MAX:
			result = uw_get_tdp_max(2);
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_PROFS_AVAILABLE:
			result = 0;
			if (uw_feats->uniwill_profile_v1_two_profs)
				result = 2;
			else if (uw_feats->uniwill_profile_v1_three_profs || uw_feats->uniwill_profile_v1_three_profs_leds_only)
				result = 3;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
#ifdef DEBUG
		case R_TF_BC:
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			reg_read_return.dword = 0;
			result = uniwill_read_ec_ram((uw_arg[1] << 8) | uw_arg[0], &reg_read_return.bytes.data_low);
			copy_result = copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword));
			// pr_info("R_TF_BC args [%0#2x, %0#2x, %0#2x, %0#2x]\n", uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3]);
			/*if (uniwill_ec_direct) {
				result = uw_ec_read_addr_direct(uw_arg[0], uw_arg[1], &reg_read_return);
				copy_result = copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 1, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
			}*/
			break;
#endif
	}

	switch (cmd) {
		case W_UW_FANSPEED:
			// Get fan speed argument
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_fan(0, argument);
			break;
		case W_UW_FANSPEED2:
			// Get fan speed argument
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_fan(1, argument);
			break;
		case W_UW_MODE:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uniwill_write_ec_ram(0x0751, argument & 0xff);
			break;
		case W_UW_MODE_ENABLE:
			// Note: Is for the moment set and cleared on init/exit of module (uniwill mode)
			/*
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uniwill_write_ec_ram(0x0741, argument & 0x01);
			*/
			break;
		case W_UW_FANAUTO:
			uw_set_fan_auto();
			break;
		case W_UW_TDP0:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_tdp(0, argument);
			break;
		case W_UW_TDP1:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_tdp(1, argument);
			break;
		case W_UW_TDP2:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_tdp(2, argument);
			break;
		case W_UW_PERF_PROF:
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_set_performance_profile_v1(argument);
			break;
#ifdef DEBUG
		case W_TF_BC:
			reg_write_return.dword = 0;
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			uniwill_write_ec_ram((uw_arg[1] << 8) | uw_arg[0], uw_arg[2]);
			copy_result = copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword));
			/*if (uniwill_ec_direct) {
				result = uw_ec_write_addr_direct(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], &reg_write_return);
				copy_result = copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 0, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
				reg_write_return.dword = uw_result[0];
			}*/
			/*pr_info("data_high %0#2x\n", reg_write_return.bytes.data_high);
			pr_info("data_low %0#2x\n", reg_write_return.bytes.data_low);
			pr_info("addr_high %0#2x\n", reg_write_return.bytes.addr_high);
			pr_info("addr_low %0#2x\n", reg_write_return.bytes.addr_low);*/
			break;
#endif
	}

	return 0;
}

static long fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 status;
	// u32 result = 0;
	u32 copy_result;

	const char *module_version = THIS_MODULE->version;
	switch (cmd) {
		case R_MOD_VERSION:
			copy_result = copy_to_user((char *) arg, module_version, strlen(module_version) + 1);
			break;
		// Hardware id checks, 1 = positive, 0 = negative
		case R_HWCHECK_CL:
			id_check_clevo = clevo_identify();
			copy_result = copy_to_user((void *) arg, (void *) &id_check_clevo, sizeof(id_check_clevo));
			break;
		case R_HWCHECK_UW:
			id_check_uniwill = uniwill_identify();
			copy_result = copy_to_user((void *) arg, (void *) &id_check_uniwill, sizeof(id_check_uniwill));
			break;
	}

	status = clevo_ioctl_interface(file, cmd, arg);
	if (status != 0) return status;
	status = uniwill_ioctl_interface(file, cmd, arg);
	if (status != 0) return status;

	return 0;
}

static struct file_operations fops_dev = {
	.owner              = THIS_MODULE,
	.unlocked_ioctl     = fop_ioctl
//	.open               = fop_open,
//	.release            = fop_release
};

struct class *tuxedo_io_device_class;
dev_t tuxedo_io_device_handle;

static struct cdev tuxedo_io_cdev;

static int __init tuxedo_io_init(void)
{
	int err;

	// Hardware identification
	id_check_clevo = clevo_identify();
	id_check_uniwill = uniwill_identify();

#ifdef DEBUG
	pr_debug("DEBUG is defined\n");

	if (id_check_clevo == 0 && id_check_uniwill == 0) {
		pr_debug("No matching hardware found on module load\n");
	}
#endif

	err = alloc_chrdev_region(&tuxedo_io_device_handle, 0, 1, "tuxedo_io_cdev");
	if (err != 0) {
		pr_err("Failed to allocate chrdev region\n");
		return err;
	}
	cdev_init(&tuxedo_io_cdev, &fops_dev);
	err = (cdev_add(&tuxedo_io_cdev, tuxedo_io_device_handle, 1));
	if (err < 0) {
		pr_err("Failed to add cdev\n");
		unregister_chrdev_region(tuxedo_io_device_handle, 1);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
	tuxedo_io_device_class = class_create(THIS_MODULE, "tuxedo_io");
#else
	tuxedo_io_device_class = class_create("tuxedo_io");
#endif

	device_create(tuxedo_io_device_class, NULL, tuxedo_io_device_handle, NULL, "tuxedo_io");
	pr_debug("Module init successful\n");
	
	return 0;
}

static void __exit tuxedo_io_exit(void)
{
	device_destroy(tuxedo_io_device_class, tuxedo_io_device_handle);
	class_destroy(tuxedo_io_device_class);
	cdev_del(&tuxedo_io_cdev);
	unregister_chrdev_region(tuxedo_io_device_handle, 1);
	pr_debug("Module exit\n");
}

module_init(tuxedo_io_init);
module_exit(tuxedo_io_exit);
