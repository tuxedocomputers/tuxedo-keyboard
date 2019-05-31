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

#include "tuxedo_keyboard.h"

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
MODULE_VERSION("1.0.0");

// Init and Exit methods
static int __init tuxdeo_keyboard_init(void);
static void __exit tuxdeo_keyboard_exit(void);

static int __init tuxedo_input_init(void);
static void __exit tuxedo_input_exit(void);

// Methods for controlling the Keyboard
static void set_brightness(u8 brightness);
static void set_kb_state(u8 state);
static void set_mode(u8 mode);
static int set_color(u32 region, u32 color);

static int set_color_region(const char *buffer, size_t size, u32 region);

static int tuxedo_wmi_remove(struct platform_device *dev);
static int tuxedo_wmi_resume(struct platform_device *dev);
static int tuxedo_wmi_probe(struct platform_device *dev);
static void tuxedo_wmi_notify(u32 value, void *context);

static int tuxedo_evaluate_method(u32 method_id, u32 arg, u32 * retval);
// Sysfs Interface Methods
// Sysfs Interface for the keyboard state (ON / OFF)
static ssize_t show_state_fs(struct device *child,
                             struct device_attribute *attr, char *buffer);
static ssize_t set_state_fs(struct device *child, struct device_attribute *attr,
                            const char *buffer, size_t size);

// Sysfs Interface for the color of the left side (Color as hexvalue)
static ssize_t show_color_left_fs(struct device *child,
                                  struct device_attribute *attr, char *buffer);
static ssize_t set_color_left_fs(struct device *child,
                                 struct device_attribute *attr,
                                 const char *buffer, size_t size);

// Sysfs Interface for the color of the center (Color as hexvalue)
static ssize_t show_color_center_fs(struct device *child,
                                    struct device_attribute *attr,
                                    char *buffer);
static ssize_t set_color_center_fs(struct device *child,
                                   struct device_attribute *attr,
                                   const char *buffer, size_t size);

// Sysfs Interface for the color of the right side (Color as hexvalue)
static ssize_t show_color_right_fs(struct device *child,
                                   struct device_attribute *attr, char *buffer);
static ssize_t set_color_right_fs(struct device *child,
                                  struct device_attribute *attr,
                                  const char *buffer, size_t size);

// Sysfs Interface for the color of the extra region (Color as hexvalue)
static ssize_t show_color_extra_fs(struct device *child,
                                   struct device_attribute *attr, char *buffer);
static ssize_t set_color_extra_fs(struct device *child,
                                  struct device_attribute *attr,
                                  const char *buffer, size_t size);

// Sysfs Interface for the keyboard brightness (unsigned int)
static ssize_t show_brightness_fs(struct device *child,
                                  struct device_attribute *attr, char *buffer);
static ssize_t set_brightness_fs(struct device *child,
                                 struct device_attribute *attr,
                                 const char *buffer, size_t size);

// Sysfs Interface for the keyboard mode
static ssize_t show_mode_fs(struct device *child, struct device_attribute *attr,
                            char *buffer);
static ssize_t set_mode_fs(struct device *child, struct device_attribute *attr,
                           const char *buffer, size_t size);

// Sysfs Interface for if the keyboard has extra region
static ssize_t show_hasextra_fs(struct device *child,
                                struct device_attribute *attr, char *buffer);
// Sysfs Interface for the keyboard state (ON / OFF)
static ssize_t
show_state_fs(struct device *child, struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", keyboard.state);
}

static ssize_t
set_state_fs(struct device *child, struct device_attribute *attr,
	     const char *buffer, size_t size)
{
	unsigned int val;
	int ret = kstrtouint(buffer, 0, &val);

	if (ret) {
		return ret;
	}

	val = clamp_t(u8, val, 0, 1);

	set_kb_state(val);

	return size;
}

// Sysfs Interface for the color of the left side (Color as hexvalue)
static ssize_t
show_color_left_fs(struct device *child, struct device_attribute *attr,
		   char *buffer)
{
	return sprintf(buffer, "%06x\n", keyboard.color.left);
}

