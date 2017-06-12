/*
 * (C) Copyright 2017 ARM Ltd.
 * Andre Przywara <andre.przywara@arm.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_ARCH_CLK_H_
#define __ASM_ARCH_CLK_H_

unsigned long get_uart_clk(int dev_index);
unsigned long get_mmc_clk(int dev_index);
unsigned long get_pwm_clk(void);

#endif
