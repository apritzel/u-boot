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
	unsigned int type; // s8
	unsigned int tpr13; // 80(sp)
	unsigned int reg_val;

	unsigned char  tccd; // 88(sp)
	unsigned char  trrd; // s7
	unsigned char  trcd; // s3
	unsigned char  trc; // s9
	unsigned char  tfaw; // s10
	unsigned char  tras; // s11
	unsigned char  trp; // 0(sp)
	unsigned char  twtr; // s1
	unsigned char  twr; // s6
	unsigned char  trtp; // 64(sp)
	unsigned char  txp; // a6
	unsigned short trefi; // s2
	unsigned short trfc; // a5 / 8(sp)

	type  = para->dram_type;
	tpr13 = para->dram_tpr13;

	// trace("type  = %d\r\n", type);
	// trace("tpr13 = %p\r\n", tpr13);

	if (para->dram_tpr13 & 0x2) {
		// dram_tpr0
		tccd = ((para->dram_tpr0 >> 21) & 0x7); // [23:21]
		tfaw = ((para->dram_tpr0 >> 15) & 0x3f); // [20:15]
		trrd = ((para->dram_tpr0 >> 11) & 0xf); // [14:11]
		trcd = ((para->dram_tpr0 >> 6) & 0x1f); // [10:6 ]
		trc	 = ((para->dram_tpr0 >> 0) & 0x3f); // [ 5:0 ]
		// dram_tpr1
		txp	 = ((para->dram_tpr1 >> 23) & 0x1f); // [27:23]
		twtr = ((para->dram_tpr1 >> 20) & 0x7); // [22:20]
		trtp = ((para->dram_tpr1 >> 15) & 0x1f); // [19:15]
		twr	 = ((para->dram_tpr1 >> 11) & 0xf); // [14:11]
		trp	 = ((para->dram_tpr1 >> 6) & 0x1f); // [10:6 ]
		tras = ((para->dram_tpr1 >> 0) & 0x3f); // [ 5:0 ]
		// dram_tpr2
		trfc  = ((para->dram_tpr2 >> 12) & 0x1ff); // [20:12]
		trefi = ((para->dram_tpr2 >> 0) & 0xfff); // [11:0 ]
	} else {
		if (type == 3) {
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
#if 1
		} else if (type == 2) {
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
		} else if (type == 6) {
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
		} else if (type == 7) {
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
#endif
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
		// assign the value back to the DRAM structure
		tccd			= 2;
		trtp			= 4; // not in .S ?
		para->dram_tpr0 = (trc << 0) | (trcd << 6) | (trrd << 11) | (tfaw << 15) | (tccd << 21);
		para->dram_tpr1 = (tras << 0) | (trp << 6) | (twr << 11) | (trtp << 15) | (twtr << 20) | (txp << 23);
		para->dram_tpr2 = (trefi << 0) | (trfc << 12);

		uint32_t tref = (para->dram_tpr4 << 0x10) >> 0x1c;
		if (tref == 1) {
			debug("trefi:3.9ms\n");
		} else if (tref == 2) {
			debug("trefi:1.95ms\n");
		} else {
			debug("trefi:7.8ms\n");
		}
	}

	unsigned int tcksrx; // t1
	unsigned int tckesr; // t4;
	unsigned int trd2wr; // t6
	unsigned int trasmax; // t3;
	unsigned int twtp; // s6 (was twr!)
	unsigned int tcke; // s8
	unsigned int tmod; // t0
	unsigned int tmrd; // t5
	unsigned int tmrw; // a1
	unsigned int t_rdata_en; // a4 (was tcwl!)
	unsigned int tcl; // a0
	unsigned int wr_latency; // a7
	unsigned int tcwl; // first a4, then a5
	unsigned int mr3; // s0
	unsigned int mr2; // t2
	unsigned int mr1; // s1
	unsigned int mr0; // a3
	unsigned int dmr3; // 72(sp)
	// unsigned int trtp;	// 64(sp)
	unsigned int dmr1; // 56(sp)
	unsigned int twr2rd; // 48(sp)
	unsigned int tdinit3; // 40(sp)
	unsigned int tdinit2; // 32(sp)
	unsigned int tdinit1; // 24(sp)
	unsigned int tdinit0; // 16(sp)

	dmr1 = para->dram_mr1;
	dmr3 = para->dram_mr3;

	switch (type) {
#if 1
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
			mr1		   = dmr1;
			break;
		}
#endif
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

			if (((tpr13 >> 2) & 0x03) == 0x01 || CONFIG_DRAM_CLK < 912) {
				mr1		   = dmr1;
				t_rdata_en = tcwl; // a5 <- a4
				tcksrx	   = 5;
				tckesr	   = 4;
				trd2wr	   = 5;
			} else {
				mr1		   = dmr1;
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
#if 1
		case 6: // LPDDR2
		{
			trasmax	   = CONFIG_DRAM_CLK / 60;
			mr3		   = dmr3;
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
			mr3		= dmr3;
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
#endif
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
	if (trtp < tcl - trp + 2) {
		trtp = tcl - trp + 2;
	}
	trtp = 4;

	// Update mode block when permitted
	if ((para->dram_mr0 & 0xffff0000) == 0)
		para->dram_mr0 = mr0;
	if ((para->dram_mr1 & 0xffff0000) == 0)
		para->dram_mr1 = mr1;
	if ((para->dram_mr2 & 0xffff0000) == 0)
		para->dram_mr2 = mr2;
	if ((para->dram_mr3 & 0xffff0000) == 0)
		para->dram_mr3 = mr3;

	// Set mode registers
	writel(para->dram_mr0, 0x3103030);
	writel(para->dram_mr1, 0x3103034);
	writel(para->dram_mr2, 0x3103038);
	writel(para->dram_mr3, 0x310303c);
	writel((para->dram_odt_en >> 4) & 0x3, 0x310302c); // ??

	// Set dram timing DRAMTMG0 - DRAMTMG5
	reg_val = (twtp << 24) | (tfaw << 16) | (trasmax << 8) | (tras << 0);
	writel(reg_val, 0x3103058);
	reg_val = (txp << 16) | (trtp << 8) | (trc << 0);
	writel(reg_val, 0x310305c);
	reg_val = (tcwl << 24) | (tcl << 16) | (trd2wr << 8) | (twr2rd << 0);
	writel(reg_val, 0x3103060);
	reg_val = (tmrw << 16) | (tmrd << 12) | (tmod << 0);
	writel(reg_val, 0x3103064);
	reg_val = (trcd << 24) | (tccd << 16) | (trrd << 8) | (trp << 0);
	writel(reg_val, 0x3103068);
	reg_val = (tcksrx << 24) | (tcksrx << 16) | (tckesr << 8) | (tcke << 0);
	writel(reg_val, 0x310306c);

	// Set two rank timing
	reg_val = readl(0x3103078);
	reg_val &= 0x0fff0000;
	reg_val |= (CONFIG_DRAM_CLK < 800) ? 0xf0006600 : 0xf0007600;
	reg_val |= 0x10;
	writel(reg_val, 0x3103078);

	// Set phy interface time PITMG0, PTR3, PTR4
	reg_val = (0x2 << 24) | (t_rdata_en << 16) | BIT(8) | (wr_latency << 0);
	writel(reg_val, 0x3103080);
	writel(((tdinit0 << 0) | (tdinit1 << 20)), 0x3103050);
	writel(((tdinit2 << 0) | (tdinit3 << 20)), 0x3103054);

	// Set refresh timing and mode
	reg_val = (trefi << 16) | (trfc << 0);
	writel(reg_val, 0x3103090);
	reg_val = 0x0fff0000 & (trefi << 15);
	writel(reg_val, 0x3103094);
}
