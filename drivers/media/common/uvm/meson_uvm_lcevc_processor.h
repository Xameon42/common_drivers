/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_UVM_LCEVC_PROCESSOR_H__
#define __MESON_UVM_LCEVC_PROCESSOR_H__

#include <linux/amlogic/meson_uvm_core.h>
/*!
 * \brief This enum represents the output packing format of
 *	  an LCEVC residual plane buffer
 */
enum LCEVC_ResidualPacking {
	/**< Residual plane is 8bit, i.e. one pixel per byte. */
	LCEVC_ResidualY8bit = 0,
	/**< Residual plane is Y packed 10 bit, 4 pixels in 5 bytes. */
	LCEVC_ResidualY10bitPacked = 1,
	/**< Residual plane is Y unpacked, 10 bits in 2 bytes - i.e. YUV */
	LCEVC_ResidualY10bitUnpacked = 2,
	/**< Residual plane is UYVY 8 bit, 2 pixels in 4 bytes. */
	LCEVC_ResidualUYVY8bit = 3,
	/**< Residual plane is YVYU 8 bit, 2 pixels in 4 bytes. */
	LCEVC_ResidualYVYU8bit = 4,
	/**< Residual plane is UYVY packed 10 bit, 2 pixels in 5 bytes. */
	LCEVC_ResidualUYVY10bitPacked = 5,
	/**< Residual plane is UYVY 10 bit, 2 pixels in 8 bytes. */
	LCEVC_ResidualUYVY10bitUnpacked = 6,
	/**< Residual plane is VYUY packed 10 bit, 2 pixels in 5 bytes. */
	LCEVC_ResidualVYUY10bitPacked = 7,
	/**< Residual plane is VYUY 10 bit, 2 pixels in 8 bytes. */
	LCEVC_ResidualVYUY10bitUnpacked = 8,
	/**< Residual plane is YVYU packed 10 bit, 2 pixels in 5 bytes. */
	LCEVC_ResidualYVYU10bitPacked = 9,
	/**< Residual plane is YVYU 10 bit, 2 pixels in 8 bytes. */
	LCEVC_ResidualYVYU10bitUnpacked = 10,
	/**< Residual plane is YUYV packed 10 bit, 2 pixels in 5 bytes. */
	LCEVC_ResidualYUYV10bitPacked = 11,
	/**< Residual plane is YUYV 10 bit, 2 pixels in 8 bytes. */
	LCEVC_ResidualYUYV10bitUnpacked = 12,
	/**< Residual plane is YUYV 16 bit, 2 pixels in 8 bytes. */
	LCEVC_ResidualYUYV16bit = 13,
};

/* Struct containing the upsample kernel.
 * two arrays of length `len` for the kernel
 * and 180-degree kernel.
 */
struct uvm_lcevc_upsample_kernel {
	/* upsample kernels of length 'len',
	 * phase kernel and 180-degree phase kernel.
	 */
	s16 k[2][8];
	u32 len;
};

/* Struct containing information about a residual plane. */
struct uvm_lcevc_residual_plane_info {
	u32 width;
	u32 height;
	u32 packing; // enum LCEVC_ResidualPacking
	u32 size; // size of the plane, in bytes
};

struct uvm_lcevc_frame_info {
	u32 width;
	u32 height;
	u32 frame_type;
	s64 y_physical_addr;
	s64 uv_physical_addr;
	u32 stride;
	struct uvm_lcevc_upsample_kernel upsample_kernel;
};

struct uvm_ioctl_lcevc_data {
	s32 lcevc_fd;//lcevc yuv dma fd
	s32 buffer_index;
	u32 stride;
	u32 has_new_plane_info;
	struct uvm_lcevc_residual_plane_info residual_plane_info;
	u32 has_new_upsample_info;
	struct uvm_lcevc_upsample_kernel upsample_kernel;
};

struct uvm_lcevc_hook_data {
	//send to video composer, vpp
	struct uvm_lcevc_frame_info lcevc_vframe;
};

int attach_lcevc_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int lcevc_setinfo(void *arg, char *buf);
int lcevc_getinfo(void *arg, char *buf);
void lcevc_free_data(void *arg);

#endif //#ifndef __MESON_UVM_LCEVC_PROCESSOR_H__
