/*
 * Copyright (C) 2017 Amit Singh Tomar <amittomer25@gmail.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dt-structs.h>
#include <dwmmc.h>
#include <errno.h>
#include <mapmem.h>
#include <linux/err.h>
#include <reset.h>
#include <asm/arch/clk.h>

#define SDMMCCLKENB 0xC00C5000
#define SDMMCCLKGEN0L 0xC00C5004
#define PLL_SEL_MASK GENMASK(4, 2)
#define CLK_DIV_MASK GENMASK(12, 5)
#define PLLSEL_SHIFT 0x2
#define PLL0_SEL 0
#define PLL1_SEL 1
#define PLL2_SEL 2
#define SDMMC_CLK_ENB 0xc /* Magic bit to enable/generate SDMMC clock */

DECLARE_GLOBAL_DATA_PTR;

struct nexell_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct nexell_dwmmc_priv {
	struct clk clk;
	struct dwmci_host host;
	struct reset_ctl reset_ctl;
	int fifo_depth;
	bool fifo_mode;
};

/* Should this be done from CCF ? */
static void nexell_dwmci_clksel(struct dwmci_host *host)
{
	u32 val;

	/* Enable SDMMC clock */
	val = readl(SDMMCCLKENB);
	val |= SDMMC_CLK_ENB;
	writel(val, SDMMCCLKENB);

	/* Select PLL1 as clock source */
	val = readl(SDMMCCLKGEN0L);
	val = val & ~(PLL_SEL_MASK);
	val |= (PLL1_SEL << PLLSEL_SHIFT) & PLL_SEL_MASK;
	writel(val, SDMMCCLKGEN0L);
}

static int nexell_dwmmc_ofdata_to_platdata(struct udevice *dev)
{
	struct nexell_dwmmc_priv *priv = dev_get_priv(dev);
	struct dwmci_host *host = &priv->host;
	int fifo_depth, ret;

	ret = reset_get_by_name(dev, "mmc", &priv->reset_ctl);
	if (ret) {
		printf("reset_get_by_name(rst) failed: %d", ret);
		return ret;
	}

	fifo_depth = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
				    "fifo-depth", 0);
	if (fifo_depth < 0) {
		printf("DWMMC: Can't get FIFO depth\n");
		return -EINVAL;
	}

	host->name = dev->name;
	host->ioaddr = (void *)devfdt_get_addr(dev);
	host->buswidth = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					"bus-width", 4);

	ret = reset_assert(&priv->reset_ctl);
	if (ret)
		return ret;

	host->clksel = nexell_dwmci_clksel;

	ret = reset_deassert(&priv->reset_ctl);
	if (ret)
		return ret;

	host->dev_index = 0;
	host->bus_hz = get_mmc_clk(host->dev_index);
	host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_depth / 2 - 1) |
			   TX_WMARK(fifo_depth / 2);
	host->priv = priv;

	return 0;
}

static int nexell_dwmmc_probe(struct udevice *dev)
{
#ifdef CONFIG_BLK
	struct nexell_mmc_plat *plat = dev_get_platdata(dev);
#endif
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct nexell_dwmmc_priv *priv = dev_get_priv(dev);
	struct dwmci_host *host = &priv->host;

#ifdef CONFIG_BLK
	dwmci_setup_cfg(&plat->cfg, host, host->bus_hz, 400000);
	host->mmc = &plat->mmc;
#else
	int ret;

	ret = add_dwmci(host, host->bus_hz, 400000);
	if (ret)
		return ret;
#endif

	host->mmc->priv = &priv->host;
	upriv->mmc = host->mmc;
	host->mmc->dev = dev;

	return 0;
}

static int nexell_dwmmc_bind(struct udevice *dev)
{
#ifdef CONFIG_BLK
	struct nexell_mmc_plat *plat = dev_get_platdata(dev);
	int ret;

	ret = dwmci_bind(dev, &plat->mmc, &plat->cfg);
	if (ret)
		return ret;
#endif

	return 0;
}

static const struct udevice_id nexell_dwmmc_ids[] = {
	{ .compatible = "nexell,s5p6818-dw-mshc" },
	{ }
};

U_BOOT_DRIVER(nexell_dwmmc_drv) = {
	.name		= "nexell_s5p6818_dw_mshc",
	.id		= UCLASS_MMC,
	.of_match	= nexell_dwmmc_ids,
	.ops		= &dm_dwmci_ops,
	.ofdata_to_platdata = nexell_dwmmc_ofdata_to_platdata,
	.bind		= nexell_dwmmc_bind,
	.probe		= nexell_dwmmc_probe,
	.priv_auto_alloc_size = sizeof(struct nexell_dwmmc_priv),
	.platdata_auto_alloc_size = sizeof(struct nexell_mmc_plat),
};
