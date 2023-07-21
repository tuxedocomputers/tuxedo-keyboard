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

#ifndef CLEVO_LEDS_H
#define CLEVO_LEDS_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

enum clevo_kb_backlight_types {
	CLEVO_KB_BACKLIGHT_TYPE_NONE = 0x00,
	CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR = 0x01,
	CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB = 0x02,
	CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB = 0x06,
	CLEVO_KB_BACKLIGHT_TYPE_PER_KEY_RGB = 0xf3
};

int clevo_leds_init(struct platform_device *dev);
int clevo_leds_remove(struct platform_device *dev);
int clevo_leds_suspend(struct platform_device *dev);
int clevo_leds_resume(struct platform_device *dev);
enum clevo_kb_backlight_types clevo_leds_get_backlight_type(void);
void clevo_leds_restore_state_extern(void);
void clevo_leds_notify_brightness_change_extern(void);
void clevo_leds_set_brightness_extern(enum led_brightness brightness);
void clevo_leds_set_color_extern(u32 color);

// TODO The following should go into a seperate .c file, but for this to work more reworking is required in the tuxedo_keyboard structure.

#include "clevo_leds.h"

#include "clevo_interfaces.h"

#include <linux/led-class-multicolor.h>
#include <linux/delay.h>

#define CLEVO_KBD_BRIGHTNESS_MAX			0xff
#define CLEVO_KBD_BRIGHTNESS_DEFAULT			0x00

#define CLEVO_KBD_BRIGHTNESS_WHITE_MAX			0x02 // White only keyboards can only be off, half, or full brightness
#define CLEVO_KBD_BRIGHTNESS_WHITE_DEFAULT		0x00

#define CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5		0x05 // Devices <= Intel 7th gen had a different white control with 5 brightness values + off
#define CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5_DEFAULT	0x00

#define CLEVO_KB_COLOR_DEFAULT_RED			0xff
#define CLEVO_KB_COLOR_DEFAULT_GREEN			0xff
#define CLEVO_KB_COLOR_DEFAULT_BLUE			0xff
#define CLEVO_KB_COLOR_DEFAULT				((CLEVO_KB_COLOR_DEFAULT_RED << 16) + (CLEVO_KB_COLOR_DEFAULT_GREEN << 8) + CLEVO_KB_COLOR_DEFAULT_BLUE)

static enum clevo_kb_backlight_types clevo_kb_backlight_type = CLEVO_KB_BACKLIGHT_TYPE_NONE;
static bool leds_initialized = false;

/**
 * Color scaling quirk list
 */
static void color_scaling(enum clevo_kb_backlight_types *type, u8 *red, u8 *green, u8 *blue)
{
	if (*type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		*red = (180 * *red) / 255;
		*blue = (200 * *blue) / 255;
	}
}

static int clevo_evaluate_set_white_brightness(u8 brightness)
{
	pr_debug("Set white brightness to %d\n", brightness);

	return clevo_evaluate_method (CLEVO_CMD_SET_KB_WHITE_LEDS, brightness, NULL);
}

static int clevo_evaluate_set_rgb_brightness(u8 brightness)
{
	pr_debug("Set RGB brightness to %d\n", brightness);

	return clevo_evaluate_method (CLEVO_CMD_SET_KB_RGB_LEDS, CLEVO_CMD_SET_KB_LEDS_SUB_RGB_BRIGHTNESS | brightness, NULL);
}

static int clevo_evaluate_set_rgb_color(u32 zone, u32 color)
{
	u32 cset = ((color & 0x0000FF) << 16) | ((color & 0xFF0000) >> 8) | ((color & 0x00FF00) >> 8);
	u32 clevo_submethod_arg = zone | cset;

	pr_debug("Set Color 0x%08x for region 0x%08x\n", color, zone);

	return clevo_evaluate_method(CLEVO_CMD_SET_KB_RGB_LEDS, clevo_submethod_arg, NULL);
}

static int clevo_evaluate_set_keyboard_status(u8 state)
{
	u32 cmd = 0xE0000000;
	TUXEDO_INFO("Set keyboard enabled to: %d\n", state);

	if (state == 0) {
		cmd |= 0x003001;
	} else {
		cmd |= 0x07F001;
	}

	return clevo_evaluate_method(CLEVO_CMD_SET_KB_RGB_LEDS, cmd, NULL);
}

