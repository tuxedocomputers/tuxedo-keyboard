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

#ifndef UNIWILL_LEDS_H
#define UNIWILL_LEDS_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/completion.h>

enum uniwill_kb_backlight_types {
	UNIWILL_KB_BACKLIGHT_TYPE_NONE,
	UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR,
	UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB,
	UNIWILL_KB_BACKLIGHT_TYPE_PER_KEY_RGB
};

#define UNIWILL_KBD_BRIGHTNESS_MAX		0xff
#define UNIWILL_KBD_BRIGHTNESS_DEFAULT		0x00

#define UNIWILL_KBD_BRIGHTNESS_WHITE_MAX	0x02
#define UNIWILL_KBD_BRIGHTNESS_WHITE_DEFAULT	0x00

#define UNIWILL_KB_COLOR_DEFAULT_RED		0xff
#define UNIWILL_KB_COLOR_DEFAULT_GREEN		0xff
#define UNIWILL_KB_COLOR_DEFAULT_BLUE		0xff
#define UNIWILL_KB_COLOR_DEFAULT		((UNIWILL_KB_COLOR_DEFAULT_RED << 16) + (UNIWILL_KB_COLOR_DEFAULT_GREEN << 8) + UNIWILL_KB_COLOR_DEFAULT_BLUE)

int uniwill_leds_init_early(struct platform_device *dev);
int uniwill_leds_init_late(struct platform_device *dev);
int uniwill_leds_remove(struct platform_device *dev);
enum uniwill_kb_backlight_types uniwill_leds_get_backlight_type(void);
int uniwill_leds_notify_brightness_change_extern(void);
void uniwill_leds_restore_state_extern(void);
void uniwill_leds_set_brightness_extern(enum led_brightness brightness);
void uniwill_leds_set_color_extern(u32 color);

// TODO The following should go into a seperate .c file, but for this to work more reworking is required in the tuxedo_keyboard structure.

#include "uniwill_leds.h"

#include "uniwill_interfaces.h"

#include <linux/led-class-multicolor.h>

static enum uniwill_kb_backlight_types uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_NONE;
static bool uw_leds_initialized = false;

static int uniwill_write_kbd_bl_white(u8 brightness)
{
	u8 data;

	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS, &data);
	// When keyboard backlight  is off, new settings to 0x078c do not get applied automatically
	// on Pulse Gen1/2 until next keypress or manual change to 0x1808 (immediate brightness
	// value for some reason.
	// Sidenote: IBP Gen6/7 has immediate brightness value on 0x1802 and not on 0x1808, but does
	// not need this workaround.
	if (!data && brightness) {
		uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS, 0x01);
	}

	data = 0;
	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
	data &= 0x0f; // lower bits must be preserved
	data |= UW_EC_REG_KBD_BL_STATUS_SUBCMD_RESET;
	data |= brightness << 5;
	return uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, data);
}

static int uniwill_write_kbd_bl_rgb(u8 red, u8 green, u8 blue)
{
	int result = 0;

	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_RED_BRIGHTNESS, red);
	if (result) {
		return result;
	}
	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_GREEN_BRIGHTNESS, green);
	if (result) {
		return result;
	}
	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS, blue);
	if (result) {
		return result;
	}

	pr_debug("Wrote kbd color [%0#4x, %0#4x, %0#4x]\n", red, green, blue);

	return result;
}

static void uniwill_leds_set_brightness(struct led_classdev *led_cdev __always_unused, enum led_brightness brightness) {
	int ret = uniwill_write_kbd_bl_white(brightness);
	if (ret) {
		pr_debug("uniwill_leds_set_brightness(): uniwill_write_kbd_bl_white() failed\n");
		return;
	}
	led_cdev->brightness = brightness;
}

static void uniwill_leds_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int ret;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);

	led_mc_calc_color_components(mcled_cdev, brightness);

	ret = uniwill_write_kbd_bl_rgb(mcled_cdev->subled_info[0].brightness,
				       mcled_cdev->subled_info[1].brightness,
				       mcled_cdev->subled_info[2].brightness);
	if (ret) {
		pr_debug("uniwill_leds_set_brightness_mc(): uniwill_write_kbd_bl_rgb() failed\n");
		return;
	}
	led_cdev->brightness = brightness;
}

