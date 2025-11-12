// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <media/aml_demux_ext.h>
#include "dma_buf_manage.h"

#define dprintk(level, fmt, arg...)						\
	do {									\
		if (dmabuf_manage_debug >= (level))						\
			pr_info("demux_dmabuf_manage: %s: " fmt, __func__, ## arg);\
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)
#define pr_enter() pr_inf("enter")

struct kdmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

struct dmabuf_manage_block {
	__u64 paddr;
	__u32 size;
	__u32 handle;
	struct dmx_dma_buf_sec_es_data dmxes;
	void *dmx;
};

MODULE_PARM_DESC(dmabuf_manage_debug, "\n\t\t set dma buf debug level");
static int dmabuf_manage_debug = 1;
module_param(dmabuf_manage_debug, int, 0644);

static int dmabuf_manage_attach(struct dma_buf *dbuf, struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach;
	struct dmabuf_manage_block *block = NULL;
	struct sg_table *sgt;
	struct page *page;
	phys_addr_t phys;
	int ret;
	int sgnum = 1;
	struct dmx_dma_buf_sec_es_data *es = (struct dmx_dma_buf_sec_es_data *)dbuf->priv;
	int len = 0;

	pr_enter();
	attach = (struct kdmabuf_attachment *)
		kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_error("kzalloc failed\n");
		goto error;
	}
	block = container_of(es, struct dmabuf_manage_block, dmxes);
	phys = block->paddr;
	if (es->data_end < es->data_start)
		sgnum = 2;
	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, sgnum, GFP_KERNEL);
	if (ret) {
		pr_error("No memory for sgtable");
		goto error_alloc;
	}
	if (block->paddr != es->data_start) {
		pr_error("Invalid buffer info 0x%llx 0x%llx",
			block->paddr, es->data_start);
		goto error_attach;
	}
	if (sgnum == 2) {
		len = PAGE_ALIGN(es->buf_end - es->data_start);
		page = phys_to_page(phys);
		sg_set_page(sgt->sgl, page, len, 0);
		len = PAGE_ALIGN(es->data_end - es->buf_start);
		page = phys_to_page(es->buf_start);
		sg_set_page(sg_next(sgt->sgl), page, len, 0);
	} else {
		page = phys_to_page(phys);
		sg_set_page(sgt->sgl, page, PAGE_ALIGN(block->size), 0);
	}
	attach->dma_dir = DMA_NONE;
	attachment->priv = attach;
	return 0;
error_attach:
	sg_free_table(sgt);
error_alloc:
	kfree(attach);
error:
	return 0;
}

static void dmabuf_manage_detach(struct dma_buf *dbuf,
				struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach = attachment->priv;
	struct sg_table *sgt;

	pr_enter();
	if (!attach)
		return;
	sgt = &attach->sgt;
	sg_free_table(sgt);
	kfree(attach);
	attachment->priv = NULL;
}

static struct sg_table *dmabuf_manage_map_dma_buf(struct dma_buf_attachment *attachment,
		enum dma_data_direction dma_dir)
{
	struct kdmabuf_attachment *attach = attachment->priv;
	struct dmabuf_manage_block *block = NULL;
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	struct mutex *lock = &attachment->dmabuf->lock;
#endif
	struct sg_table *sgt;

	pr_enter();
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	mutex_lock(lock);
	pr_dbg("mutex_lock\n");
#endif
	sgt = &attach->sgt;
	if (attach->dma_dir == dma_dir) {
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
		mutex_unlock(lock);
#endif
		return sgt;
	}
	block = container_of((struct dmx_dma_buf_sec_es_data *)attachment->dmabuf->priv,
				struct dmabuf_manage_block, dmxes);
	sgt->sgl->dma_address = block->paddr;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sgt->sgl->dma_length = PAGE_ALIGN(block->size);
#else
	sgt->sgl->length = PAGE_ALIGN(block->size);
#endif
	pr_dbg("nents %d, %llx, %d, %d\n", sgt->nents, block->paddr,
			sg_dma_len(sgt->sgl), block->size);
	attach->dma_dir = dma_dir;
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	mutex_unlock(lock);
#endif
	return sgt;
}

static void dmabuf_manage_unmap_dma_buf(struct dma_buf_attachment *attachment,
		struct sg_table *sgt,
		enum dma_data_direction dma_dir)
{
	pr_enter();
}

static void dmabuf_manage_buf_release(struct dma_buf *dbuf)
{
	struct dmabuf_manage_block *block = NULL;
	struct dmx_dma_buf_sec_es_data *es = (struct dmx_dma_buf_sec_es_data *)dbuf->priv;
	struct decoder_mem_info_64bits rp_info;
	struct dmx_demux *demux = NULL;
	struct dmx_demux_ext *demux_ext = NULL;

	pr_enter();
	block = container_of(es, struct dmabuf_manage_block, dmxes);
	demux = (struct dmx_demux *)block->dmx;
	if (es->buf_rp == 0)
		es->buf_rp = es->data_end;
	if (es->buf_rp >= es->buf_start && es->buf_rp <= es->buf_end && demux) {
		rp_info.rp_phy = es->buf_rp;
		demux_ext = container_of(demux, struct dmx_demux_ext, dmx);
		if (demux_ext && demux_ext->decode_info_64bits) {
			demux_ext->decode_info_64bits(demux, &rp_info);
			pr_dbg("decode_info rp_phy:0x%llx\n", rp_info.rp_phy);
		}
	}

	pr_dbg("dma release handle:0x%x, paddr:0x%llx, size:0x%x\n",
		block->handle, block->paddr, block->size);
	kfree(block);
}

