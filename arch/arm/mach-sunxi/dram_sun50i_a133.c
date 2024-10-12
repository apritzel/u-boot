// SPDX-License-Identifier: GPL-2.0+
/*
 * sun50i A133 platform dram controller driver
 *
 * Controller and PHY appear to be quite similar to that of the H616;
 * however certain offsets, timings, and other details are different enough that
 * the original code does not work as expected. Some device flags and calibrations
 * are not yet implemented, and configuration aside from DDR4 have not been tested.
 *
 * (C) Copyright 2024 MasterR3C0RD <masterr3c0rd@epochal.quest>
 *
 * Uses code from H616 driver, which is
 * (C) Copyright 2020 Jernej Skrabec <jernej.skrabec@siol.net>
 *
 */

#define DEBUG

#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/dram.h>
#include <asm/arch/prcm.h>
#include <asm/io.h>
#include <init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <log.h>

/* TODO: There are separate versions depending on the first dword of the chipid */
static const u8 phy_init[] = {
#ifdef CONFIG_SUNXI_DRAM_DDR3
	0x03, 0x19, 0x18, 0x02, 0x10, 0x15, 0x16, 0x07, 0x06,
	0x0e, 0x05, 0x08, 0x0d, 0x04, 0x17, 0x1a, 0x13, 0x11,
	0x12, 0x14, 0x00, 0x01, 0x0c, 0x0a, 0x09, 0x0b, 0x0f
#elif CONFIG_SUNXI_DRAM_DDR4
	0x13, 0x17, 0x0e, 0x01, 0x06, 0x12, 0x14, 0x07, 0x09,
	0x02, 0x0f, 0x00, 0x0d, 0x05, 0x16, 0x0c, 0x0a, 0x11,
	0x04, 0x03, 0x18, 0x15, 0x08, 0x10, 0x0b, 0x19, 0x1a
#elif CONFIG_SUNXI_DRAM_LPDDR3
	0x05, 0x06, 0x17, 0x02, 0x19, 0x18, 0x04, 0x07, 0x03,
	0x01, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x15, 0x16, 0x08, 0x09, 0x00, 0x1a
#elif CONFIG_SUNXI_DRAM_LPDDR4
	0x01, 0x03, 0x02, 0x19, 0x17, 0x00, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x15, 0x16, 0x04, 0x18, 0x05, 0x1a
#endif
};

static void mctl_clk_init(u32 clk)
{
	struct sunxi_ccm_reg *ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	/* Place all DRAM blocks into reset */
	clrbits_le32(&ccm->mbus_cfg, MBUS_ENABLE);
	clrbits_le32(&ccm->mbus_cfg, MBUS_RESET);
	clrbits_le32(&ccm->dram_gate_reset, BIT(GATE_SHIFT));
	clrbits_le32(&ccm->dram_gate_reset, BIT(RESET_SHIFT));
	clrbits_le32(&ccm->pll5_cfg, CCM_PLL5_CTRL_EN);
	clrbits_le32(&ccm->dram_clk_cfg, DRAM_MOD_RESET);
	udelay(5);

	/* Set up PLL5 clock, used for DRAM */
	clrsetbits_le32(&ccm->pll5_cfg, 0xff03,
			CCM_PLL5_CTRL_N((clk * 2) / 24) | CCM_PLL5_CTRL_EN);
	setbits_le32(&ccm->pll5_cfg, BIT(24));
	clrsetbits_le32(&ccm->pll5_cfg, 0x3,
			CCM_PLL5_LOCK_EN | CCM_PLL5_CTRL_EN | BIT(30));
	clrbits_le32(&ccm->pll5_cfg, 0x3 | BIT(30));
	mctl_await_completion(&ccm->pll5_cfg, CCM_PLL5_LOCK, CCM_PLL5_LOCK);

	/* Enable DRAM clock and gate*/
	clrbits_le32(&ccm->dram_clk_cfg, BIT(24) | BIT(25));
	clrsetbits_le32(&ccm->dram_clk_cfg, 0x1f, BIT(1) | BIT(0));
	setbits_le32(&ccm->dram_clk_cfg, DRAM_CLK_UPDATE);
	setbits_le32(&ccm->dram_gate_reset, BIT(RESET_SHIFT));
	setbits_le32(&ccm->dram_gate_reset, BIT(GATE_SHIFT));

	/* Re-enable MBUS and reset the DRAM module */
	setbits_le32(&ccm->mbus_cfg, MBUS_RESET);
	setbits_le32(&ccm->mbus_cfg, MBUS_ENABLE);
	setbits_le32(&ccm->dram_clk_cfg, DRAM_MOD_RESET);
	udelay(5);
}

