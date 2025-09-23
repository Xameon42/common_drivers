/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_VDAC_DEV_H_
#define _INC_VDAC_DEV_H_

#define VDAC_MODULE_MASK      (0x1f)
#define VDAC_MODULE_AVOUT_ATV (0x1)
#define VDAC_MODULE_DTV_DEMOD (0x2)
#define VDAC_MODULE_AVOUT_AV  (0x4)
#define VDAC_MODULE_CVBS_OUT  (0x8)
#define VDAC_MODULE_AUDIO_OUT (0x10) /*0x10*/

void vdac_enable(bool on, unsigned int module_sel);
int vdac_enable_check_dtv(void);
int vdac_enable_check_cvbs(void);
int vdac_vref_adj(unsigned int value);
unsigned int vdac_get_reg_addr(unsigned int index);

#endif
