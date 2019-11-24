/*
* tuxedo_keyboard.c
*
* Copyright (C) 2018 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#define DRIVER_NAME "tuxedo_keyboard"
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <linux/input.h>

MODULE_AUTHOR
    ("Christian Loritz / TUXEDO Computer GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computer Keyboard Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.0");

/* ::::  Module specific Constants and simple Macros   :::: */

#define __TUXEDO_PR(lvl, fmt, ...) do { pr_##lvl(fmt, ##__VA_ARGS__); } while (0)
#define TUXEDO_INFO(fmt, ...) __TUXEDO_PR(info, fmt, ##__VA_ARGS__)
#define TUXEDO_ERROR(fmt, ...) __TUXEDO_PR(err, fmt, ##__VA_ARGS__)
#define TUXEDO_DEBUG(fmt, ...) __TUXEDO_PR(debug, "[%s:%u] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define BRIGHTNESS_MIN                  0
#define BRIGHTNESS_MAX                  255
#define BRIGHTNESS_DEFAULT              BRIGHTNESS_MAX

#define CLEVO_EVENT_GUID                "ABBC0F6B-8EA1-11D1-00A0-C90629100000"
#define CLEVO_EMAIL_GUID                "ABBC0F6C-8EA1-11D1-00A0-C90629100000"
#define CLEVO_GET_GUID                  "ABBC0F6D-8EA1-11D1-00A0-C90629100000"

#define REGION_LEFT                     0xF0000000
#define REGION_CENTER                   0xF1000000
#define REGION_RIGHT                    0xF2000000
#define REGION_EXTRA                    0xF3000000

#define KEYBOARD_BRIGHTNESS             0xF4000000

/* All these COLOR_* macros are never used in the code, don't know why they are
   here, maybe for documentation purposes. So won't delete for now */
#define COLOR_BLACK                     0x000000
#define COLOR_RED                       0xFF0000
#define COLOR_GREEN                     0x00FF00
#define COLOR_BLUE                      0x0000FF
#define COLOR_YELLOW                    0xFFFF00
#define COLOR_MAGENTA                   0xFF00FF
#define COLOR_CYAN                      0x00FFFF
#define COLOR_WHITE                     0xFFFFFF


#define KB_COLOR_DEFAULT                COLOR_WHITE   // Default Color: White
#define DEFAULT_BLINKING_PATTERN        0

// Submethod IDs for the CLEVO_GET WMI method
#define WMI_SUBMETHOD_ID_GET_EVENT      0x01
#define WMI_SUBMETHOD_ID_GET_AP         0x46
#define WMI_SUBMETHOD_ID_SET_KB_LEDS    0x67 /* used to set color, brightness,
	                                        blinking pattern, etc. */


// WMI Event Codes
#define WMI_KEYEVENT_CODE_DECREASE_BACKLIGHT     0x81
#define WMI_KEYEVENT_CODE_INCREASE_BACKLIGHT     0x82
#define WMI_KEYEVENT_CODE_NEXT_BLINKING_PATTERN  0x83
#define WMI_KEYEVENT_CODE_TOGGLE_STATE           0x9F

#define BRIGHTNESS_STEP            25

struct color_t {
	u32 code;
	char* name;
};

struct color_list_t {
	uint size;
	struct color_t colors[];
};

// Keyboard struct
struct kbd_led_state_t {
	u8 has_extra;
	u8 enabled;

	struct {
		u32 left;
		u32 center;
		u32 right;
		u32 extra;
	} color;

	u8 brightness;
	u8 blinking_pattern;
	u8 whole_kbd_color;
};

struct blinking_pattern_t {
	u8 key;
	u32 value;
	const char *const name;
};

struct platform_device *tuxedo_platform_device;
static struct input_dev *tuxedo_input_device;

// Param Validators
static int blinking_pattern_id_validator(const char *value,
                                         const struct kernel_param *blinking_pattern_param);
static const struct kernel_param_ops param_ops_mode_ops = {
	.set = blinking_pattern_id_validator,
	.get = param_get_int,
};

static int brightness_validator(const char *val,
                                const struct kernel_param *brightness_param);
static const struct kernel_param_ops param_ops_brightness_ops = {
	.set = brightness_validator,
	.get = param_get_int,
};

