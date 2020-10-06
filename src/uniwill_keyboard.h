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
#include <linux/timer.h>
#include <linux/delay.h>

#define UNIWILL_WMI_MGMT_GUID_BA "ABBC0F6D-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BB "ABBC0F6E-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BC "ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_WMI_EVENT_GUID_0 "ABBC0F70-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_1 "ABBC0F71-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_2 "ABBC0F72-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_OSD_RADIOON			0x01A
#define UNIWILL_OSD_RADIOOFF			0x01B
#define UNIWILL_OSD_KB_LED_LEVEL0		0x03B
#define UNIWILL_OSD_KB_LED_LEVEL1		0x03C
#define UNIWILL_OSD_KB_LED_LEVEL2		0x03D
#define UNIWILL_OSD_KB_LED_LEVEL3		0x03E
#define UNIWILL_OSD_KB_LED_LEVEL4		0x03F
#define UNIWILL_OSD_DC_ADAPTER_CHANGE		0x0AB

#define UNIWILL_KEY_RFKILL			0x0A4

#define UNIWILL_OSD_TOUCHPADWORKAROUND		0xFFF

#define UNIWILL_BRIGHTNESS_MIN			0x00
#define UNIWILL_BRIGHTNESS_MAX			0xc8
#define UNIWILL_BRIGHTNESS_DEFAULT		UNIWILL_BRIGHTNESS_MAX * 0.30
#define UNIWILL_COLOR_DEFAULT			0xffffff

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

typedef u32 (uw_ec_read_func)(u8, u8, union uw_ec_read_return *);
typedef u32 (uw_ec_write_func)(u8, u8, u8, u8, union uw_ec_write_return *);

extern uw_ec_read_func uw_ec_read_addr;
extern uw_ec_write_func uw_ec_write_addr;

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

struct kbd_led_state_uw_t {
	u32 brightness;
	u32 color;
} kbd_led_state_uw = {
	.brightness = UNIWILL_BRIGHTNESS_DEFAULT,
	.color = UNIWILL_COLOR_DEFAULT,
};

static u8 uniwill_kbd_bl_enable_state_on_start;
static bool uniwill_kbd_bl_type_rgb_single_color = true;

static struct key_entry uniwill_wmi_keymap[] = {
	// { KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	// { KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	// { KE_KEY,	0xb0,				{ KEY_F13 } },
	// Manual mode rfkill
	{ KE_KEY,	UNIWILL_KEY_RFKILL,		{ KEY_RFKILL }},
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

static u8 uniwill_read_kbd_bl_enabled(void)
{
	union uw_ec_read_return reg_read_return;

	uw_ec_read_func *__uw_ec_read_addr;

	u8 enabled = 0xff;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);

	if (__uw_ec_read_addr) {
		__uw_ec_read_addr(0x8c, 0x07, &reg_read_return);
		enabled = (reg_read_return.bytes.data_low >> 1) & 0x01;
		enabled = !enabled;
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
	}

	if (__uw_ec_read_addr) symbol_put(uw_ec_read_addr);

	return enabled;
}

static void uniwill_write_kbd_bl_enable(u8 enable)
{
	union uw_ec_read_return reg_read_return;
	union uw_ec_write_return reg_write_return;

	uw_ec_read_func *__uw_ec_read_addr;
	uw_ec_write_func *__uw_ec_write_addr;

	u8 write_value = 0;
	enable = enable & 0x01;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);
	__uw_ec_write_addr = symbol_get(uw_ec_write_addr);

	if (__uw_ec_read_addr && __uw_ec_write_addr) {
		__uw_ec_read_addr(0x8c, 0x07, &reg_read_return);
		write_value = reg_read_return.bytes.data_low & ~(1 << 1);
		write_value |= (!enable << 1);
		__uw_ec_write_addr(0x8c, 0x07, write_value, 0x00, &reg_write_return);
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
	}

	if (__uw_ec_read_addr) symbol_put(uw_ec_read_addr);
	if (__uw_ec_write_addr) symbol_put(uw_ec_write_addr);
}

/*static u32 uniwill_read_kbd_bl_br_state(u8 *brightness_state)
{
	union uw_ec_read_return reg_read_return;
	u32 result;

	uw_ec_read_func *__uw_ec_read_addr;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);

	if (__uw_ec_read_addr) {
		__uw_ec_read_addr(0x8c, 0x07, &reg_read_return);
		*brightness_state = (reg_read_return.bytes.data_low & 0xf0) >> 4;
		result = 0;
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
		result = -EIO;
	}

	return result;
}*/

