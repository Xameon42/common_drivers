/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _PPMGR_PRI_INCLUDE__
#define _PPMGR_PRI_INCLUDE__

/* for ppmgr device op. */
int init_ppmgr_device(void);
int uninit_ppmgr_device(void);

/* for ppmgr device class op. */
struct class *init_ppmgr_cls(void);

/* for thread of ppmgr. */
int start_ppmgr_task(void);
void stop_ppmgr_task(void);

/* for ppmgr private member. */
void set_ppmgr_buf_info(unsigned int start, unsigned int size);
void get_ppmgr_buf_info(unsigned int *start, unsigned int *size);

/*  ppmgr buffer op. */
int ppmgr_buffer_init(int secure_mode);
int ppmgr_buffer_uninit(void);
void vf_ppmgr_reset(int type);
int ppmgr_register(void);
irqreturn_t decontour_pre_isr(int irq, void *dev_id);

/* for thread of tb detect. */
int start_tb_task(void);
void stop_tb_task(void);
void get_tb_detect_status(void);
void ppmgr_vf_peek_dec_debug(void);

#endif /* _PPMGR_PRI_INCLUDE__ */
