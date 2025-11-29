#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/bitops.h>

/*
*  arch/arm/mach-celestial/include/mach/time.h
*
*  This file contains the hardware definitions of the Celestial Platform.
*
*  Copyright (C) 2010 Celestial Semiconductor
*  Copyright (C) 2011 Cavium

*  Author: Xaodong Fan <xiaodong.fan@caviumneworks.com>
*/

/*
* Copyright (C) 2014 Oleksij Rempel <linux@rempel-privat.de>
*/


#define APB_TIMER_1_LOADCOUNT     0
#define APB_TIMER_1_CUR_VAL       0x04
#define APB_TIMER_1_CTRL          0x08
#define APB_TIMER_1_EOI           0x0c
#define APB_TIMER_1_INT_STATUS    0x10

#define APB_TIMER_2_LOADCOUNT     0x14
#define APB_TIMER_2_CUR_VAL       0x18
#define APB_TIMER_2_CTRL          0x1C
#define APB_TIMER_2_EOI           0x20
#define APB_TIMER_2_INT_STATUS    0x24

#define APB_TIMERS_INT_STATUS     0xA0
#define APB_TIMERS_EOI            0xA4
#define APB_TIMERS_RAW_INT_STATUS 0xA8


#define APB_TIMER_CTL_ENABLE      (1 <<0)
#define APB_TIMER_CTL_PERIODIC    (1<<1)
#define APB_TIMER_CTL_INTMASK     (1<<2)


static void __iomem *cncat_base;

static u64 cnc1800l_apbtimer_read(struct clocksource *cs){
	volatile unsigned int timeval_high,timeval_high2, timeval;
    unsigned long flags;

    raw_local_irq_save(flags);
	timeval_high = readw(cncat_base + APB_TIMER_1_CUR_VAL + 2);
	timeval = readw(cncat_base + APB_TIMER_1_CUR_VAL);
	timeval_high2 = readw(cncat_base + APB_TIMER_1_CUR_VAL + 2);
	if(timeval_high2 != timeval_high){
		timeval = timeval_high2 << 16 | readw(cncat_base + APB_TIMER_1_CUR_VAL);
	}else{
		timeval = timeval_high << 16 | timeval;
	}

	raw_local_irq_restore(flags);

	return ((u32)0xffffffff - timeval);
}

static struct clocksource cnc1800l_clksrc = {
	.name		= "cnc_clocksource",
	.shift		= 27,
	.rating		= 300,
	.read		= cnc1800l_apbtimer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int __init cnc1800l_apbtimer_init(struct device_node *np)
{
	struct clk *clk;
	int ret;
	unsigned long rate,u;

	cncat_base = of_io_request_and_map(np, 0, np->name);
	if (IS_ERR(cncat_base)) {
		pr_err("%pOFn: unable to map resource\n", np);
		return PTR_ERR(cncat_base);
	}

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		pr_err("Failed to get clk!\n");
		return PTR_ERR(clk);
	}

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("Failed to enable clk!\n");
		return ret;
	}

	rate = clk_get_rate(clk);

	writel_relaxed(u & (~APB_TIMER_CTL_ENABLE),cncat_base + APB_TIMER_1_CTRL);
	writew_relaxed(0xffff,cncat_base + APB_TIMER_1_LOADCOUNT);
	writew_relaxed(0xffff,cncat_base + APB_TIMER_1_LOADCOUNT + 2);

	u = readl(cncat_base + APB_TIMER_1_CTRL);
	writel_relaxed((u | APB_TIMER_CTL_INTMASK) & (~APB_TIMER_CTL_PERIODIC),cncat_base + APB_TIMER_1_CTRL);

	clocksource_register_hz(&cnc1800l_clksrc,rate);

	u = readl(cncat_base + APB_TIMER_1_CTRL);
	writel_relaxed(u | APB_TIMER_CTL_ENABLE | APB_TIMER_CTL_INTMASK | APB_TIMER_CTL_PERIODIC, cncat_base + APB_TIMER_1_CTRL);
	return 0;
}
TIMER_OF_DECLARE(cnc1800l_apbtimer, "snps,dw-apb-timer-cnc1800l", cnc1800l_apbtimer_init);
