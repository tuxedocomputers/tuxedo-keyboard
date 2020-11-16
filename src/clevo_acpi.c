#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>

#define DRIVER_NAME			"clevo_acpi"
#define CLEVO_ACPI_RESOURCE_HID		"CLV0001"

struct clevo_acpi_driver_data_t {
	struct acpi_device *device;
};

static int clevo_acpi_add(struct acpi_device *device)
{
	struct clevo_acpi_driver_data_t *driver_data;

	driver_data = devm_kzalloc(&device->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	driver_data->device = device;

	pr_debug("acpi add");

	return 0;
}

static int clevo_acpi_remove(struct acpi_device *device)
{
	pr_debug("acpi remove");
	return 0;
}

static void clevo_acpi_notify(struct acpi_device *device, u32 event)
{
	pr_debug("event: %0#10x", event);
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
