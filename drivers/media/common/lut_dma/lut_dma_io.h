/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LUT_DMA_IO_H_
#define _LUT_DMA_IO_H_
#include <linux/amlogic/iomap.h>

enum vpp_type_e {
	VPP0 = 0,
	VPP1 = 1,
	VPP2 = 2,
	PRE_VSYNC = 3,
	VPP_MAX = 4,
	VPP_INVALID = 0xff
};

typedef u32 (*rdma_rd_op)(u32 reg);
typedef int (*rdma_wr_op)(u32 reg, u32 val);
typedef int (*rdma_wr_bits_op)(u32 reg, u32 val, u32 start, u32 len);

struct rdma_fun_s {
	rdma_rd_op rdma_rd;
	rdma_wr_op rdma_wr;
	rdma_wr_bits_op rdma_wr_bits;
};

u32 lut_dma_reg_read(u32 reg, u8 vpp_index);
void lut_dma_reg_write(u32 reg, const u32 val, u8 vpp_index);
void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len,
			  u8 vpp_index);
void set_lut_rdma_func_handler(void);
#endif
