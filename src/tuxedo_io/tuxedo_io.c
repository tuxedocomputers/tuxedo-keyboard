/*!
 * Copyright (c) 2019-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
MODULE_VERSION("0.2.2");
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
	return clevo_get_active_interface_id(NULL);
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
	u32 argument = (u32) arg;

	const char str_no_if[] = "";
	char *str_clevo_if;
	
	switch (cmd) {
		case R_CL_HW_IF_STR:
			if (clevo_get_active_interface_id(&str_clevo_if) == 0) {
				return copy_to_user((char *) arg, str_clevo_if, strlen(str_clevo_if) + 1) ? -EFAULT : 0;
			} else {
				return copy_to_user((char *) arg, str_no_if, strlen(str_no_if) + 1) ? -EFAULT : 0;
			}
		case R_CL_FANINFO1:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO1, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_CL_FANINFO2:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO2, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_CL_FANINFO3:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO3, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		/*case R_CL_FANINFO4:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_FANINFO4, 0))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;*/
		case R_CL_WEBCAM_SW:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_CL_FLIGHTMODE_SW:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_FLIGHTMODE_SW, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_CL_TOUCHPAD_SW:
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_TOUCHPAD_SW, 0, &result))) {
				return status;
			}
			return copy_to_user((int32_t *) arg, &result, sizeof(result)) ? -EFAULT : 0;

		case W_CL_FANSPEED:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			if ((status = clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_VALUE, argument, &result))) {
				return status;
			}
			// Note: Delay needed to let hardware catch up with the written value.
			// No known ready flag. If the value is read too soon, the old value
			// will still be read out.
			// (Theoretically needed for other methods as well.)
			// Can it be lower? 50ms is too low
			msleep(100);
			return 0;
		case W_CL_FANAUTO:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return clevo_evaluate_method(CLEVO_CMD_SET_FANSPEED_AUTO, argument, &result);
		case W_CL_WEBCAM_SW:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			if ((status = clevo_evaluate_method(CLEVO_CMD_GET_WEBCAM_SW, 0, &result))) {
				return status;
			}
			// Only set status if it isn't already the right value
			// (workaround for old and/or buggy WMI interfaces that toggle on write)
			if ((argument & 0x01) != (result & 0x01)) {
				return clevo_evaluate_method(CLEVO_CMD_SET_WEBCAM_SW, argument, &result);
			}
			return 0;
		case W_CL_FLIGHTMODE_SW:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return clevo_evaluate_method(CLEVO_CMD_SET_FLIGHTMODE_SW, argument, &result);
		case W_CL_TOUCHPAD_SW:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return clevo_evaluate_method(CLEVO_CMD_SET_TOUCHPAD_SW, argument, &result);
	}

	return -ENOTTY;
}

