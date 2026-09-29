// SPDX-License-Identifier: GPL-2.0
/*
 * Interrupt controller driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * The VIC is a single-level, 32-line, level-triggered-only Designware-style
 * controller: one register doubles as enable/ack (writing 0 to a bit both
 * disables and acknowledges that line), a second register masks lines
 * without disabling them (the vendor board code always leaves it at 0, i.e.
 * fully unmasked, and uses the enable register alone for gating), and a
 * "final status" register reports the currently pending, enabled and
 * unmasked lines as a bitmask (there is no priority encoder).
 */

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include <asm/exception.h>

#define VIC_IRQ_ENABLE		0x00
#define VIC_IRQ_MASK		0x08
#define VIC_IRQ_INTFORCE	0x10
#define VIC_IRQ_RAW_STATUS	0x18
#define VIC_IRQ_STATUS		0x20
#define VIC_IRQ_MASK_STATUS	0x28
#define VIC_IRQ_FINAL_STATUS	0x30

#define CNC1800L_NR_IRQS	32

struct cnc1800l_irq_data {
	void __iomem		*base;
	struct irq_domain	*domain;
};

static struct cnc1800l_irq_data *cnc1800l_intc;

static void cnc1800l_irq_mask(struct irq_data *d)
{
	struct cnc1800l_irq_data *intc = irq_data_get_irq_chip_data(d);
	u32 val;

	val = readl_relaxed(intc->base + VIC_IRQ_ENABLE);
	val &= ~BIT(d->hwirq);
	writel_relaxed(val, intc->base + VIC_IRQ_ENABLE);
}

static void cnc1800l_irq_unmask(struct irq_data *d)
{
	struct cnc1800l_irq_data *intc = irq_data_get_irq_chip_data(d);
	u32 val;

	val = readl_relaxed(intc->base + VIC_IRQ_ENABLE);
	val |= BIT(d->hwirq);
	writel_relaxed(val, intc->base + VIC_IRQ_ENABLE);
}

/*
 * The enable register doubles as the ack register on this IP (clearing a
 * line's enable bit also acknowledges it), so ack is a no-op beyond what
 * .irq_mask already does; handle_level_irq() calls .irq_mask before the
 * handler and .irq_unmask after, which reproduces the vendor driver's
 * mask_ack/unmask behaviour exactly.
 */
static void cnc1800l_irq_ack(struct irq_data *d)
{
}

static struct irq_chip cnc1800l_irq_chip = {
	.name		= "cnc1800l-vic",
	.irq_ack	= cnc1800l_irq_ack,
	.irq_mask	= cnc1800l_irq_mask,
	.irq_unmask	= cnc1800l_irq_unmask,
};

static int cnc1800l_irq_map(struct irq_domain *d, unsigned int virq,
			     irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(virq, &cnc1800l_irq_chip, handle_level_irq);
	irq_set_chip_data(virq, d->host_data);
	irq_set_probe(virq);

	return 0;
}

static const struct irq_domain_ops cnc1800l_irq_domain_ops = {
	.map	= cnc1800l_irq_map,
	.xlate	= irq_domain_xlate_onecell,
};

static void __exception_irq_entry cnc1800l_handle_irq(struct pt_regs *regs)
{
	struct cnc1800l_irq_data *intc = cnc1800l_intc;
	unsigned long status;
	int hwirq;

	status = readl_relaxed(intc->base + VIC_IRQ_FINAL_STATUS);
	for_each_set_bit(hwirq, &status, CNC1800L_NR_IRQS)
		generic_handle_domain_irq(intc->domain, hwirq);
}

static int __init cnc1800l_irq_init(struct device_node *node,
				     struct device_node *parent)
{
	struct cnc1800l_irq_data *intc;
	int ret;

	intc = kzalloc(sizeof(*intc), GFP_KERNEL);
	if (!intc)
		return -ENOMEM;

	intc->base = of_iomap(node, 0);
	if (!intc->base) {
		pr_err("cnc1800l-irq: unable to map IO memory\n");
		ret = -ENOMEM;
		goto err_free;
	}

	intc->domain = irq_domain_create_linear(of_fwnode_handle(node),
						 CNC1800L_NR_IRQS,
						 &cnc1800l_irq_domain_ops, intc);
	if (!intc->domain) {
		pr_err("cnc1800l-irq: unable to add irq domain\n");
		ret = -ENOMEM;
		goto err_unmap;
	}

	/* Clear forced interrupts, disable all lines, unmask all lines */
	writel_relaxed(0, intc->base + VIC_IRQ_INTFORCE);
	writel_relaxed(0, intc->base + VIC_IRQ_ENABLE);
	writel_relaxed(0, intc->base + VIC_IRQ_MASK);

	cnc1800l_intc = intc;
	set_handle_irq(cnc1800l_handle_irq);

	return 0;

err_unmap:
	iounmap(intc->base);
err_free:
	kfree(intc);
	return ret;
}

IRQCHIP_DECLARE(cnc1800l_irq, "celestial,cnc1800l-vic", cnc1800l_irq_init);
