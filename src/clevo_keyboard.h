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
#include "tuxedo_keyboard_common.h"
#include "clevo_interfaces.h"

#define BRIGHTNESS_MIN                  0
#define BRIGHTNESS_MAX                  255
#define BRIGHTNESS_DEFAULT              BRIGHTNESS_MAX

#define REGION_LEFT                     0xF0000000
#define REGION_CENTER                   0xF1000000
#define REGION_RIGHT                    0xF2000000
#define REGION_EXTRA                    0xF3000000

#define KEYBOARD_BRIGHTNESS             0xF4000000

#define KB_COLOR_DEFAULT		0xFFFFFF // White
#define DEFAULT_BLINKING_PATTERN        0

// Submethod IDs for the CLEVO_GET interface method
#define CLEVO_METHOD_ID_GET_EVENT	0x01
#define CLEVO_METHOD_ID_GET_AP		0x46
#define CLEVO_METHOD_ID_SET_KB_LEDS	0x67 /* used to set color, brightness,
	                                        blinking pattern, etc. */


// Clevo event codes
#define CLEVO_EVENT_DECREASE_BACKLIGHT     0x81
#define CLEVO_EVENT_INCREASE_BACKLIGHT     0x82
#define CLEVO_EVENT_NEXT_BLINKING_PATTERN  0x83
#define CLEVO_EVENT_TOGGLE_STATE           0x9F

#define CLEVO_EVENT_CYCLE_BRIGHTNESS       0x8A
#define CLEVO_EVENT_TOUCHPAD_TOGGLE        0x5D
#define CLEVO_EVENT_TOUCHPAD_OFF           0xFC
#define CLEVO_EVENT_TOUCHPAD_ON            0xFD

#define CLEVO_EVENT_RFKILL1                0x85
#define CLEVO_EVENT_RFKILL2                0x86

static struct clevo_interfaces_t {
	struct clevo_interface_t *wmi;
	struct clevo_interface_t *acpi;
} clevo_interfaces;

struct clevo_interface_t *active_clevo_interface;

void clevo_keyboard_write_state(void);
void clevo_keyboard_event_callb(u32 event);

static DEFINE_MUTEX(clevo_keyboard_interface_modification_lock);

u32 clevo_keyboard_add_interface(struct clevo_interface_t *new_interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(new_interface->string_id, "clevo_wmi") == 0) {
		clevo_interfaces.wmi = new_interface;
		clevo_interfaces.wmi->event_callb = clevo_keyboard_event_callb;

		// Only use wmi if there is no other current interface
		if (ZERO_OR_NULL_PTR(active_clevo_interface)) {
			pr_debug("enable wmi events\n");
			clevo_interfaces.wmi->method_call(0x46, 0, NULL);

			active_clevo_interface = clevo_interfaces.wmi;
		}

	} else if (strcmp(new_interface->string_id, "clevo_acpi") == 0) {
		clevo_interfaces.acpi = new_interface;
		clevo_interfaces.acpi->event_callb = clevo_keyboard_event_callb;

		pr_debug("enable acpi events (takes priority)\n");
		clevo_interfaces.acpi->method_call(0x46, 0, NULL);
		active_clevo_interface = clevo_interfaces.acpi;
	} else {
		// Not recognized interface
		pr_err("unrecognized interface\n");
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	clevo_keyboard_write_state();

	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_add_interface);

u32 clevo_keyboard_remove_interface(struct clevo_interface_t *interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(interface->string_id, "clevo_wmi") == 0) {
		clevo_interfaces.wmi = NULL;
	} else if (strcmp(interface->string_id, "clevo_acpi") == 0) {
		clevo_interfaces.acpi = NULL;
	} else {
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	if (active_clevo_interface == interface)
		active_clevo_interface = NULL;

	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_remove_interface);

struct tuxedo_keyboard_driver clevo_keyboard_driver;

