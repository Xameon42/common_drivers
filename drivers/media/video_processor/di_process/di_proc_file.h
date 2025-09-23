/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _DI_PROC_FILE_H
#define _DI_PROC_FILE_H

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/meson_uvm_core.h>

extern u32 dp_buf_mgr_print_flag;
extern u32 total_fill_count;

struct file_private_data *di_proc_get_file_private_data(struct file *file_vf, bool alloc_if_null);
void dp_put_file_ext(struct file *file_vf);

#endif /* _DI_PROC_FILE_H */
