/*
 * prep Port to arch/powerpc:
 * Copyright 2007 David Gibson, IBM Corporation.
 *
 * prep Port to qemu:
 * Copyright 2007 Milton Miller, IBM Corporation.
 *
 * Based on OpenHackware 0.4
 * Copyright (c) 2004-2005 Jocelyn Mayer
 *
 * pci config based on arch/powerpc/platforms/chrp/pci.c GoldenGate code
 *
 */

#include <linux/init.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/udbg.h>

static volatile void __iomem *qemu_config_addr(struct pci_bus *bus,
	unsigned int devfn, int off)
{
	int dev, fn;
	struct pci_controller *hose = bus->sysdata;

	if (!hose->cfg_data)
		return NULL;

	if (bus->number != 0)
		return NULL;

	dev = devfn >> 3;
	fn = devfn & 7;

	if (dev < 11 || dev > 21)
		return NULL;

	return hose->cfg_data + ((1 << dev) | (fn << 8) | off);
}

int qemu_read_config(struct pci_bus *bus, unsigned int devfn, int off,
			   int len, u32 *val)
{
	volatile void __iomem *cfg_data = qemu_config_addr(bus, devfn, off);

	if (cfg_data == NULL)
		return PCIBIOS_DEVICE_NOT_FOUND;

	/*
	 * Note: the caller has already checked that off is
	 * suitably aligned and that len is 1, 2 or 4.
	 */
	switch (len) {
	case 1:
		*val =  in_8(cfg_data);
		break;
	case 2:
		*val = in_le16(cfg_data);
		break;
	default:
		*val = in_le32(cfg_data);
		break;
	}
	return PCIBIOS_SUCCESSFUL;
}

int qemu_write_config(struct pci_bus *bus, unsigned int devfn, int off,
			    int len, u32 val)
{
	volatile void __iomem *cfg_data = qemu_config_addr(bus, devfn, off);

	if (cfg_data == NULL)
		return PCIBIOS_DEVICE_NOT_FOUND;

	/*
	 * Note: the caller has already checked that off is
	 * suitably aligned and that len is 1, 2 or 4.
	 */
	switch (len) {
	case 1:
		out_8(cfg_data, val);
		break;
	case 2:
		out_le16(cfg_data, val);
		break;
	default:
		out_le32(cfg_data, val);
		break;
	}
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops qemu_pci_ops =
{
	qemu_read_config,
	qemu_write_config
};

void __init qemu_find_bridges(void)
{
	struct device_node *phb;
	struct pci_controller *hose;

	phb = of_find_node_by_type(NULL, "pci");
	if (!phb) {
		printk(KERN_ERR "PReP: Cannot find PCI bridge OF node\n");
		return;
	}

	hose = pcibios_alloc_controller(phb);
	if (!hose)
		return;

	pci_process_bridge_OF_ranges(hose, phb, 1);

#define PREP_PCI_DRAM_OFFSET 	0x80000000

	pci_dram_offset = PREP_PCI_DRAM_OFFSET;
	ISA_DMA_THRESHOLD = 0x00ffffff;
	DMA_MODE_READ = 0x44;
	DMA_MODE_WRITE = 0x48;

	hose->cfg_data = ioremap(0x80800000, 1 << 22);

	hose->ops = &qemu_pci_ops;

	udbg_init_uart(hose->io_base_virt + 0x3f8, 0, 0);
	register_early_udbg_console();
	printk(KERN_INFO "qemu_find_bridges: config at %p\n", hose->cfg_data);
}

