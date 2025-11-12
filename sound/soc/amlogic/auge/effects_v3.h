/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_V3_H__
#define __EFFECTS_V3_H__

int add_effect_v3_kcontrols(struct snd_soc_card *card);
void aed_v3_set_filter_data(void);
void aed_v3_set_drc_data(void);

#define V3_EQ_FILTER_RAM_READ			BIT(0)
#define V3_POST_EQ_FILTER_RAM_READ		BIT(1)
#define V3_CROSSOVER_FILTER_RAM_READ		BIT(2)
#define V3_MULTIBAND_FILTER_RAM_READ		BIT(3)
#define V3_FULLBAND_FILTER_RAM_READ		BIT(4)
#define V3_3D_SURROUND_RAM_READ			BIT(5)

#endif
