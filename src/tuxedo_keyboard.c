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

struct platform_device *tuxedo_platform_device;
static struct input_dev *tuxedo_input_device;

// Param Validators
static int blinking_pattern_id_validator(const char *val, const struct kernel_param *kp);
static const struct kernel_param_ops param_ops_mode_ops = {
    .set = blinking_pattern_id_validator,
    .get = param_get_int,
};

static int brightness_validator(const char *val, const struct kernel_param *kp);
static const struct kernel_param_ops param_ops_brightness_ops = {
    .set = brightness_validator,
    .get = param_get_int,
};

// Module Parameters
static uint param_color_left = KB_COLOR_DEFAULT;
module_param_named(color_left, param_color_left, uint, S_IRUSR);
MODULE_PARM_DESC(color_left, "Color for the Left Section");

static uint param_color_center = KB_COLOR_DEFAULT;
module_param_named(color_center, param_color_center, uint, S_IRUSR);
MODULE_PARM_DESC(color_center, "Color for the Center Section");

static uint param_color_right = KB_COLOR_DEFAULT;
module_param_named(color_right, param_color_right, uint, S_IRUSR);
MODULE_PARM_DESC(color_right, "Color for the Right Right");

static uint param_color_extra = KB_COLOR_DEFAULT;
module_param_named(color_extra, param_color_extra, uint, S_IRUSR);
MODULE_PARM_DESC(color_extra, "Color for the Extra Section");

static ushort param_mode = DEFAULT_MODE;
module_param_cb(mode, &param_ops_mode_ops, &param_mode, S_IRUSR);
MODULE_PARM_DESC(mode, "Set the mode");

static ushort param_brightness = BRIGHTNESS_DEFAULT;
module_param_cb(brightness, &param_ops_brightness_ops, &param_brightness,
        S_IRUSR);
MODULE_PARM_DESC(brightness, "Set the Keyboard Brightness");

static bool param_state = true;
module_param_named(state, param_state, bool, S_IRUSR);
MODULE_PARM_DESC(state,
         "Set the State of the Keyboard TRUE = ON | FALSE = OFF");

// Keyboard struct
static struct {
    u8 has_extra;
    u8 state;

    struct {
        u32 left;
        u32 center;
        u32 right;
        u32 extra;
    } color;

    u8 brightness;
    u8 mode;
} keyboard = {
    .has_extra = 0, .mode = DEFAULT_MODE,
    .state = 1, .brightness = BRIGHTNESS_DEFAULT,
    .color = {
            .left = KB_COLOR_DEFAULT, .center = KB_COLOR_DEFAULT,
            .right = KB_COLOR_DEFAULT, .extra = KB_COLOR_DEFAULT
             }
};

static struct {
    u8 key;
    u32 value;
    const char *const name;
} blinking_patterns[] = {
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
    return sprintf(buffer, "%d\n", keyboard.state);
}

// Sysfs Interface for the color of the left side (Color as hexvalue)
static ssize_t show_color_left_fs(struct device *child,
                  struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%06x\n", keyboard.color.left);
}

// Sysfs Interface for the color of the center (Color as hexvalue)
static ssize_t show_color_center_fs(struct device *child,
                    struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%06x\n", keyboard.color.center);
}

// Sysfs Interface for the color of the right side (Color as hexvalue)
static ssize_t show_color_right_fs(struct device *child,
                   struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%06x\n", keyboard.color.right);
}

// Sysfs Interface for the color of the extra region (Color as hexvalue)
static ssize_t show_color_extra_fs(struct device *child,
                   struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%06x\n", keyboard.color.extra);
}

// Sysfs Interface for the keyboard brightness (unsigned int)
static ssize_t show_brightness_fs(struct device *child,
                  struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%d\n", keyboard.brightness);
}

// Sysfs Interface for the keyboard mode
static ssize_t show_blinking_patterns_fs(struct device *child, struct device_attribute *attr,
                                         char *buffer)
{
    return sprintf(buffer, "%d\n", keyboard.mode);
}

// Sysfs Interface for if the keyboard has extra region
static ssize_t show_hasextra_fs(struct device *child,
                struct device_attribute *attr, char *buffer)
{
    return sprintf(buffer, "%d\n", keyboard.has_extra);
}

