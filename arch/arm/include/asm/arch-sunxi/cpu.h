/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2015 Hans de Goede <hdegoede@redhat.com>
 */

#ifndef _SUNXI_CPU_H
#define _SUNXI_CPU_H

#define SOCID_A10	0x1623
#define SOCID_A13	0x1625
#define SOCID_A31	0x1633
#define SOCID_A80	0x1639
#define SOCID_A23	0x1650
#define SOCID_A20	0x1651
#define SOCID_A33	0x1667
#define SOCID_A83T	0x1673
#define SOCID_H3	0x1680
#define SOCID_V3S	0x1681
#define SOCID_A64	0x1689
#define SOCID_R40	0x1701
#define SOCID_H5	0x1718
#define SOCID_H6	0x1728

#if defined(CONFIG_MACH_SUN9I)
#include <asm/arch/cpu_sun9i.h>
#elif defined(CONFIG_MACH_SUN50I_H6)
#include <asm/arch/cpu_sun50i_h6.h>
#else
#include <asm/arch/cpu_sun4i.h>
#endif

#endif /* _SUNXI_CPU_H */
