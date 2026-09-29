// SPDX-License-Identifier: GPL-2.0
/*
 * NAND controller driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * This is a small memory-mapped, ALE/CLE-address-line NAND controller with
 * a per-chip-select "force CE low" config register and a 1-bit hardware
 * ECC engine addressed by writing the physical byte address being accessed
 * into an ECC_A register. The vendor driver (drivers/mtd/nand/cnc_nand.c
 * in the GPL release) implements this on top of the legacy nand_chip
 * cmd_ctrl/ecc hooks, and its 1-bit ECC correction algorithm is the same
 * Hamming code used by mainline's davinci_nand.c 1-bit ECC (the register
 * *offsets* differ from the real TI DaVinci AEMIF, so this is a new driver
 * rather than a "ti,davinci-nand" DT-compatible reuse).
 */

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define CE_CFG_STRIDE		0x0c
#define CE_CFG_FORCE_LOW	BIT(9)	/* 0x200, in CEx_CFG_0 */

#define REG_STATUS		0x30
#define REG_STATUS_READY	BIT(0)

#define REG_ECC_CODE		0x40
#define REG_ECC_A		0x48

#define DEFAULT_MASK_ALE	0x8
#define DEFAULT_MASK_CLE	0x4

struct cnc1800l_nand {
	struct nand_controller	controller;
	struct nand_chip	chip;
	struct device		*dev;
	void __iomem		*regs;		/* control/ECC registers */
	void __iomem		*data;		/* memory-mapped data window */
	phys_addr_t		data_phys;
	u32			mask_ale;
	u32			mask_cle;
	int			cur_chip;
};

static struct cnc1800l_nand *to_cnc1800l_nand(struct nand_chip *chip)
{
	return container_of(chip, struct cnc1800l_nand, chip);
}

static void cnc1800l_nand_hwctl(struct nand_chip *chip, int mode)
{
	struct cnc1800l_nand *nc = to_cnc1800l_nand(chip);
	phys_addr_t addr;

	if (mode == NAND_ECC_READ)
		addr = nc->data_phys + ((u32)chip->legacy.IO_ADDR_R -
					 (u32)(uintptr_t)nc->data);
	else
		addr = nc->data_phys + ((u32)chip->legacy.IO_ADDR_W -
					 (u32)(uintptr_t)nc->data);

	writel_relaxed(addr, nc->regs + REG_ECC_A);
}

static int cnc1800l_nand_calculate(struct nand_chip *chip, const u8 *dat,
				    u8 *ecc_code)
{
	struct cnc1800l_nand *nc = to_cnc1800l_nand(chip);
	u32 ecc24 = readl_relaxed(nc->regs + REG_ECC_CODE);

	ecc_code[0] = ecc24;
	ecc_code[1] = ecc24 >> 8;
	ecc_code[2] = ecc24 >> 16;

	return 0;
}

/* Single-bit Hamming correction, identical algorithm to davinci_nand.c. */
static int cnc1800l_nand_correct(struct nand_chip *chip, u8 *dat,
				  u8 *read_ecc, u8 *calc_ecc)
{
	u32 ecc_nand = read_ecc[0] | (read_ecc[1] << 8) | (read_ecc[2] << 16);
	u32 ecc_calc = calc_ecc[0] | (calc_ecc[1] << 8) | (calc_ecc[2] << 16);
	u32 diff = ecc_calc ^ ecc_nand;
	unsigned int bit, byte;

	if (!diff)
		return 0;

	if ((((diff >> 1) ^ diff) & 0x555555) == 0x555555) {
		bit  = ((diff >> 19) & 1) | ((diff >> 20) & 2) | ((diff >> 21) & 4);
		byte = ((diff >> 9) & 0x100) | (diff & 0x80) |
		       ((diff << 1) & 0x40) | ((diff << 2) & 0x20) |
		       ((diff << 3) & 0x10) | ((diff >> 12) & 0x08) |
		       ((diff >> 11) & 0x04) | ((diff >> 10) & 0x02) |
		       ((diff >> 9) & 0x01);
		dat[byte] ^= BIT(bit);
		return 1;
	}

	if (!(diff & (diff - 1)))
		return 1; /* error in the ECC bytes themselves */

	return -EBADMSG;
}

static int cnc1800l_nand_dev_ready(struct nand_chip *chip)
{
	struct cnc1800l_nand *nc = to_cnc1800l_nand(chip);

	return !!(readl_relaxed(nc->regs + REG_STATUS) & REG_STATUS_READY);
}

static void cnc1800l_nand_cmd_ctrl(struct nand_chip *chip, int cmd,
				    unsigned int ctrl)
{
	struct cnc1800l_nand *nc = to_cnc1800l_nand(chip);
	void __iomem *addr = nc->data;
	u32 cfg;

	if (ctrl & NAND_CTRL_CHANGE) {
		cfg = readl_relaxed(nc->regs + nc->cur_chip * CE_CFG_STRIDE);
		if (ctrl & NAND_NCE)
			cfg |= CE_CFG_FORCE_LOW;
		else
			cfg &= ~CE_CFG_FORCE_LOW;
		writel_relaxed(cfg, nc->regs + nc->cur_chip * CE_CFG_STRIDE);

		if (ctrl & NAND_CLE)
			addr += nc->mask_cle;
		else if (ctrl & NAND_ALE)
			addr += nc->mask_ale;

		chip->legacy.IO_ADDR_W = addr;
	}

	if (cmd != NAND_CMD_NONE)
		iowrite8(cmd, chip->legacy.IO_ADDR_W);
}

