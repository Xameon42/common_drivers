/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_CAMERA_PLAT_CTRL_H
#define _AMLOGIC_CAMERA_PLAT_CTRL_H

#define ADDR8_DATA8		0
#define ADDR16_DATA8		1
#define ADDR16_DATA16		2
#define ADDR8_DATA16		3
#define TIME_DELAY		0xfe
#define END_OF_SCRIPT			0xff

struct cam_i2c_msg_s {
	unsigned char type;
	unsigned short addr;
	unsigned short data;
};

int i2c_get_byte(struct i2c_client *client, unsigned short addr);
int i2c_get_word(struct i2c_client *client, unsigned short addr);
int i2c_get_byte_add8(struct i2c_client *client, unsigned char addr);
int i2c_put_byte(struct i2c_client *client, unsigned short addr,
			unsigned char data);
int i2c_put_word(struct i2c_client *client, unsigned short addr,
			unsigned short data);
int i2c_put_byte_add8_new(struct i2c_client *client, unsigned char addr,
				 unsigned char data);
int i2c_put_byte_add8(struct i2c_client *client, char *buf, int len);
int cam_i2c_send_msg(struct i2c_client *client,
			struct cam_i2c_msg_s i2c_msg);

#endif /* _AMLOGIC_CAMERA_PLAT_CTRL_H. */
