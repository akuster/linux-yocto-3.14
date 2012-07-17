/*
 *  Ubiquiti Networks XM (rev 1.0) board support
 *
 *  Copyright (C) 2011 René Bolldorf <xsecute@googlemail.com>
 *
 *  Derived from: mach-pb44.c
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ath9k_platform.h>

#include <asm/mach-ath79/irq.h>

#include "dev-ap9x-pci.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "machtypes.h"
#include "pci.h"

#define UBNT_XM_GPIO_LED_L1		0
#define UBNT_XM_GPIO_LED_L2		1
#define UBNT_XM_GPIO_LED_L3		11
#define UBNT_XM_GPIO_LED_L4		7

#define UBNT_XM_GPIO_BTN_RESET		12

#define UBNT_XM_KEYS_POLL_INTERVAL	20
#define UBNT_XM_KEYS_DEBOUNCE_INTERVAL	(3 * UBNT_XM_KEYS_POLL_INTERVAL)

#define UBNT_XM_EEPROM_ADDR		0x1fff1000

static struct gpio_led ubnt_xm_leds_gpio[] __initdata = {
	{
		.name		= "ubnt-xm:red:link1",
		.gpio		= UBNT_XM_GPIO_LED_L1,
		.active_low	= 0,
	}, {
		.name		= "ubnt-xm:orange:link2",
		.gpio		= UBNT_XM_GPIO_LED_L2,
		.active_low	= 0,
	}, {
		.name		= "ubnt-xm:green:link3",
		.gpio		= UBNT_XM_GPIO_LED_L3,
		.active_low	= 0,
	}, {
		.name		= "ubnt-xm:green:link4",
		.gpio		= UBNT_XM_GPIO_LED_L4,
		.active_low	= 0,
	},
};

static struct gpio_keys_button ubnt_xm_gpio_keys[] __initdata = {
	{
		.desc			= "reset",
		.type			= EV_KEY,
		.code			= KEY_RESTART,
		.debounce_interval	= UBNT_XM_KEYS_DEBOUNCE_INTERVAL,
		.gpio			= UBNT_XM_GPIO_BTN_RESET,
		.active_low		= 1,
	}
};

static void __init ubnt_xm_init(void)
{
	u8 *eeprom = (u8 *) KSEG1ADDR(UBNT_XM_EEPROM_ADDR);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(ubnt_xm_leds_gpio),
				 ubnt_xm_leds_gpio);

	ath79_register_gpio_keys_polled(-1, UBNT_XM_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(ubnt_xm_gpio_keys),
					ubnt_xm_gpio_keys);

	ath79_register_m25p80(NULL);
	ap91_pci_init(eeprom, NULL);
}

MIPS_MACHINE(ATH79_MACH_UBNT_XM,
	     "UBNT-XM",
	     "Ubiquiti Networks XM (rev 1.0) board",
	     ubnt_xm_init);
