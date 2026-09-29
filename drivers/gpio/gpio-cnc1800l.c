// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * 64 lines split across two 32-bit "ports" with a DR (output data) / DDR
 * (direction) / EXT_PORT (input data) register triplet each, at
 * non-standard offsets that don't match the Synopsys DesignWare GPIO IP
 * this otherwise resembles. Direction polarity is inverted from the usual
 * convention: DDR bit set means *input*, clear means *output* (taken
 * directly from the vendor gpio_hw_set_direct() in cnc18xx_gpio.c).
 */

#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define REG_SWPORTA_DR		0x20
#define REG_SWPORTA_DDR		0x24
#define REG_SWPORTB_DR		0x34
#define REG_SWPORTB_DDR		0x38
#define REG_EXT_PORTA		0x60
#define REG_EXT_PORTB		0x64

struct cnc1800l_gpio {
	struct gpio_chip	gc;
	void __iomem		*base;
	spinlock_t		lock;
};

static void cnc1800l_port_regs(unsigned int offset, unsigned int *bit,
				u32 *dr, u32 *ddr, u32 *ext)
{
	if (offset < 32) {
		*bit = offset;
		*dr = REG_SWPORTA_DR;
		*ddr = REG_SWPORTA_DDR;
		*ext = REG_EXT_PORTA;
	} else {
		*bit = offset - 32;
		*dr = REG_SWPORTB_DR;
		*ddr = REG_SWPORTB_DDR;
		*ext = REG_EXT_PORTB;
	}
}

static int cnc1800l_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct cnc1800l_gpio *g = gpiochip_get_data(gc);
	unsigned int bit;
	u32 dr, ddr, ext;

	cnc1800l_port_regs(offset, &bit, &dr, &ddr, &ext);

	return !!(readl_relaxed(g->base + ext) & BIT(bit));
}

static int cnc1800l_gpio_set(struct gpio_chip *gc, unsigned int offset,
			      int value)
{
	struct cnc1800l_gpio *g = gpiochip_get_data(gc);
	unsigned long flags;
	unsigned int bit;
	u32 dr, ddr, ext, val;

	cnc1800l_port_regs(offset, &bit, &dr, &ddr, &ext);

	spin_lock_irqsave(&g->lock, flags);
	val = readl_relaxed(g->base + dr);
	if (value)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);
	writel_relaxed(val, g->base + dr);
	spin_unlock_irqrestore(&g->lock, flags);

	return 0;
}

static int cnc1800l_gpio_direction_input(struct gpio_chip *gc,
					  unsigned int offset)
{
	struct cnc1800l_gpio *g = gpiochip_get_data(gc);
	unsigned long flags;
	unsigned int bit;
	u32 dr, ddr, ext, val;

	cnc1800l_port_regs(offset, &bit, &dr, &ddr, &ext);

	spin_lock_irqsave(&g->lock, flags);
	val = readl_relaxed(g->base + ddr);
	val |= BIT(bit);		/* set == input */
	writel_relaxed(val, g->base + ddr);
	spin_unlock_irqrestore(&g->lock, flags);

	return 0;
}

static int cnc1800l_gpio_direction_output(struct gpio_chip *gc,
					   unsigned int offset, int value)
{
	struct cnc1800l_gpio *g = gpiochip_get_data(gc);
	unsigned long flags;
	unsigned int bit;
	u32 dr, ddr, ext, val;

	cnc1800l_gpio_set(gc, offset, value);

	cnc1800l_port_regs(offset, &bit, &dr, &ddr, &ext);

	spin_lock_irqsave(&g->lock, flags);
	val = readl_relaxed(g->base + ddr);
	val &= ~BIT(bit);		/* clear == output */
	writel_relaxed(val, g->base + ddr);
	spin_unlock_irqrestore(&g->lock, flags);

	return 0;
}

static int cnc1800l_gpio_get_direction(struct gpio_chip *gc,
					unsigned int offset)
{
	struct cnc1800l_gpio *g = gpiochip_get_data(gc);
	unsigned int bit;
	u32 dr, ddr, ext;

	cnc1800l_port_regs(offset, &bit, &dr, &ddr, &ext);

	if (readl_relaxed(g->base + ddr) & BIT(bit))
		return GPIO_LINE_DIRECTION_IN;

	return GPIO_LINE_DIRECTION_OUT;
}

static int cnc1800l_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cnc1800l_gpio *g;
	u32 ngpios = 64;
	int ret;

	g = devm_kzalloc(dev, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;

	g->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(g->base))
		return PTR_ERR(g->base);

	spin_lock_init(&g->lock);
	of_property_read_u32(dev->of_node, "ngpios", &ngpios);

	g->gc.label = "cnc1800l-gpio";
	g->gc.parent = dev;
	g->gc.owner = THIS_MODULE;
	g->gc.base = -1;
	g->gc.ngpio = ngpios;
	g->gc.get_direction = cnc1800l_gpio_get_direction;
	g->gc.direction_input = cnc1800l_gpio_direction_input;
	g->gc.direction_output = cnc1800l_gpio_direction_output;
	g->gc.get = cnc1800l_gpio_get;
	g->gc.set = cnc1800l_gpio_set;
	g->gc.can_sleep = false;

	ret = devm_gpiochip_add_data(dev, &g->gc, g);
	if (ret)
		return dev_err_probe(dev, ret, "failed to add gpiochip\n");

	return 0;
}

static const struct of_device_id cnc1800l_gpio_match[] = {
	{ .compatible = "celestial,cnc1800l-gpio" },
	{ }
};
MODULE_DEVICE_TABLE(of, cnc1800l_gpio_match);

static struct platform_driver cnc1800l_gpio_driver = {
	.probe	= cnc1800l_gpio_probe,
	.driver	= {
		.name = "cnc1800l-gpio",
		.of_match_table = cnc1800l_gpio_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(cnc1800l_gpio_driver);