static ssize_t
set_color_left_fs(struct device *child, struct device_attribute *attr,
		  const char *buffer, size_t size)
{
	return set_color_region(buffer, size, REGION_LEFT);
}

// Sysfs Interface for the color of the center (Color as hexvalue)
static ssize_t
show_color_center_fs(struct device *child, struct device_attribute *attr,
		     char *buffer)
{
	return sprintf(buffer, "%06x\n", keyboard.color.center);
}

static ssize_t
set_color_center_fs(struct device *child, struct device_attribute *attr,
		    const char *buffer, size_t size)
{
	return set_color_region(buffer, size, REGION_CENTER);
}

// Sysfs Interface for the color of the right side (Color as hexvalue)
static ssize_t
show_color_right_fs(struct device *child, struct device_attribute *attr,
		    char *buffer)
{
	return sprintf(buffer, "%06x\n", keyboard.color.right);
}

static ssize_t
set_color_right_fs(struct device *child, struct device_attribute *attr,
		   const char *buffer, size_t size)
{
	return set_color_region(buffer, size, REGION_RIGHT);
}

// Sysfs Interface for the color of the extra region (Color as hexvalue)
static ssize_t
show_color_extra_fs(struct device *child, struct device_attribute *attr,
		    char *buffer)
{
	return sprintf(buffer, "%06x\n", keyboard.color.extra);
}

static ssize_t
set_color_extra_fs(struct device *child, struct device_attribute *attr,
		   const char *buffer, size_t size)
{
	return set_color_region(buffer, size, REGION_EXTRA);
}

// Sysfs Interface for the keyboard brightness (unsigned int)
static ssize_t
show_brightness_fs(struct device *child, struct device_attribute *attr,
		   char *buffer)
{
	return sprintf(buffer, "%d\n", keyboard.brightness);
}

static ssize_t
set_brightness_fs(struct device *child, struct device_attribute *attr,
		  const char *buffer, size_t size)
{
	unsigned int val;
	// hier unsigned?
	int ret = kstrtouint(buffer, 0, &val);

	if (ret) {
		return ret;
	}

	val = clamp_t(u8, val, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
	set_brightness(val);

	return size;
}

// Sysfs Interface for the keyboard mode
static ssize_t
show_mode_fs(struct device *child, struct device_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", keyboard.mode);
}

static ssize_t
set_mode_fs(struct device *child, struct device_attribute *attr,
	    const char *buffer, size_t size)
{
	unsigned int val;
	int ret = kstrtouint(buffer, 0, &val);

	if (ret) {
		return ret;
	}

	val = clamp_t(u8, val, 0, ARRAY_SIZE(modes) - 1);
	set_mode(val);

	return size;
}

// Sysfs Interface for if the keyboard has extra region
static ssize_t
show_hasextra_fs(struct device *child, struct device_attribute *attr,
		 char *buffer)
{
	return sprintf(buffer, "%d\n", keyboard.has_extra);
}

static int __init
tuxdeo_keyboard_init(void)
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

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_state) !=
	    0) {
		TUXEDO_ERROR("Sysfs attribute creation failed for state\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_left) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute creation failed for color left\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_center) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute creation failed for color center\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_color_right) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute creation failed for color right\n");
	}

	if (set_color(REGION_EXTRA, KB_COLOR_DEFAULT) != 0) {
		TUXEDO_DEBUG("Keyboard does not support EXTRA Color");
		keyboard.has_extra = 0;
	} else {
		keyboard.has_extra = 1;
		if (device_create_file
		    (&tuxedo_platform_device->dev,
		     &dev_attr_color_extra) != 0) {
			TUXEDO_ERROR
			    ("Sysfs attribute creation failed for color extra\n");
		}

		set_color(REGION_EXTRA, param_color_extra);
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_extra) !=
	    0) {
		TUXEDO_ERROR
		    ("Sysfs attribute creation failed for extra information flag\n");
	}

	if (device_create_file(&tuxedo_platform_device->dev, &dev_attr_mode) !=
	    0) {
		TUXEDO_ERROR("Sysfs attribute creation failed for mode\n");
	}

	if (device_create_file
	    (&tuxedo_platform_device->dev, &dev_attr_brightness) != 0) {
		TUXEDO_ERROR
		    ("Sysfs attribute creation failed for brightness\n");
	}

	keyboard.color.left = param_color_left;
	keyboard.color.center = param_color_center;
	keyboard.color.right = param_color_right;
	keyboard.color.extra = param_color_extra;

	set_color(REGION_LEFT, param_color_left);
	set_color(REGION_CENTER, param_color_center);
	set_color(REGION_RIGHT, param_color_right);

	set_mode(param_mode);
	set_brightness(param_brightness);
	set_kb_state(param_state);

	return 0;
}

