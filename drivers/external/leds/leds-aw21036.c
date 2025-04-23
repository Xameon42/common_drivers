/*
 * leds-aw21036.c   aw21036 led module
 *
 * Version: 1.0.0
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

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
#include <linux/uaccess.h>
#include <linux/leds.h>

#include "leds-aw21036.h"

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) printk(KERN_DEBUG "leds-aw21036: " fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...) do {} while (0)
#endif

#define AW21036_I2C_NAME "aw21036_led"	

#define AW21036_VERSION "v1.0.0"

#define AW_I2C_RETRIES 3
#define AW_READ_CHIPID_RETRIES 3
#define AW21036_COLORS_COUNT	3
#define AW21036_MAX_IO	36		//LED 24

#define REG_GCR      		0x00

//*******************************************************BR
//LED(R_G_B):LED1-LED5, nums:5*3=15
#define REG_BR0        		0x01	//1+0
//LED(R_G_B)：LED6-LED8, nums:3*3=9
#define REG_BR24        	0x19	//1+24

//LED(R_G_B):LED9, nums:1*3=3
#define REG_BR33        	0x22	//1+33

//LED(R_G_B):RCU - RJ45 - WIFI, 3*(1*3)= 9
#define REG_BR15        	0x10
#define REG_BR18        	0x13
#define REG_BR21        	0x16

#define REG_UPDATE          0x49	//Write 0x00 to update BR

//*******************************************************COL
//LED(R_G_B):LED1-LED5, nums:5*3=15
#define REG_COL0        	0x4A	//(74+0)
//LED(R_G_B):LED6-LED8, nums:3*3=9
#define REG_COL24        	0x62 	//(74+24)

//LED(R_G_B):LED9, nums:1*3=3
#define REG_COL33        	0x6B 	//(74+33)

//LED(R_G_B):RCU - RJ45 - WIFI, 3*(1*3)= 9
#define REG_COL15        	89	//(74+15)
#define REG_COL18        	92	//(74+18)
#define REG_COL21        	95	//(74+21)

#define REG_OUTPUT_P1		0x6E	//Glogbal current control
#define REG_GCC			   	0x6E	//Glogbal current control

//*******************************************************

#define REG_ID              0x7F
#define REG_SWRST           0x7F

/* aw21036 register read/write access*/
#define REG_NONE_ACCESS                 0
#define REG_RD_ACCESS                   1 << 0
#define REG_WR_ACCESS                   1 << 1
#define AW21036_REG_MAX                  0xFF

const unsigned char aw21036_reg_access[AW21036_REG_MAX] = {
  [REG_GCR   ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_ID    ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_SWRST ] 	= REG_WR_ACCESS|REG_WR_ACCESS,
  [REG_BR0   ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_BR24  ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_BR33  ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_BR15  ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_BR18  ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_BR21  ] 	= REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_UPDATE	] 	= REG_RD_ACCESS|REG_WR_ACCESS,
};

/******************************************************
 * aw21036 i2c write
        client 			==> aw21036->i2c,
        sreg_addr:		==> start write bytes to reg_addr
        length      	==> write xxx bytes
        bufs			==> data
 *
 ******************************************************/
static int aw21036_i2c_writes(struct i2c_client *client, unsigned char sreg_addr, uint8_t length, uint8_t *bufs)
{
    int ret = -1;
    struct i2c_msg msg;
    int i,retries = 0;
    uint8_t data[64];

    data[0] = sreg_addr;
    for( i = 1; i <= length; i++ )
    {
        data[i] = bufs[i-1];
    }

    msg.addr  	= client->addr;
    msg.flags 	= !I2C_M_RD;
    msg.len   	= length+1;
    msg.buf   	= data;

    while(retries < 3) {
        ret = i2c_transfer(client->adapter, &msg, 1);
        if(ret >= 0)
        {
            ret = 1;
            break;
        }else {
            pr_err("[aw21036_i2c_writes]continue...\n");
        }
        retries++;
    }
    if(retries >= 3)
        pr_err("[aw21036_i2c_writes]error...retyies = %d\n",retries);

    return ret;
}

