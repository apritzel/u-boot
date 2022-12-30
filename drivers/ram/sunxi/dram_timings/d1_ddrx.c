// SPDX-License-Identifier: GPL-2.0+
/*
 * DRAM chip dependent timing parameters
 */

#include <asm/io.h>
#include <common.h>
#include "../dram_sun20i_d1.h"

// Main purpose of the auto_set_timing routine seems to be to calculate all
// timing settings for the specific type of sdram used. Read together with
// an sdram datasheet for context on the various variables.
//
void mctl_set_timing_params(dram_para_t *para)
{
	/* DRAM_TPR0 */
	u8 tccd		= 2;
	u8 tfaw;
	u8 trrd;
	u8 trcd;
	u8 trc;

	/* DRAM_TPR1 */
	u8 txp;
	u8 twtr;
	u8 trtp		= 4;
	u8 twr;
	u8 trp;
	u8 tras;

	/* DRAM_TPR2 */
	u16 trefi;
	u16 trfc;

	u8 tcksrx;
	u8 tckesr;
	u8 trd2wr;
	u8 twr2rd;
	u8 trasmax;
	u8 twtp;
	u8 tcke;
	u8 tmod;
	u8 tmrd;
	u8 tmrw;

	u8 tcl;
	u8 tcwl;
	u8 t_rdata_en;
	u8 wr_latency;

	u32 mr0;
	u32 mr1;
	u32 mr2;
	u32 mr3;

	u32 tdinit0;
	u32 tdinit1;
	u32 tdinit2;
	u32 tdinit3;

		if (para->dram_type == 3) {
			// DDR3
			trfc  = ns_to_t(350);
			trefi = ns_to_t(7800) / 32 + 1; // XXX
			twr	  = ns_to_t(8);
			trcd  = ns_to_t(15);
			twtr  = twr + 2; // + 2 ? XXX
			if (twr < 2)
				twtr = 2;
			twr = trcd;
			if (trcd < 2)
				twr = 2;
			if (CONFIG_DRAM_CLK <= 800) {
				tfaw = ns_to_t(50);
				trrd = ns_to_t(10);
				if (trrd < 2)
					trrd = 2;
				trc	 = ns_to_t(53);
				tras = ns_to_t(38);
				txp	 = trrd; // 10
				trp	 = trcd; // 15
			} else {
				tfaw = ns_to_t(35);
				trrd = ns_to_t(10);
				if (trrd < 2)
					trrd = 2;
				trcd = ns_to_t(14);
				trc	 = ns_to_t(48);
				tras = ns_to_t(34);
				txp	 = trrd; // 10
				trp	 = trcd; // 14
			}
		} else if (para->dram_type == 2) {
			// DDR2
			tfaw  = ns_to_t(50);
			trrd  = ns_to_t(10);
			trcd  = ns_to_t(20);
			trc	  = ns_to_t(65);
			twtr  = ns_to_t(8);
			trp	  = ns_to_t(15);
			tras  = ns_to_t(45);
			trefi = ns_to_t(7800) / 32;
			trfc  = ns_to_t(328);
			txp	  = 2;
			twr	  = trp; // 15
		} else if (para->dram_type == 6) {
			// LPDDR2
			tfaw = ns_to_t(50);
			if (tfaw < 4)
				tfaw = 4;
			trrd = ns_to_t(10);
			if (trrd == 0)
				trrd = 1;
			trcd = ns_to_t(24);
			if (trcd < 2)
				trcd = 2;
			trc = ns_to_t(70);
			txp = ns_to_t(8);
			if (txp == 0) {
				txp	 = 1;
				twtr = 2;
			} else {
				twtr = txp;
				if (txp < 2) {
					txp	 = 2;
					twtr = 2;
				}
			}
			twr = ns_to_t(15);
			if (twr < 2)
				twr = 2;
			trp	  = ns_to_t(17);
			tras  = ns_to_t(42);
			trefi = ns_to_t(3900) / 32;
			trfc  = ns_to_t(210);
		} else if (para->dram_type == 7) {
			// LPDDR3
			tfaw = ns_to_t(50);
			if (tfaw < 4)
				tfaw = 4;
			trrd = ns_to_t(10);
			if (trrd == 0)
				trrd = 1;
			trcd = ns_to_t(24);
			if (trcd < 2)
				trcd = 2;
			trc	 = ns_to_t(70);
			twtr = ns_to_t(8);
			if (twtr < 2)
				twtr = 2;
			twr = ns_to_t(15);
			if (twr < 2)
				twr = 2;
			trp	  = ns_to_t(17);
			tras  = ns_to_t(42);
			trefi = ns_to_t(3900) / 32;
			trfc  = ns_to_t(210);
			txp	  = twtr;
		} else {
			// default
			trfc  = 128;
			trp	  = 6;
			trefi = 98;
			txp	  = 10;
			twr	  = 8;
			twtr  = 3;
			tras  = 14;
			tfaw  = 16;
			trc	  = 20;
			trcd  = 6;
			trrd  = 3;
		}

	switch (para->dram_type) {
		case 2: // DDR2
		{
			trasmax = CONFIG_DRAM_CLK / 30;
			if (CONFIG_DRAM_CLK < 409) {
				tcl		   = 3;
				t_rdata_en = 1;
				mr0		   = 0x06a3;
			} else {
				t_rdata_en = 2;
				tcl		   = 4;
				mr0		   = 0x0e73;
			}
			tmrd	   = 2;
			twtp	   = twr + 5;
			tcksrx	   = 5;
			tckesr	   = 4;
			trd2wr	   = 4;
			tcke	   = 3;
			tmod	   = 12;
			wr_latency = 1;
			mr3		   = 0;
			mr2		   = 0;
			tdinit0	   = 200 * CONFIG_DRAM_CLK + 1;
			tdinit1	   = 100 * CONFIG_DRAM_CLK / 1000 + 1;
			tdinit2	   = 200 * CONFIG_DRAM_CLK + 1;
			tdinit3	   = 1 * CONFIG_DRAM_CLK + 1;
			tmrw	   = 0;
			twr2rd	   = twtr + 5;
			tcwl	   = 0;
			mr1		   = para->dram_mr1;
			break;
		}
		case 3: // DDR3
		{
			trasmax = CONFIG_DRAM_CLK / 30;
			if (CONFIG_DRAM_CLK <= 800) {
				mr0		   = 0x1c70;
				tcl		   = 6;
				wr_latency = 2;
				tcwl	   = 4;
				mr2		   = 24;
			} else {
				mr0		   = 0x1e14;
				tcl		   = 7;
				wr_latency = 3;
				tcwl	   = 5;
				mr2		   = 32;
			}

			twtp   = tcwl + 2 + twtr; // WL+BL/2+tWTR
			trd2wr = tcwl + 2 + twr; // WL+BL/2+tWR
			twr2rd = tcwl + twtr; // WL+tWTR

			tdinit0 = 500 * CONFIG_DRAM_CLK + 1; // 500 us
			tdinit1 = 360 * CONFIG_DRAM_CLK / 1000 + 1; // 360 ns
			tdinit2 = 200 * CONFIG_DRAM_CLK + 1; // 200 us
			tdinit3 = 1 * CONFIG_DRAM_CLK + 1; //   1 us

			if (((para->dram_tpr13 >> 2) & 0x03) == 0x01 || CONFIG_DRAM_CLK < 912) {
				mr1		   = para->dram_mr1;
				t_rdata_en = tcwl; // a5 <- a4
				tcksrx	   = 5;
				tckesr	   = 4;
				trd2wr	   = 5;
			} else {
				mr1		   = para->dram_mr1;
				t_rdata_en = tcwl; // a5 <- a4
				tcksrx	   = 5;
				tckesr	   = 4;
				trd2wr	   = 6;
			}
			tcke = 3; // not in .S ?
			tmod = 12;
			tmrd = 4;
			tmrw = 0;
			mr3	 = 0;
			break;
		}
		case 6: // LPDDR2
		{
			trasmax	   = CONFIG_DRAM_CLK / 60;
			mr3		   = para->dram_mr3;
			twtp	   = twr + 5;
			mr2		   = 6;
			mr1		   = 5;
			tcksrx	   = 5;
			tckesr	   = 5;
			trd2wr	   = 10;
			tcke	   = 2;
			tmod	   = 5;
			tmrd	   = 5;
			tmrw	   = 3;
			tcl		   = 4;
			wr_latency = 1;
			t_rdata_en = 1;
			tdinit0	   = 200 * CONFIG_DRAM_CLK + 1;
			tdinit1	   = 100 * CONFIG_DRAM_CLK / 1000 + 1;
			tdinit2	   = 11 * CONFIG_DRAM_CLK + 1;
			tdinit3	   = 1 * CONFIG_DRAM_CLK + 1;
			twr2rd	   = twtr + 5;
			tcwl	   = 2;
			mr1		   = 195;
			mr0		   = 0;
			break;
		}

		case 7: // LPDDR3
		{
			trasmax = CONFIG_DRAM_CLK / 60;
			if (CONFIG_DRAM_CLK < 800) {
				tcwl	   = 4;
				wr_latency = 3;
				t_rdata_en = 6;
				mr2		   = 12;
			} else {
				tcwl	   = 3;
				tcke	   = 6;
				wr_latency = 2;
				t_rdata_en = 5;
				mr2		   = 10;
			}
			twtp	= tcwl + 5;
			tcl		= 7;
			mr3		= para->dram_mr3;
			tcksrx	= 5;
			tckesr	= 5;
			trd2wr	= 13;
			tcke	= 3;
			tmod	= 12;
			tdinit0 = 400 * CONFIG_DRAM_CLK + 1;
			tdinit1 = 500 * CONFIG_DRAM_CLK / 1000 + 1;
			tdinit2 = 11 * CONFIG_DRAM_CLK + 1;
			tdinit3 = 1 * CONFIG_DRAM_CLK + 1;
			tmrd	= 5;
			tmrw	= 5;
			twr2rd	= tcwl + twtr + 5;
			mr1		= 195;
			mr0		= 0;
			break;
		}
		default:
			twr2rd	   = 8; // 48(sp)
			tcksrx	   = 4; // t1
			tckesr	   = 3; // t4
			trd2wr	   = 4; // t6
			trasmax	   = 27; // t3
			twtp	   = 12; // s6
			tcke	   = 2; // s8
			tmod	   = 6; // t0
			tmrd	   = 2; // t5
			tmrw	   = 0; // a1
			tcwl	   = 3; // a5
			tcl		   = 3; // a0
			wr_latency = 1; // a7
			t_rdata_en = 1; // a4
			mr3		   = 0; // s0
			mr2		   = 0; // t2
			mr1		   = 0; // s1
			mr0		   = 0; // a3
			tdinit3	   = 0; // 40(sp)
			tdinit2	   = 0; // 32(sp)
			tdinit1	   = 0; // 24(sp)
			tdinit0	   = 0; // 16(sp)
			break;
	}

	/* Set mode registers */
	writel(mr0, 0x3103030);
	writel(mr1, 0x3103034);
	writel(mr2, 0x3103038);
	writel(mr3, 0x310303c);
	/* TODO: dram_odt_en is either 0x0 or 0x1, so right shift looks weird */
	writel((para->dram_odt_en >> 4) & 0x3, 0x310302c); // ??

	/* Set dram timing DRAMTMG0 - DRAMTMG5 */
	writel((twtp << 24) | (tfaw << 16) | (trasmax << 8) | (tras << 0),
		0x3103058);
	writel((txp << 16) | (trtp << 8) | (trc << 0),
		0x310305c);
	writel((tcwl << 24) | (tcl << 16) | (trd2wr << 8) | (twr2rd << 0),
		0x3103060);
	writel((tmrw << 16) | (tmrd << 12) | (tmod << 0),
		0x3103064);
	writel((trcd << 24) | (tccd << 16) | (trrd << 8) | (trp << 0),
		0x3103068);
	writel((tcksrx << 24) | (tcksrx << 16) | (tckesr << 8) | (tcke << 0),
		0x310306c);

	/* Set dual rank timing */
	clrsetbits_le32(0x3103078, 0xf000ffff,
			(CONFIG_DRAM_CLK < 800) ? 0xf0006610 : 0xf0007610);

	/* Set phy interface time PITMG0, PTR3, PTR4 */
	writel((0x2 << 24) | (t_rdata_en << 16) | BIT(8) | (wr_latency << 0),
		0x3103080);
	writel(((tdinit0 << 0) | (tdinit1 << 20)), 0x3103050);
	writel(((tdinit2 << 0) | (tdinit3 << 20)), 0x3103054);

	/* Set refresh timing and mode */
	writel((trefi << 16) | (trfc << 0), 0x3103090);
	writel((trefi << 15) & 0x0fff0000, 0x3103094);
}
