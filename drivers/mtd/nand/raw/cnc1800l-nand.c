/*
 *  drivers/mtd/nand/cnc_nand.c
 *
 *  Copyrigth (C) 2010 Celestial Semiconductor
 *                2011 Cavium
 *  Author: xiaodong fan <xiaodong.fan@caviumnetworks.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   Celestial CNC18xx SOC.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/platform_device.h>
#include <linux/err.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mtd/nand.h>

#define DRIVER_NAME "cnc1800l_nand"

#define CE0_CFG_0_OFFSET 0
#define CE0_CFG_1_OFFSET 0x04
#define CE0_CFG_2_OFFSET 0x08

#define CE1_CFG_0_OFFSET 0x0C
#define CE1_CFG_1_OFFSET 0x10
#define CE1_CFG_2_OFFSET 0x14


#define CE2_CFG_0_OFFSET 0x18
#define CE2_CFG_1_OFFSET 0x1C
#define CE2_CFG_2_OFFSET 0x20


#define CE3_CFG_0_OFFSET 0x24
#define CE3_CFG_1_OFFSET 0x28
#define CE3_CFG_2_OFFSET 0x2c

#define STATUS_OFFSET 0x30
#define MODE_OFFSET 0x38

#define ECC_CODE_OFFSET 0x40
#define ECC_CNT_OFFSET 0x44
#define ECC_A_OFFSET  0x48

#define	MASK_ALE		0x8
#define	MASK_CLE		0x4

#define MASK_CE_FORCE_LOW 0x200

#define PADDR   0x80100000

// original partition structure.
#if 0
static struct mtd_partition cnc_nand_partitions[] = {
	{
		.name		= "cavm_miniloader",
		.offset		= 0,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_uboot1",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_uboot1_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_nvram_factory",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram_factory_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram1b",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_nvram2b",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_splash",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_2M,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_all_img1_info",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_all_img1_info_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_netHD_Image1",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_32M,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_netHD_Image2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_32M,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_free1",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_16M + SZ_2M + SZ_1M + SZ_512K,  /* 19.5M */
		.mask_flags	= 0,
	}, {
		.name		= "cavm_free1_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_256K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_netHD_img2_info",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_256K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_blob",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_4M - SZ_1M,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_uboot2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_1M,
// #ifdef UBOOT_UPGRADE_ALLOWED
// 		.mask_flags	= 0,
// #else
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
// #endif
	}, {
		.name		= "cavm_ffs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_32M,
		.mask_flags	= 0,
	}, {
		.name		= "customer_area",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_4M, //MTDPART_SIZ_FULL,
		.mask_flags	= 0,
	}
};
#endif

static struct mtd_partition cnc_nand_partitions[] = {
	{
		.name		= "cavm_miniloader",
		.offset		= 0,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_uboot1",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_uboot1_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_nvram_factory",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram_factory_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram1b",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_nvram2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_nvram2b",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_splash",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_2M,
		.mask_flags	= 0,
	}, {
		.name		= "cavm_all_img1_info",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "cavm_all_img1_info_pad",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,/*Read only*/
	}, {
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_8M,
		.mask_flags	= 0,
	}, {
		.name		= "rootfs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0,
	}
};

struct cnc_nand_info {
	struct nand_chip	*chip;

	struct device	*dev;
	struct clk		*clk;

    void __iomem    *smcbase;
	void __iomem	*iobase;

	uint32_t		mask_ale;
	uint32_t		mask_cle;

    struct nand_controller controller;
};

static inline unsigned int cnc_nand_readl(struct cnc_nand_info *info,
		int offset)
{
	return __raw_readl(info->smcbase + offset);
}

static inline void cnc_nand_writel(struct cnc_nand_info *info,
		int offset, unsigned long value)
{
	__raw_writel(value, info->smcbase + offset);
}


static void cnc_nand_1bitecc_hwctl(struct nand_chip *chip, int mode)
{
	struct cnc_nand_info *info = chip->priv;
    unsigned long ecc_addr;


    if (mode == NAND_ECC_READ){
        ecc_addr = (unsigned long)((u32)PADDR + ((u32)chip->legacy.IO_ADDR_R - (u32)info->iobase));
        cnc_nand_writel(info, ECC_A_OFFSET, ecc_addr);
        // CNC_DEBUG("HW Read ECC enable--0x%x\n",(u32)ecc_addr);
    }
    else if (mode == NAND_ECC_WRITE) {
        ecc_addr = (unsigned long)((u32)PADDR + ((u32)chip->legacy.IO_ADDR_W - (u32)info->iobase));
        cnc_nand_writel(info, ECC_A_OFFSET, ecc_addr);
        // CNC_DEBUG("HW Write ECC enable--0x%x\n",(u32)ecc_addr);
    }
}

