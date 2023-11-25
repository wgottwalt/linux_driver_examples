// SPDX-License-Identifier: GPL-2.0-only
/*
 * gpio-pps-client - pps client driver for multiple GPIOs
 * Copyright (C) 2021 Wilken Gottwalt
 */

#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pps_kernel.h>

#define DRIVER_NAME	"acpi_gpio_pps_client"
#define MAX_GPIOS	8
#define GPIO_MASK	1

static const char *const gpio_names[MAX_GPIOS] = {
	"GPIO00", "GPIO01", "GPIO02", "GPIO03", "GPIO4", "GPIO5", "GPIO06", "GPIO07",
};

static int gpios_mask = GPIO_MASK;
module_param(gpios_mask, int, 0444);
MODULE_PARM_DESC(gpios_mask, "bitmask of GPIOs to setup as PPS sources (default: "
		 __MODULE_STRING(GPIO_MASK) " max 255)");

struct acpi_gpio_pps_client_device_data {
	struct gpio_desc *gpio;
	struct pps_device *pps;
	struct pps_source_info pps_info;
	int irq;
};

struct acpi_gpio_pps_client_data {
	struct device *dev;
	struct acpi_gpio_pps_client_device_data pps_client[MAX_GPIOS];
	int clients;
};

static irqreturn_t irq_handler(int irq, void *data)
{
	struct acpi_gpio_pps_client_device_data *client = data;
	struct pps_event_time ts;
	int rising_edge;

	pps_get_ts(&ts);
	rising_edge = gpiod_get_value(client->gpio);

	if (rising_edge)
		pps_event(client->pps, &ts, PPS_CAPTUREASSERT, client);
	else
		pps_event(client->pps, &ts, PPS_CAPTURECLEAR, client);

	return IRQ_HANDLED;
}

static void acpi_gpio_pps_client_disable(void *data)
{
	struct acpi_gpio_pps_client_data *priv = data;
	struct acpi_gpio_pps_client_device_data *client;
	int i;

	for (i = 0; i < MAX_GPIOS; ++i) {
		client = &priv->pps_client[i];
		if (client->pps) {
			pps_unregister_source(client->pps);
			dev_info(priv->dev, "released PPS source IRQ (%d)\n", client->irq);
		}
	}
}

static int acpi_gpio_pps_client_probe(struct platform_device *pdev)
{
	struct acpi_gpio_pps_client_data *priv;
	struct acpi_gpio_pps_client_device_data *client;
	int pps_default_params = PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
	int i, err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	for (i = 0; i < MAX_GPIOS; ++i) {
		if (gpios_mask & (1 << i)) {
			client = &priv->pps_client[i];

			client->gpio = devm_gpiod_get(priv->dev, gpio_names[i], GPIOD_IN);
			if (IS_ERR(client->gpio)) {
				dev_err(priv->dev, "failed to request PPS GPIO (%s)\n",
					gpio_names[i]);
				err = IS_ERR(client->gpio);
				goto fail;
			}

			err = gpiod_to_irq(client->gpio);
			if (err < 0) {
				dev_err(priv->dev, "failed to map GPIO (%s) to IRQ (%d)\n",
					gpio_names[i], err);
				goto fail;
			}
			client->irq = err;

			client->pps_info.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT |
						PPS_ECHOASSERT | PPS_CANWAIT | PPS_TSFMT_TSPEC;
			client->pps_info.owner = THIS_MODULE;
			snprintf(client->pps_info.name, PPS_MAX_NAME_LEN - 1, "%s.GPIO0%d",
				 DRIVER_NAME, i);

			client->pps = pps_register_source(&client->pps_info, pps_default_params);
			if (IS_ERR(client->pps)) {
				dev_err(priv->dev, "failed to register IRQ (%d) as PPS source\n",
					client->irq);
				err = PTR_ERR(client->pps);
				goto fail;
			}

			err = devm_request_irq(priv->dev, client->irq, irq_handler,
					       IRQF_TRIGGER_RISING, client->pps_info.name, client);
			if (err) {
				dev_err(priv->dev, "failed to acquire IRQ (%d)\n", client->irq);
				goto fail;
			}
		}
	}

	err = devm_add_action_or_reset(priv->dev, acpi_gpio_pps_client_disable, priv);
	if (err) {
		dev_err(priv->dev, "failed to add disable action\n");
		goto fail;
	}

	for (i = 0; i < MAX_GPIOS; ++i) {
		if (gpios_mask & (1 << i)) {
			client = &priv->pps_client[i];
			dev_info(client->pps->dev, "registered IRQ (%d) as PPS source\n",
				 client->irq);
		}
	}

	platform_set_drvdata(pdev, priv);

	return 0;

fail:
	for (i = 0; i < MAX_GPIOS; ++i) {
		client = &priv->pps_client[i];
		if (client->pps) {
			pps_unregister_source(client->pps);
			dev_warn(priv->dev, "clean up PPS source IRQ (%d)\n", client->irq);
		}
	}

	return err;
}

static const struct acpi_device_id acpi_gpio_pps_client_acpi_table[] = {
	{ "MEX0001", 0, },
	{ },
};

static struct platform_driver acpi_gpio_pps_client_driver = {
	.driver = {
		.name			= DRIVER_NAME,
		.owner			= THIS_MODULE,
		.acpi_match_table	= acpi_gpio_pps_client_acpi_table,
	},
	.probe	= acpi_gpio_pps_client_probe,
};
module_platform_driver(acpi_gpio_pps_client_driver);

MODULE_AUTHOR("Wilken Gottwalt");
MODULE_DESCRIPTION("pps client driver for multiple GPIOs");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("acpi-gpio-pps-client");
