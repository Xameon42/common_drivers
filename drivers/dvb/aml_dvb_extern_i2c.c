// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/of.h>
#include "aml_dvb_extern_i2c.h"

static struct aml_dvb_extern_i2c_dev i2c_device;

int aml_dvb_extern_i2c_debug_show(char *buf)
{
	int i = 0, n = 0;

	for (i = 0; i < AML_DVB_EXTERN_I2C_DEV_MAX; i++) {
		if (!i2c_device.client[i])
			continue;

		if (buf)
			n += sprintf(buf + n, "i2c: client[%d] addr 0x%x, adapter 0x%p\n",
				i, i2c_device.client[i]->addr,
				i2c_device.client[i]->adapter);
		else
			pr_info("i2c: client[%d] addr 0x%x, adapter 0x%p\n",
				i, i2c_device.client[i]->addr,
				i2c_device.client[i]->adapter);
	}

	return n;
}

struct i2c_client *aml_dvb_extern_get_i2c_client(unsigned char addr)
{
	struct i2c_client *client = NULL;
	int i = 0;

	for (i = 0; i < AML_DVB_EXTERN_I2C_DEV_MAX; i++) {
		if (!i2c_device.client[i])
			continue;

		// addr maby 8bit, but client addr 7bit.
		if (i2c_device.client[i]->addr == ((addr & 0x80) ? addr >> 1 : addr)) {
			client = i2c_device.client[i];
			break;
		}
	}

	pr_info("i2c: addr 0x%x, client 0x%p\n", addr, client);

	return client;
}

int aml_dvb_extern_i2c_write(struct i2c_client *client,
		unsigned char *buff, unsigned int len)
{
	struct i2c_msg msg;
	int ret = 0;

	if (!client) {
		pr_err("i2c: client null\n");

		return -1;
	}

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buff;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("i2c: write addr 0x%02x error %d\n", client->addr, ret);

		return ret;
	}

	return 0;
}

int aml_dvb_extern_i2c_read(struct i2c_client *client,
		unsigned char *wbuf, unsigned int wlen,
		unsigned char *rbuf, unsigned int rlen)
{
	struct i2c_msg msgs[2];
	int ret = 0;

	if (!client) {
		pr_err("i2c: client null\n");

		return -1;
	}

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = wlen;
	msgs[0].buf = wbuf;
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = rlen;
	msgs[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("i2c: read addr 0x%02x error %d\n", client->addr, ret);

		return ret;
	}

	return 0;
}

static int aml_dvb_extern_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	const char *str = NULL;

	if (i2c_device.dev_cnt >= AML_DVB_EXTERN_I2C_DEV_MAX) {
		pr_err("i2c: dev_cnt reach max %d\n", AML_DVB_EXTERN_I2C_DEV_MAX);

		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c: check functionality failed\n");

		return -ENODEV;
	}

	ret = of_property_read_string(client->dev.of_node, "dev_name", &str);
	if (ret) {
		pr_info("i2c: can't get dev_name\n");
		strcpy(client->name, "none");
	} else {
		//pr_info("i2c: get dev_name %s\n", str);
		strncpy(client->name, str, sizeof(client->name));
	}

	pr_info("i2c: probe %s addr 0x%02x OK", client->name, client->addr);

	i2c_device.client[i2c_device.dev_cnt] = client;
	i2c_device.dev_cnt++;

	return 0;
}

static int aml_dvb_extern_i2c_remove(struct i2c_client *client)
{
	int i = 0;

	for (i = 0; i < AML_DVB_EXTERN_I2C_DEV_MAX; i++) {
		if (client == i2c_device.client[i]) {
			i2c_device.client[i] = NULL;
			i2c_device.dev_cnt--;

			break;
		}
	}

	return 0;
}

static const struct i2c_device_id aml_dvb_extern_i2c_id[] = {
	{ "aml_dvb_extern_i2c", 0 },
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id aml_dvb_extern_i2c_dt_match[] = {
	{
		.compatible = "aml_dvb_extern_i2c",
	},
	{},
};
#endif

static struct i2c_driver aml_dvb_extern_i2c_driver = {
	.probe    = aml_dvb_extern_i2c_probe,
	.remove   = aml_dvb_extern_i2c_remove,
	.id_table = aml_dvb_extern_i2c_id,
	.driver = {
		.name  = "aml_dvb_extern_i2c",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_dvb_extern_i2c_dt_match,
#endif
	},
};

int __init aml_dvb_extern_i2c_init(void)
{
	memset(&i2c_device, 0, sizeof(i2c_device));

	return i2c_add_driver(&aml_dvb_extern_i2c_driver);
}

void __exit aml_dvb_extern_i2c_exit(void)
{
	i2c_del_driver(&aml_dvb_extern_i2c_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("amlogic dvb extern i2c device driver");
//MODULE_LICENSE("GPL");
