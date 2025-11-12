// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "lut_dma_io.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../rdma/rdma.h"
#endif

struct rdma_fun_s rdma_func[5];

static u32 _VSYNC_RD_VIDEO_TABLE_REG(u32 adr)
{
	return VSYNC_RD_MPEG_REG(adr);
}

static int _VSYNC_WR_VIDEO_TABLE_REG(u32 adr, u32 val)
{
	return VSYNC_WR_MPEG_REG(adr, val);
}

static int _VSYNC_WR_VIDEO_TABLE_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}

static u32 _PRE_VSYNC_RD_VIDEO_TABLE_REG(u32 adr)
{
	return PRE_VSYNC_RD_MPEG_REG(adr);
}

static int _PRE_VSYNC_WR_VIDEO_TABLE_REG(u32 adr, u32 val)
{
	return PRE_VSYNC_WR_MPEG_REG(adr, val);
}

static int _PRE_VSYNC_WR_VIDEO_TABLE_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	return PRE_VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}

static u32 _aml_read_vcbus(u32 adr)
{
	return aml_read_vcbus(adr);
}

static int _aml_write_vcbus(u32 adr, u32 val)
{
	aml_write_vcbus(adr, val);
	return 0;
}

static int _aml_write_vcbus_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
		 ~(((1L << (len)) - 1) << (start))) |
		(((value) & ((1L << (len)) - 1)) << (start))));
	return 0;
}

void set_lut_rdma_func_handler(void)
{
	bool is_rdma = false;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_S7)) {
		is_rdma = true;
	}
	if (is_rdma) {
		rdma_func[0].rdma_rd =
			_VSYNC_RD_VIDEO_TABLE_REG;
		rdma_func[0].rdma_wr =
			_VSYNC_WR_VIDEO_TABLE_REG;
		rdma_func[0].rdma_wr_bits =
			_VSYNC_WR_VIDEO_TABLE_REG_BITS;

		rdma_func[1].rdma_rd =
			VSYNC_RD_MPEG_REG_VPP1;
		rdma_func[1].rdma_wr =
			VSYNC_WR_MPEG_REG_VPP1;
		rdma_func[1].rdma_wr_bits =
			VSYNC_WR_MPEG_REG_BITS_VPP1;

		rdma_func[2].rdma_rd =
			VSYNC_RD_MPEG_REG_VPP2;
		rdma_func[2].rdma_wr =
			VSYNC_WR_MPEG_REG_VPP2;
		rdma_func[2].rdma_wr_bits =
			VSYNC_WR_MPEG_REG_BITS_VPP2;

		rdma_func[3].rdma_rd =
			_PRE_VSYNC_RD_VIDEO_TABLE_REG;
		rdma_func[3].rdma_wr =
			_PRE_VSYNC_WR_VIDEO_TABLE_REG;
		rdma_func[3].rdma_wr_bits =
			_PRE_VSYNC_WR_VIDEO_TABLE_REG_BITS;
	} else {
		rdma_func[0].rdma_rd =
			_aml_read_vcbus;
		rdma_func[0].rdma_wr =
			_aml_write_vcbus;
		rdma_func[0].rdma_wr_bits =
			_aml_write_vcbus_bits;

		rdma_func[1].rdma_rd =
			_aml_read_vcbus;
		rdma_func[1].rdma_wr =
			_aml_write_vcbus;
		rdma_func[1].rdma_wr_bits =
			_aml_write_vcbus_bits;

		rdma_func[2].rdma_rd =
			_aml_read_vcbus;
		rdma_func[2].rdma_wr =
			_aml_write_vcbus;
		rdma_func[2].rdma_wr_bits =
			_aml_write_vcbus_bits;

		rdma_func[3].rdma_rd =
			_aml_read_vcbus;
		rdma_func[3].rdma_wr =
			_aml_write_vcbus;
		rdma_func[3].rdma_wr_bits =
			_aml_write_vcbus_bits;
	}
	rdma_func[4].rdma_rd =
		_aml_read_vcbus;
	rdma_func[4].rdma_wr =
		_aml_write_vcbus;
	rdma_func[4].rdma_wr_bits =
		_aml_write_vcbus_bits;
}

u32 lut_dma_reg_read(u32 reg, u8 vpp_index)
{
	u32 ret = 0;

	if (vpp_index >= VPP_MAX)
		vpp_index = VPP_MAX;
	ret = rdma_func[vpp_index].rdma_rd(reg);
	return ret;
};

void lut_dma_reg_write(u32 reg, const u32 val, u8 vpp_index)
{
	if (vpp_index >= VPP_MAX)
		vpp_index = VPP_MAX;
	rdma_func[vpp_index].rdma_wr(reg, val);
};

void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len,
			  u8 vpp_index)
{
	if (vpp_index >= VPP_MAX)
		vpp_index = VPP_MAX;
	rdma_func[vpp_index].rdma_wr_bits(reg, value, start, len);
}
