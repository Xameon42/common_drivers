// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <dev_ion.h>
#include <linux/dma-heap.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include <linux/amlogic/media/video_processor/video_pp_common.h>
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif

#include "meson_uvm_aisubtitle_processor.h"

static struct dma_buf *dmabuf_last;

static int uvm_aisubtitle_debug;
module_param(uvm_aisubtitle_debug, int, 0644);

static int uvm_aisubtitle_dump;
module_param(uvm_aisubtitle_dump, int, 0644);

static int uvm_aisubtitle_skip_height = 1088;
module_param(uvm_aisubtitle_skip_height, int, 0644);

static int uvm_open_aisubtitle;
module_param(uvm_open_aisubtitle, int, 0644);

static int uvm_set_aisubtitle_area;
module_param(uvm_set_aisubtitle_area, int, 0644);

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_NN_DUMP		0X0002

static int aisubtitle_canvas[4] = {-1, -1, -1, -1};
static struct ge2d_context_s *context;

struct ge2d_output_t {
	int width;
	int height;
	u32 format;
	phys_addr_t addr;
};

static int aisubtitle_print(int debug_flag, const char *fmt, ...)
{
	if ((uvm_aisubtitle_debug & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "uvm_aisubtitle:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static struct vframe_s *aisubtitle_get_dw_vf(struct uvm_aisubtitle_info *aisubtitle_info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	int shared_fd = aisubtitle_info->shared_fd;
	int interlace_mode = 0;
	struct vframe_s *dma_di_vf = NULL;
	bool dma_has_di_vf = false;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		aisubtitle_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aisubtitle_print(PRINT_ERROR,
			"%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return NULL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (IS_ERR_OR_NULL(vf)) {
			aisubtitle_print(PRINT_ERROR, "%s: vf is NULL.\n", __func__);
			dma_buf_put(dmabuf);
			return NULL;
		}

		aisubtitle_print(PRINT_OTHER,
			"vf: %d*%d,flag=%x,type=%x\n",
			vf->width,
			vf->height,
			vf->flag,
			vf->type);

		di_vf = vf->vf_ext;
		interlace_mode = vf->type & VIDTYPE_TYPEMASK;

		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_DI);
		if (!IS_ERR_OR_NULL(uhmod)) {
			dma_has_di_vf = true;
			dma_di_vf = (struct vframe_s *)uhmod->arg;
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			aisubtitle_print(PRINT_OTHER,
				"%s: dma_has_di_vf=%d, dma_di_vf=%px\n",
				__func__, dma_has_di_vf, dma_di_vf);
			if (!(dma_has_di_vf && di_vf == dma_di_vf)) {
				aisubtitle_print(PRINT_ERROR,
					"di vf err: dmabuf=%px, uhmod=%px, vf=%px, di_vf=%px, dma_di_vf=%px, frame_index=%d\n",
					dmabuf, uhmod, vf, di_vf, dma_di_vf, vf->frame_index);
				di_vf = NULL;
			}
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			if (interlace_mode != VIDTYPE_PROGRESSIVE) {
				/*for interlace*/
				aisubtitle_print(PRINT_OTHER,
					"use di vf: %d*%d,flag=%x,type=%x\n",
					di_vf->width,
					di_vf->height,
					di_vf->flag,
					di_vf->type);
				vf = di_vf;
			}
		}

		if (dma_has_di_vf)
			uvm_put_hook_mod(dmabuf, VF_PROCESS_DI);
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			aisubtitle_print(PRINT_OTHER, "get dw vf err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return NULL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data) {
			aisubtitle_print(PRINT_ERROR, "invalid fd no uvm/v4lvideo\n");
		} else {
			vf = &file_private_data->vf;
			if (vf->vf_ext)
				vf = vf->vf_ext;
		}
	}
	if (!vf) {
		aisubtitle_print(PRINT_ERROR, "not find vf\n");
		dma_buf_put(dmabuf);
		return NULL;
	}
	aisubtitle_print(PRINT_OTHER, "%s: omx->index: %d, vf: %px, vf_ext: %px.\n",
		__func__, vf->frame_index, vf, vf->vf_ext);
	dma_buf_put(dmabuf);
	return vf;
}

