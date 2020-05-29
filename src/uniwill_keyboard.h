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
#include <linux/acpi.h>
#include <linux/wmi.h>

#define UNIWILL_WMI_MGMT_GUID_BA "ABBC0F6D-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BB "ABBC0F6E-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BC "ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_WMI_EVENT_GUID_0 "ABBC0F70-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_1 "ABBC0F71-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_2 "ABBC0F72-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_OSD_RADIOON				0x01A
#define UNIWILL_OSD_RADIOOFF			0x01B

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

static struct key_entry uniwill_wmi_keymap[] = {
	{ KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	{ KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	{ KE_END,	0 }
};

static void uniwill_wmi_handle_event(u32 value, void *context, u32 guid_nr)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	acpi_status status;
	int code;

	status = wmi_get_event_data(value, &response);
	if (status != AE_OK) {
		TUXEDO_ERROR("uniwill handle event -> bad event status\n");
		return;
	}

	obj = (union acpi_object *) response.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		code = obj->integer.value;
		if (!sparse_keymap_report_known_event(current_driver->input_device, code, 1, true)) {
			TUXEDO_DEBUG("Unknown key - %d (%0#6x)\n", code, code);
		}
	}

	kfree(obj);
}

static void uniwill_wmi_notify0(u32 value, void *context)
{
	uniwill_wmi_handle_event(value, context, 0);
}

static void uniwill_wmi_notify1(u32 value, void *context)
{
	uniwill_wmi_handle_event(value, context, 1);
}

static void uniwill_wmi_notify2(u32 value, void *context)
{
	uniwill_wmi_handle_event(value, context, 2);
}

static int uniwill_keyboard_probe(struct platform_device *dev)
{
	int status;

	// Look for for GUIDs used on uniwill devices
	status =
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_0) &&
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_1) &&
		wmi_has_guid(UNIWILL_WMI_EVENT_GUID_2) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BA) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BB) &&
		wmi_has_guid(UNIWILL_WMI_MGMT_GUID_BC);
	
	if (!status) {
		TUXEDO_DEBUG("probe: At least one Uniwill GUID missing\n");
		return -ENODEV;
	}

	// Attempt to add event handlers
	status =
		wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_0, uniwill_wmi_notify0, NULL) &&
		wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_1, uniwill_wmi_notify1, NULL) &&
		wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_2, uniwill_wmi_notify2, NULL);

	if (!status) {
		TUXEDO_ERROR("probe: Failed to install at least one uniwill notify handler\n");
		goto err_remove_notifiers;
	}

	return 0;

err_remove_notifiers:
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_0);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_1);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_2);

	return -ENODEV;
}

static int uniwill_keyboard_remove(struct platform_device *dev)
{
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_0);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_1);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_2);

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
