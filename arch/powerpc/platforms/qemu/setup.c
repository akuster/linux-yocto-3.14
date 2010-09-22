/*
 * Copyright (C) 1995  Linus Torvalds
 * Adapted from 'alpha' version by Gary Thomas
 * Modified by Cort Dougan (cort@cs.nmt.edu)
 *
 * Support for PReP (Motorola MTX/MVME)
 * by Troy Benjegerdes (hozer@drgw.net)
 *
 * Port to arch/powerpc:
 * Copyright 2007 David Gibson, IBM Corporation.
 *
 * Port to qemu:
 * Copyright 2007 Milton Miller, IBM Corporation.
 *
 * Some information based on OpenHackware 0.4
 * Copyright (c) 2004-2005 Jocelyn Mayer
 *
 */

#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/initrd.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
/* #include <asm/mpic.h> */
#include <asm/i8259.h>
#include <asm/time.h>
#include <asm/udbg.h>

static const char *qemu_model = "(unknown)";

extern void qemu_find_bridges(void);

/* cpuinfo code common to all IBM PReP */
static void qemu_ibm_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "machine\t\t: PReP %s\n", qemu_model);
}

#define NVRAM_AS0 0x74
#define NVRAM_AS1 0x75
#define NVRAM_DAT 0x77

static unsigned char qemu_nvram_read_val(int addr)
{
	outb(NVRAM_AS0, addr & 0xff);
	outb(NVRAM_AS1, (addr >> 8) & 0xff);
	return inb(NVRAM_DAT);
}


static void qemu_nvram_write_val(int addr, unsigned char val)
{
	outb(NVRAM_AS0, addr & 0xff);
	outb(NVRAM_AS1, (addr >> 8) & 0xff);
	outb(NVRAM_DAT, val);
}


static void __init qemu_setup_arch(void)
{
	struct device_node *root;
	const char *model;

	root = of_find_node_by_path("/");
	model = of_get_property(root, "model", NULL);
	of_node_put(root);
	if (model)
		qemu_model = model;

	/* Lookup PCI host bridges */
	qemu_find_bridges();

	/* Read in NVRAM data */
/* 	init_qemu_nvram(); */
}

static void __init qemu_init_IRQ(void)
{
	struct device_node *pic = NULL;
	unsigned long int_ack = 0;

	pic = of_find_node_by_type(NULL, "i8259");
	if (!pic) {
		printk(KERN_ERR "No interrupt controller found!\n");
		return;
	}

	/* polling */
	i8259_init(pic, int_ack);
	ppc_md.get_irq = i8259_irq;

	/* set default host */
	irq_set_default_host(i8259_get_host());
}

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
/*
 * IDE stuff.
 */
static int qemu_ide_default_irq(unsigned long base)
{
	switch (base) {
	case 0x1f0: return 13;
	case 0x170: return 13;
	case 0x1e8: return 11;
	case 0x168: return 10;
	case 0xfff0: return 14;		/* MCP(N)750 ide0 */
	case 0xffe0: return 15;		/* MCP(N)750 ide1 */
	default: return 0;
	}
}

static unsigned long qemu_ide_default_io_base(int index)
{
	switch (index) {
	case 0: return 0x1f0;
	case 1: return 0x170;
	case 2: return 0x1e8;
	case 3: return 0x168;
	default:
		return 0;
	}
}
#endif

#if 0
static int __init prep_request_io(void)
{
#ifdef CONFIG_NVRAM
	request_region(PREP_NVRAM_AS0, 0x8, "nvram");
#endif
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");

	return 0;
}
device_initcall(prep_request_io);
#endif


static int __init qemu_probe(void)
{
	if (!of_flat_dt_is_compatible(of_get_flat_dt_root(), "qemu-prep"))
		return 0;

#if 0
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
	ppc_ide_md.default_irq = qemu_ide_default_irq;
	ppc_ide_md.default_io_base = qemu_ide_default_io_base;
#endif
#endif

	return 1;
}

void qemu_restart(char *cmd)
{
	local_irq_disable(); /* no interrupts */
	/* set exception prefix high - to the prom */
	mtmsr(mfmsr() | MSR_IP);

	/* make sure bit 0 (reset) is a 0 */
	outb(inb(0x92) & ~1L, 0x92);
	/* signal a reset to system control port A - soft reset */
	outb(inb(0x92) | 1, 0x92);

	while(1);

	/* not reached */
}

define_machine(qemu) {
	.name			= "QEMU",
	.probe			= qemu_probe,
	.setup_arch		= qemu_setup_arch,
	.progress		= udbg_progress,
	.show_cpuinfo		= qemu_ibm_cpuinfo,
	.init_IRQ		= qemu_init_IRQ,
/* 	.pcibios_fixup		= qemu_pcibios_fixup, */
	.restart		= qemu_restart,
/*	.power_off		= qemu_halt, */
/*	.halt			= qemu_halt, */
/* 	.time_init		= todc_time_init, */
/* 	.set_rtc_time		= todc_set_rtc_time, */
/* 	.get_rtc_time		= todc_get_rtc_time, */
	.calibrate_decr		= generic_calibrate_decr,
	.nvram_read_val		= qemu_nvram_read_val,
	.nvram_write_val	= qemu_nvram_write_val,
	.phys_mem_access_prot	= pci_phys_mem_access_prot,
};
