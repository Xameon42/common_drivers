// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/amlogic/gpiolib.h>
#include "nta533.h"


#ifdef NTA533_DEBUG
#define DEBUG_PRINT(fmt, args...) printk(KERN_DEBUG "nta533: " fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...) do {} while (0)
#endif

#define USER_MEMORY_START (0x0000)
#define REPEAT_WRITE_COUNT_DEFAULT 3
#define NFC_SRAM_BUFF_SIZE 256
u8 sramBuff[NFC_SRAM_BUFF_SIZE];

struct nta533 {
	u32 repeat_write_count;
	u32 slave_addr;
    unsigned int udelay;
    int irq_num;
    int ed;
	struct i2c_client* client;
	struct miscdevice nta_misc_device;
};

static void nta_read_block(struct i2c_client *i2c,  u32 slave_addr, u16 reg, u8 *rdbuf, int rdlen)
{
	u8 info[2];
	struct i2c_msg msg[2];
	int ret;
	// //pr_info("[nta] nta_read_register\n");

	info[0] = (reg >> 8);
	info[1] = (reg & 0xFF);

	msg[0].flags = 0;
	msg[0].addr  = slave_addr;
	msg[0].len   = 2;
	msg[0].buf   = info;

	msg[1].flags = I2C_M_RD;
	msg[1].addr  = slave_addr;
	msg[1].len   = rdlen;
	msg[1].buf   = rdbuf;
	ret = i2c_transfer(i2c->adapter, &msg[0], 2);
	if (ret < 0)
		DEBUG_PRINT("nta read block %x failed\n", reg);
}

static void nta_write_block(struct i2c_client *i2c, u32 slave_addr, u16 reg, u8 *wrbuf, int wrlen)
{
	u8 info[258];
	struct i2c_msg msg;
	int ret;

	if (wrlen > 256) {
		pr_err("%s %d nta write block : wrlen too long\n", __func__, __LINE__);
		return;
	}

	info[0] = (reg >> 8);
	info[1] = (reg & 0xFF);

	memcpy(&info[2], wrbuf, wrlen);

	msg.flags = 0;
	msg.addr  = slave_addr;
	msg.len   = 2 + wrlen;
	msg.buf   = info;

	ret = i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 0)
		DEBUG_PRINT("nta write block %x failed\n", reg);
}

static void nta_read_register(struct i2c_client *i2c, u32 slave_addr,
				u16 reg, u8 index, u8 *rdbuf, u8 rdlen)
{
	u8 info[3];
	struct i2c_msg msg[2];
	int ret;

	info[0] = (reg >> 8);
	info[1] = (reg & 0xFF);
	info[2] = index;

	msg[0].flags = 0;
	msg[0].addr  = slave_addr;
	msg[0].len   = 3;
	msg[0].buf   = info;

	msg[1].flags = I2C_M_RD;
	msg[1].addr  = slave_addr;
	msg[1].len   = rdlen;
	msg[1].buf   = rdbuf;
	ret = i2c_transfer(i2c->adapter, &msg[0], 2);
	if (ret < 0)
		DEBUG_PRINT("nta read reg %x %d failed\n", reg, index);

}

static void nta_write_register(struct i2c_client *i2c, u32 slave_addr,
				u16 reg, u8 index, u8 mask, u8 *wrbuf)
{
	u8 info[5];
	struct i2c_msg msg;
	int ret;

	info[0] = (reg >> 8);
	info[1] = (reg & 0xFF);
	info[2] = index;
	info[3] = mask;
	info[4] = *wrbuf;

	msg.flags = 0;
	msg.addr  = slave_addr;
	msg.len   = 5;
	msg.buf   = info;

	ret = i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 0)
		DEBUG_PRINT("nta write reg %x %d failed\n", reg, index);
}

static void nta_init(struct nta533 *nta)
{
	u8 regval;
	//nta_read_register(nta->client, nta->slave_addr, NTAG_TAG_CONFIG_REG, 1, &regval, 1);
	//pr_info("[nta] %s: nta_read_register TAG_CONFIG_REG 1 : %x\n", __func__, regval);
	regval = 0x00;
	nta_write_register(nta->client, nta->slave_addr, NTAG_TAG_CONFIG_REG, 1, 0x0C, &regval);
	//nta_read_register(nta->client, nta->slave_addr, NTAG_TAG_CONFIG_REG, 1, &regval, 1);
	//pr_info("[nta] %s: nta_read_register TAG_CONFIG_REG 1 : %x\n", __func__, regval);

	//nta_read_register(nta->client, nta->slave_addr, NTAG_TAG_ED_CONF_REG, 0, &regval, 1);
	//pr_info("[nta] %s: nta_read_register ED_CONF_REG 0 : %x\n", __func__, regval);
	regval = 0x01;
	nta_write_register(nta->client, nta->slave_addr, NTAG_TAG_ED_CONF_REG, 0, 0x0F, &regval);
	//nta_read_register(nta->client, nta->slave_addr, NTAG_TAG_ED_CONF_REG, 0, &regval, 1);
	//pr_info("[nta] %s: nta_read_register ED_CONF_REG 0 : %x\n", __func__, regval);
}

