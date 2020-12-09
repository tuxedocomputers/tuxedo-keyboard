/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-cc-wmi.
 *
 * tuxedo-cc-wmi is free software: you can redistribute it and/or modify
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
#include <linux/acpi.h>
#include <linux/wmi.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#define UNIWILL_WMI_MGMT_GUID_BA            "ABBC0F6D-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BB            "ABBC0F6E-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_MGMT_GUID_BC            "ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_WMI_EVENT_GUID_0            "ABBC0F70-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_1            "ABBC0F71-8EA1-11D1-00A0-C90629100000"
#define UNIWILL_WMI_EVENT_GUID_2            "ABBC0F72-8EA1-11D1-00A0-C90629100000"

#define UNIWILL_EC_REG_LDAT     0x8a
#define UNIWILL_EC_REG_HDAT     0x8b
#define UNIWILL_EC_REG_FLAGS    0x8c
#define UNIWILL_EC_REG_CMDL     0x8d
#define UNIWILL_EC_REG_CMDH     0x8e

#define UNIWILL_EC_BIT_RFLG     0
#define UNIWILL_EC_BIT_WFLG     1
#define UNIWILL_EC_BIT_BFLG     2
#define UNIWILL_EC_BIT_CFLG     3
#define UNIWILL_EC_BIT_DRDY     7

#define UW_EC_WAIT_CYCLES       0x50

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

static bool uniwill_ec_direct = true;

DEFINE_MUTEX(uniwill_ec_lock);

static u32 uw_wmi_ec_evaluate(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, u8 read_flag, u32 *return_buffer)
{
    acpi_status status;
    union acpi_object *out_acpi;
    u32 e_result = 0;

    // Kernel buffer for input argument
    u32 *wmi_arg = (u32 *) kmalloc(sizeof(u32)*10, GFP_KERNEL);
    // Byte reference to the input buffer
    u8 *wmi_arg_bytes = (u8 *) wmi_arg;

    u8 wmi_instance = 0x00;
    u32 wmi_method_id = 0x04;
    struct acpi_buffer wmi_in = { (acpi_size) sizeof(wmi_arg), wmi_arg};
    struct acpi_buffer wmi_out = { ACPI_ALLOCATE_BUFFER, NULL };

    mutex_lock(&uniwill_ec_lock);

    // Zero input buffer
    memset(wmi_arg, 0x00, 10 * sizeof(u32));

    // Configure the input buffer
    wmi_arg_bytes[0] = addr_low;
    wmi_arg_bytes[1] = addr_high;
    wmi_arg_bytes[2] = data_low;
    wmi_arg_bytes[3] = data_high;

    if (read_flag != 0) {
        wmi_arg_bytes[5] = 0x01;
    }
    
    status = wmi_evaluate_method(UNIWILL_WMI_MGMT_GUID_BC, wmi_instance, wmi_method_id, &wmi_in, &wmi_out);
    out_acpi = (union acpi_object *) wmi_out.pointer;

    if (out_acpi && out_acpi->type == ACPI_TYPE_BUFFER) {
        memcpy(return_buffer, out_acpi->buffer.pointer, out_acpi->buffer.length);
    } /* else if (out_acpi && out_acpi->type == ACPI_TYPE_INTEGER) {
        e_result = (u32) out_acpi->integer.value;
    }*/
    if (ACPI_FAILURE(status)) {
        pr_err("uniwill_wmi.h: Error evaluating method\n");
        e_result = -EIO;
    }

    kfree(out_acpi);
    kfree(wmi_arg);

    mutex_unlock(&uniwill_ec_lock);

    return e_result;
}

/**
 * EC address read through WMI
 */
static u32 uw_ec_read_addr_wmi(u8 addr_low, u8 addr_high, union uw_ec_read_return *output)
{
    u32 uw_data[10];
    u32 ret = uw_wmi_ec_evaluate(addr_low, addr_high, 0x00, 0x00, 1, uw_data);
    output->dword = uw_data[0];
    pr_debug("addr: 0x%02x%02x value: %0#4x (high: %0#4x) result: %d\n", addr_high, addr_low, output->bytes.data_low, output->bytes.data_high, ret);
    return ret;
}

/**
 * EC address write through WMI
 */
static u32 uw_ec_write_addr_wmi(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, union uw_ec_write_return *output)
{
    u32 uw_data[10];
    u32 ret = uw_wmi_ec_evaluate(addr_low, addr_high, data_low, data_high, 0, uw_data);
    output->dword = uw_data[0];
    return ret;
}

/**
 * Direct EC address read
 */
