/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_VIDEO_H__
#define __AML_VIDEO_H__

#include <media/media-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>

enum aml_plane {
	AML_PLANE_A  = 0,
	AML_PLANE_B = 1,
	AML_PLANE_C = 2,
	AML_PLANE_MAX
};

enum {
	AML_OFF = 0,
	AML_ON,
};

struct aml_format {
	u32 width;
	u32 height;
	u32 code;
	u32 fourcc;
	u32 nplanes;
	u32 bpp;  // bits per pixel
};

struct aml_crop {
	u32 hstart;
	u32 vstart;
	u32 hsize;
	u32 vsize;
};

struct aml_buffer {
	struct vb2_v4l2_buffer vb;
	void *vaddr[AML_PLANE_MAX];
	dma_addr_t addr[AML_PLANE_MAX];
	int flag;
	struct list_head list;
};

struct aml_video;

struct aml_video_file_ops {
	int (*video_open)(struct aml_video *video);
	int (*video_close)(struct aml_video *video);
};

// device related video ioctl ops.
// simplified version of v4l2_ioctl_ops, please add if needed.
struct aml_video_vidioc_ops {
	int (*video_enum_framesizes)(struct aml_video *video, struct v4l2_frmsizeenum *fsize);

	int (*video_enum_input)(struct aml_video *video, struct v4l2_input *inp);

	int (*video_enum_frameintervals)(struct aml_video *video, struct v4l2_frmivalenum *fival);

	int (*video_querycap)(struct aml_video *video, struct v4l2_capability *cap);

	int (*video_g_parm)(struct aml_video *video, struct v4l2_streamparm *parms);

	int (*video_set_format)(struct aml_video *video, struct v4l2_format *f);
};

struct aml_video_buf_ops {
	// implemented by aml_video_ops. use by device.
	int (*video_irq_handler)(struct aml_video *video, int status);

	// used by aml_video
	int (*video_buf_q_setup)(struct aml_video *video, unsigned int *num_buffers,
			unsigned int *num_planes,
			unsigned int sizes[]);

	int (*video_cfg_buffer)(struct aml_video *video, struct aml_buffer *buf);

	void (*video_stream_on)(struct aml_video *video);
	void (*video_stream_off)(struct aml_video *video);

	void (*video_flush_buffer)(struct aml_video *video);
};

// device should implement these functions.
struct aml_dev_ops {
	int (*dev_open)(void *dev);
	int (*dev_close)(void *dev);

	int (*dev_set_fmt)(void *dev, struct v4l2_format *f);

	int (*dev_cfg_buf)(void *dev, struct aml_buffer *buff);

	int (*dev_stream_on)(void *dev);
	int (*dev_stream_off)(void *dev);
};

struct aml_video {
	int id;

	int status;
	char name[32];
	char *bus_info;
	struct device *dev;
	struct v4l2_device *v4l2_dev;
	struct media_pad pad;
	struct vb2_queue vb2_q;
	struct video_device vdev;
	int    input;
	const struct aml_format *format;
	u32 fmt_cnt;
	u32 frm_cnt;
	struct aml_crop acrop;
	struct aml_format afmt;
	struct v4l2_format f_current;
	struct aml_buffer *b_current;
	enum v4l2_buf_type type;

	// implemented by aml_video_ops
	struct aml_video_file_ops    video_file_ops;
	struct aml_video_vidioc_ops  video_vidioc_ops;
	struct aml_video_buf_ops     video_buf_ops;

	// implemented by device
	struct aml_dev_ops  dev_ops;

	// spinlock for list head.
	spinlock_t buff_list_lock;

	// mutex for vdev->lock
	struct mutex lock;

	// mutex for vb2_q->lock
	struct mutex q_lock;

	// mutex for aml video object
	struct mutex user_lock;
	u32   users;

	void *priv;
	struct list_head head;
	int first_frame_logged;

	// just for audit
	s64 audit_begtime_ms;
	s64 audit_endtime_ms;
};

int aml_video_init_ops(struct aml_video *video);
int aml_video_register(struct aml_video *video);
void aml_video_unregister(struct aml_video *video);

#endif /* __AML_VIDEO_H__ */
