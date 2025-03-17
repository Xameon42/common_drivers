/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HOST_MODULE_H
#define _HOST_MODULE_H
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/miscdevice.h>
#include <linux/amlogic/scpi_protocol.h>

/*m4 cmd and subid*/
#define SMC_M4_CMD			0x8200004E
/*soc: g12a/sm1*/
#define SMC_SUBID_MFH_V1_BOOT		0x20
/*soc: p1*/
#define SMC_SUBID_MFH_V2_BOOT		0x21
#define SMC_SUBID_MFH_V2_RESET		0x22
#define SMC_M4_BANK			0xFCB87430
/*hifi dsp cmd and subid*/
#define SMC_HIFI_DSP_CMD		0x82000090
#define SMC_SUBID_HIFI_DSP_BOOT		0x10
#define SMC_SUBID_HIFI_DSP_REMAP	0x11
#define SMC_SUBID_HIFI_DSP_PWR_SET	0x13
#define SMC_SUBID_SHIFT			0x8
#define PACK_SMC_SUBID_ID(subid, id) (((subid) << SMC_SUBID_SHIFT) | (id))

/*hifi dsp ffv to wake arm*/
#define MBX_CMD_VAD_AWE_WAKEUP		0x62
#define DSP_VAD_WAKUP_ARM		0x5555AAAA

/* pm support function
 *
 * bit0: pm_support_suspend
 * bit1: pm_support_always_on
 * bit2: pm_support_ffv
 */
#define PM_SUPPORT_DSP_SUSPEND(pm_support)	((pm_support) & 0x1)
#define PM_SUPPORT_DSP_ALWAYS_ON(pm_support)	(((pm_support) & 0x2) >> 1)
#define PM_SUPPORT_DSP_FFV(pm_support)		(((pm_support) & 0x4) >> 2)

struct host_module;
struct dsp_info_t;
struct host_data;
struct host_mfh;
struct host_dsp;

struct host_data {
	struct miscdevice misc;
	char name[20];
	u8 hostid;
	struct host_module *host;
} __packed;

struct dsp_info_t {
	char id;		/*dsp_id 0,1,2...*/
	char fw_id;
	char fw_name[32];	/*name of firmware which used for dsp*/
	char fw1_name[32];	/*name of firmware which used for dsp ddr*/
	char fw2_name[32];	/*name of firmware which used for dsp sram*/
	unsigned int phy_addr;  /*phy address of firmware will be loaded on*/
	unsigned int size;	/*size of reserved hifi memory*/
} __packed;

struct dsp_shm_info_t {
	unsigned int addr;
	unsigned int size;
} __packed;

struct mbox_buf {
	u32 id;
	u32 addr;
	u32 cfg0;
} __packed;

struct mfh_info {
	int cpuid;
	char name[30];
};

struct mfh_msg_buf {
	int size;
	char buf[96];
} __packed;

struct mfh_msg {
	struct list_head list;
	struct mfh_msg_buf msg_buf;
	struct completion complete;
};

#define HOSTFW_NAME_LEN                  32
#define REG_DSP_CFG0			(0x0)
#define REG_DSP_CFG1			(0x4)
#define REG_DSP_CFG2			(0x8)
#define REG_DSP_RESET_VEC		(0x004 << 2)

#define HOST_IOC_MAGIC  'H'

#define MFH_FIRMWARE_LOAD	_IOWR(HOST_IOC_MAGIC, 1, struct mfh_info)
#define MFH_CMD_SEND		_IOWR(HOST_IOC_MAGIC, 2, struct mfh_msg_buf)
#define MFH_CMD_LISTEN		_IOWR(HOST_IOC_MAGIC, 3, struct mfh_msg_buf)
#define HOST_LOAD		_IOWR(HOST_IOC_MAGIC, 1, struct dsp_info_t)
#define HOST_START		_IOWR(HOST_IOC_MAGIC, 3, struct dsp_info_t)
#define HOST_STOP		_IOWR(HOST_IOC_MAGIC, 4, struct dsp_info_t)
#define HOST_2LOAD		_IOWR(HOST_IOC_MAGIC, 7, struct dsp_info_t)
#define HOST_GET_INFO		_IOWR((HOST_IOC_MAGIC), (18), \
							struct dsp_info_t)
#define HOST_SHM_CLEAN		_IOWR(HOST_IOC_MAGIC, 64, \
							struct dsp_shm_info_t)
#define HOST_SHM_INV		_IOWR(HOST_IOC_MAGIC, 65, \
							struct dsp_shm_info_t)

/** struct host_mfh, the struct of host mfh
 * @mfh_info:          m4 mfh_info struct
 * @mfh_buf:           m4 mfh_buf struct
 * @mfh_msg:           m4 mfh_msg struct
 */

struct host_mfh {
	struct mfh_info mfh_info;
	struct mfh_msg_buf mfh_buf;
	struct mfh_msg *mfh_msg;
};

