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
#include "aml_dvb_extern_driver.h"

extern struct dvb_extern_device *dvb_extern_dev;

int aml_dvb_extern_i2c_debug_show(char *buf)
{
	int n = 0;
	struct aml_dvb_extern_i2c_client *tmp = NULL;

	if (!dvb_extern_dev) {
		pr_err("i2c: %s failed\n", __func__);
		return -ENODEV;
	}

	list_for_each_entry(tmp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp)) {
			if (!tmp->client)
				continue;

			if (buf)
				n += sprintf(buf + n,
					"i2c: client %s 0x%02x, adapter 0x%p %s, flag %d\n",
					tmp->client->name, tmp->client->addr, tmp->client->adapter,
					tmp->client->adapter ? tmp->client->adapter->name : "null",
					tmp->flag);
			else
				pr_info("i2c: client %s 0x%02x, adapter 0x%p %s, flag %d\n",
					tmp->client->name, tmp->client->addr, tmp->client->adapter,
					tmp->client->adapter ? tmp->client->adapter->name : "null",
					tmp->flag);
		}
	}

	return n;
}

int aml_dvb_extern_i2c_write(u16 addr, u8 *buf, u16 len)
{
	struct i2c_adapter *adapter = NULL;
	struct aml_dvb_extern_i2c_client *tmp = NULL;
	struct i2c_msg msg = {0};
	u16 i2c_addr = (addr & 0x80) ? addr >> 1 : addr;

	if (!dvb_extern_dev || !addr) {
		pr_err("i2c: %s 0x%02x failed\n", __func__, addr);
		return -ENODEV;
	}

	list_for_each_entry(tmp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp)) {
			if (!tmp->client || !tmp->client->adapter)
				continue;

			if (i2c_addr == tmp->client->addr) {
				adapter = tmp->client->adapter;
				break;
			}
		}
	}

	if (!adapter) {
		pr_err("i2c: 0x%02x invalid adapter!\n", addr);
		return -EINVAL;
	}

	msg.addr = i2c_addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buf;

	return i2c_transfer(adapter, &msg, 1);
}
EXPORT_SYMBOL(aml_dvb_extern_i2c_write);

int aml_dvb_extern_i2c_write_sub(u16 main_addr, u16 sub_addr, u8 *buf, u16 len)
{
	struct i2c_adapter *adapter = NULL;
	struct aml_dvb_extern_i2c_client *tmp = NULL;
	struct i2c_msg msg = {0};

	if (!dvb_extern_dev || !main_addr || !sub_addr) {
		pr_err("i2c: %s 0x%02x 0x%02x failed\n", __func__, main_addr, sub_addr);
		return -ENODEV;
	}

	list_for_each_entry(tmp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp)) {
			if (!tmp->client || !tmp->client->adapter)
				continue;

			if (((main_addr & 0x80) ? main_addr >> 1 : main_addr) ==
					tmp->client->addr) {
				adapter = tmp->client->adapter;
				break;
			}
		}
	}

	if (!adapter) {
		pr_err("i2c: 0x%02x invalid adapter!\n", sub_addr);
		return -EINVAL;
	}

	msg.addr = (sub_addr & 0x80) ? sub_addr >> 1 : sub_addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buf;

	return i2c_transfer(adapter, &msg, 1);
}
EXPORT_SYMBOL(aml_dvb_extern_i2c_write_sub);

int aml_dvb_extern_i2c_read(u16 addr, u8 *wbuf, u16 wlen, u8 *rbuf, u16 rlen)
{
	struct aml_dvb_extern_i2c_client *tmp = NULL;
	struct i2c_adapter *adapter = NULL;
	struct i2c_msg msgs[2] = {0};
	int num = 0;
	u16 i2c_addr = (addr & 0x80) ? addr >> 1 : addr;

	if (!dvb_extern_dev || !addr) {
		pr_err("i2c: %s 0x%02x failed\n", __func__, addr);
		return -ENODEV;
	}

	list_for_each_entry(tmp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp)) {
			if (!tmp->client || !tmp->client->adapter)
				continue;

			if (i2c_addr == tmp->client->addr) {
				adapter = tmp->client->adapter;
				break;
			}
		}
	}

	if (!adapter) {
		pr_err("i2c: 0x%02x invalid adapter!\n", addr);
		return -EINVAL;
	}

	if (!wbuf || !wlen) {
		msgs[0].addr = i2c_addr;
		msgs[0].flags = I2C_M_RD;
		msgs[0].len = rlen;
		msgs[0].buf = rbuf;
		num = 1;
	} else {
		msgs[0].addr = i2c_addr;
		msgs[0].flags = 0;
		msgs[0].len = wlen;
		msgs[0].buf = wbuf;
		msgs[1].addr = i2c_addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = rlen;
		msgs[1].buf = rbuf;
		num = 2;
	}

	return i2c_transfer(adapter, msgs, num);
}
EXPORT_SYMBOL(aml_dvb_extern_i2c_read);

