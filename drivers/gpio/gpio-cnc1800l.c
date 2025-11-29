/****************************************************************************
  * Copyright (C) 2008-2010 Celestial Semiconductor Inc.
  * All rights reserved
  *
  * [RELEASE HISTORY]
  * VERSION  DATE       AUTHOR                  DESCRIPTION
  * 0.1      10-04-?   Hao.Ran           			Original
  ****************************************************************************
*/
/*
 * Copyright (C) 2013 Altera Corporation
 * Based on gpio-mpc8xxx.c
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h> /* For of_mm_gpio_chip */
#include <linux/platform_device.h>

#define REG_GPIO_SWPORTA_DR             0x20
#define REG_GPIO_SWPORTA_DDR            0x24
#define REG_GPIO_EXT_PORTA              0x60


#define REG_GPIO_SWPORTB_DR             0x34
#define REG_GPIO_SWPORTB_DDR            0x38
#define REG_GPIO_EXT_PORTB              0x64

#define GPIO_MAJOR  	0
#define GPIO_NR_DEVS	64

#define GPIO_PINMUXA    0x30
#define GPIO_PINMUXB    0x44

//IRQ
#define PORTA_MAX_NUMBER        32

#define CNC_GPIO_IRQ            6

#define REG_GPIO_INTMASK_A		0x28
#define REG_GPIO_INTMASK_B		0x3c

#define REG_GPIO_POLARITY_A		0x2c
#define REG_GPIO_POLARITY_B		0x40

#define REG_GPIO_INT_S_A		    0x68
#define REG_GPIO_INT_S_B		    0x6c

struct cnc1800l_gpio_chip {
    struct gpio_chip chip;
	raw_spinlock_t gpio_lock;
	int interrupt_trigger;
	int mapped_irq;
	struct irq_chip irq_chip;
    void __iomem *base;
};

static int cnc1800l_gpio_get(struct gpio_chip *gc, unsigned offset)
{
    struct cnc1800l_gpio_chip* cnc_gc = gpiochip_get_data(gc);
    unsigned int *gpio_base = (unsigned int*)cnc_gc->base;


    if((offset < 32)&&(offset >= 0))
	{
        return (gpio_base[REG_GPIO_EXT_PORTA >> 2] >> offset) & 0x1;
	}
	else if((offset >= 32)&&(offset < gc->ngpio))
	{
        return (gpio_base[REG_GPIO_EXT_PORTB >> 2] >> offset) & 0x1;
	}

	return 0;
}

static void cnc1800l_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct cnc1800l_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned long flags;
    unsigned int *gpio_base = (unsigned int*)chip->base;

	raw_spin_lock_irqsave(&chip->gpio_lock, flags);

    if(offset < 32 && offset >= 0){
        if(!value){
            gpio_base[REG_GPIO_SWPORTA_DR>>2 ] &= ~(1 << offset);
        }else{
            gpio_base[REG_GPIO_SWPORTA_DR>>2 ] |= (1 << offset);
        }
    }else if(offset >= 32 && offset < gc->ngpio){
        offset -= 32;
        if(!value){
            gpio_base[REG_GPIO_SWPORTB_DR>>2 ] &= ~(1 << offset);
        }else{
            gpio_base[REG_GPIO_SWPORTB_DR>>2 ] |= (1 << offset);
        }
    }
	raw_spin_unlock_irqrestore(&chip->gpio_lock, flags);
}

static int cnc1800_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct cnc1800l_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned long flags;
    unsigned int *gpio_base = (unsigned int*)chip->base;

	raw_spin_lock_irqsave(&chip->gpio_lock, flags);

    if(offset < 32 && offset >= 0){
        gpio_base[REG_GPIO_SWPORTA_DDR >> 2] |= (1 << offset);
    }else if(offset >= 32 && offset < gc->ngpio){
        offset -= 32;
        gpio_base[REG_GPIO_SWPORTB_DDR >> 2] |= (1 << offset);
    }
	raw_spin_unlock_irqrestore(&chip->gpio_lock, flags);

	return 0;
}

