/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _LINUX_SECMEM_H_
#define _LINUX_SECMEM_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define AML_DMA_BUF_MANAGER_VERSION  2
#define VIDEODEC_DATA_MAX_LEN 256
#define DMA_BUF_MANAGER_MAX_BUFFER_LEN 320

enum dmabuf_manage_type {
	DMA_BUF_TYPE_INVALID,
	DMA_BUF_TYPE_SECMEM,
	DMA_BUF_TYPE_DMX_ES,
	DMA_BUF_TYPE_DMABUF,
	DMA_BUF_TYPE_VIDEODEC_ES,
	DMA_BUF_TYPE_SECURE_VDEC,
	DMA_BUF_TYPE_MAX
};

struct dmabuf_dmx_sec_es_data {
	__u8 pts_dts_flag;
	__u64 video_pts;
	__u64 video_dts;
	__u32 buf_start;
	__u32 buf_end;
	__u32 data_start;
	__u32 data_end;
	__u32 buf_rp;
	__u64 av_handle;
	__u32 token;
	__u32 extend0;
	__u32 extend1;
	__u32 extend2;
	__u32 extend3;
};

struct secmem_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
};

struct secmem_extend_block {
	__u64 paddr;
	__u64 size;
	__u64 handle;
};

enum dmabuf_manage_videodec_type {
	DMA_BUF_VIDEODEC_HDR10PLUS = 1
};

struct dmabuf_videodec_es_data {
	__u32 data_type;
	__u8  data[VIDEODEC_DATA_MAX_LEN];
	__u32 data_len;
};

struct dmabuf_manage_buffer {
	__u32 type;
	__u32 fd;
	__u32 paddr;
	__u32 size;
	__u32 handle;
	__u32 flags;
	__u32 extend;
	union {
		struct dmabuf_videodec_es_data vdecdata;
		__u8 data[DMA_BUF_MANAGER_MAX_BUFFER_LEN];
	} buffer;
};

struct dmabuf_manage_extend_buffer {
	__u64 type;
	__u64 fd;
	__u64 paddr;
	__u64 size;
	__u64 handle;
	__u64 flags;
	__u64 extend;
	union {
		struct dmabuf_videodec_es_data vdecdata;
		__u8 data[DMA_BUF_MANAGER_MAX_BUFFER_LEN];
	} buffer;
};

#define DMABUF_ALLOC_FROM_CMA   1

struct dmabuf_manage_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
	__u32 type;
	__u32 flags;
	__u32 extend;
	__u64 extend_paddr;
	__u64 extend_size;
	__u64 extend_handle;
	__u64 extend_flags;
	void *priv;
};

struct secure_vdec_channel {
	__u32 id_high;
	__u32 id_low;
};

#define SECURE_HEAP_USER_TA_VERSION				3
#define SECURE_HEAP_USER_TA_VERSION_EXTEND      4
#define SECURE_HEAP_MAX_VERSION					5

int dmabuf_manage_secure_pool_create(u32 id_high, u32 id_low, u32 block_size,
	u32 version);
u64 dmabuf_manage_secure_block_alloc(u32 id_high, u32 id_low, u32 size,
	u32 version);
int dmabuf_manage_secure_block_free(u32 id_high, u32 id_low,
	u64 addr, u32 size, u32 version);
int dmabuf_manage_get_secure_heap_version(void);

unsigned int dmabuf_manage_get_type(struct dma_buf *dbuf);
void *dmabuf_manage_get_info(struct dma_buf *dbuf, unsigned int type);

bool dmabuf_is_esbuf(struct dma_buf *dmabuf);
int dmabuf_manage_support_nohead(void);
int dmabuf_manage_vp9_probe_metadata(phys_addr_t phy, u32 size,
	unsigned char *meta, u32 *meta_size);

#define DMABUF_MANAGE_IOC_MAGIC			'S'

#define DMABUF_MANAGE_EXPORT_DMA             _IOWR(DMABUF_MANAGE_IOC_MAGIC, 1, int)
#define DMABUF_MANAGE_GET_HANDLE             _IOWR(DMABUF_MANAGE_IOC_MAGIC, 2, int)
#define DMABUF_MANAGE_GET_PHYADDR            _IOWR(DMABUF_MANAGE_IOC_MAGIC, 3, int)
#define DMABUF_MANAGE_IMPORT_DMA             _IOWR(DMABUF_MANAGE_IOC_MAGIC, 4, int)
#define DMABUF_MANAGE_REGISTER_CHANNEL       _IOWR(DMABUF_MANAGE_IOC_MAGIC, 5, int)

#define DMABUF_MANAGE_EXTEND_EXPORT_DMA \
	 _IOWR(DMABUF_MANAGE_IOC_MAGIC, 6, struct secmem_extend_block)
#define DMABUF_MANAGE_EXTEND_GET_HANDLE \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 7, struct secmem_extend_block)
#define DMABUF_MANAGE_EXTEND_GET_PHYADDR \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 8, struct secmem_extend_block)
#define DMABUF_MANAGE_EXTEND_IMPORT_DMA \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 9, long)
#define DMABUF_MANAGE_EXTEND_REGISTER_CHANNEL \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 10, struct secure_vdec_channel)

#define DMABUF_MANAGE_VERSION \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 100, struct dmabuf_manage_buffer)
#define DMABUF_MANAGE_EXPORT_DMABUF \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 101, struct dmabuf_manage_buffer)
#define DMABUF_MANAGE_GET_DMABUFINFO \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 102, struct dmabuf_manage_buffer)
#define DMABUF_MANAGE_ALLOCDMABUF \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 104, struct dmabuf_manage_buffer)

#define DMABUF_MANAGE_EXTEND_EXPORT_DMABUF \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 201, struct dmabuf_manage_extend_buffer)
#define DMABUF_MANAGE_EXTEND_GET_DMABUFINFO \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 202, struct dmabuf_manage_extend_buffer)
#define DMABUF_MANAGE_EXTEND_ALLOCDMABUF \
	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 204, struct dmabuf_manage_extend_buffer)

#define DMABUF_MANAGE_V_VERSION                _IOWR(DMABUF_MANAGE_IOC_MAGIC, 1000, int)
#define DMABUF_MANAGE_V_EXPORT_DMABUF          _IOWR(DMABUF_MANAGE_IOC_MAGIC, 1001, int)
#define DMABUF_MANAGE_V_GET_DMABUFINFO         _IOWR(DMABUF_MANAGE_IOC_MAGIC, 1002, int)
#define DMABUF_MANAGE_V_ALLOCDMABUF            _IOWR(DMABUF_MANAGE_IOC_MAGIC, 1004, int)

#endif /* _LINUX_DMABUF_MANAGE_H_ */
