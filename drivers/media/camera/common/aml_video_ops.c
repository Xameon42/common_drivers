// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <media/v4l2-device.h>

#include "aml_video.h"

// device  == > common video ops ====> v4l2 video interface
// ov5640 ==> aml_video_ops ==> aml_video
// gc2145 ==> aml_video_ops ==> aml_video

#define DEFAULT_VIDEO_NAME "cam_v4l2"

#define DEF_VIDEO_Q_BUFFER_NUM        6
#define DEF_VIDEO_Q_BUFFER_SIZE_MEGA  32

#define VIDEO_CAMERA_MAJOR_VERSION  0
#define VIDEO_CAMERA_MINOR_VERSION  7
#define VIDEO_CAMERA_RELEASE        0
#define VIDEO_CAMERA_VERSION                       \
		(((VIDEO_CAMERA_MAJOR_VERSION) << 16) + \
		((VIDEO_CAMERA_MINOR_VERSION) << 8) + \
		(VIDEO_CAMERA_RELEASE))

// ge2d can convert to these formats.
static const struct aml_format video_support_format[] = {
	{0, 0, 0, V4L2_PIX_FMT_VYUY, 1, 16},
	{0, 0, 0, V4L2_PIX_FMT_UYVY, 1, 16},
	{0, 0, 0, V4L2_PIX_FMT_YVYU, 1, 16},
	{0, 0, 0, V4L2_PIX_FMT_YUYV, 1, 16},
	{0, 0, 0, V4L2_PIX_FMT_BGR24, 1, 24},
	{0, 0, 0, V4L2_PIX_FMT_RGB24, 1, 24},
	{0, 0, 0, V4L2_PIX_FMT_NV21, 1, 12},
	{0, 0, 0, V4L2_PIX_FMT_NV12, 1, 12}
};

static struct v4l2_frmsize_discrete video_support_frmsize[] = {
	{1920, 1080},
	{1280, 720},
	{640,  480},
	{640,  360}
};

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
		dev_err(video->dev, " match fmt failed.\n");
		index = -1;
	}

	return index;
}

//========== begin video buffer ops ==================

static int video_irq_handler(struct aml_video *vd, int status)
{
	unsigned long flags;
	int ret = 0;
	static int drop_frame_count;
	struct aml_buffer *b_current_filled = vd->b_current;
	struct aml_buffer *b_next_to_fill = NULL;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->status != AML_ON) {
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		pr_err("err not stream on\n");
		return 0;
	}

	// step 1: get from free list. next buf to fill.
	b_next_to_fill = list_first_entry_or_null(&vd->head, struct aml_buffer, list);

	// step 2:report buf done &  set new hw addr to fill.
	if (b_next_to_fill) {
		if (b_current_filled) {
			vd->frm_cnt++;
			if ((vd->frm_cnt % 100) == 0) {
				vd->audit_endtime_ms = ktime_to_ms(ktime_get());
				if (vd->audit_endtime_ms > vd->audit_begtime_ms) {
					pr_debug("adap fps - 100 frames %d ms, drop %d frames\n",
						(u32)(vd->audit_endtime_ms - vd->audit_begtime_ms),
						drop_frame_count);
					drop_frame_count = 0;
				}
				vd->audit_begtime_ms = vd->audit_endtime_ms;
			}
			b_current_filled->vb.sequence = vd->frm_cnt;
			b_current_filled->vb.vb2_buf.timestamp = ktime_get_ns();
			b_current_filled->vb.field = V4L2_FIELD_NONE;
			vb2_buffer_done(&b_current_filled->vb.vb2_buf, VB2_BUF_STATE_DONE);
		} else {
			pr_err("free buf. no filled buf.\n");
		}
		vd->b_current = b_next_to_fill;
		list_del(&vd->b_current->list);
		// must cfg current buf to device.
		// if fail or not implemented, pr_err.
		ret = -1;
		if (vd->dev_ops.dev_cfg_buf)
			ret = vd->dev_ops.dev_cfg_buf(vd->priv, vd->b_current);
		if (ret)
			pr_err("dev cfg buf fail. ret %d", ret);

	} else {
		//video_isr_printk("no free buf. drop frame, video id %d\n", vd->id);
		drop_frame_count++;
	}

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	return 0;
}