static int cnc1800l_gpio_direction_output(struct gpio_chip *gc,
		unsigned offset, int value)
{
	struct cnc1800l_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned long flags;
    unsigned int *gpio_base = (unsigned int*)chip->base;

	raw_spin_lock_irqsave(&chip->gpio_lock, flags);

    if(offset < 32 && offset >= 0){
        gpio_base[REG_GPIO_SWPORTA_DDR >> 2] &= ~(1<<offset);
        if(!value){
            gpio_base[REG_GPIO_SWPORTA_DR>>2 ] &= ~(1 << offset);
        }else{
            gpio_base[REG_GPIO_SWPORTA_DR>>2 ] |= (1 << offset);
        }
    }else if(offset >= 32 && offset < gc->ngpio){
        offset -= 32;
        gpio_base[REG_GPIO_SWPORTB_DDR >> 2] &= ~(1<<offset);
        if(!value){
            gpio_base[REG_GPIO_SWPORTB_DR>>2 ] &= ~(1 << offset);
        }else{
            gpio_base[REG_GPIO_SWPORTB_DR>>2 ] |= (1 << offset);
        }
    }
	raw_spin_unlock_irqrestore(&chip->gpio_lock, flags);

	return 0;
}

static int cnc1800l_gpio_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int reg, ret;
	struct cnc1800l_gpio_chip *cnc_gc;
    struct resource *res;

	cnc_gc = devm_kzalloc(&pdev->dev, sizeof(struct cnc1800l_gpio_chip), GFP_KERNEL);
	if (!cnc_gc)
		return -ENOMEM;

	raw_spin_lock_init(&cnc_gc->gpio_lock);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res){
        kfree(cnc_gc);
        return -EINVAL;
    }

    cnc_gc->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(cnc_gc->base)){
        kfree(cnc_gc);
        return PTR_ERR(cnc_gc->base);
    }

	if (of_property_read_u32(node, "cavium,nrdevs", &reg))
		cnc_gc->chip.ngpio = GPIO_NR_DEVS;
	else
		cnc_gc->chip.ngpio = reg;

	if (cnc_gc->chip.ngpio > GPIO_NR_DEVS) {
		dev_warn(&pdev->dev,
			"ngpio is greater than %d, defaulting to %d\n",
			GPIO_NR_DEVS, GPIO_NR_DEVS);
		cnc_gc->chip.ngpio = GPIO_NR_DEVS;
	}

	cnc_gc->chip.direction_input	= cnc1800_gpio_direction_input;
	cnc_gc->chip.direction_output	= cnc1800l_gpio_direction_output;
	cnc_gc->chip.get		= cnc1800l_gpio_get;
	cnc_gc->chip.set		= cnc1800l_gpio_set;
	cnc_gc->chip.owner		= THIS_MODULE;
	cnc_gc->chip.parent		= &pdev->dev;

    devm_gpiochip_add_data(&pdev->dev, &cnc_gc->chip, cnc_gc);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed adding memory mapped gpiochip\n");
		return ret;
	}

	platform_set_drvdata(pdev, cnc_gc);

	return 0;
}

static const struct of_device_id cnc1800l_gpio_of_match[] = {
	{ .compatible = "cavium,cnc1800l-gpio", },
	{},
};
MODULE_DEVICE_TABLE(of, cnc1800l_gpio_of_match);

static struct platform_driver cnc1800l_gpio_driver = {
	.driver = {
		.name	= "cnc1800l_gpio",
		.of_match_table = cnc1800l_gpio_of_match,
	},
	.probe		= cnc1800l_gpio_probe,
};

static int __init cnc1800l_gpio_init(void)
{
	return platform_driver_register(&cnc1800l_gpio_driver);
}
subsys_initcall(cnc1800l_gpio_init);

static void __exit cnc1800l_gpio_exit(void)
{
	platform_driver_unregister(&cnc1800l_gpio_driver);
}
module_exit(cnc1800l_gpio_exit);

MODULE_AUTHOR("Cavium");
MODULE_DESCRIPTION("Cavium Celestial GPIO driver");
MODULE_LICENSE("GPL");
