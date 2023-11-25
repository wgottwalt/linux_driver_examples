// SPDX-License-Identifier: GPL-2.0-only
/*
 * sc4-list-gpios - test driver for mapping Smartcamera 4 GPIOs from ACPI tables to sysfs
 * Copyright (C) 2021 Wilken Gottwalt
 */

#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DRIVER_NAME	"sc4_list_gpios"
#define DEVICE_NODE	"MEX0001"
#define DEVICE_PATH	"/sys/devices/platform/"

#define GPIO_STR_ANY	"any"
#define GPIO_STR_IN	"input"
#define GPIO_STR_OUT	"output"
#define GPIO_STR_UNDEF	"undefined"

enum gpio_direction {
	GPIO_DIR_ANY,
	GPIO_DIR_IN,
	GPIO_DIR_OUT,
};

struct gpio_attribute {
	struct device_attribute attr;
	int num;
};
#define to_gpio_attr(x) container_of(x, struct gpio_attribute, attr)

struct sc4_list_gpios_acpi_handler_info {
	const union acpi_object *ao;
	enum gpio_direction dir;
};

struct sc4_list_gpios_acpi_gpio_info {
	int num;
	int idx;
	enum gpio_direction dir;
	bool found;
};

struct sc4_list_gpios_data {
	struct device *dev;
	struct gpio_attribute *gpio_attrs;
	struct attribute **gpio_attr_addrs;
	struct attribute_group attr_group;
	int gpios_count;
};

typedef int (*sc4_list_gpios_counter)(struct sc4_list_gpios_data *priv, const void *data);

/*----- helper functions -----*/

static const char *const sc4_list_gpios_dir_to_str(const enum gpio_direction dir)
{
	switch (dir) {
	case GPIO_DIR_ANY:
		return GPIO_STR_ANY;
	case GPIO_DIR_IN:
		return GPIO_STR_IN;
	case GPIO_DIR_OUT:
		return GPIO_STR_OUT;
	default:
		return GPIO_STR_UNDEF;
	};
}

/*----- sysfs -----*/

static ssize_t sc4_list_gpios_show_gpio_number(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	struct gpio_attribute *gpio_attr = to_gpio_attr(attr);

	return sprintf(buf, "%d\n", gpio_attr->num);
}

/*----- ACPI <-> GPIO translation/query support -----*/

