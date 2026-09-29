// SPDX-License-Identifier: GPL-2.0
/*
 * I2C bus driver for the Celestial Semiconductor CNC1800L SoC.
 *
 * The controller's register set uses DesignWare-style names
 * (IC_ENABLE, IC_SCLH_CNT/IC_SCLL_CNT, IC_DATA_CMD, IC_RAW_INTR_STAT...)
 * but not the same offsets or IC_DATA_CMD command encoding as the real
 * Synopsys DW_apb_i2c IP, so this is a new driver rather than a
 * "i2c-designware"/"i2c-davinci" DT-compatible reuse (the vendor driver's
 * own MODULE_DESCRIPTION claiming "TI DaVinci I2C bus adapter" is
 * incorrect -- the register layout matches neither DaVinci nor mainline
 * DesignWare I2C).
 *
 * The IC_DATA_CMD command encoding and polling protocol below (including
 * the exact magic command values) are carried over unchanged from the
 * vendor driver (drivers/i2c/busses/i2c-cnc18xx.c in the GPL release),
 * which is entirely polled -- there is no working interrupt path to port.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define IC_ENABLE		0x00
#define IC_SCLH_CNT		0x04
#define IC_SCLL_CNT		0x08
#define IC_DATA_CMD		0x0c
#define IC_RAW_INTR_STAT	0x14
#define IC_STAT			0x20
#define IC_FIFO_CLR		0x34
#define IC_RESET		0x38

#define IC_STAT_ENABLE_DONE	BIT(0)	/* enable/disable has taken effect */
#define IC_STAT_TFNF		BIT(1)	/* Tx FIFO not full */
#define IC_STAT_TX_DONE		BIT(2)	/* transfer/stop fully completed */
#define IC_STAT_RFNE		BIT(3)	/* Rx FIFO not empty */
#define IC_STAT_ACTIVITY	BIT(5)	/* bus busy */

#define IC_RAW_INTR_ERR_MASK	(BIT(5) | BIT(6) | BIT(7)) /* nack|arblost|timeout */
#define IC_RAW_INTR_NACK	BIT(5)
#define IC_RAW_INTR_ARB_LOST	BIT(6)
#define IC_RAW_INTR_TIMEOUT	BIT(7)

/* IC_DATA_CMD command encoding, see file header. */
#define CMD_ADDR_START		0x100
#define CMD_ADDR_RD		0x001
#define CMD_WRITE		0x200
#define CMD_STOP		0x100
#define CMD_READ		0x400
#define CMD_ABORT_STOP		0x700

#define CNC1800L_POLL_TIMEOUT_US	1000000
#define CNC1800L_POLL_SLEEP_US		5

struct cnc1800l_i2c {
	struct device		*dev;
	void __iomem		*base;
	struct clk		*clk;
	struct i2c_adapter	adap;
	u32			bus_freq;
	u16			sclh_cnt;
	u16			scll_cnt;
};

static void cnc1800l_i2c_recover(struct cnc1800l_i2c *i2c)
{
	writew_relaxed(1, i2c->base + IC_RESET);
	writew_relaxed(0, i2c->base + IC_RESET);
	writew_relaxed(1, i2c->base + IC_ENABLE);
	writew_relaxed(CMD_ABORT_STOP, i2c->base + IC_DATA_CMD);
}