static void mctl_set_odtmap(const struct dram_para *para,
			    const struct dram_config *config)
{
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;

	u32 val, temp1, temp2;

	/* Set ODT/rank mappings*/
	if (config->bus_full_width)
		writel(0x0201, &mctl_ctl->odtmap);
	else
		writel(0x0303, &mctl_ctl->odtmap);

	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		val = 0x06000400;
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		/* TODO: What's the purpose of these values? */
		temp1 = para->clk * 7 / 2000;
		if (para->clk < 400)
			temp2 = 0x3;
		else
			temp2 = 0x4;

		val = 0x400 | (temp2 - temp1) << 16 | temp1 << 24;
		break;
	case SUNXI_DRAM_TYPE_DDR4:
	case SUNXI_DRAM_TYPE_LPDDR4:
		val = 0x400 | (para->mr4 << 10 & 0x70000) |
		      (((para->mr4 >> 12) & 1) + 6) << 24;

		break;
	}

	writel(val, &mctl_ctl->odtcfg);
	writel(val, &mctl_ctl->unk_0x2240); /* Documented as ODTCFG_SHADOW */
	writel(val,
	       &mctl_ctl->unk_0x3240); /* Offset's interesting; additional undocumented shadows? */
	writel(val, &mctl_ctl->unk_0x4240);
}

/*
 * Note: Unlike the H616, config->ranks is the number of rank *bits*, not the number of ranks *present*.
 * For example, if `ranks = 0`, then there is only one rank. If `ranks = 1`, there are two.
 */
static void mctl_set_addrmap(const struct dram_config *config)
{
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;

	u8 bankgrp_bits = config->bankgrps;
	u8 bank_bits = config->banks;
	u8 rank_bits = config->ranks;
	u8 col_bits = config->cols;
	u8 row_bits = config->rows;
	bool bus_full_width = config->bus_full_width;

	u8 addrmap_bank_bx = bankgrp_bits + col_bits - 2;
	u8 addrmap_row_bx = (bankgrp_bits + bank_bits + col_bits) - 6;

	if (!bus_full_width)
		col_bits -= 1;

	/* Bank groups */
	switch (bankgrp_bits) {
	case 0:
		writel(0x3f3f, &mctl_ctl->addrmap[8]);
		break;
	case 1:
		writel(0x01 | 0x3f << 8, &mctl_ctl->addrmap[8]);
		break;
	case 2:
		writel(0x01 | 0x01 << 8, &mctl_ctl->addrmap[8]);
		break;
	default:
		panic("Unsupported dram configuration (bankgrp_bits = %d)",
		      bankgrp_bits);
	}

	/* Columns */
	writel(bankgrp_bits | bankgrp_bits << 8 | bankgrp_bits << 16 |
		       bankgrp_bits << 24,
	       &mctl_ctl->addrmap[2]);

	switch (col_bits) {
	case 8:
		writel(bankgrp_bits | bankgrp_bits << 8 | 0x1f << 16 |
			       0x1f << 24,
		       &mctl_ctl->addrmap[3]);
		writel(0x1f | 0x1f << 8, &mctl_ctl->addrmap[4]);
		break;
	case 9:
		writel(bankgrp_bits | bankgrp_bits << 8 | bankgrp_bits << 16 |
			       0x1f << 24,
		       &mctl_ctl->addrmap[3]);
		writel(0x1f | 0x1f << 8, &mctl_ctl->addrmap[4]);
		break;
	case 10:
		writel(bankgrp_bits | bankgrp_bits << 8 | bankgrp_bits << 16 |
			       bankgrp_bits << 24,
		       &mctl_ctl->addrmap[3]);
		writel(0x1f | 0x1f << 8, &mctl_ctl->addrmap[4]);
		break;
	case 11:
		writel(bankgrp_bits | bankgrp_bits << 8 | bankgrp_bits << 16 |
			       bankgrp_bits << 24,
		       &mctl_ctl->addrmap[3]);
		writel(bankgrp_bits | 0x1f << 8, &mctl_ctl->addrmap[4]);
		break;
	case 12:
		writel(bankgrp_bits | bankgrp_bits << 8 | bankgrp_bits << 16 |
			       bankgrp_bits << 24,
		       &mctl_ctl->addrmap[3]);
		writel(bankgrp_bits | bankgrp_bits << 8, &mctl_ctl->addrmap[4]);
		break;
	default:
		panic("Unsupported dram configuration (col_bits = %d)",
		      col_bits);
	}

	/* Banks */
	if (bank_bits == 3) {
		writel(addrmap_bank_bx | addrmap_bank_bx << 8 |
			       addrmap_bank_bx << 16,
		       &mctl_ctl->addrmap[1]);
	} else {
		writel(addrmap_bank_bx | addrmap_bank_bx << 8 | 0x3f << 16,
		       &mctl_ctl->addrmap[1]);
	}

	/* Rows */
	writel(addrmap_row_bx | addrmap_row_bx << 8 | addrmap_row_bx << 16 |
		       addrmap_row_bx << 24,
	       &mctl_ctl->addrmap[5]);

	switch (row_bits) {
	case 14:
		writel(addrmap_row_bx | addrmap_row_bx << 8 | 0x0f << 16 |
			       0x0f << 24,
		       &mctl_ctl->addrmap[6]);
		writel(0x0f | 0x0f << 8, &mctl_ctl->addrmap[7]);
		break;
	case 15:
		if ((rank_bits == 1 && col_bits == 11) ||
		    (rank_bits == 2 && col_bits == 10)) {
			writel(addrmap_row_bx | (addrmap_row_bx + 1) << 8 |
				       (addrmap_row_bx + 1) << 16 | 0x0f << 24,
			       &mctl_ctl->addrmap[6]);
		} else {
			writel(addrmap_row_bx | addrmap_row_bx << 8 |
				       addrmap_row_bx << 16 | 0x0f << 24,
			       &mctl_ctl->addrmap[6]);
		}
		writel(0x0f | 0x0f << 8, &mctl_ctl->addrmap[7]);
		break;
	case 16:
		if (rank_bits == 1 && col_bits == 10) {
			writel((addrmap_row_bx + 1) |
				       (addrmap_row_bx + 1) << 8 |
				       (addrmap_row_bx + 1) << 16 |
				       (addrmap_row_bx + 1) << 24,
			       &mctl_ctl->addrmap[6]);
		} else {
			writel(addrmap_row_bx | addrmap_row_bx << 8 |
				       addrmap_row_bx << 16 |
				       addrmap_row_bx << 24,
			       &mctl_ctl->addrmap[6]);
		}
		writel(0x0f | 0x0f << 8, &mctl_ctl->addrmap[7]);
		break;
	case 17:
		writel(addrmap_row_bx | addrmap_row_bx << 8 |
			       addrmap_row_bx << 16 | addrmap_row_bx << 24,
		       &mctl_ctl->addrmap[6]);
		writel(addrmap_row_bx | 0x0f << 8, &mctl_ctl->addrmap[7]);
		break;
	case 18:
		writel(addrmap_row_bx | addrmap_row_bx << 8 |
			       addrmap_row_bx << 16 | addrmap_row_bx << 24,
		       &mctl_ctl->addrmap[6]);
		writel(addrmap_row_bx | addrmap_row_bx << 8,
		       &mctl_ctl->addrmap[7]);
		break;
	default:
		panic("Unsupported dram configuration (row_bits = %d)",
		      row_bits);
	}

	/* Ranks */
	if (rank_bits == 0) {
		writel(0x1f, &mctl_ctl->addrmap[0]);
	} else if ((rank_bits + col_bits + row_bits) == 27) {
		writel(addrmap_row_bx + row_bits - 2, &mctl_ctl->addrmap[0]);
	} else {
		writel(addrmap_row_bx + row_bits, &mctl_ctl->addrmap[0]);
	}
}

