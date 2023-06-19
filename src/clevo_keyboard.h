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
#ifndef CLEVO_KEYBOARD_H
#define CLEVO_KEYBOARD_H

#include "tuxedo_keyboard_common.h"
#include "clevo_interfaces.h"
#include "clevo_leds.h"

// Clevo event codes
#define CLEVO_EVENT_KB_LEDS_DECREASE		0x81
#define CLEVO_EVENT_KB_LEDS_INCREASE		0x82
#define CLEVO_EVENT_KB_LEDS_CYCLE_MODE		0x83
#define CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS	0x8A
#define CLEVO_EVENT_KB_LEDS_TOGGLE		0x9F

#define CLEVO_EVENT_TOUCHPAD_TOGGLE		0x5D
#define CLEVO_EVENT_TOUCHPAD_OFF		0xFC
#define CLEVO_EVENT_TOUCHPAD_ON			0xFD

#define CLEVO_EVENT_RFKILL1			0x85
#define CLEVO_EVENT_RFKILL2			0x86

#define CLEVO_KB_MODE_DEFAULT			0 // "CUSTOM"/Static Color

static struct clevo_interfaces_t {
	struct clevo_interface_t *wmi;
	struct clevo_interface_t *acpi;
} clevo_interfaces;

static struct clevo_interface_t *active_clevo_interface;

static DEFINE_MUTEX(clevo_keyboard_interface_modification_lock);

static struct key_entry clevo_keymap[] = {
	// Keyboard backlight (RGB versions)
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_DECREASE, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_INCREASE, { KEY_KBDILLUMUP } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_TOGGLE, { KEY_KBDILLUMTOGGLE } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_CYCLE_MODE, { KEY_LIGHTS_TOGGLE } },
	// Single cycle key (white only versions) (currently handled in driver)
	// { KE_KEY, CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS, { KEY_KBDILLUMTOGGLE } },

	// Touchpad
	// The weirdly named touchpad toggle key that is implemented as KEY_F21 "everywhere"
	// (instead of KEY_TOUCHPAD_TOGGLE or on/off)
	// Most "new" devices just provide one toggle event
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_TOGGLE, { KEY_F21 } },
	// Some "old" devices produces on/off events
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_OFF, { KEY_F21 } },
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_ON, { KEY_F21 } },
	// The alternative key events (currently not used)
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_OFF, { KEY_TOUCHPAD_OFF } },
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_ON, { KEY_TOUCHPAD_ON } },
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_TOGGLE, { KEY_TOUCHPAD_TOGGLE } },

	// Rfkill still needed by some devices
	{ KE_KEY, CLEVO_EVENT_RFKILL1, { KEY_RFKILL } },
	{ KE_IGNORE, CLEVO_EVENT_RFKILL2, { KEY_RFKILL } }, // Older rfkill event
	// Note: Volume events need to be ignored as to not interfere with built-in functionality
	{ KE_IGNORE, 0xfa, { KEY_UNKNOWN } }, // Appears by volume up/down
	{ KE_IGNORE, 0xfb, { KEY_UNKNOWN } }, // Appears by mute toggle

	{ KE_END, 0 }
};

// Keyboard struct
static struct kbd_led_state_t {
	u8 has_mode;
	u8 mode;
	u8 whole_kbd_color;
} kbd_led_state = {
	.has_mode = 1,
	.mode = CLEVO_KB_MODE_DEFAULT,
	.whole_kbd_color = 7,
};

static struct kbd_backlight_mode_t {
	u8 key;
	u32 value;
	const char *const name;
} kbd_backlight_modes[] = {
        { .key = 0, .value = 0x00000000, .name = "CUSTOM"},
        { .key = 1, .value = 0x1002a000, .name = "BREATHE"},
        { .key = 2, .value = 0x33010000, .name = "CYCLE"},
        { .key = 3, .value = 0x80000000, .name = "DANCE"},
        { .key = 4, .value = 0xA0000000, .name = "FLASH"},
        { .key = 5, .value = 0x70000000, .name = "RANDOM_COLOR"},
        { .key = 6, .value = 0x90000000, .name = "TEMPO"},
        { .key = 7, .value = 0xB0000000, .name = "WAVE"}
};

int clevo_evaluate_method2(u8 cmd, u32 arg, union acpi_object **result)
{
	if (IS_ERR_OR_NULL(active_clevo_interface)) {
		pr_err("clevo_keyboard: no active interface while attempting cmd %02x arg %08x\n", cmd, arg);
		return -ENODEV;
	}
	return active_clevo_interface->method_call(cmd, arg, result);
}
EXPORT_SYMBOL(clevo_evaluate_method2);

