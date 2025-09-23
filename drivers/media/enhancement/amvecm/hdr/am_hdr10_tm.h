/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "../amcsc.h"
#include "am_hdr10_tmo_fw.h"

#ifndef HDR10_TONE_MAPPING
#define HDR10_TONE_MAPPING

#define MAX12_BIT 12
#define OE_X 149
#define MAX32_BIT 32
#define MAX_BEIZER_ORDER 10
#define TM_GAIN_BIT 6
#define MAX_32 0xffffffff

extern unsigned int panell;
int hdr10_tm_dynamic_proc(struct vframe_master_display_colour_s *p);
#endif
#endif
