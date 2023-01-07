// SPDX-License-Identifier: (GPL-2.0+)
/*
 * suniv DRAM initialization
 *
 * Copyright (C) 2018 Icenowy Zheng <icenowy@aosc.io>
 *
 * Based on xboot's arch/arm32/mach-f1c100s/sys-dram.c, which is:
 *
 * Copyright(c) 2007-2018 Jianjun Jiang <8192542@qq.com>
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/dram.h>
#include <asm/arch/gpio.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <hang.h>

enum dram_type {
	DRAM_TYPE_SDR	= 0,
	DRAM_TYPE_DDR	= 1,
	/* Mobile DDR, older version of LPDDR. Not supported yet. */
	DRAM_TYPE_MDDR	= 2,
};

struct dram_para {
	u32 size;		/* DRAM size (unit: MByte) */
	u32 clk;		/* DRAM work clock (unit: MHz) */
	enum dram_type type;
	u8 access_mode;		/* 0: interleave mode 1: sequence mode */
	u8 cs_num;		/* DRAM chip count  1: one chip  2: two chips */
	u8 ddr8_remap;		/* for 8bits data width DDR 0: normal  1: 8bits */
	u8 bwidth;		/* DRAM bus width */
	u8 col_width;		/* column address width */
	u8 row_width;		/* row address width */
	u8 bank_size;		/* DRAM bank count */
	u8 cas;			/* DRAM CAS */
};

static bool set_bit_and_wait(unsigned long addr, int bit)
{
	unsigned int tries = 0x10000;

	setbits_le32(addr, BIT(bit));
	while ((readl(addr) & BIT(bit)) && --tries)
		;

	return tries > 0;
}

static unsigned int ns_to_t(unsigned int ns_delay)
{
	return DIV_ROUND_UP(ns_delay * CONFIG_DRAM_CLK, 1000);
}

/*
 * Calculate the length of the refresh interval, in clock ticks.
 * According to the JEDEC spec, for 256Mbit and bigger arrays, it's 7.8 us,
 * for smaller ones 15.6 us.
 */
static void dram_set_refresh_cycle(u32 clk)
{
	u32 row_width = (readl(SUNXI_DRAMC_BASE + DRAM_SCONR) & 0x1e0) >> 5;

	if (row_width == 12)
		writel(ns_to_t(7800), SUNXI_DRAMC_BASE + DRAM_SREFR);
	else if (row_width == 11)
		writel(ns_to_t(15600), SUNXI_DRAMC_BASE + DRAM_SREFR);
}

static int dram_para_setup(struct dram_para *para)
{
	u32 val = 0;

	val = (para->ddr8_remap 		<< 0)	|
	      BIT(1)					|
	      ((para->bank_size >> 2) 		<< 3)	|
	      ((para->cs_num >> 1) 		<< 4)	|
	      ((para->row_width - 1)		<< 5)	|
	      ((para->col_width - 1)		<< 9)	|
	      ((para->type == DRAM_TYPE_DDR ?
			(para->bwidth >> 4) :
			(para->bwidth >> 5))	<< 13)	|
	      (para->access_mode		<< 15)	|
	      (para->type 			<< 16);

	writel(val, SUNXI_DRAMC_BASE + DRAM_SCONR);

	setbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR, BIT(19));
	return set_bit_and_wait(SUNXI_DRAMC_BASE + DRAM_SCTLR, 0);
}

static u32 dram_check_delay(u32 bwidth)
{
	int i;
	u32 num = 0;
	u32 dflag;

	/* For each 8-bit lane */
	for (i = 0; i < bwidth / BITS_PER_BYTE; i++) {
		dflag = readl(SUNXI_DRAMC_BASE + DRAM_DRPTR0 + i * 4);
		num += hweight32(dflag);
	}

	return num;
}

static int sdr_readpipe_scan(void)
{
	u32 k = 0;

	for (k = 0; k < 32; k++)
		writel(k, CFG_SYS_SDRAM_BASE + 4 * k);
	for (k = 0; k < 32; k++) {
		if (readl(CFG_SYS_SDRAM_BASE + 4 * k) != k)
			return 0;
	}
	return 1;
}

static u32 sdr_readpipe_select(void)
{
	u32 value = 0;
	u32 i = 0;

	for (i = 0; i < 8; i++) {
		clrsetbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR,
				0x7 << 6, i << 6);
		if (sdr_readpipe_scan()) {
			value = i;
			return value;
		}
	}
	return value;
}