static void cnc1800l_nand_select_chip(struct nand_chip *chip, int cs)
{
	struct cnc1800l_nand *nc = to_cnc1800l_nand(chip);
	void __iomem *addr = nc->data;

	if (cs < 0) {
		nc->cur_chip = 0;
		return;
	}

	nc->cur_chip = cs;
	addr += cs * BIT(26); /* 64 MiB per chip select */

	chip->legacy.IO_ADDR_R = addr;
	chip->legacy.IO_ADDR_W = addr;
}

static int cnc1800l_nand_attach_chip(struct nand_chip *chip)
{
	if (chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_HOST)
		return 0;

	chip->ecc.hwctl = cnc1800l_nand_hwctl;
	chip->ecc.calculate = cnc1800l_nand_calculate;
	chip->ecc.correct = cnc1800l_nand_correct;
	chip->ecc.bytes = 3;
	chip->ecc.size = 512;
	chip->ecc.strength = 1;

	return 0;
}

static const struct nand_controller_ops cnc1800l_nand_ops = {
	.attach_chip = cnc1800l_nand_attach_chip,
};

static int cnc1800l_nand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cnc1800l_nand *nc;
	struct resource *res;
	struct mtd_info *mtd;
	int ret;

	nc = devm_kzalloc(dev, sizeof(*nc), GFP_KERNEL);
	if (!nc)
		return -ENOMEM;

	nc->dev = dev;

	nc->regs = devm_platform_ioremap_resource_byname(pdev, "smc");
	if (IS_ERR(nc->regs))
		return PTR_ERR(nc->regs);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "data");
	if (!res)
		return -EINVAL;
	nc->data_phys = res->start;
	nc->data = devm_ioremap_resource(dev, res);
	if (IS_ERR(nc->data))
		return PTR_ERR(nc->data);

	if (of_property_read_u32(dev->of_node, "celestial,mask-ale",
				  &nc->mask_ale))
		nc->mask_ale = DEFAULT_MASK_ALE;
	if (of_property_read_u32(dev->of_node, "celestial,mask-cle",
				  &nc->mask_cle))
		nc->mask_cle = DEFAULT_MASK_CLE;

	nc->controller.ops = &cnc1800l_nand_ops;
	nand_controller_init(&nc->controller);
	nc->chip.controller = &nc->controller;
	nand_set_controller_data(&nc->chip, nc);
	nand_set_flash_node(&nc->chip, dev->of_node);

	mtd = nand_to_mtd(&nc->chip);
	mtd->dev.parent = dev;

	nc->chip.legacy.IO_ADDR_R = nc->data;
	nc->chip.legacy.IO_ADDR_W = nc->data;
	nc->chip.legacy.cmd_ctrl = cnc1800l_nand_cmd_ctrl;
	nc->chip.legacy.dev_ready = cnc1800l_nand_dev_ready;
	nc->chip.legacy.select_chip = cnc1800l_nand_select_chip;
	nc->chip.legacy.chip_delay = 0;
	nc->chip.options = NAND_NO_SUBPAGE_WRITE;
	nc->chip.ecc.engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;

	platform_set_drvdata(pdev, nc);

	ret = nand_scan(&nc->chip, 1);
	if (ret)
		return dev_err_probe(dev, ret, "nand_scan failed\n");

	ret = mtd_device_parse_register(mtd, NULL, NULL, NULL, 0);
	if (ret) {
		nand_cleanup(&nc->chip);
		return dev_err_probe(dev, ret, "failed to register mtd\n");
	}

	return 0;
}

static void cnc1800l_nand_remove(struct platform_device *pdev)
{
	struct cnc1800l_nand *nc = platform_get_drvdata(pdev);

	WARN_ON(mtd_device_unregister(nand_to_mtd(&nc->chip)));
	nand_cleanup(&nc->chip);
}

static const struct of_device_id cnc1800l_nand_match[] = {
	{ .compatible = "celestial,cnc1800l-nand" },
	{ }
};
MODULE_DEVICE_TABLE(of, cnc1800l_nand_match);

static struct platform_driver cnc1800l_nand_driver = {
	.probe	= cnc1800l_nand_probe,
	.remove	= cnc1800l_nand_remove,
	.driver	= {
		.name = "cnc1800l-nand",
		.of_match_table = cnc1800l_nand_match,
	},
};
module_platform_driver(cnc1800l_nand_driver);

MODULE_DESCRIPTION("Celestial CNC1800L NAND controller driver");
MODULE_LICENSE("GPL");
