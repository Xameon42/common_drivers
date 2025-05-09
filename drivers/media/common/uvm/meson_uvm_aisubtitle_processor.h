/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum aisubtitle_get_info_type_e {
	AISUBTITLE_GET_INVALID = 0,
	AISUBTITLE_GET_DATA = 1,
	AISUBTITLE_GET_INDEX_INFO = 2,
	AISUBTITLE_GET_BASIC_INFO = 3,
};

/*hwc attach aisubtitle info*/
struct uvm_aisubtitle_info {
	s32 shared_fd;
	s32 aisubtitle_fd;
	s32 need_do_aisubtitle;
	s32 repeat_frame;
	s32 dw_width;
	s32 dw_height;
	s32 nn_input_frame_width;
	s32 nn_input_frame_height;
	s32 frame_index;
	s32 is_secure_source;
	s32 get_info_type;
	s32 out_area;
	s32 reserved[5];
};

int attach_aisubtitle_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int aisubtitle_getinfo(void *arg, char *buf);