static void dram_set_type(struct dram_para *para)
{
	u32 invalids = 0;
	int i;

	for (i = 0; i < 8; i++) {
		clrsetbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR,
				0x7 << 6, i << 6);
		set_bit_and_wait(SUNXI_DRAMC_BASE + DRAM_DDLYR, 0);
		if (readl(SUNXI_DRAMC_BASE + DRAM_DDLYR) & 0x30)
			invalids++;
	}

	if (invalids == 8)
		para->type = DRAM_TYPE_SDR;
	else
		para->type = DRAM_TYPE_DDR;
}

static unsigned int ddr_readpipe_select(struct dram_para *para)
{
	unsigned int rp_best = 0;
	u32 rp_val = 0, delay;
	int i;

	for (i = 0; i < 8; i++) {
		clrsetbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR,
				0x7 << 6, i << 6);
		set_bit_and_wait(SUNXI_DRAMC_BASE + DRAM_DDLYR, 0);
		if (readl(SUNXI_DRAMC_BASE + DRAM_DDLYR) & 0x30)
			continue;

		delay = dram_check_delay(para->bwidth);
		if (delay >= rp_val) {
			rp_val = delay;
			rp_best = i;
		}
	}

	return rp_best;
}

static void dram_scan_readpipe(struct dram_para *para)
{
	unsigned int rp_best;

	if (para->type == DRAM_TYPE_DDR) {
		rp_best = ddr_readpipe_select(para);
		clrsetbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR,
				0x7 << 6, rp_best << 6);
		set_bit_and_wait(SUNXI_DRAMC_BASE + DRAM_DDLYR, 0);
	} else {
		clrbits_le32(SUNXI_DRAMC_BASE + DRAM_SCONR,
			     (0x1 << 16) | (0x3 << 13));
		rp_best = sdr_readpipe_select();
		clrsetbits_le32(SUNXI_DRAMC_BASE + DRAM_SCTLR,
				0x7 << 6, rp_best << 6);
	}
}

static u32 dram_get_dram_size(struct dram_para *para)
{
	u32 col_width = 10, row_width = 13;
	u32 val1 = 0;
	u32 count = 0;
	u32 addr1, addr2;
	int i;

	para->col_width = col_width;
	para->row_width = row_width;
	dram_para_setup(para);
	dram_scan_readpipe(para);
	for (i = 0; i < 32; i++) {
		*((u8 *)(CFG_SYS_SDRAM_BASE + 0x200 + i)) = 0x11;
		*((u8 *)(CFG_SYS_SDRAM_BASE + 0x600 + i)) = 0x22;
	}
	for (i = 0; i < 32; i++) {
		val1 = *((u8 *)(CFG_SYS_SDRAM_BASE + 0x200 + i));
		if (val1 == 0x22)
			count++;
	}
	if (count == 32)
		col_width = 9;
	else
		col_width = 10;
	count = 0;
	para->col_width = col_width;
	para->row_width = row_width;
	dram_para_setup(para);
	if (col_width == 10) {
		addr1 = CFG_SYS_SDRAM_BASE + 0x400000;
		addr2 = CFG_SYS_SDRAM_BASE + 0xc00000;
	} else {
		addr1 = CFG_SYS_SDRAM_BASE + 0x200000;
		addr2 = CFG_SYS_SDRAM_BASE + 0x600000;
	}
	for (i = 0; i < 32; i++) {
		*((u8 *)(addr1 + i)) = 0x33;
		*((u8 *)(addr2 + i)) = 0x44;
	}
	for (i = 0; i < 32; i++) {
		val1 = *((u8 *)(addr1 + i));
		if (val1 == 0x44)
			count++;
	}
	if (count == 32)
		row_width = 12;
	else
		row_width = 13;
	para->col_width = col_width;
	para->row_width = row_width;
	if (para->row_width != 13)
		para->size = 16;
	else if (para->col_width == 10)
		para->size = 64;
	else
		para->size = 32;

	dram_set_refresh_cycle(CONFIG_DRAM_CLK);
	para->access_mode = 0;
	dram_para_setup(para);

	return 0;
}

