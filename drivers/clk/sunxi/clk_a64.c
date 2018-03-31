/*
 * (C) Copyright 2018 Arm Ltd.
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <dt-bindings/clock/sun50i-a64-ccu.h>

struct a64_clk_priv {
	void *base;
};

static ulong a64_clk_get_rate(struct clk *clk)
{
	debug("%s(#%ld)\n", __FUNCTION__, clk->id);

	debug("  unhandled\n");
	return -EINVAL;
}

static ulong a64_clk_set_rate(struct clk *clk, ulong rate)
{
	debug("%s(#%ld, rate: %lu)\n", __FUNCTION__, clk->id, rate);

	debug("  unhandled\n");
	return -EINVAL;
}

static int a64_clk_enable(struct clk *clk)
{
	debug("%s(#%ld)\n", __FUNCTION__, clk->id);

	debug("  unhandled\n");
	return -EINVAL;
}

static struct clk_ops a64_clk_ops = {
	.get_rate = a64_clk_get_rate,
	.set_rate = a64_clk_set_rate,
	.enable = a64_clk_enable,
};

static int a64_clk_probe(struct udevice *dev)
{
	return 0;
}

static int a64_clk_ofdata_to_platdata(struct udevice *dev)
{
	struct a64_clk_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static const struct udevice_id a64_clk_ids[] = {
        { .compatible = "allwinner,sun50i-a64-ccu" },
        { }
};

U_BOOT_DRIVER(clk_sun50i_a64) = {
        .name           = "sun50i-a64-ccu",
        .id             = UCLASS_CLK,
        .of_match       = a64_clk_ids,
        .priv_auto_alloc_size = sizeof(struct a64_clk_priv),
        .ofdata_to_platdata = a64_clk_ofdata_to_platdata,
        .ops            = &a64_clk_ops,
        .probe          = a64_clk_probe,
};
