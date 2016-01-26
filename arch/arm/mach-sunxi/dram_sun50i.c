/*
 * sun50i platform dram controller init
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
	/* DCDC5 is already set as 1.5V by default */
	return 0;
}

/* A piece of data, extracted from the Pine64 Android image */
u32 dram_para[] = {
    0x000002A0,
    0x00000003,
    0x003B3BBB,
    0x00000001,
    0x10E410E4,
    0x00001000,
    0x00001840,
    0x00000040,
    0x00000018,
    0x00000002,
    0x004A2195,
    0x02424190,
    0x0008B060,
    0x04B005DC,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00008808,
    0x20250000,
    0x00000000,
    0x04000800,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

/* Entry point to the libdram blob */
unsigned long init_DRAM(u32, void *);

unsigned long sunxi_dram_init(void)
{
	init_DRAM(0, &dram_para[0]);
	return get_ram_size((long *)PHYS_SDRAM_0, PHYS_SDRAM_0_SIZE);
}