int clevo_evaluate_method(u8 cmd, u32 arg, u32 *result)
{
	int status = 0;
	union acpi_object *out_obj;

	status = clevo_evaluate_method2(cmd, arg, &out_obj);
	if (status) {
		return status;
	}
	else {
		if (out_obj->type == ACPI_TYPE_INTEGER) {
			if (!IS_ERR_OR_NULL(result))
				*result = (u32) out_obj->integer.value;
		} else {
			pr_err("return type not integer, use clevo_evaluate_method2\n");
			status = -ENODATA;
		}
		ACPI_FREE(out_obj);
	}

	return status;
}
EXPORT_SYMBOL(clevo_evaluate_method);

int clevo_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(active_clevo_interface))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = active_clevo_interface->string_id;

	return 0;
}
EXPORT_SYMBOL(clevo_get_active_interface_id);

static int set_enabled_cmd(u8 state)
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

static void set_next_color_whole_kb(void)
{
	/* "Calculate" new to-be color */
	u32 new_color_id;
	u32 new_color_code;

	new_color_id = kbd_led_state.whole_kbd_color + 1;
	if (new_color_id >= color_list.size) {
		new_color_id = 1; // Skip black
	}
	new_color_code = color_list.colors[new_color_id].code;

	TUXEDO_INFO("set_next_color_whole_kb(): new_color_id: %i, new_color_code %X", 
		    new_color_id, new_color_code);

	/* Set color on all four regions*/
	clevo_leds_set_color_extern(new_color_code);
	kbd_led_state.whole_kbd_color = new_color_id;
}

static void set_kbd_backlight_mode(u8 kbd_backlight_mode)
{
	TUXEDO_INFO("Set keyboard backlight mode on %s", kbd_backlight_modes[kbd_backlight_mode].name);

	if (!clevo_evaluate_method(CLEVO_CMD_SET_KB_RGB_LEDS, kbd_backlight_modes[kbd_backlight_mode].value, NULL)) {
		// method was succesfull so update ur internal state struct
		kbd_led_state.mode = kbd_backlight_mode;
	}
}

// Sysfs Interface for the keyboard backlight mode
static ssize_t list_kbd_backlight_modes_fs(struct device *child, struct device_attribute *attr,
                                         char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.mode);
}

static ssize_t set_kbd_backlight_mode_fs(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size)
{
	unsigned int kbd_backlight_mode;

	int err = kstrtouint(buffer, 0, &kbd_backlight_mode);
	if (err) {
		return err;
	}

	kbd_backlight_mode = clamp_t(u8, kbd_backlight_mode, 0, ARRAY_SIZE(kbd_backlight_modes) - 1);
	set_kbd_backlight_mode(kbd_backlight_mode);

	return size;
}

// Sysfs attribute file permissions and method linking
static DEVICE_ATTR(kbd_backlight_mode, 0644, list_kbd_backlight_modes_fs, set_kbd_backlight_mode_fs);

static int kbd_backlight_mode_id_validator(const char *value,
					   const struct kernel_param *kbd_backlight_mode_param)
{
	int kbd_backlight_mode = 0;

	if (kstrtoint(value, 10, &kbd_backlight_mode) != 0
	    || kbd_backlight_mode < 0
	    || kbd_backlight_mode > (ARRAY_SIZE(kbd_backlight_modes) - 1)) {
		return -EINVAL;
	}

	return param_set_int(value, kbd_backlight_mode_param);
}

static const struct kernel_param_ops param_ops_mode_ops = {
	.set = kbd_backlight_mode_id_validator,
	.get = param_get_int,
};

static u8 param_kbd_backlight_mode = CLEVO_KB_MODE_DEFAULT;
module_param_cb(kbd_backlight_mode, &param_ops_mode_ops, &param_kbd_backlight_mode, S_IRUSR);
MODULE_PARM_DESC(kbd_backlight_mode, "Set the keyboard backlight mode");

static void clevo_keyboard_event_callb(u32 event)
{
	u32 key_event = event;

	TUXEDO_DEBUG("Clevo event: %0#6x\n", event);

	switch (key_event) {
		case CLEVO_EVENT_KB_LEDS_CYCLE_MODE:
			set_next_color_whole_kb();
			break;
		case CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS:
			clevo_leds_notify_brightness_change_extern();
			break;
		default:
			break;
	}

	if (current_driver != NULL && current_driver->input_device != NULL) {
		if (!sparse_keymap_report_known_event(current_driver->input_device, key_event, 1, true)) {
			TUXEDO_DEBUG("Unknown key - %d (%0#6x)\n", key_event, key_event);
		}
	}
}

static void clevo_keyboard_init_device_interface(struct platform_device *dev)
{
	// Setup sysfs
	if (clevo_leds_get_backlight_type() == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		if (device_create_file(&dev->dev, &dev_attr_kbd_backlight_mode) != 0) {
			TUXEDO_ERROR("Sysfs attribute file creation failed for blinking pattern\n");
		}
		else {
			kbd_led_state.has_mode = 1;
		}
	}
}

/**
 * strstr version of dmi_match
 */
static bool dmi_string_in(enum dmi_field f, const char *str)
{
	const char *info = dmi_get_system_info(f);

	if (info == NULL || str == NULL)
		return info == str;

	return strstr(info, str) != NULL;
}