static void leds_aw21036b_work_func(struct work_struct *work){
    int i = 0, j = 0;

    while (0 == cycles_val){
        for (j = 0; j < instruct_val; j++)
        {
            if (platform_id == 5) {
                //LED-1~10:LED1~LED10
                aw21036_i2c_writes(aw21036->i2c, REG_BR0, aw21036->led_counts*3, &tmp_w[j][0]);
                usleep_range(1000*(time_val + 100 - 10), 1000 * (time_val + 100 + 10));
            } else {
                aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, tmp_w[j] + 0);
                aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, tmp_w[j] + 24);
                usleep_range(1000*(time_val - 10), 1000*(time_val + 10));
            }
            aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
            if(0 == state_mode)
                break;
        }
        if(0 == state_mode)
            break;
    }
    for (i = 1; i <= cycles_val; i++)
    {
        for (j = 0; j < instruct_val; j++)
        {
            if (platform_id == 5) {
                aw21036_i2c_writes(aw21036->i2c, REG_BR0, aw21036->led_counts*3, &tmp_w[j][0]);
            } else {
                aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, &tmp_w[j][0]);
                aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, &tmp_w[j][24]);
            }
            usleep_range(1000*(time_val - 10), 1000 * (time_val + 10));
            aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
            if (0 == state_mode)
                break;
        }
        if (0 == state_mode)
            break;
    }
    return;
}


static void init_leds_mode(int leds_mode)
{
    int offset,i,j;
    time_val = *(all_data[leds_mode] + 0);
    cycles_val = *((all_data[leds_mode]) + 1);
    instruct_val = (length_data[leds_mode] - 2) / 36;

    state_mode = 0;
    cancel_work_sync(&aw21036->leds_work);
    flush_workqueue(aw21036->leds_wq);

    DEBUG_PRINT("Tiger] %s: leds_mode=%d < time_val=%d, cycles_val=%d, instruct_val=%d >\n",__func__,leds_mode,time_val,cycles_val,instruct_val);

    for (i = 0; i < instruct_val; i++)
    {
        for(j=0;j<36;j++)
        {
            offset = 36*i+j;
            tmp_w[i][j] = *((all_data[leds_mode])+offset+2);
        }
    }

    state_mode = leds_mode;
    DEBUG_PRINT("Tiger]%s enter,Line:%d...state_mode=%d\n", __func__,__LINE__,state_mode);
    queue_work(aw21036->leds_wq, &aw21036->leds_work);
    DEBUG_PRINT("Tiger]%s enter,Line:%d...state_mode=%d\n", __func__,__LINE__,state_mode);

}

static void meson_aw21036_set_colors(unsigned int *colors, unsigned int counts)
{
    struct meson_aw21036_colors *color;
    struct meson_aw21036_io *io;
    unsigned char color_data[AW21036_MAX_IO];
    unsigned int i;

    DEBUG_PRINT("led count = %d \n", counts);
    memset(color_data, 0, sizeof(color_data));
    for (i = 0; i < counts; i++) {
        io = &aw21036->io[i];
        color = &aw21036->colors[i];
        color_data[io->b_io] = color->blue = colors[i] & 0xff;
        color_data[io->g_io] = color->green = (colors[i] >> 8) & 0xff;
        color_data[io->r_io] = color->red = (colors[i] >> 16) & 0xff;
    }
    DEBUG_PRINT("Tiger colors0 = 0x%x...3\n",colors[0]);
    if (platform_id == 5) { //for X5 ADT-5 raman-ai, total 10 rgb leds
        aw21036_i2c_writes(aw21036->i2c, REG_BR0, counts*3, &color_data[0]); //LED-1~10:LED1~LED10
    } else {
        //aw21036_i2c_writes(aw21036->i2c, REG_DIM00, AW21036_MAX_IO, color_data);
        aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, &color_data[0]); //LED-1~5:LED1~LED15
        aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, &color_data[24]); //LED-6~8:LED25~LED32
    }

    aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
}

