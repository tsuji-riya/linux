// SPDX-License-Identifier: GPL-2.0
/*
 * Pin mux driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * The hardware is a set of 16-bit "group enable" registers: each register
 * gates a whole bank of pins between GPIO and one on-chip peripheral (or a
 * small choice of peripherals) rather than offering a per-pin function
 * field, so this driver models each hardware group as exactly one pinmux
 * function with exactly one group of the same name. Individual pin numbers
 * within a group are not modelled (no pins array is registered) since the
 * vendor documentation available for this port only identifies pins by
 * their group, not by an authoritative per-pin table.
 */

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>

#include "pinctrl-utils.h"

#define REG_PINMUX_I2C		0x004
#define REG_PINMUX_UART		0x008
#define REG_PINMUX_SDIO		0x024

struct cnc1800l_pmx_func {
	const char	*name;
	const char	*group;
	u16		reg;
	u16		set_mask;
	u16		clr_mask;
};

/*
 * Bit patterns below are taken directly from the vendor
 * cnc1800l_mux.c pinmux_enable_i2c()/pinmux_enable_uart() functions.
 */
static const struct cnc1800l_pmx_func cnc1800l_functions[] = {
	{ "i2c0",  "i2c0",  REG_PINMUX_I2C,  BIT(0), 0 },
	{ "uart0", "uart0", REG_PINMUX_UART, BIT(0), 0 },
	{ "uart1", "uart1", REG_PINMUX_UART, BIT(5) | BIT(1), BIT(3) },
	/*
	 * Confirmed by the TigaMini board's MSHCI probe function, which
	 * writes 0x01 to this register before bringing up the SD/SDIO
	 * controller (and clears it on remove).
	 */
	{ "sdio",  "sdio",  REG_PINMUX_SDIO, BIT(0), 0 },
};

struct cnc1800l_pinctrl {
	void __iomem		*base;
	struct pinctrl_dev	*pctl;
};

static int cnc1800l_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(cnc1800l_functions);
}

static const char *cnc1800l_get_group_name(struct pinctrl_dev *pctldev,
					    unsigned int selector)
{
	return cnc1800l_functions[selector].group;
}

static int cnc1800l_get_group_pins(struct pinctrl_dev *pctldev,
				    unsigned int selector,
				    const unsigned int **pins,
				    unsigned int *num_pins)
{
	*pins = NULL;
	*num_pins = 0;
	return 0;
}

static const struct pinctrl_ops cnc1800l_pctlops = {
	.get_groups_count	= cnc1800l_get_groups_count,
	.get_group_name		= cnc1800l_get_group_name,
	.get_group_pins		= cnc1800l_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_all,
	.dt_free_map		= pinctrl_utils_free_map,
};

static int cnc1800l_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(cnc1800l_functions);
}

static const char *cnc1800l_get_function_name(struct pinctrl_dev *pctldev,
					       unsigned int selector)
{
	return cnc1800l_functions[selector].name;
}

static int cnc1800l_get_function_groups(struct pinctrl_dev *pctldev,
					 unsigned int selector,
					 const char * const **groups,
					 unsigned int *num_groups)
{
	*groups = &cnc1800l_functions[selector].group;
	*num_groups = 1;
	return 0;
}

static int cnc1800l_set_mux(struct pinctrl_dev *pctldev,
			     unsigned int func_selector,
			     unsigned int group_selector)
{
	struct cnc1800l_pinctrl *pmx = pinctrl_dev_get_drvdata(pctldev);
	const struct cnc1800l_pmx_func *f = &cnc1800l_functions[func_selector];
	u16 val;

	val = readw_relaxed(pmx->base + f->reg);
	val &= ~f->clr_mask;
	val |= f->set_mask;
	writew_relaxed(val, pmx->base + f->reg);

	return 0;
}

static const struct pinmux_ops cnc1800l_pmxops = {
	.get_functions_count	= cnc1800l_get_functions_count,
	.get_function_name	= cnc1800l_get_function_name,
	.get_function_groups	= cnc1800l_get_function_groups,
	.set_mux		= cnc1800l_set_mux,
	.strict			= true,
};

static int cnc1800l_pinconf_get(struct pinctrl_dev *pctldev,
				 unsigned int pin, unsigned long *config)
{
	return -ENOTSUPP;
}

static int cnc1800l_pinconf_set(struct pinctrl_dev *pctldev,
				 unsigned int pin, unsigned long *configs,
				 unsigned int num_configs)
{
	return -ENOTSUPP;
}

static const struct pinconf_ops cnc1800l_confops = {
	.is_generic		= true,
	.pin_config_get		= cnc1800l_pinconf_get,
	.pin_config_set		= cnc1800l_pinconf_set,
};

static struct pinctrl_desc cnc1800l_desc = {
	.name	= "cnc1800l-pinctrl",
	.pctlops = &cnc1800l_pctlops,
	.pmxops	= &cnc1800l_pmxops,
	.confops = &cnc1800l_confops,
	.owner	= THIS_MODULE,
};

static int cnc1800l_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cnc1800l_pinctrl *pmx;

	pmx = devm_kzalloc(dev, sizeof(*pmx), GFP_KERNEL);
	if (!pmx)
		return -ENOMEM;

	pmx->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pmx->base))
		return PTR_ERR(pmx->base);

	pmx->pctl = devm_pinctrl_register(dev, &cnc1800l_desc, pmx);
	if (IS_ERR(pmx->pctl))
		return dev_err_probe(dev, PTR_ERR(pmx->pctl),
				      "failed to register pinctrl\n");

	return 0;
}

static const struct of_device_id cnc1800l_pinctrl_match[] = {
	{ .compatible = "celestial,cnc1800l-pinctrl" },
	{ }
};
MODULE_DEVICE_TABLE(of, cnc1800l_pinctrl_match);

static struct platform_driver cnc1800l_pinctrl_driver = {
	.probe	= cnc1800l_pinctrl_probe,
	.driver	= {
		.name = "cnc1800l-pinctrl",
		.of_match_table = cnc1800l_pinctrl_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(cnc1800l_pinctrl_driver);
