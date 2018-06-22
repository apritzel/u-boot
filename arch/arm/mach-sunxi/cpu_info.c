// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */

#include <common.h>
#include <init.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <axp_pmic.h>
#include <errno.h>

int sunxi_get_ss_bonding_id(void)
{
	static int bonding_id = -1;

#ifdef CONFIG_MACH_SUN6I
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	if (bonding_id != -1)
		return bonding_id;

	/* Enable Security System */
	setbits_le32(&ccm->ahb_reset0_cfg, 1 << AHB_RESET_OFFSET_SS);
	setbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_SS);

	bonding_id = readl(SUNXI_SS_BASE);
	bonding_id = (bonding_id >> 16) & 0x7;

	/* Disable Security System again */
	clrbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_SS);
	clrbits_le32(&ccm->ahb_reset0_cfg, 1 << AHB_RESET_OFFSET_SS);
#endif

	return bonding_id;
}

unsigned int sunxi_read_socid(void)
{
	uint id;

	/* Unlock sram info reg, read it, relock */
	setbits_le32(SUNXI_SRAMC_BASE + 0x24, (1 << 15));
	id = readl(SUNXI_SRAMC_BASE + 0x24) >> 16;
	clrbits_le32(SUNXI_SRAMC_BASE + 0x24, (1 << 15));

	return id;
}

#ifdef CONFIG_DISPLAY_CPUINFO
int print_cpuinfo(void)
{
	uint socid = sunxi_read_socid();
	char *socname;
	int generation = 0;
	u32 val;

	switch (socid) {
	case SOCID_A10: socname = "A10"; generation = 4; break;
	case SOCID_A13:
		generation = 5;
		val = readl(SUNXI_SID_BASE + 0x08);
		switch ((val >> 12) & 0xf) {
		case 0: socname = "A12"; break;
		case 3: socname = "A13"; break;
		case 7: socname = "A10s"; break;
		default: socname = "A1X"; break;
		}
		break;
	case SOCID_A31:
		generation = 6;
		switch (sunxi_get_ss_bonding_id()) {
		case SUNXI_SS_BOND_ID_A31: socname = "A31"; break;
		case SUNXI_SS_BOND_ID_A31S: socname = "A31s"; break;
		default: socname = "A31?"; break;
		}
		break;
	case SOCID_A20: generation = 7; socname = "A20"; break;
	case SOCID_A23: generation = 8; socname = "A23"; break;
	case SOCID_A33: generation = 8; socname = "A33"; break;
	case SOCID_A83T: generation = 8; socname = "A83T"; break;
	case SOCID_H3: generation = 8; socname = "H3"; break;
	case SOCID_R40: generation = 8; socname = "R40"; break;
	case SOCID_V3S: generation = 8; socname = "V3s"; break;
	case SOCID_A80: generation = 9; socname = "A80"; break;
	case SOCID_A64: generation = 50; socname = "A64"; break;
	case SOCID_H5: generation = 50; socname = "H5"; break;
	case SOCID_H6: generation = 50; socname = "H6"; break;		 
	default:
		puts("CPU: unknown SUNXI family\n");
		return 0;
	}
	printf("CPU:   Allwinner %s (SUN%dI %04x)\n", socname, generation,
	       socid);

	return 0;
}
#endif

#ifdef CONFIG_MACH_SUN8I_H3

#define SIDC_PRCTL 0x40
#define SIDC_RDKEY 0x60

#define SIDC_OP_LOCK 0xAC

uint32_t sun8i_efuse_read(uint32_t offset)
{
	uint32_t reg_val;

	reg_val = readl(SUNXI_SIDC_BASE + SIDC_PRCTL);
	reg_val &= ~(((0x1ff) << 16) | 0x3);
	reg_val |= (offset << 16);
	writel(reg_val, SUNXI_SIDC_BASE + SIDC_PRCTL);

	reg_val &= ~(((0xff) << 8) | 0x3);
	reg_val |= (SIDC_OP_LOCK << 8) | 0x2;
	writel(reg_val, SUNXI_SIDC_BASE + SIDC_PRCTL);

	while (readl(SUNXI_SIDC_BASE + SIDC_PRCTL) & 0x2);

	reg_val &= ~(((0x1ff) << 16) | ((0xff) << 8) | 0x3);
	writel(reg_val, SUNXI_SIDC_BASE + SIDC_PRCTL);

	reg_val = readl(SUNXI_SIDC_BASE + SIDC_RDKEY);
	return reg_val;
}
#endif

int sunxi_get_sid(unsigned int *sid)
{
#ifdef CONFIG_AXP221_POWER
	return axp_get_sid(sid);
#elif defined CONFIG_MACH_SUN8I_H3
	/*
	 * H3 SID controller has a bug, which makes the initial value of
	 * SUNXI_SID_BASE at boot wrong.
	 * Read the value directly from SID controller, in order to get
	 * the correct value, and also refresh the wrong value at
	 * SUNXI_SID_BASE.
	 */
	int i;

	for (i = 0; i< 4; i++)
		sid[i] = sun8i_efuse_read(i * 4);

	return 0;
#elif defined SUNXI_SID_BASE
	int i;

	for (i = 0; i< 4; i++)
		sid[i] = readl((ulong)SUNXI_SID_BASE + 4 * i);

	return 0;
#else
	return -ENODEV;
#endif
}