static void init_leds_config(void)
{
    aw21036_hw_reset(aw21036);
    //aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //0x7F  SW_RSTN

    aw21036_i2c_write(aw21036, REG_GCR, 0x01);   //GCR: chip enable

    aw21036_i2c_write(aw21036, 0xAB, 0x00); //GCFG0: GEn = 0
    aw21036_i2c_write(aw21036, 0xAC, 0x08); //GCFG1: GCOLDIS = 1
    //aw21036_i2c_write(aw21036, 0x7A, 0x00); //GCR2: RGBMD = 0,disable
    //aw21036_i2c_write(aw21036, 0xA0, 0x08); //PATCFG : bit3 : SWITCH <0: LED off, 1: LED on>
    if (platform_id == 5) {
        aw21036_i2c_write(aw21036, REG_GCC, 0x70); //GCCR: 8bit
    } else {
        aw21036_i2c_write(aw21036, REG_GCC, 0xFF); //GCCR: 8bit
    }

    aw21036_i2c_writes(aw21036->i2c, REG_COL0, 36, tmp_col);

}

static void init_leds_data(void)
{
    all_data[0] = (int *)&data_mode0;
    all_data[1] = (int *)&data_mode1;
    all_data[2] = (int *)&data_mode2;
    all_data[3] = (int *)&data_mode3;
    all_data[4] = (int *)&data_mode4;
    all_data[5] = (int *)&data_mode5;
    all_data[6] = (int *)&data_mode6;
    all_data[7] = (int *)&data_mode7;

    all_data[8] = (int *)&data_mode8;
    all_data[9] = (int *)&data_mode9;
    all_data[10] = (int *)&data_mode10;
    all_data[11] = (int *)&data_mode11;

/*add X3 SEI600TID */
    all_data[12] = (int *)&data_mode12;
    all_data[13] = (int *)&data_mode13;

    length_data[0] = sizeof(data_mode0) / sizeof(data_mode0[0]);
    length_data[1] = sizeof(data_mode1) / sizeof(data_mode1[0]);
    length_data[2] = sizeof(data_mode2) / sizeof(data_mode2[0]);
    length_data[3] = sizeof(data_mode3) / sizeof(data_mode3[0]);
    length_data[4] = sizeof(data_mode4) / sizeof(data_mode4[0]);
    length_data[5] = sizeof(data_mode5) / sizeof(data_mode5[0]);
    length_data[6] = sizeof(data_mode6) / sizeof(data_mode6[0]);
    length_data[7] = sizeof(data_mode7) / sizeof(data_mode7[0]);
    length_data[8] = sizeof(data_mode8) / sizeof(data_mode8[0]);
    length_data[9] = sizeof(data_mode9) / sizeof(data_mode9[0]);
    length_data[10] = sizeof(data_mode10) / sizeof(data_mode10[0]);
    length_data[11] = sizeof(data_mode11) / sizeof(data_mode11[0]);

    /*add X3 SEI600TID */
    length_data[12] = sizeof(data_mode12) / sizeof(data_mode12[0]);
    length_data[13] = sizeof(data_mode13) / sizeof(data_mode13[0]);

    init_leds_config();

    if(platform_id == 3){
        init_leds_mode(12); //for SEI600TID
        //aw21036_i2c_writes(aw21036->i2c, REG_BR15, 9, tmp_ledon);
    }else if(platform_id == 2){
        init_leds_mode(1); //for SEI520
    }else if(platform_id == 4){
        init_leds_mode(1); //for SEI610F
    }else if (platform_id == 5){
        init_leds_mode(13); //for ATD-5 raman-ai
    }else{
        init_leds_mode(6);
    }
}

/******************************************************
 *
 * aw21036 i2c write/read
 *
 ******************************************************/
static int aw21036_i2c_write(struct meson_aw21036 *aw21036,
        unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw21036->i2c, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
    }

    return ret;
}