static void mctl_com_init(const struct dram_para *para,
			  const struct dram_config *config)
{
	struct sunxi_mctl_com_reg *mctl_com =
		(struct sunxi_mctl_com_reg *)SUNXI_DRAM_COM_BASE;
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;

	/* Might control power/reset of DDR-related blocks */
	clrsetbits_le32(&mctl_com->unk_0x008, BIT(24), BIT(25) | BIT(9));

	/* Unlock mctl_ctl registers */
	setbits_le32(&mctl_com->maer0, BIT(15));

	if (para->type == SUNXI_DRAM_TYPE_LPDDR4)
		setbits_le32(0x03102ea8, BIT(0));

	clrsetbits_le32(&mctl_ctl->sched[0], 0xff << 8, 0x30 << 8);
	writel(0, &mctl_ctl->hwlpctl);

	/* Master settings */
	u32 mstr_value = MSTR_DEVICECONFIG_X32 |
			 MSTR_ACTIVE_RANKS(config->ranks);

	if (config->bus_full_width)
		mstr_value |= MSTR_BUSWIDTH_FULL;
	else
		mstr_value |= MSTR_BUSWIDTH_HALF;

	/*
	 * Geardown and 2T mode are always enabled here, but is controlled by a flag in boot0;
	 * it has not been a problem so far, but may be suspect if a particular board isn't booting.
	 */
	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		mstr_value |= MSTR_DEVICETYPE_DDR3 | MSTR_BURST_LENGTH(8) |
			      MSTR_2TMODE;
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		mstr_value |= MSTR_DEVICETYPE_DDR4 | MSTR_BURST_LENGTH(8) |
			      MSTR_GEARDOWNMODE | MSTR_2TMODE;
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		mstr_value |= MSTR_DEVICETYPE_LPDDR3 | MSTR_BURST_LENGTH(8);
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		mstr_value |= MSTR_DEVICETYPE_LPDDR4 | MSTR_BURST_LENGTH(16);
		break;
	}

	writel(mstr_value, &mctl_ctl->mstr);

	mctl_set_odtmap(para, config);
	mctl_set_addrmap(config);
	mctl_set_timing_params(para);

	writel(0, &mctl_ctl->pwrctl);

	/* Update values */
	setbits_le32(&mctl_ctl->dfiupd[0], BIT(31) | BIT(30));
	setbits_le32(&mctl_ctl->zqctl[0], BIT(31) | BIT(30));
	setbits_le32(&mctl_ctl->unk_0x2180, BIT(31) | BIT(30));
	setbits_le32(&mctl_ctl->unk_0x3180, BIT(31) | BIT(30));
	setbits_le32(&mctl_ctl->unk_0x4180, BIT(31) | BIT(30));

	/*
	 * Data bus inversion
	 * Controlled by a flag in boot0, enabled by default here.
	 */
	if (para->type == SUNXI_DRAM_TYPE_DDR4 ||
	    para->type == SUNXI_DRAM_TYPE_LPDDR4)
		setbits_le32(&mctl_ctl->dbictl, BIT(2));
}

