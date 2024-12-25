/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/amlogic/media/aml_image_info.h>

#ifndef _AVBC_WRAPPER_INTERFACE_H_
#define _AVBC_WRAPPER_INTERFACE_H_

/*
 * AVBC processing mode.
 *
 * @AVBC_FLAG_IO_BLOCKING	: Block waiting for avbcd processing to complete.
 * @AVBC_FLAG_IO_NON_BLOCKING	: Submit avbcd task without blocking.
 */
#define AVBC_FLAG_IO_BLOCKING		BIT(0)
#define AVBC_FLAG_IO_NON_BLOCKING	BIT(1)

/*
 * enum avbc_memory_type_e - AVBC output buffer type.
 *
 * @AVBC_MEM_VIRTADDR	: Output buffer type is virtual address.
 * @AVBC_MEM_PHYADDR	: Output buffer type is physical address.
 * @AVBC_MEM_DMABUF	: Output buffer type is dma buffer.
 */
enum avbc_memory_type_e {
	AVBC_MEM_VIRTADDR,
	AVBC_MEM_PHYADDR,
	AVBC_MEM_DMABUF,
	AVBC_MEM_MAX
};

typedef void (*avbc_done)(void *out);

/*
 * struct avbc_input - Input parameters for AVBCD.
 *
 * @img     : Input image information.
 */
struct avbc_input {
	struct image_info_s	img;
};

/*
 * struct avbc_output - Output parameters for AVBCD.
 *
 * @img       : Output image information.
 * @done_func : Callback for non-blocking operations.
 */
struct avbc_output {
	struct image_info_s	img;
	avbc_done		done_func;
};

int AMLOGIC_AVBC_WRAPPER_vframe_decoder(struct avbc_output *out,
						struct avbc_input *in,
						u32 flag);

typedef int (*AMLOGIC_AVBC_WRAPPER_vframe_decoder_fun_t)(struct avbc_output *,
						struct avbc_input *,
						u32);

int register_amlogic_avbc_wrapper_fun(AMLOGIC_AVBC_WRAPPER_vframe_decoder_fun_t fn);
int unregister_amlogic_avbc_wrapper_fun(void);
#endif