static int aw21036_i2c_read(struct meson_aw21036 *aw21036,
                            unsigned char reg_addr, unsigned int *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw21036->i2c, reg_addr);
        if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            *reg_data = ret;
            break;
        }
        cnt ++;
    }

    return ret;
}

static void aw21036_set_brightness(struct led_classdev *cdev,
        enum led_brightness brightness)
{
    struct meson_aw21036 *aw21036 = container_of(cdev, struct meson_aw21036, cdev);

    aw21036->cdev.brightness = brightness;

    //schedule_work(&aw21036->brightness_work);
}

static int aw21036_hw_reset(struct meson_aw21036 *aw21036)
{
    if (aw21036 && gpio_is_valid(aw21036->reset_gpio)) {
        gpio_set_value(aw21036->reset_gpio, 0);
        gpio_set_value(aw21036->reset_gpio, 1);
    } else {
        aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //0x7F  SW_RSTN
        dev_err(aw21036->dev, "%s: failed, switch to SW_RSTN!\n", __func__);
    }
    return 0;
}

static int aw21036_hw_off(struct meson_aw21036 *aw21036)
{
    DEBUG_PRINT("Tiger]%s enter,Line:%d\n", __func__,__LINE__);

    if (aw21036 && gpio_is_valid(aw21036->reset_gpio)) {
        gpio_set_value(aw21036->reset_gpio, 0);
    } else {
        //aw21036_i2c_write(aw21036, REG_SWRST, 0x00); //0x7F  SW_RSTN
        dev_err(aw21036->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

#ifndef CONFIG_REDUCE_TIME_CONSUME
/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw21036_read_chipid(struct meson_aw21036 *aw21036)
{
    int ret = -1;
    unsigned char cnt = 0;
    unsigned int reg_val = 0;

    while(cnt < AW_READ_CHIPID_RETRIES) {
        ret = aw21036_i2c_read(aw21036, REG_ID, &reg_val);
        if (ret < 0) {
            dev_err(aw21036->dev,
                "%s: failed to read register aw21036_REG_ID: %d\n",
                __func__, ret);
            return -EIO;
        }
        switch (reg_val) {
        case AW21036_ID:
            DEBUG_PRINT("Tiger]%s aw21036 detected\n", __func__);
            aw21036->chipid = AW21036_ID;
            return 0;
        default:
            break;
        }
        cnt ++;
    }

    return -EINVAL;
}
#endif


/******************************************************
 *
 * sys group attribute: mode
 *
 ******************************************************/
static ssize_t aw21036_mode_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    char *after;
    unsigned int state_mode = simple_strtoul(buf, &after, 10);
    {
        init_leds_mode(state_mode);
    }
    return count;
}

static ssize_t aw21036_mode_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "current mode:%d\n",state_mode);

    return len;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw21036_reg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw21036_i2c_write(aw21036, (unsigned char)databuf[0], (unsigned char)databuf[1]);
    }

    return count;
}

static ssize_t aw21036_reg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
    ssize_t len = 0;
    unsigned char i = 0;
    unsigned int reg_val = 0;
    for(i = 0; i < AW21036_REG_MAX; i ++) {
        if(!(aw21036_reg_access[i]&REG_RD_ACCESS))
            continue;
        aw21036_i2c_read(aw21036, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}

static ssize_t aw21036_hwen_store(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(1 == databuf[0]) {
            aw21036_hw_reset(aw21036);
        }else if(2 == databuf[0]){ //sw-reset
            aw21036_i2c_write(aw21036, 0x7F, 0x00);   //0x7F  SW_RSTN
        }else if(3 == databuf[0]){ //sw-init
            init_leds_data();
        }else{
            aw21036_hw_off(aw21036);
        }

        hwen_id = databuf[0];
    }

    return count;
}

static ssize_t aw21036_hwen_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n", hwen_id);

    return len;
}

