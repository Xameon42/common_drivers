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
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>

#include "meson_uvm_lcevc_processor.h"

#define LCEVC_LOG_ERROR 0
#define LCEVC_LOG_DEBUG 1

static int uvm_lcevc_debug_level;
module_param(uvm_lcevc_debug_level, int, 0644);

static int lcevc_print(int debug_flag, const char *fmt, ...)
{
	if ((uvm_lcevc_debug_level & debug_flag) ||
		debug_flag == LCEVC_LOG_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "lcevc:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}

	return 0;
}

static long long phy_addr_from_dmabuff(struct dma_buf *dbuf)
{
	long long phy_addr = 0;
	struct dma_heap *heap = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;

	heap = dma_heap_find(CODECMM_HEAP_NAME);
	if (!heap) {
		lcevc_print(LCEVC_LOG_ERROR, "%s: heap is NULL\n", __func__);
		return 0;
	}
	attach = dma_buf_attach(dbuf, dma_heap_get_dev(heap));
	if (IS_ERR(attach)) {
		lcevc_print(LCEVC_LOG_ERROR, "%s: attach err\n", __func__);
		return 0;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (!table) {
		dma_buf_detach(dbuf, attach);
		lcevc_print(LCEVC_LOG_ERROR, "%s: table is NULL\n", __func__);
		return 0;
	}
	page = sg_page(table->sgl);
	phy_addr = (long long)PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
	if (!phy_addr) {
		lcevc_print(LCEVC_LOG_ERROR, "%s: phy_addr is null\n", __func__);
		return 0;
	}

	return phy_addr;
}

static int lcevc_ioctl_data_2_hook_data
(struct uvm_ioctl_lcevc_data *ioctl_data, struct uvm_lcevc_hook_data *hook_data)
{
	struct dma_buf *dmabuf = NULL;
	long long phy_addr = 0;

	dmabuf = dma_buf_get(ioctl_data->lcevc_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		lcevc_print(LCEVC_LOG_ERROR,
			"%s: Invalid dmabuf\n", __func__);
		return -1;
	}

	phy_addr = phy_addr_from_dmabuff(dmabuf);
	if (phy_addr == 0)
		lcevc_print(LCEVC_LOG_ERROR, "phy_addr_from_dmafd failed, lcevc dma fd = %d",
			ioctl_data->lcevc_fd);
	else
		lcevc_print(LCEVC_LOG_DEBUG,
		"phy_addr_from_dmafd lcevc dma fd = %d, phy_addr = %llx",
			ioctl_data->lcevc_fd, phy_addr);
	dma_buf_put(dmabuf);

	hook_data->lcevc_vframe.frame_type = ioctl_data->residual_plane_info.packing;
	hook_data->lcevc_vframe.width = ioctl_data->residual_plane_info.width;
	hook_data->lcevc_vframe.height = ioctl_data->residual_plane_info.height;
	hook_data->lcevc_vframe.stride = ioctl_data->stride;
	hook_data->lcevc_vframe.y_physical_addr = phy_addr;
	hook_data->lcevc_vframe.uv_physical_addr = 0;
	memcpy(&hook_data->lcevc_vframe.upsample_kernel, &ioctl_data->upsample_kernel,
		sizeof(struct uvm_lcevc_upsample_kernel));
	lcevc_print(LCEVC_LOG_DEBUG,
		"%s:lcevc_vframe width=%d, height=%d, stride=%d, y_physical_addr=%llx\n",
		__func__, hook_data->lcevc_vframe.width, hook_data->lcevc_vframe.height,
		hook_data->lcevc_vframe.stride, hook_data->lcevc_vframe.y_physical_addr);

	return 0;
}

int attach_lcevc_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct uvm_hook_mod *v4l_uhmod = NULL;
	struct uvm_ioctl_lcevc_data *lcevc_info = (struct uvm_ioctl_lcevc_data *)buf;
	struct uvm_handle *handle = NULL;
	struct dma_buf *dmabuf = NULL;
	bool attached = false;
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct uvm_lcevc_hook_data *lcevc_4_attach = NULL;
	struct vframe_s *vf = NULL;
	struct file_private_data *v4l_private_data = NULL;

	lcevc_print(LCEVC_LOG_DEBUG, "%s:shared_fd=%d\n", __func__, shared_fd);
	dmabuf = dma_buf_get(shared_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		lcevc_print(LCEVC_LOG_ERROR,
			"%s: Invalid dmabuf\n", __func__);
		return -EINVAL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);
	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		dmabuf_put_vframe(dmabuf);
	} else if (is_v4l_vf) {
		v4l_uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(v4l_uhmod) || !v4l_uhmod->arg) {
			lcevc_print(LCEVC_LOG_DEBUG, "get vf err: no v4lvideo\n");
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		v4l_private_data = v4l_uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!v4l_private_data)
			lcevc_print(LCEVC_LOG_ERROR, "invalid fd no uvm/v4lvideo\n");
		else
			vf = &v4l_private_data->vf;
	} else {
		lcevc_print(LCEVC_LOG_ERROR,
			"%s:dmabuf is not dec vf, uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	if (vf) {
		vf->type_ext = VIDTYPE_EXT_LCEVC;
		lcevc_print(LCEVC_LOG_DEBUG, "%s add VIDTYPE_EXT_LCEVC\n", __func__);
	} else {
		lcevc_print(LCEVC_LOG_ERROR,
			"%s:got vf from dmabuf failed, uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	handle = dmabuf->priv;
	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_LCEVC);
	if (IS_ERR_OR_NULL(uhmod)) {
		lcevc_4_attach = kzalloc(sizeof(*lcevc_4_attach), GFP_KERNEL);
		lcevc_print(LCEVC_LOG_DEBUG, "%s first attach: %p\n",
			__func__, lcevc_4_attach);
		if (!lcevc_4_attach) {
			lcevc_print(LCEVC_LOG_ERROR,
				"%s:first attach kzalloc lcevc_4_attach failed.\n",
				__func__);
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
		lcevc_ioctl_data_2_hook_data(lcevc_info, lcevc_4_attach);
	} else {
		attached = true;
		lcevc_4_attach = uhmod->arg;
		lcevc_ioctl_data_2_hook_data(lcevc_info, lcevc_4_attach);
	}

	dma_buf_put(dmabuf);

	if (attached)
		return 0;

	mod_info->type = PROCESS_LCEVC;
	mod_info->arg = lcevc_4_attach;
	mod_info->free = lcevc_free_data;
	mod_info->acquire_fence = NULL;
	mod_info->getinfo = lcevc_getinfo;
	mod_info->setinfo = lcevc_setinfo;

	return 0;
}

int lcevc_setinfo(void *arg, char *buf)
{
	struct uvm_ioctl_lcevc_data *lcevc_info_input =  (struct uvm_ioctl_lcevc_data *)buf;
	struct uvm_lcevc_hook_data *lcevc_4_save = (struct uvm_lcevc_hook_data *)arg;

	lcevc_print(LCEVC_LOG_DEBUG, "%s arg:%p, buf:%p\n", __func__, arg, buf);
	if (lcevc_info_input && lcevc_4_save) {
		lcevc_ioctl_data_2_hook_data(lcevc_info_input, lcevc_4_save);
		return 0;
	}

	return -EINVAL;
}

int lcevc_getinfo(void *arg, char *buf)
{
	lcevc_print(LCEVC_LOG_DEBUG, "%s arg:%px, buf:%px\n", __func__, arg, buf);

	return 0;
}

void lcevc_free_data(void *arg)
{
	struct uvm_lcevc_hook_data *hook_data = (struct uvm_lcevc_hook_data *)arg;

	lcevc_print(LCEVC_LOG_DEBUG, "%s arg:%p\n", __func__, arg);
	if (hook_data)
		kfree((u8 *)arg);
}
