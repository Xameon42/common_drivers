// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "[ov5640]:%s:%d: " fmt, __func__, __LINE__

#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/media-bus-format.h>

#include <linux/amlogic/media/camera/aml_cam_info.h>
#include <linux/amlogic/media/mipi/am_mipi_csi2.h>
#include <linux/amlogic/media/camera/vmapi.h>
#include "common/plat_ctrl.h"
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/cpu_version.h>

#include "common/vm.h"
#include "common/aml_video.h"
#include "ov5640.h"

#define OV5640_CAMERA_MODULE_NAME   "ov5640"
#define OV5640_DRIVER_VERSION       "OV5640-COMMON-01-140717"
#define OV5640_VIDEO_NAME           "ov5640_v4l"

static struct vdin_v4l2_ops_s *vops;

static struct aml_camera_i2c_fig_s setting_640_480_2lane_45fps[] = {
	{0x3103, 0x11 }, // system clk from pad bit [1]
	{0x3008, 0x82 }, // sw reset bit[7]
	{0xffff, 0x05 },
	{0x3008, 0x42 }, // sw power down bit[6]
	{0x3103, 0x03 }, // system clk from pll
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x12},
	{0x3036, 0x48},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0xa4},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x08},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x41},
	{0x3821, 0x07},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3800, 0x00},
	{0x3801, 0x10},
	{0x3802, 0x00},
	{0x3803, 0x0e},
	{0x3804, 0x0a},
	{0x3805, 0x2f},
	{0x3806, 0x07},
	{0x3807, 0xa5},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x06},
	{0x380d, 0x40},
	{0x380e, 0x03},
	{0x380f, 0xe8},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x370c, 0x03},
	{0x3a02, 0x03},
	{0x3a03, 0xd8},
	{0x3a08, 0x01},
	{0x3a09, 0xc2},
	{0x3a0a, 0x01},
	{0x3a0b, 0x77},
	{0x3a0e, 0x02},
	{0x3a0d, 0x02},
	{0x3a14, 0x03},
	{0x3a15, 0xd8},
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x4050, 0xff},
	{0x4051, 0x7f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x23},
	{0x501f, 0x01},
	{0x5684, 0x10},
	{0x5685, 0xa0},
	{0x5686, 0x0c},
	{0x5687, 0x78},
	{0x4713, 0x02},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x35},
	{0x460c, 0x22},
	{0x4800, 0x24}, // dis-continue clk mode; warning: not compatible with sm1;
	{0x4837, 0x0d},
	{0x3824, 0x02},
	{0x5000, 0xa7},
	{0x5001, 0xa3},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x88},
	{0x518a, 0x54},
	{0x518b, 0xee},
	{0x518c, 0xb2},
	{0x518d, 0x50},
	{0x518e, 0x34},
	{0x518f, 0x6b},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x05},
	{0x5199, 0xf2},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x23},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x38},
	{0x3a10, 0x2f},
	{0x3a1b, 0x38},
	{0x3a1e, 0x2f},
	{0x3a11, 0x70},
	{0x3a1f, 0x17},
	{0x3008, 0x02}
};

static struct aml_camera_i2c_fig_s OV5640_preview_1080P_script[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0xffff, 0xa},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3800, 0x01},
	{0x3801, 0x50},
	{0x3802, 0x01},
	{0x3803, 0xb2},
	{0x3804, 0x08},
	{0x3805, 0xef},
	{0x3806, 0x05},
	{0x3807, 0xf1},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x09},
	{0x380d, 0xc4},
	{0x380e, 0x04},
	{0x380f, 0x60},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x3a02, 0x04},
	{0x3a03, 0x60},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0e, 0x03},
	{0x3a0d, 0x04},
	{0x3a14, 0x04},
	{0x3a15, 0x60},
	{0x4001, 0x02},
	{0x4004, 0x06},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x45},
	{0x302e, 0x08},
	{0x4300, 0x32},
	{0x501f, 0x00},
	{0x5684, 0x07},
	{0x5685, 0xa0},
	{0x5686, 0x04},
	{0x5687, 0x40},
	{0x4713, 0x02},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x37},
	{0x460c, 0x20},
	{0x4800, 0x24}, // dis-continue clk mode; warning: not compatible with sm1;
	{0x4837, 0x0a},
	{0x3824, 0x04},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
};