static int nta533_open(struct inode *inode, struct file *filp)
{
	DEBUG_PRINT("[nta] nta533_open\n");
	return 0;
}

static int nta533_release(struct inode *inode, struct file *filp)
{
	DEBUG_PRINT("[nta] nta533_release\n");
	return 0;
}

static const struct file_operations nta533_fops = {
	.owner		= THIS_MODULE,
	.open		= nta533_open,
	.release	= nta533_release,
};

static ssize_t nta_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 regval;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct nta533 *nta = container_of(miscdev, struct nta533, nta_misc_device);	

	regval = 0x01;
	nta_read_register(nta->client, nta->slave_addr, NTAG_TAG_CONFIG_REG, 1, &regval, 1);
	*buf = ((regval & 0x1) + '0');
	
	DEBUG_PRINT("[nta] nta_check_show\n");
    return 1;
}

static ssize_t nta_check_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 regval;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct nta533 *nta = container_of(miscdev, struct nta533, nta_misc_device);	
	
	regval = (*buf - '0');
	nta_write_register(nta->client, nta->slave_addr, NTAG_TAG_CONFIG_REG, 1, 0x01, &regval);

	DEBUG_PRINT("[nta] nta_check_store\n");
    return size;
}

static ssize_t nta_edit_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct nta533 *nta = container_of(miscdev, struct nta533, nta_misc_device);

	DEBUG_PRINT("[nta] nta_edit_show\n");
	nta_read_block(nta->client, nta->slave_addr, USER_MEMORY_START, sramBuff, NFC_SRAM_BUFF_SIZE);
	memcpy(buf, sramBuff, NFC_SRAM_BUFF_SIZE);
	return NFC_SRAM_BUFF_SIZE;
}

static ssize_t nta_edit_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 i = 0;
	u8 tempIndex = 0;
	u8 sramIndex = 0;
	u16 block;
	int bufsize;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct nta533 *nta = container_of(miscdev, struct nta533, nta_misc_device);
	u8 tempBuff[256];
	memset(tempBuff, 0, sizeof(tempBuff));
	if (sscanf(buf, "%4x %d %s", &block, &bufsize, tempBuff)) {
		if (block == 0) {
			pr_err("%s,%d, block 0 is not allowed to write\n", __func__, __LINE__);
			return 0;
		}
		for (tempIndex = 0; tempIndex < bufsize;) {
			if ('0' <= tempBuff[tempIndex] && tempBuff[tempIndex] <= '9') {
				sramBuff[sramIndex] = (tempBuff[tempIndex] - '0') << 4;
			} else if ('A' <= tempBuff[tempIndex] && tempBuff[tempIndex] <= 'F') {
				sramBuff[sramIndex] = (tempBuff[tempIndex] - 'A' + 10) << 4;
			} else if ('a' <= tempBuff[tempIndex] && tempBuff[tempIndex] <= 'f') {
				sramBuff[sramIndex] = (tempBuff[tempIndex] - 'a' + 10) << 4;
			}
			if ('0' <= tempBuff[tempIndex + 1] && tempBuff[tempIndex + 1] <= '9') {
				sramBuff[sramIndex] |= (tempBuff[tempIndex + 1] - '0');
			} else if ('A' <= tempBuff[tempIndex + 1] && tempBuff[tempIndex + 1] <= 'F') {
				sramBuff[sramIndex] |= (tempBuff[tempIndex + 1] - 'A' + 10);
			} else if ('a' <= tempBuff[tempIndex + 1] && tempBuff[tempIndex + 1] <= 'f') {
				sramBuff[sramIndex] |= (tempBuff[tempIndex + 1] - 'a' + 10);
			}
			DEBUG_PRINT("sramBuff[%d] : %x\n", sramIndex, sramBuff[sramIndex]);
			tempIndex += 2;
			sramIndex++;
		}
		for (i = 0; i < nta->repeat_write_count; i++)
			nta_write_block(nta->client, nta->slave_addr, block, sramBuff, bufsize/2);
	} else {
		pr_err("%s,%d, invalid input", __func__, __LINE__);
		return -EINVAL;
	}
	return size;
}

