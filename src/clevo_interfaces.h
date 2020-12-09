/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#ifndef TUXEDO_INTERFACES_H
#define TUXEDO_INTERFACES_H

#include <linux/types.h>

#define CLEVO_WMI_EVENT_GUID		"ABBC0F6B-8EA1-11D1-00A0-C90629100000"
#define CLEVO_WMI_EMAIL_GUID		"ABBC0F6C-8EA1-11D1-00A0-C90629100000"
#define CLEVO_WMI_METHOD_GUID		"ABBC0F6D-8EA1-11D1-00A0-C90629100000"

#define CLEVO_ACPI_RESOURCE_HID		"CLV0001"
#define CLEVO_ACPI_DSM_UUID		"93f224e4-fbdc-4bbf-add6-db71bdc0afad"

// The clevo get commands expect no parameters
#define CLEVO_CMD_GET_FANINFO1		0x63
#define CLEVO_CMD_GET_FANINFO2		0x64
#define CLEVO_CMD_GET_FANINFO3		0x6e

#define CLEVO_CMD_GET_WEBCAM_SW		0x06
#define CLEVO_CMD_GET_FLIGHTMODE_SW	0x07
#define CLEVO_CMD_GET_TOUCHPAD_SW	0x09

// The clevo set commands expect a parameter
#define CLEVO_CMD_SET_FANSPEED_VALUE	0x68
#define CLEVO_CMD_SET_FANSPEED_AUTO	0x69

#define CLEVO_CMD_SET_WEBCAM_SW		0x22
#define CLEVO_CMD_SET_FLIGHTMODE_SW	0x20
#define CLEVO_CMD_SET_TOUCHPAD_SW	0x2a

int clevo_keyboard_init(void);

struct clevo_interface_t {
	char *string_id;
	void (*event_callb)(u32);
	u32 (*method_call)(u8, u32, u32*);
};

u32 clevo_keyboard_add_interface(struct clevo_interface_t *new_interface);
u32 clevo_keyboard_remove_interface(struct clevo_interface_t *interface);
u32 clevo_evaluate_method(u8 cmd, u32 arg, u32 *result);
u32 clevo_get_active_interface_id(char **id_str);

#define MODULE_ALIAS_CLEVO_WMI() \
	MODULE_ALIAS("wmi:" CLEVO_WMI_EVENT_GUID); \
	MODULE_ALIAS("wmi:" CLEVO_WMI_METHOD_GUID);

#define MODULE_ALIAS_CLEVO_ACPI() \
	MODULE_ALIAS("acpi*:" CLEVO_ACPI_RESOURCE_HID ":*");

#define MODULE_ALIAS_CLEVO_INTERFACES() \
	MODULE_ALIAS_CLEVO_WMI(); \
	MODULE_ALIAS_CLEVO_ACPI();

#endif
