/*
 * Copyright (C) 2017 Amit Singh Tomar <amittomer25@gmail.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
*/

#include <common.h>
#include <asm/armv8/mmu.h>

DECLARE_GLOBAL_DATA_PTR;

static struct mm_region nexell_s5p6818_mem_map[] = {
	{
		.virt   = 0xc0000000UL,
		.phys   = 0xc0000000UL,
		.size   = 0x20000000UL,
		.attrs  = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
				PTE_BLOCK_INNER_SHARE |
				PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	}, {
		.virt   = 0x40000000UL,
		.phys   = 0x40000000UL,
		.size   = 0x80000000UL,
		.attrs  = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_OUTER_SHARE,
	}, {
		.virt = 0xFFFF0000ULL,
		.phys = 0xFFFF0000ULL,
		.size = 0x00010000ULL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_INNER_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	},
};

struct mm_region *mem_map = nexell_s5p6818_mem_map;