static void clevo_leds_set_brightness(struct led_classdev *led_cdev __always_unused, enum led_brightness brightness) {
	int ret = clevo_evaluate_set_white_brightness(brightness);
	if (ret) {
		pr_debug("clevo_leds_set_brightness(): clevo_evaluate_set_white_brightness() failed\n");
		return;
	}
	led_cdev->brightness = brightness;
}

/*static void clevo_leds_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int ret;
	u32 zone, color;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);

	ret = clevo_evaluate_set_rgb_brightness(CLEVO_KBD_BRIGHTNESS_MAX);
	if (ret) {
		pr_debug("clevo_leds_set_brightness_mc(): clevo_evaluate_set_rgb_brightness() failed\n");
		return;
	}

	zone = mcled_cdev->subled_info[0].channel;

	led_mc_calc_color_components(mcled_cdev, brightness);
	color = (mcled_cdev->subled_info[0].brightness << 16) +
		(mcled_cdev->subled_info[1].brightness << 8) +
		mcled_cdev->subled_info[2].brightness;

	ret = clevo_evaluate_set_rgb_color(zone, color);
	if (ret) {
		pr_debug("clevo_leds_set_brightness_mc(): clevo_evaluate_set_rgb_color() failed\n");
		return;
	}
	led_cdev->brightness = brightness;
}*/

// Temprary fix for KDE: KDE does only set one kbd_backlight brightness value, this version of the
// function uses clevos built in brightness setting to set the whole keyboard brightness at once.
// -> use clevo_evaluate_set_rgb_brightness() to set overall brightness via firmware instead of scaling
//    the RGB values
// -> update all clevo_mcled_cdevs brightness levels to refect that the firmware method sets the
//    the whole keyboard brightness and not just one zone
// This is a temporary fix until KDE handles multiple keyboard backlights correctly
static struct led_classdev_mc clevo_mcled_cdevs[3]; // forward declaration
static void clevo_leds_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int ret;
	u32 zone, color;
	u8 red, green, blue;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);

	ret = clevo_evaluate_set_rgb_brightness(brightness);
	if (ret) {
		pr_debug("clevo_leds_set_brightness_mc(): clevo_evaluate_set_rgb_brightness() failed\n");
		return;
	}
	clevo_mcled_cdevs[0].led_cdev.brightness = brightness;
	clevo_mcled_cdevs[1].led_cdev.brightness = brightness;
	clevo_mcled_cdevs[2].led_cdev.brightness = brightness;

	zone = mcled_cdev->subled_info[0].channel;

	red = mcled_cdev->subled_info[0].intensity;
	green = mcled_cdev->subled_info[1].intensity;
	blue = mcled_cdev->subled_info[2].intensity;

	color_scaling(&clevo_kb_backlight_type, &red, &green, &blue);

	color = (red << 16) +
		(green << 8) +
		blue;

	ret = clevo_evaluate_set_rgb_color(zone, color);
	if (ret) {
		pr_debug("clevo_leds_set_brightness_mc(): clevo_evaluate_set_rgb_color() failed\n");
	}
}

static struct led_classdev clevo_led_cdev = {
	.name = "white:" LED_FUNCTION_KBD_BACKLIGHT,
	.max_brightness = CLEVO_KBD_BRIGHTNESS_WHITE_MAX,
	.brightness_set = &clevo_leds_set_brightness,
	.brightness = CLEVO_KBD_BRIGHTNESS_WHITE_DEFAULT,
	.flags = LED_BRIGHT_HW_CHANGED
};

static struct mc_subled clevo_mcled_cdevs_subleds[3][3] = {
	{
		{
			.color_index = LED_COLOR_ID_RED,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_RED,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_0
		},
		{
			.color_index = LED_COLOR_ID_GREEN,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_GREEN,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_0
		},
		{
			.color_index = LED_COLOR_ID_BLUE,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_BLUE,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_0
		}
	},
	{
		{
			.color_index = LED_COLOR_ID_RED,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_RED,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_1
		},
		{
			.color_index = LED_COLOR_ID_GREEN,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_GREEN,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_1
		},
		{
			.color_index = LED_COLOR_ID_BLUE,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_BLUE,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_1
		}
	},
	{
		{
			.color_index = LED_COLOR_ID_RED,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_RED,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_2
		},
		{
			.color_index = LED_COLOR_ID_GREEN,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_GREEN,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_2
		},
		{
			.color_index = LED_COLOR_ID_BLUE,
			.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
			.intensity = CLEVO_KB_COLOR_DEFAULT_BLUE,
			.channel = CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_2
		}
	}
};

