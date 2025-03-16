// SPDX-License-Identifier:	GPL-2.0+
/*
 * A133 dram controller register and constant defines
 *
 * (C) Copyright 2024  MasterR3C0RD <masterr3c0rd@epochal.quest>
 */

#ifndef _SUNXI_DRAM_SUN50I_A133_H
#define _SUNXI_DRAM_SUN50I_A133_H

#include <linux/bitops.h>

enum sunxi_dram_type {
	SUNXI_DRAM_TYPE_DDR3 = 3,
	SUNXI_DRAM_TYPE_DDR4,
	SUNXI_DRAM_TYPE_LPDDR3 = 7,
	SUNXI_DRAM_TYPE_LPDDR4
};

static inline int ns_to_t(int nanoseconds)
{
	const unsigned int ctrl_freq = CONFIG_DRAM_CLK / 2;

	return DIV_ROUND_UP(ctrl_freq * nanoseconds, 1000);
}

/* MBUS part is largely the same as in H6, except for one special register */
#define MCTL_COM_UNK_008	0x008
/* NOTE: This register has the same importance as mctl_ctl->clken in H616 */
#define MCTL_COM_MAER0		0x020

/*
 * Controller registers seems to be the same or at least very similar
 * to those in H6.
 */
struct sunxi_mctl_ctl_reg {
	u32 mstr; 			/* 0x000 */
	u32 statr; 			/* 0x004 unused */
	u32 mstr1; 			/* 0x008 unused */
	u32 clken; 			/* 0x00c */
	u32 mrctrl0; 			/* 0x010 unused */
	u32 mrctrl1; 			/* 0x014 unused */
	u32 mrstatr; 			/* 0x018 unused */
	u32 mrctrl2; 			/* 0x01c unused */
	u32 derateen; 			/* 0x020 unused */
	u32 derateint; 			/* 0x024 unused */
	u8 reserved_0x028[8]; 		/* 0x028 */
	u32 pwrctl; 			/* 0x030 unused */
	u32 pwrtmg; 			/* 0x034 unused */
	u32 hwlpctl; 			/* 0x038 unused */
	u8 reserved_0x03c[20];		/* 0x03c */
	u32 rfshctl0;			/* 0x050 unused */
	u32 rfshctl1;			/* 0x054 unused */
	u8 reserved_0x058[8];		/* 0x05c */
	u32 rfshctl3;			/* 0x060 */
	u32 rfshtmg;			/* 0x064 */
	u8 reserved_0x068[104];		/* 0x068 */
	u32 init[8];			/* 0x0d0 */
	u32 dimmctl;			/* 0x0f0 unused */
	u32 rankctl;			/* 0x0f4 */
	u8 reserved_0x0f8[8];		/* 0x0f8 */
	u32 dramtmg[17];		/* 0x100 */
	u8 reserved_0x144[60];		/* 0x144 */
	u32 zqctl[3];			/* 0x180 */
	u32 zqstat;			/* 0x18c unused */
	u32 dfitmg0;			/* 0x190 */
	u32 dfitmg1;			/* 0x194 */
	u32 dfilpcfg[2];		/* 0x198 unused */
	u32 dfiupd[3];			/* 0x1a0 */
	u32 reserved_0x1ac;		/* 0x1ac */
	u32 dfimisc;			/* 0x1b0 */
	u32 dfitmg2;			/* 0x1b4 unused */
	u32 dfitmg3;			/* 0x1b8 unused */
	u32 dfistat;			/* 0x1bc */
	u32 dbictl;			/* 0x1c0 */
	u8 reserved_0x1c4[60];		/* 0x1c4 */
	u32 addrmap[12];		/* 0x200 */
	u8 reserved_0x230[16];		/* 0x230 */
	u32 odtcfg;			/* 0x240 */
	u32 odtmap;			/* 0x244 */
	u8 reserved_0x248[8];		/* 0x248 */
	u32 sched[2];			/* 0x250 */
	u8 reserved_0x258[180]; 	/* 0x258 */
	u32 dbgcmd;			/* 0x30c unused */
	u32 dbgstat;			/* 0x310 unused */
	u8 reserved_0x314[12];		/* 0x314 */
	u32 swctl;			/* 0x320 */
	u32 swstat;			/* 0x324 */
	u8 reserved_0x328[7768];	/* 0x328 */
	u32 unk_0x2180;			/* 0x2180 */
	u8 reserved_0x2184[188];	/* 0x2184 */
	u32 unk_0x2240;			/* 0x2240 */
	u8 reserved_0x2244[3900];	/* 0x2244 */
	u32 unk_0x3180;			/* 0x3180 */
	u8 reserved_0x3184[188];	/* 0x3184 */
	u32 unk_0x3240;			/* 0x3240 */
	u8 reserved_0x3244[3900];	/* 0x3244 */
	u32 unk_0x4180;			/* 0x4180 */
	u8 reserved_0x4184[188];	/* 0x4184 */
	u32 unk_0x4240;			/* 0x4240 */
};

