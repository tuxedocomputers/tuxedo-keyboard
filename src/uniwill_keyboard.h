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
#include <linux/workqueue.h>
#include <linux/keyboard.h>

#define UNIWILL_WMI_MGMT_GUID_BA "ABBC0F6D-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BB "ABBC0F6E-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BC "ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_WMI_EVENT_GUID_0 "ABBC0F70-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_1 "ABBC0F71-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_2 "ABBC0F72-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_OSD_RADIOON			0x01A
#define UNIWILL_OSD_RADIOOFF			0x01B

#define UNIWILL_OSD_TOUCHPADWORKAROUND	0xFFF

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

static struct key_entry uniwill_wmi_keymap[] = {
	// { KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	// { KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	// { KE_KEY,	0xb0,				{ KEY_F13 } },
	{ KE_KEY,	UNIWILL_OSD_TOUCHPADWORKAROUND,	{ KEY_F21 } },
	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },
	{ KE_END,	0 }
};

static void key_event_work(struct work_struct *work)
{
	sparse_keymap_report_known_event(
		current_driver->input_device,
		UNIWILL_OSD_TOUCHPADWORKAROUND,
		1,
		true
	);
}

// Previous key codes for detecting longer combination
static u32 prev_key = 0, prevprev_key = 0;
static DECLARE_WORK(uniwill_key_event_work, key_event_work);

static int keyboard_notifier_callb(struct notifier_block *nb, unsigned long code, void *_param)
{
	struct keyboard_notifier_param *param = _param;
	int ret = NOTIFY_OK;

	if (!param->down) {

		if (code == KBD_KEYCODE) {
			switch (param->value) {
			case 125:
				// If the last keys up were 85 -> 29 -> 125
				// manually report KEY_F21
				if (prevprev_key == 85 && prev_key == 29) {
					TUXEDO_DEBUG("Touchpad Toggle\n");
					schedule_work(&uniwill_key_event_work);
					ret = NOTIFY_STOP;
				}
				break;
			}
			prevprev_key = prev_key;
			prev_key = param->value;
		}
	}

	return ret;
}

static struct notifier_block keyboard_notifier_block = {
    .notifier_call = keyboard_notifier_callb
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
			TUXEDO_DEBUG("[Ev %d] Unknown key - %d (%0#6x)\n", guid_nr, code, code);
		}

		// Special key combination when mode change key is pressed
		if (code == 0xb0) {
			input_report_key(current_driver->input_device, KEY_LEFTMETA, 1);
			input_report_key(current_driver->input_device, KEY_LEFTALT, 1);
			input_report_key(current_driver->input_device, KEY_F6, 1);
			input_sync(current_driver->input_device);
			input_report_key(current_driver->input_device, KEY_F6, 0);
			input_report_key(current_driver->input_device, KEY_LEFTALT, 0);
			input_report_key(current_driver->input_device, KEY_LEFTMETA, 0);
			input_sync(current_driver->input_device);
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
	status = wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_0, uniwill_wmi_notify0, NULL);
	if (ACPI_FAILURE(status)) {
		TUXEDO_ERROR("probe: Failed to install uniwill notify handler 0\n");
		goto err_remove_notifiers;
	}
	
	status = wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_1, uniwill_wmi_notify1, NULL);
	if (ACPI_FAILURE(status)) {
		TUXEDO_ERROR("probe: Failed to install uniwill notify handler 1\n");
		goto err_remove_notifiers;
	}

	status = wmi_install_notify_handler(UNIWILL_WMI_EVENT_GUID_2, uniwill_wmi_notify2, NULL);
	if (ACPI_FAILURE(status)) {
		TUXEDO_ERROR("probe: Failed to install uniwill notify handler 2\n");
		goto err_remove_notifiers;
	}

	status = register_keyboard_notifier(&keyboard_notifier_block);

	return 0;

err_remove_notifiers:
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_0);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_1);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_2);

	return -ENODEV;
}

static int uniwill_keyboard_remove(struct platform_device *dev)
{
	unregister_keyboard_notifier(&keyboard_notifier_block);
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
