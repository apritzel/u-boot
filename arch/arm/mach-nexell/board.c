/*
 * Copyright (C) 2017 Andre Przywara <andre.przywara@arm.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>

DECLARE_GLOBAL_DATA_PTR;

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

int board_init(void)
{
	return 0;
}
