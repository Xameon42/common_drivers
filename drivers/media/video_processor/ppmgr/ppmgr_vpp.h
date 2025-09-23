/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _PPMGR_TV_H_
#define _PPMGR_TV_H_

void vf_ppmgr_unreg_provider(void);
void vf_ppmgr_reset(int type);
void ppmgr_vf_put_dec(struct vframe_s *vf);

enum ppmgr_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

extern u32 omx_cur_session;
extern int ppmgr_secure_debug;
extern int ppmgr_secure_mode;
#endif