static void free_aisubtitle_data(void *arg)
{
	uvm_set_aisubtitle_area = 0;
	aisubtitle_print(PRINT_ERROR, "%s: free data\n", __func__);
}

static int aisubtitle_setinfo(void *arg, char *buf)
{
	struct uvm_aisubtitle_info *aisubtitle_info = NULL;

	aisubtitle_info = (struct uvm_aisubtitle_info *)buf;
	uvm_set_aisubtitle_area = aisubtitle_info->out_area;
	aisubtitle_print(PRINT_OTHER, "%s:uvm_set_aisubtitle_area=%d\n",
		__func__, uvm_set_aisubtitle_area);

	return 0;
}

int attach_aisubtitle_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct uvm_handle *handle;
	bool attached = false;
	struct uvm_aisubtitle_info *aisubtitle_info = (struct uvm_aisubtitle_info *)buf;
	struct vframe_s *vf = NULL;
	int output_fps, output_pts_inc_scale = 0, output_pts_inc_scale_base = 0;

	aisubtitle_info->need_do_aisubtitle = 1;
	aisubtitle_info->dw_height = 0;
	aisubtitle_info->dw_width = 0;

	if (!uvm_open_aisubtitle) {
		aisubtitle_info->need_do_aisubtitle = 0;
	} else {
		get_output_pcrscr_info(&output_pts_inc_scale, &output_pts_inc_scale_base);
		if (!output_pts_inc_scale_base) {
			aisubtitle_print(PRINT_OTHER, "get output pcrscr info failed.\n");
			output_fps = 0;
		} else {
			output_fps = 90000 * 16 * (u64)output_pts_inc_scale;
			output_fps = div64_u64(output_fps, output_pts_inc_scale_base);
		}
		aisubtitle_print(PRINT_OTHER, "scale: %d, base: %d, output_fps is %d.\n",
			output_pts_inc_scale, output_pts_inc_scale_base, output_fps);
		if (output_fps < 24000) {
			aisubtitle_print(PRINT_OTHER,
				"output_fps more than 60, ai_pq bypass.\n");
			aisubtitle_info->need_do_aisubtitle = 0;
		}

		vf = aisubtitle_get_dw_vf(aisubtitle_info);
		if (IS_ERR_OR_NULL(vf)) {
			aisubtitle_print(PRINT_OTHER, "get no vf\n");
			return -EINVAL;
		}

		aisubtitle_info->dw_height = vf->height;
		aisubtitle_info->dw_width = vf->width;
		aisubtitle_info->frame_index = vf->frame_index;
		if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
			aisubtitle_info->is_secure_source = 1;

		if (vf->width > 3840 ||
		    vf->height > 2160 ||
			vf->flag & VFRAME_FLAG_VIDEO_SECURE ||
		    vf->flag & VFRAME_FLAG_GAME_MODE ||
		    vf->flag & VFRAME_FLAG_PC_MODE ||
		    vf->canvas0_config[0].bit_depth & P010_MODE) {
			aisubtitle_print(PRINT_OTHER, "bypass %d %d\n",
				vf->width, vf->height);
			aisubtitle_info->need_do_aisubtitle = 0;
		}
	}

	dmabuf = dma_buf_get(shared_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		aisubtitle_print(PRINT_ERROR,
			"Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		aisubtitle_print(PRINT_ERROR,
			"attach:dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	handle = dmabuf->priv;
	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_AISUBTITLE);
	if (IS_ERR_OR_NULL(uhmod)) {
		aisubtitle_print(PRINT_OTHER, "attach:first attach\n");
	} else {
		uvm_put_hook_mod(dmabuf, PROCESS_AISUBTITLE);
		attached = true;
	}

	dma_buf_put(dmabuf);

	if (dmabuf_last == dmabuf) {
		aisubtitle_info->repeat_frame = 1;
	} else {
		aisubtitle_info->repeat_frame = 0;
		dmabuf_last = dmabuf;
	}

	if (attached)
		return 0;

	info->type = PROCESS_AISUBTITLE;
	info->arg = NULL;
	info->free = free_aisubtitle_data;
	info->acquire_fence = NULL;
	info->getinfo = aisubtitle_getinfo;
	info->setinfo = aisubtitle_setinfo;

	return 0;
}

