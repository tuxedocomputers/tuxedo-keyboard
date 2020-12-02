#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>

#ifndef TUXEDO_KEYBOARD_H
#define TUXEDO_KEYBOARD_H

struct clevo_interface_t {
	char *string_id;
	void (*event_callb)(u32);
	u32 (*method_call)(u8, u32, u32*);
};

u32 clevo_keyboard_add_interface(struct clevo_interface_t *new_interface);


#endif