static struct led_classdev_mc clevo_mcled_cdevs[3] = {
	{
		.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT,
		.led_cdev.max_brightness = CLEVO_KBD_BRIGHTNESS_MAX,
		.led_cdev.brightness_set = &clevo_leds_set_brightness_mc,
		.led_cdev.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
		.num_colors = 3,
		.subled_info = clevo_mcled_cdevs_subleds[0]
	},
	{
		.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT,
		.led_cdev.max_brightness = CLEVO_KBD_BRIGHTNESS_MAX,
		.led_cdev.brightness_set = &clevo_leds_set_brightness_mc,
		.led_cdev.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
		.num_colors = 3,
		.subled_info = clevo_mcled_cdevs_subleds[1]
	},
	{
		.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT,
		.led_cdev.max_brightness = CLEVO_KBD_BRIGHTNESS_MAX,
		.led_cdev.brightness_set = &clevo_leds_set_brightness_mc,
		.led_cdev.brightness = CLEVO_KBD_BRIGHTNESS_DEFAULT,
		.num_colors = 3,
		.subled_info = clevo_mcled_cdevs_subleds[2]
	}
};

int clevo_leds_init(struct platform_device *dev)
{
	int ret, i;
	int status;
	union acpi_object *result;
	u32 result_fallback;

	for (i = 0; i < 3; ++i) {
		status = clevo_evaluate_method2(CLEVO_CMD_GET_SPECS, 0, &result);
		if (!status) {
			if (result->type == ACPI_TYPE_BUFFER) {
				pr_debug("CLEVO_CMD_GET_SPECS result->buffer.pointer[0x0f]: 0x%02x\n", result->buffer.pointer[0x0f]);
				clevo_kb_backlight_type = result->buffer.pointer[0x0f];
				if (clevo_kb_backlight_type) {
					status = clevo_evaluate_method(CLEVO_CMD_GET_BIOS_FEATURES_2, 0, &result_fallback);
					if (!status) {
						pr_debug("CLEVO_CMD_GET_BIOS_FEATURES_2 result_fallback: 0x%08x\n", result_fallback);
						if (result_fallback & CLEVO_CMD_GET_BIOS_FEATURES_2_SUB_WHITE_ONLY_KB_MAX_5) {
							clevo_led_cdev.max_brightness = CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5;
							clevo_led_cdev.brightness = CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5_DEFAULT;
						}
					}
					break;
				} else {
					pr_debug("clevo_kb_backlight_type 0x00 probably wrong, retrying...\n");
					msleep(50);
				}
			}
			else {
				pr_err("CLEVO_CMD_GET_SPECS does not exist on this device or return value has wrong type, trying CLEVO_CMD_GET_BIOS_FEATURES\n");
				status = -EINVAL;
			}
			ACPI_FREE(result);
		}
		else {
			pr_notice("CLEVO_CMD_GET_SPECS does not exist on this device or failed, trying CLEVO_CMD_GET_BIOS_FEATURES_1\n");
		}
	}

	if (status || clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_NONE) {
		// check for devices <= Intel 8th gen (only white only, 3 zone RGB, or no backlight on these devices)
		status = clevo_evaluate_method(CLEVO_CMD_GET_BIOS_FEATURES_1, 0, &result_fallback);
		if (!status) {
			pr_debug("CLEVO_CMD_GET_BIOS_FEATURES_1 result_fallback: 0x%08x\n", result_fallback);
			if (result_fallback & CLEVO_CMD_GET_BIOS_FEATURES_1_SUB_3_ZONE_RGB_KB) {
				clevo_kb_backlight_type = CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB;
			}
			else if (result_fallback & CLEVO_CMD_GET_BIOS_FEATURES_1_SUB_WHITE_ONLY_KB) {
				clevo_kb_backlight_type = CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR;

				status = clevo_evaluate_method(CLEVO_CMD_GET_BIOS_FEATURES_2, 0, &result_fallback);
				if (!status) {
					pr_debug("CLEVO_CMD_GET_BIOS_FEATURES_2 result_fallback: 0x%08x\n", result_fallback);
					if (result_fallback & CLEVO_CMD_GET_BIOS_FEATURES_2_SUB_WHITE_ONLY_KB_MAX_5) {
						clevo_led_cdev.max_brightness = CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5;
						clevo_led_cdev.brightness = CLEVO_KBD_BRIGHTNESS_WHITE_MAX_5_DEFAULT;
					}
				}
				else {
					pr_notice("CLEVO_CMD_GET_BIOS_FEATURES_2 does not exist on this device or failed\n");
				}
			}
		}
		else {
			pr_notice("CLEVO_CMD_GET_BIOS_FEATURES_1 does not exist on this device or failed\n");
		}
	}
	pr_debug("Keyboard backlight type: 0x%02x\n", clevo_kb_backlight_type);

	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR)
		clevo_leds_set_brightness_extern(clevo_led_cdev.brightness);
	else
		clevo_leds_set_color_extern(CLEVO_KB_COLOR_DEFAULT);

	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		pr_debug("Registering fixed color leds interface\n");
		ret = led_classdev_register(&dev->dev, &clevo_led_cdev);
		if (ret) {
			pr_err("Registering fixed color leds interface failed\n");
			return ret;
		}
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		clevo_evaluate_set_keyboard_status(1);
		pr_debug("Registering single zone rgb leds interface\n");
		ret = devm_led_classdev_multicolor_register(&dev->dev, &clevo_mcled_cdevs[0]);
		if (ret) {
			pr_err("Registering single zone rgb leds interface failed\n");
			return ret;
		}
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		clevo_evaluate_set_keyboard_status(1);
		pr_debug("Registering three zone rgb leds interface\n");
		ret = devm_led_classdev_multicolor_register(&dev->dev, &clevo_mcled_cdevs[0]);
		if (ret) {
			pr_err("Registering three zone rgb zone 0 leds interface failed\n");
			return ret;
		}
		ret = devm_led_classdev_multicolor_register(&dev->dev, &clevo_mcled_cdevs[1]);
		if (ret) {
			pr_err("Registering three zone rgb zone 1 leds interface failed\n");
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[0]);
			return ret;
		}
		ret = devm_led_classdev_multicolor_register(&dev->dev, &clevo_mcled_cdevs[2]);
		if (ret) {
			pr_err("Registering three zone rgb zone 2 leds interface failed\n");
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[0]);
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[1]);
			return ret;
		}
	}

	leds_initialized = true;
	return 0;
}
EXPORT_SYMBOL(clevo_leds_init);

