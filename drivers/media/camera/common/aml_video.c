// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)  "aml-video:%s:%d: " fmt, __func__, __LINE__

#include <linux/version.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "aml_video.h"

#define  CAM_VIDEO_IDX_BEGIN_NUM  50

#define VB2_MIN_BUFFER_COUNT 3

// match success. return matched index;
// match fail. return -1
static int video_match_fmt(struct aml_video *video, struct v4l2_format *fmt)
{
	int index = 0;
	int found = 0;

	for (index = 0; index < video->fmt_cnt; index++) {
		if (fmt->fmt.pix.pixelformat == video->format[index].fourcc) {
			found = 1;
			break;
		}
	}

	if (!found) {
		dev_err(video->dev, "match fmt failed.\n");
		index = -1;
	}

	return index;
}

static void video_init_fmt(struct aml_video *video)
{
	struct v4l2_format fmt;

	memset(&fmt, 0, sizeof(fmt));

	fmt.type = video->type;
	fmt.fmt.pix.width = 1920;
	fmt.fmt.pix.height = 1080;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	fmt.fmt.pix.pixelformat = video->format[0].fourcc;
	fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	fmt.fmt.pix.sizeimage = fmt.fmt.pix.width *
					fmt.fmt.pix.height *
					video->format[0].bpp / 8;

	video->f_current = fmt;
}

//====== begin v4l2_ioctl_ops ====================

static int video_querycap(struct file *file, void *fh,
			struct v4l2_capability *cap)
{
	int ret = 0;
	struct aml_video *video = video_drvdata(file);

	if (video->video_vidioc_ops.video_querycap)
		ret = video->video_vidioc_ops.video_querycap(video, cap);

	return ret;
}

static int video_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *fmt)
{
	struct aml_video *video = video_drvdata(file);

	if (fmt->type != video->type || fmt->index >= video->fmt_cnt)
		return -EINVAL;

	fmt->pixelformat = video->format[fmt->index].fourcc;

	dev_dbg(video->dev, "enum fmt %.4s\n", (char *)&fmt->pixelformat);

	return 0;
}

static int video_get_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct aml_video *video = video_drvdata(file);

	dev_dbg(video->dev, "video get fmt\n");

	*fmt = video->f_current;

	return 0;
}

static inline u32 get_bytesperline(u32 fourcc, u32 bpp, u32 width)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		return width;
	default:
		return (width * bpp) >> 3;
	}
}

static int video_set_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	int idx = 0;

	struct aml_video *video = video_drvdata(file);

	dev_dbg(video->dev, "video set fmt\n");

	if (vb2_is_busy(&video->vb2_q))
		return -EBUSY;

	idx = video_match_fmt(video, fmt);
	if (idx < 0) {
		dev_err(video->dev, " match fmt failed. reset to idx 0\n");
		idx = 0;
	}

	fmt->fmt.pix.sizeimage = fmt->fmt.pix.width *
					fmt->fmt.pix.height *
					video->format[idx].bpp / 8;
	fmt->type = video->type;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.pixelformat = video->format[idx].fourcc;
	fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	fmt->fmt.pix.bytesperline = get_bytesperline(video->format[idx].fourcc,
		video->format[idx].bpp, fmt->fmt.pix.width);

	video->f_current = *fmt;
	video->afmt = video->format[idx];

	if (video->video_vidioc_ops.video_set_format)
		video->video_vidioc_ops.video_set_format(video, fmt);

	return 0;
}

static int video_try_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	int idx = 0;
	struct aml_video *video = video_drvdata(file);

	idx = video_match_fmt(video, fmt);
	if (idx < 0) {
		pr_warn("Fourcc format (0x%08x) invalid.\n",
				fmt->fmt.pix.pixelformat);
		return -EINVAL;
	}

	fmt->fmt.pix.sizeimage = fmt->fmt.pix.width *
					fmt->fmt.pix.height *
					video->format[idx].bpp / 8;
	fmt->type = video->type;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.pixelformat = video->format[idx].fourcc;
	fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	fmt->fmt.pix.bytesperline = get_bytesperline(video->format[idx].fourcc,
		video->format[idx].bpp, fmt->fmt.pix.width);

	return 0;
}