// Module Parameters
static uint param_color_left = KB_COLOR_DEFAULT;
module_param_named(color_left, param_color_left, uint, S_IRUSR);
MODULE_PARM_DESC(color_left, "Color for the Left Region");

static uint param_color_center = KB_COLOR_DEFAULT;
module_param_named(color_center, param_color_center, uint, S_IRUSR);
MODULE_PARM_DESC(color_center, "Color for the Center Region");

static uint param_color_right = KB_COLOR_DEFAULT;
module_param_named(color_right, param_color_right, uint, S_IRUSR);
MODULE_PARM_DESC(color_right, "Color for the Right Region");

static uint param_color_extra = KB_COLOR_DEFAULT;
module_param_named(color_extra, param_color_extra, uint, S_IRUSR);
MODULE_PARM_DESC(color_extra, "Color for the Extra Region");

static ushort param_blinking_pattern = DEFAULT_BLINKING_PATTERN;
module_param_cb(mode, &param_ops_mode_ops, &param_blinking_pattern, S_IRUSR);
MODULE_PARM_DESC(mode, "Set the keyboard backlight blinking pattern");

static ushort param_brightness = BRIGHTNESS_DEFAULT;
module_param_cb(brightness, &param_ops_brightness_ops, &param_brightness,
		S_IRUSR);
MODULE_PARM_DESC(brightness, "Set the Keyboard Brightness");

static bool param_state = true;
module_param_named(state, param_state, bool, S_IRUSR);
MODULE_PARM_DESC(state,
		 "Set the State of the Keyboard TRUE = ON | FALSE = OFF");

static struct kbd_led_state_t kbd_led_state = {
	.has_extra = 0,
	.enabled = 1,
	.color = {
	        .left = KB_COLOR_DEFAULT, .center = KB_COLOR_DEFAULT,
	        .right = KB_COLOR_DEFAULT, .extra = KB_COLOR_DEFAULT
	         },
	.brightness = BRIGHTNESS_DEFAULT,
	.blinking_pattern = DEFAULT_BLINKING_PATTERN,
	.whole_kbd_color = 7
};

static struct color_list_t color_list = {
	.size = 8,
	.colors = {
	       { .name = "BLACK",    .code = 0x000000 },  // 0
	       { .name = "RED",      .code = 0xFF0000 },  // 1
	       { .name = "GREEN",    .code = 0x00FF00 },  // 2
	       { .name = "BLUE",     .code = 0x0000FF },  // 3
	       { .name = "YELLOW",   .code = 0xFFFF00 },  // 4
	       { .name = "MAGENTA",  .code = 0xFF00FF },  // 5
	       { .name = "CYAN",     .code = 0x00FFFF },  // 6
	       { .name = "WHITE",    .code = 0xFFFFFF },  // 7
	}
};

static struct blinking_pattern_t blinking_patterns[] = {
        { .key = 0,.value = 0,.name = "CUSTOM"},
        { .key = 1,.value = 0x1002a000,.name = "BREATHE"},
        { .key = 2,.value = 0x33010000,.name = "CYCLE"},
        { .key = 3,.value = 0x80000000,.name = "DANCE"},
        { .key = 4,.value = 0xA0000000,.name = "FLASH"},
        { .key = 5,.value = 0x70000000,.name = "RANDOM_COLOR"},
        { .key = 6,.value = 0x90000000,.name = "TEMPO"},
        { .key = 7,.value = 0xB0000000,.name = "WAVE"}
};

// Sysfs Interface Methods
// Sysfs Interface for the keyboard state (ON / OFF)
static ssize_t show_state_fs(struct device *child,
			     struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.enabled);
}

// Sysfs Interface for the color of the left side (Color as hexvalue)
static ssize_t show_color_left_fs(struct device *child,
				  struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%06x\n", kbd_led_state.color.left);
}

// Sysfs Interface for the color of the center (Color as hexvalue)
static ssize_t show_color_center_fs(struct device *child,
				    struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%06x\n", kbd_led_state.color.center);
}

// Sysfs Interface for the color of the right side (Color as hexvalue)
static ssize_t show_color_right_fs(struct device *child,
				   struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%06x\n", kbd_led_state.color.right);
}