static u32 uw_ec_read_addr_direct(u8 addr_low, u8 addr_high, union uw_ec_read_return *output)
{
    u32 result;
    u8 tmp, count, flags;

    mutex_lock(&uniwill_ec_lock);

    ec_write(UNIWILL_EC_REG_LDAT, addr_low);
    ec_write(UNIWILL_EC_REG_HDAT, addr_high);

    flags = (0 << UNIWILL_EC_BIT_DRDY) | (1 << UNIWILL_EC_BIT_RFLG);
    ec_write(UNIWILL_EC_REG_FLAGS, flags);

    // Wait for ready flag
    count = UW_EC_WAIT_CYCLES;
    ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
    while (((tmp & (1 << UNIWILL_EC_BIT_DRDY)) == 0) && count != 0) {
        msleep(1);
        ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
        count -= 1;
    }

    if (count != 0) {
        output->dword = 0;
        ec_read(UNIWILL_EC_REG_CMDL, &tmp);
        output->bytes.data_low = tmp;
        ec_read(UNIWILL_EC_REG_CMDH, &tmp);
        output->bytes.data_high = tmp;
        result = 0;
    } else {
        output->dword = 0xfefefefe;
        result = -EIO;
    }

    ec_write(UNIWILL_EC_REG_FLAGS, 0x00);

    mutex_unlock(&uniwill_ec_lock);

    pr_debug("addr: 0x%02x%02x value: %0#4x result: %d\n", addr_high, addr_low, output->bytes.data_low, result);

    return result;
}

static u32 uw_ec_write_addr_direct(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, union uw_ec_write_return *output)
{
    u32 result = 0;
    u8 tmp, count, flags;

    mutex_lock(&uniwill_ec_lock);

    ec_write(UNIWILL_EC_REG_LDAT, addr_low);
    ec_write(UNIWILL_EC_REG_HDAT, addr_high);
    ec_write(UNIWILL_EC_REG_CMDL, data_low);
    ec_write(UNIWILL_EC_REG_CMDH, data_high);

    flags = (0 << UNIWILL_EC_BIT_DRDY) | (1 << UNIWILL_EC_BIT_WFLG);
    ec_write(UNIWILL_EC_REG_FLAGS, flags);

    // Wait for ready flag
    count = UW_EC_WAIT_CYCLES;
    ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
    while (((tmp & (1 << UNIWILL_EC_BIT_DRDY)) == 0) && count != 0) {
        msleep(1);
        ec_read(UNIWILL_EC_REG_FLAGS, &tmp);
        count -= 1;
    }

    // Replicate wmi output depending on success
    if (count != 0) {
        output->bytes.addr_low = addr_low;
        output->bytes.addr_high = addr_high;
        output->bytes.data_low = data_low;
        output->bytes.data_high = data_high;
        result = 0;
    } else {
        output->dword = 0xfefefefe;
        result = -EIO;
    }

    ec_write(UNIWILL_EC_REG_FLAGS, 0x00);

    mutex_unlock(&uniwill_ec_lock);

    return result;
}

u32 uw_ec_read_addr(u8 addr_low, u8 addr_high, union uw_ec_read_return *output)
{
    if (uniwill_ec_direct) {
        return uw_ec_read_addr_direct(addr_low, addr_high, output);
    } else {
        return uw_ec_read_addr_wmi(addr_low, addr_high, output);
    }
}
EXPORT_SYMBOL(uw_ec_read_addr);

u32 uw_ec_write_addr(u8 addr_low, u8 addr_high, u8 data_low, u8 data_high, union uw_ec_write_return *output)
{
    if (uniwill_ec_direct) {
        return uw_ec_write_addr_direct(addr_low, addr_high, data_low, data_high, output);
    } else {
        return uw_ec_write_addr_wmi(addr_low, addr_high, data_low, data_high, output);
    }
}
EXPORT_SYMBOL(uw_ec_write_addr);

static u32 uniwill_identify(void)
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

    if (!status)
    {
        pr_debug("probe: At least one Uniwill GUID missing\n");
        return -ENODEV;
    }

    return 0;
}

static void uniwill_init(void)
{
    union uw_ec_write_return reg_write_return;

    // Enable manual mode
    uw_ec_write_addr(0x41, 0x07, 0x01, 0x00, &reg_write_return);

    // Zero second fan temp for detection
    uw_ec_write_addr(0x4f, 0x04, 0x00, 0x00, &reg_write_return);
}

static void uniwill_exit(void)
{
    union uw_ec_write_return reg_write_return;

    // Disable manual mode
    uw_ec_write_addr(0x41, 0x07, 0x00, 0x00, &reg_write_return);
}
