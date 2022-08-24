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

#include <linux/platform_device.h>

enum clevo_kb_backlight_types {
	CLEVO_KB_BACKLIGHT_TYPE_NONE = 0x00,
	CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR = 0x01,
	CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB = 0x02,
	CLEVO_KB_BACKLIGHT_TYPE_1_ZONE_RGB = 0x06,
	CLEVO_KB_BACKLIGHT_TYPE_PER_KEY_RGB = 0xf3
};

int clevo_leds_init(struct platform_device *dev);
int clevo_leds_remove(struct platform_device *dev);
enum clevo_kb_backlight_types clevo_leds_get_backlight_type(void);
void clevo_leds_set_brightness_extern(u32 brightness);
void clevo_leds_set_color_extern(u32 color);

#endif // CLEVO_LEDS_H
