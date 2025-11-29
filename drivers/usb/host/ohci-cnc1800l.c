/*
* OHCI HCD (Host Controller Driver) for USB.
*
* (C) Copyright 2010 celestialsemi Company
*
* Bus Glue for CNC1800H
*
* Written by  Ran Hao <ran.hao@celestialsemi.cn>
*
*
* This file is licenced under the GPL.
*/

/*
* OHCI HCD (Host Controller Driver) for USB.
*
*  Copyright (C) 2004 SAN People (Pty) Ltd.
*  Copyright (C) 2005 Thibaut VARENE <varenet@parisc-linux.org>
*
* AT91 Bus Glue
*
* Based on fragments of 2.4 driver by Rick Bronson.
* Based on ohci-omap.c
*
* This file is licenced under the GPL.
*/

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/of_irq.h>

#include "ohci.h"

#define DRIVER_DESC "OHCI CNC1800L driver"

static const char hcd_name[] = "ohci-cnc1800l";

static struct hc_driver __read_mostly ohci_cnc1800l_hc_driver;

#define hcd_to_ohci_cnc1800l_priv(h) \
	((struct ohci_cnc1800l_priv *)hcd_to_ohci(h)->priv)


struct ohci_cnc1800l_priv {
	struct clk *iclk;
	bool clocked;
	bool wakeup;		/* Saved wake-up state for resume */
	struct regmap *sfr_regmap;
};

static void cnc1800l_start_hc(struct device *dev)
{
	/*
	 * Set register pin to enable and 48MHz
	 */

    uint8_t* clkreg = ioremap(0xB2100218,1);

	// (*((volatile unsigned long *)(IO_ADDRESS(0xB2100218)))) =0x3;
    if(clkreg != NULL)
        *clkreg = 0x3;
    else
        dev_dbg(dev, "ohci: failed to enable 48m clock\n");

    iounmap(clkreg);
	udelay(100);
}

static int ohci_hcd_cnc1800l_drv_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    int	ret, irq;
    struct usb_hcd *hcd;
    struct device *dev = &pdev->dev;
    struct ohci_cnc1800l_priv *ohci_cnc;
    struct resource *res;


	/* Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

    irq = irq_of_parse_and_map(np, 0);
	if (irq < 0) {
		dev_dbg(dev, "hcd probe: missing irq resource\n");
		return irq;
	}

    hcd = usb_create_hcd(&ohci_cnc1800l_hc_driver, dev, dev_name(dev));
	if (!hcd)
		return -ENOMEM;
	ohci_cnc = hcd_to_ohci_cnc1800l_priv(hcd);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    hcd->regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(hcd->regs)) {
        ret = PTR_ERR(hcd->regs);
        goto err;
    }

    hcd->rsrc_start = res->start;
    hcd->rsrc_len = resource_size(res);

    cnc1800l_start_hc(dev);

    ret = usb_add_hcd(hcd,irq, IRQF_SHARED);
    if(ret){
        dev_err(&pdev->dev, "Failed to add USB HCD\n");
        goto err;
    }

    device_wakeup_enable(hcd->self.controller);
    return 0;
err:
    usb_put_hcd(hcd);
	return ret;
}

static void ohci_hcd_cnc1800l_drv_remove(struct platform_device *pdev)
{
    struct usb_hcd *hcd = platform_get_drvdata(pdev);
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
}

static const struct of_device_id ohci_hcd_cnc1800l_dt_ids[] = {
	{ .compatible = "cavium,cnc1800l-ohci" },
	{ }
};

MODULE_DEVICE_TABLE(of, ohci_hcd_cnc1800l_dt_ids);

static struct platform_driver ohci_hcd_cnc1800l_driver = {
	.probe		= ohci_hcd_cnc1800l_drv_probe,
	.remove		= ohci_hcd_cnc1800l_drv_remove,
	.driver		= {
		.name	= "cnc1800l-ohci",
		.of_match_table	= ohci_hcd_cnc1800l_dt_ids,
	},
};

static int __init ohci_cnc1800l_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);
	ohci_init_driver(&ohci_cnc1800l_hc_driver, NULL);

	return platform_driver_register(&ohci_hcd_cnc1800l_driver);
}
module_init(ohci_cnc1800l_init);

static void __exit ohci_cnc1800l_cleanup(void)
{
	platform_driver_unregister(&ohci_hcd_cnc1800l_driver);
}
module_exit(ohci_cnc1800l_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:cnc1800l-ohci");
