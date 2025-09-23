/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_TASK_H__
#define __DI_TASK_H__

extern unsigned int di_dbg_task_flg;	/*debug only*/

enum ETSK_STATE {
	ETSK_STATE_IDLE,
	ETSK_STATE_WORKING,
};

void task_stop(void);
int task_start(void);

void dbg_task(void);

bool task_send_cmd(unsigned int cmd);
void task_send_ready(unsigned int id);
bool task_send_cmd2(unsigned int ch, unsigned int cmd);
void task_polling_cmd_keep(unsigned int ch, unsigned int top_sts);
void task_delay(unsigned int val);

void mtask_stop(void);
int mtask_start(void);
void dbg_mtask(void);
//bool mtsk_alloc(unsigned int ch, unsigned int nub, unsigned int size_page);
//bool mtsk_release(unsigned int ch, unsigned int cmd);
bool mtsk_alloc_block(unsigned int ch, struct mtsk_cmd_s *cmd);
bool mtsk_release_block(unsigned int ch, unsigned int cmd);
bool mtsk_alloc_block2(unsigned int ch, struct mtsk_cmd_s *cmd);
bool mtask_send_cmd(unsigned int ch, struct mtsk_cmd_s *cmd);
bool mtsk_release(unsigned int ch, unsigned int cmd);
void mtask_wake_m(void);

#endif /*__DI_TASK_H__*/