/* Returns 0 on success, -EIO on NACK/arbitration-lost/timeout. */
static int cnc1800l_i2c_check_error(struct cnc1800l_i2c *i2c)
{
	u16 raw = readw_relaxed(i2c->base + IC_RAW_INTR_STAT);

	if (!(raw & IC_RAW_INTR_ERR_MASK))
		return 0;

	writew_relaxed(0x3, i2c->base + IC_FIFO_CLR);

	if (raw & IC_RAW_INTR_ARB_LOST) {
		writew_relaxed(IC_RAW_INTR_ARB_LOST, i2c->base + IC_RAW_INTR_STAT);
		dev_err(i2c->dev, "arbitration lost\n");
	}
	if (raw & IC_RAW_INTR_TIMEOUT) {
		writew_relaxed(IC_RAW_INTR_TIMEOUT, i2c->base + IC_RAW_INTR_STAT);
		dev_err(i2c->dev, "bus timeout\n");
	}
	if (raw & IC_RAW_INTR_NACK) {
		writew_relaxed(IC_RAW_INTR_NACK, i2c->base + IC_RAW_INTR_STAT);
		writew_relaxed(CMD_ABORT_STOP, i2c->base + IC_DATA_CMD);
		dev_dbg(i2c->dev, "no ack from slave\n");
	}

	return -EIO;
}

static int cnc1800l_i2c_wait_bus_idle(struct cnc1800l_i2c *i2c)
{
	u16 stat;

	return readw_poll_timeout(i2c->base + IC_STAT, stat,
				   !(stat & IC_STAT_ACTIVITY),
				   CNC1800L_POLL_SLEEP_US,
				   CNC1800L_POLL_TIMEOUT_US);
}

static int cnc1800l_i2c_read_msg(struct cnc1800l_i2c *i2c, struct i2c_msg *msg,
				  bool stop)
{
	unsigned int cmd_cnt = 0, data_cnt = 0;
	u16 stat;
	int ret;

	while (data_cnt < msg->len) {
		while (cmd_cnt < msg->len &&
		       (readw_relaxed(i2c->base + IC_STAT) & IC_STAT_TFNF)) {
			u16 cmd = CMD_READ;

			if (stop && cmd_cnt == msg->len - 1)
				cmd |= CMD_STOP;
			writew_relaxed(cmd, i2c->base + IC_DATA_CMD);
			cmd_cnt++;
		}

		if (readw_relaxed(i2c->base + IC_STAT) & IC_STAT_RFNE)
			msg->buf[data_cnt++] = readw_relaxed(i2c->base + IC_DATA_CMD);

		ret = cnc1800l_i2c_check_error(i2c);
		if (ret)
			return ret;
	}

	if (!stop)
		return 0;

	ret = readw_poll_timeout(i2c->base + IC_STAT, stat,
				  stat & IC_STAT_TX_DONE,
				  CNC1800L_POLL_SLEEP_US,
				  CNC1800L_POLL_TIMEOUT_US);
	if (ret) {
		cnc1800l_i2c_recover(i2c);
		return ret;
	}

	return cnc1800l_i2c_wait_bus_idle(i2c);
}

static int cnc1800l_i2c_write_msg(struct cnc1800l_i2c *i2c, struct i2c_msg *msg,
				   bool stop)
{
	unsigned int data_cnt = 0;
	u16 stat;
	int ret;

	while (data_cnt < msg->len) {
		if (readw_relaxed(i2c->base + IC_STAT) & IC_STAT_TFNF) {
			u16 cmd = CMD_WRITE | msg->buf[data_cnt];

			if (stop && data_cnt == msg->len - 1)
				cmd |= CMD_STOP;
			writew_relaxed(cmd, i2c->base + IC_DATA_CMD);
			data_cnt++;
		}

		ret = cnc1800l_i2c_check_error(i2c);
		if (ret)
			return ret;
	}

	if (!stop)
		return 0;

	ret = readw_poll_timeout(i2c->base + IC_STAT, stat,
				  stat & IC_STAT_TX_DONE,
				  CNC1800L_POLL_SLEEP_US,
				  CNC1800L_POLL_TIMEOUT_US);
	if (ret) {
		cnc1800l_i2c_recover(i2c);
		return ret;
	}

	return cnc1800l_i2c_wait_bus_idle(i2c);
}