static struct led_classdev uniwill_led_cdev = {
	.name = "white:" LED_FUNCTION_KBD_BACKLIGHT,
	.max_brightness = UNIWILL_KBD_BRIGHTNESS_WHITE_MAX,
	.brightness_set = &uniwill_leds_set_brightness,
	.brightness = UNIWILL_KBD_BRIGHTNESS_WHITE_DEFAULT,
	.flags = LED_BRIGHT_HW_CHANGED
};

static struct mc_subled uw_mcled_cdev_subleds[3] = {
	{
		.color_index = LED_COLOR_ID_RED,
		.brightness = UNIWILL_KBD_BRIGHTNESS_MAX,
		.intensity = UNIWILL_KB_COLOR_DEFAULT_RED,
		.channel = 0
	},
	{
		.color_index = LED_COLOR_ID_GREEN,
		.brightness = UNIWILL_KBD_BRIGHTNESS_MAX,
		.intensity = UNIWILL_KB_COLOR_DEFAULT_GREEN,
		.channel = 0
	},
	{
		.color_index = LED_COLOR_ID_BLUE,
		.brightness = UNIWILL_KBD_BRIGHTNESS_MAX,
		.intensity = UNIWILL_KB_COLOR_DEFAULT_BLUE,
		.channel = 0
	}
};

static struct led_classdev_mc uniwill_mcled_cdev = {
	.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT,
	.led_cdev.max_brightness = UNIWILL_KBD_BRIGHTNESS_MAX,
	.led_cdev.brightness_set = &uniwill_leds_set_brightness_mc,
	.led_cdev.brightness = UNIWILL_KBD_BRIGHTNESS_DEFAULT,
	.led_cdev.flags = LED_BRIGHT_HW_CHANGED,
	.num_colors = 3,
	.subled_info = uw_mcled_cdev_subleds
};

int uniwill_leds_init_early(struct platform_device *dev)
{
	// FIXME Use mutexes
	int ret;
	u8 data;

	ret = uniwill_read_ec_ram(UW_EC_REG_BAREBONE_ID, &data);
	if (ret) {
		pr_err("Reading barebone ID failed.\n");
		return ret;
	}
	pr_debug("EC Barebone ID: %#04x\n", data);

	if (data == UW_EC_REG_BAREBONE_ID_VALUE_PFxxxxx ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PFxMxxx ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH4TRX1 ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH4TUX1 ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH4TQx1 ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH6TRX1 ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH6TQxx ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH4Axxx ||
	    data == UW_EC_REG_BAREBONE_ID_VALUE_PH4Pxxx) {
		ret = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
		if (ret) {
			pr_err("Reading keyboard backlight status failed.\n");
			return ret;
		}
		if (data & UW_EC_REG_KBD_BL_STATUS_BIT_WHITE_ONLY_KB) {
			uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR;
		}
	}
	else {
		ret = uniwill_read_ec_ram(UW_EC_REG_FEATURES_1, &data);
		if (ret) {
			pr_err("Reading features 1 failed.\n");
			return ret;
		}
		if (data & UW_EC_REG_FEATURES_1_BIT_1_ZONE_RGB_KB) {
			uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB;
		}
	}
	pr_debug("Keyboard backlight type: 0x%02x\n", uniwill_kb_backlight_type);

	if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		pr_debug("Registering fixed color leds interface\n");
		ret = led_classdev_register(&dev->dev, &uniwill_led_cdev);
		if (ret) {
			pr_err("Registering fixed color leds interface failed\n");
			return ret;
		}
	}
	else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		pr_debug("Registering single zone rgb leds interface\n");
		ret = devm_led_classdev_multicolor_register(&dev->dev, &uniwill_mcled_cdev);
		if (ret) {
			pr_err("Registering single zone rgb leds interface failed\n");
			return ret;
		}
	}

	uw_leds_initialized = true;
	return 0;
}
EXPORT_SYMBOL(uniwill_leds_init_early);

