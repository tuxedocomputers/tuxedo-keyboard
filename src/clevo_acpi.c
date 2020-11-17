#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>

#define DRIVER_NAME			"clevo_acpi"
#define CLEVO_ACPI_RESOURCE_HID		"CLV0001"
#define CLEVO_ACPI_DSM_UUID		"93f224e4-fbdc-4bbf-add6-db71bdc0afad"

struct clevo_acpi_driver_data_t {
	struct acpi_device *device;
};

static u32 clevo_acpi_evaluate(struct acpi_device *device, u8 cmd, u32 arg)
{
	u32 status;
	u32 result;
	acpi_handle handle;
	u64 dsm_rev_dummy = 0x00; // Dummy 0 value since not used
	u64 dsm_func = cmd;
	// Integer package data for argument
	union acpi_object dsm_argv4_package_data[] = {
		{
			.integer.type = ACPI_TYPE_INTEGER,
			.integer.value = arg
		}
	};

	// Package argument
	union acpi_object dsm_argv4 = {
		.package.type = ACPI_TYPE_PACKAGE,
		.package.count = 1,
		.package.elements = dsm_argv4_package_data
	};

	union acpi_object *out_obj;

	guid_t clevo_acpi_dsm_uuid;

	status = guid_parse(CLEVO_ACPI_DSM_UUID, &clevo_acpi_dsm_uuid);
	if (status < 0)
		return -ENOENT;

	handle = acpi_device_handle(device);
	if (handle == NULL)
		return -ENODEV;

	out_obj = acpi_evaluate_dsm(handle, &clevo_acpi_dsm_uuid, dsm_rev_dummy, dsm_func, &dsm_argv4);
	if (!out_obj) {
		pr_err("failed to evaluate _DSM\n");
		result = 0;
	} else {
		if (out_obj->type == ACPI_TYPE_INTEGER) {
			result = (u32) out_obj->integer.value;
		} else {
			pr_err("unknown output from _DSM\n");
			result = 0;
		}
	}

	ACPI_FREE(out_obj);

	return result;
}

static int clevo_acpi_add(struct acpi_device *device)
{
	struct clevo_acpi_driver_data_t *driver_data;

	driver_data = devm_kzalloc(&device->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	driver_data->device = device;

	pr_debug("acpi add\n");

	pr_debug("enable acpi events\n");
	clevo_acpi_evaluate(device, 0x46, 0);

	return 0;
}

static int clevo_acpi_remove(struct acpi_device *device)
{
	pr_debug("acpi remove\n");
	return 0;
}

static void clevo_acpi_notify(struct acpi_device *device, u32 event)
{
	pr_debug("event: %0#10x\n", event);
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct device *dev)
{
	pr_debug("driver suspend\n");
	return 0;
}

static int driver_resume_callb(struct device *dev)
{
	pr_debug("driver resume\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(clevo_driver_pm_ops, driver_suspend_callb, driver_resume_callb);
#endif

static const struct acpi_device_id clevo_acpi_device_ids[] = {
	{CLEVO_ACPI_RESOURCE_HID, 0},
	{"", 0}
};

static struct acpi_driver clevo_acpi_driver = {
	.name = DRIVER_NAME,
	.class = DRIVER_NAME,
	.owner = THIS_MODULE,
	.ids = clevo_acpi_device_ids,
	.flags = ACPI_DRIVER_ALL_NOTIFY_EVENTS,
	.ops = {
		.add = clevo_acpi_add,
		.remove = clevo_acpi_remove,
		.notify = clevo_acpi_notify,
	},
#ifdef CONFIG_PM
	.drv.pm = &clevo_driver_pm_ops
#endif
};

module_acpi_driver(clevo_acpi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for Clevo ACPI interface");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