/*mipi lane bps : M*/
static struct resolution_param  prev_resolutions[] = {
	{
	.frmsize            = {1920, 1080},
	.active_frmsize     = {1920, 1080},
	.active_fps         = 30,
	.lanes              = 2,
	.bps                = 672,
	.size_type          = SIZE_1920X1080,
	.reg_script         = OV5640_preview_1080P_script,
	.reg_script_nums    = ARRAY_SIZE(OV5640_preview_1080P_script),
	.sensor_out_fmt     = MEDIA_BUS_FMT_UYVY8_2X8,
	},
	{
	.frmsize            = {640, 480},
	.active_frmsize     = {640, 480},
	.active_fps         = 45,
	.lanes              = 2,
	.bps                = 228,
	.size_type          = SIZE_640X480,
	.reg_script         = setting_640_480_2lane_45fps,
	.reg_script_nums    = ARRAY_SIZE(setting_640_480_2lane_45fps),
	.sensor_out_fmt     = MEDIA_BUS_FMT_RBG888_1X24,
	}
};

static int set_flip(struct ov5640_dev_t *dev)
{
	struct i2c_client *client = dev->client;
	unsigned char temp;

	temp = i2c_get_byte(client, 0x3821);
	temp &= 0xf9;
	temp |= dev->cam_info.m_flip << 1 | dev->cam_info.m_flip << 2;
	if ((i2c_put_byte(client, 0x3821, temp)) < 0) {
		pr_err("fail in setting sensor orientation\n");
		return -1;
	}
	temp = i2c_get_byte(client, 0x3820);
	temp &= 0xf9;
	temp |= dev->cam_info.v_flip << 1 | dev->cam_info.v_flip << 2;
	if ((i2c_put_byte(client, 0x3820, temp)) < 0) {
		pr_err("fail in setting sensor orientation\n");
		return -1;
	}

	return 0;
}

static int set_resolution_param(struct ov5640_dev_t *dev,
		struct resolution_param *res_param)
{
	struct i2c_client *client = dev->client;
	int i = 0;

	if (!res_param->reg_script) {
		pr_err("error, resolution reg script is NULL\n");
		return -1;
	}

	pr_debug("res_para->size_type = %d, res_param->frmsize.width = %d, height = %d\n",
		res_param->size_type, res_param->frmsize.width,
		res_param->frmsize.height);

	for (i = 0; i < res_param->reg_script_nums; ++i) {
		if (res_param->reg_script[i].addr == 0xffff) {
			msleep(res_param->reg_script[i].val);
			continue;
		}

		if ((i2c_put_byte(client, res_param->reg_script[i].addr,
			res_param->reg_script[i].val)) < 0) {
			pr_err("fail in setting resolution param.i=%d\n", i);
			break;
		}
	}

	dev->cur_resolution = res_param;

	set_flip(dev);

	return 0;
}

static void power_down_ov5640(struct ov5640_dev_t *dev)
{
	struct i2c_client *client = dev->client;

	i2c_put_byte(client, 0x3022, 0x8);
	i2c_put_byte(client, 0x3008, 0x42);
}

static int ov5640_thread(void *data)
{
	int ret;
	struct ov5640_dev_t *dev = data;
	struct vm_output_para *para = &dev->vm_dev->output_para;

	// must be DC_MEM
	para->v4l2_memory = MAGIC_DC_MEM;

	// set fmt.
	para->v4l2_format = dev->video.f_current.fmt.pix.pixelformat;
	para->width = dev->video.f_current.fmt.pix.width;
	para->height = dev->video.f_current.fmt.pix.height;

	// effection params
	para->zoom = 0; // not supported any more.
	para->ext_canvas = 0;

	set_freezable();
	allow_signal(SIGTERM);

	while (true) {
		struct aml_buffer *b_next_to_fill = NULL;

		if (kthread_should_stop())
			break;

		b_next_to_fill = dev->cur_hw_buffer;

		if (!b_next_to_fill) {
			pr_err("no buf to fill.");
			continue;
		}

		// fill b_current. will block 33ms.
		para->addr = (uintptr_t)b_next_to_fill->addr[AML_PLANE_A];
vm_fill_this_buf:
		ret = vm_fill_this_buffer(dev->vm_dev);
		if (ret) {
			pr_err("vm fill this buffer fail. ret %d", ret);
			if (kthread_should_stop())
				break;

			goto vm_fill_this_buf;
		}
		// swap dev->video.b_current
		dev->video.video_buf_ops.video_irq_handler(&dev->video, 0);
		if (kthread_should_stop())
			break;

	}

	pr_debug("ov5640 thread exit");

	return 0;
}

