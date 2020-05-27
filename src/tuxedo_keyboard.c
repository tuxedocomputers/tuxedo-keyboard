/*
* tuxedo_keyboard.c
*
* Copyright (C) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define DRIVER_NAME "tuxedo_keyboard"
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include "tuxedo_keyboard_common.h"
#include "clevo_keyboard.h"

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers keyboard & keyboard backlight Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.4");

MODULE_ALIAS("wmi:" CLEVO_EVENT_GUID);
MODULE_ALIAS("wmi:" CLEVO_GET_GUID);

static int __init tuxedo_input_init(void)
{
	int err;

	tuxedo_input_device = input_allocate_device();
	if (unlikely(!tuxedo_input_device)) {
		TUXEDO_ERROR("Error allocating input device\n");
		return -ENOMEM;
	}

	tuxedo_input_device->name = "TUXEDO Keyboard";
	tuxedo_input_device->phys = DRIVER_NAME "/input0";
	tuxedo_input_device->id.bustype = BUS_HOST;
	tuxedo_input_device->dev.parent = &tuxedo_platform_device->dev;

	err = sparse_keymap_setup(tuxedo_input_device, clevo_wmi_keymap, NULL);
	if (err) {
		TUXEDO_ERROR("Failed to setup sparse keymap\n");
		goto err_free_input_device;
	}

	err = input_register_device(tuxedo_input_device);
	if (unlikely(err)) {
		TUXEDO_ERROR("Error registering input device\n");
		goto err_free_input_device;
	}

	return 0;

err_free_input_device:
	input_free_device(tuxedo_input_device);

	return err;
}

static void __exit tuxedo_input_exit(void)
{
	if (unlikely(!tuxedo_input_device)) {
		return;
	}

	input_unregister_device(tuxedo_input_device);
	{
		tuxedo_input_device = NULL;
	}
}

static int __init tuxdeo_keyboard_init(void)
{
	int err;

	if (!wmi_has_guid(CLEVO_EVENT_GUID)) {
		TUXEDO_ERROR("No known WMI event notification GUID found\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(CLEVO_GET_GUID)) {
		TUXEDO_ERROR("No known WMI control method GUID found\n");
		return -ENODEV;
	}

	TUXEDO_INFO("Model '%s' found\n",
		    dmi_get_system_info(DMI_PRODUCT_NAME));

	tuxedo_platform_device =
	    platform_create_bundle(&platform_driver_clevo, tuxedo_wmi_probe,
				   NULL, 0, NULL, 0);

	if (unlikely(IS_ERR(tuxedo_platform_device))) {
		TUXEDO_ERROR("Can not init Platform driver");
		return PTR_ERR(tuxedo_platform_device);
	}

	err = tuxedo_input_init();
	if (unlikely(err)) {
		TUXEDO_ERROR("Could not register input device\n");
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_state) != 0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for state\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_left) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color left\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_center) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color center\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_right) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color right\n");
	}

	if (set_color(REGION_EXTRA, KB_COLOR_DEFAULT) != 0) {
		TUXEDO_DEBUG("Keyboard does not support EXTRA Color");
		kbd_led_state.has_extra = 0;
	} else {
		kbd_led_state.has_extra = 1;
		if (device_create_file
		    (&tuxedo_platform_device->dev,
		     &dev_attr_color_extra) != 0) {
			TUXEDO_ERROR
			    ("Sysfs attribute file creation failed for color extra\n");
		}

		set_color(REGION_EXTRA, param_color_extra);
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_extra) !=
	    0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for extra information flag\n");
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_mode) !=
	    0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for blinking pattern\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_brightness) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for brightness\n");
	}

	kbd_led_state.color.left = param_color_left;
	kbd_led_state.color.center = param_color_center;
	kbd_led_state.color.right = param_color_right;
	kbd_led_state.color.extra = param_color_extra;

	set_color(REGION_LEFT, param_color_left);
	set_color(REGION_CENTER, param_color_center);
	set_color(REGION_RIGHT, param_color_right);

	set_blinking_pattern(param_blinking_pattern);
	set_brightness(param_brightness);
	set_enabled(param_state);

	return 0;
}

static void __exit tuxdeo_keyboard_exit(void)
{
	tuxedo_input_exit();

	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_state);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_color_left);
	device_remove_file(&tuxedo_platform_device->dev,
			   &dev_attr_color_center);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_color_right);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_extra);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_mode);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_brightness);

	if (kbd_led_state.has_extra == 1) {
		device_remove_file(&tuxedo_platform_device->dev,
				   &dev_attr_color_extra);
	}

	platform_device_unregister(tuxedo_platform_device);

	platform_driver_unregister(&platform_driver_clevo);

	TUXEDO_DEBUG("exit");
}

module_init(tuxdeo_keyboard_init);
module_exit(tuxdeo_keyboard_exit);
