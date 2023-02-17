/*!
 * Copyright (c) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#define pr_fmt(fmt) "tuxedo_keyboard" ": " fmt

#include "tuxedo_keyboard_common.h"
#include "clevo_keyboard.h"
#include "uniwill_keyboard.h"
#include <linux/mutex.h>

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers keyboard & keyboard backlight Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.1.4");

static DEFINE_MUTEX(tuxedo_keyboard_init_driver_lock);

// static struct tuxedo_keyboard_driver *driver_list[] = { };

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

struct platform_device *tuxedo_keyboard_init_driver(struct tuxedo_keyboard_driver *tk_driver)
{
	int err;
	struct platform_device *new_platform_device = NULL;

	TUXEDO_DEBUG("init driver start\n");

	mutex_lock(&tuxedo_keyboard_init_driver_lock);

	if (!IS_ERR_OR_NULL(tuxedo_platform_device)) {
		// If already initialized, don't proceed
		TUXEDO_DEBUG("platform device already initialized\n");
		goto init_driver_exit;
	} else {
		// Otherwise, attempt to initialize structures
		TUXEDO_DEBUG("create platform bundle\n");
		new_platform_device = platform_create_bundle(
			tk_driver->platform_driver, tk_driver->probe, NULL, 0, NULL, 0);

		tuxedo_platform_device = new_platform_device;

		if (IS_ERR_OR_NULL(tuxedo_platform_device)) {
			// Normal case probe failed, no init
			goto init_driver_exit;
		}

		TUXEDO_DEBUG("initialize input device\n");
		if (tk_driver->key_map != NULL) {
			err = tuxedo_input_init(tk_driver->key_map);
			if (unlikely(err)) {
				TUXEDO_ERROR("Could not register input device\n");
				tk_driver->input_device = NULL;
			} else {
				TUXEDO_DEBUG("input device registered\n");
				tk_driver->input_device = tuxedo_input_device;
			}
		}

		current_driver = tk_driver;
	}

init_driver_exit:
	mutex_unlock(&tuxedo_keyboard_init_driver_lock);
	return new_platform_device;
}
EXPORT_SYMBOL(tuxedo_keyboard_init_driver);

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

void tuxedo_keyboard_remove_driver(struct tuxedo_keyboard_driver *tk_driver)
{
	bool specified_driver_differ_from_used =
		tk_driver != NULL && 
		(
			strcmp(
				tk_driver->platform_driver->driver.name,
				current_driver->platform_driver->driver.name
			) != 0
		);

	if (specified_driver_differ_from_used)
		return;

	TUXEDO_DEBUG("tuxedo_input_exit()\n");
	tuxedo_input_exit();
	TUXEDO_DEBUG("platform_device_unregister()\n");
	if (!IS_ERR_OR_NULL(tuxedo_platform_device)) {
		platform_device_unregister(tuxedo_platform_device);
		tuxedo_platform_device = NULL;
	}
	TUXEDO_DEBUG("platform_driver_unregister()\n");
	if (!IS_ERR_OR_NULL(current_driver)) {
		platform_driver_unregister(current_driver->platform_driver);
		current_driver = NULL;
	}

}
EXPORT_SYMBOL(tuxedo_keyboard_remove_driver);

static int __init tuxedo_keyboard_init(void)
{
	TUXEDO_INFO("module init\n");
	return 0;
}

static void __exit tuxedo_keyboard_exit(void)
{
	TUXEDO_INFO("module exit\n");

	if (tuxedo_platform_device != NULL)
		tuxedo_keyboard_remove_driver(NULL);
}

module_init(tuxedo_keyboard_init);
module_exit(tuxedo_keyboard_exit);