static int video_buffer_queue_setup(struct aml_video *video,  unsigned int *num_buffers,
			unsigned int *num_planes,
			unsigned int sizes[])
{
/*
	*num_buffers = DEF_VIDEO_Q_BUFFER_NUM;
*/
	while (sizes[0] * *num_buffers > DEF_VIDEO_Q_BUFFER_SIZE_MEGA * 1024 * 1024)
		(*num_buffers)--;

	pr_debug("buffer count %d", *num_buffers);

	return 0;
}

static int video_cfg_buffer(struct aml_video *vd, struct aml_buffer *buff)
{
	int ret = 0;
	unsigned long flags;
	struct aml_buffer *buffer = buff;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->b_current) {
		list_add_tail(&buffer->list, &vd->head);
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		return ret;
	}

	// set vd->b_current.
	vd->b_current = buff;

	if (vd->dev_ops.dev_cfg_buf)
		ret = vd->dev_ops.dev_cfg_buf(vd->priv, vd->b_current);

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	if (ret)
		pr_err("dev cfg buf fail. ret %d", ret);

	return ret;
}

static void video_stream_on(struct aml_video *vd)
{
	int ret = 0;

	pr_debug("stream on");
	if (vd->dev_ops.dev_stream_on)
		ret = vd->dev_ops.dev_stream_on(vd->priv);

	vd->frm_cnt = 0;
	vd->audit_begtime_ms = ktime_to_ms(ktime_get());

	if (ret)
		pr_err("dev stream on fail. ret %d", ret);
	else
		vd->status = AML_ON;
}

static void video_stream_off(struct aml_video *vd)
{
	int ret = 0;

	pr_debug("stream off");
	if (vd->dev_ops.dev_stream_off)
		ret = vd->dev_ops.dev_stream_off(vd->priv);

	if (ret)
		pr_err("dev stream off fail. ret %d", ret);
	else
		vd->status = AML_OFF;
}

static void video_vb2_discard_done(struct vb2_queue *q)
{
	struct vb2_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&q->done_lock, flags);
	list_for_each_entry(vb, &q->done_list, done_entry)
		vb->state = VB2_BUF_STATE_ERROR;
	spin_unlock_irqrestore(&q->done_lock, flags);
}

