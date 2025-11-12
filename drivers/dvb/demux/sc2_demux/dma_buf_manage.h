/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DMA_BUF_MANAGE_H_
#define _DMA_BUF_MANAGE_H_

#include <linux/types.h>
#include <linux/dvb/aml_dmx_ext.h>
#include "demux.h"

int dma_buf_get_fd(struct dmx_dma_buf_info *info, struct dmx_demux *dmx);
int dma_buf_get_info(struct dmx_dma_buf_info *info);

#endif