int clevo_leds_suspend(struct platform_device *dev)
{
	switch (clevo_kb_backlight_type) {
	case CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB:
	case CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB:
		clevo_evaluate_set_keyboard_status(0);
		break;
	default:
	}
	return 0;
}
EXPORT_SYMBOL(clevo_leds_suspend);

int clevo_leds_resume(struct platform_device *dev)
{
	switch (clevo_kb_backlight_type) {
	case CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB:
	case CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB:
		clevo_evaluate_set_keyboard_status(1);
		break;
	default:
	}
	return 0;
}
EXPORT_SYMBOL(clevo_leds_resume);

int clevo_leds_remove(struct platform_device *dev) {
	if (leds_initialized) {
		if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			led_classdev_unregister(&clevo_led_cdev);
		}
		else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[0]);
		}
		else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[0]);
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[1]);
			devm_led_classdev_multicolor_unregister(&dev->dev, &clevo_mcled_cdevs[2]);
		}
	}

	leds_initialized = false;

	return 0;
}
EXPORT_SYMBOL(clevo_leds_remove);

enum clevo_kb_backlight_types clevo_leds_get_backlight_type(void) {
	return clevo_kb_backlight_type;
}
EXPORT_SYMBOL(clevo_leds_get_backlight_type);

// TODO Don't reuse brightness_set as it is writing back the same brightness which could lead to race conditions.
// Reimplement brightness_set instead without writing back brightness value like in uniwill_leds.h.
void clevo_leds_restore_state_extern(void) {
	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		clevo_led_cdev.brightness_set(&clevo_led_cdev, clevo_led_cdev.brightness);
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, clevo_mcled_cdevs[0].led_cdev.brightness);
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, clevo_mcled_cdevs[0].led_cdev.brightness);
		clevo_mcled_cdevs[1].led_cdev.brightness_set(&clevo_mcled_cdevs[1].led_cdev, clevo_mcled_cdevs[1].led_cdev.brightness);
		clevo_mcled_cdevs[2].led_cdev.brightness_set(&clevo_mcled_cdevs[2].led_cdev, clevo_mcled_cdevs[2].led_cdev.brightness);
	}
}
EXPORT_SYMBOL(clevo_leds_restore_state_extern);