static long uniwill_ioctl_interface(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 result = 0, status;
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
			if ((status = uw_ec_read_addr(0x04, 0x18, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_UW_FANSPEED2:
			if ((status = uw_ec_read_addr(0x09, 0x18, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_UW_FAN_TEMP:
			if ((status = uw_ec_read_addr(0x3e, 0x04, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_UW_FAN_TEMP2:
			if ((status = uw_ec_read_addr(0x4f, 0x04, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_UW_MODE:
			if ((status = uw_ec_read_addr(0x51, 0x07, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
		case R_UW_MODE_ENABLE:
			if ((status = uw_ec_read_addr(0x41, 0x07, &reg_read_return))) {
				return status;
			}
			result = reg_read_return.bytes.data_low;
			return copy_to_user((void *) arg, &result, sizeof(result)) ? -EFAULT : 0;
#ifdef DEBUG
		case R_TF_BC:
			if (copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg))) {
				return -EFAULT;
			}
			// pr_info("R_TF_BC args [%0#2x, %0#2x, %0#2x, %0#2x]\n", uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3]);
			if (uniwill_ec_direct) {
				if ((status = uw_ec_read_addr_direct(uw_arg[0], uw_arg[1], &reg_read_return))) {
					return status;
				}
				return copy_to_user((void *) arg, &reg_read_return.dword, sizeof(reg_read_return.dword)) ? -EFAULT : 0;
			} else {
				if ((status = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 1, uw_result))) {
					return status;
				}
				return copy_to_user((void *) arg, &uw_result, sizeof(uw_result)) ? -EFAULT : 0;
			}
#endif

		case W_UW_FANSPEED:
			// Get fan speed argument
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return uw_set_fan(0, argument);
		case W_UW_FANSPEED2:
			// Get fan speed argument
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return uw_set_fan(1, argument);
		case W_UW_MODE:
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return uw_ec_write_addr(0x51, 0x07, argument & 0xff, 0x00, &reg_write_return);
		case W_UW_MODE_ENABLE:
			// Note: Is for the moment set and cleared on init/exit of module (uniwill mode)
			/*
			if (copy_from_user(&argument, (int32_t *) arg, sizeof(argument))) {
				return -EFAULT;
			}
			return uw_ec_write_addr(0x41, 0x07, argument & 0x01, 0x00, &reg_write_return);
			*/
			return -EINVAL;
        case W_UW_FANAUTO:
			return uw_set_fan_auto();
#ifdef DEBUG
		case W_TF_BC:
			if (copy_from_user(&uw_arg, (void *) arg, sizeof(uw_arg))) {
				return -EFAULT;
			}
			if (uniwill_ec_direct) {
				if ((status = uw_ec_write_addr_direct(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], &reg_write_return))) {
					return status;
				}
				if (copy_to_user((void *) arg, &reg_write_return.dword, sizeof(reg_write_return.dword))) {
					return -EFAULT;
				}
			} else {
				if ((status = uw_wmi_ec_evaluate(uw_arg[0], uw_arg[1], uw_arg[2], uw_arg[3], 0, uw_result))) {
					return status;
				}
				if (copy_to_user((void *) arg, &uw_result, sizeof(uw_result))) {
					return -EFAULT;
				}
				reg_write_return.dword = uw_result[0];
			}
			/*pr_info("data_high %0#2x\n", reg_write_return.bytes.data_high);
			pr_info("data_low %0#2x\n", reg_write_return.bytes.data_low);
			pr_info("addr_high %0#2x\n", reg_write_return.bytes.addr_high);
			pr_info("addr_low %0#2x\n", reg_write_return.bytes.addr_low);*/
			return 0;
#endif
	}

	return -ENOTTY;
}

static long fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 status = -ENOTTY;

	const char *module_version = THIS_MODULE->version;
	switch (cmd) {
		case R_MOD_VERSION:
			return copy_to_user((char *) arg, module_version, strlen(module_version) + 1) ? -EFAULT : 0;
		// Hardware id checks, 1 = positive, 0 = negative
		case R_HWCHECK_CL:
			// Check should be performed again here as Clevo function availability may
			// change as clevo_acpi or clevo_wmi gets (un)loaded
			id_check_clevo = clevo_identify() == 0 ? 1 : 0;
			return copy_to_user((void *) arg, (void *) &id_check_clevo, sizeof(id_check_clevo)) ? -EFAULT : 0;
		case R_HWCHECK_UW:
			return copy_to_user((void *) arg, (void *) &id_check_uniwill, sizeof(id_check_uniwill)) ? -EFAULT : 0;
	}

	if (id_check_clevo == 1) {
		status = clevo_ioctl_interface(file, cmd, arg);
		if (status != -ENOTTY) {
			return status;
		}
	}

	if (id_check_uniwill == 1) {
		status = uniwill_ioctl_interface(file, cmd, arg);
		if (status != -ENOTTY) {
			return status;
		}
	}

	return status;
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
	id_check_clevo = clevo_identify() == 0 ? 1 : 0;
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
