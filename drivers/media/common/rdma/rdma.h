/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_
enum {
	VSYNC_RDMA = 0,      /* for write */
	VSYNC_RDMA_VPP1 = 1,
	VSYNC_RDMA_VPP2 = 2,
	PRE_VSYNC_RDMA = 3,
	EX_VSYNC_RDMA = 4,
	LINE_N_INT_RDMA = 5,
	VSYNC_RDMA_READ = 6, /* for read */
};

extern int has_multi_vpp;
extern ulong rdma_done_us[];
extern ulong rdma_vsync_us[];
extern ulong rdma_config_us[];
extern int rdma_reset_trigger_flag[];
extern int rdma_configured[];
extern unsigned int use_rdma_done_detect;
extern unsigned int rdma_done_detect_cnt;
extern unsigned int rdma_done_detect_reg;
extern int enc_num_configed[];

ulong get_enc_time_start(u8 index);
u32 get_enc_num_start(u8 index);
u32 get_cur_enc_num(void);
void rdma_stop(int handle);

void vpp1_vsync_rdma_register(void);
void vpp2_vsync_rdma_register(void);
void pre_vsync_rdma_register(void);
void ex_vsync_rdma_register(void);
int vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
int vsync_rdma_vpp1_config(void);
void vsync_rdma_vpp1_config_pre(void);
int vsync_rdma_vpp2_config(void);
void vsync_rdma_vpp2_config_pre(void);
int pre_vsync_rdma_config(void);
void pre_vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
bool is_vsync_vpp1_rdma_enable(void);
bool is_vsync_vpp2_rdma_enable(void);
bool is_pre_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
int rdma_watchdog_setting(int flag, int handle);
int rdma_init2(void);
struct rdma_op_s *get_rdma_ops(int rdma_type);
void set_rdma_handle(int rdma_type, int handle);
int get_rdma_handle(int rdma_type);
int get_rdma_type(int handle);
int set_vsync_rdma_id(u8 id);
int rdma_init(void);
void rdma_exit(void);
int get_ex_vsync_rdma_enable(void);
void set_ex_vsync_rdma_enable(int enable);
void set_force_rdma_config(int handle);
int is_in_vsync_isr(u8 cur_cpuid);
int is_in_pre_vsync_isr(u8 cur_cpuid);
int is_in_vsync_isr_viu2(u8 cur_cpuid);
int is_in_vsync_isr_viu3(u8 cur_cpuid);
int is_video_process_in_thread(void);
bool get_lowlatency_mode(void);
#ifdef CONFIG_AMLOGIC_BL_LDIM
int is_in_ldim_vsync_isr(u8 cur_cpuid);
#endif
void set_rdma_channel_enable(u8 rdma_en);
unsigned int rdma_hw_done_bit(void);

//extern int vsync_rdma_handle[5];
u32 VCBUS_RD_MPEG_REG(u32 adr);
int VCBUS_WR_MPEG_REG(u32 adr, u32 val);
int VCBUS_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);

u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr);
u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr);
u32 PRE_VSYNC_RD_MPEG_REG(u32 adr);
//int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val);
int PRE_VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len);
int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len);
int PRE_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
#endif