static int tuxedo_evaluate_wmi_method(u32 method_id, u32 arg, u32 * retval)
{
    struct acpi_buffer in = { (acpi_size) sizeof(arg), &arg };
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

    obj = (union acpi_object *)out.pointer;
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

static void set_brightness(u8 brightness)
{
    TUXEDO_INFO("Set brightness on %d", brightness);
    if (!tuxedo_evaluate_wmi_method
        (SET_KB_LED, 0xF4000000 | brightness, NULL)) {
        keyboard.brightness = brightness;
    }
}

static ssize_t set_brightness_fs(struct device *child,
                                 struct device_attribute *attr,
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

static void set_state(u8 state)
{
    u32 cmd = 0xE0000000;
    TUXEDO_INFO("Set keyboard state on: %d\n", state);

    if (state == 0) {
        cmd |= 0x003001;
    } else {
        cmd |= 0x07F001;
    }

    if (!tuxedo_evaluate_wmi_method(SET_KB_LED, cmd, NULL)) {
        keyboard.state = state;
    }
}

static ssize_t set_state_fs(struct device *child, struct device_attribute *attr,
                const char *buffer, size_t size)
{
    unsigned int val;
    int ret = kstrtouint(buffer, 0, &val);

    if (ret) {
        return ret;
    }

    val = clamp_t(u8, val, 0, 1);

    set_state(val);

    return size;
}

static int set_color(u32 region, u32 color)
{
    u32 cset =
        ((color & 0x0000FF) << 16) | ((color & 0xFF0000) >> 8) |
        ((color & 0x00FF00) >> 8);
    u32 cmd = region | cset;

    TUXEDO_DEBUG("Set Color '%08x' for region '%08x'", color, region);

    return tuxedo_evaluate_wmi_method(SET_KB_LED, cmd, NULL);
}

static int set_color_region(const char *buffer, size_t size, u32 region)
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

static ssize_t set_color_left_fs(struct device *child,
                 struct device_attribute *attr,
                 const char *buffer, size_t size)
{
    return set_color_region(buffer, size, REGION_LEFT);
}

static ssize_t set_color_center_fs(struct device *child,
                   struct device_attribute *attr,
                   const char *buffer, size_t size)
{
    return set_color_region(buffer, size, REGION_CENTER);
}

static ssize_t set_color_right_fs(struct device *child,
                  struct device_attribute *attr,
                  const char *buffer, size_t size)
{
    return set_color_region(buffer, size, REGION_RIGHT);
}

static ssize_t set_color_extra_fs(struct device *child,
                  struct device_attribute *attr,
                  const char *buffer, size_t size)
{
    return set_color_region(buffer, size, REGION_EXTRA);
}

static void set_blinking_pattern(u8 mode)
{
    TUXEDO_INFO("set_mode on %s", blinking_patterns[mode].name);

    if (!tuxedo_evaluate_wmi_method(SET_KB_LED, blinking_patterns[mode].value, NULL)) {
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

static ssize_t set_blinking_pattern_fs(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size)
{
    unsigned int val;
    int ret = kstrtouint(buffer, 0, &val);

    if (ret) {
        return ret;
    }

    val = clamp_t(u8, val, 0, ARRAY_SIZE(blinking_patterns) - 1);
    set_blinking_pattern(val);

    return size;
}

static int blinking_pattern_id_validator(const char *val,
                                         const struct kernel_param *kp)
{
    int mode = 0;

    if (kstrtoint(val, 10, &mode) != 0
        || mode < 0
        || mode > (ARRAY_SIZE(blinking_patterns) - 1)) {
        return -EINVAL;
    }

    return param_set_int(val, kp);
}

static int brightness_validator(const char *val, const struct kernel_param *kp)
{
    int brightness = 0;

    if (kstrtoint(val, 10, &brightness) != 0
        || brightness < BRIGHTNESS_MIN
        || brightness > BRIGHTNESS_MAX) {
        return -EINVAL;
    }

    return param_set_int(val, kp);
}

static void tuxedo_wmi_notify(u32 value, void *context)
{
    u32 event;

    tuxedo_evaluate_wmi_method(GET_EVENT, 0, &event);
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
        set_blinking_pattern((keyboard.mode + 1) >
                 (ARRAY_SIZE(blinking_patterns) - 1) ? 0 : (keyboard.mode + 1));
        break;

    case WMI_CODE_TOGGLE_STATE:
        set_state(keyboard.state == 0 ? 1 : 0);
        break;

    default:
        break;
    }
}

static int tuxedo_wmi_probe(struct platform_device *dev)
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

    tuxedo_evaluate_wmi_method(GET_AP, 0, NULL);

    return 0;
}

static int tuxedo_wmi_remove(struct platform_device *dev)
{
    wmi_remove_notify_handler(CLEVO_EVENT_GUID);
    return 0;
}

static int tuxedo_wmi_resume(struct platform_device *dev)
{
    tuxedo_evaluate_wmi_method(GET_AP, 0, NULL);

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

// Sysfs device Attributes
static DEVICE_ATTR(state, 0644, show_state_fs, set_state_fs);
static DEVICE_ATTR(color_left, 0644, show_color_left_fs, set_color_left_fs);
static DEVICE_ATTR(color_center, 0644, show_color_center_fs,
           set_color_center_fs);
static DEVICE_ATTR(color_right, 0644, show_color_right_fs, set_color_right_fs);
static DEVICE_ATTR(color_extra, 0644, show_color_extra_fs, set_color_extra_fs);
static DEVICE_ATTR(brightness, 0644, show_brightness_fs, set_brightness_fs);
static DEVICE_ATTR(mode, 0644, show_blinking_patterns_fs, set_blinking_pattern_fs);
static DEVICE_ATTR(extra, 0444, show_hasextra_fs, NULL);

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

    set_blinking_pattern(param_mode);
    set_brightness(param_brightness);
    set_state(param_state);

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

    if (keyboard.has_extra == 1) {
        device_remove_file(&tuxedo_platform_device->dev,
                   &dev_attr_color_extra);
    }

    platform_device_unregister(tuxedo_platform_device);

    platform_driver_unregister(&tuxedo_platform_driver);

    TUXEDO_DEBUG("exit");
}

module_init(tuxdeo_keyboard_init);
module_exit(tuxdeo_keyboard_exit);