static void video_flush_buffer(struct aml_video *vd)
{
	unsigned long flags;
	struct aml_buffer *buff;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	list_for_each_entry(buff, &vd->head, list) {
		if (buff->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(&buff->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	INIT_LIST_HEAD(&vd->head);

	if (vd->b_current && vd->b_current->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE)
		vb2_buffer_done(&vd->b_current->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	video_vb2_discard_done(&vd->vb2_q);
	vd->b_current = NULL;

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);
}

struct aml_video_buf_ops buf_ops = {
	.video_irq_handler  = video_irq_handler,
	.video_buf_q_setup  = video_buffer_queue_setup,
	.video_cfg_buffer   = video_cfg_buffer,
	.video_stream_on    = video_stream_on,
	.video_stream_off   = video_stream_off,
	.video_flush_buffer = video_flush_buffer,
};

//========== end video buffer ops ==================

//========== begin video ioctl ops ==================

static int video_enum_framesizes(struct aml_video *video, struct v4l2_frmsizeenum *fsize)
{
	struct v4l2_format temp_fmt;

	if (fsize->index >= ARRAY_SIZE(video_support_frmsize))
		return -EINVAL;

	// only support V4L2_FRMSIZE_TYPE_DISCRETE
	if (fsize->type > 0 && fsize->type != V4L2_FRMSIZE_TYPE_DISCRETE)
		return -EINVAL;

	// if type not specified. override to V4L2_FRMSIZE_TYPE_DISCRETE
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	memset(&temp_fmt, 0, sizeof(temp_fmt));
	temp_fmt.fmt.pix.pixelformat = fsize->pixel_format;

	if (video_match_fmt(video, &temp_fmt) >= 0) {
		fsize->discrete = video_support_frmsize[fsize->index];
		return 0;
	}

	return -EINVAL;
}

static int video_enum_input(struct aml_video *video, struct v4l2_input *inp)
{
	if (inp->index > 1)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	if (inp->index)
		snprintf(inp->name, sizeof(inp->name), "Camera %u", inp->index);
	else
		strlcpy(inp->name, "camera", sizeof(inp->name));

	return 0;
}

static int video_enum_frameintervals(struct aml_video *video, struct v4l2_frmivalenum *fival)
{
	struct v4l2_format temp_fmt;

	// only support 30fps.
	if (fival->index > 0)
		return -EINVAL;

	// only support V4L2_FRMIVAL_TYPE_DISCRETE
	if (fival->type > 0 && fival->type != V4L2_FRMIVAL_TYPE_DISCRETE)
		return -EINVAL;

	// if type not specified. override to V4L2_FRMIVAL_TYPE_DISCRETE
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;

	memset(&temp_fmt, 0, sizeof(temp_fmt));
	temp_fmt.fmt.pix.pixelformat = fival->pixel_format;

	if (video_match_fmt(video, &temp_fmt) >= 0) {
		// format supported. do not care width and height.
		// always 30fps;

		fival->discrete.numerator = 1;
		fival->discrete.denominator = 30;
		return 0;
	}

	return -EINVAL;
}

static int video_querycap(struct aml_video *video, struct v4l2_capability *cap)
{
	strcpy(cap->driver, DEFAULT_VIDEO_NAME);
	strcpy(cap->card, DEFAULT_VIDEO_NAME);

	strlcpy(cap->bus_info, video->v4l2_dev->name, sizeof(cap->bus_info));
	cap->version = VIDEO_CAMERA_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;

	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int video_g_parm(struct aml_video *video, struct v4l2_streamparm *parms)
{
	int rtn = 0;
	struct v4l2_captureparm *cp = &parms->parm.capture;

	pr_debug("vidioc g parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = 30;

	return rtn;
}

static int video_set_format(struct aml_video *video, struct v4l2_format *fmt)
{
	int ret = 0;

	pr_debug("video set fmt. w %d h %d cc %.4s\n",
		fmt->fmt.pix.width, fmt->fmt.pix.height, (char *)&fmt->fmt.pix.pixelformat);

	if (video->dev_ops.dev_set_fmt)
		ret = video->dev_ops.dev_set_fmt(video->priv, fmt);

	if (ret)
		pr_err("dev set fmt fail. ret %d", ret);

	return ret;
}

struct aml_video_vidioc_ops vidioc_ops = {
	.video_enum_framesizes     = video_enum_framesizes,
	.video_enum_input          = video_enum_input,
	.video_enum_frameintervals = video_enum_frameintervals,

	.video_querycap            = video_querycap,
	.video_g_parm              = video_g_parm,
	.video_set_format          = video_set_format,
};

//========== end video ioctl ops ==================

//========== begin video file ops ==================
static int video_file_open(struct aml_video *video)
{
	int ret = 0;

	if (video->dev_ops.dev_open)
		ret = video->dev_ops.dev_open(video->priv);

	if (ret)
		pr_err("dev open fail. ret %d", ret);

	return ret;
}

static int video_file_close(struct aml_video *video)
{
	int ret = 0;

	if (video->dev_ops.dev_close)
		ret = video->dev_ops.dev_close(video->priv);

	if (ret)
		pr_err("dev close fail. ret %d", ret);

	return ret;
}

struct aml_video_file_ops  file_ops = {
	.video_open  = video_file_open,
	.video_close = video_file_close,
};

//========== end video file ops ==================

int aml_video_init_ops(struct aml_video *video)
{
	int ret = 0;

	if (!video->dev_ops.dev_open) {
		ret = -1;
		pr_err("device dev_open is invalid");
	}

	if (!video->dev_ops.dev_close) {
		ret = -1;
		pr_err("device dev_close is invalid");
	}

	if (!video->dev_ops.dev_set_fmt) {
		ret = -1;
		pr_err("device dev_set_fmt is invalid");
	}

	if (!video->dev_ops.dev_cfg_buf) {
		ret = -1;
		pr_err("device dev_cfg_buf is invalid");
	}

	if (!video->dev_ops.dev_stream_on) {
		ret = -1;
		pr_err("device dev_stream_on is invalid");
	}

	if (!video->dev_ops.dev_stream_off) {
		ret = -1;
		pr_err("device dev_stream_off is invalid");
	}

	if (!video->format) {
		video->format = video_support_format;
		video->fmt_cnt = ARRAY_SIZE(video_support_format);
	}

	video->video_file_ops = file_ops;
	video->video_vidioc_ops = vidioc_ops;
	video->video_buf_ops = buf_ops;

	return ret;
}

