/*!
 * Copyright (c) 2020-2021 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#ifndef UNIWILL_KEYBOARD_H
#define UNIWILL_KEYBOARD_H

#include "tuxedo_keyboard_common.h"
#include <linux/acpi.h>
#include <linux/wmi.h>
#include <linux/workqueue.h>
#include <linux/keyboard.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/led-class-multicolor.h>
#include <linux/string.h>
#include <linux/version.h>
#include "uniwill_interfaces.h"
#include "uniwill_leds.h"

#define UNIWILL_OSD_RADIOON			0x01A
#define UNIWILL_OSD_RADIOOFF			0x01B
#define UNIWILL_OSD_KB_LED_LEVEL0		0x03B
#define UNIWILL_OSD_KB_LED_LEVEL1		0x03C
#define UNIWILL_OSD_KB_LED_LEVEL2		0x03D
#define UNIWILL_OSD_KB_LED_LEVEL3		0x03E
#define UNIWILL_OSD_KB_LED_LEVEL4		0x03F
#define UNIWILL_OSD_DC_ADAPTER_CHANGE		0x0AB
#define UNIWILL_OSD_MODE_CHANGE_KEY_EVENT	0x0B0

#define UNIWILL_KEY_RFKILL			0x0A4
#define UNIWILL_KEY_KBDILLUMDOWN		0x0B1
#define UNIWILL_KEY_KBDILLUMUP			0x0B2
#define UNIWILL_KEY_KBDILLUMTOGGLE		0x0B9

#define UNIWILL_OSD_TOUCHPADWORKAROUND		0xFFF

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

static u8 uniwill_kbd_bl_enable_state_on_start;
static bool uniwill_kbd_bl_type_rgb_single_color = true;

static struct key_entry uniwill_wmi_keymap[] = {
	// { KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	// { KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	// { KE_KEY,	0xb0,				{ KEY_F13 } },
	// Manual mode rfkill
	{ KE_KEY,	UNIWILL_KEY_RFKILL,		{ KEY_RFKILL }},
	{ KE_KEY,	UNIWILL_OSD_TOUCHPADWORKAROUND,	{ KEY_F21 } },
	// Keyboard brightness
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMDOWN,	{ KEY_KBDILLUMDOWN } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMUP,		{ KEY_KBDILLUMUP } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMTOGGLE,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL0,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL1,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL2,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL3,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL4,	{ KEY_KBDILLUMTOGGLE } },
	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },
	{ KE_END,	0 }
};

static struct uniwill_interfaces_t {
	struct uniwill_interface_t *wmi;
} uniwill_interfaces = { .wmi = NULL };

uniwill_event_callb_t uniwill_event_callb;

int uniwill_read_ec_ram(u16 address, u8 *data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->read_ec_ram(address, data);
	else {
		pr_err("no active interface while read addr 0x%04x\n", address);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_read_ec_ram);

int uniwill_write_ec_ram(u16 address, u8 data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->write_ec_ram(address, data);
	else {
		pr_err("no active interface while write addr 0x%04x data 0x%02x\n", address, data);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_write_ec_ram);

static DEFINE_MUTEX(uniwill_interface_modification_lock);

int uniwill_add_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0)
		uniwill_interfaces.wmi = interface;
	else {
		TUXEDO_DEBUG("trying to add unknown interface\n");
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}
	interface->event_callb = uniwill_event_callb;

	mutex_unlock(&uniwill_interface_modification_lock);

	// Initialize driver if not already present
	tuxedo_keyboard_init_driver(&uniwill_keyboard_driver);

	return 0;
}
EXPORT_SYMBOL(uniwill_add_interface);

int uniwill_remove_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0) {
		// Remove driver if last interface is removed
		tuxedo_keyboard_remove_driver(&uniwill_keyboard_driver);

		uniwill_interfaces.wmi = NULL;
	} else {
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}

	mutex_unlock(&uniwill_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(uniwill_remove_interface);

int uniwill_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = uniwill_interfaces.wmi->string_id;

	return 0;
}
EXPORT_SYMBOL(uniwill_get_active_interface_id);

static void key_event_work(struct work_struct *work)
{
	sparse_keymap_report_known_event(
		uniwill_keyboard_driver.input_device,
		UNIWILL_OSD_TOUCHPADWORKAROUND,
		1,
		true
	);
}

// Previous key codes for detecting longer combination
static u32 prev_key = 0, prevprev_key = 0;
static DECLARE_WORK(uniwill_key_event_work, key_event_work);

static int keyboard_notifier_callb(struct notifier_block *nb, unsigned long code, void *_param)
{
	struct keyboard_notifier_param *param = _param;
	int ret = NOTIFY_OK;

	if (!param->down) {

		if (code == KBD_KEYCODE) {
			switch (param->value) {
			case 125:
				// If the last keys up were 85 -> 29 -> 125
				// manually report KEY_F21
				if (prevprev_key == 85 && prev_key == 29) {
					TUXEDO_DEBUG("Touchpad Toggle\n");
					schedule_work(&uniwill_key_event_work);
					ret = NOTIFY_OK;
				}
				break;
			}
			prevprev_key = prev_key;
			prev_key = param->value;
		}
	}

	return ret;
}

static struct notifier_block keyboard_notifier_block = {
	.notifier_call = keyboard_notifier_callb
};

static void uniwill_write_kbd_bl_enable(u8 enable)
{
	u8 backlight_data;
	enable = enable & 0x01;

	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &backlight_data);
	backlight_data = backlight_data & ~(1 << 1);
	backlight_data |= (!enable << 1);
	uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, backlight_data);
}

void uniwill_event_callb(u32 code)
{
	if (uniwill_keyboard_driver.input_device != NULL)
		if (!sparse_keymap_report_known_event(uniwill_keyboard_driver.input_device, code, 1, true)) {
			TUXEDO_DEBUG("Unknown code - %d (%0#6x)\n", code, code);
		}

	// Special key combination when mode change key is pressed
	if (code == UNIWILL_OSD_MODE_CHANGE_KEY_EVENT) {
		input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 1);
		input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 1);
		input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 1);
		input_sync(uniwill_keyboard_driver.input_device);
		input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 0);
		input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 0);
		input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 0);
		input_sync(uniwill_keyboard_driver.input_device);
	}

	// Refresh keyboard state on cable switch event
	if (code == UNIWILL_OSD_DC_ADAPTER_CHANGE) {
		uniwill_leds_restore_state_extern();
	}
}

static void uw_kbd_bl_init_set(struct platform_device *dev)
{
	uniwill_leds_init_late(dev);
	uniwill_write_kbd_bl_enable(1);
}

// Keep track of previous colors on start, init array with different non-colors
static u32 uw_prev_colors[] = {0x01000000, 0x02000000, 0x03000000};
static u32 uw_prev_colors_size = 3;
static u32 uw_prev_colors_index = 0;

// Timer for checking animation colors
static struct timer_list uw_kbd_bl_init_timer;
static volatile int uw_kbd_bl_check_count = 40;
static int uw_kbd_bl_init_check_interval_ms = 500;

static int uniwill_read_kbd_bl_rgb(u8 *red, u8 *green, u8 *blue)
{
	int result = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_RED_BRIGHTNESS, red);
	if (result) {
		return result;
	}
	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_GREEN_BRIGHTNESS, green);
	if (result) {
		return result;
	}
	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS, blue);
	if (result) {
		return result;
	}

	return result;
}

static struct platform_device *uw_kbd_bl_init_ready_check_work_func_args_dev;

static void uw_kbd_bl_init_ready_check_work_func(struct work_struct *work)
{
	u8 uw_cur_red, uw_cur_green, uw_cur_blue;
	int i;
	bool prev_colors_same;
	uniwill_read_kbd_bl_rgb(&uw_cur_red, &uw_cur_green, &uw_cur_blue);
	uw_prev_colors[uw_prev_colors_index] = (uw_cur_red << 0x10) | (uw_cur_green << 0x08) | uw_cur_blue;
	uw_prev_colors_index = (uw_prev_colors_index + 1) % uw_prev_colors_size;

	prev_colors_same = true;
	for (i = 1; i < uw_prev_colors_size; ++i) {
		if (uw_prev_colors[i-1] != uw_prev_colors[i]) prev_colors_same = false;
	}

	if (prev_colors_same) {
		uw_kbd_bl_init_set(uw_kbd_bl_init_ready_check_work_func_args_dev);
		del_timer(&uw_kbd_bl_init_timer);
	} else {
		if (uw_kbd_bl_check_count != 0) {
			mod_timer(&uw_kbd_bl_init_timer, jiffies + msecs_to_jiffies(uw_kbd_bl_init_check_interval_ms));
		} else {
			TUXEDO_INFO("uw kbd init timeout, failed to detect end of boot animation\n");
			del_timer(&uw_kbd_bl_init_timer);
		}
	}

	uw_kbd_bl_check_count -= 1;
}

static DECLARE_WORK(uw_kbd_bl_init_ready_check_work, uw_kbd_bl_init_ready_check_work_func);

static void uw_kbd_bl_init_ready_check(struct timer_list *t)
{
	schedule_work(&uw_kbd_bl_init_ready_check_work);
}

static int uw_kbd_bl_init(struct platform_device *dev)
{
	int status = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
	TUXEDO_ERROR("Warning: Kernel version less that 4.18, keyboard backlight might not be properly recognized.");
#endif

	uniwill_leds_init_early(dev);

	if (uniwill_kbd_bl_type_rgb_single_color) {
		// Start periodic checking of animation, set and enable bl when done
		uw_kbd_bl_init_ready_check_work_func_args_dev = dev;
		timer_setup(&uw_kbd_bl_init_timer, uw_kbd_bl_init_ready_check, 0);
		mod_timer(&uw_kbd_bl_init_timer, jiffies + msecs_to_jiffies(uw_kbd_bl_init_check_interval_ms));
	} else {
		// For non-RGB versions
		// Enable keyboard backlight immediately (should it be disabled)
		uniwill_write_kbd_bl_enable(1);
	}

	return status;
}

#define UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS	0x24
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_RED	"lightbar_rgb:1:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN	"lightbar_rgb:2:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE	"lightbar_rgb:3:status"
#define UNIWILL_LIGHTBAR_LED_NAME_ANIMATION	"lightbar_animation::status"

static void uniwill_write_lightbar_rgb(u8 red, u8 green, u8 blue)
{
	if (red <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x0749, red);
	}
	if (green <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074a, green);
	}
	if (blue <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074b, blue);
	}
}

static void uniwill_read_lightbar_rgb(u8 *red, u8 *green, u8 *blue)
{
	uniwill_read_ec_ram(0x0749, red);
	uniwill_read_ec_ram(0x074a, green);
	uniwill_read_ec_ram(0x074b, blue);
}

static void uniwill_write_lightbar_animation(bool animation_status)
{
	u8 value;

	uniwill_read_ec_ram(0x0748, &value);
	if (animation_status) {
		value |= 0x80;
	} else {
		value &= ~0x80;
	}
	uniwill_write_ec_ram(0x0748, value);
}

static void uniwill_read_lightbar_animation(bool *animation_status)
{
	u8 lightbar_animation_data;
	uniwill_read_ec_ram(0x0748, &lightbar_animation_data);
	*animation_status = (lightbar_animation_data & 0x80) > 0;
}

static int lightbar_set_blocking(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	u8 red = 0xff, green = 0xff, blue = 0xff;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		if (led_red) {
			red = brightness;
		} else if (led_green) {
			green = brightness;
		} else if (led_blue) {
			blue = brightness;
		}
		uniwill_write_lightbar_rgb(red, green, blue);
		// Also make sure the animation is off
		uniwill_write_lightbar_animation(false);
	} else if (led_animation) {
		if (brightness == 1) {
			uniwill_write_lightbar_animation(true);
		} else {
			uniwill_write_lightbar_animation(false);
		}
	}
	return 0;
}

static enum led_brightness lightbar_get(struct led_classdev *led_cdev)
{
	u8 red, green, blue;
	bool animation_status;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		uniwill_read_lightbar_rgb(&red, &green, &blue);
		if (led_red) {
			return red;
		} else if (led_green) {
			return green;
		} else if (led_blue) {
			return blue;
		}
	} else if (led_animation) {
		uniwill_read_lightbar_animation(&animation_status);
		return animation_status ? 1 : 0;
	}

	return 0;
}

static bool uw_lightbar_loaded;
static struct led_classdev lightbar_led_classdevs[] = {
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_RED,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_ANIMATION,
		.max_brightness = 1,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	}
};

static int uw_lightbar_init(struct platform_device *dev)
{
	int i, j, status;

	bool lightbar_supported = false
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1501I")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1701I")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")

#endif
		;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
	TUXEDO_ERROR(
		"Warning: Kernel version less that 4.18, lightbar might not be properly recognized.");
#endif

	if (!lightbar_supported)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		status = led_classdev_register(&dev->dev, &lightbar_led_classdevs[i]);
		if (status < 0) {
			for (j = 0; j < i; j++)
				led_classdev_unregister(&lightbar_led_classdevs[j]);
			return status;
		}
	}

	// Init default state
	uniwill_write_lightbar_animation(false);
	uniwill_write_lightbar_rgb(0, 0, 0);

	return 0;
}

static int uw_lightbar_remove(struct platform_device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		led_classdev_unregister(&lightbar_led_classdevs[i]);
	}
	return 0;
}

static int uniwill_keyboard_probe(struct platform_device *dev)
{
	u32 i;
	u8 data;
	int status;

	// FIXME Hard set balanced profile until we have implemented a way to
	// switch it while tuxedo_io is loaded
	// uw_ec_write_addr(0x51, 0x07, 0x00, 0x00, &reg_write_return);
	uniwill_write_ec_ram(0x0751, 0x00);

	// Set manual-mode fan-curve in 0x0743 - 0x0747
	// Some kind of default fan-curve is stored in 0x0786 - 0x078a: Using it to initialize manual-mode fan-curve
	for (i = 0; i < 5; ++i) {
		uniwill_read_ec_ram(0x0786 + i, &data);
		uniwill_write_ec_ram(0x0743 + i, data);
	}

	// Enable manual mode
	uniwill_write_ec_ram(0x0741, 0x01);

	// Zero second fan temp for detection
	uniwill_write_ec_ram(0x044f, 0x00);

	status = register_keyboard_notifier(&keyboard_notifier_block);

	uw_kbd_bl_init(dev);

	status = uw_lightbar_init(dev);
	uw_lightbar_loaded = (status >= 0);

	return 0;
}

static int uniwill_keyboard_remove(struct platform_device *dev)
{
	uniwill_leds_remove(dev);

	// Restore previous backlight enable state
	if (uniwill_kbd_bl_enable_state_on_start != 0xff) {
		uniwill_write_kbd_bl_enable(uniwill_kbd_bl_enable_state_on_start);
	}

	unregister_keyboard_notifier(&keyboard_notifier_block);

	del_timer(&uw_kbd_bl_init_timer);

	if (uw_lightbar_loaded)
		uw_lightbar_remove(dev);

	// Disable manual mode
	uniwill_write_ec_ram(0x0741, 0x00);

	return 0;
}

static int uniwill_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	uniwill_write_kbd_bl_enable(0);
	return 0;
}

static int uniwill_keyboard_resume(struct platform_device *dev)
{
	uniwill_leds_restore_state_extern();
	uniwill_write_kbd_bl_enable(1);
	return 0;
}

static struct platform_driver platform_driver_uniwill = {
	.remove = uniwill_keyboard_remove,
	.suspend = uniwill_keyboard_suspend,
	.resume = uniwill_keyboard_resume,
	.driver =
		{
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
		},
};

struct tuxedo_keyboard_driver uniwill_keyboard_driver = {
	.platform_driver = &platform_driver_uniwill,
	.probe = uniwill_keyboard_probe,
	.key_map = uniwill_wmi_keymap,
};

#endif // UNIWILL_KEYBOARD_H
