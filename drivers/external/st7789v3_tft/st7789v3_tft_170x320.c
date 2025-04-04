// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#include <linux/cdev.h>
#include <linux/reboot.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/delay.h>
#include <asm-generic/delay.h>

#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <uapi/linux/ioctl.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/amlogic/pm.h>

#include "st7789v3_tft_170x320.h"

struct st7789v3_gpio_t *st7789v3_gpio;
struct spi_device   *st7789v3_spi;

//#define CLONE_KERNEL    (CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
#define ST7789V3_DEBUG(format, ...) //printk("%s] %d " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

static int st7789v3_write(struct spi_device *device, size_t len, unsigned char *buf)
{
	struct spi_transfer st[1];
	struct spi_message  msg;
	char *dma_buf = NULL;
	int ret;

	dma_buf = kzalloc(len, GFP_KERNEL | GFP_DMA);
	memcpy(dma_buf, buf, len);

	ST7789V3_DEBUG("%s start\n", __func__);
	spi_message_init(&msg);

	memset(st, 0, sizeof(st));
	st[0].tx_buf = (void *)dma_buf;
	st[0].rx_buf = NULL;
	st[0].len = len;
	ST7789V3_DEBUG();
	spi_message_add_tail(&st[0], &msg);

	ret = spi_sync(device, &msg);
	kfree(dma_buf);
	if (ret < 0) {
		ST7789V3_DEBUG("ret < %d\n", ret);
		return -1;
	} else {
		ST7789V3_DEBUG("ok\n");
		return msg.actual_length;
	}
}

static void st7789v3_senddata(unsigned char data_buf)
{
	unsigned char buf[1] = {0};

	buf[0] = data_buf;
	ST7789V3_DEBUG("Enter");
	st7789v3_write(st7789v3_spi, 1, buf);
}

static void st7789v3_Write_Index(unsigned char cmd)
{
	ST7789V3_DEBUG("Enter");
	gpio_direction_output(st7789v3_gpio->gpio_rs_pin, 0);
	st7789v3_senddata(cmd);
	gpio_direction_output(st7789v3_gpio->gpio_rs_pin, 1);
}

static void st7789v3_Write_Data(unsigned char dat)
{
	ST7789V3_DEBUG("Enter");
	st7789v3_senddata(dat);
}

void windows_set(unsigned int xs, unsigned int ys, unsigned int xe, unsigned int ye)
{
	ST7789V3_DEBUG("Enter");
	if (st7789v3_gpio->display_mode == 0) {	//vertical mode
		st7789v3_Write_Index(0x2A);
		st7789v3_Write_Data(xs >> 8);
		st7789v3_Write_Data(0x00FF & (xs + 35));
		st7789v3_Write_Data(xe >> 8);
		st7789v3_Write_Data(0x00FF & (xe + 35));

		st7789v3_Write_Index(0x2B);
		st7789v3_Write_Data(ys >> 8);
		st7789v3_Write_Data(0x00FF & (ys + 0));
		st7789v3_Write_Data(ye >> 8);
		st7789v3_Write_Data(0x00FF & (ye + 319));
	} else {								//horizontal mode
		st7789v3_Write_Index(0x2A);
		st7789v3_Write_Data(xs >> 8);
		st7789v3_Write_Data(0x00FF & (xs + 0));
		st7789v3_Write_Data(xe >> 8);
		st7789v3_Write_Data(0x00FF & (xe + 319));

		st7789v3_Write_Index(0x2B);
		st7789v3_Write_Data(ys >> 8);
		st7789v3_Write_Data(0x00FF & (ys + 35));
		st7789v3_Write_Data(ye >> 8);
		st7789v3_Write_Data(0x00FF & (ye + 35));
	}

	st7789v3_Write_Index(0x2C);
}