static void mctl_drive_odt_config(const struct dram_para *para)
{
	u32 val;
	u64 base;
	u32 i;

	/* DX drive */
	for (i = 0; i < 4; i++) {
		base = SUNXI_DRAM_PHY0_BASE + 0x388 + 0x20 * i;
		val = (para->dx_dri >> (i * 8)) & 0x1f;

		writel(val, base);
		if (para->type == SUNXI_DRAM_TYPE_LPDDR4) {
			if (para->tpr3 & 0x1f1f1f1f)
				val = (para->tpr3 >> (i * 8)) & 0x1f;
			else
				val = 4;
		}
		writel(val, base + 4);
	}

	/* CA drive */
	for (i = 0; i < 2; i++) {
		base = SUNXI_DRAM_PHY0_BASE + 0x340UL + 0x8UL * i;
		val = (para->ca_dri >> (i * 8)) & 0x1f;

		writel(val, base);
		writel(val, base + 4);
	}

	/* DX ODT */
	for (i = 0; i < 4; i++) {
		base = SUNXI_DRAM_PHY0_BASE + 0x380 + 0x40UL * i;
		val = (para->dx_odt >> (i * 8)) & 0x1f;

		if (para->type == SUNXI_DRAM_TYPE_DDR4 ||
		    para->type == SUNXI_DRAM_TYPE_LPDDR3)
			writel(0, base);
		else
			writel(val, base);

		if (para->type == SUNXI_DRAM_TYPE_LPDDR4)
			writel(0, base + 4);
		else
			writel(val, base + 4);
	}
}

static void mctl_phy_ca_bit_delay_compensation(const struct dram_para *para)
{
	u32 val, i;
	u32 *ptr;

	if (para->tpr10 & BIT(31)) {
		val = para->mr2;
	} else {
		val = ((para->tpr10 << 1) & 0x1e) |
		      ((para->tpr10 << 5) & 0x1e00) |
		      ((para->tpr10 << 9) & 0x1e0000) |
		      ((para->tpr10 << 13) & 0x1e000000);

		if (para->tpr10 & BIT(19))
			val <<= 1;
	}

	ptr = (u32 *)(SUNXI_DRAM_PHY0_BASE + 0x780);
	for (i = 0; i < 128; i++)
		writel((para->tpr2 >> 8) & 0x3f, &ptr[i]);

	writel(val & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x7dc);
	writel(val & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x7e0);

	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		writel((val >> 16) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x7b8);
		writel((val >> 24) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x784);
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		writel((val >> 16) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x784);
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		writel((val >> 16) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x788);
		writel((val >> 24) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x790);
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		writel((val >> 16) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x790);
		writel((val >> 24) & 0x3f, SUNXI_DRAM_PHY0_BASE + 0x78c);
		break;
	}
}

