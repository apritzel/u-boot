/*
 * Allwinner platform libdram blob DRAM controller init
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/dram.h>
#include <linux/kconfig.h>

/* Some callback functions, needed for the libdram blob */

void __usdelay(unsigned long us)
{
	udelay(us);
}

int set_ddr_voltage(int set_vol)
{
	return 0;
}

int axp_set_aldo3(int voltage, int enable)
{
	return 0;
}

/*
 * The init_DRAM function in libdram expects a data structure with the
 * DRAM parameters. From previous source code releases and by comparing
 * the parameters these field names have been derived:
 */
struct libdram_para {
	u32 dram_clk;
	u32 dram_type;
	u32 dram_zq;
	u32 dram_odt_en;
	u32 dram_para1;
	u32 dram_para2;
	u32 dram_mr0;
	u32 dram_mr1;
	u32 dram_mr2;
	u32 dram_mr3;
	u32 dram_mr4;
	u32 dram_mr5;
	u32 dram_mr6;
	u32 dram_tpr0;
	u32 dram_tpr1;
	u32 dram_tpr2;
	u32 dram_tpr3;
	u32 dram_tpr4;
	u32 dram_tpr5;
	u32 dram_tpr6;
	u32 dram_tpr7;
	u32 dram_tpr8;
	u32 dram_tpr9;
	u32 dram_tpr10;
	u32 dram_tpr11;
	u32 dram_tpr12;
	u32 dram_tpr13;
	u32 dram_tpr14;
	u32 dram_tpr15;
	u32 dram_tpr16;
	u32 dram_tpr17;
	u32 dram_tpr18;
} dram_para = { 0 };

/* Entry point to the libdram blob */
unsigned long init_DRAM(u32, void *);

unsigned long sunxi_dram_init(void)
{
	init_DRAM(0, &dram_para.dram_clk);

	return get_ram_size((long *)PHYS_SDRAM_0, PHYS_SDRAM_0_SIZE);
}