static void simple_dram_check(void)
{
	volatile u32 *dram = (u32 *)CFG_SYS_SDRAM_BASE;
	int i;

	for (i = 0; i < 0x40; i++)
		dram[i] = i;

	for (i = 0; i < 0x40; i++) {
		if (dram[i] != i) {
			printf("initialization failed: DRAM[0x%x] != 0x%x.",
			       i, dram[i]);
			hang();
		}
	}

	for (i = 0; i < 0x10000; i += 0x40)
		dram[i] = i;

	for (i = 0; i < 0x10000; i += 0x40) {
		if (dram[i] != i) {
			printf("initialization failed: DRAM[0x%x] != 0x%x.",
			       i, dram[i]);
			hang();
		}
	}
}

/*
 * The timing values below are from the JEDS79C (DDR SDRAM spec) document,
 * for DDR-333 and DDR-400, respectively. Some parameters are not mentioned,
 * but some Mobile DDR SDRAM datasheet mentions additionally:
 * tXP:Exit Power Down to next valid Command= tIS + 1tCK (+2tCK for DDR-400)
 * tIS=1.1ns for DDR-333, 0.9ns for DDR-400
 * tXSR:Self Refresh Exit to next valid Command Delay: 120ns
 */
static void mctl_set_timing_params(unsigned int clkrate)
{
	u8 cas		= 2;		// CAS latency aka CL=2?
	u8 ras;
	u8 rcd;
	u8 rp;
	u8 wr;
	u8 rfc;
	u16 xsr		= 249;		// JEDEC XSRD: 200 tCK
	u8 rc;
	u16 init	= 8;
	u8 init_ref	= 7;
	u8 wtr;
	u8 rrd;
	u8 xp		= 0;		// JEDEC tXP: 1.1ns+1 tCK => 1.17
	u32 val;

	if (clkrate < 168) {		/* DDR-333 JEDEC timings */
		ras	= ns_to_t(42);
		rcd	= ns_to_t(18);
		rp	= ns_to_t(18);
		wr	= ns_to_t(15);
		rfc	= ns_to_t(72);
		rc	= ns_to_t(60);
		wtr	= 1;
		rrd	= ns_to_t(12);
	} else if (clkrate == 168) {	/* fixed DDR-333 JEDEC timings */
		/*
		 * At exactly 166.666.. MHz, the above DDR-333 JEDEC timings
		 * would result in those values below. The clock controller
		 * cannot run at this frequency exactly, the closest is
		 * 168 MHz. Most of the calculated timing values cross an
		 * integer boundary at exactly 166.666... MHz, so due to the
		 * round-up algorithm, the calculated values would result in
		 * a timing delay of one *extra* cycle:
		 * 	42 ns @ 166.66MHz = 6.99972 cycles => 7 cycles
		 * 	42 ns @ 166.67Mhz = 7.00014 cycles => 8 cycles
		 * 168 MHz is just 0.8% higher than the nominal frequency,
		 * so force those fixed timings in, for optimal performance.
		 */
		ras	= 7;	// 42 ns @ 166.6 MHz
		rcd	= 3;	// 18 ns @ 166.6 MHz
		rp	= 3;	// 18 ns @ 166.6 MHz
		wr	= 3;	// 15 ns @ 166.6 MHz
		rfc	= 12;	// 72 ns @ 166.6 MHz
		rc	= 10;	// 60 ns @ 166.6 MHz
		wtr	= 1;
		rrd	= 2;	// 12 ns @ 166.6 MHz
	} else if (clkrate == 204) {	/* fixed DDR-400 JEDEC timings */
		/*
		 * Same story as above, use the cycle values calculated at
		 * 200 MHz, when running at 204 MHz, which is the closest
		 * we can get with the DRAM PLL.
		 */
		ras	= 8;	// 40 ns @ 200 MHz
		rcd	= 3;	// 15 ns @ 200 MHz
		rp	= 3;	// 15 ns @ 200 MHz
		wr	= 3;	// 15 ns @ 200 MHz
		rfc	= 14;	// 70 ns @ 200 MHz
		rc	= 11;	// 55 ns @ 200 MHz
		wtr	= 2;
		rrd	= 2;	// 10 ns @ 200 MHz
	} else {			/* DDR-400 JEDEC timings */
		ras	= ns_to_t(40);
		rcd	= ns_to_t(15);
		rp	= ns_to_t(15);
		wr	= ns_to_t(15);
		rfc	= ns_to_t(70);
		rc	= ns_to_t(55);
		wtr	= 2;
		rrd	= ns_to_t(10);
	}

	val = (cas << 0) | (ras << 3) | (rcd << 7) | (rp << 10) |
	      (wr << 13) | (rfc << 15) | (xsr << 19) | (rc << 28);
	writel(val, SUNXI_DRAMC_BASE + DRAM_STMG0R);
	val = (init << 0) | (init_ref << 16) | (wtr << 20) |
	      (rrd << 22) | (xp << 25);
	writel(val, SUNXI_DRAMC_BASE + DRAM_STMG1R);
}