void clevo_leds_notify_brightness_change_extern(void) {
	int status;
	u32 result;

	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		status = clevo_evaluate_method(CLEVO_CMD_GET_KB_WHITE_LEDS, 0, &result);
		pr_debug("Firmware set brightness: %u\n", result);
		clevo_led_cdev.brightness = result;
		led_classdev_notify_brightness_hw_changed(&clevo_led_cdev, result);
	}
}
EXPORT_SYMBOL(clevo_leds_notify_brightness_change_extern);

// TODO Not used externaly, but only on init. Should not be exposed because it would require a correct
// led_classdev_notify_brightness_hw_changed implementation when used outside of init.
void clevo_leds_set_brightness_extern(enum led_brightness brightness) {
	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		clevo_led_cdev.brightness_set(&clevo_led_cdev, brightness);
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, brightness);
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, brightness);
		clevo_mcled_cdevs[1].led_cdev.brightness_set(&clevo_mcled_cdevs[1].led_cdev, brightness);
		clevo_mcled_cdevs[2].led_cdev.brightness_set(&clevo_mcled_cdevs[2].led_cdev, brightness);
	}
}
EXPORT_SYMBOL(clevo_leds_set_brightness_extern);

// TODO Not used externaly, but only on init. Should not be exposed because it would require a correct
// led_classdev_notify_brightness_hw_changed equivalent for color implementation when used outside of init.
void clevo_leds_set_color_extern(u32 color) {
	if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		clevo_mcled_cdevs[0].subled_info[0].intensity = (color >> 16) & 0xff;
		clevo_mcled_cdevs[0].subled_info[1].intensity = (color >> 8) & 0xff;
		clevo_mcled_cdevs[0].subled_info[2].intensity = color & 0xff;
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, clevo_mcled_cdevs[0].led_cdev.brightness);
	}
	else if (clevo_kb_backlight_type == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		clevo_mcled_cdevs[0].subled_info[0].intensity = (color >> 16) & 0xff;
		clevo_mcled_cdevs[0].subled_info[1].intensity = (color >> 8) & 0xff;
		clevo_mcled_cdevs[0].subled_info[2].intensity = color & 0xff;
		clevo_mcled_cdevs[0].led_cdev.brightness_set(&clevo_mcled_cdevs[0].led_cdev, clevo_mcled_cdevs[0].led_cdev.brightness);
		clevo_mcled_cdevs[1].subled_info[0].intensity = (color >> 16) & 0xff;
		clevo_mcled_cdevs[1].subled_info[1].intensity = (color >> 8) & 0xff;
		clevo_mcled_cdevs[1].subled_info[2].intensity = color & 0xff;
		clevo_mcled_cdevs[1].led_cdev.brightness_set(&clevo_mcled_cdevs[1].led_cdev, clevo_mcled_cdevs[1].led_cdev.brightness);
		clevo_mcled_cdevs[2].subled_info[0].intensity = (color >> 16) & 0xff;
		clevo_mcled_cdevs[2].subled_info[1].intensity = (color >> 8) & 0xff;
		clevo_mcled_cdevs[2].subled_info[2].intensity = color & 0xff;
		clevo_mcled_cdevs[2].led_cdev.brightness_set(&clevo_mcled_cdevs[2].led_cdev, clevo_mcled_cdevs[2].led_cdev.brightness);
	}
}
EXPORT_SYMBOL(clevo_leds_set_color_extern);

MODULE_LICENSE("GPL");

#endif // CLEVO_LEDS_H