static struct key_entry clevo_keymap[] = {
	// Keyboard backlight (RGB versions)
	{ KE_KEY, CLEVO_EVENT_DECREASE_BACKLIGHT, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, CLEVO_EVENT_INCREASE_BACKLIGHT, { KEY_KBDILLUMUP } },
	{ KE_KEY, CLEVO_EVENT_TOGGLE_STATE, { KEY_KBDILLUMTOGGLE } },
	{ KE_KEY, CLEVO_EVENT_NEXT_BLINKING_PATTERN, { KEY_LIGHTS_TOGGLE } },
	// Single cycle key (white only versions)
	{ KE_KEY, CLEVO_EVENT_CYCLE_BRIGHTNESS, { KEY_KBDILLUMUP } },

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

#define BRIGHTNESS_STEP            25

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


static int blinking_pattern_id_validator(const char *value,
                                         const struct kernel_param *blinking_pattern_param);
static const struct kernel_param_ops param_ops_mode_ops = {
	.set = blinking_pattern_id_validator,
	.get = param_get_int,
};

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

u32 clevo_evaluate_method(u8 cmd, u32 arg, u32 *result)
{
	if (IS_ERR_OR_NULL(active_clevo_interface)) {
		pr_err("clevo_keyboard: no active interface\n");
		return -ENODEV;
	}
	return active_clevo_interface->method_call(cmd, arg, result);
}
EXPORT_SYMBOL(clevo_evaluate_method);

u32 clevo_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(active_clevo_interface))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = active_clevo_interface->string_id;

	return 0;
}
EXPORT_SYMBOL(clevo_get_active_interface_id);

