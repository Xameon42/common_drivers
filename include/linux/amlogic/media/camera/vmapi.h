/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VM_API_INCLUDE_
#define VM_API_INCLUDE_

struct vm_output_para {
	int width;
	int height;
	int bytesperline;
	int v4l2_format;
	int canvas_id;
	int v4l2_memory;
	int zoom;     /* set -1 as invalid */
	int mirror;   /* set -1 as invalid */
	int angle;
	uintptr_t addr;/*unsigned*/
	unsigned int ext_canvas;
};

struct videobuf_buffer;
struct vb2_buffer;

#endif /* VM_API_INCLUDE_ */