// Sysfs Interface for the color of the extra region (Color as hexvalue)
static ssize_t show_color_extra_fs(struct device *child,
				   struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%06x\n", kbd_led_state.color.extra);
}

// Sysfs Interface for the keyboard brightness (unsigned int)
static ssize_t show_brightness_fs(struct device *child,
				  struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.brightness);
}

// Sysfs Interface for the backlight blinking pattern
static ssize_t show_blinking_patterns_fs(struct device *child, struct device_attribute *attr,
                                         char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.blinking_pattern);
}

// Sysfs Interface for if the keyboard has extra region
static ssize_t show_hasextra_fs(struct device *child,
				struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.has_extra);
}

static int tuxedo_evaluate_wmi_method(u32 submethod_id, u32 submethod_arg, u32 * retval)
{
	struct acpi_buffer acpi_input = { (acpi_size) sizeof(submethod_arg), &submethod_arg };
	struct acpi_buffer acpi_output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	u32 wmi_output;

	TUXEDO_DEBUG("evaluate wmi method: %0#4x  IN : %0#6x\n", submethod_id, submethod_arg);

	status = wmi_evaluate_method(CLEVO_GET_GUID, 0x00, submethod_id,
	                             &acpi_input, &acpi_output);

	if (unlikely(ACPI_FAILURE(status))) {
		TUXEDO_ERROR("evaluate_wmi_method error");
		return -EIO;
	}

	obj = (union acpi_object *)acpi_output.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		wmi_output = (u32) obj->integer.value;
	} else {
		wmi_output = 0;
	}

	TUXEDO_DEBUG("WMI submethod %0#4x output: %0#6x (input: %0#6x)\n",
	             submethod_id, wmi_output, submethod_arg);

	if (likely(retval)) { /* if no NULL pointer */
		*retval = wmi_output;
	}

	kfree(obj);
	return 0;
}

static void set_brightness(u8 brightness)
{
	TUXEDO_INFO("Set brightness on %d", brightness);
	if (!tuxedo_evaluate_wmi_method
	    (WMI_SUBMETHOD_ID_SET_KB_LEDS, 0xF4000000 | brightness, NULL)) {
		kbd_led_state.brightness = brightness;
	}
}

static ssize_t set_brightness_fs(struct device *child,
                                 struct device_attribute *attr,
                                 const char *buffer, size_t size)
{
	unsigned int val;
	// hier unsigned?

	int err = kstrtouint(buffer, 0, &val);
	if (err) {
		return err;
	}

	val = clamp_t(u8, val, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
	set_brightness(val);

	return size;
}

static void set_enabled(u8 state)
{
	u32 cmd = 0xE0000000;
	TUXEDO_INFO("Set keyboard enabled to: %d\n", state);

	if (state == 0) {
		cmd |= 0x003001;
	} else {
		cmd |= 0x07F001;
	}

	if (!tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_SET_KB_LEDS, cmd, NULL)) {
		kbd_led_state.enabled = state;
	}
}

static ssize_t set_state_fs(struct device *child, struct device_attribute *attr,
			    const char *buffer, size_t size)
{
	unsigned int state;

	int err = kstrtouint(buffer, 0, &state);
	if (err) {
		return err;
	}

	state = clamp_t(u8, state, 0, 1);

	set_enabled(state);

	return size;
}

static int set_color(u32 region, u32 color)
{
	u32 cset =
	    ((color & 0x0000FF) << 16) | ((color & 0xFF0000) >> 8) |
	    ((color & 0x00FF00) >> 8);
	u32 wmi_submethod_arg = region | cset;

	TUXEDO_DEBUG("Set Color '%08x' for region '%08x'", color, region);

	return tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_SET_KB_LEDS, wmi_submethod_arg, NULL);
}
static int set_color_code_region(u32 region, u32 colorcode)
{
	int err;
	if (0 == (err = set_color(region, colorcode))) {
		// after succesfully setting color, update our state struct
		// depending on which region was changed
		switch (region) {
		case REGION_LEFT:
			kbd_led_state.color.left = colorcode;
			break;
		case REGION_CENTER:
			kbd_led_state.color.center = colorcode;
			break;
		case REGION_RIGHT:
			kbd_led_state.color.right = colorcode;
			break;
		case REGION_EXTRA:
			kbd_led_state.color.extra = colorcode;
			break;
		}
	}

	return err;
}