static void set_brightness(u8 brightness)
{
	TUXEDO_INFO("Set brightness on %d", brightness);
	if (!clevo_evaluate_method
	    (CLEVO_METHOD_ID_SET_KB_LEDS, 0xF4000000 | brightness, NULL)) {
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

static int set_enabled_cmd(u8 state)
{
	u32 cmd = 0xE0000000;
	TUXEDO_INFO("Set keyboard enabled to: %d\n", state);

	if (state == 0) {
		cmd |= 0x003001;
	} else {
		cmd |= 0x07F001;
	}

	return clevo_evaluate_method(CLEVO_METHOD_ID_SET_KB_LEDS, cmd, NULL);
}

static void set_enabled(u8 state)
{
	if (!set_enabled_cmd(state)) {
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
	u32 clevo_submethod_arg = region | cset;

	TUXEDO_DEBUG("Set Color '%08x' for region '%08x'", color, region);

	return clevo_evaluate_method(CLEVO_METHOD_ID_SET_KB_LEDS, clevo_submethod_arg, NULL);
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

	if (!clevo_evaluate_method(CLEVO_METHOD_ID_SET_KB_LEDS, blinking_patterns[blinkling_pattern].value, NULL)) {
		// method was succesfull so update ur internal state struct
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

void clevo_keyboard_event_callb(u32 event)
{
	u32 key_event = event;

	// TUXEDO_DEBUG("clevo event: %0#6x\n", event);

	switch (key_event) {
	case CLEVO_EVENT_DECREASE_BACKLIGHT:
		if (kbd_led_state.brightness == BRIGHTNESS_MIN
		    || (kbd_led_state.brightness - 25) < BRIGHTNESS_MIN) {
			set_brightness(BRIGHTNESS_MIN);
		} else {
			set_brightness(kbd_led_state.brightness - 25);
		}

		break;

	case CLEVO_EVENT_INCREASE_BACKLIGHT:
		if (kbd_led_state.brightness == BRIGHTNESS_MAX
		    || (kbd_led_state.brightness + 25) > BRIGHTNESS_MAX) {
			set_brightness(BRIGHTNESS_MAX);
		} else {
			set_brightness(kbd_led_state.brightness + 25);
		}

		break;

//	case CLEVO_EVENT_NEXT_BLINKING_PATTERN:
//		set_blinking_pattern((kbd_led_state.blinking_pattern + 1) >
//		         (ARRAY_SIZE(blinking_patterns) - 1) ? 0 : (kbd_led_state.blinking_pattern + 1));
//		break;

	case CLEVO_EVENT_NEXT_BLINKING_PATTERN:
		set_next_color_whole_kb();
		break;

	case CLEVO_EVENT_TOGGLE_STATE:
		set_enabled(kbd_led_state.enabled == 0 ? 1 : 0);
		break;

	default:
		break;
	}

	if (current_driver != NULL && current_driver->input_device != NULL) {
		if (!sparse_keymap_report_known_event(
			    current_driver->input_device, key_event, 1, true)) {
			TUXEDO_DEBUG("Unknown key - %d (%0#6x)\n", key_event,
				     key_event);
		}
	}
}

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

static void clevo_keyboard_init_device_interface(struct platform_device *dev)
{
	// Setup sysfs
	if (device_create_file(&dev->dev, &dev_attr_state) != 0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for state\n");
	}

	if (device_create_file
	    (&dev->dev, &dev_attr_color_left) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color left\n");
	}

	if (device_create_file
	    (&dev->dev, &dev_attr_color_center) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color center\n");
	}

	if (device_create_file
	    (&dev->dev, &dev_attr_color_right) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for color right\n");
	}

	if (set_color(REGION_EXTRA, KB_COLOR_DEFAULT) != 0) {
		TUXEDO_DEBUG("Keyboard does not support EXTRA Color");
		kbd_led_state.has_extra = 0;
	} else {
		kbd_led_state.has_extra = 1;
		if (device_create_file
		    (&dev->dev,
		     &dev_attr_color_extra) != 0) {
			TUXEDO_ERROR
			    ("Sysfs attribute file creation failed for color extra\n");
		}

		set_color(REGION_EXTRA, param_color_extra);
	}

	if (device_create_file(&dev->dev, &dev_attr_extra) !=
	    0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for extra information flag\n");
	}

	if (device_create_file(&dev->dev, &dev_attr_mode) !=
	    0) {
		TUXEDO_ERROR("Sysfs attribute file creation failed for blinking pattern\n");
	}

	if (device_create_file
	    (&dev->dev, &dev_attr_brightness) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute file creation failed for brightness\n");
	}

	// Set state variables
	kbd_led_state.color.left = param_color_left;
	kbd_led_state.color.center = param_color_center;
	kbd_led_state.color.right = param_color_right;
	kbd_led_state.color.extra = param_color_extra;
}

void clevo_keyboard_write_state(void)
{
	// Write state
	set_color(REGION_LEFT, param_color_left);
	set_color(REGION_CENTER, param_color_center);
	set_color(REGION_RIGHT, param_color_right);

	set_blinking_pattern(param_blinking_pattern);
	if (param_brightness > BRIGHTNESS_MAX) param_brightness = BRIGHTNESS_DEFAULT;
	set_brightness(param_brightness);
	set_enabled(param_state);
}

static int clevo_keyboard_probe_only_init(struct platform_device *dev)
{
	clevo_keyboard_init_device_interface(dev);
	return 0;
}

static void clevo_keyboard_remove_device_interface(struct platform_device *dev)
{
	device_remove_file(&dev->dev, &dev_attr_state);
	device_remove_file(&dev->dev, &dev_attr_color_left);
	device_remove_file(&dev->dev, &dev_attr_color_center);
	device_remove_file(&dev->dev, &dev_attr_color_right);
	device_remove_file(&dev->dev, &dev_attr_extra);
	device_remove_file(&dev->dev, &dev_attr_mode);
	device_remove_file(&dev->dev, &dev_attr_brightness);

	if (kbd_led_state.has_extra == 1) {
		device_remove_file(&dev->dev, &dev_attr_color_extra);
	}
}

static int clevo_keyboard_remove(struct platform_device *dev)
{
	clevo_keyboard_remove_device_interface(dev);
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
	clevo_evaluate_method(CLEVO_METHOD_ID_GET_AP, 0, NULL);

	set_color(REGION_LEFT, kbd_led_state.color.left);
	set_color(REGION_CENTER, kbd_led_state.color.center);
	set_color(REGION_RIGHT, kbd_led_state.color.right);
	if (kbd_led_state.has_extra)
		set_color(REGION_EXTRA, kbd_led_state.color.extra);

	set_blinking_pattern(kbd_led_state.blinking_pattern);
	set_brightness(kbd_led_state.brightness);
	set_enabled(kbd_led_state.enabled);

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

struct tuxedo_keyboard_driver clevo_keyboard_driver_v2 = {
	.platform_driver = &platform_driver_clevo,
	.probe = clevo_keyboard_probe_only_init,
	.key_map = clevo_keymap,
};

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

int clevo_keyboard_init(void)
{
	bool performance_profile_set_workaround;

	tuxedo_keyboard_init_driver(&clevo_keyboard_driver_v2);

	// Workaround for firmware issue not setting selected performance profile.
	// Explicitly set "performance" perf. profile on init regardless of what is chosen
	// for these devices (Aura, XP14, IBS14v5)
	performance_profile_set_workaround = false
		|| dmi_string_in(DMI_BOARD_NAME, "AURA1501")
		|| dmi_string_in(DMI_BOARD_NAME, "NL5xRU")
		|| dmi_string_in(DMI_BOARD_NAME, "NV4XMB,ME,MZ")
		|| dmi_string_in(DMI_BOARD_NAME, "L140CU")
		;
	if (performance_profile_set_workaround) {
		TUXEDO_INFO("Performance profile 'performance' set workaround applied\n");
		clevo_evaluate_method(0x79, 0x19000002, NULL);
	}

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_init);