static void st7789v3_data_init(void)
{
	// int i = 100;
	// int value = 0;
	ST7789V3_DEBUG("Enter");
	ST7789V3_DEBUG("20240907-1055");
	st7789v3_Write_Index(0x11);
	ST7789V3_DEBUG();
	st7789v3_Write_Index(0xB2);
	st7789v3_Write_Data(0x0C);
	st7789v3_Write_Data(0x0C);
	st7789v3_Write_Data(0x00);
	st7789v3_Write_Data(0x33);
	st7789v3_Write_Data(0x33);

	st7789v3_Write_Index(0x35);
	st7789v3_Write_Data(0x00);

	st7789v3_Write_Index(0x36);
	if (st7789v3_gpio->display_mode == 0)
		st7789v3_Write_Data(0x00);
	else
		st7789v3_Write_Data(0x20 | 0x40);

	st7789v3_Write_Index(0x3A);
	st7789v3_Write_Data(0x05);

	st7789v3_Write_Index(0xB7);
	st7789v3_Write_Data(0x55);

	st7789v3_Write_Index(0xBB);
	st7789v3_Write_Data(0x1D);

	st7789v3_Write_Index(0xC0);
	st7789v3_Write_Data(0x2C);

	st7789v3_Write_Index(0xC2);
	st7789v3_Write_Data(0x01);

	st7789v3_Write_Index(0xC3);
	st7789v3_Write_Data(0x13);

	st7789v3_Write_Index(0xC6);
	st7789v3_Write_Data(0x0F);

	st7789v3_Write_Index(0xD0);
	st7789v3_Write_Data(0xA7);

	st7789v3_Write_Index(0xD0);
	st7789v3_Write_Data(0xA4);
	st7789v3_Write_Data(0xA1);

	st7789v3_Write_Index(0xE0);
	st7789v3_Write_Data(0xF0);
	st7789v3_Write_Data(0x05);
	st7789v3_Write_Data(0x0D);
	st7789v3_Write_Data(0x0B);
	st7789v3_Write_Data(0x0A);
	st7789v3_Write_Data(0x07);
	st7789v3_Write_Data(0x31);
	st7789v3_Write_Data(0x44);
	st7789v3_Write_Data(0x48);
	st7789v3_Write_Data(0x37);
	st7789v3_Write_Data(0x13);
	st7789v3_Write_Data(0x12);
	st7789v3_Write_Data(0x2E);
	st7789v3_Write_Data(0x35);

	st7789v3_Write_Index(0xE1);
	st7789v3_Write_Data(0xF0);
	st7789v3_Write_Data(0x06);
	st7789v3_Write_Data(0x0A);
	st7789v3_Write_Data(0x08);
	st7789v3_Write_Data(0x07);
	st7789v3_Write_Data(0x23);
	st7789v3_Write_Data(0x31);
	st7789v3_Write_Data(0x43);
	st7789v3_Write_Data(0x47);
	st7789v3_Write_Data(0x18);
	st7789v3_Write_Data(0x14);
	st7789v3_Write_Data(0x14);
	st7789v3_Write_Data(0x2F);
	st7789v3_Write_Data(0x36);

	st7789v3_Write_Index(0x21);
	st7789v3_Write_Index(0x29);
	ST7789V3_DEBUG();
	if (st7789v3_gpio->display_mode == 0)
		windows_set(0, 0, (170 - 1), (320 - 1));	//vertical mode
	else
		windows_set(0, 0, 319, 169);		// horizontal mode
}

static int st7789v3_open(struct inode *inode, struct file *file)
{
	ST7789V3_DEBUG("st7789v3_open init ok.\n");
	file->private_data = st7789v3_spi;
	return 0;
}

static ssize_t st7789v3_file_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t st7789v3_file_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	char *kbuf;
	unsigned int x[4] = {0};
	ST7789V3_DEBUG(" st7789v3_file_write init ok.\n");
	kbuf = kzalloc(count, GFP_KERNEL | GFP_DMA);
	// kbuf = kzalloc(count, GFP_KERNEL);
	if (!kbuf) {
		ST7789V3_DEBUG(" st7789v3 kbuf kzalloc error.\n");
		return -1;
	}
	ret = copy_from_user(kbuf, user_buf, count);
	if (ret) {
		pr_err("%s: write error\n", __func__);
		return -EIO;
	}
	if (count == 1) {
		x[0] = *kbuf;
		// if(x[0] == 0) {
		//	gpio_direction_output(st7789v3_gpio->gpio_pwm_pin, 0);
		//}
		//else if(x[0] == 1){
		//	gpio_direction_output(st7789v3_gpio->gpio_pwm_pin, 1);
		//}
	} else {
		//x[0] = *(kbuf+count - 4);
		//x[1] = *(kbuf+count - 3);
		//x[2]=*(kbuf+count - 2);
		//x[3]=*(kbuf+count - 1);
		//windows_set(x[0], x[1], x[2]-1, x[3]-1);
		//windows_set(0, 0, 169, 319);
		//st7789v3_write(st7789v3_spi, count-4, (unsigned char *)kbuf);
		st7789v3_write(st7789v3_spi, count, (unsigned char *)kbuf);
	}
	kfree(kbuf);
	return count;
}

