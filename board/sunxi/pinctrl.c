// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Low level GPIO/pin controller access functions, to be shared by non-DM
 * SPL code and the DM pinctrl/GPIO drivers.
 */

#include <common.h>
#include <asm/io.h>
#include <sunxi_gpio.h>

void sunxi_gpio_set_cfgbank(void *bank_base, int pin_offset, u32 val)
{
	u32 index = GPIO_CFG_INDEX(pin_offset);
	u32 offset = GPIO_CFG_OFFSET(pin_offset);

	clrsetbits_le32(bank_base + GPIO_CFG_REG_OFFSET + index * 4,
			0xfU << offset, val << offset);
}

void sunxi_gpio_set_cfgpin(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	sunxi_gpio_set_cfgbank(pio, pin % 32, val);
}

int sunxi_gpio_get_cfgbank(void *bank_base, int pin_offset)
{
	u32 index = GPIO_CFG_INDEX(pin_offset);
	u32 offset = GPIO_CFG_OFFSET(pin_offset);
	u32 cfg;

	cfg = readl(bank_base + GPIO_CFG_REG_OFFSET + index * 4);
	cfg >>= offset;

	return cfg & 0xf;
}

int sunxi_gpio_get_cfgpin(u32 pin)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);

	return sunxi_gpio_get_cfgbank(bank_base, pin % 32);
}

void sunxi_gpio_set_output_bank(void *bank_base, int pin, bool set)
{
	u32 mask = 1U << GPIO_NUM(pin);

	clrsetbits_le32(bank_base + GPIO_DAT_REG_OFFSET,
			set ? 0 : mask, set ? mask : 0);
}

void sunxi_gpio_set_output(u32 pin, bool set)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	sunxi_gpio_set_output_bank(pio, pin, set);
}

int sunxi_gpio_get_output_bank(void *bank_base, int pin)
{
	u32 mask = 1U << GPIO_NUM(pin);

	return !!(readl(bank_base + GPIO_DAT_REG_OFFSET) & mask);
}

int sunxi_gpio_get_output(u32 pin)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	return sunxi_gpio_get_output_bank(pio, pin);
}

void sunxi_gpio_set_drv(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);

	sunxi_gpio_set_drv_bank(bank_base, pin % 32, val);
}

void sunxi_gpio_set_drv_bank(void *bank_base, u32 pin_offset, u32 val)
{
	u32 index = GPIO_DRV_INDEX(pin_offset);
	u32 offset = GPIO_DRV_OFFSET(pin_offset);

	clrsetbits_le32(bank_base + GPIO_DRV_REG_OFFSET + index * 4,
			0x3U << offset, val << offset);
}

void sunxi_gpio_set_pull(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);

	sunxi_gpio_set_pull_bank(bank_base, pin % 32, val);
}

void sunxi_gpio_set_pull_bank(void *bank_base, int pin_offset, u32 val)
{
	u32 index = GPIO_PULL_INDEX(pin_offset);
	u32 offset = GPIO_PULL_OFFSET(pin_offset);

	clrsetbits_le32(bank_base + GPIO_PULL_REG_OFFSET + index * 4,
			0x3U << offset, val << offset);
}