static ssize_t aw21036_isel_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        isel_id = databuf[0];
        aw21036_i2c_write(aw21036, 0x11, databuf[0]);
    }

    return count;
}

static ssize_t aw21036_isel_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "isel=%d\n", isel_id);

    return len;
}

static ssize_t aw21036_time_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%d", &databuf[0])) {
        time_val = databuf[0];
    }

    return count;
}

static ssize_t aw21036_time_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "time_val=%d\n", time_val);

    return len;
}

static ssize_t aw21036_cycles_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%d", &databuf[0])) {
        cycles_val = databuf[0];
    }

    DEBUG_PRINT("%s():%d cycles_val=%d\n", __func__, __LINE__, cycles_val);
    return count;
}

static ssize_t aw21036_cycles_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "cycles_val=%d\n", cycles_val);

    return len;
}

static ssize_t aw21036_colors_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev,
                        struct meson_aw21036, cdev);
    unsigned int colors[12] = {0};	//max io is 12, so 12 is enough
    int ret = -1, i;

    ret = sscanf(buf, "%x %x %x %x %x %x %x %x %x %x %x %x", &colors[0], &colors[1], &colors[2], &colors[3], &colors[4], &colors[5], &colors[6], &colors[7], &colors[8], &colors[9], &colors[10], &colors[11]);

    pr_info("ret = %d \n",ret);
    if (ret == 1) {
        for(i=1; i < 12; i++) {
            colors[i] = colors[0];
        }
    } else {
        for(i=0; i < ret; i++) {
            pr_info("colors[%d] = 0x%x\n", i, colors[i]);
        }
    }
    DEBUG_PRINT("Tiger colors0 = 0x%x...1\n",colors[0]);
    if (ret <= 0) {
        pr_info(" enter,Line:...set led colors fail\n");
        return count;
    }

    meson_aw21036_set_colors(colors, aw21036->led_counts);

    return count;
}

static ssize_t aw21036_colors_show(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
#if 0
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "current mode:%d\n", state_mode);
    return len;
#endif
    return 0;
}

static ssize_t aw21036_led1_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(databuf[0] == 0){			//off
            led_value[0] = 0;
            led_value[1] = 0;
            led_value[2] = 0;
        } else if(databuf[0] == 1){		//on
            led_value[0] = 0x68;
            led_value[1] = 0xdd;
            led_value[2] = 0xf5;
        }else{
            led_value[0] = (databuf[0] >> 16) & 0xFF;	//R
            led_value[1] = (databuf[0] >> 8) & 0xFF; 	//G
            led_value[2] = databuf[0] & 0xFF;			//B
        }

        //pr_info("Tiger]...%s:set mode=%d\n",__func__, databuf[0]);
        pr_info("Tiger]...%s:set R=%x,G=%x,B=%x\n",__func__, led_value[0], led_value[1], led_value[2]);

        aw21036_i2c_writes(aw21036->i2c, REG_BR21, 3, led_value); 
        aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
    }

    return count;
}

static ssize_t aw21036_led1_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    //struct led_classdev *led_cdev = dev_get_drvdata(dev);
    //struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
    ssize_t len = 0;

    aw21036_i2c_read(aw21036, 0x10, &ledr_value);
    aw21036_i2c_read(aw21036, 0x11, &ledg_value);
    aw21036_i2c_read(aw21036, 0x12, &ledb_value);

    len += snprintf(buf+len, PAGE_SIZE-len, "led1:%d %d %d\n", ledr_value, ledg_value, ledb_value);

    return len;
}

static ssize_t aw21036_led2_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(databuf[0] == 0){		//off
            led_value[0] = 0;
            led_value[1] = 0;
            led_value[2] = 0;
        } else if(databuf[0] == 1){	//on
            led_value[0] = 0x68;
            led_value[1] = 0xdd;
            led_value[2] = 0xf5;
        }else{
            led_value[0] = (databuf[0] >> 16) & 0xFF;	//R
            led_value[1] = (databuf[0] >> 8) & 0xFF; 	//G
            led_value[2] = databuf[0] & 0xFF;			//B
        }

        DEBUG_PRINT("Tiger]...%s:set R=%x,G=%x,B=%x\n",__func__, led_value[0], led_value[1], led_value[2]);

        aw21036_i2c_writes(aw21036->i2c, REG_BR18, 3, led_value);
        aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
    }

    return count;
}

