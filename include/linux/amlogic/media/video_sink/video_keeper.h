/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_KEEPER_HEADER___
#define VIDEO_KEEPER_HEADER___

#include <linux/amlogic/media/vfm/vframe.h>

void video_keeper_new_frame_notify(u8 layer_id);
void try_free_keep_video(int flags);
void try_free_keep_vdx(int flags, u8 layer_id);
int video_keeper_init(void);
void video_keeper_exit(void);
unsigned int vf_keep_current
	(struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf2);
unsigned int vf_keep_current_locked
	(u8 layer_id,
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf_el);
#endif
