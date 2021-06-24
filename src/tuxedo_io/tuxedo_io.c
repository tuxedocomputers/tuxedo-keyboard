/*!
 * Copyright (c) 2019-2021 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include "../clevo_interfaces.h"
#include "tongfang_wmi.h"
#include "tuxedo_io_ioctl.h"

MODULE_DESCRIPTION("Hardware interface for TUXEDO laptops");
MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_VERSION("0.2.4");
MODULE_LICENSE("GPL");

MODULE_ALIAS_CLEVO_INTERFACES();
MODULE_ALIAS("wmi:" CLEVO_WMI_METHOD_GUID);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BA);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BB);
MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BC);

// Initialized in module init, global for ioctl interface
static u32 id_check_clevo;
static u32 id_check_uniwill;

static u32 clevo_identify(void)
{
	return clevo_get_active_interface_id(NULL) == 0 ? 1 : 0;
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
			clevo_arg = (CLEVO_OPT_SUBCMD_SET_PERF_PROF << 0x18) | (argument & 0xff);
			clevo_evaluate_method(CLEVO_CMD_OPT, clevo_arg, &result);
			break;
	}

	return 0;
}

static long uniwill_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0;
	u32 copy_result;
	u32 argument;
	union uw_ec_read_return reg_read_return;
	union uw_ec_write_return reg_write_return;

#ifdef DEBUG
	u32 uw_arg[10];
	u32 uw_result[10];
	int i;
	for (i = 0; i < 10; ++i) {
		uw_result[i] = 0xdeadbeef;
	}
#endif

	switch (cmd) {
		case R_UW_FANSPEED:
			uw_ec_read_addr(0x04, 0x18, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FANSPEED2:
			uw_ec_read_addr(0x09, 0x18, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP:
			uw_ec_read_addr(0x3e, 0x04, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_FAN_TEMP2:
			uw_ec_read_addr(0x4f, 0x04, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE:
			uw_ec_read_addr(0x51, 0x07, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
		case R_UW_MODE_ENABLE:
			uw_ec_read_addr(0x41, 0x07, &reg_read_return);
			result = reg_read_return.bytes.data_low;
			copy_result = copy_to_user((void *) arg, &result, sizeof(result));
			break;
#ifdef DEBUG
		case R_TF_BC:
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			// pr_info("R_TF_BC args [%0#2x, %0#2x, %0#2x, %0#2x]\n", uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3]);
			if (uniwill_ec_direct) {
				result = uw_ec_read_addr_direct(uw_arg[0], uw_arg[1], &reg_read_return);
				copy_result = copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 1, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
			}
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
			uw_ec_write_addr(0x51, 0x07, argument & 0xff, 0x00, &reg_write_return);
			break;
		case W_UW_MODE_ENABLE:
			// Note: Is for the moment set and cleared on init/exit of module (uniwill mode)
			/*
			copy_result = copy_from_user(&argument, (int32_t *) arg, sizeof(argument));
			uw_ec_write_addr(0x41, 0x07, argument & 0x01, 0x00, &reg_write_return);
			*/
			break;
        case W_UW_FANAUTO:
			uw_set_fan_auto();
			break;
#ifdef DEBUG
		case W_TF_BC:
			copy_result = copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg));
			if (uniwill_ec_direct) {
				result = uw_ec_write_addr_direct(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], &reg_write_return);
				copy_result = copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword));
			} else {
				result = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 0, uw_result);
				copy_result = copy_to_user((void *) arg, &uw_result, sizeof(uw_result));
				reg_write_return.dword = uw_result[0];
			}
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
//    .open               = fop_open,
//    .release            = fop_release
};

struct class *tuxedo_io_device_class;
dev_t tuxedo_io_device_handle;

static struct cdev tuxedo_io_cdev;

static int __init tuxedo_io_init(void)
{
	int err;

	// Hardware identification
	id_check_clevo = clevo_identify();
	id_check_uniwill = uniwill_identify() == 0 ? 1 : 0;

	if (id_check_uniwill == 1) {
		uniwill_init();
	}

#ifdef DEBUG
	if (id_check_clevo == 0 && id_check_uniwill == 0) {
		pr_debug("No matching hardware found\n");
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
	tuxedo_io_device_class = class_create(THIS_MODULE, "tuxedo_io");
	device_create(tuxedo_io_device_class, NULL, tuxedo_io_device_handle, NULL, "tuxedo_io");
	pr_debug("Module init successful\n");
	
	return 0;
}

static void __exit tuxedo_io_exit(void)
{
	if (id_check_uniwill == 1) {
		uniwill_exit();
	}

	device_destroy(tuxedo_io_device_class, tuxedo_io_device_handle);
	class_destroy(tuxedo_io_device_class);
	cdev_del(&tuxedo_io_cdev);
	unregister_chrdev_region(tuxedo_io_device_handle, 1);
	pr_debug("Module exit\n");
}

module_init(tuxedo_io_init);
module_exit(tuxedo_io_exit);
