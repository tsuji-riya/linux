// SPDX-License-Identifier: GPL-2.0
/*
 * Clock/reset controller driver for the Celestial Semiconductor CNC1800L
 * SoC's combined clock-generator/power-control/reset register block.
 *
 * The vendor 2.6.32 driver (cnc1800l_power_clk.c) implements this block as
 * roughly 90 ad-hoc register-poking functions rather than a clock-framework
 * provider, and most of the domains it controls (display, audio, HDMI,
 * transport, graphics...) are out of scope for this port. The only gates
 * this driver exposes are the ones the Core+Storage+USB scope actually
 * needs: the USB0/USB1 PHY power gates and the USB reset line, both taken
 * from the vendor's power_usb0phy_down()/power_usb1phy_down()/
 * clock_usb_reset() register-level behaviour.
 */

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>

/* Offsets are relative to the block's reg base (0xb2100000). */
#define CLOCK_SOFT_RESET	0x200
#define CLOCK_SOFT_RESET_USB	BIT(4)

#define CLOCK_USB_ULPI_BYPASS	0x218

/*
 * The vendor board bring-up code for every peripheral gated through this
 * mux/clock-select register (MSHCI included) unconditionally writes
 * 0xffffffff here before using the peripheral. The individual bitfields
 * are not documented in any source available for this port, so this is
 * reproduced verbatim as an opaque "enable everything this register
 * gates" step rather than guessed at bit-by-bit.
 */
#define CLOCK_MUX_ALL_ENABLE	0x140

#define CNC1800L_NUM_CLKS	2
#define CNC1800L_CLK_USB0_PHY	0
#define CNC1800L_CLK_USB1_PHY	1

#define CNC1800L_NUM_RESETS	1
#define CNC1800L_RESET_USB	0

struct cnc1800l_clk {
	void __iomem			*base;
	struct reset_controller_dev	rcdev;
};

static struct cnc1800l_clk *to_cnc1800l_clk(struct reset_controller_dev *rc)
{
	return container_of(rc, struct cnc1800l_clk, rcdev);
}

static int cnc1800l_reset_update(struct reset_controller_dev *rcdev,
				  unsigned long id, bool assert)
{
	struct cnc1800l_clk *clk = to_cnc1800l_clk(rcdev);
	u32 val;

	if (id != CNC1800L_RESET_USB)
		return -EINVAL;

	val = readl_relaxed(clk->base + CLOCK_SOFT_RESET);
	if (assert)
		val &= ~CLOCK_SOFT_RESET_USB;
	else
		val |= CLOCK_SOFT_RESET_USB;
	writel_relaxed(val, clk->base + CLOCK_SOFT_RESET);

	return 0;
}

static int cnc1800l_reset_assert(struct reset_controller_dev *rcdev,
				  unsigned long id)
{
	return cnc1800l_reset_update(rcdev, id, true);
}

static int cnc1800l_reset_deassert(struct reset_controller_dev *rcdev,
				    unsigned long id)
{
	return cnc1800l_reset_update(rcdev, id, false);
}

static int cnc1800l_reset_status(struct reset_controller_dev *rcdev,
				  unsigned long id)
{
	struct cnc1800l_clk *clk = to_cnc1800l_clk(rcdev);
	u32 val;

	if (id != CNC1800L_RESET_USB)
		return -EINVAL;

	val = readl_relaxed(clk->base + CLOCK_SOFT_RESET);

	/* bit set == out of reset/running */
	return !(val & CLOCK_SOFT_RESET_USB);
}

static const struct reset_control_ops cnc1800l_reset_ops = {
	.assert		= cnc1800l_reset_assert,
	.deassert	= cnc1800l_reset_deassert,
	.status		= cnc1800l_reset_status,
};

static int cnc1800l_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct clk_hw_onecell_data *clk_data;
	struct cnc1800l_clk *clk;
	struct clk_hw *hw;
	int ret;

	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(clk->base))
		return PTR_ERR(clk->base);

	writel_relaxed(0xffffffff, clk->base + CLOCK_MUX_ALL_ENABLE);

	clk->rcdev.owner = THIS_MODULE;
	clk->rcdev.nr_resets = CNC1800L_NUM_RESETS;
	clk->rcdev.ops = &cnc1800l_reset_ops;
	clk->rcdev.of_node = np;
	ret = devm_reset_controller_register(dev, &clk->rcdev);
	if (ret)
		return dev_err_probe(dev, ret,
				      "failed to register reset controller\n");

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws,
						  CNC1800L_NUM_CLKS),
				 GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;
	clk_data->num = CNC1800L_NUM_CLKS;

	/*
	 * CLOCK_USB_ULPI_BYPASS bit2/bit3: set = PHY analog block powered
	 * down, clear = powered up. Model as a gate clock whose "enable"
	 * clears the bit (CLK_GATE_SET_TO_DISABLE).
	 */
	hw = devm_clk_hw_register_gate(dev, "usb0-phy", NULL, 0,
					clk->base + CLOCK_USB_ULPI_BYPASS, 2,
					CLK_GATE_SET_TO_DISABLE, NULL);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	clk_data->hws[CNC1800L_CLK_USB0_PHY] = hw;

	hw = devm_clk_hw_register_gate(dev, "usb1-phy", NULL, 0,
					clk->base + CLOCK_USB_ULPI_BYPASS, 3,
					CLK_GATE_SET_TO_DISABLE, NULL);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	clk_data->hws[CNC1800L_CLK_USB1_PHY] = hw;

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					    clk_data);
}

static const struct of_device_id cnc1800l_clk_match[] = {
	{ .compatible = "celestial,cnc1800l-clk" },
	{ }
};
MODULE_DEVICE_TABLE(of, cnc1800l_clk_match);

static struct platform_driver cnc1800l_clk_driver = {
	.probe	= cnc1800l_clk_probe,
	.driver	= {
		.name = "cnc1800l-clk",
		.of_match_table = cnc1800l_clk_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(cnc1800l_clk_driver);
