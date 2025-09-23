/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "../amcsc.h"

#ifndef GAMUT_H
#define GAMUT_H

extern uint force_primary;
extern uint force_matrix;
extern u32 force_matrix_primary[3][3];
extern u32 force_src_primary[8];
extern u32 force_dst_primary[8];
int gamut_convert_process(struct vinfo_s *vinfo,
			  enum hdr_type_e *source_type,
			  enum vd_path_e vd_path,
			  struct matrix_s *mtx,
			  int mtx_depth,
			  enum dest_hdr_type_e dest_type);
int gamut_convert(s64 *s_prmy,
			  s64 *d_prmy,
			  struct matrix_s *mtx,
			  int mtx_depth);
#endif
#endif