static int get_canvas(u32 index)
{
	const char *owner = "aisubtitle";

	if (aisubtitle_canvas[index] < 0)
		aisubtitle_canvas[index] = canvas_pool_map_alloc_canvas(owner);

	if (aisubtitle_canvas[index] < 0)
		aisubtitle_print(PRINT_ERROR, "no canvas\n");
	return aisubtitle_canvas[index];
}

static int ge2d_vf_process(struct vframe_s *vf, struct ge2d_output_t *output)
{
	struct config_para_ex_s ge2d_config_s;
	struct config_para_ex_s *ge2d_config = &ge2d_config_s;
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode, src_format;
	int input_width, input_height;
	u32 output_canvas = get_canvas(3);
	bool is_tvp = false;

	if (!context) {
		context = create_ge2d_work_queue();
		if (IS_ERR_OR_NULL(context)) {
			aisubtitle_print(PRINT_ERROR, "%s: creat ge2d work failed\n", __func__);
			return -1;
		}
	}

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	input_width = vf->width;
	input_height = vf->height;
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(get_canvas(0),
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(get_canvas(1),
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(get_canvas(1),
					&vf->canvas0_config[1]);
			canvas_config_config(get_canvas(2),
					&vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			get_canvas(0)
			| (get_canvas(1) << 8)
			| (get_canvas(2) << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}
#ifdef CONFIG_AMLOGIC_VIDEO_COMPOSER
	src_format = get_ge2d_input_format(vf);
#endif
	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM ||
	    interlace_mode == VIDTYPE_INTERLACE_TOP) {
		input_height >>= 1;
	} else if (vf->height > uvm_aisubtitle_skip_height) {
		/*used to reduce bandwidth by change format to interlace*/
		aisubtitle_print(PRINT_OTHER, "use interlace format.\n");
		input_height >>= 1;
		src_format |= (GE2D_FMT_M24_YUV420T & (3 << 3));
	}

	ge2d_config->src_para.format = src_format;
	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
		is_tvp = true;

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	aisubtitle_print(PRINT_OTHER, "src width: %d, height: %d format =%x\n",
		input_width, input_height, ge2d_config->src_para.format);

	canvas_config(output_canvas, output->addr, output->width * 3,
		      output->height, CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	canvas_read(output_canvas & 0xff, &cd);

	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = input_width;
	ge2d_config->src_para.height = input_height;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->mem_sec = is_tvp;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = output->format | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;
	ge2d_config->dst_xy_swap = 0;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		aisubtitle_print(PRINT_ERROR,
			      "++ge2d configing error.\n");
		return -1;
	}

	stretchblt_noalpha(context, 0, 0, input_width, input_height,
			   0, 0, output->width, output->height);

	return 0;
}

static void dump_vf(struct vframe_s *vf, phys_addr_t addr,
	struct uvm_aisubtitle_info *info, int num)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp;
	char name_buf[32];
	int write_size;
	int write_size_uv;
	u8 *data;
	u8 *data_uv;
	loff_t pos;

	if (IS_ERR_OR_NULL(vf) || IS_ERR_OR_NULL(info)) {
		aisubtitle_print(PRINT_ERROR, "dump param invalid.\n");
		return;
	}

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		aisubtitle_print(PRINT_ERROR, "%s: security vf.\n", __func__);
		return;
	}
	snprintf(name_buf, sizeof(name_buf), "/data/aisubtitle_ge2dOut_%d.rgb", num);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		aisubtitle_print(PRINT_ERROR, "open file path %s failed.\n", name_buf);
		return;
	}
	write_size = info->nn_input_frame_height * info->nn_input_frame_width * 3;
	data = codec_mm_vmap(addr, write_size);
	if (!data)
		return;
	pos = 0;
	kernel_write(fp, data, write_size, &pos);
	aisubtitle_print(PRINT_NN_DUMP, "aisubtitle: write %u size to addr%p\n",
		write_size, data);
	aisubtitle_print(PRINT_NN_DUMP, "aisubtitle: ge2dout w %d, h %d.\n",
		info->nn_input_frame_width, info->nn_input_frame_height);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);

	snprintf(name_buf, sizeof(name_buf), "/data/aisubtitle_dec_%d.yuv", num);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height;
	write_size_uv = vf->canvas0_config[1].width * vf->canvas0_config[1].height / 2;
	data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, write_size_uv);
	if (!data || !data_uv) {
		aisubtitle_print(PRINT_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}
	pos = 0;
	kernel_write(fp, data, write_size, &pos);
	kernel_write(fp, data_uv, write_size_uv, &pos);
	aisubtitle_print(PRINT_NN_DUMP, "aisubtitle: write y data %u size from addr%p\n",
		write_size, data);
	aisubtitle_print(PRINT_NN_DUMP, "aisubtitle: write uv data %u size from addr%p\n",
		write_size_uv, data_uv);
	aisubtitle_print(PRINT_NN_DUMP, "aisubtitle: yuv w %d, h %d.\n",
		vf->canvas0_config[0].width, vf->canvas0_config[0].height);
	codec_mm_unmap_phyaddr(data);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

int aisubtitle_getinfo(void *arg, char *buf)
{
	struct uvm_aisubtitle_info *aisubtitle_info = NULL;
	int ret = -1;
	phys_addr_t *phy_addr = 0;
	struct vframe_s *vf = NULL;
	s32 aisubtitle_fd;
	struct ge2d_output_t output;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;
	struct dma_buf *dbuf = NULL;
	struct dma_heap *heap = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	static int dump_count;

	aisubtitle_info = (struct uvm_aisubtitle_info *)buf;

	if (aisubtitle_info->get_info_type == AISUBTITLE_GET_DATA) {
		vf = aisubtitle_get_dw_vf(aisubtitle_info);
		if (IS_ERR_OR_NULL(vf)) {
			aisubtitle_print(PRINT_ERROR, "get no vf\n");
			return -EINVAL;
		}

		aisubtitle_fd = aisubtitle_info->aisubtitle_fd;
		if (aisubtitle_fd != -1) {
			dbuf = dma_buf_get(aisubtitle_fd);
			if (IS_ERR_OR_NULL(dbuf)) {
				pr_err("%s: dbuf: dbuf=%px\n", __func__, dbuf);
				return -EINVAL;
			}
			heap = dma_heap_find(CODECMM_HEAP_NAME);
			if (!heap) {
				dma_buf_put(dbuf);
				pr_err("%s: heap is NULL\n", __func__);
				return -EINVAL;
			}
			attach = dma_buf_attach(dbuf, dma_heap_get_dev(heap));
			if (IS_ERR(attach)) {
				dma_buf_put(dbuf);
				pr_err("%s: attach err\n", __func__);
				return -EINVAL;
			}
			table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
			if (!table) {
				dma_buf_detach(dbuf, attach);
				dma_buf_put(dbuf);
				pr_err("%s: table is NULL\n", __func__);
				return -ENOMEM;
			}
			page = sg_page(table->sgl);
			phy_addr = (void *)PFN_PHYS(page_to_pfn(page));
			dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
			dma_buf_detach(dbuf, attach);
			dma_buf_put(dbuf);
			if (!phy_addr) {
				pr_info("%s: phy_addr is null\n", __func__);
				return -ENOMEM;
			}
		}
		aisubtitle_info->frame_index = vf->frame_index;
		memset(&output, 0, sizeof(struct ge2d_output_t));
		output.width = aisubtitle_info->nn_input_frame_width;
		output.height = aisubtitle_info->nn_input_frame_height;
		output.format = GE2D_FORMAT_S24_BGR;
		output.addr = (ulong)phy_addr;

		do_gettimeofday(&begin_time);
		ret = ge2d_vf_process(vf, &output);
		if (ret < 0) {
			aisubtitle_print(PRINT_ERROR, "ge2d output RGB24 err\n");
			return -EINVAL;
		}
		do_gettimeofday(&end_time);
		cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
			+ (end_time.tv_usec - begin_time.tv_usec)) / 1000;
		aisubtitle_print(PRINT_OTHER, "ge2d cost: %d ms\n", cost_time);
		if (uvm_aisubtitle_dump) {
			if (dump_count < uvm_aisubtitle_dump) {
				dump_vf(vf, (ulong)phy_addr, aisubtitle_info, dump_count + 1);
				dump_count++;
			} else {
				aisubtitle_print(PRINT_ERROR, "finish dump %d vframe.\n",
					uvm_aisubtitle_dump);
				uvm_aisubtitle_dump = 0;
				dump_count = 0;
			}
		}
	}
	return 0;
}