static int set_color_string_region(const char *color_string, size_t size, u32 region)
{
	u32 colorcode;
	int err = kstrtouint(color_string, 0, &colorcode);

	if (err) {
		return err;
	}

	if (!set_color(region, colorcode)) {
		// after succesfully setting color, update our state struct
		// depending on which region was changed
		switch (region) {
		case REGION_LEFT:
			kbd_led_state.color.left = colorcode;
			break;
		case REGION_CENTER:
			kbd_led_state.color.center = colorcode;
			break;
		case REGION_RIGHT:
			kbd_led_state.color.right = colorcode;
			break;
		case REGION_EXTRA:
			kbd_led_state.color.extra = colorcode;
			break;
		}
	}

	return size;
}

static ssize_t set_color_left_fs(struct device *child,
				 struct device_attribute *attr,
                                 const char *color_string, size_t size)
{
	return set_color_string_region(color_string, size, REGION_LEFT);
}

static ssize_t set_color_center_fs(struct device *child,
				   struct device_attribute *attr,
                                   const char *color_string, size_t size)
{
	return set_color_string_region(color_string, size, REGION_CENTER);
}

static ssize_t set_color_right_fs(struct device *child,
				  struct device_attribute *attr,
                                  const char *color_string, size_t size)
{
	return set_color_string_region(color_string, size, REGION_RIGHT);
}

static ssize_t set_color_extra_fs(struct device *child,
				  struct device_attribute *attr,
                                  const char *color_string, size_t size)
{
	return set_color_string_region(color_string, size, REGION_EXTRA);
}

static int set_next_color_whole_kb(void)
{
	/* "Calculate" new to-be color */
	u32 new_color_id;
	u32 new_color_code;

	new_color_id = kbd_led_state.whole_kbd_color + 1;
	if (new_color_id >= color_list.size) {
		new_color_id = 0;
	}
	new_color_code = color_list.colors[new_color_id].code;

	TUXEDO_INFO("set_next_color_whole_kb(): new_color_id: %i, new_color_code %X", 
		    new_color_id, new_color_code);

	/* Set color on all four regions*/
	// TODO: perhaps use set_color_region here, because of better struct state
	// handling (or implement something like it myself)
	set_color_code_region(REGION_LEFT,   new_color_code);
	set_color_code_region(REGION_CENTER, new_color_code);
	set_color_code_region(REGION_RIGHT,  new_color_code);
	set_color_code_region(REGION_EXTRA,  new_color_code);

	kbd_led_state.whole_kbd_color = new_color_id;

	return 0;
}

static void set_blinking_pattern(u8 blinkling_pattern)
{
	TUXEDO_INFO("set_mode on %s", blinking_patterns[blinkling_pattern].name);

	if (!tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_SET_KB_LEDS, blinking_patterns[blinkling_pattern].value, NULL)) {
		// wmi method was succesfull so update ur internal state struct
		kbd_led_state.blinking_pattern = blinkling_pattern;
	}

	if (blinkling_pattern == 0) {  // 0 is the "custom" blinking pattern

		// so just set all regions to the stored colors
		set_color(REGION_LEFT, kbd_led_state.color.left);
		set_color(REGION_CENTER, kbd_led_state.color.center);
		set_color(REGION_RIGHT, kbd_led_state.color.right);

		if (kbd_led_state.has_extra == 1) {
			set_color(REGION_EXTRA, kbd_led_state.color.extra);
		}
	}
}

static ssize_t set_blinking_pattern_fs(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size)
{
	unsigned int blinking_pattern;

	int err = kstrtouint(buffer, 0, &blinking_pattern);
	if (err) {
		return err;
	}

	blinking_pattern = clamp_t(u8, blinking_pattern, 0, ARRAY_SIZE(blinking_patterns) - 1);
	set_blinking_pattern(blinking_pattern);

	return size;
}

static int blinking_pattern_id_validator(const char *value,
                                         const struct kernel_param *blinking_pattern_param)
{
	int blinking_pattern = 0;

	if (kstrtoint(value, 10, &blinking_pattern) != 0
	    || blinking_pattern < 0
	    || blinking_pattern > (ARRAY_SIZE(blinking_patterns) - 1)) {
		return -EINVAL;
	}

	return param_set_int(value, blinking_pattern_param);
}