static int ov5640_start_thread(struct ov5640_dev_t *dev)
{
	int ret = 0;

	if (!dev->kthread) {
		dev->kthread = kthread_run(ov5640_thread, dev, "ov5640");
		if (IS_ERR_OR_NULL(dev->kthread)) {
			pr_err("create 5640 thread failed\n");
			ret =  PTR_ERR(dev->kthread);
			dev->kthread = NULL;
			return ret;
		}

		pr_debug("create thread success\n");
	}

	return 0;
}

static void ov5640_stop_thread(struct ov5640_dev_t *dev)
{
	pr_debug("%s in\n", __func__);

	if (dev->kthread) {
		send_sig(SIGTERM, dev->kthread, 1);
		pr_debug("%s set stop bit\n", __func__);
		kthread_stop(dev->kthread);
		dev->kthread = NULL;
	}

	pr_debug("%s leave\n", __func__);
}

// ======== begin dev ops ==========================

static int ov5640_set_fmt(void *ov5640_dev, struct v4l2_format *fmt)
{
	struct ov5640_dev_t *dev = ov5640_dev;
	struct sensor_info info;
	struct resolution_param *prev_resolution;

	pr_debug("ov5640 set fmt.\n");
	prev_resolution = fmt->fmt.pix.width > 640 ? &prev_resolutions[0] : &prev_resolutions[1];
	info.width = prev_resolution->frmsize.width;
	info.height = prev_resolution->frmsize.height;
	info.bps_m = prev_resolution->bps;
	info.nlanes = prev_resolution->lanes;
	update_sensor_info(info);
	set_resolution_param(dev, prev_resolution);
	return 0;
}

static int ov5640_stream_on(void *ov5640_dev)
{
	struct vdin_parm_s para;
	unsigned int vdin_path;
	int ret = 0;
	struct ov5640_dev_t *dev = ov5640_dev;

	memset(&para, 0, sizeof(para));
	para.port  = TVIN_PORT_MIPI;
	para.fmt = TVIN_SIG_FMT_MAX;

	if (dev->cur_resolution) {
		para.frame_rate = 30;
		para.h_active = dev->cur_resolution->active_frmsize.width;
		para.v_active = dev->cur_resolution->active_frmsize.height;
		para.hs_bp = 0;
		para.vs_bp = 2;
		para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
	} else {
		pr_err("error, cur_resolution_param is NULL\n");
		return -1;
	}
	vdin_path = dev->cam_info.vdin_path;

	pr_debug("h_active=%d; v_active=%d, frame_rate=%d,vdin_path=%d\n",
		para.h_active, para.v_active,
		para.frame_rate, vdin_path);

	para.skip_count =  5;

	// default to uyvy
	para.cfmt = TVIN_YUV422;
	if (dev->cur_resolution->sensor_out_fmt == MEDIA_BUS_FMT_RBG888_1X24)
		para.cfmt = TVIN_RGB444; // override to rgb888

	para.dfmt = TVIN_NV21;
	para.hsync_phase = 1;
	para.vsync_phase = 1;
	para.bt_path = dev->cam_info.bt_path;

	/*config mipi parameter*/
	para.csi_hw_info.settle = dev->cur_resolution->bps;
	para.csi_hw_info.lanes = dev->cur_resolution->lanes;

	pr_debug("tvin stream on.\n");

	if (vops) {
		ret = vops->start_tvin_service(vdin_path, &para);
		if (ret) {
			pr_err("vdin start_tvin_service fail ret %d", ret);
			return ret;
		}
	} else {
		pr_err("vops is NULL. can not start tvin service");
	}

	dev->is_streaming = 1;
	ov5640_start_thread(dev);

	return ret;
}

static int ov5640_stream_off(void *ov5640_dev)
{
	int ret = 0;
	struct ov5640_dev_t *dev = ov5640_dev;

	ov5640_stop_thread(dev);

	if (vops)
		vops->stop_tvin_service(0);

	dev->is_streaming = 0;

	return ret;
}

static int ov5640_cfg_buf(void *ov5640_dev, struct aml_buffer *buff)
{
	struct ov5640_dev_t *dev = ov5640_dev;

	dev->cur_hw_buffer = buff;
	return 0;
}

static int ov5640_open(void *ov5640_dev)
{
	struct ov5640_dev_t *dev = ov5640_dev;
	struct aml_cam_info_s *temp_cam = NULL;

	pr_debug("tgid %d pid %d", current->tgid, current->pid);

	temp_cam = get_aml_cam_info_s();

	if (temp_cam)
		memcpy(&dev->cam_info, temp_cam, sizeof(struct aml_cam_info_s));

	aml_cam_init(&dev->cam_info);

	msleep(20);

	dev->ov5640_have_opened = 1;

	return 0;
}

