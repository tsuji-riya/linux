/*
*  arch/arm/mach-celestialsemi/include/mach/cnc1800l.h
 *
 *  This file contains the hardware definitions of the cavium Celestial Platform.
 *
 *  Copyright (C) 2010 Celestial Semiconductor
 *  Copyright (C) 2011 Cavium
 *  Author: Xaodong Fan <xiaodong.fan@caviumnetworks.com>
 */

#ifndef __ASM_ARCH_CNC1800L_H
#define __ASM_ARCH_CNC1800L_H
#define PCLK_FREQ                  47250000
#define TIMER_CLK                  94500000

#define CS_PHY_RAM_BASE            0x00000000

#define PA_IO_REGS_BASE            0x80100000
#define IO_REGS_SIZE               0x500000

#define PA_DMAC_BASE               (PA_IO_REGS_BASE + 0x130000) // 0xB0230000
#define PA_UART0_BASE              (PA_IO_REGS_BASE + 0xf1000)
#define PA_UART1_BASE              (PA_IO_REGS_BASE + 0xf2000)
#define PA_TIMER0_BASE             (PA_IO_REGS_BASE + 0x170000)
#define PA_TIMER1_BASE             (PA_IO_REGS_BASE + 0x180000)
#define PA_APB_TIMER_BASE          (PA_IO_REGS_BASE + 0xe2000)
#define PA_VIC_BASE                (PA_IO_REGS_BASE + 0x40000)
#define PA_I2C_BASE                (PA_IO_REGS_BASE + 0x70000)
#define PA_I2C2_BASE               (PA_IO_REGS_BASE + 0x74000)
#define PA_FPC_BASE                (PA_IO_REGS_BASE + 0x72000)
#define PA_GPIO_BASE               (PA_IO_REGS_BASE + 0x160000)
#define PA_SCI_BASE                (PA_IO_REGS_BASE + 0xf0000)
#define PA_USB_EHCI_BASE           (PA_IO_REGS_BASE + 0x100000)
#define PA_USB_OHCI_BASE           (PA_IO_REGS_BASE + 0x110000)
#endif