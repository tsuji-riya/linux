// SPDX-License-Identifier: GPL-2.0
/*
 * Timer driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * The SoC has two independent timer IP blocks used together as a single
 * time base, exactly as the vendor 2.6.32 board code combined them:
 *
 *  - the "AHB timer" is a 32-bit down-counter with a one-shot/periodic
 *    mode bit and its own interrupt; used here as the clockevent.
 *  - the "APB timer" is a free-running 32-bit down-counter built from two
 *    16-bit halves (a classic dual DesignWare-APB-timer instance, used
 *    here as a single continuous counter); used here as the clocksource
 *    and sched_clock source. There is no register that reads both halves
 *    atomically, so a high-low-high read-and-compare loop is used to
 *    detect a rollover between the two 16-bit reads.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>

/* AHB timer (clockevent) registers */
#define TIMER_PRE_LOAD_0	0x00
#define TIMER_THRESHOLD_0	0x08
#define TIMER_MODE		0x10
#define TIMER_MODE_ONESHOT	BIT(0)
#define TIMER_INTR_EN		0x14
#define TIMER_INTR_CLR		0x18
#define TIMER_ENABLE		0x1c

/* APB timer (clocksource) registers - instance 1 of 2 */
#define APB_TIMER_LOADCOUNT	0x00
#define APB_TIMER_CUR_VAL	0x04
#define APB_TIMER_CTRL		0x08
#define APB_TIMER_CTL_ENABLE	BIT(0)
#define APB_TIMER_CTL_PERIODIC	BIT(1)
#define APB_TIMER_CTL_INTMASK	BIT(2)

static void __iomem *ahb_base;
static void __iomem *apb_base;

/*
 * The APB timer counts down from 0xffffffff; invert it so callers see an
 * up-counting value, matching the vendor driver's cs_clksrc_read().
 */
static u32 cnc1800l_apb_read(void)
{
	u16 hi, hi2, lo;

	hi = readw_relaxed(apb_base + APB_TIMER_CUR_VAL + 2);
	lo = readw_relaxed(apb_base + APB_TIMER_CUR_VAL);
	hi2 = readw_relaxed(apb_base + APB_TIMER_CUR_VAL + 2);
	if (hi2 != hi)
		lo = readw_relaxed(apb_base + APB_TIMER_CUR_VAL);

	return 0xffffffffU - (((u32)hi2 << 16) | lo);
}

static u64 cnc1800l_clocksource_read(struct clocksource *cs)
{
	return cnc1800l_apb_read();
}

static struct clocksource cnc1800l_clocksource = {
	.name		= "cnc1800l-apb-timer",
	.rating		= 300,
	.read		= cnc1800l_clocksource_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static u64 notrace cnc1800l_sched_clock_read(void)
{
	return cnc1800l_apb_read();
}

static int cnc1800l_clkevt_shutdown(struct clock_event_device *evt)
{
	writel_relaxed(0, ahb_base + TIMER_ENABLE);
	return 0;
}

static int cnc1800l_clkevt_set_next_event(unsigned long delta,
					   struct clock_event_device *evt)
{
	writel_relaxed(0, ahb_base + TIMER_ENABLE);

	/* clear and re-arm the interrupt */
	writel_relaxed(0x7, ahb_base + TIMER_INTR_CLR);
	writel_relaxed(0x0, ahb_base + TIMER_INTR_CLR);

	writel_relaxed(delta, ahb_base + TIMER_THRESHOLD_0);
	writel_relaxed(0, ahb_base + TIMER_PRE_LOAD_0);

	writel_relaxed(TIMER_MODE_ONESHOT, ahb_base + TIMER_MODE);
	writel_relaxed(1, ahb_base + TIMER_INTR_EN);
	writel_relaxed(1, ahb_base + TIMER_ENABLE);

