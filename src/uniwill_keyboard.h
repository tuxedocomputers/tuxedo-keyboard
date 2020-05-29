/*
* uniwill_keyboard.h
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
#include "tuxedo_keyboard_common.h"

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

static struct key_entry uniwill_wmi_keymap[] = {
	{ KE_END,	0 }
};

static int uniwill_keyboard_probe(struct platform_device *dev)
{
	return -ENODEV;
}

static int uniwill_keyboard_remove(struct platform_device *dev)
{
	return 0;
}

static int uniwill_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int uniwill_keyboard_resume(struct platform_device *dev)
{
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