static void mctl_phy_init(const struct dram_para *para,
			  const struct dram_config *config)
{
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;
	const struct sunxi_prcm_reg *prcm =
		(struct sunxi_prcm_reg *)SUNXI_PRCM_BASE;
	struct sunxi_mctl_com_reg *mctl_com =
		(struct sunxi_mctl_com_reg *)SUNXI_DRAM_COM_BASE;

	u32 val, val2, i;
	u32 *ptr;

	/* Disable auto refresh. */
	setbits_le32(&mctl_ctl->rfshctl3, BIT(0));

	/* Set "phy_dbi_mode" to mark the DFI as implementing DBI functionality */
	writel(0, &mctl_ctl->pwrctl);
	clrbits_le32(&mctl_ctl->dfimisc, 1);
	writel(0x20, &mctl_ctl->pwrctl);

	/* PHY cold reset */
	clrsetbits_le32(&mctl_com->unk_0x008, BIT(24), BIT(9));
	udelay(1);
	setbits_le32(&mctl_com->unk_0x008, BIT(24));

	/* Not sure what this gates the power of. */
	clrbits_le32(&prcm->sys_pwroff_gating, BIT(4));

	if (para->type == SUNXI_DRAM_TYPE_LPDDR4)
		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x4, BIT(7));

	/* Similar enumeration of values is used during read training */
	if (config->bus_full_width)
		val = 0xf;
	else
		val = 0x3;

	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 0x3c, 0xf, val);

	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		val = 13;
		val2 = 9;
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		val = 13;
		val2 = 10;
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		val = 14;
		val2 = 8;
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		val = 22;
		val2 = 10;
		break;
	}

	writel(val, SUNXI_DRAM_PHY0_BASE + 0x14);
	writel(val, SUNXI_DRAM_PHY0_BASE + 0x35c);
	writel(val, SUNXI_DRAM_PHY0_BASE + 0x368);
	writel(val, SUNXI_DRAM_PHY0_BASE + 0x374);
	writel(0, SUNXI_DRAM_PHY0_BASE + 0x18);
	writel(0, SUNXI_DRAM_PHY0_BASE + 0x360);
	writel(0, SUNXI_DRAM_PHY0_BASE + 0x36c);
	writel(0, SUNXI_DRAM_PHY0_BASE + 0x378);
	writel(val2, SUNXI_DRAM_PHY0_BASE + 0x1c);
	writel(val2, SUNXI_DRAM_PHY0_BASE + 0x364);
	writel(val2, SUNXI_DRAM_PHY0_BASE + 0x370);
	writel(val2, SUNXI_DRAM_PHY0_BASE + 0x37c);

	/* boot0 does this in "phy_set_address_remapping". Seems odd for an address map table, though. */
	ptr = (u32 *)(SUNXI_DRAM_PHY0_BASE + 0xc0);
	for (i = 0; i < ARRAY_SIZE(phy_init); i++)
		writel(phy_init[i], &ptr[i]);

	/* Set VREF */
	val = 0;
	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		val = para->tpr6 & 0xff;
		if (val == 0)
			val = 0x80;
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		val = (para->tpr6 >> 8) & 0xff;
		if (val == 0)
			val = 0x80;
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		val = (para->tpr6 >> 16) & 0xff;
		if (val == 0)
			val = 0x80;
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		val = (para->tpr6 >> 24) & 0xff;
		if (val == 0)
			val = 0x33;
		break;
	}
	writel(val, SUNXI_DRAM_PHY0_BASE + 0x35c);
	writel(val, SUNXI_DRAM_PHY0_BASE + 0x45c);

	mctl_drive_odt_config(para);

	if (para->tpr10 & TPR10_CA_BIT_DELAY)
		mctl_phy_ca_bit_delay_compensation(para);

	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		val = 2;
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		val = 3;
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		val = 4;
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		val = 5;
		break;
	}

	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 0x4, 0x7, val | 8);

	if (para->clk <= 672)
		writel(0xf, SUNXI_DRAM_PHY0_BASE + 0x20);

	if (para->clk > 500) {
		val = 0;
		val2 = 0;
	} else {
		val = 0x80;
		val2 = 0x20;
	}

	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 0x144, 0x80, val);
	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 0x14c, 0xe0, val2);

	clrbits_le32(&mctl_com->unk_0x008, BIT(9));
	udelay(1);
	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x14c, BIT(3));

	mctl_await_completion((u32 *)(SUNXI_DRAM_PHY0_BASE + 0x180), BIT(2),
			      BIT(2));

	/* This is controlled by a tpr13 flag in boot0; doesn't hurt to always do it though. */
	udelay(1000);
	writel(0x37, SUNXI_DRAM_PHY0_BASE + 0x58);

	setbits_le32(&prcm->sys_pwroff_gating, BIT(4));
}

/* Helper macros for MR reads/writes */
#define WRITE_MR(val0, val1)                                  \
	writel((val1), &mctl_ctl->mrctrl1);                   \
	writel(MRCTRL0_MR_WR | MRCTRL0_MR_RANKS_ALL | (val0), \
	       &mctl_ctl->mrctrl0);                           \
	mctl_await_completion(&mctl_ctl->mrctrl0, BIT(31), 0);

#define WRITE_LPDDR4_MR(addr, val) \
	WRITE_MR(0, MRCTRL1_MR_ADDR(addr) | MRCTRL1_MR_DATA(val));

/* Bit [7:6] are set by boot0, but undocumented */
#define WRITE_LPDDR3_MR(addr, val) \
	WRITE_MR(BIT(6) | BIT(7), MRCTRL1_MR_ADDR(addr) | MRCTRL1_MR_DATA(val));

