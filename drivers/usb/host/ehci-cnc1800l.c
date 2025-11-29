/*
* linux/driver/usb/host/ehci-csm18xx.c
*
* Copyright (c) 2010  celestialsemi  corporation.
*
* Ran Hao <ran.hao@celestialsemi.cn>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation
*
*/
/*
* ehci-omap.c - driver for USBHOST on OMAP3/4 processors
*
* Bus Glue for the EHCI controllers in OMAP3/4
* Tested on several OMAP3 boards, and OMAP4 Pandaboard
*
* Copyright (C) 2007-2013 Texas Instruments, Inc.
*	Author: Vikram Pandita <vikram.pandita@ti.com>
*	Author: Anand Gadiyar <gadiyar@ti.com>
*	Author: Keshava Munegowda <keshava_mgowda@ti.com>
*	Author: Roger Quadros <rogerq@ti.com>
*
* Copyright (C) 2009 Nokia Corporation
*	Contact: Felipe Balbi <felipe.balbi@nokia.com>
*
* Based on "ehci-fsl.c" and "ehci-au1xxx.c" ehci glue layers
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/ehci_def.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>

#include "ehci.h"

#define DRIVER_DESC "EHCI CNC1800L driver"

extern void cnc1800l_clock_usb_reset(int mode);
extern void cnc1800l_clock_usb1phy_bypassenable(void);
extern void cnc1800l_clock_usb1phy_bypassdisable(void);

static const char hcd_name[] = "ehci-cnc1800l";

static struct hc_driver __read_mostly ehci_cnc1800l_hc_driver;

static int usb_hcd_cnc18xx_make_hc(struct usb_hcd *hcd)
{
	u32 *reg, val;

	reg = (u32 *)((char *)(hcd->regs) + 0x10);
	val = readl(reg);

	if ((val & 3) != 3 && (val & 3) != 0) {
		printk("usb_hcd_cnc18xx_make_hc(): unsane hardware state\n");
		return -EIO;
	}

	val |= 3;
	*reg = val;

	return 0;
}

static void cnc18xx_start_ehc(struct platform_device *dev)
{
	/* Reset USB core */
	cnc1800l_clock_usb_reset(0);
	udelay(100);
	cnc1800l_clock_usb_reset(1);
	cnc1800l_clock_usb1phy_bypassenable();
}

static void cnc18xx_stop_ehc(struct platform_device *dev)
{
	cnc1800l_clock_usb1phy_bypassdisable();
}


static int ehci_hcd_cnc1800l_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource	*res;
	struct usb_hcd	*hcd;
	int ret;
	int irq;
    struct ehci_hcd *ehci;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	ret = -ENODEV;
	hcd = usb_create_hcd(&ehci_cnc1800l_hc_driver, dev,
			dev_name(dev));
	if (!hcd) {
		dev_err(dev, "Failed to create HCD\n");
		return -ENOMEM;
	}

	res =  platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(hcd->regs))
		return PTR_ERR(hcd->regs);

    hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	platform_set_drvdata(pdev, hcd);

    cnc18xx_start_ehc(pdev);

    ret = usb_hcd_cnc18xx_make_hc(hcd);
    if(ret)
        return ret;

    ehci = hcd_to_ehci(hcd);
    ehci->caps = hcd->regs;
    ehci->regs = hcd->regs + HC_LENGTH(ehci, readl(&ehci->caps->hc_capbase));

    ehci->hcs_params = readl(&ehci->caps->hcs_params);

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(dev, "failed to add hcd with err %d\n", ret);
		goto err;
	}
	device_wakeup_enable(hcd->self.controller);

	return 0;

err:
    cnc18xx_stop_ehc(pdev);
	usb_put_hcd(hcd);
	return ret;
}

static void ehci_hcd_cnc1800l_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	usb_remove_hcd(hcd);
    cnc18xx_stop_ehc(pdev);

	usb_put_hcd(hcd);
}

void
usb_hcd_cnc18xx_shutdown(struct platform_device* dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}


static const struct of_device_id cnc1800l_ehci_dt_ids[] = {
	{ .compatible = "cavium,cnc1800l-ehci" },
	{ }
};

static struct platform_driver ehci_hcd_cnc1800l_driver = {
	.probe			= ehci_hcd_cnc1800l_probe,
	.remove			= ehci_hcd_cnc1800l_remove,
	.shutdown		= usb_hcd_cnc18xx_shutdown,
	.driver = {
		.name		= hcd_name,
		.of_match_table = cnc1800l_ehci_dt_ids,
	}
};

MODULE_DEVICE_TABLE(of, cnc1800l_ehci_dt_ids);

static int __init ehci_cnc1800l_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);

	ehci_init_driver(&ehci_cnc1800l_hc_driver, NULL);
	return platform_driver_register(&ehci_hcd_cnc1800l_driver);
}
module_init(ehci_cnc1800l_init);

static void __exit ehci_cnc1800l_cleanup(void)
{
	platform_driver_unregister(&ehci_hcd_cnc1800l_driver);
}
module_exit(ehci_cnc1800l_cleanup);


MODULE_ALIAS("platform:cnc1800l-ehci");

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("post: clk-cnc1800l");