static int st7789v3_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/**************************************************************/
static struct file_operations st7789v3_fops = {
	.owner   = THIS_MODULE,
	.open    = st7789v3_open,
	.read    = st7789v3_file_read,
	.write   = st7789v3_file_write,
	.release = st7789v3_release,
};

static struct miscdevice st7789v3_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "st7789v3",
	.fops		= &st7789v3_fops,
};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void st7789v3_early_suspend(struct early_suspend *h)
{
	// gpio_direction_output(st7789v3_gpio->gpio_pwm_pin, 0);
	ST7789V3_DEBUG("st7789v3 early suspend\n");
}

static void st7789v3_late_resume(struct early_suspend *h)
{
	// gpio_direction_output(st7789v3_gpio->gpio_pwm_pin, 1);
	ST7789V3_DEBUG("st7789v3 early resume\n");
}

static struct early_suspend st7789v3_early_suspend_handler;

#endif

static int st7789v3_probe(struct spi_device *spi)
{
	// struct gpio_desc *sw_desc = NULL;
	struct device_node *pdev = spi->dev.of_node;
	enum of_gpio_flags flags;
	int ret;
	if (!pdev) {
		ST7789V3_DEBUG("st7789v3 node is null.\n");
		return -1;
	}

	ST7789V3_DEBUG("st7789v3 spi panel init.\n");
	st7789v3_gpio = kzalloc(sizeof(*st7789v3_gpio), GFP_KERNEL);
	if (!st7789v3_gpio) {
		ST7789V3_DEBUG("st7789v3_gpio is null.\n");
		return -1;
	}

	ret = of_property_read_u32(pdev, "display_mode", &st7789v3_gpio->display_mode);
	if (ret < 0) {
		st7789v3_gpio->display_mode = 1;
		ST7789V3_DEBUG("can not get display_mode from dts. use default value. display_mode = %d\n", st7789v3_gpio->display_mode);
	} else {
		ST7789V3_DEBUG("display_mode = %d\n", st7789v3_gpio->display_mode);
	}

	st7789v3_gpio->gpio_rs_pin = 0;

	ST7789V3_DEBUG("st7789v3 spi panel init.\n");
	st7789v3_gpio->gpio_rs_pin = of_get_named_gpio_flags(pdev, "gpio_rs", 0, &flags);
	if (st7789v3_gpio->gpio_rs_pin < 0) {
		ST7789V3_DEBUG("st7789v3_probe: error cannot map to gpio_rs_pin!\n");
		kfree(st7789v3_gpio);
		return -1;
	} else if (gpio_request(st7789v3_gpio->gpio_rs_pin, "gpio_rs") < 0) {
		ST7789V3_DEBUG("st7789v3_gpio: gpio_request  gpio_rs_pin error\n");
		kfree(st7789v3_gpio);
		return -1;
	}

	ST7789V3_DEBUG("st7789v3_gpio:gpio_rs_pin:%d\n", st7789v3_gpio->gpio_rs_pin);
	st7789v3_gpio->gpio_rst_pin = 0;
	st7789v3_gpio->gpio_rst_pin = of_get_named_gpio_flags(pdev, "gpio_rst", 0, &flags);
	if (st7789v3_gpio->gpio_rst_pin < 0) {
		ST7789V3_DEBUG("st7789v3_probe: error cannot map to gpio_rst_pin!\n");
		kfree(st7789v3_gpio);
		return -1;
	} else if (gpio_request(st7789v3_gpio->gpio_rst_pin, "gpio_rst") < 0) {
		ST7789V3_DEBUG("st7789v3_gpio: gpio_request  gpio_rst_pin error\n");
		kfree(st7789v3_gpio);
		return -1;
	}

	ST7789V3_DEBUG("st7789v3_gpio:gpio_rst_pin:%d\n", st7789v3_gpio->gpio_rst_pin);

	//st7789v3_gpio->gpio_fmark_pin = 0;
	//st7789v3_gpio->gpio_fmark_pin = of_get_named_gpio_flags(pdev, "gpio_fmark", 0, &flags);
	//if (st7789v3_gpio->gpio_fmark_pin<0) {
	//	ST7789V3_DEBUG("st7789v3_probe: error cannot map to gpio_fmark_pin!\n");
	//	kfree(st7789v3_gpio);
	//	return -1;
	//}
	//else if (gpio_request(st7789v3_gpio->gpio_fmark_pin, "gpio_fmark") < 0) {
	//	ST7789V3_DEBUG("st7789v3_gpio: gpio_request  gpio_fmark_pin error\n");
	//	kfree(st7789v3_gpio);
	//	return -1;
	//}

	//ST7789V3_DEBUG("st7789v3_gpio:gpio_fmark_pin:%d\n", st7789v3_gpio->gpio_fmark_pin);

	gpio_direction_output(st7789v3_gpio->gpio_rs_pin, 1);
	gpio_direction_output(st7789v3_gpio->gpio_rst_pin, 1);
	// gpio_direction_input(st7789v3_gpio->gpio_fmark_pin);
	// gpio_direction_output(st7789v3_gpio->gpio_pwm_pin, 0);
	ST7789V3_DEBUG();
	st7789v3_spi = spi;
	spi_set_drvdata(spi, st7789v3_spi);
	gpio_direction_output(st7789v3_gpio->gpio_rst_pin, 0);
	ret = misc_register(&st7789v3_device);
	if (ret) {
		pr_err("mic: misc_register failed\n");
	}
	ST7789V3_DEBUG();
	#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	st7789v3_early_suspend_handler.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
	st7789v3_early_suspend_handler.suspend = st7789v3_early_suspend;
	st7789v3_early_suspend_handler.resume = st7789v3_late_resume;
	st7789v3_early_suspend_handler.param = st7789v3_spi;
	register_early_suspend(&st7789v3_early_suspend_handler);
	#endif
	gpio_direction_output(st7789v3_gpio->gpio_rst_pin, 1);
	ST7789V3_DEBUG();
	st7789v3_data_init();
	ST7789V3_DEBUG("Probe success");
	return 0;
}