static void mctl_dfi_init(const struct dram_para *para)
{
	struct sunxi_mctl_com_reg *mctl_com =
		(struct sunxi_mctl_com_reg *)SUNXI_DRAM_COM_BASE;
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;

	/* Unlock DFI registers? */
	setbits_le32(&mctl_com->maer0, BIT(8));

	/* Enable dfi_init_complete signal and trigger PHY init start request */
	writel(0, &mctl_ctl->swctl);
	setbits_le32(&mctl_ctl->dfimisc, BIT(0));
	setbits_le32(&mctl_ctl->dfimisc, BIT(5));
	writel(1, &mctl_ctl->swctl);
	mctl_await_completion(&mctl_ctl->swstat, BIT(0), BIT(0));

	/* Stop sending init request and wait for DFI initialization to complete. */
	writel(0, &mctl_ctl->swctl);
	clrbits_le32(&mctl_ctl->dfimisc, BIT(5));
	writel(1, &mctl_ctl->swctl);
	mctl_await_completion(&mctl_ctl->swstat, BIT(0), BIT(0));
	mctl_await_completion(&mctl_ctl->dfistat, BIT(0), BIT(0));

	/* Enter Software Exit from Self Refresh */
	writel(0, &mctl_ctl->swctl);
	clrbits_le32(&mctl_ctl->pwrctl, BIT(5));
	writel(1, &mctl_ctl->swctl);
	mctl_await_completion(&mctl_ctl->swstat, BIT(0), BIT(0));
	mctl_await_completion(&mctl_ctl->statr, BIT(1) | 0x3, 1);

	udelay(200);

	/* Disable dfi_init_complete signal */
	writel(0, &mctl_ctl->swctl);
	clrbits_le32(&mctl_ctl->dfimisc, BIT(0));
	writel(1, &mctl_ctl->swctl);
	mctl_await_completion(&mctl_ctl->swstat, BIT(0), BIT(0));

	/* Write mode registers */
	switch (para->type) {
	case SUNXI_DRAM_TYPE_DDR3:
		WRITE_MR(MRCTRL0_MR_ADDR(0), para->mr0);
		WRITE_MR(MRCTRL0_MR_ADDR(1), para->mr1);
		WRITE_MR(MRCTRL0_MR_ADDR(2), para->mr2);
		WRITE_MR(MRCTRL0_MR_ADDR(3), para->mr3);
		break;
	case SUNXI_DRAM_TYPE_DDR4:
		WRITE_MR(MRCTRL0_MR_ADDR(0), para->mr0);
		WRITE_MR(MRCTRL0_MR_ADDR(1), para->mr1);
		WRITE_MR(MRCTRL0_MR_ADDR(2), para->mr2);
		WRITE_MR(MRCTRL0_MR_ADDR(3), para->mr3);
		WRITE_MR(MRCTRL0_MR_ADDR(4), para->mr4);
		WRITE_MR(MRCTRL0_MR_ADDR(5), para->mr5);

		WRITE_MR(MRCTRL0_MR_ADDR(6), para->mr6 | BIT(7));
		WRITE_MR(MRCTRL0_MR_ADDR(6), para->mr6 | BIT(7));
		WRITE_MR(MRCTRL0_MR_ADDR(6), para->mr6 & (~BIT(7)));
		break;
	case SUNXI_DRAM_TYPE_LPDDR3:
		WRITE_LPDDR3_MR(1, para->mr1);
		WRITE_LPDDR3_MR(2, para->mr2);
		WRITE_LPDDR3_MR(3, para->mr3);
		WRITE_LPDDR3_MR(11, para->mr11);
		break;
	case SUNXI_DRAM_TYPE_LPDDR4:
		WRITE_LPDDR4_MR(0, para->mr0);
		WRITE_LPDDR4_MR(1, para->mr1);
		WRITE_LPDDR4_MR(2, para->mr2);
		WRITE_LPDDR4_MR(3, para->mr3);
		WRITE_LPDDR4_MR(4, para->mr4);
		WRITE_LPDDR4_MR(11, para->mr11);
		WRITE_LPDDR4_MR(12, para->mr12);
		WRITE_LPDDR4_MR(13, para->mr13);
		WRITE_LPDDR4_MR(14, para->mr14);
		WRITE_LPDDR4_MR(22, para->tpr1);
		break;
	}

	writel(0, SUNXI_DRAM_PHY0_BASE + 0x54);

	/* Re-enable controller refresh */
	writel(0, &mctl_ctl->swctl);
	clrbits_le32(&mctl_ctl->rfshctl3, BIT(0));
	writel(1, &mctl_ctl->swctl);
}

/* Slightly modified from H616 driver */
static bool mctl_phy_read_calibration(const struct dram_config *config)
{
	bool result = true;
	u32 val, tmp;

	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 0x30, 0x20);

	setbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 1);

	if (config->bus_full_width)
		val = 0xf;
	else
		val = 3;

	while ((readl(SUNXI_DRAM_PHY0_BASE + 0x184) & val) != val) {
		if (readl(SUNXI_DRAM_PHY0_BASE + 0x184) & 0x20) {
			result = false;
			break;
		}
	}

	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 1);

	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 0x30);

	if (config->ranks == 1) {
		clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 0x30, 0x10);

		setbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 1);

		while ((readl(SUNXI_DRAM_PHY0_BASE + 0x184) & val) != val) {
			if (readl(SUNXI_DRAM_PHY0_BASE + 0x184) & 0x20) {
				result = false;
				break;
			}
		}

		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 1);
	}

	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 0x30);

	val = readl(SUNXI_DRAM_PHY0_BASE + 0x274) & 7;
	tmp = readl(SUNXI_DRAM_PHY0_BASE + 0x26c) & 7;
	if (val < tmp)
		val = tmp;
	tmp = readl(SUNXI_DRAM_PHY0_BASE + 0x32c) & 7;
	if (val < tmp)
		val = tmp;
	tmp = readl(SUNXI_DRAM_PHY0_BASE + 0x334) & 7;
	if (val < tmp)
		val = tmp;
	clrsetbits_le32(SUNXI_DRAM_PHY0_BASE + 0x38, 0x7, (val + 2) & 7);

	setbits_le32(SUNXI_DRAM_PHY0_BASE + 4, 0x20);

	return result;
}

/*
 * TODO: Reimplement; copied from aodzip's repo, not studying this code to ensure originality.
 * Notes from RE, however, A133 uses different values than the H616.
 */