int video_ioctl_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	return vb2_ioctl_qbuf(file, priv, p);
}

int video_ioctl_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct aml_video *video = video_drvdata(file);

	int ret = 0x0;

	if (video->first_frame_logged == 0)
		dev_err(video->dev, "video %d beg dq first 1 frame\n", video->id);

	ret = vb2_ioctl_dqbuf(file, priv, p);

	if (video->first_frame_logged == 0) {
		dev_err(video->dev, "video %d end dq first 1 frame\n", video->id);
		video->first_frame_logged = 1;
	}

	return ret;
}

static int video_ioctl_enum_input(struct file *file, void *fh, struct v4l2_input *inp)
{
	int ret = 0;

	struct aml_video *video = video_drvdata(file);

	if (video->video_vidioc_ops.video_enum_input)
		ret = video->video_vidioc_ops.video_enum_input(video, inp);

	return ret;
}

static int video_g_selection(struct file *file, void *fh, struct v4l2_selection *slt)
{
	struct aml_video *video = video_drvdata(file);
	struct aml_crop *crop = &video->acrop;

	slt->r.left = crop->hstart;
	slt->r.top = crop->vstart;
	slt->r.width = crop->hsize;
	slt->r.height = crop->vsize;

	return 0;
}

static int video_s_selection(struct file *file, void *fh, struct v4l2_selection *slt)
{
	struct aml_video *video = video_drvdata(file);
	struct aml_crop *crop = &video->acrop;

	crop->hstart = slt->r.left;
	crop->vstart = slt->r.top;
	crop->hsize = slt->r.width;
	crop->vsize = slt->r.height;

	return 0;
}

static int video_g_input(struct file *file, void *fh, unsigned int *input)
{
	struct aml_video *video = video_drvdata(file);

	*input = video->input;
	return 0;
}

static int video_s_input(struct file *file, void *priv, unsigned int input)
{
	struct aml_video *video = video_drvdata(file);

	video->input = input;

	return 0;
}

static int video_ioctl_enum_framesizes(struct file *file, void *priv,
	struct v4l2_frmsizeenum *fsize)
{
	int ret = 0;

	struct aml_video *video = video_drvdata(file);

	if (video->video_vidioc_ops.video_enum_framesizes)
		ret = video->video_vidioc_ops.video_enum_framesizes(video, fsize);

	return ret;
}

static int video_ioctl_enum_frameintervals(struct file *file, void *priv,
	struct v4l2_frmivalenum *fival)
{
	int ret = 0;

	struct aml_video *video = video_drvdata(file);

	if (video->video_vidioc_ops.video_enum_frameintervals)
		ret = video->video_vidioc_ops.video_enum_frameintervals(video, fival);

	return ret;
}

static int video_ioctl_g_param(struct file *file, void *priv, struct v4l2_streamparm *parms)
{
	int ret = 0;

	struct aml_video *video = video_drvdata(file);

	if (video->video_vidioc_ops.video_g_parm)
		ret = video->video_vidioc_ops.video_g_parm(video, parms);

	return ret;
}

static const struct v4l2_ioctl_ops aml_v4l2_ioctl_ops = {
	.vidioc_querycap          = video_querycap,
	.vidioc_enum_fmt_vid_cap  = video_enum_fmt,
	.vidioc_g_fmt_vid_cap     = video_get_fmt,
	.vidioc_s_fmt_vid_cap     = video_set_fmt,
	.vidioc_try_fmt_vid_cap   = video_try_fmt,
	.vidioc_reqbufs           = vb2_ioctl_reqbufs,
	.vidioc_querybuf          = vb2_ioctl_querybuf,
	.vidioc_qbuf              = video_ioctl_qbuf,
	.vidioc_expbuf            = vb2_ioctl_expbuf,
	.vidioc_dqbuf             = video_ioctl_dqbuf,
	.vidioc_enum_input        = video_ioctl_enum_input,
	.vidioc_enum_framesizes   = video_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals = video_ioctl_enum_frameintervals,
	.vidioc_g_parm              = video_ioctl_g_param,
	.vidioc_prepare_buf       = vb2_ioctl_prepare_buf,
	.vidioc_create_bufs       = vb2_ioctl_create_bufs,
	.vidioc_g_selection       = video_g_selection,
	.vidioc_s_selection       = video_s_selection,
	.vidioc_streamon          = vb2_ioctl_streamon,
	.vidioc_streamoff         = vb2_ioctl_streamoff,
	.vidioc_subscribe_event   = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_g_input           = video_g_input,
	.vidioc_s_input           = video_s_input,
};

