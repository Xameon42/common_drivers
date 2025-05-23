/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DVB_EXTERN_I2C_H__
#define __AML_DVB_EXTERN_I2C_H__

#include <linux/platform_device.h>
#include <linux/cdev.h>

#define AML_DVB_EXTERN_I2C_DEV_MAX 4

enum i2c_client_alloc_flag {
	FLAG_NO_ALLOC,
	FLAG_CUSTOM_ALLOC,
	FLAG_SYSTEM_ALLOC
};

struct aml_dvb_extern_i2c_client {
	struct i2c_client *client;
	enum i2c_client_alloc_flag flag;
	struct list_head list;
};

int aml_dvb_extern_i2c_debug_show(char *buf);
int aml_dvb_extern_i2c_init(void);
void aml_dvb_extern_i2c_exit(void);

#endif //__AML_DVB_EXTERN_I2C_H__