static u32 uniwill_read_kbd_bl_rgb(u8 *red, u8 *green, u8 *blue)
{
	union uw_ec_read_return reg_read_return;
	u32 result;

	uw_ec_read_func *__uw_ec_read_addr;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);

	if (__uw_ec_read_addr) {
		__uw_ec_read_addr(0x03, 0x18, &reg_read_return);
		*red = reg_read_return.bytes.data_low;
		__uw_ec_read_addr(0x05, 0x18, &reg_read_return);
		*green = reg_read_return.bytes.data_low;
		__uw_ec_read_addr(0x08, 0x18, &reg_read_return);
		*blue = reg_read_return.bytes.data_low;
		result = 0;
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
		result = -EIO;
	}

	if (__uw_ec_read_addr) symbol_put(uw_ec_read_addr);

	return result;
}

static void uniwill_write_kbd_bl_rgb(u8 red, u8 green, u8 blue)
{
	union uw_ec_write_return reg_write_return;

	uw_ec_read_func *__uw_ec_read_addr;
	uw_ec_write_func *__uw_ec_write_addr;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);
	__uw_ec_write_addr = symbol_get(uw_ec_write_addr);

	if (__uw_ec_read_addr && __uw_ec_write_addr) {
		// Write the colors
		if (red > 0xc8) red = 0xc8;
		if (green > 0xc8) green = 0xc8;
		if (blue > 0xc8) blue = 0xc8;
		__uw_ec_write_addr(0x03, 0x18, red, 0x00, &reg_write_return);
		__uw_ec_write_addr(0x05, 0x18, green, 0x00, &reg_write_return);
		__uw_ec_write_addr(0x08, 0x18, blue, 0x00, &reg_write_return);
		TUXEDO_DEBUG("Wrote color [%0#4x, %0#4x, %0#4x]\n", red, green, blue);
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
	}

	if (__uw_ec_read_addr) symbol_put(uw_ec_read_addr);
	if (__uw_ec_write_addr) symbol_put(uw_ec_write_addr);
}

static void uniwill_write_kbd_bl_state(void) {
	// Get single colors from state
	u32 color_red = ((kbd_led_state_uw.color >> 0x10) & 0xff);
	u32 color_green = (kbd_led_state_uw.color >> 0x08) & 0xff;
	u32 color_blue = (kbd_led_state_uw.color >> 0x00) & 0xff;

	u32 brightness_percentage = (kbd_led_state_uw.brightness * 100) / UNIWILL_BRIGHTNESS_MAX;

	// Scale color values to valid range
	color_red = (color_red * 0xc8) / 0xff;
	color_green = (color_green * 0xc8) / 0xff;
	color_blue = (color_blue * 0xc8) / 0xff;

	// Scale the respective color values with brightness
	color_red = (color_red * brightness_percentage) / 100;
	color_green = (color_green * brightness_percentage) / 100;
	color_blue = (color_blue * brightness_percentage) / 100;

	uniwill_write_kbd_bl_rgb(color_red, color_green, color_blue);
}

