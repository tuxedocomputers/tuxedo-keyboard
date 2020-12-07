/*
* clevo_interfaces.h
*
* Copyright (C) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#ifndef TUXEDO_INTERFACES_H
#define TUXEDO_INTERFACES_H

#include <linux/types.h>

// The clevo get commands expect no parameters
#define CLEVO_CMD_GET_FANINFO1		0x63
#define CLEVO_CMD_GET_FANINFO2		0x64
#define CLEVO_CMD_GET_FANINFO3		0x6e

#define CLEVO_CMD_GET_WEBCAM_SW		0x06
#define CLEVO_CMD_GET_FLIGHTMODE_SW	0x07
#define CLEVO_CMD_GET_TOUCHPAD_SW	0x09

// The clevo set commands expect a parameter
#define CLEVO_CMD_SET_FANSPEED_VALUE	0x68
#define CLEVO_CMD_SET_FANSPEED_AUTO	0x69

#define CLEVO_CMD_SET_WEBCAM_SW		0x22
#define CLEVO_CMD_SET_FLIGHTMODE_SW	0x20
#define CLEVO_CMD_SET_TOUCHPAD_SW	0x2a

int clevo_keyboard_init(void);

struct clevo_interface_t {
	char *string_id;
	void (*event_callb)(u32);
	u32 (*method_call)(u8, u32, u32*);
};

u32 clevo_keyboard_add_interface(struct clevo_interface_t *new_interface);
u32 clevo_keyboard_remove_interface(struct clevo_interface_t *interface);

#endif
