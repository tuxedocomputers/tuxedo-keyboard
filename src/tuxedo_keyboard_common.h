/*
* tuxedo_keyboard_common.h
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
#ifndef TUXEDO_KEYBOARD_COMMON_H
#define TUXEDO_KEYBOARD_COMMON_H

#include <linux/module.h>
#include <linux/input/sparse-keymap.h>

/* ::::  Module specific Constants and simple Macros   :::: */
#define __TUXEDO_PR(lvl, fmt, ...) do { pr_##lvl(fmt, ##__VA_ARGS__); } while (0)
#define TUXEDO_INFO(fmt, ...) __TUXEDO_PR(info, fmt, ##__VA_ARGS__)
#define TUXEDO_ERROR(fmt, ...) __TUXEDO_PR(err, fmt, ##__VA_ARGS__)
#define TUXEDO_DEBUG(fmt, ...) __TUXEDO_PR(debug, "[%s:%u] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#ifndef DRIVER_NAME
#define DRIVER_NAME "tuxedo_keyboard"
#endif

struct tuxedo_keyboard_driver {
	// Platform driver provided by driver
	struct platform_driver *platform_driver;
	// Probe method provided by driver
	int (*probe)(struct platform_device *);
	// Keymap provided by driver
	struct key_entry *key_map;
	// Input device reference filled in on module init after probe success
	struct input_dev *input_device;
};

// Global module devices
static struct platform_device *tuxedo_platform_device;
static struct input_dev *tuxedo_input_device;

// Currently chosen driver
static struct tuxedo_keyboard_driver *current_driver;

/**
 * Basically a copy of the existing report event but doesn't report unknown events
 */
bool sparse_keymap_report_known_event(struct input_dev *dev, unsigned int code,
					unsigned int value, bool autorelease)
{
	const struct key_entry *ke =
		sparse_keymap_entry_from_scancode(dev, code);

	if (ke) {
		sparse_keymap_report_entry(dev, ke, value, autorelease);
		return true;
	}

	return false;
}

#endif