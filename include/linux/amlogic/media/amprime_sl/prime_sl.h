/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PRIMSE_SL_H_
#define _PRIMSE_SL_H_

#include <linux/types.h>
#include <linux/amlogic/media/vfm/vframe.h>

#define PRIME_SL_HDR_MODE_2020_TO_709 0
#define PRIME_SL_HDR_MODE_709_TO_2020 1
#define PRIME_SL_HDR_MODE_BYPASS_2020 2
#define PRIME_SL_HDR_MODE_BYPASS_709 3

void prime_sl_process(struct vframe_s *vf);
bool is_prime_sl_enable(void);
bool is_prime_sl_on(void);
int get_prime_sl_hdr_process_policy(struct vframe_s *vf);
bool is_valid_prime_sl_metadata(char *buf, int size);
bool get_prime_sl_frame(void);
void set_prime_sl_frame(bool val);

#endif
