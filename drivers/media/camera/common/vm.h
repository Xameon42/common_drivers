/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _VM_INCLUDE__
#define _VM_INCLUDE__

#include <linux/interrupt.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/io-mapping.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/camera/aml_cam_info.h>
#include <linux/amlogic/media/camera/vmapi.h>

/*************************************
 **
 ** macro define
 **
 *************************************/

#define MAX_CANVAS_INDEX 12

#define CANVAS_WIDTH_ALIGN 32

#define VM_MAP_NAME_SIZE 100

#define VM_PROVIDER_NAME_SIZE 10

struct vm_device_s {
	unsigned int  index;
	char          name[20];
	struct platform_device *pdev;

	struct vframe_receiver_s vm_vf_recv;
	struct task_struct *task;
	wait_queue_head_t frame_ready_q;
	int task_running;
	int vm_skip_count;

	struct vm_output_para output_para;
	struct completion vb_start_sema;
	struct completion vb_done_sema;

	char vf_provider_name[VM_PROVIDER_NAME_SIZE];
	char vfm_map_id[VM_MAP_NAME_SIZE];
	char vfm_map_chain[VM_MAP_NAME_SIZE];

	int vm_canvas[MAX_CANVAS_INDEX];
};


int vm_get_device_by_idx(struct vm_device_s **vm_dev, int idx);
int vm_fill_this_buffer(struct vm_device_s *vdevp);


#define MAGIC_SG_MEM    0x17890714
#define MAGIC_DC_MEM    0x0733ac61
#define MAGIC_VMAL_MEM  0x18221223
#define MAGIC_RE_MEM    0x123039dc

#endif /* _VM_INCLUDE__ */

