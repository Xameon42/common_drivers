/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __CM2_ADJ__
#define __CM2_ADJ__

#include <uapi/amlogic/amvecm_ext.h>

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/*H00 ~ H31*/
#define CM2_ENH_COEF0_H00 0x100
#define CM2_ENH_COEF1_H00 0x101
#define CM2_ENH_COEF2_H00 0x102
#define CM2_ENH_COEF3_H00 0x103
#define CM2_ENH_COEF4_H00 0x104

#define CM2_ENH_COEF0_H01 0x108
#define CM2_ENH_COEF1_H01 0x109
#define CM2_ENH_COEF2_H01 0x10a
#define CM2_ENH_COEF3_H01 0x10b
#define CM2_ENH_COEF4_H01 0x10c

#define CM2_ENH_COEF0_H02 0x110
#define CM2_ENH_COEF1_H02 0x111
#define CM2_ENH_COEF2_H02 0x112
#define CM2_ENH_COEF3_H02 0x113
#define CM2_ENH_COEF4_H02 0x114

void cm2_curve_update_hue_by_hs(struct cm_color_md cm_color_md_hue_by_hs);
void cm2_curve_update_hue(struct cm_color_md cm_color_md_hue);
void cm2_curve_update_luma(struct cm_color_md cm_color_md_luma);
void cm2_curve_update_sat(struct cm_color_md cm_color_md_sat);

void cm2_hue_by_hs(struct cm_color_md cm_color_mode, int hue_val, int lpf_en);
void cm2_hue(struct cm_color_md cm_color_mode, int hue_val, int lpf_en);
void cm2_luma(struct cm_color_md cm_color_mode, int luma_val, int lpf_en);
void cm2_sat(struct cm_color_md cm_color_mode, int sat_val, int lpf_en);

void default_sat_param(unsigned int reg, unsigned int value);

#endif
#endif