static void
libdram_mctl_phy_dx_bit_delay_compensation(const struct dram_para *para)
{
	int i;
	uint32_t val, *ptr;

	if (para->tpr10 & 0x40000) {
		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x60, 1);
		setbits_le32(SUNXI_DRAM_PHY0_BASE + 8, 8);
		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x190, 0x10);

		if (para->type == SUNXI_DRAM_TYPE_LPDDR4)
			clrbits_le32(SUNXI_DRAM_PHY0_BASE + 4, 0x80);

		val = para->tpr11 & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x484);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = para->para0 & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x4d0);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x590);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x4cc);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x58c);

		val = (para->tpr11 >> 8) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x4d8);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->para0 >> 8) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x524);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x5e4);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x520);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x5e0);

		val = (para->tpr11 >> 16) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x604);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->para0 >> 16) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x650);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x710);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x64c);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x70c);

		val = (para->tpr11 >> 24) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x658);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->para0 >> 24) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x6a4);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x764);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x6a0);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x760);
		setbits_le32(SUNXI_DRAM_PHY0_BASE + 0x60, 1);
	}

	if (para->tpr10 & 0x20000) {
		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x54, 0x80);
		clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x190, 4);

		val = para->tpr12 & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x480);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = para->tpr14 & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x528);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x5e8);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x4c8);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x588);

		val = (para->tpr12 >> 8) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x4d4);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->tpr14 >> 8) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x52c);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x5ec);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x51c);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x5dc);

		val = (para->tpr12 >> 16) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x600);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->tpr14 >> 16) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x6a8);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x768);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x648);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x708);

		val = (para->tpr12 >> 24) & 0x3F;
		ptr = (uint32_t *)(SUNXI_DRAM_PHY0_BASE + 0x654);
		for (i = 0; i < 9; i++) {
			writel(val, ptr);
			writel(val, ptr + 0x30);
			ptr += 2;
		}
		val = (para->tpr14 >> 24) & 0x3F;
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x6ac);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x76c);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x69c);
		writel(val, SUNXI_DRAM_PHY0_BASE + 0x75c);
	}

	setbits_le32(SUNXI_DRAM_PHY0_BASE + 0x54, 0x80);
}

static bool mctl_calibrate_phy(const struct dram_para *para,
			       const struct dram_config *config)
{
	struct sunxi_mctl_ctl_reg *mctl_ctl =
		(struct sunxi_mctl_ctl_reg *)SUNXI_DRAM_CTL0_BASE;

	int i;

	/* TODO: Implement write levelling */
	if (para->tpr10 & TPR10_READ_CALIBRATION) {
		for (i = 0; i < 5; i++)
			if (mctl_phy_read_calibration(config))
				break;
		if (i == 5) {
			return false;
		}
	}

	/* TODO: Implement read training levelling */
	/* TODO: Implement write training */

	libdram_mctl_phy_dx_bit_delay_compensation(para);
	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x60, BIT(0));
	clrbits_le32(SUNXI_DRAM_PHY0_BASE + 0x54, 7);

	writel(0, &mctl_ctl->swctl);
	clrbits_le32(&mctl_ctl->rfshctl3, BIT(1));
	writel(1, &mctl_ctl->swctl);
	mctl_await_completion(&mctl_ctl->swstat, BIT(0), BIT(0));

	return true;
}

static bool mctl_core_init(const struct dram_para *para,
			   const struct dram_config *config)
{
	mctl_clk_init(para->clk);
	mctl_com_init(para, config);
	mctl_phy_init(para, config);
	mctl_dfi_init(para);

	return mctl_calibrate_phy(para, config);
}

/* Heavily inspired from H616 driver. UNUSED */
/* static */void auto_detect_ranks(const struct dram_para *para,
			      struct dram_config *config)
{
	int i;
	bool found_config;

	config->cols = 9;
	config->rows = 14;
	config->ranks = 0;
	config->banks = 0;
	config->bankgrps = 0;

	/* Test ranks */
	found_config = false;
	for (i = 1; i >= 0; i--) {
		config->ranks = i;
		config->bus_full_width = true;
		debug("Testing ranks = %d, 32-bit bus\n", i);
		if (mctl_core_init(para, config)) {
			found_config = true;
			break;
		}

		config->bus_full_width = false;
		debug("Testing ranks = %d, 16-bit bus\n", i);
		if (mctl_core_init(para, config)) {
			found_config = true;
			break;
		}
	}

	debug("Found ranks = %d\n", config->ranks);
}

/* Modified from H616 driver, UNUSED? */
/* static */ void auto_detect_size(const struct dram_para *para,
			     struct dram_config *config)
{
	/* detect row address bits */
	config->cols = 8;
	config->rows = 18;
	config->banks = 0;
	config->bankgrps = 0;
	mctl_core_init(para, config);

	for (config->rows = 14; config->rows < 18; config->rows++) {
		/* 8 banks, 8 bit per byte and 16/32 bit width */
		if (mctl_mem_matches((1 << (config->bankgrps + config->banks +
					    config->cols + config->rows +
					    config->bus_full_width + 1))))
			break;
	}

	/* detect column address bits */
	config->cols = 12;
	mctl_core_init(para, config);