static int st7789v3_remove(struct spi_device *spi)
{
	ST7789V3_DEBUG("Enter");
	// gpio_free(st7789v3_gpio->gpio_fmark_pin);
	gpio_free(st7789v3_gpio->gpio_rst_pin);
	gpio_free(st7789v3_gpio->gpio_rs_pin);

	unregister_early_suspend(&st7789v3_early_suspend_handler);
	misc_deregister(&st7789v3_device);
	// gpio_free(st7789v3_gpio->gpio_rs_pin);
	// gpio_free(st7789v3_gpio->gpio_rst_pin);
	kfree(st7789v3_gpio);
	ST7789V3_DEBUG("Exit");
	return 0;
}

static const struct of_device_id st7789v3_dt_match[] = {
	{	.compatible	=	"st7789v3",
	},
	{},
};

static struct spi_driver st7789v3_driver = {
	.probe = st7789v3_probe,
	.remove = st7789v3_remove,
	.driver = {
		.name = "st7789v3",
		.bus = &spi_bus_type,
		.of_match_table = st7789v3_dt_match,
	},
};

static int __init st7789v3_init(void)
{
	int error;
	error = spi_register_driver(&st7789v3_driver);
	if (error < 0)
		printk(KERN_ERR "spi_register_driver() returned %d\n", error);

	return error;
}

static void __exit st7789v3_exit(void)
{
	spi_unregister_driver(&st7789v3_driver);
}

module_init(st7789v3_init);
module_exit(st7789v3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lianglh@seirobotics.net");
MODULE_DESCRIPTION("st7789v3 driver");
