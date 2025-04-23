// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/leds.h>

#include "htu20d.h"

int htu20d_read(struct i2c_adapter *adapt, u8 cmd, u8 *buf)
{
	struct i2c_msg msg[2];

	msg[0].flags = 0;
	msg[0].addr  = TEST_DEV_ADDR;
	msg[0].len   = 1;
	msg[0].buf   = &cmd;

	msg[1].flags = I2C_M_RD;
	msg[1].addr  = TEST_DEV_ADDR;
	msg[1].len   = 3;
	msg[1].buf   = buf;

	return i2c_transfer(adapt, &msg[0], 2);
}

int htu20d_read2(struct i2c_adapter *adapt, u8 *buf)
{
	struct i2c_msg msg;

	msg.flags = I2C_M_RD;
	msg.addr  = TEST_DEV_ADDR;
	msg.len   = 3;
	msg.buf   = buf;

	return i2c_transfer(adapt, &msg, 1);
}


static int  htu20d_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int  htu20d_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t htu20d_temperature_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t size)
{
    return size;
}

static ssize_t htu20d_temperature_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	int temp;
	int temperature;
	ssize_t len = 0;
	u8 pc[3];
	pc[0] = 0x00;
	pc[1] = 0x00;
	pc[2] = 0x00;
	i2c_smbus_write_byte(htu20d,0xF3);
	mdelay(50);
	htu20d_read2(htu20d->adapter,pc);
	pr_info("htu20d i2c read temperature %0x %0x %0x \n", pc[0], pc[1], pc[2]);
	temp=pc[0]*256+pc[1];
	temperature = (temp*17572) / 65536 - 4685;
    len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", temperature);

    return len;

}

static ssize_t htu20d_humidity_store(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t size)
{
	return size;
}

static ssize_t htu20d_humidity_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	int humi;
	int humidity;
	ssize_t len = 0;
	char pc[3];
	pc[0] = 0x00;
	pc[1] = 0x00;
	pc[2] = 0x00;
	i2c_smbus_write_byte(htu20d,0xF5);
	mdelay(50);
	htu20d_read2(htu20d->adapter,pc);
	pr_info("htu20d i2c read humidity %0x %0x %0x \n", pc[0], pc[1], pc[2]);
	humi=pc[0] * 256 + pc[1];
	humidity =(humi*125) / 65536 - 6;

    len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", humidity);

    return len;
}


static const struct file_operations htu20d_fops = {
	.owner		= THIS_MODULE,
	.open		= htu20d_open,
	.release	= htu20d_release,
};

static struct miscdevice htu20d_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "htu20d",
	.fops		= &htu20d_fops,
};


static DEVICE_ATTR(temperature, S_IWUSR | S_IRUGO, htu20d_temperature_show, htu20d_temperature_store);
static DEVICE_ATTR(humidity, S_IWUSR | S_IRUGO, htu20d_humidity_show, htu20d_humidity_store);


static struct attribute *htu20d_attributes[] = {
    &dev_attr_temperature.attr,
	&dev_attr_humidity.attr,
    NULL
};

static struct attribute_group htu20d_attribute_group = {
    .attrs = htu20d_attributes
};


static int htu20d_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret;
	htu20d = i2c;
	ret= misc_register(&htu20d_device);
	if (ret) {
		pr_err("htu20d: misc_register failed\n");
	} else {
		ret=sysfs_create_group(&htu20d_device.this_device->kobj, &htu20d_attribute_group);
		if(ret){
			pr_err("%s sysfs_create_group  error\n", __func__);
		}
		pr_info("htu20d]%s:%d misc_register success\n", __func__,__LINE__);
	}
    return 0;
}

static int htu20d_i2c_remove(struct i2c_client *i2c)
{

    return 0;
}

static const struct i2c_device_id htu20d_i2c_id[] = {
    { "htu20d", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, htu20d_i2c_id);

static struct of_device_id htu20d_dt_match[] = {
    { .compatible = "sei,htu20d" },
    { },
};

static struct i2c_driver htu20d_i2c_driver = {
    .driver = {
        .name = "htu20d",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(htu20d_dt_match),
    },
    .probe = htu20d_i2c_probe,
    .remove = htu20d_i2c_remove,
    .id_table = htu20d_i2c_id,
};


static int __init htu20d_i2c_init(void)
{
    int ret = 0;

    ret = i2c_add_driver(&htu20d_i2c_driver);
    if(ret){
        pr_err("fail to add htu20d device into i2c\n");
        return ret;
    }

    return 0;
}
module_init(htu20d_i2c_init);


static void __exit htu20d_i2c_exit(void)
{

    i2c_del_driver(&htu20d_i2c_driver);
}
module_exit(htu20d_i2c_exit);


MODULE_DESCRIPTION("htu20d Driver");
MODULE_LICENSE("GPL v2");