static int brightness_validator(const char *value,
                                const struct kernel_param *brightness_param)
{
	int brightness = 0;

	if (kstrtoint(value, 10, &brightness) != 0
	    || brightness < BRIGHTNESS_MIN
	    || brightness > BRIGHTNESS_MAX) {
		return -EINVAL;
	}

	return param_set_int(value, brightness_param);
}

static void tuxedo_wmi_notify(u32 value, void *context)
{
	u32 key_event;

	tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_GET_EVENT, 0, &key_event);
	TUXEDO_DEBUG("WMI event (%0#6x)\n", key_event);

	switch (key_event) {
	case WMI_KEYEVENT_CODE_DECREASE_BACKLIGHT:
		if (kbd_led_state.brightness == BRIGHTNESS_MIN
		    || (kbd_led_state.brightness - 25) < BRIGHTNESS_MIN) {
			set_brightness(BRIGHTNESS_MIN);
		} else {
			set_brightness(kbd_led_state.brightness - 25);
		}

		break;

	case WMI_KEYEVENT_CODE_INCREASE_BACKLIGHT:
		if (kbd_led_state.brightness == BRIGHTNESS_MAX
		    || (kbd_led_state.brightness + 25) > BRIGHTNESS_MAX) {
			set_brightness(BRIGHTNESS_MAX);
		} else {
			set_brightness(kbd_led_state.brightness + 25);
		}

		break;

//	case WMI_CODE_NEXT_BLINKING_PATTERN:
//		set_blinking_pattern((kbd_led_state.blinking_pattern + 1) >
//		         (ARRAY_SIZE(blinking_patterns) - 1) ? 0 : (kbd_led_state.blinking_pattern + 1));
//		break;

	case WMI_KEYEVENT_CODE_NEXT_BLINKING_PATTERN:
		set_next_color_whole_kb();
		break;

	case WMI_KEYEVENT_CODE_TOGGLE_STATE:
		set_enabled(kbd_led_state.enabled == 0 ? 1 : 0);
		break;

	default:
		break;
	}
}

static int tuxedo_wmi_probe(struct platform_device *dev)
{
	int  status = wmi_install_notify_handler(CLEVO_EVENT_GUID, tuxedo_wmi_notify, NULL);

	// neuer name?
	TUXEDO_DEBUG("clevo_xsm_wmi_probe status: (%0#6x)", status);

	if (unlikely(ACPI_FAILURE(status))) {
		TUXEDO_ERROR("Could not register WMI notify handler (%0#6x)\n", status);
		return -EIO;
	}

	tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_GET_AP, 0, NULL);

	return 0;
}

static int tuxedo_wmi_remove(struct platform_device *dev)
{
	wmi_remove_notify_handler(CLEVO_EVENT_GUID);
	return 0;
}

static int tuxedo_wmi_resume(struct platform_device *dev)
{
	tuxedo_evaluate_wmi_method(WMI_SUBMETHOD_ID_GET_AP, 0, NULL);

	return 0;
}

static struct platform_driver tuxedo_platform_driver = {
	.remove = tuxedo_wmi_remove,
	.resume = tuxedo_wmi_resume,
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
};

// Sysfs attribute file permissions and method linking
static DEVICE_ATTR(state, 0644, show_state_fs, set_state_fs);
static DEVICE_ATTR(color_left, 0644, show_color_left_fs, set_color_left_fs);
static DEVICE_ATTR(color_center, 0644, show_color_center_fs,
		   set_color_center_fs);
static DEVICE_ATTR(color_right, 0644, show_color_right_fs, set_color_right_fs);
static DEVICE_ATTR(color_extra, 0644, show_color_extra_fs, set_color_extra_fs);
static DEVICE_ATTR(brightness, 0644, show_brightness_fs, set_brightness_fs);
static DEVICE_ATTR(mode, 0644, show_blinking_patterns_fs, set_blinking_pattern_fs);
static DEVICE_ATTR(extra, 0444, show_hasextra_fs, NULL);

