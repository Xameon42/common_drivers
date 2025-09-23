/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

extern unsigned int cvbs_clk_path;
void set_vmode_clk(void);
void disable_vmode_clk(void);
void cvbs_out_vid_pll_set(unsigned int _reg, unsigned int _value,
			  unsigned int _start, unsigned int _len);
unsigned int cvbs_out_vid_pll_read(unsigned int _reg);
#endif
