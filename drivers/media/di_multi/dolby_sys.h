/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DOLBY_SYS_H__
#define __DOLBY_SYS_H__
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/cdev.h>

#define AMDV_CORE1C_REG_START                     0x1800
#define AMDV_CORE1C_CLKGATE_CTRL                  0x18f2
#define AMDV_CORE1C_SWAP_CTRL0                    0x18f3
#define AMDV_CORE1C_SWAP_CTRL1                    0x18f4
#define AMDV_CORE1C_SWAP_CTRL2                    0x18f5
#define AMDV_CORE1C_SWAP_CTRL3                    0x18f6
#define AMDV_CORE1C_SWAP_CTRL4                    0x18f7
#define AMDV_CORE1C_SWAP_CTRL5                    0x18f8
#define AMDV_CORE1C_DMA_CTRL                      0x18f9
#define AMDV_CORE1C_DMA_STATUS                    0x18fa
#define AMDV_CORE1C_STATUS0                       0x18fb
#define AMDV_CORE1C_STATUS1                       0x18fc
#define AMDV_CORE1C_STATUS2                       0x18fd
#define AMDV_CORE1C_STATUS3                       0x18fe
#define AMDV_CORE1C_DMA_PORT                      0x18ff

#define DI_SC_TOP_CTRL                             0x374f
#define DI_HDR_IN_HSIZE                            0x376e
#define DI_HDR_IN_VSIZE                            0x376f

void di_dolby_sw_init(void);
int di_dolby_do_setting(void);
void di_dolby_enable(bool enable);
#endif

