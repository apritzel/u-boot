/*
 * (C) Copyright 2016 ARM Ltd.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <spl.h>

#include <asm/io.h>
#include <asm/barriers.h>

static void __noreturn jump_to_image_native(struct spl_image_info *spl_image)
{
	typedef void __noreturn (*image_entry_noargs_t)(void);

	image_entry_noargs_t image_entry =
				(image_entry_noargs_t)spl_image->entry_point;

	image_entry();
}

static void __noreturn reset_rmr_switch(void)
{
#ifdef CONFIG_ARM64
	__asm__ volatile ( "mrs	 x0, RMR_EL3\n\t"
			   "bic  x0, x0, #1\n\t"   /* Clear enter-in-64 bit */
			   "orr  x0, x0, #2\n\t"   /* set reset request bit */
			   "msr  RMR_EL3, x0\n\t"
			   "isb  sy\n\t"
			   "nop\n\t"
			   "wfi\n\t"
			   "b    .\n"
			   ::: "x0");
#else
	__asm__ volatile ( "mrc  15, 0, r0, cr12, cr0, 2\n\t"
			   "orr  r0, r0, #3\n\t"   /* request reset in 64 bit */
			   "mcr  15, 0, r0, cr12, cr0, 2\n\t"
			   "isb\n\t"
			   "nop\n\t"
			   "wfi\n\t"
			   "b    .\n"
			   ::: "r0");
#endif
	while (1);	/* to avoid a compiler warning about __noreturn */
}

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
	if (spl_image->arch == IH_ARCH_DEFAULT) {
		debug("entering by branch\n");
		jump_to_image_native(spl_image);
	} else {
		debug("entering by RMR switch\n");
		writel(spl_image->entry_point, 0x17000a0);
		DSB;
		ISB;
		reset_rmr_switch();
	}
}
