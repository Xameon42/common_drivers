/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _CVBS_MODE_H_
#define _CVBS_MODE_H_

enum cvbs_mode_e {
	MODE_480CVBS = 0,
	MODE_576CVBS,
	MODE_PAL_M,
	MODE_PAL_N,
	MODE_NTSC_M,
	MODE_MAX,
};

enum cvbs_mode_e get_local_cvbs_mode(void);

#endif