#define PIO_SDR_PAD_DRV	0x2c0
#define PIO_SDR_PAD_PULL	0x2c4

static void do_dram_init(struct dram_para *para)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	u32 val;
	u8 m; /* PLL_DDR clock factor */

	/* Make sure DDR_REF_D multiplex is not selected. */
	sunxi_gpio_set_cfgpin(SUNXI_GPB(3), 0x7);
	mdelay(5);

	/* Enable and set internal reference configuration factor to 1/2 VDDQ */
	if (para->cas & BIT(3))
		setbits_le32(SUNXI_PIO_BASE + PIO_SDR_PAD_PULL,
			     BIT(23) | (0x20 << 17));

	/* Set drive level of all DRAM pins (except ODT) */
	if (para->clk >= 144 && para->clk < 180)
		writel(0xaaa, SUNXI_PIO_BASE + PIO_SDR_PAD_DRV); /* level 2 */
	if (para->clk >= 180)
		writel(0xfff, SUNXI_PIO_BASE + PIO_SDR_PAD_DRV); /* level 3 */

	if (para->cas & BIT(4))
		writel(0xd1303333, &ccm->pll5_pattern_cfg);
	else if (para->cas & BIT(5))
		writel(0xcce06666, &ccm->pll5_pattern_cfg);
	else if (para->cas & BIT(6))
		writel(0xc8909999, &ccm->pll5_pattern_cfg);
	else if (para->cas & BIT(7))
		writel(0xc440cccc, &ccm->pll5_pattern_cfg);

	if (para->clk <= 96)
		m = 2;
	else
		m = 1;

	val = CCM_PLL5_CTRL_EN | CCM_PLL5_CTRL_UPD |
	      CCM_PLL5_CTRL_N((para->clk * 2) / (24 / m)) |
	      CCM_PLL5_CTRL_K(1) | CCM_PLL5_CTRL_M(m);
	if (para->cas & GENMASK(7, 4))
		val |= CCM_PLL5_CTRL_SIGMA_DELTA_EN;
	writel(val, &ccm->pll5_cfg);
	setbits_le32(&ccm->pll5_cfg, CCM_PLL5_CTRL_UPD);
	mctl_await_completion(&ccm->pll5_cfg, BIT(28), BIT(28));
//	if (wait_for_bit_le32(&ccm->pll5_cfg, BIT(28), BIT(28), 1000, false))
//		panic("PLL_DDR not locking\n");
	mdelay(5);

	setbits_le32(&ccm->ahb_gate0, (1 << AHB_GATE_OFFSET_MCTL));
	clrbits_le32(&ccm->ahb_reset0_cfg, (1 << AHB_RESET_OFFSET_MCTL));
	udelay(50);
	setbits_le32(&ccm->ahb_reset0_cfg, (1 << AHB_RESET_OFFSET_MCTL));

	clrsetbits_le32(SUNXI_PIO_BASE + 0x2c4, (1 << 16),
			para->type == DRAM_TYPE_DDR ?  BIT(16) : 0);

	mctl_set_timing_params(para->clk);
	dram_para_setup(para);
	dram_set_type(para);

	clrsetbits_le32(SUNXI_PIO_BASE + 0x2c4, (1 << 16),
			para->type == DRAM_TYPE_DDR ? BIT(16) : 0);

	dram_set_refresh_cycle(para->clk);
	dram_scan_readpipe(para);
	dram_get_dram_size(para);
	simple_dram_check();
}

unsigned long sunxi_dram_init(void)
{
	struct dram_para para = {
		.size = 32,
		.clk = CONFIG_DRAM_CLK,
		.access_mode = 1,
		.cs_num = 1,
		.ddr8_remap = 0,
		.type = DRAM_TYPE_DDR,
		.bwidth = 16,
		.col_width = 10,
		.row_width = 13,
		.bank_size = 4,
		.cas = 0x3,
	};

	do_dram_init(&para);

	return para.size * 1024UL * 1024;
}