check_member(sunxi_mctl_ctl_reg, swstat, 0x324);
check_member(sunxi_mctl_ctl_reg, unk_0x4240, 0x4240);

#define MSTR_DEVICETYPE_DDR3	BIT(0)
#define MSTR_DEVICETYPE_LPDDR2	BIT(2)
#define MSTR_DEVICETYPE_LPDDR3	BIT(3)
#define MSTR_DEVICETYPE_DDR4	BIT(4)
#define MSTR_DEVICETYPE_LPDDR4	BIT(5)
#define MSTR_DEVICETYPE_MASK	GENMASK(5, 0)
#define MSTR_GEARDOWNMODE	BIT(0)		/* Same as MSTR_DEVICETYPE_DDR3, only used for DDR4 */
#define MSTR_2TMODE		BIT(10)
#define MSTR_BUSWIDTH_FULL	(0 << 12)
#define MSTR_BUSWIDTH_HALF	(1 << 12)
#define MSTR_ACTIVE_RANKS(x)	(((x == 1) ? 3 : 1) << 24)
#define MSTR_BURST_LENGTH(x)	(((x) >> 1) << 16)
#define MSTR_DEVICECONFIG_X32	(3 << 30)

#define TPR10_CA_BIT_DELAY	BIT(16)
#define TPR10_DX_BIT_DELAY0	BIT(17)
#define TPR10_DX_BIT_DELAY1	BIT(18)
#define TPR10_WRITE_LEVELING	BIT(20)
#define TPR10_READ_CALIBRATION	BIT(21)
#define TPR10_READ_TRAINING	BIT(22)
#define TPR10_WRITE_TRAINING	BIT(23)

/* MRCTRL constants */
#define MRCTRL0_MR_RANKS_ALL	(3 << 4)
#define MRCTRL0_MR_ADDR(x)	(x << 12)
#define MRCTRL0_MR_WR		BIT(31)

#define MRCTRL1_MR_ADDR(x)	(x << 8)
#define MRCTRL1_MR_DATA(x)	(x)

struct dram_para {
	uint32_t clk;
	enum sunxi_dram_type type;
	uint32_t dx_odt;
	uint32_t dx_dri;
	uint32_t ca_dri;
	uint32_t para0;
	uint32_t mr11;
	uint32_t mr12;
	uint32_t mr13;
	uint32_t mr14;
	uint32_t tpr1;
	uint32_t tpr2;
	uint32_t tpr3;
	uint32_t tpr6;
	uint32_t tpr10;
	uint32_t tpr11;
	uint32_t tpr12;
	uint32_t tpr13;
	uint32_t tpr14;
};

void mctl_set_timing_params(const struct dram_para *para);

struct dram_config {
	u8 cols;		/* Column bits */
	u8 rows;		/* Row bits */
	u8 ranks;		/* Rank bits (different from H616!) */
	u8 banks;		/* Bank bits */
	u8 bankgrps;		/* Bank group bits */
	u8 bus_full_width;	/* 1 = x32, 0 = x16 */
};

#endif /* _SUNXI_DRAM_SUN50I_A133_H */
