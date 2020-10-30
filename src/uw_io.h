/*
* uw_io.h
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
#ifndef UW_IO_H
#define UW_IO_H

#include <linux/kernel.h>
#include <linux/module.h>

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

/**
 * uw ec read from tccwmi for now stuffed in this wrapper for simplification
 * 
 * @returns The result of the tccwmi function, or -ENOENT
 * 	    if tccwmi symbol is not accessible
 */
static u32 __uw_ec_read_addr(u8 addr_low, u8 addr_high,
			     union uw_ec_read_return *output)
{
	uw_ec_read_func *symbol_uw_ec_read_addr;
	u32 result = 0;
	symbol_uw_ec_read_addr = symbol_get(uw_ec_read_addr);
	if (symbol_uw_ec_read_addr) {
		result = symbol_uw_ec_read_addr(addr_low, addr_high, output);
	} else {
		pr_debug("tuxedo-cc-wmi symbols not found\n");
		result = -ENOENT;
	}

	if (symbol_uw_ec_read_addr)
		symbol_put(uw_ec_read_addr);

	return result;
}

/**
 * uw ec write from tccwmi for now stuffed in this wrapper for simplification
 * 
 * @returns The result of the tccwmi function, or -ENOENT
 * 	    if tccwmi symbol is not accessible
 */
static u32 __uw_ec_write_addr(u8 addr_low, u8 addr_high, u8 data_low,
			      u8 data_high, union uw_ec_write_return *output)
{
	uw_ec_write_func *symbol_uw_ec_write_addr;
	u32 result = 0;
	symbol_uw_ec_write_addr = symbol_get(uw_ec_write_addr);

	if (symbol_uw_ec_write_addr) {
		result = symbol_uw_ec_write_addr(addr_low, addr_high, data_low,
						 data_high, output);
	} else {
		pr_debug("tuxedo-cc-wmi symbols not found\n");
		result = -ENOENT;
	}

	if (symbol_uw_ec_write_addr)
		symbol_put(uw_ec_write_addr);

	return result;
}

#endif