static void uniwill_write_kbd_bl_reset(void)
{
	union uw_ec_write_return reg_write_return;
	union uw_ec_read_return reg_read_return;

	uw_ec_read_func *__uw_ec_read_addr;
	uw_ec_write_func *__uw_ec_write_addr;

	__uw_ec_read_addr = symbol_get(uw_ec_read_addr);
	__uw_ec_write_addr = symbol_get(uw_ec_write_addr);

	if (__uw_ec_read_addr && __uw_ec_write_addr) {
		__uw_ec_write_addr(0x8c, 0x07, 0x10, 0x00, &reg_write_return);
	} else {
		TUXEDO_DEBUG("tuxedo-cc-wmi symbols not found\n");
	}

	if (__uw_ec_read_addr) symbol_put(uw_ec_read_addr);
	if (__uw_ec_write_addr) symbol_put(uw_ec_write_addr);
}

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
	if (obj) {
		if (obj->type == ACPI_TYPE_INTEGER) {
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

			// Keyboard backlight brightness toggle
			if (uniwill_kbd_bl_type_rgb_single_color) {
				switch (code) {
				case UNIWILL_OSD_KB_LED_LEVEL0:
					kbd_led_state_uw.brightness = 0x00;
					uniwill_write_kbd_bl_state();
					break;
				case UNIWILL_OSD_KB_LED_LEVEL1:
					kbd_led_state_uw.brightness = 0x20;
					uniwill_write_kbd_bl_state();
					break;
				case UNIWILL_OSD_KB_LED_LEVEL2:
					kbd_led_state_uw.brightness = 0x50;
					uniwill_write_kbd_bl_state();
					break;
				case UNIWILL_OSD_KB_LED_LEVEL3:
					kbd_led_state_uw.brightness = 0x80;
					uniwill_write_kbd_bl_state();
					break;
				case UNIWILL_OSD_KB_LED_LEVEL4:
					kbd_led_state_uw.brightness = 0xc8;
					uniwill_write_kbd_bl_state();
					break;
				// Also refresh keyboard state on cable switch event
				case UNIWILL_OSD_DC_ADAPTER_CHANGE:
					uniwill_write_kbd_bl_state();
					break;
				}
			}
		} else {
			TUXEDO_DEBUG("[Ev %d] Unknown event type - %d (%0#6x)\n", guid_nr, obj->type, obj->type);
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

static ssize_t uw_brightness_show(struct device *child,
				  struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state_uw.brightness);
}

static ssize_t uw_brightness_store(struct device *child,
				   struct device_attribute *attr,
				   const char *buffer, size_t size)
{
	u32 brightness_input;
	int err = kstrtouint(buffer, 0, &brightness_input);
	if (err) return err;
	if (brightness_input > UNIWILL_BRIGHTNESS_MAX) return -EINVAL;
	kbd_led_state_uw.brightness = (u8)brightness_input;
	uniwill_write_kbd_bl_state();
	return size;
}

static ssize_t uw_color_string_show(struct device *child,
				 struct device_attribute *attr, char *buffer)
{
	int i;
	sprintf(buffer, "Color values:");
	for (i = 0; i < color_list.size; ++i) {
		sprintf(buffer + strlen(buffer), " %s",
			color_list.colors[i].name);
	}
	sprintf(buffer + strlen(buffer), "\n");
	return strlen(buffer);
}

static ssize_t uw_color_string_store(struct device *child,
				   struct device_attribute *attr,
				   const char *buffer, size_t size)
{
	u32 color_value;
	char *buffer_copy;
	
	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	color_value = color_lookup(&color_list, strstrip(buffer_copy));
	kfree(buffer_copy);

	if (color_value > 0xffffff) return -EINVAL;
	kbd_led_state_uw.color = color_value;
	uniwill_write_kbd_bl_state();
	return size;
}

// Device attributes used by uw kbd
struct uw_kbd_dev_attrs_t {
	struct device_attribute brightness;
	struct device_attribute color_string;
} uw_kbd_dev_attrs = {
	.brightness = __ATTR(brightness, 0644, uw_brightness_show, uw_brightness_store),
	.color_string = __ATTR(color_string, 0644, uw_color_string_show, uw_color_string_store)
};

// Device attributes used for uw_kbd_bl_color
static struct attribute *uw_kbd_bl_color_attrs[] = {
	&uw_kbd_dev_attrs.brightness.attr,
	&uw_kbd_dev_attrs.color_string.attr,
	NULL
};

static struct attribute_group uw_kbd_bl_color_attr_group = {
	.name = "uw_kbd_bl_color",
	.attrs = uw_kbd_bl_color_attrs
};

static void uw_kbd_bl_init_set(void)
{
	if (uniwill_kbd_bl_type_rgb_single_color) {
		// Reset keyboard backlight
		uniwill_write_kbd_bl_reset();
		// Make sure reset finish before continue
		msleep(100);

		// Disable backlight while initializing
		// uniwill_write_kbd_bl_enable(0);

		// Update keyboard backlight according to the current state
		uniwill_write_kbd_bl_state();
	}

	// Enable keyboard backlight
	uniwill_write_kbd_bl_enable(1);
}

// Keep track of previous colors on start, init array with different non-colors
static u32 uw_prev_colors[] = {0x01000000, 0x02000000, 0x03000000};
static u32 uw_prev_colors_size = 3;
static u32 uw_prev_colors_index = 0;

// Timer for checking animation colors
static struct timer_list uw_kbd_bl_init_timer;
static volatile int uw_kbd_bl_check_count = 40;
static int uw_kbd_bl_init_check_interval_ms = 500;

static void uw_kbd_bl_init_ready_check_work_func(struct work_struct *work)
{
	u8 uw_cur_red, uw_cur_green, uw_cur_blue;
	int i;
	bool prev_colors_same;
	uniwill_read_kbd_bl_rgb(&uw_cur_red, &uw_cur_green, &uw_cur_blue);
	uw_prev_colors[uw_prev_colors_index] = (uw_cur_red << 0x10) | (uw_cur_green << 0x08) | uw_cur_blue;
	uw_prev_colors_index = (uw_prev_colors_index + 1) % uw_prev_colors_size;

	prev_colors_same = true;
	for (i = 1; i < uw_prev_colors_size; ++i) {
		if (uw_prev_colors[i-1] != uw_prev_colors[i]) prev_colors_same = false;
	}

	if (prev_colors_same) {
		uw_kbd_bl_init_set();
		del_timer(&uw_kbd_bl_init_timer);
	} else {
		if (uw_kbd_bl_check_count != 0) {
			mod_timer(&uw_kbd_bl_init_timer, jiffies + msecs_to_jiffies(uw_kbd_bl_init_check_interval_ms));
		} else {
			TUXEDO_INFO("uw kbd init timeout, failed to detect end of boot animation\n");
			del_timer(&uw_kbd_bl_init_timer);
		}
	}

	uw_kbd_bl_check_count -= 1;
}

static DECLARE_WORK(uw_kbd_bl_init_ready_check_work, uw_kbd_bl_init_ready_check_work_func);

static void uw_kbd_bl_init_ready_check(struct timer_list *t)
{
	schedule_work(&uw_kbd_bl_init_ready_check_work);
}

static int uw_kbd_bl_init(struct platform_device *dev)
{
	int status = 0;

	uniwill_kbd_bl_type_rgb_single_color = false
		// New names
		| dmi_match(DMI_BOARD_NAME, "POLARIS1501A1650TI")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1501A2060")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1501I1650TI")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1501I2060")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1701A1650TI")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1701A2060")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1701I1650TI")
		| dmi_match(DMI_BOARD_NAME, "POLARIS1701I2060")

		// Old names
		// | dmi_match(DMI_BOARD_NAME, "Polaris15I01")
		// | dmi_match(DMI_BOARD_NAME, "Polaris17I01")
		// | dmi_match(DMI_BOARD_NAME, "Polaris15A01")
		// | dmi_match(DMI_BOARD_NAME, "Polaris1501I2060")
		// | dmi_match(DMI_BOARD_NAME, "Polaris1701I2060")
		;

	// Save previous enable state
	uniwill_kbd_bl_enable_state_on_start = uniwill_read_kbd_bl_enabled();

	if (uniwill_kbd_bl_type_rgb_single_color) {
		// Initialize keyboard backlight driver state according to parameters
		if (param_brightness > UNIWILL_BRIGHTNESS_MAX) param_brightness = UNIWILL_BRIGHTNESS_DEFAULT;
		kbd_led_state_uw.brightness = param_brightness;
		if (color_lookup(&color_list, param_color) <= (u32) 0xffffff) kbd_led_state_uw.color = color_lookup(&color_list, param_color);
		else kbd_led_state_uw.color = UNIWILL_COLOR_DEFAULT;

		// Init sysfs bl attributes group
		status = sysfs_create_group(&dev->dev.kobj, &uw_kbd_bl_color_attr_group);
		if (status) TUXEDO_ERROR("Failed to create sysfs group\n");

		// Start periodic checking of animation, set and enable bl when done
		timer_setup(&uw_kbd_bl_init_timer, uw_kbd_bl_init_ready_check, 0);
		mod_timer(&uw_kbd_bl_init_timer, jiffies + msecs_to_jiffies(uw_kbd_bl_init_check_interval_ms));
	} else {
		// For non-RGB versions
		// Enable keyboard backlight immediately (should it be disabled)
		uniwill_write_kbd_bl_enable(1);
	}

	return status;
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

	uw_kbd_bl_init(dev);

	return 0;

err_remove_notifiers:
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_0);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_1);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_2);

	return -ENODEV;
}

static int uniwill_keyboard_remove(struct platform_device *dev)
{
	// Restore previous backlight enable state
	if (uniwill_kbd_bl_enable_state_on_start != 0xff) {
		sysfs_remove_group(&dev->dev.kobj, &uw_kbd_bl_color_attr_group);
		uniwill_write_kbd_bl_enable(uniwill_kbd_bl_enable_state_on_start);
	}

	unregister_keyboard_notifier(&keyboard_notifier_block);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_0);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_1);
	wmi_remove_notify_handler(UNIWILL_WMI_EVENT_GUID_2);

	del_timer(&uw_kbd_bl_init_timer);

	return 0;
}

static int uniwill_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	uniwill_write_kbd_bl_enable(0);
	return 0;
}

static int uniwill_keyboard_resume(struct platform_device *dev)
{
	if (uniwill_kbd_bl_type_rgb_single_color) {
		uniwill_write_kbd_bl_reset();
		msleep(100);
		uniwill_write_kbd_bl_state();
	}
	uniwill_write_kbd_bl_enable(1);
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
