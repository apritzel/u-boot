/*
 * Nexell NSIH header, to allow loading U-Boot from secondboot.
 *
 * Both the proprietary and the GPL version of the first stage boot loader
 * look for this magic header to determine the size and load address of
 * the payload (similar to the information in an U-Boot image header).
 * Make them happy by providing the essential bits:
 * 	@0x040:	device address: 0 for SDMMC
 * 	@0x044:	load size
 * 	@0x048:	load address
 * 	@0x04c:	launch address (entry point)
 * 	@0x1fc: "NSIH" magic
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifdef CONFIG_ARM64
	b	reset				// jump over the header
	.space	0x3c				// fast forward to 0x40
#else
_start:
	ARM_VECTORS
	.space	0x20			 	// fill up from 0x20 till 0x40
#endif
	.word	0				// device "address" (device ID)
	.word	(_end - _start) + 32768		// binary size + space for .dtb
	.word	CONFIG_SYS_TEXT_BASE		// load address
	.word	CONFIG_SYS_TEXT_BASE		// launch address
	.space	0x1ac				// fast forward till 0x1fc
	.word	0x4849534e			// "NSIH" magic

	// In case someone enters right after the header:
#ifdef CONFIG_ARM64
	b	reset
#endif
