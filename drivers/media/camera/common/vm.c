// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>
#include <linux/amlogic/media/ge2d/ge2d_cmd.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include "vm.h"

#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/camera/vmapi.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/ctype.h>
#include <linux/of.h>

#include <linux/sizes.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/list.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <uapi/linux/sched/types.h>

#ifndef CONFIG_AMLOGIC_VM_DISABLE_VIDEOLAYER
#error not support  VM VIDEOLAYER feature any more.
#endif

/*the counter of VM*/
#define VM_MAX_DEVS        2

static struct vm_device_s  *vm_device[VM_MAX_DEVS];

#define RECEIVER_NAME  "vm"


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static int vm_start_task(struct vm_device_s *vdevp);
static void vm_stop_task(struct vm_device_s *vdevp);

/*
 ***********************************************
 *
 *   buffer op for video sink.
 *
 ***********************************************
 */

// peek frame from provider. eg. vdin0
static inline struct vframe_s *vm_vf_peek_from_provider(unsigned int idx)
{
	struct vframe_provider_s *vfp;
	struct vframe_s *vf;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, idx);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	vf  = vfp->ops->peek(vfp->op_arg);
	return vf;
}

// get frame from provider. eg. vdin0
static inline struct vframe_s *vm_vf_get_from_provider(unsigned int idx)
{
	struct vframe_provider_s *vfp;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, idx);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	return vfp->ops->get(vfp->op_arg);
}

static inline void vm_vf_put_to_provider(struct vframe_s *vf,
		unsigned int idx)
{
	struct vframe_provider_s *vfp;
	char name[20];

	sprintf(name, "%s%d", RECEIVER_NAME, idx);
	vfp = vf_get_provider(name);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return;
	vfp->ops->put(vf, vfp->op_arg);
}

static struct vframe_s *local_vf_peek(unsigned int idx)
{
	struct vframe_s *vf = NULL;

	vf = vm_vf_peek_from_provider(idx);
	if (vf) {
		if (vm_device[idx]->vm_skip_count > 0) {
			vm_device[idx]->vm_skip_count--;
			vm_vf_get_from_provider(idx);
			vm_vf_put_to_provider(vf, idx);
			vf = NULL;
		}
	}
	return vf;
}

static struct vframe_s *local_vf_get(unsigned int idx)
{
	return vm_vf_get_from_provider(idx);
}

static void local_vf_put(struct vframe_s *vf, unsigned int idx)
{
	if (vf)
		vm_vf_put_to_provider(vf, idx);
}