//====== end v4l2_ioctl_ops ====================

//====== begin vb2_ops ====================

static int video_buff_queue_setup(struct vb2_queue *queue,
			unsigned int *num_buffers,
			unsigned int *num_planes,
			unsigned int sizes[],
			struct device *alloc_devs[])
{
	struct aml_video *video = queue->drv_priv;
	const struct v4l2_pix_format *pix = &video->f_current.fmt.pix;

	dev_dbg(video->dev, "%s ++\n", __func__);

	if (*num_planes) {
		if (sizes[0] < pix->sizeimage)
			return -EINVAL;
	}

	*num_planes = 1;
	sizes[0] = pix->sizeimage;

	queue->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;

	if (video->video_buf_ops.video_buf_q_setup)
		video->video_buf_ops.video_buf_q_setup(video, num_buffers, num_planes, sizes);

	return 0;
}

static void video_buff_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);

	if (video->video_buf_ops.video_cfg_buffer)
		video->video_buf_ops.video_cfg_buffer(video, buff);
}

static int video_buff_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);
	u32 size = video->f_current.fmt.pix.sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		dev_err(video->dev,
			"Error user buffer too small (%ld < %u)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);

	vbuf->field = V4L2_FIELD_NONE;

	return 0;
}

static int video_buff_init(struct vb2_buffer *vb)
{
	u32 p_size = 0;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	const struct v4l2_pix_format *pix = &video->f_current.fmt.pix;

	buff->addr[AML_PLANE_A] = *((u32 *)vb2_plane_cookie(vb, 0));
	buff->vaddr[AML_PLANE_A] = vb2_plane_vaddr(vb, 0);

	dev_dbg(video->dev,
		"%s ++ , addr 0x%x, vaddr 0x%px\n", __func__, (u32)buff->addr[AML_PLANE_A],
		buff->vaddr[AML_PLANE_A]);

	if (pix->pixelformat == V4L2_PIX_FMT_NV12 ||
			pix->pixelformat == V4L2_PIX_FMT_NV21) {
		p_size = pix->bytesperline * pix->height;
		buff->addr[AML_PLANE_B] = buff->addr[AML_PLANE_A] + p_size;
	}

	return 0;
}

static int video_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct aml_video *video = vb2_get_drv_priv(queue);

	if (video->video_buf_ops.video_stream_on)
		video->video_buf_ops.video_stream_on(video);

	return 0;
}

static void video_stop_streaming(struct vb2_queue *queue)
{
	struct aml_video *video = vb2_get_drv_priv(queue);

	if (video->video_buf_ops.video_stream_off)
		video->video_buf_ops.video_stream_off(video);

	if (video->video_buf_ops.video_flush_buffer)
		video->video_buf_ops.video_flush_buffer(video);
}

//====== end vb2_ops ====================

static struct vb2_ops aml_vb2_ops = {
	.queue_setup = video_buff_queue_setup,
	.buf_queue = video_buff_queue,
	.buf_prepare = video_buff_prepare,
	.buf_init = video_buff_init,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = video_start_streaming,
	.stop_streaming = video_stop_streaming,
};

