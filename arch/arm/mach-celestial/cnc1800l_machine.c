#include <linux/clocksource.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/smp.h>
#include <asm/smp_scu.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/psci.h>
#include <linux/arm-smccc.h>

#define CNC1800L_VIRT_UART0 0xfec00000

static struct map_desc cnc1800l_of_io_desc[] __initdata = {
#ifdef CONFIG_DEBUG_CELESTIAL_UART
	{
		.virtual = CNC1800L_VIRT_UART0,
		.pfn = __phys_to_pfn(0x801f1000),
		.length = SZ_4K,
		.type = MT_DEVICE,
	},
#endif
};

static void __init cnc1800l_of_map_io(void)
{
	iotable_init(cnc1800l_of_io_desc, ARRAY_SIZE(cnc1800l_of_io_desc));
}


static void __init cnc1800l_init_machine(void)
{
	return;
}

//based on board_bcm281xx.c
static void cnc1800l_restart(enum reboot_mode mode, const char *cmd){
	struct device_node *np_wdog;
	unsigned short __iomem *p;

	np_wdog = of_find_compatible_node(NULL, NULL, "cavium,cnc1800l-wdt");
	if (!np_wdog) {
		pr_emerg("Couldn't find cavium,cnc1800l-wdt\n");
		return;
	}

	p = (unsigned short*)of_iomap(np_wdog, 0);

	of_node_put(np_wdog);
	if (!p) {
		pr_emerg("failed to obtain wdt vaddress\n");
		return;
	}

	/* WDT_TORR */
	p[2] = 0x0;
	p[3] = 0x0;
	/* WDT_CCVR */
	p[4] = 0x0;
	p[5] = 0x0;

	//reload
	*((u32*)&p[6]) = 0x76;
	/* WDT_CR, enable WDT and generated System Reset */
	p[0] = 0x1;

	while(1);
}

static const char * const cnc1800l_dt_match[] = {
	"cavium,cnc1800l",
	NULL
};

DT_MACHINE_START(CNC1800L, "Cavium Celestial CNC1800L")
	.map_io		= cnc1800l_of_map_io,
	.init_machine	= cnc1800l_init_machine,
	.restart	= cnc1800l_restart,
	.dt_compat	= cnc1800l_dt_match,
MACHINE_END