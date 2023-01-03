// SPDX-License-Identifier:	GPL-2.0+
/*
 * D1/R528/T113 DRAM controller register and constant defines
 *
 * (C) Copyright 2022 Arm Ltd.
 * Based on H6 and H616 header, which are:
 * (C) Copyright 2017  Icenowy Zheng <icenowy@aosc.io>
 * (C) Copyright 2020  Jernej Skrabec <jernej.skrabec@siol.net>
 *
 */

#ifndef _SUNXI_DRAM_SUN20I_D1_H
#define _SUNXI_DRAM_SUN20I_D1_H

#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

enum sunxi_dram_type {
	SUNXI_DRAM_TYPE_DDR2 = 2,
	SUNXI_DRAM_TYPE_DDR3 = 3,
	SUNXI_DRAM_TYPE_LPDDR2 = 6,
	SUNXI_DRAM_TYPE_LPDDR3 = 7,
};

typedef struct dram_para {
	/* normal configuration */
	u32	dram_clk;
	u32	dram_type;
	u32	dram_zq;
	u32	dram_odt_en;

	/* control configuration */
	u32	dram_para1;
	u32	dram_para2;

	/* timing configuration */
	u32	dram_mr0;
	u32	dram_mr1;
	u32	dram_mr2;
	u32	dram_mr3;
	u32	dram_tpr0;	//DRAMTMG0
	u32	dram_tpr1;	//DRAMTMG1
	u32	dram_tpr2;	//DRAMTMG2
	u32	dram_tpr3;	//DRAMTMG3
	u32	dram_tpr4;	//DRAMTMG4
	u32	dram_tpr5;	//DRAMTMG5
	u32	dram_tpr6;	//DRAMTMG8
	u32	dram_tpr7;
	u32	dram_tpr8;
	u32	dram_tpr9;
	u32	dram_tpr10;
	u32	dram_tpr11;
	u32	dram_tpr12;
	u32	dram_tpr13;
} dram_para_t;

static inline int ns_to_t(int nanoseconds)
{
	const unsigned int ctrl_freq = CONFIG_DRAM_CLK / 2;

	return DIV_ROUND_UP(ctrl_freq * nanoseconds, 1000);
}

//static void mctl_set_timing_params(struct dram_para *para);

#endif /* _SUNXI_DRAM_SUN20I_D1_H */