static int video_fh_open(struct file *filp)
{
	int ret = 0;
	struct v4l2_fh *fh = NULL;
	struct aml_video *video = video_drvdata(filp);

	pr_debug("open by tgid %d pid %d", current->tgid, current->pid);
	mutex_lock(&video->user_lock);
	if (video->users > 0) {
		pr_err("has beed opened. exit");
		mutex_unlock(&video->user_lock);
		return -EINVAL;
	}
	video->users++;
	mutex_unlock(&video->user_lock);

	ret = v4l2_fh_open(filp);
	if (ret != 0) {
		pr_err("v4l2_fh_open fails, ret %d", ret);
		return ret;
	}

	fh = filp->private_data;

	pr_debug("call dev file open");
	if (video->video_file_ops.video_open)
		ret = video->video_file_ops.video_open(video);

	pr_debug("dev file open, ret %d\n", ret);
	return ret;
}

static int video_fh_release(struct file *filp)
{
	int ret = 0;
	struct aml_video *video = video_drvdata(filp);

	pr_debug("dev file close, user count %d", video->users);
	mutex_lock(&video->user_lock);
	video->users--;
	mutex_unlock(&video->user_lock);

	if (video->video_file_ops.video_close)
		video->video_file_ops.video_close(video);

	// vb2_fop_release will call v4l2_fh_release
	ret = vb2_fop_release(filp);
	if (ret != 0)
		pr_err("v4l2_fh_release fails, ret %d", ret);

	return ret;
}

int video_fh_mmap(struct file *file, struct vm_area_struct *vma)
{
	pr_debug("map buffer");
	return vb2_fop_mmap(file, vma);
}

static const struct v4l2_file_operations aml_v4l2_fops = {
	.owner  = THIS_MODULE,
	.open = video_fh_open,
	.release = video_fh_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = video_fh_mmap,
	.read = vb2_fop_read,
};

static void aml_video_release(struct video_device *vdev)
{
	struct aml_video *video = video_get_drvdata(vdev);

	media_entity_cleanup(&vdev->entity);

	mutex_destroy(&video->q_lock);
	mutex_destroy(&video->lock);
}

int aml_video_register(struct aml_video *video)
{
	int rtn = 0;
	struct video_device *vdev;
	struct vb2_queue *vb2_q;

	vdev = &video->vdev;

	mutex_init(&video->user_lock);

	mutex_init(&video->q_lock);

	mutex_init(&video->lock);

	vb2_q = &video->vb2_q;
	vb2_q->drv_priv = video;
	vb2_q->mem_ops = &vb2_dma_contig_memops;
	vb2_q->ops = &aml_vb2_ops;
	vb2_q->type = video->type;
	vb2_q->io_modes = VB2_DMABUF | VB2_MMAP | VB2_READ;
	vb2_q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vb2_q->buf_struct_size = sizeof(struct aml_buffer);
	vb2_q->dev = video->dev;
	vb2_q->lock = &video->q_lock;
	vb2_q->min_buffers_needed = VB2_MIN_BUFFER_COUNT;
	rtn = vb2_queue_init(vb2_q);
	if (rtn < 0) {
		dev_err(video->dev, "Failed to init vb2 queue: %d\n", rtn);
		goto error_vb2_init;
	}

	video_init_fmt(video);

	vdev->fops = &aml_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE |
			V4L2_CAP_STREAMING |
			V4L2_CAP_READWRITE;

	vdev->ioctl_ops = &aml_v4l2_ioctl_ops;
	vdev->release = aml_video_release;
	vdev->v4l2_dev = video->v4l2_dev;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->queue = &video->vb2_q;
	vdev->lock = &video->lock;
	vdev->tvnorms = V4L2_STD_525_60,

	strncpy(vdev->name, video->name, sizeof(vdev->name));

	rtn = video_register_device(vdev, VFL_TYPE_VIDEO, CAM_VIDEO_IDX_BEGIN_NUM);

	if (rtn < 0) {
		dev_err(video->dev, "Failed to register video device: %d\n", rtn);
		goto error_video_register;
	}

	video_set_drvdata(vdev, video);

	return 0;

error_video_register:
	vb2_queue_release(&video->vb2_q);
error_vb2_init:
	mutex_destroy(&video->lock);
	mutex_destroy(&video->q_lock);
	mutex_destroy(&video->user_lock);

	return rtn;
}

void aml_video_unregister(struct aml_video *video)
{
	vb2_queue_release(&video->vb2_q);
	video_unregister_device(&video->vdev);
}

