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

#define pr_fmt(fmt) "tuxedo_keyboard" ": " fmt

#include "tuxedo_keyboard_common.h"
#include "clevo_keyboard.h"
#include "uniwill_keyboard.h"

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers keyboard & keyboard backlight Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.1.0");

MODULE_ALIAS("wmi:" CLEVO_EVENT_GUID);
MODULE_ALIAS("wmi:" CLEVO_GET_GUID);

MODULE_ALIAS("wmi:" UNIWILL_WMI_EVENT_GUID_0);
MODULE_ALIAS("wmi:" UNIWILL_WMI_EVENT_GUID_1);
MODULE_ALIAS("wmi:" UNIWILL_WMI_EVENT_GUID_2);

MODULE_SOFTDEP("pre: tuxedo-cc-wmi");

static struct tuxedo_keyboard_driver *driver_list[] = {
	&clevo_keyboard_driver,
	&uniwill_keyboard_driver
};

static int tuxedo_input_init(const struct key_entry key_map[])
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

	if (key_map != NULL) {
		err = sparse_keymap_setup(tuxedo_input_device, key_map, NULL);
		if (err) {
			TUXEDO_ERROR("Failed to setup sparse keymap\n");
			goto err_free_input_device;
		}
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
	int i, err;
	int num_drivers = sizeof(driver_list) / sizeof(*driver_list);
	TUXEDO_INFO("Model '%s' found\n",
		    dmi_get_system_info(DMI_PRODUCT_NAME));

	// Attempt to load each available driver
	// Associated probe decides if it fits
	// Driver from first successful probe is used

	i = 0;
	while (IS_ERR_OR_NULL(tuxedo_platform_device) && i < num_drivers) {
		current_driver = driver_list[i];
		tuxedo_platform_device = platform_create_bundle(
			current_driver->platform_driver,
			current_driver->probe, NULL, 0, NULL, 0);
		++i;
	}

	if (IS_ERR_OR_NULL(tuxedo_platform_device)) {
		TUXEDO_ERROR("No matching hardware found\n");
		return -ENODEV;
	}

	if (current_driver->key_map != NULL) {
		err = tuxedo_input_init(current_driver->key_map);
		if (unlikely(err)) {
			TUXEDO_ERROR("Could not register input device\n");
			current_driver->input_device = NULL;
		} else {
			current_driver->input_device = tuxedo_input_device;
		}
	}

	return 0;
}

static void __exit tuxdeo_keyboard_exit(void)
{
	tuxedo_input_exit();

	platform_device_unregister(tuxedo_platform_device);

	platform_driver_unregister(current_driver->platform_driver);

	TUXEDO_DEBUG("exit");
}

module_init(tuxdeo_keyboard_init);
module_exit(tuxdeo_keyboard_exit);
