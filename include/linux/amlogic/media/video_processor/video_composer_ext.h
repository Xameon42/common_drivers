/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_COMPOSER_EXT_H
#define VIDEO_COMPOSER_EXT_H

#include <linux/amlogic/media/vfm/vframe.h>

struct composer_dst {
	dma_addr_t phy_addr;
	u32 buffer_width;
	u32 buffer_height;
	u32 content_left;
	u32 content_top;
	u32 content_w;
	u32 content_h;
	u32 format;
};

#endif /* VIDEO_COMPOSER_EXT_H */

