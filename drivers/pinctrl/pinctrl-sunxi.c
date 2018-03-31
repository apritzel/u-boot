/*
 * Copyright (C) 2018 Andre Przywara <andre.przywara@arm.com>
 *
 * Loosely based on drivers/pinctrl/broadcom/pinctrl-bcm2835.c
 *
 * SPDX-License-Identifier:	GPL-2.0
 * https://spdx.org/licenses
 */

#include <common.h>
#include <config.h>
#include <errno.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <dm/root.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/gpio.h>

#define NO_DRIVE	~0

#define SUNXI_PIN_BANK_OFFSET(gpio, offset)	\
	((((gpio) / 32) * 0x24) + (offset))
#define SUNXI_PIN_REG_OFFSET(gpio, bits)	\
	((((gpio) % 32) / (32 / (bits))) * 4)
#define SUNXI_PIN_BIT_OFFSET(gpio, bits)	\
	(((gpio) % (32 / (bits))) * (bits))

struct sunxi_pinctrl_priv {
	void *base_addr;
};

static int sunxi_gpio_name_to_num(const char *name)
{
	int pin;

	if (name[0] != 'P')
		return -1;

	pin = (name[1] - 'A') * 32;

	if (pin >= 26 * 32)
		return -1;

	return pin + simple_strtol(&name[2], NULL, 10);
}

static void sunxi_set_mux_func(struct udevice *dev, unsigned int gpio,
			       unsigned int muxval)
{
	struct sunxi_pinctrl_priv *priv = dev_get_priv(dev);
	void *addr = priv->base_addr;
	unsigned int bit_offset;

	addr += SUNXI_PIN_BANK_OFFSET(gpio, 0x00);
	addr += SUNXI_PIN_REG_OFFSET(gpio, 4);
	bit_offset = SUNXI_PIN_BIT_OFFSET(gpio, 4);

	clrsetbits_le32(addr, 0xf << bit_offset, muxval << bit_offset);
}

static void sunxi_set_drive(struct udevice *dev, unsigned int gpio,
			    unsigned int driveval)
{
	struct sunxi_pinctrl_priv *priv = dev_get_priv(dev);
	void *addr = priv->base_addr;
	unsigned int bit_offset;

	addr += SUNXI_PIN_BANK_OFFSET(gpio, 0x14);
	addr += SUNXI_PIN_REG_OFFSET(gpio, 2);
	bit_offset = SUNXI_PIN_BIT_OFFSET(gpio, 2);

	clrsetbits_le32(addr, 0x3 << bit_offset, driveval << bit_offset);
}

static void sunxi_set_pull(struct udevice *dev, unsigned int gpio,
			   unsigned int pullval)
{
	struct sunxi_pinctrl_priv *priv = dev_get_priv(dev);
	void *addr = priv->base_addr;
	unsigned int bit_offset;

	addr += SUNXI_PIN_BANK_OFFSET(gpio, 0x1c);
	addr += SUNXI_PIN_REG_OFFSET(gpio, 2);
	bit_offset = SUNXI_PIN_BIT_OFFSET(gpio, 2);

	clrsetbits_le32(addr, 0x3 << bit_offset, pullval << bit_offset);
}

/**
 * sunxi_pinctrl_set_state: configure pin functions
 * @dev: the pinctrl device to be configured.
 * @config: the state to be configured.
 * @return: 0 if successful
 */
int sunxi_pinctrl_set_state(struct udevice *dev, struct udevice *config)
{
	int function, i, pin_count;
	unsigned int pull = 0, drive;

	debug("sunxi_pinctrl_set_state(%s, %s);\n", dev->name, config->name);

	pin_count = dev_read_string_count(config, "pins");
	if (!pin_count)
		return 0;

	function = dev_read_u32_default(config, "pinmux", -1);
	if (function < 0) {
		debug("Failed reading function for pinconfig %s (%d)\n",
		      config->name, function);
		return -EINVAL;
	}

	if (dev_read_bool(config, "bias-pull-up"))
		pull = 0x1;
	if (dev_read_bool(config, "bias-pull-down"))
		pull = 0x2;

	drive = dev_read_u32_default(config, "drive-strength", NO_DRIVE);
	if (drive != NO_DRIVE) {
		drive = (drive + 1) / 10;
		if (drive > 3)
			drive = 0x3;
	}

	debug("  %d pins (%s ...): mux: %u, drive: %d, pull: %u\n", pin_count,
	      dev_read_string(config, "pins"), function, drive, pull);

	for (i = 0; i < pin_count; i++) {
		const char *pin_name;
		int pin, ret;

		ret = dev_read_string_index(config, "pins", i, &pin_name);
		if (ret) {
			debug("%s: could not read name of pin #%d\n", __func__,
			      i);
			break;
		}
		pin = sunxi_gpio_name_to_num(pin_name);
		if (pin < 0) {
			debug("%s: pin #%d is an invalid \"%s\"\n", __func__,
			      i, pin_name);
			continue;
		}

		sunxi_set_mux_func(dev, pin, function);
		if (pull)
			sunxi_set_pull(dev, pin, pull);
		if (drive != NO_DRIVE)
			sunxi_set_drive(dev, pin, drive);
	}

	return 0;
}

static const struct udevice_id sunxi_pinctrl_id[] = {
	{.compatible = "allwinner,sun50i-a64-pinctrl"},
	{}
};

int sunxi_pinctl_probe(struct udevice *dev)
{
	struct sunxi_pinctrl_priv *priv;

	priv = dev_get_priv(dev);
	if (!priv) {
		debug("%s: Failed to get private\n", __func__);
		return -EINVAL;
	}

	priv->base_addr = dev_read_addr_ptr(dev);
	if (priv->base_addr == (void *)FDT_ADDR_T_NONE) {
		debug("%s: Failed to get base address\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct pinctrl_ops sunxi_pinctrl_ops = {
	.set_state	= sunxi_pinctrl_set_state,
};

U_BOOT_DRIVER(pinctrl_sunxi) = {
	.name		= "sunxi_pinctrl",
	.id		= UCLASS_PINCTRL,
	.of_match	= of_match_ptr(sunxi_pinctrl_id),
	.priv_auto_alloc_size = sizeof(struct sunxi_pinctrl_priv),
	.ops		= &sunxi_pinctrl_ops,
	.probe		= sunxi_pinctl_probe,
};