static int sc4_list_gpios_add_gpio(struct sc4_list_gpios_data *priv, const char *gpio_name,
				   u32 gpio_name_len, enum gpio_direction dir)
{
	struct gpio_attribute *attr;
	struct gpio_desc *desc;
	char *name;
	char *suffix;
	u32 len;
	int err;

	name = devm_kzalloc(priv->dev, gpio_name_len + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	suffix = strstr(gpio_name, "-gpio");
	if (!suffix) {
		dev_err(priv->dev, "GPIO '%s' must have suffix -gpio or -gpios", gpio_name);
		return -EINVAL;
	}

	len = suffix - gpio_name;
	strncpy(name, gpio_name, len);
	attr = &priv->gpio_attrs[priv->gpios_count];
	attr->attr.attr.name = name;
	attr->attr.attr.mode = 0444;
	attr->attr.show = sc4_list_gpios_show_gpio_number;
	priv->gpio_attr_addrs[priv->gpios_count] = &attr->attr.attr;

	desc = gpiod_get(priv->dev, name, GPIOD_ASIS);
	if (IS_ERR(desc)) {
		err = PTR_ERR(desc);
		if (err != -EPROBE_DEFER)
			dev_err(priv->dev, "can not retrieve GPIO %s\n", name);

		return err;
	}
	attr->num = desc_to_gpio(desc);
	gpiod_put(desc);

	if (dir != GPIO_DIR_ANY && !strncmp(name, "GPIO", 4)) {
		name[2] = (dir == GPIO_DIR_IN) ? 'I' : 'O';
		--len;
		memmove(&name[3], &name[4], len - 3);
		name[len] = 0;
	}

	++priv->gpios_count;

	dev_info(priv->dev, "GPIO: %s%s:00/gpios/%s (%s)\n", DEVICE_PATH, DEVICE_NODE, name,
		 sc4_list_gpios_dir_to_str(dir));

	return 0;
}

static int sc4_list_gpios_add_acpi_gpios(struct sc4_list_gpios_data *priv, const void *data)
{
	const struct sc4_list_gpios_acpi_handler_info *ahinfo = data;

	return sc4_list_gpios_add_gpio(priv, ahinfo->ao->string.pointer, ahinfo->ao->string.length,
				  ahinfo->dir);
}

static acpi_status sc4_list_gpios_gpio_restriction_lookup(struct acpi_resource *ares, void *data)
{
	struct sc4_list_gpios_acpi_gpio_info *aginfo = data;

	if (ares->type != ACPI_RESOURCE_TYPE_GPIO)
		return 1;

	if (aginfo->num++ == aginfo->idx) {
		const struct acpi_resource_gpio *argpio = &ares->data.gpio;

		switch (argpio->io_restriction) {
		case ACPI_IO_RESTRICT_INPUT:
			aginfo->dir = GPIO_DIR_IN;
			break;
		case ACPI_IO_RESTRICT_OUTPUT:
			aginfo->dir = GPIO_DIR_OUT;
			break;
		default:
			aginfo->dir = GPIO_DIR_ANY;
			break;
		};
	}

	return AE_OK;
}

static enum gpio_direction sc4_list_gpios_get_gpio_dir(struct acpi_device *adev, const int idx)
{
	struct sc4_list_gpios_acpi_gpio_info aginfo;
	acpi_status status;

	aginfo.num = 0;
	aginfo.idx = idx;
	aginfo.found = false;

	status = acpi_walk_resources(adev->handle, METHOD_NAME__CRS,
				     sc4_list_gpios_gpio_restriction_lookup, &aginfo);
	if (ACPI_SUCCESS(status) && aginfo.found)
		return aginfo.dir;

	return GPIO_DIR_ANY;
}

static int sc4_list_gpios_count_gpios(struct sc4_list_gpios_data *priv, const void *data)
{
	++priv->gpios_count;

	return 0;
}

static int sc4_list_gpios_handle_gpio(struct sc4_list_gpios_data *priv, struct acpi_device *adev,
				 const union acpi_object *ao, sc4_list_gpios_counter func)
{
	struct fwnode_reference_args args;
	struct sc4_list_gpios_acpi_handler_info ahinfo;
	int err;

	if (ao->type != ACPI_TYPE_STRING || !strstr(ao->string.pointer, "-gpio"))
		return 0;

	memset(&args, 0, sizeof(args));

	err = acpi_node_get_property_reference(acpi_fwnode_handle(adev), ao->string.pointer, 0,
					       &args);
	if (err) {
		dev_err(priv->dev, "fetching ACPI properties failed (%d)\n", err);
		return err;
	}

	ahinfo.ao = ao;
	ahinfo.dir = sc4_list_gpios_get_gpio_dir(adev, args.args[0]);

	return func(priv, &ahinfo);
}

static int sc4_list_gpios_enumerate_acpi_gpios(struct sc4_list_gpios_data *priv,
					       sc4_list_gpios_counter func)
{
	const union acpi_object *ao1;
	struct acpi_device *adev = ACPI_COMPANION(priv->dev);
	int i, j, k;
	int err;

	if (!adev || !adev->data.pointer) {
		dev_info(priv->dev, "no ACPI companion available\n");
		return 0;
	}

	ao1 = adev->data.pointer;
	for (i = 0; i < ao1->package.count; ++i) {
		const union acpi_object *ao2 = &ao1->package.elements[i];

		if (ao2->type != ACPI_TYPE_PACKAGE)
			continue;

		for (j = 0; j < ao2->package.count; ++j) {
			const union acpi_object *ao3 = &ao2->package.elements[j];

			if (ao3->type != ACPI_TYPE_PACKAGE)
				continue;

			for (k = 0; k < ao3->package.count; ++k) {
				err = sc4_list_gpios_handle_gpio(priv, adev,
								 &ao3->package.elements[k], func);
				if (err < 0)
					return err;
			}
		}
	}

	return 0;
}

/*----- platform driver -----*/

static void sc4_list_gpios_disable(void *data)
{
	struct sc4_list_gpios_data *priv = data;

	sysfs_remove_group(&priv->dev->kobj, &priv->attr_group);
}

static int sc4_list_gpios_probe(struct platform_device *pdev)
{
	struct sc4_list_gpios_data *priv;
	int err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	err = sc4_list_gpios_enumerate_acpi_gpios(priv, sc4_list_gpios_count_gpios);
	if (err < 0)
		return err;

	dev_info(priv->dev, "found %d gpios available in %s/ACPI node:\n", priv->gpios_count,
		 DEVICE_NODE);
	priv->gpio_attrs = devm_kzalloc(&pdev->dev, sizeof(*priv->gpio_attrs) * priv->gpios_count,
					GFP_KERNEL);
	if (!priv->gpio_attrs)
		return -ENOMEM;
	priv->gpio_attr_addrs = devm_kzalloc(&pdev->dev, sizeof(*priv->gpio_attr_addrs) *
					     priv->gpios_count + 1, GFP_KERNEL);
	if (!priv->gpio_attr_addrs)
		return -ENOMEM;

	priv->gpios_count = 0;

	err = sc4_list_gpios_enumerate_acpi_gpios(priv, sc4_list_gpios_add_acpi_gpios);
	if (err < 0)
		return err;

	priv->attr_group.name = "gpios";
	priv->attr_group.attrs = priv->gpio_attr_addrs;
	err = sysfs_create_group(&priv->dev->kobj, &priv->attr_group);
	if (err)
		return err;

	err = devm_add_action_or_reset(priv->dev, sc4_list_gpios_disable, priv);
	if (err) {
		dev_err(&pdev->dev, "failed to add %s disable action\n", DRIVER_NAME);
		return err;
	}

	platform_set_drvdata(pdev, priv);

	return 0;
}

static const struct acpi_device_id sc4_list_gpios_acpi_table[] = {
	{ DEVICE_NODE, 0 },
	{ },
};

static struct platform_driver sc4_list_gpios_driver = {
	.driver	= {
		.name			= DRIVER_NAME,
		.owner			= THIS_MODULE,
		.acpi_match_table	= sc4_list_gpios_acpi_table,
	},
	.probe	= sc4_list_gpios_probe,
};
module_platform_driver(sc4_list_gpios_driver);

MODULE_AUTHOR("Wilken Gottwalt");
MODULE_DESCRIPTION("test driver for mapping Smartcamera 4 GPIOs from ACPI tables to sysfs");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("sc4-list-gpios");