// register our fake input device
// from the wmi events we generate fake input events and send them up to user space
// as if the were normale sane input events or key presses
// TODO: Check if the input_device is actually used currently, maybe comment it out
// until needed
static int __init tuxedo_input_init(void)
{
	int err;

	tuxedo_input_device = input_allocate_device();
	if (unlikely(!tuxedo_input_device)) {
		TUXEDO_ERROR("Error allocating input device\n");
		return -ENOMEM;
	}

	tuxedo_input_device->name = "TUXEDO Keyboard";
	tuxedo_input_device->phys = DRIVER_NAME "/input0";
	tuxedo_input_device->id.bustype = BUS_HOST;
	tuxedo_input_device->dev.parent = &tuxedo_platform_device->dev;

	set_bit(EV_KEY, tuxedo_input_device->evbit);

	err = input_register_device(tuxedo_input_device);
	if (unlikely(err)) {
		TUXEDO_ERROR("Error registering input device\n");
		goto err_free_input_device;
	}

	return 0;

err_free_input_device:
	input_free_device(tuxedo_input_device);

	return err;
}

static void __exit tuxedo_input_exit(void)
{
	if (unlikely(!tuxedo_input_device)) {
		return;
	}

	input_unregister_device(tuxedo_input_device);
	{
		tuxedo_input_device = NULL;
	}
}

static int __init tuxdeo_keyboard_init(void)
{
	int err;

	if (!wmi_has_guid(CLEVO_EVENT_GUID)) {
		TUXEDO_ERROR("No known WMI event notification GUID found\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(CLEVO_GET_GUID)) {
		TUXEDO_ERROR("No known WMI control method GUID found\n");
		return -ENODEV;
	}

	TUXEDO_INFO("Model '%s' found\n",
		    dmi_get_system_info(DMI_PRODUCT_NAME));

	tuxedo_platform_device =
	    platform_create_bundle(&tuxedo_platform_driver, tuxedo_wmi_probe,
				   NULL, 0, NULL, 0);

	if (unlikely(IS_ERR(tuxedo_platform_device))) {
		TUXEDO_ERROR("Can not init Platform driver");
		return PTR_ERR(tuxedo_platform_device);
	}

	err = tuxedo_input_init();
	if (unlikely(err)) {
		TUXEDO_ERROR("Could not register input device\n");
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_state) != 0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for state\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_left) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color left\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_center) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color center\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_right) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color right\n");
	}

	if (set_color(REGION_EXTRA, KB_COLOR_DEFAULT) != 0) {
		TUXEDO_DEBUG("Keyboard does not support EXTRA Color");
		kbd_led_state.has_extra = 0;
	} else {
		kbd_led_state.has_extra = 1;
		if (device_create_file
		    (&tuxedo_platform_device->dev,
		     &dev_attr_color_extra) != 0) {
			TUXEDO_ERROR
			    ("Sysfs attribute file creation failed for color extra\n");
		}

		set_color(REGION_EXTRA, param_color_extra);
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_extra) !=
	    0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for extra information flag\n");
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_mode) !=
	    0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for blinking pattern\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_brightness) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for brightness\n");
	}

	kbd_led_state.color.left = param_color_left;
	kbd_led_state.color.center = param_color_center;
	kbd_led_state.color.right = param_color_right;
	kbd_led_state.color.extra = param_color_extra;

	set_color(REGION_LEFT, param_color_left);
	set_color(REGION_CENTER, param_color_center);
	set_color(REGION_RIGHT, param_color_right);

	set_blinking_pattern(param_blinking_pattern);
	set_brightness(param_brightness);
	set_enabled(param_state);

	return 0;
}

static void __exit tuxdeo_keyboard_exit(void)
{
	tuxedo_input_exit();

	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_state);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_color_left);
	device_remove_file(&tuxedo_platform_device->dev,
			   &dev_attr_color_center);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_color_right);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_extra);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_mode);
	device_remove_file(&tuxedo_platform_device->dev, &dev_attr_brightness);

	if (kbd_led_state.has_extra == 1) {
		device_remove_file(&tuxedo_platform_device->dev,
				   &dev_attr_color_extra);
	}

	platform_device_unregister(tuxedo_platform_device);

	platform_driver_unregister(&tuxedo_platform_driver);

	TUXEDO_DEBUG("exit");
}

module_init(tuxdeo_keyboard_init);
module_exit(tuxdeo_keyboard_exit);