	return 0;
}

static struct clock_event_device cnc1800l_clockevent = {
	.name			= "cnc1800l-ahb-timer",
	.features		= CLOCK_EVT_FEAT_ONESHOT,
	.rating			= 400,
	.set_next_event		= cnc1800l_clkevt_set_next_event,
	.set_state_shutdown	= cnc1800l_clkevt_shutdown,
	.set_state_oneshot	= cnc1800l_clkevt_shutdown,
	.tick_resume		= cnc1800l_clkevt_shutdown,
};

static irqreturn_t cnc1800l_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	writel_relaxed(1, ahb_base + TIMER_INTR_CLR);
	writel_relaxed(0, ahb_base + TIMER_INTR_CLR);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int __init cnc1800l_timer_init(struct device_node *np)
{
	struct clk *apb_clk, *ahb_clk;
	unsigned long apb_rate;
	int irq, ret;
	u32 ctrl;

	ahb_base = of_iomap(np, 0);
	if (!ahb_base) {
		pr_err("%pOFn: unable to map ahb timer\n", np);
		return -ENXIO;
	}

	apb_base = of_iomap(np, 1);
	if (!apb_base) {
		pr_err("%pOFn: unable to map apb timer\n", np);
		return -ENXIO;
	}

	ahb_clk = of_clk_get_by_name(np, "ahb");
	if (IS_ERR(ahb_clk)) {
		pr_err("%pOFn: unable to get ahb clock\n", np);
		return PTR_ERR(ahb_clk);
	}
	clk_prepare_enable(ahb_clk);

	apb_clk = of_clk_get_by_name(np, "apb");
	if (IS_ERR(apb_clk)) {
		pr_err("%pOFn: unable to get apb clock\n", np);
		return PTR_ERR(apb_clk);
	}
	clk_prepare_enable(apb_clk);
	apb_rate = clk_get_rate(apb_clk);

	irq = irq_of_parse_and_map(np, 0);
	if (!irq) {
		pr_err("%pOFn: unable to parse irq\n", np);
		return -EINVAL;
	}

	/*
	 * Free-running APB clocksource: load both 16-bit halves with all
	 * ones, mask its own interrupt, and let it wrap continuously in
	 * periodic (auto-reload) mode.
	 */
	writel_relaxed(0, apb_base + APB_TIMER_CTRL);
	writew_relaxed(0xffff, apb_base + APB_TIMER_LOADCOUNT);
	writew_relaxed(0xffff, apb_base + APB_TIMER_LOADCOUNT + 2);

	ctrl = readl_relaxed(apb_base + APB_TIMER_CTRL);
	ctrl = (ctrl | APB_TIMER_CTL_INTMASK) & ~APB_TIMER_CTL_PERIODIC;
	writel_relaxed(ctrl, apb_base + APB_TIMER_CTRL);

	ret = clocksource_register_hz(&cnc1800l_clocksource, apb_rate);
	if (ret) {
		pr_err("%pOFn: failed to register clocksource\n", np);
		return ret;
	}

	writel_relaxed(ctrl | APB_TIMER_CTL_ENABLE | APB_TIMER_CTL_PERIODIC,
		       apb_base + APB_TIMER_CTRL);

	sched_clock_register(cnc1800l_sched_clock_read, 32, apb_rate);

	/* AHB clockevent, one-shot only */
	writel_relaxed(0, ahb_base + TIMER_INTR_EN);
	writel_relaxed(0, ahb_base + TIMER_ENABLE);
	writel_relaxed(1, ahb_base + TIMER_INTR_CLR);
	writel_relaxed(0, ahb_base + TIMER_INTR_CLR);

	ret = request_irq(irq, cnc1800l_timer_interrupt, IRQF_TIMER,
			   "cnc1800l-ahb-timer", &cnc1800l_clockevent);
	if (ret) {
		pr_err("%pOFn: failed to request irq\n", np);
		return ret;
	}

	cnc1800l_clockevent.cpumask = cpumask_of(0);
	cnc1800l_clockevent.irq = irq;
	clockevents_config_and_register(&cnc1800l_clockevent,
					 clk_get_rate(ahb_clk), 0xf, 0xfffffffc);

	return 0;
}

TIMER_OF_DECLARE(cnc1800l_timer, "celestial,cnc1800l-timer", cnc1800l_timer_init);