static ssize_t aw21036_led2_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    //struct led_classdev *led_cdev = dev_get_drvdata(dev);
    //struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
    ssize_t len = 0;

    aw21036_i2c_read(aw21036, 0x13, &ledr_value);
    aw21036_i2c_read(aw21036, 0x14, &ledg_value);
    aw21036_i2c_read(aw21036, 0x15, &ledb_value);

    len += snprintf(buf+len, PAGE_SIZE-len, "led2:%d %d %d\n", ledr_value, ledg_value, ledb_value);

    return len;
}

static ssize_t aw21036_led3_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(databuf[0] == 0){			//off
            led_value[0] = 0;
            led_value[1] = 0;
            led_value[2] = 0;
        } else if(databuf[0] == 1){		//on
            led_value[0] = 0x68;
            led_value[1] = 0xdd;
            led_value[2] = 0xf5;
        }else{
            led_value[0] = (databuf[0] >> 16) & 0xFF;	//R
            led_value[1] = (databuf[0] >> 8) & 0xFF; 	//G
            led_value[2] = databuf[0] & 0xFF;			//B
        }

        DEBUG_PRINT("Tiger]...%s:set R=%x,G=%x,B=%x\n",__func__, led_value[0], led_value[1], led_value[2]);

        aw21036_i2c_writes(aw21036->i2c, REG_BR15, 3, led_value);
        aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
    }

    return count;
}

static ssize_t aw21036_led3_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;

    aw21036_i2c_read(aw21036, 0x16, &ledr_value);
    aw21036_i2c_read(aw21036, 0x17, &ledg_value);
    aw21036_i2c_read(aw21036, 0x18, &ledb_value);

    len += snprintf(buf+len, PAGE_SIZE-len, "led3:%d %d %d\n", ledr_value, ledg_value, ledb_value);

    return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw21036_reg_show, aw21036_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw21036_hwen_show, aw21036_hwen_store);
static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, aw21036_mode_show, aw21036_mode_store);
static DEVICE_ATTR(isel, S_IWUSR | S_IRUGO, aw21036_isel_show, aw21036_isel_store);
static DEVICE_ATTR(time_val, S_IWUSR | S_IRUGO, aw21036_time_val_show, aw21036_time_val_store);
static DEVICE_ATTR(cycles_val, S_IWUSR | S_IRUGO, aw21036_cycles_val_show, aw21036_cycles_val_store);
static DEVICE_ATTR(colors, S_IWUSR | S_IRUGO, aw21036_colors_show, aw21036_colors_store);

static DEVICE_ATTR(led1, S_IWUSR | S_IRUGO, aw21036_led1_show, aw21036_led1_store);
static DEVICE_ATTR(led2, S_IWUSR | S_IRUGO, aw21036_led2_show, aw21036_led2_store);
static DEVICE_ATTR(led3, S_IWUSR | S_IRUGO, aw21036_led3_show, aw21036_led3_store);

static struct attribute *aw21036_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_mode.attr,
    &dev_attr_isel.attr,
    &dev_attr_time_val.attr,
    &dev_attr_cycles_val.attr,
    &dev_attr_colors.attr,
    &dev_attr_led1.attr,
    &dev_attr_led2.attr,
    &dev_attr_led3.attr,
    NULL
};