static int cnc1800l_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			      int num)
{
	struct cnc1800l_i2c *i2c = i2c_get_adapdata(adap);
	int i, ret;

	ret = cnc1800l_i2c_wait_bus_idle(i2c);
	if (ret) {
		cnc1800l_i2c_recover(i2c);
		return ret;
	}

	writew_relaxed(1, i2c->base + IC_ENABLE);

	for (i = 0; i < num; i++) {
		bool stop = (i == num - 1);
		u16 addr_cmd = CMD_ADDR_START | ((msgs[i].addr << 1) & 0xfe);

		if (msgs[i].flags & I2C_M_RD)
			addr_cmd |= CMD_ADDR_RD;
		writew_relaxed(addr_cmd, i2c->base + IC_DATA_CMD);

		if (msgs[i].flags & I2C_M_RD)
			ret = cnc1800l_i2c_read_msg(i2c, &msgs[i], stop);
		else
			ret = cnc1800l_i2c_write_msg(i2c, &msgs[i], stop);

		if (ret)
			return ret;
	}

	return num;
}

static u32 cnc1800l_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm cnc1800l_i2c_algo = {
	.master_xfer	= cnc1800l_i2c_xfer,
	.functionality	= cnc1800l_i2c_func,
};

static void cnc1800l_i2c_calc_timing(struct cnc1800l_i2c *i2c,
				      unsigned long clk_hz)
{
	unsigned long clk_mhz = clk_hz / 1000000;
	unsigned int scl_cnt;

	if (i2c->bus_freq >= 400000) {
		i2c->sclh_cnt = (10 * clk_mhz) / 24;
		scl_cnt = (clk_mhz * 10) / 8;
	} else {
		i2c->sclh_cnt = (10 * clk_mhz) / 4;
		scl_cnt = clk_mhz * 5;
	}
	i2c->scll_cnt = scl_cnt - i2c->sclh_cnt;
}

static void cnc1800l_i2c_hw_init(struct cnc1800l_i2c *i2c)
{
	writew_relaxed(0, i2c->base + IC_ENABLE);
	writew_relaxed(i2c->sclh_cnt, i2c->base + IC_SCLH_CNT);
	writew_relaxed(i2c->scll_cnt, i2c->base + IC_SCLL_CNT);
}

static int cnc1800l_i2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cnc1800l_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->dev = dev;
	i2c->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2c->base))
		return PTR_ERR(i2c->base);

	i2c->clk = devm_clk_get_optional_enabled(dev, NULL);
	if (IS_ERR(i2c->clk))
		return PTR_ERR(i2c->clk);

	if (of_property_read_u32(dev->of_node, "clock-frequency",
				  &i2c->bus_freq))
		i2c->bus_freq = 100000;

	cnc1800l_i2c_calc_timing(i2c, clk_get_rate(i2c->clk) ?: 47250000);
	cnc1800l_i2c_hw_init(i2c);

	i2c_set_adapdata(&i2c->adap, i2c);
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.class = I2C_CLASS_HWMON;
	i2c->adap.algo = &cnc1800l_i2c_algo;
	i2c->adap.dev.parent = dev;
	i2c->adap.dev.of_node = dev->of_node;
	i2c->adap.timeout = HZ;
	strscpy(i2c->adap.name, "cnc1800l-i2c", sizeof(i2c->adap.name));

	ret = devm_i2c_add_adapter(dev, &i2c->adap);
	if (ret)
		return dev_err_probe(dev, ret, "failed to add adapter\n");

	return 0;
}

static const struct of_device_id cnc1800l_i2c_match[] = {
	{ .compatible = "celestial,cnc1800l-i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, cnc1800l_i2c_match);

static struct platform_driver cnc1800l_i2c_driver = {
	.probe	= cnc1800l_i2c_probe,
	.driver	= {
		.name = "cnc1800l-i2c",
		.of_match_table = cnc1800l_i2c_match,
	},
};
module_platform_driver(cnc1800l_i2c_driver);

MODULE_DESCRIPTION("Celestial CNC1800L I2C bus driver");
MODULE_LICENSE("GPL");