	for (config->cols = 8; config->cols < 12; config->cols++) {
		/* 8 bits per byte and 16/32 bit width */
		if (mctl_mem_matches(1 << (config->bankgrps + config->banks +
					   config->cols +
					   config->bus_full_width + 1)))
			break;
	}

	/* detect bank address bits */
	config->banks = 3;
	mctl_core_init(para, config);

	for (config->banks = 0; config->banks < 3; config->banks++) {
		if (mctl_mem_matches(1 << (config->banks + config->bankgrps +
					   config->cols +
					   config->bus_full_width + 1)))
			break;
	}

	/* TODO: This needs further testing on devices with different numbers of banks! */
	/* detect bank group address bits */
	config->bankgrps = 2;
	mctl_core_init(para, config);
	for (config->bankgrps = 0; config->bankgrps < 2; config->bankgrps++) {
		if (mctl_mem_matches_base(3 << (config->bankgrps + 2 +
						config->bus_full_width),
					  CFG_SYS_SDRAM_BASE + 0x10))
			break;
	}
}

/* Modified from H616 driver to add banks and bank groups */
static unsigned long calculate_dram_size(const struct dram_config *config)
{
	/* Bootrom only uses x32 or x16 bus widths */
	u8 width = config->bus_full_width ? 4 : 2;

	return (1ULL << (config->cols + config->rows + config->banks +
			 config->bankgrps)) *
	       width * (1ULL << config->ranks);
}

static const struct dram_para para = {
	.clk = CONFIG_DRAM_CLK,
#ifdef CONFIG_SUNXI_DRAM_DDR3
	.type = SUNXI_DRAM_TYPE_DDR3,
#elif defined(CONFIG_SUNXI_DRAM_DDR4)
	.type = SUNXI_DRAM_TYPE_DDR4,
#elif defined(CONFIG_SUNXI_DRAM_LPDDR3)
	.type = SUNXI_DRAM_TYPE_LPDDR3,
#elif defined(CONFIG_SUNXI_DRAM_LPDDR4)
	.type = SUNXI_DRAM_TYPE_LPDDR4,
#endif
	/* TODO: Populate from config */
	.dx_odt = 0x3030303,
	.dx_dri = 0xc0c0c0c,
	.ca_dri = 0x1919,
	.para0 = 0x12131615,
	.para1 = 0x610a,
	.para2 = 0x8000000,
	.mr0 = 0x520,
	.mr1 = 0x601,
	.mr2 = 0x8,
	.mr5 = 0x400,
	.mr6 = 0x862,
	.tpr3 = 0x80000000,
	.tpr6 = 0xa000,
	.tpr10 = 0x2f7777,
	.tpr11 = 0xd0f1411,
	.tpr12 = 0xb0b110f,
	.tpr13 = 0x7501,
	.tpr14 = 0x19191c1c,
};

/* TODO: Remove, copied and modified slightly from aodzip repo as temporary sanity check */
static int libdram_dramc_simple_wr_test(uint32_t dram_size, uint32_t test_range)
{
	uint32_t *dram_memory = (uint32_t *)CFG_SYS_SDRAM_BASE;
	uint32_t step = dram_size / 8;

	for (unsigned i = 0; i < test_range; i++) {
		dram_memory[i] = i + 0x1234567;
		dram_memory[i + step] = i - 0x1234568;
	}

	for (unsigned i = 0; i < test_range; i++) {
		uint32_t *ptr;
		if (dram_memory[i] != i + 0x1234567) {
			ptr = &dram_memory[i];
			goto fail;
		}
		if (dram_memory[i + step] != i - 0x1234568) {
			ptr = &dram_memory[i + step];
			goto fail;
		}
		continue;
fail:
		debug("DRAM simple test FAIL----- at address %p\n", ptr);
		return 1;
	}

	debug("DRAM simple test OK.\n");
	return 0;
}

unsigned long sunxi_dram_init(void)
{
	unsigned long size;

	/* Keeping for now as documentation of where different parameters come from */
	struct dram_config config = {
		.cols = (para.para1 & 0xF),
		.rows = (para.para1 >> 4) & 0xFF,
		.banks = (para.para1 >> 12) & 0x3,
		.bankgrps = (para.para1 >> 14) & 0x3,
		.ranks = ((para.tpr13 >> 16) & 3),
		.bus_full_width = !((para.para2 >> 3) & 1),
	};

	/* Writing to undocumented SYS_CFG area, according to user manual. */
	setbits_le32(0x03000160, BIT(8));
	clrbits_le32(0x03000168, 0x3f);

	auto_detect_ranks(&para, &config);
	auto_detect_size(&para, &config);

	if (!mctl_core_init(&para, &config))
		return 0;

	debug("cols = %d, rows = %d, banks = %d, bankgrps = %d, ranks = %d, full_width = %d\n",
	      config.cols, config.rows, config.banks, config.bankgrps,
	      config.ranks, config.bus_full_width);

	size = calculate_dram_size(&config);

	/* TODO: This is just a sanity check for now. */
	if (libdram_dramc_simple_wr_test(size, 4096))
		return 0;

	return size;
}