int aml_dvb_extern_i2c_read_sub(u16 main_addr, u16 sub_addr,
		u8 *wbuf, u16 wlen, u8 *rbuf, u16 rlen)
{
	struct aml_dvb_extern_i2c_client *tmp = NULL;
	struct i2c_adapter *adapter = NULL;
	struct i2c_msg msgs[2] = {0};
	int num = 0;

	if (!dvb_extern_dev || !main_addr || !sub_addr) {
		pr_err("i2c: %s 0x%02x 0x%02x failed\n", __func__, main_addr, sub_addr);
		return -ENODEV;
	}

	list_for_each_entry(tmp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp)) {
			if (!tmp->client || !tmp->client->adapter)
				continue;

			if (((main_addr & 0x80) ? main_addr >> 1 : main_addr) ==
					tmp->client->addr) {
				adapter = tmp->client->adapter;
				break;
			}
		}
	}

	if (!adapter) {
		pr_err("i2c: 0x%02x invalid adapter!\n", sub_addr);
		return -EINVAL;
	}

	if (!wbuf || !wlen) {
		msgs[0].addr = (sub_addr & 0x80) ? sub_addr >> 1 : sub_addr;
		msgs[0].flags = I2C_M_RD;
		msgs[0].len = rlen;
		msgs[0].buf = rbuf;
		num = 1;
	} else {
		msgs[0].addr = (sub_addr & 0x80) ? sub_addr >> 1 : sub_addr;
		msgs[0].flags = 0;
		msgs[0].len = wlen;
		msgs[0].buf = wbuf;
		msgs[1].addr = (sub_addr & 0x80) ? sub_addr >> 1 : sub_addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = rlen;
		msgs[1].buf = rbuf;
		num = 2;
	}

	return i2c_transfer(adapter, msgs, num);
}
EXPORT_SYMBOL(aml_dvb_extern_i2c_read_sub);

static int aml_dvb_extern_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	const char *str = NULL;
	struct aml_dvb_extern_i2c_client *dvb_client = NULL, *tmp1 = NULL, *tmp2 = NULL;

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

	if (!dvb_extern_dev) {
		pr_err("i2c: %s failed\n", __func__);
		return -ENODEV;
	}
	list_for_each_entry_safe(tmp1, tmp2, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(tmp1) && !IS_ERR_OR_NULL(tmp1->client)) {
			if (!tmp1->client->adapter || (client->addr == tmp1->client->addr &&
					client->adapter == tmp1->client->adapter)) {
				if (tmp1->flag == FLAG_CUSTOM_ALLOC) {
					list_del_init(&tmp1->list);
					kfree(tmp1->client);
					kfree(tmp1);
				}
			}
		}
	}

	dvb_client = kzalloc(sizeof(*dvb_client), GFP_KERNEL);
	if (!dvb_client)
		return -ENOMEM;

	INIT_LIST_HEAD(&dvb_client->list);
	dvb_client->client = client;
	dvb_client->flag = FLAG_SYSTEM_ALLOC;
	list_add_tail(&dvb_client->list, &dvb_extern_dev->i2c_client_list);

	pr_info("i2c: probe %s addr 0x%02x OK", client->name, client->addr);

	return 0;
}

static int aml_dvb_extern_i2c_remove(struct i2c_client *client)
{
	struct aml_dvb_extern_i2c_client *dvb_client = NULL, *temp = NULL;

	if (!dvb_extern_dev) {
		pr_err("i2c: %s failed\n", __func__);
		return -ENODEV;
	}

	list_for_each_entry_safe(dvb_client, temp, &dvb_extern_dev->i2c_client_list, list) {
		if (!IS_ERR_OR_NULL(dvb_client)) {
			if (dvb_client->flag == FLAG_SYSTEM_ALLOC && dvb_client->client == client) {
				dvb_client->client = NULL;
				list_del_init(&dvb_client->list);
				kfree(dvb_client);
				break;
			}
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
	return i2c_add_driver(&aml_dvb_extern_i2c_driver);
}

void __exit aml_dvb_extern_i2c_exit(void)
{
	i2c_del_driver(&aml_dvb_extern_i2c_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("amlogic dvb extern i2c device driver");
//MODULE_LICENSE("GPL");
