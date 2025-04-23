// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#ifndef __ST7789V3_H
#define __ST7789V3_H
#include <uapi/linux/ioctl.h>
#include <linux/pwm.h>

#define ST7789V3_DEVICE_NAME  		"st7789v3"
// #define ST7789V3_NODE_NAME    		"st7789v3"
#define ST7789V3_CLASS_NAME   		"st7789v3_class"

#define MAGIC_NUM 'M'

typedef struct {
	unsigned char x0;
	unsigned char x;
	unsigned char y0;
	unsigned char y;
	unsigned char *buf;
} st7735_app;

#define SPI_SET	_IOW('M', 88, st7735_app)
#define SPI_OPEN	_IOW('M', 89, st7735_app)
#define SPI_CLOSE	_IOW('M', 90, st7735_app)

enum {
	MIC_DISABLE = 0,
	MIC_ENABLE
};

#define BACKLIGHT_CONTROL_NONE 0
#define BACKLIGHT_CONTROL_GPIO 1
#define BACKLIGHT_CONTROL_PWM  2

struct st7789v3_plat_data_t {
//	unsigned int mute_ctl_pre_pin;
	unsigned int gpio_sda_pin;
	unsigned int gpio_rs_pin;
	unsigned int gpio_cs_pin;
	unsigned int gpio_clk_pin;
	unsigned int gpio_rst_pin;
	unsigned int gpio_fmark_pin;
	unsigned int gpio_pwm_pin;
	unsigned int gpio_pwm_pin_active_low;
	unsigned int backlight_control_type;

	struct pwm_device	*pwm;
	struct pwm_state	pwmstate;
	unsigned int 		brightness;
	unsigned int 		default_brightness;
	unsigned int 		duty_cycle;

	//0: vertical, 1: horizontal
	unsigned int 		display_mode;
	//1: skip init, 0: init [default]
	unsigned int 		init_mode;
	//debug level 0x0001, 0x002 0x0004 0x0008 etc..
	unsigned int 		debug_level;
};

#endif	//#ifndef __ST7789V3_TFT_170X320_H__
