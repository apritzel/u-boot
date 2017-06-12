/*
 * Copyright (C) 2017 Andre Przywara <andre.przywara@arm.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define NEXELL_PLLSETREG0	0xc0010008UL

#define OSC_FREQ 24000000

/* TODO: dummy implementation for now, add proper reset code */
void reset_cpu(ulong addr)
{
}

/* TODO: enhance when multiple SoCs are supported (S5P4418) */
int print_cpuinfo(void)
{
	puts("CPU:   Nexell S5P6818\n");

	return 0;
}

/* TODO: dummy for now, either implement DRAM init or rely on vendor code */
int dram_init(void)
{
	/* TODO: hard coded for now, read from DT? */
	gd->ram_size = 0x40000000;

	return 0;
}

ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}

static unsigned long get_pll_freq(int pll_index)
{
	uint32_t reg;
	unsigned int pdiv, mdiv, sdiv, plloutdiv;
	unsigned int multfreq;

	if (pll_index < 0 || pll_index > 3)
		return 0;

	reg = readl(NEXELL_PLLSETREG0 + pll_index * 4);
	sdiv = reg & 0xff;
	mdiv = (reg >> 8) & 0x3ff;
	pdiv = (reg >> 18) & 0x3f;
	plloutdiv = ((reg >> 24) & 0xf) + 1;

	multfreq = (OSC_FREQ / 1000) * mdiv;
	return (1000 * (multfreq / (pdiv * 2 * sdiv))) / plloutdiv;
}

static unsigned long get_level1_clk_freq(uintptr_t base_addr)
{
	uint32_t reg;
	unsigned int pll_index, div;

	reg = readl(base_addr + 0x4);
	pll_index = (reg >> 2) & 0x7;
	if (pll_index > 3)
		return -1UL;

	div = ((reg >> 5) & 0xff) + 1;

	return get_pll_freq(pll_index) / div;
}

unsigned long get_uart_clk(int dev_index)
{
	uintptr_t clock_ofs[6] = {0xc00a9000, 0xc00a8000, 0xc00aa000,
				  0xc00ab000, 0xc006e000, 0xc0084000};

	if (dev_index < 0 || dev_index > 5)
		return 0;

	return get_level1_clk_freq(clock_ofs[dev_index]);
}

int board_init(void)
{
	return 0;
}
