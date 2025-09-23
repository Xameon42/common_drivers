/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef __PATTERN_DETECTION_CORN_SETTINGS__
#define __PATTERN_DETECTION_CORN_SETTINGS__

static struct setting_regs_s corn_cvd2_settings[] = {
	/* corn AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0x50502020  },
		}
	},
	/* corn AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0x18182020  },
		}
	},
	/* default AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0xffffffff  },
		}
	},
	/* default AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0xffffffff  },
		}
	},
};

#endif
#endif