static DEVICE_ATTR_RW(nta_check);
static DEVICE_ATTR_RW(nta_edit);
static struct attribute *nta_attributes[] = {
	&dev_attr_nta_check.attr,
	&dev_attr_nta_edit.attr,
	NULL
};

static struct attribute_group nta_attribute_group = {
    .attrs = nta_attributes
};

static irqreturn_t nta_irq_handler(int irq, void *dev_id)
{
	DEBUG_PRINT("%s,%d\n", __func__, __LINE__);
	return IRQ_HANDLED;
}

static int nta533_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct nta533 *nta;
	struct gpio_desc *gpio_desc;
	struct device_node *np = i2c->dev.of_node;
	int ret;

	nta = devm_kzalloc(&i2c->dev, sizeof(struct nta533), GFP_KERNEL);
	if (!nta)
		return -ENOMEM;
	nta->client = i2c;
	nta->slave_addr = i2c->addr;
	DEBUG_PRINT("[nta] slave_addr : %x\n", nta->slave_addr);

	ret = of_property_read_u32(np, "repeatcount", &(nta->repeat_write_count));
	if (ret < 0) {
		nta->repeat_write_count = REPEAT_WRITE_COUNT_DEFAULT;
		pr_err("fail to get repeat write count: %x\n", nta->repeat_write_count);
	} else {
		DEBUG_PRINT("repeat write count success: %x\n", nta->repeat_write_count);
	}

	// ED interrupt
	nta->ed = of_get_named_gpio(np, "ed-gpio", 0);
	if (nta->ed < 0) {
		pr_err("%s %d Failed to get edgpio\n", __func__, __LINE__);
	}
	ret = devm_gpio_request(&i2c->dev, nta->ed, "ed-gpio");
	if (ret) {
		pr_err("%s %d Failed to request edgpio\n", __func__, __LINE__);
	}
	gpio_desc = gpio_to_desc(nta->ed);
	gpiod_set_pull(gpio_desc, GPIOD_PULL_UP);

	nta->irq_num = gpio_to_irq(nta->ed);
	if (nta->irq_num < 0) {
		pr_err("%s %d request irq num failed\n", __func__, __LINE__);
	}
	ret = devm_request_irq(&i2c->dev, nta->irq_num, nta_irq_handler, IRQF_TRIGGER_FALLING, "edgpio", nta);
	if (ret) {
		pr_err("%s %d request irq failed\n", __func__, __LINE__);
	}

	// nfc init
	nta_init(nta);
	i2c_set_clientdata(i2c, nta);

	// misc
	nta->nta_misc_device.minor = MISC_DYNAMIC_MINOR;
	nta->nta_misc_device.name = "nta533";
	nta->nta_misc_device.fops = &nta533_fops;

	ret = misc_register(&nta->nta_misc_device);
	if (ret) {
		pr_err("%s %d misc_register failed\n", __func__, __LINE__);
	}else{
		ret = sysfs_create_group(&nta->nta_misc_device.this_device->kobj, &nta_attribute_group);
		if (ret) {
			sysfs_remove_group(&nta->nta_misc_device.this_device->kobj, &nta_attribute_group);
			return ret;
		}
	}

	return ret;
}

static int nta533_remove(struct i2c_client *i2c)
{
	struct nta533 *nta = i2c_get_clientdata(i2c);

	sysfs_remove_group(&(i2c->dev.kobj), &nta_attribute_group);
	misc_deregister(&nta->nta_misc_device);
	free_irq(nta->irq_num, nta);
	// devm_gpio_free(&i2c->dev, nta->ed);

	return 0;
}

static const struct i2c_device_id nfc_i2c_id[] = {
	{ "nta533", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, nfc_i2c_id);

static const struct of_device_id of_nfc_match[] = {
	{ .compatible = "NXP,nta533", },
	{},
};

static struct i2c_driver nta533_driver = {
	.probe		= nta533_probe,
	.remove		= nta533_remove,
	.driver		= {
		.name	= "nta533",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_nfc_match),
	},
    .id_table = nfc_i2c_id,
};

int __init nta533_init(void)
{
	return i2c_add_driver(&nta533_driver);
}
module_init(nta533_init);

void __exit nta533_exit(void)
{
	i2c_del_driver(&nta533_driver);
}
module_exit(nta533_exit);

MODULE_DESCRIPTION("nfc Driver");
MODULE_LICENSE("GPL v2");
