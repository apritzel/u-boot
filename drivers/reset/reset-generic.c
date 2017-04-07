/*
 * Copyright (C) 2017 Amit Singh Tomar <amittomer25@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <reset-uclass.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

struct generic_reset_priv {
	void __iomem *membase;
	int max_reset;
};

#define BITS_PER_BYTE 8
static int generic_reset_toggle(struct reset_ctl *rst, bool assert)
{
	struct generic_reset_priv *priv = dev_get_priv(rst->dev);
	int reg_width = sizeof(u32);
	int bank, offset;
	u32 reg;

	if (rst->id >= priv->max_reset)
		return -EINVAL;

	bank = rst->id / (reg_width * BITS_PER_BYTE);
	offset = rst->id % (reg_width * BITS_PER_BYTE);

	reg = readl(priv->membase + (bank * reg_width));
	if (assert)
		writel(reg & ~BIT(offset), priv->membase + (bank * reg_width));
	else
		writel(reg | BIT(offset), priv->membase + (bank * reg_width));

	return 0;
}

static int generic_reset_assert(struct reset_ctl *rst)
{
	return generic_reset_toggle(rst, true);
}

static int generic_reset_deassert(struct reset_ctl *rst)
{
	return generic_reset_toggle(rst, false);
}

static int generic_reset_free(struct reset_ctl *rst)
{
	return 0;
}

static int generic_reset_request(struct reset_ctl *rst)
{
	struct generic_reset_priv *priv = dev_get_priv(rst->dev);

	if (rst->id >= priv->max_reset)
		return -EINVAL;

	return generic_reset_assert(rst);
}

struct reset_ops generic_reset_reset_ops = {
	.free = generic_reset_free,
	.request = generic_reset_request,
	.rst_assert = generic_reset_assert,
	.rst_deassert = generic_reset_deassert,
};

static const struct udevice_id generic_reset_ids[] = {
	{ .compatible = "generic-reset" },
	{ .compatible = "nexell,s5p6818-reset" },
	{ }
};

static int generic_reset_probe(struct udevice *dev)
{
	struct generic_reset_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;
	fdt_size_t size;

	addr = devfdt_get_addr_size_index(dev, 0, &size);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->max_reset = dev_read_u32_default(dev, "num-resets", -1);
	if (priv->max_reset == -1)
		priv->max_reset = size * BITS_PER_BYTE;

	priv->membase = devm_ioremap(dev, addr, size);
	if (!priv->membase)
		return -EFAULT;

	return 0;
}

U_BOOT_DRIVER(generic_reset) = {
	.name = "generic_reset",
	.id = UCLASS_RESET,
	.of_match = generic_reset_ids,
	.ops = &generic_reset_reset_ops,
	.probe = generic_reset_probe,
	.priv_auto_alloc_size = sizeof(struct generic_reset_priv),
};