static int dmabuf_manage_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	struct dmabuf_manage_block *block = NULL;
	struct dmx_dma_buf_sec_es_data *es = (struct dmx_dma_buf_sec_es_data *)dbuf->priv;
	unsigned long addr = vma->vm_start;
	int len = 0;
	int ret = -EFAULT;

	pr_enter();
	block = container_of(es, struct dmabuf_manage_block, dmxes);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (block->paddr != es->data_start) {
		pr_error("Invalid buffer info %llx %llx",
			block->paddr, es->data_start);
		goto error;
	}
	if (es->data_end >= es->data_start) {
		len = PAGE_ALIGN(es->data_end - es->data_start);
		ret = remap_pfn_range(vma, addr,
				page_to_pfn(phys_to_page(block->paddr)),
				len, vma->vm_page_prot);
	} else {
		len = PAGE_ALIGN(es->buf_end - es->data_start);
		ret = remap_pfn_range(vma, addr,
				page_to_pfn(phys_to_page(block->paddr)),
				len, vma->vm_page_prot);
		if (ret) {
			pr_error("remap failed %d", ret);
			goto error;
		}
		addr += len;
		if (addr >= vma->vm_end)
			return ret;
		len = PAGE_ALIGN(es->data_end - es->buf_start);
		ret = remap_pfn_range(vma, addr,
				page_to_pfn(phys_to_page(es->buf_start)),
				len, vma->vm_page_prot);
	}
error:
	return ret;
}

static struct dma_buf_ops dmabuf_manage_ops = {
	.attach = dmabuf_manage_attach,
	.detach = dmabuf_manage_detach,
	.map_dma_buf = dmabuf_manage_map_dma_buf,
	.unmap_dma_buf = dmabuf_manage_unmap_dma_buf,
	.release = dmabuf_manage_buf_release,
	.mmap = dmabuf_manage_mmap
};

static struct dma_buf *get_dmabuf(struct dmabuf_manage_block *block,
				  unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	pr_dbg("export handle:0x%x, paddr:0x%llx, size:0x%x\n",
		block->handle, block->paddr, block->size);
	exp_info.ops = &dmabuf_manage_ops;
	exp_info.size = block->size;
	exp_info.flags = flags;
	exp_info.priv = (void *)&block->dmxes;
	exp_info.exp_name = "demux_dmabuf_manage";

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR_OR_NULL(dbuf))
		return NULL;
	return dbuf;
}

int dma_buf_get_fd(struct dmx_dma_buf_info *info, struct dmx_demux *dmx)
{
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int fd = -1;
	int fd_flags = O_CLOEXEC;

	pr_enter();
	if (info->paddr != info->dmxes.data_start) {
		pr_error("Invalid buffer info 0x%llx 0x%llx",
			info->paddr, info->dmxes.data_start);
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}
	memcpy(&block->dmxes, &info->dmxes, sizeof(block->dmxes));
	block->dmx = dmx;
	block->paddr = info->paddr;
	block->size = PAGE_ALIGN(info->size);
	block->handle = info->handle;
	dbuf = get_dmabuf(block, fd_flags);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc_object;
	}
	fd = dma_buf_fd(dbuf, fd_flags);
	if (fd < 0) {
		pr_error("dma_buf_fd failed, ret:%d, dbuf:%px, file:%px\n",
				fd, dbuf, dbuf->file);
		goto error_fd;
	}
	pr_dbg("output fd:%d\n", fd);
	info->fd = fd;
	return 0;
error_fd:
	dma_buf_put(dbuf);
	return fd;
error_alloc_object:
	pr_error("kfree block:%px\n", block);
	kfree(block);
error_copy:
	return -EFAULT;
}

int dma_buf_get_info(struct dmx_dma_buf_info *info)
{
	struct dmabuf_manage_block *block = NULL;
	struct dma_buf *dbuf;

	pr_enter();
	pr_dbg("input fd:%d\n", info->fd);
	dbuf = dma_buf_get(info->fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error;
	}
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = container_of((struct dmx_dma_buf_sec_es_data *)dbuf->priv,
					struct dmabuf_manage_block, dmxes);
		info->paddr = block->paddr;
		info->size = block->size;
		info->handle = block->handle;
		memcpy(&info->dmxes, &block->dmxes, sizeof(block->dmxes));
	}
	dma_buf_put(dbuf);
	return 0;
error:
	return -EFAULT;
}
