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
#ifndef CLEVO_INTERFACES_H
#define CLEVO_INTERFACES_H

#include <linux/types.h>
#include <linux/acpi.h>

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

#define CLEVO_CMD_GET_EVENT		0x01

#define CLEVO_CMD_GET_SPECS		0x0D // Returns buffer -> only works with clevo_evaluate_method2
#define CLEVO_CMD_GET_BIOS_FEATURES_1	0x52
#define CLEVO_CMD_GET_BIOS_FEATURES_1_SUB_WHITE_ONLY_KB	0x40000000
#define CLEVO_CMD_GET_BIOS_FEATURES_1_SUB_3_ZONE_RGB_KB	0x00400000

#define CLEVO_CMD_GET_BIOS_FEATURES_2	0x7A
#define CLEVO_CMD_GET_BIOS_FEATURES_2_SUB_WHITE_ONLY_KB_MAX_5	0x4000

#define CLEVO_CMD_GET_KB_WHITE_LEDS	0x3D // Get brightness of white only keyboard backlights

// The clevo set commands expect a parameter
#define CLEVO_CMD_SET_FANSPEED_VALUE	0x68
#define CLEVO_CMD_SET_FANSPEED_AUTO	0x69

#define CLEVO_CMD_SET_WEBCAM_SW		0x22
#define CLEVO_CMD_SET_FLIGHTMODE_SW	0x20
#define CLEVO_CMD_SET_TOUCHPAD_SW	0x2a

#define CLEVO_CMD_SET_EVENTS_ENABLED	0x46

#define CLEVO_CMD_SET_KB_WHITE_LEDS	0x27 // Set brightness of white only keyboard backlights
#define CLEVO_CMD_SET_KB_RGB_LEDS	0x67 // Used to set color, brightness, blinking pattern, etc.
#define CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_0		0xF0000000 // 1-zone RGB and 3-zone RGB left
#define CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_1		0xF1000000 // 3-zone RGB center
#define CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_2		0xF2000000 // 3-Zone RGB right
#define CLEVO_CMD_SET_KB_LEDS_SUB_RGB_ZONE_3		0xF3000000 // Unused on all known Clevo devices
#define CLEVO_CMD_SET_KB_LEDS_SUB_RGB_BRIGHTNESS	0xF4000000

#define CLEVO_CMD_OPT			0x79
#define CLEVO_CMD_OPT_SUB_SET_PERF_PROF	0x19

struct clevo_interface_t {
	char *string_id;
	void (*event_callb)(u32);
	int (*method_call)(u8, u32, union acpi_object **);
};

int clevo_keyboard_add_interface(struct clevo_interface_t *new_interface);
int clevo_keyboard_remove_interface(struct clevo_interface_t *interface);
int clevo_evaluate_method(u8 cmd, u32 arg, u32 *result);
int clevo_evaluate_method2(u8 cmd, u32 arg, union acpi_object **result);
int clevo_get_active_interface_id(char **id_str);

#define MODULE_ALIAS_CLEVO_WMI() \
	MODULE_ALIAS("wmi:" CLEVO_WMI_EVENT_GUID); \
	MODULE_ALIAS("wmi:" CLEVO_WMI_METHOD_GUID);

#define CLEVO_INTERFACE_WMI_STRID	"clevo_wmi"
#define CLEVO_INTERFACE_ACPI_STRID	"clevo_acpi"

#define MODULE_ALIAS_CLEVO_ACPI() \
	MODULE_ALIAS("acpi*:" CLEVO_ACPI_RESOURCE_HID ":*");

#define MODULE_ALIAS_CLEVO_INTERFACES() \
	MODULE_ALIAS_CLEVO_WMI(); \
	MODULE_ALIAS_CLEVO_ACPI();

#endif
