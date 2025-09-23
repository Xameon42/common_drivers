/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_CM_H
#define __AM_CM_H

#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/cm.h"

struct sr1_regs_s {
	unsigned int addr;
	unsigned int mask;
	unsigned int  val;
};

extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int cm2_patch_flag;
extern int cm_en; /* 0:disable;1:enable */
extern int dnlp_en;/*0:disable;1:enable */

extern unsigned int sr1_reg_val[101];
extern int pq_reg_wr_rdma;

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */
void am_set_regmap(struct am_regs_s *p, int vpp_index);
void amcm_disable(enum wr_md_e md, int vpp_index);
void amcm_enable(enum wr_md_e md, int vpp_index);
void amcm_level_sel(unsigned int cm_level, int vpp_index);
void cm2_frame_size_patch(struct vframe_s *vf,
	unsigned int width, unsigned int height, int vpp_index);
void cm2_frame_switch_patch(void);
void cm_latch_process(int vpp_index);
int cm_load_reg(struct am_regs_s *arg);
void pd_combing_fix_patch(enum pd_comb_fix_lvl_e level);

#endif