static void clevo_keyboard_init(void)
{
	bool performance_profile_set_workaround;

	kbd_led_state.mode = param_kbd_backlight_mode;
	set_kbd_backlight_mode(kbd_led_state.mode);

	clevo_evaluate_method(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);
	set_enabled_cmd(1);

	// Workaround for firmware issue not setting selected performance profile.
	// Explicitly set "performance" perf. profile on init regardless of what is chosen
	// for these devices (Aura, XP14, IBS14v5)
	performance_profile_set_workaround = false
		|| dmi_string_in(DMI_BOARD_NAME, "AURA1501")
		|| dmi_string_in(DMI_BOARD_NAME, "EDUBOOK1502")
		|| dmi_string_in(DMI_BOARD_NAME, "NL5xRU")
		|| dmi_string_in(DMI_BOARD_NAME, "NV4XMB,ME,MZ")
		|| dmi_string_in(DMI_BOARD_NAME, "L140CU")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50MU")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50_70MU")
		|| dmi_string_in(DMI_BOARD_NAME, "PCX0DX")
		|| dmi_string_in(DMI_BOARD_NAME, "PCx0Dx_GN20")
		|| dmi_string_in(DMI_BOARD_NAME, "L14xMU")
		;
	if (performance_profile_set_workaround) {
		TUXEDO_INFO("Performance profile 'performance' set workaround applied\n");
		clevo_evaluate_method(CLEVO_CMD_OPT, 0x19000002, NULL);
	}
}

static int clevo_keyboard_probe(struct platform_device *dev)
{
	clevo_leds_init(dev);
	// clevo_keyboard_init_device_interface() must come after clevo_leds_init()
	// to know keyboard backlight type
	clevo_keyboard_init_device_interface(dev);
	clevo_keyboard_init();

	return 0;
}

static void clevo_keyboard_remove_device_interface(struct platform_device *dev)
{
	if (kbd_led_state.has_mode == 1) {
		device_remove_file(&dev->dev, &dev_attr_kbd_backlight_mode);
	}
}

static int clevo_keyboard_remove(struct platform_device *dev)
{
	clevo_keyboard_remove_device_interface(dev);
	clevo_leds_remove(dev);
	return 0;
}

static int clevo_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	// turning the keyboard off prevents default colours showing on resume
	set_enabled_cmd(0);
	return 0;
}

static int clevo_keyboard_resume(struct platform_device *dev)
{
	clevo_evaluate_method(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);
	clevo_leds_restore_state_extern(); // Sometimes clevo devices forget their last state after
					   // suspend, so let the kernel ensure it.
	set_enabled_cmd(1);
	return 0;
}

static struct platform_driver platform_driver_clevo = {
	.remove = clevo_keyboard_remove,
	.suspend = clevo_keyboard_suspend,
	.resume = clevo_keyboard_resume,
	.driver =
		{
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
		},
};

static struct tuxedo_keyboard_driver clevo_keyboard_driver = {
	.platform_driver = &platform_driver_clevo,
	.probe = clevo_keyboard_probe,
	.key_map = clevo_keymap,
};

int clevo_keyboard_add_interface(struct clevo_interface_t *new_interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(new_interface->string_id, CLEVO_INTERFACE_WMI_STRID) == 0) {
		clevo_interfaces.wmi = new_interface;
		clevo_interfaces.wmi->event_callb = clevo_keyboard_event_callb;

		// Only use wmi if there is no other current interface
		if (ZERO_OR_NULL_PTR(active_clevo_interface)) {
			pr_debug("enable wmi events\n");
			clevo_interfaces.wmi->method_call(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);

			active_clevo_interface = clevo_interfaces.wmi;
		}
	} else if (strcmp(new_interface->string_id, CLEVO_INTERFACE_ACPI_STRID) == 0) {
		clevo_interfaces.acpi = new_interface;
		clevo_interfaces.acpi->event_callb = clevo_keyboard_event_callb;

		pr_debug("enable acpi events (takes priority)\n");
		clevo_interfaces.acpi->method_call(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);
		active_clevo_interface = clevo_interfaces.acpi;
	} else {
		// Not recognized interface
		pr_err("unrecognized interface\n");
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	if (active_clevo_interface != NULL)
		tuxedo_keyboard_init_driver(&clevo_keyboard_driver);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_add_interface);

int clevo_keyboard_remove_interface(struct clevo_interface_t *interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(interface->string_id, CLEVO_INTERFACE_WMI_STRID) == 0) {
		clevo_interfaces.wmi = NULL;
	} else if (strcmp(interface->string_id, CLEVO_INTERFACE_ACPI_STRID) == 0) {
		clevo_interfaces.acpi = NULL;
	} else {
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	if (active_clevo_interface == interface) {
		tuxedo_keyboard_remove_driver(&clevo_keyboard_driver);
		active_clevo_interface = NULL;
	}


	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_remove_interface);

#endif // CLEVO_KEYBOARD_H