/*
 ***********************************************
 *
 *   main task functions.
 *
 ***********************************************
 */

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_S16_YUV422 |
				  (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_S16_YUV422 |
				  (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else
			format =  GE2D_FORMAT_S16_YUV422;
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_M24_NV21 |
				  (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_M24_NV21 |
				  (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else
			format =  GE2D_FORMAT_M24_NV21;
	} else {
		pr_err("unknown input frame fmt. default to YUV420.\n");
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
			format =  GE2D_FORMAT_M24_YUV420 |
				  (GE2D_FMT_M24_YUV420B & (3 << 3));
		else if (vf->type & VIDTYPE_INTERLACE_TOP)
			format =  GE2D_FORMAT_M24_YUV420 |
				  (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		else
			format =  GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

static int get_output_format(int v4l2_format)
{
	int format = GE2D_FORMAT_S24_YUV444;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
		format = GE2D_FORMAT_S16_RGB_565;
		break;
	case V4L2_PIX_FMT_YUV444:
		format = GE2D_FORMAT_S24_YUV444;
		break;
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_YUYV:
		format = GE2D_FORMAT_S16_YUV422;
		break;
	case V4L2_PIX_FMT_BGR24:
		format = GE2D_FORMAT_S24_RGB;
		break;
	case V4L2_PIX_FMT_RGB24:
		format = GE2D_FORMAT_S24_BGR;
		break;
	case V4L2_PIX_FMT_NV12:
		format = GE2D_FORMAT_M24_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = GE2D_FORMAT_M24_NV21;
		break;
	default:
		pr_err("unsupport fmt 0x%x", v4l2_format);
		break;
	}
	return format;
}

static int vm_canvas_config(int canvas_idx, int v4l2_format, int *depth, int width,
			 int height, unsigned int buf)
{
	int canvas = canvas_idx;
	*depth = 16;
	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = canvas_idx & 0xff;
		*depth = 16;
		canvas_config(canvas,
			      (unsigned long)buf,
			      width * 2, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = canvas_idx & 0xff;
		*depth = 24;
		canvas_config(canvas,
			      (unsigned long)buf,
			      width * 3, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		canvas_config(canvas_idx & 0xff,
			      (unsigned long)buf,
			      width, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((canvas_idx & 0xff00) >> 8,
			      (unsigned long)(buf + width * height),
			      width, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas = canvas_idx & 0xffff;
		*depth = 12;
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		canvas_config(canvas_idx & 0xff,
			      (unsigned long)buf,
			      width, height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((canvas_idx & 0xff00) >> 8,
			      (unsigned long)(buf + width * height),
			      width / 2, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config((canvas_idx & 0xff0000) >> 16,
			      (unsigned long)(buf + width * height * 5 / 4),
			      width / 2, height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas = canvas_idx & 0xffffff;
		*depth = 12;
		break;
	default:
		break;
	}
	return canvas;
}

static void vm_cache_flush(struct vm_device_s *devp, uintptr_t buf_start, unsigned int buf_size)
{
	if (devp->pdev) {
		dma_sync_single_for_cpu(&devp->pdev->dev,
				buf_start, buf_size, DMA_FROM_DEVICE);
	}
}

int vm_fill_this_buffer(struct vm_device_s *vdevp)
{
	int ret = 0;
	int canvas_id = 0;
	int depth = 0;
	int buf_size = 0;

	if (!vdevp->task_running) {
		pr_err("vm task not running, maybe provider not registered yet!");
		msleep(30);
		return -1;
	}

	canvas_id = ((vdevp->vm_canvas[0]) |
		   (vdevp->vm_canvas[1] << 8) |
		   (vdevp->vm_canvas[2] << 16));

	vdevp->output_para.canvas_id = vm_canvas_config(canvas_id,
		vdevp->output_para.v4l2_format, &depth,
		vdevp->output_para.width, vdevp->output_para.height, vdevp->output_para.addr);

	vdevp->output_para.bytesperline = (vdevp->output_para.width * depth) >> 3;

	complete(&vdevp->vb_start_sema);

	wait_for_completion(&vdevp->vb_done_sema);

	buf_size = vdevp->output_para.bytesperline * vdevp->output_para.height;
	vm_cache_flush(vdevp,
				vdevp->output_para.addr,
				buf_size);

	return ret;
}

/*
 *for decoder input processing
 *  1. output window should 1:1 as source frame size
 *  2. keep the frame ratio
 */

int vm_ge2d_process(struct vframe_s *vf,
					struct ge2d_context_s *ge2d_context,
					struct config_para_ex_s *ge2d_config,
					struct vm_output_para *output_para)
{
	int src_top, src_left, src_width, src_height;
	struct canvas_s cs0, cs1, cs2;

	int current_mirror = 0;
	int cur_angle = 0;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	current_mirror = output_para->mirror;
	if (current_mirror < 0)
		current_mirror = 0;

	cur_angle = output_para->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	/* data operating. */
	ge2d_config->alu_const_color = 0; /* 0x000000ff; */
	ge2d_config->bitmask_en  = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);

	if (vf->canvas0Addr == (u32)-1) {
		ge2d_config->src_planes[0].addr = vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w = src_width;
		ge2d_config->src_planes[0].h = src_height;
		ge2d_config->src_planes[1].addr = vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w = src_width;
		ge2d_config->src_planes[1].h = src_height / 2;
	} else {
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
	}

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	if (vf->canvas0Addr == (u32)-1) {
		ge2d_config->src_para.mem_type =  CANVAS_ALLOC;
		ge2d_config->src_para.canvas_index = 0;
	} else {
		ge2d_config->src_para.mem_type =  CANVAS_TYPE_INVALID;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}
	ge2d_config->src_para.format = get_input_format(vf);

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = output_para->canvas_id;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.format =
		get_output_format(output_para->v4l2_format) |
		GE2D_LITTLE_ENDIAN;

	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output_para->width;
	ge2d_config->dst_para.height = output_para->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(ge2d_context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(ge2d_context, src_left, src_top,
			   src_width, src_height, 0, 0,
			   output_para->width, output_para->height);

	pr_debug("src fmt 0x%x w %d h %d, dst fmt 0x%x w %d h %d\n",
	 ge2d_config->src_para.format, ge2d_config->src_para.width, ge2d_config->src_para.height,
	 ge2d_config->dst_para.format, ge2d_config->dst_para.width, ge2d_config->dst_para.height);

	return 0;
}

static bool vm_is_vf_available(unsigned int vdin_id)
{
	bool ret = (local_vf_peek(vdin_id) ||
			(!vm_device[vdin_id]->task_running));
	return ret;
}

/* static int reset_frame = 1; */
static int vm_task(void *data)
{
	int ret = 0;
	struct vframe_s *vf;

	struct vm_device_s *devp = (struct vm_device_s *)data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
	struct ge2d_context_s *ge2d_context = create_ge2d_work_queue();
	struct config_para_ex_s ge2d_config;

#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
	struct timeval start;
	struct timeval end;
	unsigned long time_use = 0;
#endif
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
	pr_debug("vm task begin run");

	while (1) {
		ret = wait_for_completion_interruptible(&devp->vb_start_sema);

		if (kthread_should_stop()) {
			break;
		}

		/* wait for frame from vdin provider until 500ms runs out */
		wait_event_interruptible_timeout(devp->frame_ready_q,
						vm_is_vf_available(devp->index),
						msecs_to_jiffies(2000));

		if (!devp->task_running) {
			ret = -1;
			goto vm_exit;
		}

		/* start to convert frame. */
#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
		do_gettimeofday(&start);
#endif
		vf = local_vf_get(devp->index);

		if (vf) {

			vm_ge2d_process(vf, ge2d_context,
				 &ge2d_config, &devp->output_para);

			local_vf_put(vf, devp->index);
#ifdef CONFIG_AMLCAP_LOG_TIME_USEFORFRAMES
			do_gettimeofday(&end);
			time_use = (end.tv_sec - start.tv_sec) * 1000 +
				   (end.tv_usec - start.tv_usec) / 1000;
			pr_debug("step 1, ge2d use: %ldms\n", time_use);
			do_gettimeofday(&start);
#endif

		}
		// complete vb_done_sema
		if (kthread_should_stop()) {
			break;
		}
		complete(&devp->vb_done_sema);
	}
vm_exit:
	complete(&devp->vb_done_sema);
	destroy_ge2d_work_queue(ge2d_context);
	while (!kthread_should_stop()) {
		/*may not call stop, wait..
		 *it is killed by SIGTERM,eixt on
		 *down_interruptible.
		 *if not call stop,this thread
		 *may on do_exit and
		 *kthread_stop may not work good;
		 */
		msleep(20);/*default 10ms*/
	}
	pr_debug("vm task exit");

	return ret;
}

/*
 ***********************************************
 *
 *   init functions.
 *
 ***********************************************
 */

int vm_alloc_canvas(struct vm_device_s *vdevp)
{
	int j;

	if (vdevp->index == 0) {
		for (j = 0; j < MAX_CANVAS_INDEX; j++) {
			memset(&vdevp->vm_canvas[j], -1, sizeof(int));
			vdevp->vm_canvas[j] =
				canvas_pool_map_alloc_canvas("vm0");
			if (vdevp->vm_canvas[j] < 0) {
				pr_err("alloc vm0_canvas %d failed\n", j);
				return -1;
			}
		}
	} else {
		for (j = 0; j < MAX_CANVAS_INDEX; j++) {
			memset(&vdevp->vm_canvas[j], -1, sizeof(int));
			vdevp->vm_canvas[j] =
				canvas_pool_map_alloc_canvas("vm1");
			if (vdevp->vm_canvas[j] < 0) {
				pr_err("alloc vm1_canvas %d failed\n", j);
				return -1;
			}
		}
	}

	return 0;
}

int vm_free_canvas(struct vm_device_s *vdevp)
{
	int i = 0;

	for (i = 0; i < MAX_CANVAS_INDEX; i++) {
		if (vdevp &&
			vdevp->vm_canvas[i] >= 0) {
			canvas_pool_map_free_canvas(vdevp->vm_canvas[i]);
			memset(&vdevp->vm_canvas[i],
				-1, sizeof(int));
		}
	}

	return 0;
}

static int vm_start_task(struct vm_device_s *vdevp)
{
	if (!vdevp->task) {
		pr_debug("create and start vm task\n");
		vdevp->task = kthread_create(vm_task, vdevp, vdevp->name);
		if (IS_ERR(vdevp->task)) {
			pr_err("thread creating error.\n");
			return -1;
		}
		init_waitqueue_head(&vdevp->frame_ready_q);
		wake_up_process(vdevp->task);
	}
	vdevp->task_running = 1;
	return 0;
}

static void vm_stop_task(struct vm_device_s *vdevp)
{
	if (vdevp->task) {
		pr_debug("stop and close vm task\n");
		vdevp->task_running = 0;
		send_sig(SIGTERM, vdevp->task, 1);
		complete(&vdevp->vb_start_sema);
		wake_up_interruptible(&vdevp->frame_ready_q);
		kthread_stop(vdevp->task);
		vdevp->task = NULL;
		pr_debug("stop and close vm task success\n");
	}
}

/*
 **********************************************************************
 *
 * file op init section.
 *
 **********************************************************************
 */

void vm_on_provider_unreg(struct vm_device_s *vdevp)
{
	vm_stop_task(vdevp);
}

void vm_on_provider_reg(struct vm_device_s *vdevp)
{
	vm_start_task(vdevp);
}

// from vdin0 to vm0;
// this is vm0's receiver_event_fun
static int vm_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vm_device_s *vdevp = (struct vm_device_s *)private_data;

	//pr_info("recv event %d\n", type);

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		wake_up_interruptible(&vdevp->frame_ready_q);
		break;
	case VFRAME_EVENT_PROVIDER_START:
		vm_on_provider_reg(vdevp);
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		vm_on_provider_unreg(vdevp);
		break;
	default:
		break;
	}
	return 0;
}

static struct vframe_receiver_op_s vm_vf_receiver = {
	.event_cb = vm_receiver_event_fun
};

static int vm_init_vfm_receiver(struct vm_device_s *vdevp)
{
	int  ret = 0;

	memset(&vdevp->output_para, 0, sizeof(struct vm_output_para));
	sprintf(vdevp->name, "%s%d", RECEIVER_NAME, vdevp->index);
	sprintf(vdevp->vf_provider_name, "%s%d", "vdin", vdevp->index);

	snprintf(vdevp->vfm_map_chain, VM_MAP_NAME_SIZE,
				"%s %s", vdevp->vf_provider_name,
				vdevp->name);
	snprintf(vdevp->vfm_map_id, VM_MAP_NAME_SIZE,
				"vm-map-%d", vdevp->index);
	ret = vfm_map_add(vdevp->vfm_map_id,
		vdevp->vfm_map_chain);
	if (ret < 0) {
		pr_err("vm pipeline map creation failed %s.\n",
			vdevp->vfm_map_id);
		return ret;
	}

	vf_receiver_init(&vdevp->vm_vf_recv, vdevp->name,
			&vm_vf_receiver, vdevp);

	vf_reg_receiver(&vdevp->vm_vf_recv);

	pr_debug("reg receiver success\n");

	return 0;
}

static int vm_uninit_vfm_receiver(struct vm_device_s *vdevp)
{
	vf_unreg_receiver(&vdevp->vm_vf_recv);

	if (vdevp->vfm_map_id[0]) {
		vfm_map_remove(vdevp->vfm_map_id);
		vdevp->vfm_map_id[0] = 0;
	}
	return 0;
}

int vm_get_device_by_idx(struct vm_device_s **vm_dev, int idx)
{
	if (vm_device[idx]) {
		*vm_dev = vm_device[idx];
		return 0;
	}
	pr_err("no vm device found for idx %d", idx);
	return -1;
}
EXPORT_SYMBOL(vm_get_device_by_idx);

/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/

/* for driver. */
static int vm_driver_probe(struct platform_device *pdev)
{
	int ret;
	struct vm_device_s *vdevp;

	pr_debug("vm memory init\n");
	/* malloc vdev */
	vdevp = kmalloc(sizeof(*vdevp), GFP_KERNEL);
	if (!vdevp)
		return -ENOMEM;

	memset(vdevp, 0, sizeof(*vdevp));

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vm_id", &vdevp->index);
		if (ret) {
			pr_err("don't find vm id. default to 0\n");
			vdevp->index = 0;
		}
	}
	vdevp->pdev = pdev;

	vm_device[vdevp->index] = vdevp;

	platform_set_drvdata(pdev, vdevp);

	init_completion(&vdevp->vb_start_sema);
	init_completion(&vdevp->vb_done_sema);

	ret = vm_alloc_canvas(vdevp);
	if (ret != 0) {
		pr_err("alloc vm canvas failed\n");
		return ret;
	}

	ret = vm_init_vfm_receiver(vdevp);
	if (ret != 0)
		return ret;

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_err("of_reserved_mem_device_init failed !\n");
		return ret;
	}
#if defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_DEVICE) || \
	defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) || \
	defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU_ALL)
	pdev->dev.dma_coherent = true;
#endif
	pr_debug("probe success.\n");

	return 0;
}

static int vm_drv_remove(struct platform_device *plat_dev)
{
	struct vm_device_s *vdevp = platform_get_drvdata(plat_dev);

	vm_free_canvas(vdevp);

	vm_uninit_vfm_receiver(vdevp);

	vm_device[vdevp->index] = NULL;

	/* free drvdata */
	platform_set_drvdata(plat_dev, NULL);
	kfree(vdevp);
	return 0;
}

static const struct of_device_id amlogic_vm_dt_match[] = {
	{
		.compatible = "amlogic, vm",
	},
	{},
};

/* general interface for a linux driver .*/
static struct platform_driver vm_drv = {
	.probe  = vm_driver_probe,
	.remove = vm_drv_remove,
	.driver = {
		.name = "vm",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_vm_dt_match,
	}
};

int __init
vm_init_module(void)
{
	int ret = 0;

	pr_debug("register platform driver vm_drv.\n");
	ret = platform_driver_register(&vm_drv);
	if (ret) {
		pr_err("Failed to register vm driver (error=%d\n",
		       ret);
	}
	return ret;
}

void __exit
vm_remove_module(void)
{
	platform_driver_unregister(&vm_drv);
	pr_debug("vm module removed.\n");
}