/*
 * Read hardware ECC value and pack into three bytes
 */
static int cnc_nand_1bitecc_calculate(struct nand_chip *chip,
				     const uint8_t *dat, uint8_t *ecc_code)
{
	struct cnc_nand_info *info = chip->priv;
	unsigned int ecc24 = cnc_nand_readl(info, ECC_CODE_OFFSET);

	ecc_code[0] = (uint8_t)(ecc24);
	ecc_code[1] = (uint8_t)(ecc24 >> 8);
	ecc_code[2] = (uint8_t)(ecc24 >> 16);

	return 0;
}

static int cnc_nand_1bitecc_correct(struct nand_chip *chip, uint8_t *dat,
				     uint8_t *read_ecc, uint8_t *calc_ecc)
{
    //	struct nand_chip *chip = mtd->priv;
	uint32_t eccNand = read_ecc[0] | (read_ecc[1] << 8) |
					  (read_ecc[2] << 16);
	uint32_t eccCalc = calc_ecc[0] | (calc_ecc[1] << 8) |
					  (calc_ecc[2] << 16);
	uint32_t diff = eccCalc ^ eccNand;
	unsigned int bit, byte;


    // CNC_DEBUG("eccNand=0x%x, eccCalc=0x%x, diff=0x%x\n", eccNand, eccCalc, diff);

	if (diff) {
		if ((((diff >> 1) ^ diff) & 0x555555) == 0x555555) {
			/* Correctable error */

            /* calculate the bit position of the error */
            bit  = ((diff >> 19) & 1) |
                ((diff >> 20) & 2) |
                ((diff >> 21) & 4);
            /* calculate the byte position of the error */

            byte = ((diff >> 9) & 0x100) |
                (diff  & 0x80)  |
                ((diff << 1) & 0x40)  |
                ((diff << 2) & 0x20)  |
                ((diff << 3) & 0x10)  |
                ((diff >> 12) & 0x08)  |
                ((diff >> 11) & 0x04)  |
                ((diff >> 10) & 0x02)  |
                ((diff >> 9) & 0x01);

            // CNC_DEBUG("byte=%d, bit=%d, dat[byte]=0x%2x\n",byte,bit,dat[byte]);

            dat[byte] ^= (1 << bit);


            // CNC_DEBUG("corrected dat[byte]=0x%x\n",dat[byte]);

            return 1;
        } else if (!(diff & (diff - 1))) {
            /* Single bit ECC error in the ECC itself,
             * nothing to fix */
            return 1;
        } else {
            /* Uncorrectable error */
            return -1;
        }
    }
    return 0;
}


static void cnc_nand_cmd_ctrl(struct nand_chip *chip, int cmd, unsigned int ctrl)
{

	struct cnc_nand_info	*info =  chip->priv;
	struct nand_chip		*nand = chip;
    uint32_t addr = (uint32_t __force)info->iobase;

	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_NCE){
            (* (unsigned int *) (info->smcbase)) |= MASK_CE_FORCE_LOW;
        }
		else {
            (* (unsigned int *) (info->smcbase)) &= ~(MASK_CE_FORCE_LOW);
        }

        //command latch enable, address latch enable
        //these are controlled by address line
        if (ctrl & NAND_CLE){
            addr |= info->mask_cle;
        }
        else if (ctrl & NAND_ALE) {
            addr |= info->mask_ale;
        }
		nand->legacy.IO_ADDR_W = (void __iomem __force *)addr;
	}


	if (cmd != NAND_CMD_NONE)
		iowrite8(cmd, nand->legacy.IO_ADDR_W);
}

static int cnc_nand_device_ready(struct nand_chip *chip)
{
	struct cnc_nand_info *info = chip->priv;

    return cnc_nand_readl(info, STATUS_OFFSET) & BIT(0);
}


static void cnc_nand_select_chip(struct nand_chip *chip, int cs)
{
	struct cnc_nand_info	*info = chip->priv;

    //-1: ctrl change
    if (cs == -1) {
        chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE, 0 | NAND_CTRL_CHANGE);
    }

    chip->legacy.IO_ADDR_W = info->iobase;
    chip->legacy.IO_ADDR_R = chip->legacy.IO_ADDR_W;
}

static void cnc_nand_read_buf(struct nand_chip *chip, u8 *buf, int len)
{
	__raw_readsb(chip->legacy.IO_ADDR_R, buf, len);
}

static void cnc_nand_write_buf(struct nand_chip *chip, const u8 *buf, int len)
{
	__raw_writesb(chip->legacy.IO_ADDR_W, buf, len);
}

