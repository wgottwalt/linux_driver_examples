// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwdog - a simple PCI driver von Quancom watchdog cards
 * Copyright (C) 2021 Wilken 'Akiko' Gottwalt
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/watchdog.h>

#define DRIVER_NAME	"pwdog"

#define HEARTBEAT	4
#define NOWAYOUT	0

#define PCI_BAR		0
#define PCI_BAR_SIZE	256

#define WDT_TRIGGER	0x00
#define WDT_DISABLE	0x01
#define INTR_RESET	0x08
#define INTR_FLAG	0xF9 /* was an interrupt issued by this device? */

static int heartbeat = HEARTBEAT;
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "watchdog heartbeat in seconds (default="
		 __MODULE_STRING(HEARTBEAT) ")");

static bool nowayout = NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "watchdog cannot be stopped once started (default="
		 __MODULE_STRING(NOWAYOUT) ")");

struct pwdog_data {
	struct pci_dev *pdev;
	void __iomem *base_addr;
	struct watchdog_device wdd;
};

static struct watchdog_info pwdog_wdt_info = {
	.identity = "PWDOG timer",
	.options = WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static int pwdog_wdt_timer_start(struct watchdog_device *wdd)
{
	struct pwdog_data *priv = watchdog_get_drvdata(wdd);

	iowrite8(0, priv->base_addr + WDT_TRIGGER);

	return 0;
}

static int pwdog_wdt_timer_stop(struct watchdog_device *wdd)
{
	struct pwdog_data *priv = watchdog_get_drvdata(wdd);

	iowrite8(0, priv->base_addr + WDT_DISABLE);

	return 0;
}

static int pwdog_wdt_timer_ping(struct watchdog_device *wdd)
{
	struct pwdog_data *priv = watchdog_get_drvdata(wdd);

	iowrite8(0, priv->base_addr + WDT_TRIGGER);

	return 0;
}

static const struct watchdog_ops pwdog_wdt_ops = {
	.owner	= THIS_MODULE,
	.start	= pwdog_wdt_timer_start,
	.stop	= pwdog_wdt_timer_stop,
	.ping	= pwdog_wdt_timer_ping,
};

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct pwdog_data *priv = dev_id;
	u8 intr_flag;

	intr_flag = ioread8(priv->base_addr + INTR_FLAG);
	if (intr_flag == 0)
		return IRQ_NONE;

	iowrite8(ioread8(priv->base_addr + INTR_RESET), priv->base_addr + INTR_RESET);
	iowrite8(0, priv->base_addr + INTR_FLAG);

	return IRQ_HANDLED;
}

static void pwdog_disable(void *data)
{
	struct pwdog_data *priv = data;

	dev_info(&priv->pdev->dev, "release resources\n");
}

static int pwdog_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct pwdog_data *priv;
	struct watchdog_device *wdd;
	int err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	err = pci_enable_device(pdev);
	if (err < 0)
		return err;

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) < 0) {
		if ((pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) < 0) {
			dev_err(&pdev->dev, "no usable DMA configuration\n");
			err = -EFAULT;
			goto fail;
		} else {
			pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		}
	} else {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	}

	priv->base_addr = pci_iomap(pdev, PCI_BAR, PCI_BAR_SIZE);
	if (!priv->base_addr) {
		dev_warn(&pdev->dev, "unable to obtain io-mem address\n");
		goto fail;
	}

	err = devm_request_irq(&pdev->dev, pdev->irq, irq_handler, IRQF_SHARED, DRIVER_NAME, priv);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to acquire IRQ (%d)\n", err);
		goto fail;
	}

	wdd = &priv->wdd;
	wdd->parent = &pdev->dev;
	wdd->info = &pwdog_wdt_info;
	wdd->ops = &pwdog_wdt_ops;
	wdd->timeout = HEARTBEAT;
	wdd->min_timeout = 1;
	wdd->max_timeout = 32400;

	watchdog_init_timeout(wdd, heartbeat, NULL);
	watchdog_set_nowayout(wdd, nowayout);
	watchdog_stop_on_reboot(wdd);
	watchdog_stop_on_unregister(wdd);
	watchdog_set_drvdata(wdd, priv);

	err = devm_watchdog_register_device(&pdev->dev, wdd);
	if (err < 0)
		goto fail;

	err = devm_add_action_or_reset(&pdev->dev, pwdog_disable, priv);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to add disable action\n");
		goto fail;
	}

	dev_info(&pdev->dev, "registered IRQ (%d)\n", pdev->irq);

	priv->pdev = pdev;
	pci_set_drvdata(pdev, priv);

	return 0;

fail:
	pci_disable_device(pdev);

	return err;
}

static const struct pci_device_id pwdog_pci_ids[] = {
	{ PCI_DEVICE(0x8008, 0x0010) },
	{ },
};

static struct pci_driver pwdog_driver = {
	.name		= DRIVER_NAME,
	.probe		= pwdog_probe,
	.id_table	= pwdog_pci_ids,
};
module_pci_driver(pwdog_driver);

MODULE_AUTHOR("Wilken 'Akiko' Gottwalt");
MODULE_DESCRIPTION("simple PCI driver for PCI Quancom watchdog cards");
MODULE_LICENSE("GPL");
