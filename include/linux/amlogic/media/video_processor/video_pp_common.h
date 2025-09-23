/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_PP_COMMON_H
#define VIDEO_PP_COMMON_H

#include <linux/amlogic/media/vfm/vframe.h>

int get_ge2d_input_format(struct vframe_s *vf);
#ifdef CONFIG_AMLOGIC_VIDEO_COMPOSER
bool get_lowlatency_mode(void);
#endif
#endif /* VIDEO_PP_COMMON_H */