static void __exit
tuxdeo_keyboard_exit(void)
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

	if (keyboard.has_extra == 1) {
		device_remove_file(&tuxedo_platform_device->dev,
				   &dev_attr_color_extra);
	}

	platform_device_unregister(tuxedo_platform_device);

	platform_driver_unregister(&tuxedo_platform_driver);

	TUXEDO_DEBUG("exit");
}

static int __init
tuxedo_input_init(void)
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

static void __exit
tuxedo_input_exit(void)
{
	if (unlikely(!tuxedo_input_device)) {
		return;
	}

	input_unregister_device(tuxedo_input_device);
	{
		tuxedo_input_device = NULL;
	}
}

static int
tuxedo_wmi_probe(struct platform_device *dev)
{
	int status;

	status =
	    wmi_install_notify_handler(CLEVO_EVENT_GUID, tuxedo_wmi_notify,
				       NULL);
	// neuer name?
	TUXEDO_DEBUG("clevo_xsm_wmi_probe status: (%0#6x)", status);

	if (unlikely(ACPI_FAILURE(status))) {
		TUXEDO_ERROR("Could not register WMI notify handler (%0#6x)\n",
			     status);
		return -EIO;
	}

	tuxedo_evaluate_method(GET_AP, 0, NULL);

	return 0;
}

static int
tuxedo_wmi_remove(struct platform_device *dev)
{
	wmi_remove_notify_handler(CLEVO_EVENT_GUID);
	return 0;
}

static int
tuxedo_wmi_resume(struct platform_device *dev)
{
	tuxedo_evaluate_method(GET_AP, 0, NULL);

	return 0;
}

static void
tuxedo_wmi_notify(u32 value, void *context)
{
	u32 event;

	tuxedo_evaluate_method(GET_EVENT, 0, &event);
	TUXEDO_DEBUG("WMI event (%0#6x)\n", event);

	switch (event) {
	case WMI_CODE_DECREASE_BACKLIGHT:
		if (keyboard.brightness == BRIGHTNESS_MIN
		    || (keyboard.brightness - 25) < BRIGHTNESS_MIN) {
			set_brightness(BRIGHTNESS_MIN);
		} else {
			set_brightness(keyboard.brightness - 25);
		}

		break;

	case WMI_CODE_INCREASE_BACKLIGHT:
		if (keyboard.brightness == BRIGHTNESS_MAX
		    || (keyboard.brightness + 25) > BRIGHTNESS_MAX) {
			set_brightness(BRIGHTNESS_MAX);
		} else {
			set_brightness(keyboard.brightness + 25);
		}

		break;

	case WMI_CODE_NEXT_MODE:
		set_mode((keyboard.mode + 1) >
			 (ARRAY_SIZE(modes) - 1) ? 0 : (keyboard.mode + 1));
		break;

	case WMI_CODE_TOGGLE_STATE:
		set_kb_state(keyboard.state == 0 ? 1 : 0);
		break;

	default:
		break;
	}
}

