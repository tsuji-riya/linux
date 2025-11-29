/****************************************************************************
  * Copyright (C) 2008-2010 Celestial Semiconductor Inc.
  * All rights reserved
  *
  * [RELEASE HISTORY]
  * VERSION  DATE       AUTHOR                  DESCRIPTION
  * 0.1      10-03-10   Jia Ma           			Original
  ****************************************************************************
*/

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>

// #define CLOCK_ID_BASE           (CLOCK_REG_BASE+(0<<2))
// #define CLOCK_PLL_BASE          (CLOCK_REG_BASE+(0x40<<2))
// #define CLOCK_MUX_BASE          (CLOCK_REG_BASE+(0x50<<2))
// #define CLOCK_RESET_BASE        (CLOCK_REG_BASE+(0x80<<2))

// #define CLOCK_ID_LO             (CLOCK_ID_BASE+(0<<2))
// #define CLOCK_ID_HI             (CLOCK_ID_BASE+(1<<2))

// #define CLOCK_PLL_DDR_DIV       (CLOCK_PLL_BASE+(0<<2))
// #define CLOCK_PLL_AVS_DIV       (CLOCK_PLL_BASE+(1<<2))
// #define CLOCK_PLL_TVE_DIV       (CLOCK_PLL_BASE+(2<<2))
// #define CLOCK_AUD_HPD           (CLOCK_PLL_BASE+(3<<2))
// #define CLOCK_AUD_FREQ          (CLOCK_PLL_BASE+(6<<2))
// #define CLOCK_AUD_JITTER        (CLOCK_PLL_BASE+(7<<2))
// #define CLOCK_CLK_GEN_DIV       (CLOCK_PLL_BASE+(8<<2))

// #define CLOCK_PLL_CLK_EN        (CLOCK_MUX_BASE+(0<<2))
// #define CLOCK_TVE_SEL           (CLOCK_MUX_BASE+(1<<2))
// #define CLOCK_TVE_DIV_N         (CLOCK_MUX_BASE+(2<<2))

// #define CLOCK_SOFT_RESET        (CLOCK_RESET_BASE+(0<<2))
// #define CLOCK_CLK_RESET         (CLOCK_RESET_BASE+(1<<2))

// #define CLOCK_PLL_SLEEP_N       (CLOCK_RESET_BASE+(3<<2))
// #define CLOCK_DAC_POWER_DOWN    (CLOCK_RESET_BASE+(4<<2))
// #define CLOCK_XPORT_CLK_SEL     (CLOCK_RESET_BASE+(5<<2))
// #define CLOCK_USB_ULPI_BYPASS   (CLOCK_RESET_BASE+(6<<2))
// #define CLOCK_AUD_POWER_ON      (CLOCK_RESET_BASE+(7<<2))
// #define CLOCK_LOW_POWER_CTL     (CLOCK_PLL_BASE+(0x10<<2))

#define CLOCK_ID_BASE           (0<<2)
#define CLOCK_PLL_BASE          (0x40<<2)
#define CLOCK_MUX_BASE          (0x50<<2)
#define CLOCK_RESET_BASE        (0x80<<2)

#define CLOCK_ID_LO             (CLOCK_ID_BASE+(0<<2))
#define CLOCK_ID_HI             (CLOCK_ID_BASE+(1<<2))

#define CLOCK_PLL_DDR_DIV       (CLOCK_PLL_BASE+(0<<2))
#define CLOCK_PLL_AVS_DIV       (CLOCK_PLL_BASE+(1<<2))
#define CLOCK_PLL_TVE_DIV       (CLOCK_PLL_BASE+(2<<2))
#define CLOCK_AUD_HPD           (CLOCK_PLL_BASE+(3<<2))
#define CLOCK_AUD_FREQ          (CLOCK_PLL_BASE+(6<<2))
#define CLOCK_AUD_JITTER        (CLOCK_PLL_BASE+(7<<2))
#define CLOCK_CLK_GEN_DIV       (CLOCK_PLL_BASE+(8<<2))

#define CLOCK_PLL_CLK_EN        (CLOCK_MUX_BASE+(0<<2))
#define CLOCK_TVE_SEL           (CLOCK_MUX_BASE+(1<<2))
#define CLOCK_TVE_DIV_N         (CLOCK_MUX_BASE+(2<<2))

#define CLOCK_SOFT_RESET        (CLOCK_RESET_BASE+(0<<2))
#define CLOCK_CLK_RESET         (CLOCK_RESET_BASE+(1<<2))

#define CLOCK_PLL_SLEEP_N       (CLOCK_RESET_BASE+(3<<2))
#define CLOCK_DAC_POWER_DOWN    (CLOCK_RESET_BASE+(4<<2))
#define CLOCK_XPORT_CLK_SEL     (CLOCK_RESET_BASE+(5<<2))
#define CLOCK_USB_ULPI_BYPASS   (CLOCK_RESET_BASE+(6<<2))
#define CLOCK_AUD_POWER_ON      (CLOCK_RESET_BASE+(7<<2))
#define CLOCK_LOW_POWER_CTL     (CLOCK_PLL_BASE+(0x10<<2))

static void __iomem *cnc1800l_clockctrl_base;

#define cnc_clk_read(a) readl(cnc1800l_clockctrl_base+((a)&0xfff))
#define cnc_clk_write(a,v) writel(v,cnc1800l_clockctrl_base+((a)&0xfff))


//mode  0:reset
//      1:set
void cnc1800l_clock_usb_reset(int mode){
    unsigned int val;
    if(mode == 0){
        val = cnc_clk_read(CLOCK_SOFT_RESET);
        cnc_clk_write(CLOCK_SOFT_RESET, val&(~(0x1<<4)));
    }else{
        val = cnc_clk_read(CLOCK_SOFT_RESET);
        cnc_clk_write(CLOCK_SOFT_RESET, val|(0x1<<4));
    }
}

void cnc1800l_clock_usb1phy_bypassenable(void)
{
    unsigned int val;
    val = cnc_clk_read(CLOCK_USB_ULPI_BYPASS);
    cnc_clk_write(CLOCK_USB_ULPI_BYPASS, val|0x3);
}

void cnc1800l_clock_usb1phy_bypassdisable(void)
{
    unsigned int reg_val;
    reg_val = cnc_clk_read(CLOCK_USB_ULPI_BYPASS);
    cnc_clk_write(CLOCK_USB_ULPI_BYPASS, reg_val|0x1);
}

static void __init cnc1800l_clock_init(struct device_node *np)
{
	cnc1800l_clockctrl_base = of_io_request_and_map(np, 0, np->name);
	if (IS_ERR(cnc1800l_clockctrl_base)){
		panic("%pOFn: unable to map resource", np);
        goto fail;
    }

    printk("cnc1800l clockctrl init");

	return;
fail:
	iounmap(cnc1800l_clockctrl_base);
}

EXPORT_SYMBOL(cnc1800l_clock_usb_reset);
EXPORT_SYMBOL(cnc1800l_clock_usb1phy_bypassenable);
EXPORT_SYMBOL(cnc1800l_clock_usb1phy_bypassdisable);


CLK_OF_DECLARE(cnc1800l_clock, "cavium,cnc1800l-clockctrl",
		cnc1800l_clock_init);