int uniwill_leds_init_late(struct platform_device *dev)
{
	// FIXME Use mutexes
	int ret;

	ret = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_MAX_BRIGHTNESS, 0xff);
	if (ret) {
		pr_err("Setting max keyboard brightness value failed\n");
		uniwill_leds_remove(dev);
		return ret;
	}

	uniwill_leds_restore_state_extern();

	return 0;
}
EXPORT_SYMBOL(uniwill_leds_init_late);

int uniwill_leds_remove(struct platform_device *dev)
{
	// FIXME Use mutexes
	int ret = 0;

	if (uw_leds_initialized) {
		uw_leds_initialized = false;

		uniwill_leds_set_brightness_extern(0x00);
		ret = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_MAX_BRIGHTNESS, 0xc8);
		if (ret) {
			pr_err("Resetting max keyboard brightness value failed\n");
		}

		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			led_classdev_unregister(&uniwill_led_cdev);
		}
		else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			devm_led_classdev_multicolor_unregister(&dev->dev, &uniwill_mcled_cdev);
		}
	}

	return ret;
}
EXPORT_SYMBOL(uniwill_leds_remove);

enum uniwill_kb_backlight_types uniwill_leds_get_backlight_type(void) {
	return uniwill_kb_backlight_type;
}
EXPORT_SYMBOL(uniwill_leds_get_backlight_type);

int uniwill_leds_notify_brightness_change_extern(void) {
	u8 data = 0;

	if (uw_leds_initialized) {
		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
			data = (data >> 5) & 0x3;
			uniwill_led_cdev.brightness = data;
			led_classdev_notify_brightness_hw_changed(&uniwill_led_cdev, data);
			return true;
		}
	}
	return false;
}

void uniwill_leds_restore_state_extern(void) {
	u8 data;

	if (uw_leds_initialized) {
		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			if (uniwill_write_kbd_bl_white(uniwill_led_cdev.brightness)) {
				pr_debug("uniwill_leds_restore_state_extern(): uniwill_write_kbd_bl_white() failed\n");
			}
		}
		else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			// reset
			uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
			data |= UW_EC_REG_KBD_BL_STATUS_SUBCMD_RESET;
			uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, data);

			// write
			if (uniwill_write_kbd_bl_rgb(uniwill_mcled_cdev.subled_info[0].brightness,
						     uniwill_mcled_cdev.subled_info[1].brightness,
						     uniwill_mcled_cdev.subled_info[2].brightness)) {
				pr_debug("uniwill_leds_restore_state_extern(): uniwill_write_kbd_bl_rgb() failed\n");
			}
		}
	}
}
EXPORT_SYMBOL(uniwill_leds_restore_state_extern);

void uniwill_leds_set_brightness_extern(enum led_brightness brightness) {
	if (uw_leds_initialized) {
		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			uniwill_led_cdev.brightness_set(&uniwill_led_cdev, brightness);
		}
		else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			uniwill_mcled_cdev.led_cdev.brightness_set(&uniwill_mcled_cdev.led_cdev, brightness);
		}
	}
}
EXPORT_SYMBOL(uniwill_leds_set_brightness_extern);

void uniwill_leds_set_color_extern(u32 color) {
	if (uw_leds_initialized) {
		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			uniwill_mcled_cdev.subled_info[0].intensity = (color >> 16) & 0xff;
			uniwill_mcled_cdev.subled_info[1].intensity = (color >> 8) & 0xff;
			uniwill_mcled_cdev.subled_info[2].intensity = color & 0xff;
			uniwill_mcled_cdev.led_cdev.brightness_set(&uniwill_mcled_cdev.led_cdev, uniwill_mcled_cdev.led_cdev.brightness);
		}
	}
}
EXPORT_SYMBOL(uniwill_leds_set_color_extern);

MODULE_LICENSE("GPL");

#endif // UNIWILL_LEDS_H
