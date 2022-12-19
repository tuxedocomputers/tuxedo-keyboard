/*!
 * Copyright (c) 2021 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#ifndef UNIWILL_INTERFACES_H
#define UNIWILL_INTERFACES_H

#include <linux/types.h>

#define UNIWILL_WMI_MGMT_GUID_BA	"ABBC0F6D-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BB	"ABBC0F6E-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BC	"ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_WMI_EVENT_GUID_0	"ABBC0F70-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_1	"ABBC0F71-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_2	"ABBC0F72-8EA1-11D1-00A0-C90629100000"

#define MODULE_ALIAS_UNIWILL_WMI() \
	MODULE_ALIAS("wmi:" UNIWILL_WMI_EVENT_GUID_2); \
	MODULE_ALIAS("wmi:" UNIWILL_WMI_MGMT_GUID_BC);

#define UNIWILL_INTERFACE_WMI_STRID "uniwill_wmi"

typedef u32 (uniwill_read_ec_ram_t)(u16, u8*);
typedef u32 (uniwill_write_ec_ram_t)(u16, u8);
typedef u32 (uniwill_write_ec_ram_with_retry_t)(u16, u8, int);
typedef void (uniwill_event_callb_t)(u32);

struct uniwill_interface_t {
	char *string_id;
	uniwill_event_callb_t *event_callb;
	uniwill_read_ec_ram_t *read_ec_ram;
	uniwill_write_ec_ram_t *write_ec_ram;
};

#define UW_MODEL_PF5LUXG	0x09
#define UW_MODEL_PH4TUX		0x13
#define UW_MODEL_PH4TRX		0x12
#define UW_MODEL_PH4TQF		0x14
#define UW_MODEL_PH4AQF_ARX	0x17

struct uniwill_device_features_t {
	u8 model;
	/**
	 * Identification for uniwill_power_profile_v1
	 *
	 * - Two profiles present in low power devices often called
	 *   "power save" and "balanced".
	 * - Three profiles present mainly in devices with discrete
	 *   graphics card often called "power save", "balanced"
	 *   and "enthusiast"
	 */
	bool uniwill_profile_v1;
	bool uniwill_profile_v1_two_profs;
	bool uniwill_profile_v1_three_profs;
	bool uniwill_profile_v1_three_profs_leds_only;
	bool uniwill_has_charging_prio;
	bool uniwill_has_charging_profile;
};

u32 uniwill_add_interface(struct uniwill_interface_t *new_interface);
u32 uniwill_remove_interface(struct uniwill_interface_t *interface);
uniwill_read_ec_ram_t uniwill_read_ec_ram;
uniwill_write_ec_ram_t uniwill_write_ec_ram;
uniwill_write_ec_ram_with_retry_t uniwill_write_ec_ram_with_retry;
u32 uniwill_get_active_interface_id(char **id_str);
struct uniwill_device_features_t *uniwill_get_device_features(void);

union uw_ec_read_return {
	u32 dword;
	struct {
		u8 data_low;
		u8 data_high;
	} bytes;
};

union uw_ec_write_return {
	u32 dword;
	struct {
		u8 addr_low;
		u8 addr_high;
		u8 data_low;
		u8 data_high;
	} bytes;
};

#endif
