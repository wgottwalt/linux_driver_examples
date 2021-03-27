// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * acpi-get-gpio - a simple test driver for acquiring a GPIO from an ACPI node
 * Copyright (C) 2021 Wilken 'Akiko' Gottwalt
 */

#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DRIVER_NAME	"acpi_get_gpio"
#define GPIO_NAME	"GPIO00"

static char *gpio_name = GPIO_NAME;
module_param(gpio_name, charp, 0444);
MODULE_PARM_DESC(gpio_name, "unique GPIO name to acquire (default: '" GPIO_NAME "')");

struct acpi_get_gpio_data {
	struct device *dev;
	struct gpio_desc *desc;
};

static void acpi_get_gpio_disable(void *data)
{
	struct acpi_get_gpio_data *priv = data;

	dev_info(priv->dev, "releasing %s\n", gpio_name);
}

static int acpi_get_gpio_probe(struct platform_device *pdev)
{
	struct acpi_get_gpio_data *priv;
	int err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	priv->desc = devm_gpiod_get(priv->dev, gpio_name, GPIOD_ASIS);
	if (IS_ERR(priv->desc))
		return PTR_ERR(priv->desc);

	dev_info(priv->dev, "acquired %s\n", gpio_name);

	err = devm_add_action_or_reset(priv->dev, acpi_get_gpio_disable, priv);
	if (err) {
		dev_err(priv->dev, "failed to add disable action\n");
		return err;
	}

	platform_set_drvdata(pdev, priv);

	return 0;
}

static const struct acpi_device_id acpi_get_gpio_acpi_table[] = {
	{ "MEX0001", 0 },
	{ },
};

static struct platform_driver acpi_get_gpio_driver = {
	.driver = {
		.name			= DRIVER_NAME,
		.owner			= THIS_MODULE,
		.acpi_match_table	= acpi_get_gpio_acpi_table,
	},
	.probe	= acpi_get_gpio_probe,
};
module_platform_driver(acpi_get_gpio_driver);

MODULE_AUTHOR("Wilken 'Akiko' Gottwalt");
MODULE_DESCRIPTION("test driver for acquiring a GPIO from ACPI node");
MODULE_LICENSE("GPL");
MODULE_ALIAS("acpi-get-gpio");
