/*
 * Configuration settings for the Allwinner A64 (sun50i) CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifdef CONFIG_ARM_BOOT_HOOK_BOOT0
	b	reset
	.space	1532
#elif defined(CONFIG_ARM_BOOT_HOOK_RMR)
	tst     x0, x0                  // this is "b #0x84" in ARM
	b       reset
	.space  0x7c
	.word   0xe3a01617              // mov  r1, #0x1700000
	.word   0xe38110a0              // orr  r1, r1, #0xa0
	.word   0xe59f0020              // ldr  r0, [pc, #32]
	.word   0xe5810000              // str  r0, [r1]
	.word   0xf57ff04f              // dsb  sy
	.word   0xf57ff06f              // isb  sy
	.word   0xee1c0f50              // mrc  15, 0, r0, cr12, cr0, {2}
	.word   0xe3800003              // orr  r0, r0, #3
	.word   0xee0c0f50              // mcr  15, 0, r0, cr12, cr0, {2}
	.word   0xf57ff06f              // isb  sy
	.word   0xe320f003              // wfi
	.word   0xeafffffd              // b    @wfi
	.word   CONFIG_SYS_TEXT_BASE
#else
	b	reset
#endif