static int cnc_nand_attach_chip(struct nand_chip *chip)
{
    chip->ecc.calculate = cnc_nand_1bitecc_calculate;
    chip->ecc.correct = cnc_nand_1bitecc_correct;
    chip->ecc.hwctl = cnc_nand_1bitecc_hwctl;
    chip->ecc.strength = 1;
    chip->ecc.bytes = 3;
    chip->ecc.size = 512;
    chip->ecc.engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;

	return 0;
}

static const struct nand_controller_ops cnc_nand_controller_ops = {
	.attach_chip = cnc_nand_attach_chip,
};

static int cnc1800l_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *this;
	struct cnc_nand_info *pdata;
    struct mtd_info *mtd;
	struct resource *res1;
    struct resource *res2;
	int err = 0;

	this = devm_kzalloc(&pdev->dev, sizeof(struct nand_chip),
			GFP_KERNEL);
	if (!this){
        dev_err(&pdev->dev, "cnc-nand: allocation failed\n");
        return -ENOMEM;
    }

    pdata = devm_kzalloc(&pdev->dev, sizeof(struct cnc_nand_info), GFP_KERNEL);

    if(!pdata)
        goto nopdata;

    this->priv = pdata;

    pdata->chip = this;

	pdata->dev = &pdev->dev;
    mtd = nand_to_mtd(this);

    pdata->mask_ale = MASK_ALE;
    pdata->mask_cle = MASK_CLE;

	mtd->dev.parent = &pdev->dev;
	mtd->name = DRIVER_NAME;
    mtd->priv = &pdata->chip;

	this->legacy.chip_delay = 0;

	nand_set_controller_data(this, pdata);
	nand_set_flash_node(this, pdev->dev.of_node);

    this->options |= NAND_KEEP_TIMINGS | NAND_BBT_USE_FLASH | NAND_BBT_LASTBLOCK | NAND_BBT_WRITE;

    res1 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->smcbase = devm_ioremap_resource(&pdev->dev, res1);
    if(IS_ERR(pdata->smcbase)){
        err = PTR_ERR(pdata->smcbase);
        goto err_res1;
    }

    res2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    pdata->iobase = devm_ioremap_resource(&pdev->dev, res2);
	if (IS_ERR(pdata->iobase)){
        err = PTR_ERR(pdata->iobase);
        goto err_res2;
    }

    this->controller = &pdata->controller;

    this->legacy.IO_ADDR_R = pdata->iobase;
    this->legacy.IO_ADDR_W = pdata->iobase;

	this->legacy.select_chip = cnc_nand_select_chip;
    this->legacy.cmd_ctrl = cnc_nand_cmd_ctrl;
    this->legacy.dev_ready = cnc_nand_device_ready;


    this->legacy.read_buf = cnc_nand_read_buf;
    this->legacy.write_buf = cnc_nand_write_buf;

    this->controller->ops = &cnc_nand_controller_ops;
	err = nand_scan(this,  1);
	if (err){
        dev_err(&pdev->dev, "cnc-nand: failed to scan.\n");
		goto escan;
    }

    err = mtd_device_register(mtd, cnc_nand_partitions, ARRAY_SIZE(cnc_nand_partitions));
	if (err){
        dev_err(&pdev->dev, "cnc-nand: failed to register partitions.\n");
		goto cleanup_nand;
    }

	platform_set_drvdata(pdev, pdata);

	return 0;


escan:
cleanup_nand:
	nand_cleanup(this);
err_res2:
err_res1:
kfree(pdata);
nopdata:
    kfree(this);

	return err;
}

static void cnc1800l_nand__remove(struct platform_device *pdev)
{
	struct cnc_nand_info *info = platform_get_drvdata(pdev);
	struct nand_chip *chip = info->chip;

	mtd_device_unregister(nand_to_mtd(chip));
	nand_cleanup(chip);
    kfree(chip->priv);
}

static const struct of_device_id cnc1800l_nand_dt_ids[] = {
	{ .compatible = "cavium,cnc1800l-nand", },
	{}
};
MODULE_DEVICE_TABLE(of, cnc1800l_nand_dt_ids);


static struct platform_driver cnc1800l_nand_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = cnc1800l_nand_dt_ids,
	},
	.probe = cnc1800l_nand_probe,
	.remove = cnc1800l_nand__remove,
};
module_platform_driver(cnc1800l_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaodong fan <xiaodong.fan@caviumnetworks.com>");
MODULE_DESCRIPTION("NAND flash driver for Cavium celestial CNC18xx serials SOC");
MODULE_ALIAS("platform:cnc1800l-nand");