static struct attribute_group aw21036_attribute_group = {
    .attrs = aw21036_attributes
};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void aw21036b_early_suspend(struct early_suspend *h)
{
    //struct meson_aw21036 *ldev = (struct meson_aw21036 *)h->param;

    if(3 == platform_id){
        init_leds_mode(0);		//off
        //aw21036_i2c_write(aw21036, REG_OUTPUT_P1, 0xF0); //OOOO DDDD: 1111 0000
        //aw21036_i2c_writes(aw21036->i2c, REG_BR15, 9, tmp_ledoff);
        aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
    }else if(2 == platform_id){
        init_leds_mode(6);		//multi-colored horse
    }

    DEBUG_PRINT("Tiger]aw21036b early suspend\n");
}

static void aw21036b_late_resume(struct early_suspend *h)
{
    //struct meson_aw21036 *ldev = (struct meson_aw21036 *)h->param;
    if(3 == platform_id){
        init_leds_mode(12);		//user-blue
        //aw21036_i2c_write(aw21036, REG_OUTPUT_P1, 0x00); //OOOO DDDD: 0000 0000
        //aw21036_i2c_writes(aw21036->i2c, REG_BR15, 9, tmp_ledon);
        aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
    }else if(2 == platform_id){
        init_leds_mode(11);		//white
    }

    DEBUG_PRINT("Tiger]aw21036b early resume\n");
}

static struct early_suspend aw21036b_early_suspend_handler;
#endif

/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int aw21036_parse_led_cdev(struct meson_aw21036 *aw21036,
                   struct device_node *np)
{
    struct device_node *temp;
    unsigned int colors[AW21036_COLORS_COUNT];
    int ret = -1;
    int i = 0;

    for_each_child_of_node(np, temp) {
        ret = of_property_read_u32_array(temp, "default_colors",
                        colors, AW21036_COLORS_COUNT);
        if (ret < 0) {
            dev_err(aw21036->dev,
                "Failure reading default colors ret = %d\n", ret);
            goto free_pdata;
        }

        ret = of_property_read_u32(temp, "r_io_number",
                        &(aw21036->io[i].r_io));
        if (ret < 0) {
            dev_err(aw21036->dev,
                "Failure reading imax ret = %d\n", ret);
            goto free_pdata;
        }

        ret = of_property_read_u32(temp, "g_io_number",
                        &(aw21036->io[i].g_io));
        if (ret < 0) {
            dev_err(aw21036->dev,
                "Failure reading brightness ret = %d\n", ret);
            goto free_pdata;
        }

        ret = of_property_read_u32(temp, "b_io_number",
                        &(aw21036->io[i].b_io));
        if (ret < 0) {
            dev_err(aw21036->dev,
                "Failure reading max brightness ret = %d\n",
                ret);
            goto free_pdata;
        }

        aw21036->colors[i].red = colors[0];
        aw21036->colors[i].green = colors[1];
        aw21036->colors[i].blue = colors[2];
        i++;
    }

    //node = pdev->dev.of_node;
    ret = of_property_read_u32(np, "platform", &platform_id);
    if (ret < 0) {
        //dev_err(aw21036->dev, "Failure reading platform, ret = %d\n", ret);
        pr_info("Tiger] %s:%d default platform_id=%d\n", __func__, __LINE__, platform_id);
    }

    if(platform_id == 3)
        aw21036->cdev.name = "aw9523_led";	//For the same API as the previous aw9523b by SEI600TID
    else
        aw21036->cdev.name = "aw21036_led";

    aw21036->cdev.brightness = 0;
    aw21036->cdev.max_brightness = 255;
    aw21036->cdev.brightness_set = aw21036_set_brightness;
    ret = led_classdev_register(aw21036->dev, &aw21036->cdev);
    if (ret) {
        dev_err(aw21036->dev,
            "unable to register led ret=%d\n", ret);
        goto free_pdata;
    }

    ret = sysfs_create_group(&aw21036->cdev.dev->kobj,
            &aw21036_attribute_group);
    if (ret) {
        dev_err(aw21036->dev, "led sysfs ret: %d\n", ret);
        goto free_class;
    }

