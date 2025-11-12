/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_V2_H__
#define __EFFECTS_V2_H__

#define AED_REG_NUM	13

enum {
	VERSION1 = 0,
	VERSION2,
	VERSION3,
	VERSION4,
	VERSION5
};

struct audioeffect {
	struct device *dev;

	/* gate */
	struct clk *gate;
	/* source mpll */
	struct clk *srcpll;
	/* eqdrc clk */
	struct clk *clk;

	struct effect_chipinfo *chipinfo;

	int lane_mask;
	int ch_mask;

	/* which module should be effected */
	int effect_module;
	unsigned int syssrc_clk_rate;
	/* store user setting */
	u32 user_setting[AED_REG_NUM];
	int aed_get_ram_mask;
};

struct audioeffect *get_audioeffects(void);
int check_aed_version(void);
int str2int(char *str, unsigned int *data, int size);
int mixer_aed_read(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int mixer_aed_write(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
#endif
