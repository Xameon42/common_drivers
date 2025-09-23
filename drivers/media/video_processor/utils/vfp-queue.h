/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __VFP_QUEUE_H__
#define __VFP_QUEUE_H__
#include "vfp.h"

static inline bool vfq_full(struct vfq_s *q)
{
	bool ret = (((q->wp + 1) % q->size) == q->rp);
	return ret;
}
#endif
