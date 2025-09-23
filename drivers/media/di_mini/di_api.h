/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_API_H__
#define __DI_API_H__

#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "../di_local/di_local.h"
//#include "di_local.h"
//#include "deinterlace.h"

/*--------------------------*/
unsigned int l_DI_POST_REG_RD(unsigned int addr);
int l_DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len);

/*--------------------------*/
bool di_attach_ext_api(struct di_ext_ops *di_api);

/*attach di_ops to di_local*/
bool dil_attach_ext_api(const struct di_ext_ops *di_api);
void dim_attach_to_local(void);

void diext_clk_b_sw(bool on);

/*--------------------------*/
int get_current_vscale_skip_count(struct vframe_s *vf);

/*--------------------------*/
void dil_get_rev_mem(unsigned long *mstart, unsigned int *msize);
void dil_get_flg(unsigned int *flg);

#endif	/*__DI_API_H__*/
