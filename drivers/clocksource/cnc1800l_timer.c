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


#define TIMER_PRE_LOAD_0       0x0
#define TIMER_PRE_LOAD_1       0x4
#define TIMER_THRESHOLD_0      0x8
#define TIMER_THRESHOLD_1      0xc
#define TIMER_MODE             0x10
#define TIMER_INTR_EN          0x14
#define TIMER_INTR_CLR         0x18
#define TIMER_ENABLE           0x1c
#define TIMER_CURRENT_VAL_0    0x20
#define TIMER_CURRENT_VAL_1    0x24
#define TIMER_INTR_STATUS      0x28
#define TIMER_INTR_RAW_STATUS  0x2c

static void __iomem *cnct_base;
static unsigned long cnct_reload;

static int cnc1800l_timer_set_next_event(unsigned long delta,
					 struct clock_event_device *evt)
{
    unsigned long flags;
    if(delta == 0){
        return -ETIME;
    }
    raw_local_irq_save(flags);

    writel_relaxed(0,cnct_base + TIMER_ENABLE);
    writel_relaxed(0x7,cnct_base +TIMER_INTR_CLR);
    writel_relaxed(0x0,cnct_base +TIMER_INTR_CLR);

    writel_relaxed(delta,cnct_base + TIMER_THRESHOLD_0);
    writel_relaxed(0,cnct_base + TIMER_PRE_LOAD_0);

    writel_relaxed(0x1,cnct_base + TIMER_MODE);
    writel_relaxed(0x1,cnct_base + TIMER_INTR_EN);
    writel_relaxed(0x1,cnct_base + TIMER_ENABLE);

    raw_local_irq_restore(flags);

	return 0;
}

static int cnc1800l_timer_shutdown(struct clock_event_device *evt)
{
	writel_relaxed(0x0, cnct_base + TIMER_ENABLE);
	return 0;
}

static int cnc1800l_timer_set_periodic(struct clock_event_device *evt)
{
    unsigned long flags;
    raw_local_irq_save(flags);

    writel_relaxed(0,cnct_base + TIMER_ENABLE);
    writel_relaxed(cnct_reload,cnct_base + TIMER_THRESHOLD_0);
    writel_relaxed(0,cnct_base + TIMER_PRE_LOAD_0);
    writel_relaxed(0,cnct_base + TIMER_MODE);
    writel_relaxed(0x1,cnct_base + TIMER_INTR_EN);
    writel_relaxed(0x1,cnct_base + TIMER_ENABLE);

    raw_local_irq_restore(flags);
	return 0;
}

static int cnc1800l_timer_set_oneshot(struct clock_event_device *evt)
{
    unsigned long flags;
    raw_local_irq_save(flags);

    writel_relaxed(0x7, cnct_base + TIMER_INTR_CLR);
    writel_relaxed(0x0, cnct_base + TIMER_INTR_CLR);
    writel_relaxed(0x0,cnct_base + TIMER_ENABLE);

    raw_local_irq_restore(flags);
	return 0;
}

static struct clock_event_device event_dev = {
	.name			= "cnc1800l-timer",
	.rating			= 400,
	.features		= CLOCK_EVT_FEAT_PERIODIC |
				  CLOCK_EVT_FEAT_ONESHOT,
    .shift          = 32,
	.set_next_event		= cnc1800l_timer_set_next_event,
	.set_state_shutdown	= cnc1800l_timer_shutdown,
	.set_state_periodic	= cnc1800l_timer_set_periodic,
	.set_state_oneshot	= cnc1800l_timer_set_oneshot,
	.tick_resume		= cnc1800l_timer_shutdown,
};

static irqreturn_t cnc1800l_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	writel_relaxed(0x1,cnct_base + TIMER_INTR_CLR);
    writel_relaxed(0x0,cnct_base + TIMER_INTR_CLR);

	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static int __init cnc1800l_timer_init(struct device_node *np)
{
	int irq;
	struct clk *clk;
	int ret;
	unsigned long rate;

	cnct_base = of_io_request_and_map(np, 0, np->name);
	if (IS_ERR(cnct_base)) {
		pr_err("%pOFn: unable to map resource\n", np);
		return PTR_ERR(cnct_base);
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

	irq = irq_of_parse_and_map(np, 0);
	ret = request_irq(irq, cnc1800l_timer_interrupt, IRQF_TIMER,
			"cnc1800l-timer", &event_dev);
	if (ret) {
		pr_err("Failed to setup irq!\n");
		return ret;
	}

    writel_relaxed(0x0,cnct_base + TIMER_INTR_EN);
    writel_relaxed(0x0,cnct_base + TIMER_ENABLE);
    writel_relaxed(0x1,cnct_base + TIMER_INTR_CLR);
    writel_relaxed(0x0,cnct_base + TIMER_INTR_CLR);

    rate = clk_get_rate(clk);

	cnct_reload = DIV_ROUND_CLOSEST(rate,HZ);

	event_dev.cpumask = cpumask_of(0);
	clockevents_config_and_register(&event_dev, rate, 0xf, 0xfffffffc);

	return 0;
}
TIMER_OF_DECLARE(cnc1800l_timer, "cavium,cnc1800l-timer",
		cnc1800l_timer_init);