static void
set_mode(u8 mode)
{
	TUXEDO_INFO("set_mode on %s", modes[mode].name);

	if (!tuxedo_evaluate_method(SET_KB_LED, modes[mode].value, NULL)) {
		keyboard.mode = mode;
	}

	if (mode == 0) {
		set_color(REGION_LEFT, keyboard.color.left);
		set_color(REGION_CENTER, keyboard.color.center);
		set_color(REGION_RIGHT, keyboard.color.right);

		if (keyboard.has_extra == 1) {
			set_color(REGION_EXTRA, keyboard.color.extra);
		}
	}
}

static void
set_brightness(u8 brightness)
{
	TUXEDO_INFO("Set brightness on %d", brightness);
	if (!tuxedo_evaluate_method(SET_KB_LED, 0xF4000000 | brightness, NULL)) {
		keyboard.brightness = brightness;
	}
}

static void
set_kb_state(u8 state)
{
	u32 cmd = 0xE0000000;
	TUXEDO_INFO("Set keyboard state on: %d\n", state);

	if (state == 0) {
		cmd |= 0x003001;
	} else {
		cmd |= 0x07F001;
	}

	if (!tuxedo_evaluate_method(SET_KB_LED, cmd, NULL)) {
		keyboard.state = state;
	}
}

static int
tuxedo_evaluate_method(u32 method_id, u32 arg, u32 * retval)
{
	struct acpi_buffer in = { (acpi_size) sizeof (arg), &arg };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	u32 tmp;

	TUXEDO_DEBUG("evaluate method: %0#4x  IN : %0#6x\n", method_id, arg);

	status =
	    wmi_evaluate_method(CLEVO_GET_GUID, 0x00, method_id, &in, &out);

	if (unlikely(ACPI_FAILURE(status))) {
		TUXEDO_ERROR("evaluate method error");
		goto exit;
	}

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		tmp = (u32) obj->integer.value;
	} else {
		tmp = 0;
	}

	TUXEDO_DEBUG("%0#4x  OUT: %0#6x (IN: %0#6x)\n", method_id, tmp, arg);

	if (likely(retval)) {
		*retval = tmp;
	}

	kfree(obj);

      exit:
	if (unlikely(ACPI_FAILURE(status))) {
		return -EIO;
	}

	return 0;
}

static int
set_color(u32 region, u32 color)
{
	u32 cset =
	    ((color & 0x0000FF) << 16) | ((color & 0xFF0000) >> 8) |
	    ((color & 0x00FF00) >> 8);
	u32 cmd = region | cset;

	TUXEDO_DEBUG("Set Color '%08x' for region '%08x'", color, region);

	return tuxedo_evaluate_method(SET_KB_LED, cmd, NULL);
}

static int
set_color_region(const char *buffer, size_t size, u32 region)
{
	u32 val;
	int ret = kstrtouint(buffer, 0, &val);

	if (ret) {
		return ret;
	}

	if (!set_color(region, val)) {
		switch (region) {
		case REGION_LEFT:
			keyboard.color.left = val;
			break;
		case REGION_CENTER:
			keyboard.color.center = val;
			break;
		case REGION_RIGHT:
			keyboard.color.right = val;
			break;
		case REGION_EXTRA:
			keyboard.color.extra = val;
			break;
		}
	}

	return size;
}

static int
mode_validator(const char *val, const struct kernel_param *kp)
{
	int mode = 0;
	int ret;

	ret = kstrtoint(val, 10, &mode);
	if (ret != 0 || mode < 0 || mode > (ARRAY_SIZE(modes) - 1)) {
		return -EINVAL;
	}

	return param_set_int(val, kp);
}

static int
brightness_validator(const char *val, const struct kernel_param *kp)
{
	int brightness = 0;
	int ret;

	ret = kstrtoint(val, 10, &brightness);
	if (ret != 0 || brightness < BRIGHTNESS_MIN
	    || brightness > BRIGHTNESS_MAX) {
		return -EINVAL;
	}

	return param_set_int(val, kp);
}

module_init(tuxdeo_keyboard_init);
module_exit(tuxdeo_keyboard_exit);