static int ov5640_close(void *ov5640_dev)
{
	unsigned int vdin_path;
	struct ov5640_dev_t *dev = ov5640_dev;

	if (!dev->ov5640_have_opened) {
		pr_err("not opened, skip close");
		return 0;
	}

	vdin_path = dev->cam_info.vdin_path;

	dev->ov5640_have_opened = 0;

	if (dev->is_streaming) {
		pr_debug("stop_tvin_service vdin_path = %d\n",
				vdin_path);
		if (vops)
			vops->stop_tvin_service(vdin_path);
	}
	power_down_ov5640(dev);

	aml_cam_uninit(&dev->cam_info);

	return 0;
}

static const struct aml_dev_ops ov5640_dev_hw_ops = {
	.dev_open        = ov5640_open,
	.dev_close       = ov5640_close,
	.dev_set_fmt     = ov5640_set_fmt,
	.dev_cfg_buf     = ov5640_cfg_buf,
	.dev_stream_on   = ov5640_stream_on,
	.dev_stream_off  = ov5640_stream_off,
};

// ======== end dev ops ==========================

static int ov5640_video_register(struct ov5640_dev_t *ov5640_dev)
{
	int rtn = 0;
	struct aml_video *video;

	video = &ov5640_dev->video;
	video->id = 0;

	snprintf(video->name, sizeof(video->name), OV5640_VIDEO_NAME);

	video->dev = &ov5640_dev->vm_dev->pdev->dev;

	video->v4l2_dev = &ov5640_dev->v4l2_dev;

	video->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	video->dev_ops = ov5640_dev_hw_ops;

	video->status = AML_OFF;
	video->priv = ov5640_dev;
	video->frm_cnt = 0;

	video->pad.flags = MEDIA_PAD_FL_SINK;

	INIT_LIST_HEAD(&video->head);
	spin_lock_init(&video->buff_list_lock);

	rtn = aml_video_init_ops(video);
	if (rtn)
		pr_err("Failed to init video-%d ops, ret %d\n", video->id, rtn);

	rtn = aml_video_register(video);
	if (rtn)
		pr_err("Failed to register video-%d: %d\n", video->id, rtn);

	return rtn;
}

static void ov5640_video_unregister(struct ov5640_dev_t *ov5640_dev)
{
	aml_video_unregister(&ov5640_dev->video);
}

static int ov5640_probe(struct i2c_client *client)
{
	int ret;

	struct ov5640_dev_t *dev;

	vops = get_vdin_v4l2_ops();

	pr_debug("chip found @ 0x%x (%s)\n",
		client->addr, client->adapter->name);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("ov5640 probe kzalloc failed.\n");
		return -ENOMEM;
	}

	// for multiple instance, should get index from dts;
	dev->index = 0;
	dev->client = client;
	mutex_init(&dev->mutex);

	i2c_set_clientdata(client, dev);

	// part 1 v4l2 device
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
		"%s-%03d", "ov5640", 0);

	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret) {
		pr_err("%s, v4l2 device register failed", __func__);
		kfree(dev);
		return ret;
	}

	// part 3 reg cam_info
	dev->cam_info.version = OV5640_DRIVER_VERSION;
	if (aml_cam_info_reg(&dev->cam_info) < 0)
		pr_err("reg caminfo error\n");

	// part 4 get vm device
	if (vm_get_device_by_idx(&dev->vm_dev, 0) != 0) {
		pr_err("get vm device fail\n");
		return -1;
	}

	// part 5 video register
	if (ov5640_video_register(dev) != 0) {
		pr_err("reg video device fail\n");
		return -1;
	}

	pr_debug("ov5640 probe successful.\n");

	return 0;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct ov5640_dev_t *dev = i2c_get_clientdata(client);

	ov5640_video_unregister(dev);

	aml_cam_info_unreg(&dev->cam_info);

	v4l2_device_unregister(&dev->v4l2_dev);

	kfree(dev);

	return 0;
}

static const struct of_device_id ov5640_of_match[] = {
	{.compatible = "ov5640"},
	{/* sentinel */}};
MODULE_DEVICE_TABLE(of, ov5640_of_match);

static const struct of_device_id ov5640_of_table[] = {
	{.compatible = "ov5640"},
	{ },
};

static struct i2c_driver ov5640_i2c_driver = {
	.probe_new = ov5640_probe,
	.remove = ov5640_remove,

	.driver = {
		.name = "ov5640",
		.of_match_table = of_match_ptr(ov5640_of_match),
	},
};

int ov5640_i2c_driver_init(void)
{
	return i2c_add_driver(&ov5640_i2c_driver);
}

