/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DVB_EXTERN_I2C_H__
#define __AML_DVB_EXTERN_I2C_H__

#include <linux/platform_device.h>
#include <linux/cdev.h>

#define AML_DVB_EXTERN_I2C_DEV_MAX 4

struct aml_dvb_extern_i2c_dev {
	unsigned int dev_cnt;
	struct i2c_client *client[AML_DVB_EXTERN_I2C_DEV_MAX];
};

int aml_dvb_extern_i2c_debug_show(char *buf);
struct i2c_client *aml_dvb_extern_get_i2c_client(unsigned char addr);
int aml_dvb_extern_i2c_init(void);
void aml_dvb_extern_i2c_exit(void);

#endif //__AML_DVB_EXTERN_I2C_H__
