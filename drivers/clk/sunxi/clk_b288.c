// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2025 Arm Ltd.
 */

#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <clk/sunxi.h>
#include <dt-bindings/clock/sun8i-b288-ccu.h>
#include <dt-bindings/reset/sun8i-b288-ccu.h>
#include <linux/bitops.h>

static struct ccu_clk_gate b288_gates[] = {
	[CLK_BUS_MMC0]		= GATE(0x060, BIT(8)),
	[CLK_BUS_MMC1]		= GATE(0x060, BIT(9)),
	[CLK_BUS_MMC2]		= GATE(0x060, BIT(10)),
	[CLK_BUS_MMC3]		= GATE(0x060, BIT(11)),
	[CLK_BUS_SPI0]		= GATE(0x060, BIT(20)),
	[CLK_BUS_SPI1]		= GATE(0x060, BIT(21)),
	[CLK_BUS_SPI2]		= GATE(0x060, BIT(22)),
	[CLK_BUS_OTG]		= GATE(0x060, BIT(24)),
	[CLK_BUS_EHCI0]		= GATE(0x060, BIT(26)),
	[CLK_BUS_OHCI0]		= GATE(0x060, BIT(29)),

	[CLK_BUS_TCON]		= GATE(0x064, BIT(4)),
	[CLK_BUS_DE]		= GATE(0x064, BIT(12)),

	[CLK_BUS_PIO]		= GATE(0x068, BIT(5)),

	[CLK_BUS_I2C0]		= GATE(0x06c, BIT(0)),
	[CLK_BUS_I2C1]		= GATE(0x06c, BIT(1)),
	[CLK_BUS_I2C2]		= GATE(0x06c, BIT(2)),
	[CLK_BUS_UART0]		= GATE(0x06c, BIT(16)),
	[CLK_BUS_UART1]		= GATE(0x06c, BIT(17)),
	[CLK_BUS_UART2]		= GATE(0x06c, BIT(18)),
	[CLK_BUS_UART3]		= GATE(0x06c, BIT(19)),
	[CLK_BUS_UART4]		= GATE(0x06c, BIT(20)),
	[CLK_BUS_UART5]		= GATE(0x06c, BIT(21)),

	[CLK_SPI0]		= GATE(0x0a0, BIT(31)),
	[CLK_SPI1]		= GATE(0x0a4, BIT(31)),
	[CLK_SPI2]		= GATE(0x0a8, BIT(31)),

	[CLK_USB_PHY0]		= GATE(0x0cc, BIT(8)),
	[CLK_USB_OHCI0]		= GATE(0x0cc, BIT(16)),

	[CLK_DE]		= GATE(0x104, BIT(31)),
	[CLK_TCON]		= GATE(0x118, BIT(31)),
};

static struct ccu_reset b288_resets[] = {
	[RST_USB_PHY0]		= RESET(0x0cc, BIT(0)),

	[RST_BUS_MMC0]		= RESET(0x2c0, BIT(8)),
	[RST_BUS_MMC1]		= RESET(0x2c0, BIT(9)),
	[RST_BUS_MMC2]		= RESET(0x2c0, BIT(10)),
	[RST_BUS_MMC3]		= RESET(0x2c0, BIT(11)),
	[RST_BUS_SPI0]		= RESET(0x2c0, BIT(20)),
	[RST_BUS_SPI1]		= RESET(0x2c0, BIT(21)),
	[RST_BUS_SPI2]		= RESET(0x2c0, BIT(22)),
	[RST_BUS_OTG]		= RESET(0x2c0, BIT(24)),
	[RST_BUS_EHCI0]		= RESET(0x2c0, BIT(26)),
	[RST_BUS_OHCI0]		= RESET(0x2c0, BIT(29)),

	[RST_BUS_DE]		= RESET(0x2c4, BIT(12)),
	[RST_BUS_TCON]		= RESET(0x2c4, BIT(4)),

	[RST_BUS_I2C0]		= RESET(0x2cc, BIT(0)),
	[RST_BUS_I2C1]		= RESET(0x2cc, BIT(1)),
	[RST_BUS_I2C2]		= RESET(0x2cc, BIT(2)),
	[RST_BUS_UART0]		= RESET(0x2cc, BIT(16)),
	[RST_BUS_UART1]		= RESET(0x2cc, BIT(17)),
	[RST_BUS_UART2]		= RESET(0x2cc, BIT(18)),
	[RST_BUS_UART3]		= RESET(0x2cc, BIT(19)),
	[RST_BUS_UART4]		= RESET(0x2cc, BIT(20)),
	[RST_BUS_UART5]		= RESET(0x2cc, BIT(21)),
};

const struct ccu_desc b288_ccu_desc = {
	.gates = b288_gates,
	.resets = b288_resets,
	.num_gates = ARRAY_SIZE(b288_gates),
	.num_resets = ARRAY_SIZE(b288_resets),
};
