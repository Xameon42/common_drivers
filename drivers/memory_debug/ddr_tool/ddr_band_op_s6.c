// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "ddr_bandwidth.h"
#include <linux/io.h>
#include <linux/slab.h>

#define DMC_MON_CTRL0			((0x0010 << 2))
#define DMC_MON_TIMER			((0x0011 << 2))
#define DMC_MON_ALL_IDLE_CNT		((0x0012 << 2))
#define DMC_MON_ALL_BW			((0x0013 << 2))
#define DMC_MON_ALL16_BW		((0x0014 << 2))

#define DMC_AXI0_READ_BW		((0x0020  << 2))

static void s6_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int i, val;

	/* s6 can not se port, default be set */
	for (i = 0; i < db->channels; i++) {
		db->port[i] = 1 << i;
		db->range[i].start = 0;
		db->range[i].end   = 0x3ffffffffULL;
	}

	val = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	val |= 0xffffff;
	writel(val,  db->ddr_reg1 + DMC_MON_CTRL0);
}

static unsigned long s6_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	db->ddr_freq = (readl(db->pll_reg) & 0xffff) * 1000000;

	/* S6 dmc freq =  1/4 ddr freq */
	db->dmc_freq = db->ddr_freq >> 2;

	return db->dmc_freq;
}

static void s6_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	void *io;
	unsigned int i, val;

	for (i = 0; i < db->dmc_number; i++) {
		switch (i) {
		case 0:
			io = db->ddr_reg1;
			break;
		case 1:
			io = db->ddr_reg2;
			break;
		case 2:
			io = db->ddr_reg3;
			break;
		case 3:
			io = db->ddr_reg4;
			break;
		default:
			break;
		}

		val = db->mode << 31;
		val |= (readl(io + DMC_MON_CTRL0) & ~BIT(31));
		writel(val, io + DMC_MON_CTRL0);
	}
}

static void s6_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;
	void *io;

	/* set timer trigger clock_cnt */
	for (i = 0; i < db->dmc_number; i++) {
		switch (i) {
		case 0:
			io = db->ddr_reg1;
			break;
		case 1:
			io = db->ddr_reg2;
			break;
		case 2:
			io = db->ddr_reg3;
			break;
		case 3:
			io = db->ddr_reg4;
			break;
		default:
			break;
		}
		writel(db->clock_count, io + DMC_MON_TIMER);
	}
	s6_dmc_bandwidth_enable(db);

	s6_dmc_port_config(db, -1, -1);
}

static int s6_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off, d;
	void *io;
	int ret = -1;

	for (d = 0; d < db->dmc_number; d++) {
		switch (d) {
		case 0:
			io = db->ddr_reg1;
			break;
		case 1:
			io = db->ddr_reg2;
			break;
		case 2:
			io = db->ddr_reg3;
			break;
		case 3:
			io = db->ddr_reg4;
			break;
		default:
			break;
		}

		val = readl(io + DMC_MON_CTRL0);
		if (val & DMC_QOS_IRQ) {
			/*
			 * get total bytes by each channel, each cycle 16 bytes;
			 */
			dg->all_grant    += readl(io + DMC_MON_ALL_BW);
			dg->all_grant16  += readl(io + DMC_MON_ALL16_BW);
			for (i = 0; i < db->channels; i++) {
				off = i * 8 + DMC_AXI0_READ_BW;
				dg->channel_grant[i] += readl(io + off);
				dg->channel_grant[i] += readl(io + off + 4);
			}
			ret = 0;
		}
	}

	if (!ret) {
		/* clear irq flags */
		s6_dmc_bandwidth_enable(db);

		/* each register */
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++)
			dg->channel_grant[i] *= 16;
	}

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int s6_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i, d;
	unsigned int r, off;
	void *io;

	for (d = 0; d < db->dmc_number; d++) {
		switch (d) {
		case 0:
			io = db->ddr_reg1;
			break;
		case 1:
			io = db->ddr_reg2;
			break;
		case 2:
			io = db->ddr_reg3;
			break;
		case 3:
			io = db->ddr_reg4;
			break;
		default:
			break;
		}

		s += sprintf(buf + s, "\nDMC%d:\n", d);

		r  = readl(io + DMC_MON_CTRL0);
		s += sprintf(buf + s, "DMC_MON_CTRL0:        %08x\n", r);
		r  = readl(io + DMC_MON_TIMER);
		s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);
		r  = readl(io + DMC_MON_ALL_IDLE_CNT);
		s += sprintf(buf + s, "DMC_MON_ALL_IDLE_CNT: %08x\n", r);
		r  = readl(io + DMC_MON_ALL_BW);
		s += sprintf(buf + s, "DMC_MON_ALL_BW:       %08x\n", r);
		r  = readl(io + DMC_MON_ALL16_BW);
		s += sprintf(buf + s, "DMC_MON_ALL16_BW:     %08x\n", r);

		for (i = 0; i < db->channels; i++) {
			off = i * 8 + DMC_AXI0_READ_BW;
			r  = readl(io + off);
			s += sprintf(buf + s, "DMC_AXI%d_READ_BW:          %08x\n", i, r);
			r  = readl(io + off + 4);
			s += sprintf(buf + s, "DMC_AXI%d_WRITE_BW:          %08x\n", i, r);
		}
	}
	return s;
}
#endif

struct ddr_bandwidth_ops s6_ddr_bw_ops = {
	.init             = s6_dmc_bandwidth_init,
	.config_port      = s6_dmc_port_config,
	.get_freq         = s6_get_dmc_freq_quick,
	.handle_irq       = s6_handle_irq,
	.bandwidth_enable = s6_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = s6_dump_reg,
#endif
};
