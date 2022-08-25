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

#define UNIWILL_KBD_BRIGHTNESS_MIN	0x00
#define UNIWILL_KBD_BRIGHTNESS_MAX	0xff // Uniwill devices actually operate on a range
					     // from 0x00 - 0xc8 (200), but because
					     // userspace will get it wrong we do the
					     // conversion in driver.
#define UNIWILL_KBD_BRIGHTNESS_DEFAULT	(UNIWILL_KBD_BRIGHTNESS_MAX * 0.5)

#define UNIWILL_KB_COLOR_DEFAULT_RED	0xff // Same applies as for brightness: Actuall
#define UNIWILL_KB_COLOR_DEFAULT_GREEN	0xff // range is 0x00 - 0xc8. Conversion is done in
#define UNIWILL_KB_COLOR_DEFAULT_BLUE	0xff // this driver.
#define UNIWILL_KB_COLOR_DEFAULT	((UNIWILL_KB_COLOR_DEFAULT_RED << 16) + (UNIWILL_KB_COLOR_DEFAULT_GREEN << 8) + UNIWILL_KB_COLOR_DEFAULT_BLUE)

enum uniwill_kb_backlight_types {
	UNIWILL_KB_BACKLIGHT_TYPE_NONE,
	UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR,
	UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB,
	UNIWILL_KB_BACKLIGHT_TYPE_PER_KEY_RGB
};

int uniwill_leds_init(struct platform_device *dev);
int uniwill_leds_remove(struct platform_device *dev);
enum uniwill_kb_backlight_types uniwill_leds_get_backlight_type(void);
void uniwill_leds_set_brightness_extern(u32 brightness);
void uniwill_leds_set_color_extern(u32 color);

// TODO The following should go into a seperate .c file, but for this to work more reworking is required in the tuxedo_keyboard structure.

#include "clevo_leds.h"

#include "clevo_interfaces.h"

#include <linux/leds.h>
#include <linux/led-class-multicolor.h>

#define FF_TO_UW_RANGE(x)	(x * 0xc8 / 0xff)
#define UW_TO_FF_RANGE(x)	(x * 0xff / 0xc8)

static u32 uniwill_read_kbd_bl_rgb(u8 *red, u8 *green, u8 *blue)
{
	u32 result;

	uniwill_read_ec_ram(0x1803, red);
	uniwill_read_ec_ram(0x1805, green);
	uniwill_read_ec_ram(0x1808, blue);

	result = 0;

	return result;
}

static void uniwill_write_kbd_bl_rgb(u8 red, u8 green, u8 blue)
{
	if (red > 0xc8) red = 0xc8;
	if (green > 0xc8) green = 0xc8;
	if (blue > 0xc8) blue = 0xc8;
	uniwill_write_ec_ram(0x1803, red);
	uniwill_write_ec_ram(0x1805, green);
	uniwill_write_ec_ram(0x1808, blue);
	TUXEDO_DEBUG("Wrote kbd color [%0#4x, %0#4x, %0#4x]\n", red, green, blue);
}

static struct mc_subled clevo_mcled_cdev_subleds[3] = {
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
	.led_cdev.brightness = &uniwill_leds_set_brightness_mc,
	.led_cdev.brightness = UNIWILL_KBD_BRIGHTNESS_DEFAULT,
	.num_colors = 3,
	.subled_info = cdev_kb_uw_mc_subled
};

#endif // UNIWILL_LEDS_H
