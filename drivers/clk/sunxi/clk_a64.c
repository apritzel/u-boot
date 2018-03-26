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

#define OSC_FREQ 24000000
#define OSC_FREQ_ULL 24000000ULL

struct a64_clk_priv {
	void *base;
};

static unsigned int get_pll_rate(void *base)
{
	uint32_t reg = readl(base);
	int n, k, m, p;

	if (!(reg & BIT(31)))
		return 0;

	n = ((reg >> 8) & 0x1f) + 1;
	k = ((reg >> 4) & 0x03) + 1;
	m = ((reg >> 0) & 0x03) + 1;
	p = ((reg >> 16) & 0x3) + 1;

	return OSC_FREQ_ULL * n * k / (m * p);
}


static unsigned int get_apb2_rate(void *base)
{
	uint32_t reg = readl(base + 0x58), ret;
	unsigned int n, m;

	switch ((reg >> 24) & 0x3) {
	case 0:
		ret = 32768;
		break;
	case 1:
		ret = OSC_FREQ;
		break;
	case 2:
	case 3:
		ret = get_pll_rate(base + 0x28);	/* PLL_PERIPH0 */
		break;
	}
	n = 1U << ((reg >> 16) & 0x3);
	m = ((reg >> 0) & 0x1f) + 1;

	return ret / (n * m);
}

static ulong a64_clk_get_rate(struct clk *clk)
{
	struct a64_clk_priv *priv = dev_get_priv(clk->dev);

	debug("%s(#%ld)\n", __func__, clk->id);
	switch(clk->id) {
	case CLK_BUS_UART0:
	case CLK_BUS_UART1:
	case CLK_BUS_UART2:
	case CLK_BUS_UART3:
	case CLK_BUS_UART4:
		return get_apb2_rate(priv->base);
	default:
		debug("  unhandled\n");
		return -EINVAL;
	}
}

static ulong a64_clk_set_rate(struct clk *clk, ulong rate)
{
	struct a64_clk_priv *priv = dev_get_priv(clk->dev);
	void *clk_base;

	debug("%s(#%ld, rate: %lu)\n", __func__, clk->id, rate);
	switch(clk->id) {
	case CLK_MMC0:
	case CLK_MMC1:
	case CLK_MMC2:
		clk_base = priv->base + 0x88 + (clk->id - CLK_MMC0) * 4;
		return sunxi_mmc_set_mod_clk(clk_base, rate, false);
	default:
		debug("  unhandled\n");
		return -ENODEV;
	}
}

static int a64_clk_enable(struct clk *clk)
{
	struct a64_clk_priv *priv = dev_get_priv(clk->dev);
	void *clk_base;

	debug("%s(#%ld)\n", __func__, clk->id);
	switch(clk->id) {
	case CLK_BUS_UART0:
	case CLK_BUS_UART1:
	case CLK_BUS_UART2:
	case CLK_BUS_UART3:
	case CLK_BUS_UART4:
		setbits_le32(priv->base + 0x6c,
			     BIT(16 + (clk->id - CLK_BUS_UART0)));
		return 0;
	case CLK_BUS_MMC0:
	case CLK_BUS_MMC1:
	case CLK_BUS_MMC2:
		setbits_le32(priv->base + 0x60,
			     BIT(8 + (clk->id - CLK_BUS_MMC0)));
		return 0;
	case CLK_MMC0:
	case CLK_MMC1:
	case CLK_MMC2:
		clk_base = priv->base + 0x88 + (clk->id - CLK_MMC0) * 4;
		setbits_le32(clk_base, BIT(31));
		return 0;
	default:
		debug("  unhandled\n");
		return -ENODEV;
	}
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
