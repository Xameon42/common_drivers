/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _VIDEO_AMLOGIC_FLASHLIGHT_INCLUDE_
#define _VIDEO_AMLOGIC_FLASHLIGHT_INCLUDE_
struct aml_plat_flashlight_data_s {
	void (*flashlight_on)(void);
	void (*flashlight_off)(void);
};

enum aml_plat_flashlight_status_s {
	FLASHLIGHT_AUTO = 0,
	FLASHLIGHT_ON,
	FLASHLIGHT_OFF,
	FLASHLIGHT_TORCH,
	FLASHLIGHT_RED_EYE,
};

#endif

