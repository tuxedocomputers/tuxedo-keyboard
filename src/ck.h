#include <linux/module.h>
#include <linux/kernel.h>

#ifndef TUXEDO_KEYBOARD_H
#define TUXEDO_KEYBOARD_H

int clevo_keyboard_init(void);

struct clevo_interface_t {
	char *string_id;
	void (*event_callb)(u32);
	u32 (*method_call)(u8, u32, u32*);
};

u32 clevo_keyboard_add_interface(struct clevo_interface_t *new_interface);
u32 clevo_keyboard_remove_interface(struct clevo_interface_t *new_interface);

#endif
