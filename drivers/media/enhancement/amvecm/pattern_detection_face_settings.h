/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef __PATTERN_DETECTION_FACE_SETTINGS__
#define __PATTERN_DETECTION_FACE_SETTINGS__

static struct setting_regs_s face_cvd2_settings[] = {
	/* face AV NTSC */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xa00833da  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0x40100160  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0x00000050  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0x00000000  },
		}
	},
	/* face AV PAL */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0x800833da  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0x4011a293  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0x34f2038a  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xfe0df9e8  },
		}
	},
	/* default AV NTSC */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xffffffff  },
		}
	},
	/* default AV PAL */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xffffffff  },
		}
	}
};

#endif
#endif