/** struct host_dsp, the struct of host dsp
 * @mbox_buf：           dsp mbox_buf struct
 * @shminfo：            dsp dsp_shm_info_t struct
 * @usrinfo:             dsp dsp_info_t struct
 * @pm_support_suspend:  dsp support suspend/resume
 * @pm_support_always_on:dsp work when arm suspend
 * @pm_support_ffv:      If support dsp far field voice
 * @input_device:        input_device for ffv to wakeup screen
 * @clk_hifi:            dsp hifi clk source
 */

struct host_dsp {
	struct mbox_buf mbox_buf;
	struct dsp_shm_info_t shminfo;
	struct dsp_info_t *usrinfo;
	u8 pm_support_suspend;
	u8 pm_support_always_on;
	u8 pm_support_ffv;
	struct input_dev *input_device;
	struct clk *clk_hifi;
	struct host_module *host;
	struct delayed_work host_ffv_work;
	struct workqueue_struct *host_ffv_wq;
};

/** struct host_module, the struct of host module
 * @dev:	                struct device for this Host driver
 * @base_reg:                   Base register of host
 * @health_reg:                 Health monitor register
 * @device_spt_reg:             Whether the boards support dsp/m4
 * @sram_addr:                  Sram virtual address
 * @phys_sram_addr:             Sram physical address
 * @phys_sram_size:             Sram physical address size
 * @ddr_addr:                   Ddr virtual address
 * @shm_addr:                   Share memory address
 * @phys_ddr_addr:              Ddr physical address
 * @phys_ddr_size:              Ddr physical address size
 * @phys_shm_addr:              Share memory physical address
 * @phys_shm_size:              Share memory physical address size
 * @phys_remap_addr:            Ddr physical remap address
 * @phys_remap_size:            Ddr physical remap size
 * @start_pos:                  Boot from sram, ddr or both
 * @logbuff_polling_period_ms:  Host logbuff polling period
 * @health_polling_ms:          Host health polling period
 * @addr_remap:                 Addr remap flag
 * @clk:                        Host clock
 * @clk_rate:                   Host clock rate
 * @pm_support:                 If support power management
 * @mbox_chan_to_dev:           Mbox channel from host to dev
 * @mbox_chan_from_dev:         Mbox channel from dev to host
 * @init_mbox_chan:             Mbox channel for dev init
 * @misc:                       Misc device
 * @hostid:                     Host id
 * @fname0:                     Host firmware name0
 * @fname1:                     Host firmware name1
 * @suspended:                  Host suspend flag
 * @pre_cnt:                    Host health monitor last value
 * @cur_cnt:                    Host health monitor current value
 * @host_monitor_work:          Host monitor work
 * @host_logbuff_work:          Host logbuff work
 * @host_wq:                    Host work queue
 * @hang:                       Host hang flag
 * @mbox_buf:                   Aocpu mbox buffer
 * @firmware_load:              Host firmware is load
 * @firmware_started:           host dsp firmware started
 * @device_support_bit:         DSP/M4 support bit
 * @host_data:                  Struct host_data
 * @host_mfh:                   Struct host_mfh
 * @host_dsp:                   Struct host_dsp
 * @nb:                         Host die notifier
 */

struct host_module {
	struct device *dev;
	void __iomem *base_reg;
	void __iomem *health_reg;
	void __iomem *device_spt_reg;
	void __iomem *sram_addr;
	phys_addr_t  phys_sram_addr;
	phys_addr_t  phys_sram_size;
	void __iomem *ddr_addr;
	void __iomem *shm_addr;
	phys_addr_t  phys_ddr_addr;
	phys_addr_t  phys_ddr_size;
	phys_addr_t  phys_shm_addr;
	phys_addr_t  phys_shm_size;
	phys_addr_t  phys_remap_addr;
	phys_addr_t  phys_remap_size;
	u8 start_pos;
	u32 logbuff_polling_ms;
	u32 health_polling_ms;
	bool addr_remap;
	struct clk *clk;
	u32 clk_rate;
	u8 pm_support;
	struct mbox_chan *mbox_chan_to_dev;
	struct mbox_chan *mbox_chan_from_dev;
	struct mbox_chan *init_mbox_chan;
	struct miscdevice *misc;
	int hostid;
	char fname0[HOSTFW_NAME_LEN];
	char fname1[HOSTFW_NAME_LEN];
	u32 pre_cnt, cur_cnt;
	struct delayed_work host_monitor_work;
	struct delayed_work host_logbuff_work;
	struct workqueue_struct *host_wq;
	struct workqueue_struct *host_logbuff_wq;
	u32 hang;

	u32 firmware_load;
	u32 firmware_started;
	u32 device_support_bit;
	struct host_data *host_data;
	struct host_mfh *host_mfh;
	struct host_dsp *host_dsp;
	struct notifier_block nb;
};

bool host_firmware_ready(u8 host_id);
struct device *host_to_device(u8 host_id);

#endif /*_HOST_MODULE_H*/