    return 0;

free_class:
    led_classdev_unregister(&aw21036->cdev);
free_pdata:
    return ret;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw21036_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
//    struct meson_aw21036 *aw21036;
    struct device_node *np = i2c->dev.of_node;

#ifndef CONFIG_REDUCE_TIME_CONSUME
    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C|I2C_FUNC_SMBUS_EMUL)) {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }
#endif
    aw21036 = devm_kzalloc(&i2c->dev, sizeof(struct meson_aw21036), GFP_KERNEL);
    if (aw21036 == NULL)
        return -ENOMEM;

    aw21036->dev = &i2c->dev;
    aw21036->i2c = i2c;

    aw21036->led_counts = device_get_child_node_count(&i2c->dev);
    aw21036->io = devm_kcalloc(&i2c->dev, aw21036->led_counts, sizeof(struct meson_aw21036_io),
                GFP_KERNEL);
    if (aw21036->io == NULL)
        return -ENOMEM;

    aw21036->colors = devm_kcalloc(&i2c->dev, aw21036->led_counts, sizeof(struct meson_aw21036_colors),
                GFP_KERNEL);
    if (aw21036->colors == NULL)
        return -ENOMEM;

    //i2c_set_clientdata(i2c, aw21036);
    //dev_set_drvdata(&i2c->dev, aw21036);
#ifndef CONFIG_REDUCE_TIME_CONSUME
    /* aw21036 chip id */
    ret = aw21036_read_chipid(aw21036);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: aw21036_read_chipid failed ret=%d\n", __func__, ret);
        return -EIO;
    }
#endif
    i2c_set_clientdata(i2c, aw21036);

    dev_set_drvdata(&i2c->dev, aw21036);

    aw21036_parse_led_cdev(aw21036, np);

    INIT_WORK(&aw21036->leds_work, leds_aw21036b_work_func);
    aw21036->leds_wq= create_singlethread_workqueue("leds_wq");
    //aw21036->leds_wq= create_workqueue("leds_wq");
    if (aw21036->leds_wq== NULL) {
        printk("create leds_work->input_wq fail!\n");
        return -ENOMEM;
    }
    queue_work(aw21036->leds_wq, &aw21036->leds_work);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    aw21036b_early_suspend_handler.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
    aw21036b_early_suspend_handler.suspend = aw21036b_early_suspend;
    aw21036b_early_suspend_handler.resume = aw21036b_late_resume;
    aw21036b_early_suspend_handler.param = aw21036;
    register_early_suspend(&aw21036b_early_suspend_handler);
#endif

    init_leds_data();

    return 0;
}

static int aw21036_i2c_remove(struct i2c_client *i2c)
{
    struct meson_aw21036 *aw21036 = i2c_get_clientdata(i2c);

    aw21036_hw_reset(aw21036);

    cancel_work_sync(&aw21036->leds_work);
    destroy_workqueue(aw21036->leds_wq);
    devm_kfree(&i2c->dev,aw21036);

    if (gpio_is_valid(aw21036->reset_gpio))
        gpio_free(aw21036->reset_gpio);

    return 0;
}

static const struct i2c_device_id aw21036_i2c_id[] = {
    { AW21036_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, aw21036_i2c_id);

static struct of_device_id aw21036_dt_match[] = {
    { .compatible = "seirobotics,aw21036_led" },
    { },
};

static struct i2c_driver meson_aw21036_driver = {
    .driver = {
        .name = AW21036_I2C_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(aw21036_dt_match),
    },
    .probe = aw21036_i2c_probe,
    .remove = aw21036_i2c_remove,
    .id_table = aw21036_i2c_id,
};

int led_aw21036_sei_init(void)
{
    return i2c_add_driver(&meson_aw21036_driver);
}
module_init(led_aw21036_sei_init);

void __exit led_aw21036_sei_exit(void)
{
    return i2c_del_driver(&meson_aw21036_driver);
}

module_exit(led_aw21036_sei_exit);

MODULE_AUTHOR("Tiger Hu <huyh@seirobotics.net>");
MODULE_DESCRIPTION("AW21036 LED Driver");
MODULE_LICENSE("GPL v2");
