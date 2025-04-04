// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/version.h>

#include "ntp8918.h"

#if NTP_DEBUG_EN

#define NTP_DEBUG_LEVEL     2

#if (NTP_DEBUG_LEVEL == 2)
#define NTP_DEBUG(fmt, args...) printk(KERN_INFO "[NTP8918][%s][%d] " fmt "\n", __func__, __LINE__, ##args)
#define NTP_ERR(fmt, args...) printk(KERN_ERR "[NTP8918][%s][%d] " fmt "\n", __func__, __LINE__, ##args)
#else
#define NTP_DEBUG(fmt, args...) printk(KERN_INFO "[NTP8918][%s] " fmt "\n", __func__, ##args)
#define NTP_ERR(fmt, args...) printk(KERN_ERR "[NTP8918][%s] " fmt "\n", __func__, ##args)
#endif

#define NTP_FUNC_ENTER(fmt, args...) printk(KERN_INFO "[NTP8918]%s: Enter(%d) " fmt "\n", __func__, __LINE__, ##args)
#define NTP_FUNC_EXIT(fmt, args...)  printk(KERN_INFO "[NTP8918]%s: Exit(%d) " fmt "\n", __func__, __LINE__, ##args)

#else
#define NTP_DEBUG(fmt, args...)
#define NTP_FUNC_ENTER()
#define NTP_FUNC_EXIT()
#endif

//32 44.1 48 96
#define ntp8918_RATES (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_64000)

//16 18 20 24
//S32 IC not support, auto drop MSB
#define ntp8918_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int ntp8918_hw_params(struct snd_pcm_substream *substream,
								struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	NTP_FUNC_ENTER();
	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		NTP_DEBUG("24bit");
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		NTP_DEBUG("20bit");
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		NTP_DEBUG("16bit");
		break;
	default:
		return -EINVAL;
	}
	NTP_FUNC_EXIT();
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int NTP_write1byte(struct snd_soc_codec *codec, const unsigned char array[][2], int count)
#else
static int NTP_write1byte(struct snd_soc_component *codec, const unsigned char array[][2], int count)
#endif
{
	int i = 0;
	int ret = 0;

	NTP_DEBUG("NTP_write1byte count:%d ", count);
	for (i = 0; i < count; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
		ret = snd_soc_write(codec, array[i][0], array[i][1]);
#else
		ret = snd_soc_component_write(codec, array[i][0], array[i][1]);
#endif
		if (ret != 0) {
			NTP_DEBUG("snd_soc_write 1 byte error,ret:%d", ret);
			return ret;
		}
	}
	NTP_FUNC_EXIT();
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int NTP_write4byte(struct snd_soc_codec *codec, const unsigned int array[][2], int count)
#else
static int NTP_write4byte(struct snd_soc_component *codec, const unsigned int array[][2], int count)
#endif
{
	int i = 0;
	int ret = 0;
	struct i2c_msg msg;
	int retries = 0;
	u8 data[5];

	NTP_DEBUG("NTP_write4byte count:%d ----- ", count);

	for (i = 0; i < count; i++) {
		retries = 0;
		data[0] = array[i][0];			/*Register start address<--write*/

		data[1] = (array[i][1] >> 24) & 0xff;
		data[2] = (array[i][1] >> 16) & 0xff;
		data[3] = (array[i][1] >> 8) & 0xff;
		data[4] = array[i][1] & 0xff;

		msg.addr  	= i2c_ntp8918->addr;
		msg.flags 	= !I2C_M_RD;
		msg.len   	= 5;
		msg.buf   	= data;

		while (retries < 3) {
			ret = i2c_transfer(i2c_ntp8918->adapter, &msg, 1);
			//NTP_DEBUG("[ntp8918]ret: %d", ret);
			if (ret >= 0) {
				ret = 1;
				break;
			} else {
				NTP_ERR("[ntp8918]continue...");
				if (retries >= 3) {
					NTP_ERR("retyies = %d", retries);
					return -retries;
				}
			}
			retries++;
		}
	}
	NTP_FUNC_EXIT();
	return ret;
}

static int assert_ntp8918(struct ntp8918_priv *ntp8918)
{
	int ret = -1;
	unsigned char cnt = 0;

	NTP_FUNC_ENTER();
	while (cnt < 5) {
		ret = i2c_smbus_read_byte_data(i2c_ntp8918, 0x0c); //Master Volume & SPK PWM Switching On/Off Control
		if (ret < 0) {
			NTP_DEBUG("ntp8918 is not exits!");
			//return -EIO;	//Try more
		} else {
			NTP_DEBUG("ntp8918 is exits! value: %d", ret);
			return 0;
		}
		cnt++;
		NTP_DEBUG("cnt: %d", cnt);
		msleep(10);
	}

	NTP_FUNC_EXIT();
	return -EINVAL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_reg_init(struct snd_soc_codec *codec)
#else
static int ntp8918_reg_init(struct snd_soc_component *codec)
#endif
{
	int ret = 0;
	struct ntp8918_priv *ntp8918 = NULL;
	NTP_FUNC_ENTER();
	if (!codec) {
		NTP_ERR("codec is null!");
		return -1;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	ntp8918 = snd_soc_codec_get_drvdata(codec);
#else
	ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	if (0 != ntp8918_assert_chip) {
		NTP_ERR("ntp8918 does not exist, so do not use init reg!");
		return -1;
	}

	if (3 == ntp8918->eq_power) {
		NTP_DEBUG("init eq power %d(w) ", ntp8918->eq_power);
		ret = NTP_write1byte(codec, section0, SECTION_0_MAX);
		ret = NTP_write4byte(codec, section1, SECTION_1_MAX);

		ret = NTP_write1byte(codec, section2, SECTION_2_MAX);
		ret = NTP_write4byte(codec, section3, SECTION_3_MAX);

		ret = NTP_write1byte(codec, section4, SECTION_4_MAX);
		ret = NTP_write4byte(codec, section5, SECTION_5_MAX);

		ret = NTP_write1byte(codec, section6, SECTION_6_MAX);
		ret = NTP_write1byte(codec, section7, SECTION_7_MAX);

		NTP_DEBUG("init work_mode %d ", ntp8918->work_mode);
		if (ntp8918->work_mode == 1) {
			snd_soc_component_write(codec, 0x68, 0x60);
			snd_soc_component_write(codec, 0x03, 0x4E);
			snd_soc_component_write(codec, 0x04, 0x00);
			snd_soc_component_write(codec, 0x05, 0x00);
			snd_soc_component_write(codec, 0x06, 0x4E);
			snd_soc_component_write(codec, 0x3E, 0x08); //note: warning: {0x3E, 0x00}, // PWM Mapping 1A to 1A/1B
			snd_soc_component_write(codec, 0x3F, 0x1A); //note: warning: {0x3F, 0x09}, // PWM Mapping 1B to 1A/1B
		}

		ret = NTP_write1byte(codec, section8, SECTION_8_MAX);
	} else if (5 == ntp8918->eq_power) {
		NTP_DEBUG("init eq power %d(w) ", ntp8918->eq_power);
		ret = NTP_write1byte(codec, section0_5, SECTION_0_5_MAX);
		ret = NTP_write4byte(codec, section1_5, SECTION_1_5_MAX);

		ret = NTP_write1byte(codec, section2_5, SECTION_2_5_MAX);
		ret = NTP_write4byte(codec, section3_5, SECTION_3_5_MAX);

		ret = NTP_write1byte(codec, section4_5, SECTION_4_5_MAX);
		ret = NTP_write4byte(codec, section5_5, SECTION_5_5_MAX);

		ret = NTP_write1byte(codec, section6_5, SECTION_6_5_MAX);
		ret = NTP_write1byte(codec, section7_5, SECTION_7_5_MAX);

		NTP_DEBUG("init work_mode %d ", ntp8918->work_mode);
		if (ntp8918->work_mode == 1) {
			snd_soc_component_write(codec, 0x68, 0x60);
			snd_soc_component_write(codec, 0x03, 0x4E);
			snd_soc_component_write(codec, 0x04, 0x00);
			snd_soc_component_write(codec, 0x05, 0x00);
			snd_soc_component_write(codec, 0x06, 0x4E);

			snd_soc_component_write(codec, 0x3E, 0x08); //note: warning: {0x3E, 0x00}, // PWM Mapping 1A to 1A/1B //BTL or PBTL
			snd_soc_component_write(codec, 0x3F, 0x1A); //note: warning: {0x3F, 0x09}, // PWM Mapping 1B to 1A/1B //BTL or PBTL
		}

		ret = NTP_write1byte(codec, section8_5, SECTION_8_5_MAX);
	}

	NTP_DEBUG("ret: %d", ret);
	NTP_FUNC_EXIT();
	return ret;
}

static int ntp8918_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	struct snd_soc_codec *codec = dai->codec;
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
#else
	struct snd_soc_component *codec = dai->component;
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	//struct ntp8918_platform_data *pdata = ntp8918->pdata;

	//if (1 == ntp8918->pdata->pd_pin_state) {
		//gpio_direction_output(pdata->pd_pin,1);	//pd_pin set high
		//mdelay(5);
	//}
	//gpiod_set_value(ntp8918->pdata->pd_gpio, 1);	//pd_gpio set high, speaker on
	//ntp8918_reg_init(codec);

	NTP_FUNC_EXIT("pd:%d, sw mute:%d", ntp8918->pdata->pd_pin_state, ntp8918->pdata->soft_mute);
	return 0;
}

static int ntp8918_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;
	}

	return 0;
}

static int ntp8918_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id, unsigned int freq, int dir)
{
	NTP_FUNC_ENTER();
	return 0;
}

static const struct snd_soc_dai_ops ntp8918_dai_ops = {
	.hw_params = ntp8918_hw_params,
	.set_sysclk = ntp8918_set_dai_sysclk,
	.set_fmt = ntp8918_set_dai_fmt,
	.prepare = ntp8918_prepare,
};

static struct snd_soc_dai_driver ntp8918_dai = {
	.name = "ntp8918",
	.playback = {
		.stream_name = "NTP Playback",
		.channels_min = 2,
		.channels_max = 16,
		.rates = ntp8918_RATES,
		.formats = ntp8918_FORMATS,
	},
	.ops = &ntp8918_dai_ops,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_probe(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

#else
static int ntp8918_probe(struct snd_soc_component *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);

#endif
	NTP_FUNC_ENTER();
#ifdef CONFIG_HAS_EARLYSUSPEND
	//struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

	//ntp8918->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	//ntp8918->early_suspend.suspend = ntp8918_early_suspend;
	//ntp8918->early_suspend.resume = ntp8918_late_resume;
	//ntp8918->early_suspend.param = codec;
	//register_early_suspend(&(ntp8918->early_suspend));
#endif

	if (codec) {
		ntp8918->codec = codec;
		snd_soc_component_set_drvdata(codec, ntp8918);
		NTP_DEBUG("Added snd component");
		ntp8918_reg_init(codec);
		//1: unmute, 0: mute
		if (ntp8918->spk_mute == 0) { 			//default on, only for test
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
			snd_soc_write(codec, 0x0c, 0x00);
			snd_soc_write(codec, 0x34, 0x03);
			snd_soc_write(codec, 0x35, 0x02);
#else
			snd_soc_component_write(codec, 0x0c, 0x00);
			snd_soc_component_write(codec, 0x34, 0x03);
			snd_soc_component_write(codec, 0x35, 0x02);

#endif
			NTP_DEBUG("default mute!");
		}

		if (ntp8918) {
			ntp8918->pdata->soft_mute = 0;
			NTP_DEBUG("soft_mute = %d", ntp8918->pdata->soft_mute);
		}
	}
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_remove(struct snd_soc_codec *codec)
#else
static void ntp8918_remove(struct snd_soc_component *codec)
#endif
{
	NTP_FUNC_ENTER();
#ifdef CONFIG_HAS_EARLYSUSPEND
	//struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	//unregister_early_suspend(&(ntp8918->early_suspend));
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	return 0;
#else
	//return 0;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_setmute(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
#else
static int ntp8918_setmute(struct snd_soc_component *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	int ret = 0;

	NTP_FUNC_ENTER();
	if (!ntp8918) {
		NTP_DEBUG("ntp8918 is null");
		return -1;
	}
	if (!codec) {
		NTP_DEBUG("ntp8918 codec is null");
		return -1;
	}

	if (ntp8918->pdata->soft_mute) {
		NTP_DEBUG("set soft speaker off");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
		ret = snd_soc_write(codec, 0x0c, 0xff);
		ret = snd_soc_write(codec, 0x34, 0x00);
		ret = snd_soc_write(codec, 0x35, 0x00);
#else
		ret = snd_soc_component_write(codec, 0x0c, 0xff);
		ret = snd_soc_component_write(codec, 0x34, 0x00);
		ret = snd_soc_component_write(codec, 0x35, 0x00);
#endif
		if (ntp8918->pdata->pd_pin_state) {
			gpiod_set_value(ntp8918->pdata->pd_gpio, 0);	//pd_gpio set low, speaker off
			NTP_DEBUG("pd_gpio set low, set hw speaker off");
		}
	} else {
		NTP_DEBUG("set soft speaker on");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
		ret = snd_soc_write(codec, 0x0c, 0x00);
		ret = snd_soc_write(codec, 0x34, 0x03);
		ret = snd_soc_write(codec, 0x35, 0x02);
#else
		ret = snd_soc_component_write(codec, 0x0c, 0x00);
		ret = snd_soc_component_write(codec, 0x34, 0x03);
		ret = snd_soc_component_write(codec, 0x35, 0x02);
#endif
		if (ntp8918->pdata->pd_pin_state) {
			gpiod_set_value(ntp8918->pdata->pd_gpio, 1);	//pd_gpio set high, speaker on
			NTP_DEBUG("pd_gpio set low, set hw speaker on");
		}
	}

	return 0;
}

static int ntp8918_power_on_timing(struct ntp8918_priv *ntp8918)
{
	if (1 == ntp8918->pdata->pd_pin_state) {
		gpiod_set_value(ntp8918->pdata->pd_gpio, 0);	//mute
		NTP_DEBUG("pd_gpio init mute ok");
	}

	if (1 == ntp8918->pdata->power_pin_state) {
		gpiod_set_value(ntp8918->pdata->power_gpio, 1);	//power on
		msleep(5);
		NTP_DEBUG("power_gpio power on ok");
	}

	if (1 == ntp8918->pdata->reset_pin_state) {
		gpiod_set_value(ntp8918->pdata->reset_gpio, 1); //reset timing
		msleep(4);
		gpiod_set_value(ntp8918->pdata->reset_gpio, 0);
		msleep(1);
		gpiod_set_value(ntp8918->pdata->reset_gpio, 1);
		msleep(4);
		NTP_DEBUG("reset_gpio init ok");
	}

	// ntp8918_assert_chip = assert_ntp8918(ntp8918);
	// if ((0 == ntp8918_assert_chip) && (1 == ntp8918->pdata->power_pin_state)) { // for resume
	// 	ntp8918_reg_init(ntp8918->codec);
	// 	NTP_DEBUG("init EQ ok");
	// } else {
	// 	NTP_DEBUG("ntp8918 check status err: %d", ntp8918_assert_chip);
	// }

	NTP_FUNC_EXIT("Hardware timing initialization is ok");
	return 0;
}

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_suspend(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	NTP_FUNC_ENTER();
#else
static int ntp8918_suspend(struct snd_soc_component *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	NTP_DEBUG("mute status:%d", ntp8918->pdata->soft_mute);

	// if (ntp8918->pdata->pd_pin_state) {
	// 	gpiod_set_value(ntp8918->pdata->pd_gpio, 0);
	// 	NTP_DEBUG("pd_gpio set 0");
	// }

	if (ntp8918->low_power && ntp8918->pdata->power_pin_state) {
		gpiod_set_value(ntp8918->pdata->power_gpio, 0);
		NTP_DEBUG("set power_gpio low");
	}
	NTP_FUNC_EXIT("ntp8918 enter low consumer");
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_resume(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
#else
static int ntp8918_resume(struct snd_soc_component *codec)
{
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	NTP_FUNC_ENTER();
	if (!ntp8918) {
		NTP_DEBUG("ntp8918 dev is null");
		return 0;
	}

	//NTP_DEBUG("mute status:%d", ntp8918->pdata->soft_mute);
	//ntp8918_reg_init(codec);
	if (NULL == ntp8918->codec) {
		NTP_DEBUG("ntp8918->codec dev is null");
	}

	if (ntp8918->low_power) {
		NTP_DEBUG("ntp8918 exit low consumer");
		ntp8918_power_on_timing(ntp8918);

		ntp8918_assert_chip = assert_ntp8918(ntp8918);
		if (/*(*/0 == ntp8918_assert_chip/*) && (1 == ntp8918->pdata->power_pin_state)*/) { // for resume
			ntp8918_reg_init(ntp8918->codec);
			NTP_DEBUG("init EQ ok");
		} else {
			NTP_DEBUG("ntp8918 check status err: %d", ntp8918_assert_chip);
		}
		ntp8918_setmute(codec);
		NTP_DEBUG("ntp8918 resume mute status: %d", ntp8918->pdata->soft_mute);
	}

	NTP_FUNC_EXIT();
	return 0;
}
#else
#define ntp8918_suspend NULL
#define ntp8918_resume NULL
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static int ntp8918_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
#else
static int ntp8918_set_bias_level(struct snd_soc_component *codec,
				  enum snd_soc_bias_level level)
#endif
{
	NTP_FUNC_ENTER();
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	codec->component.dapm.bias_level = level;
#else
	codec->dapm.bias_level = level;
#endif
	NTP_FUNC_EXIT();
	return 0;
}

static int ntp8918_set_mute(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_component_get_drvdata(codec);
#endif
	//int ret = 0;
	bool mute = !!ucontrol->value.integer.value[0];
	NTP_FUNC_ENTER();

	/*if (!codec) {
		NTP_DEBUG("codec is null;");
		return -1;
	}

	if (!ntp8918) {
		NTP_DEBUG("ntp8918 is null;");
		return -1;
	}

	if (mute) {
		NTP_DEBUG("set soft speaker off");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
		ret = snd_soc_write(codec, 0x0c, 0xff);
		ret = snd_soc_write(codec, 0x34, 0x00);
		ret = snd_soc_write(codec, 0x35, 0x00);
#else
		ret = snd_soc_component_write(codec, 0x0c, 0xff);
		ret = snd_soc_component_write(codec, 0x34, 0x00);
		ret = snd_soc_component_write(codec, 0x35, 0x00);
#endif
		if (ntp8918->pdata->pd_pin_state) {
			gpiod_set_value(ntp8918->pdata->pd_gpio, 0);	//pd_gpio set low, speaker off
			NTP_DEBUG("pd_gpio set low, set hw speaker off");
		}
	} else {
		NTP_DEBUG("set soft speaker on");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
		ret = snd_soc_write(codec, 0x0c, 0x00);
		ret = snd_soc_write(codec, 0x34, 0x03);
		ret = snd_soc_write(codec, 0x35, 0x02);
#else
		ret = snd_soc_component_write(codec, 0x0c, 0x00);
		ret = snd_soc_component_write(codec, 0x34, 0x03);
		ret = snd_soc_component_write(codec, 0x35, 0x02);
#endif
		if (ntp8918->pdata->pd_pin_state) {
			gpiod_set_value(ntp8918->pdata->pd_gpio, 1);	//pd_gpio set high, speaker on
			NTP_DEBUG("pd_gpio set low, set hw speaker on");
		}
	}*/
	NTP_DEBUG("mute %d", mute);
	ntp8918->pdata->soft_mute = mute;
	ntp8918_setmute(codec);
	NTP_FUNC_EXIT();
	return 0;
}

static int ntp8918_get_mute(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
#else
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
#endif
	struct ntp8918_priv *ntp8918  = snd_soc_component_get_drvdata(component);
    NTP_FUNC_ENTER();

	//printk("%s] Enter: %d", __func__, __LINE__);
	if (!ntp8918) {
		printk("ntp8918_get_mute] ntp8918 is null;");
		return -1;
	}

	ucontrol->value.integer.value[0] = ntp8918->pdata->soft_mute;
	printk("ntp8918_get_mute: %d", ntp8918->pdata->soft_mute);

	NTP_FUNC_EXIT();
	return 0;
}

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -10300, 50, 1);
//static const DECLARE_TLV_DB_SCALE(mfvol_tlv, -10300, 12.5, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static const struct snd_kcontrol_new ntp8918_snd_controls[] = {
	//SOC_SINGLE("SPK mute", SOFT_MUTE_CONTROL, 0, 3, 0),
	SOC_SINGLE_BOOL_EXT("SPK mute 1", 0, ntp8918_get_mute, ntp8918_set_mute),
	SOC_SINGLE_TLV("SPK Volume", MVOL, 0, 0xff, 1, mvol_tlv),
	//SOC_SINGLE_TLV("SPK Fine Volume", MFVOL, 0, 0xff, 1, mfvol_tlv),
	SOC_SINGLE_TLV("SPK Ch1 Volume", C1VOL, 0, 0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("SPK Ch2 Volume", C2VOL, 0, 0xff, 1, chvol_tlv),
};

static const struct snd_soc_dapm_widget ntp8918_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "NTP Playback", SND_SOC_NOPM, 0, 0),
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static const struct snd_soc_codec_driver soc_codec_dev_ntp8918_case0 = {
#else
static const struct snd_soc_component_driver soc_codec_dev_ntp8918_case0 = {
	.name = "ntp8918",
#endif
	.probe = ntp8918_probe,
	.remove = ntp8918_remove,
	.suspend = ntp8918_suspend,
	.resume = ntp8918_resume,
	.set_bias_level = ntp8918_set_bias_level,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
static const struct snd_soc_codec_driver soc_codec_dev_ntp8918 = {
#else
static const struct snd_soc_component_driver soc_codec_dev_ntp8918 = {
	.name = "ntp8918",
#endif
	.probe = ntp8918_probe,
	.remove = ntp8918_remove,
	.suspend = ntp8918_suspend,
	.resume = ntp8918_resume,
	.set_bias_level = ntp8918_set_bias_level,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	.component_driver = {
#else
#endif
		.controls = ntp8918_snd_controls,
		.num_controls = ARRAY_SIZE(ntp8918_snd_controls),
		.dapm_widgets = ntp8918_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ntp8918_dapm_widgets),
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	}
#else
#endif
};

static const struct regmap_config ntp8918_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = NTP8918_REGISTER_MAX,
	.reg_defaults = ntp8918_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(ntp8918_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int ntp8918_parse_dt(struct ntp8918_priv *ntp8918, struct device *dev)
{
	int ret = 0;
	int pd_pin = -1;
	int reset_pin = -1;
	int power_pin = -1;

	if (!ntp8918) {
		return -1;
	}

	//gpio configuration
	ntp8918->pdata->pd_gpio = devm_gpiod_get_optional(dev, "pd", GPIOD_OUT_LOW);
	NTP_DEBUG("IS_ERR: %d IS_ERR_OR_NULL: %d", IS_ERR(ntp8918->pdata->pd_gpio), IS_ERR_OR_NULL(ntp8918->pdata->pd_gpio));
	if (IS_ERR_OR_NULL(ntp8918->pdata->pd_gpio)) {
		NTP_DEBUG("Failed to allocate reset gpio : %d, try the legacy mode again", PTR_ERR(ntp8918->pdata->pd_gpio));
		pd_pin = of_get_named_gpio(dev->of_node, "pd_pin", 0);
		if (pd_pin < 0) {
			NTP_ERR("fail to get PD pin from dts!");
			ret = -1;
		} else {
			ret = gpio_request(pd_pin, "pd_pin");
			if (ret < 0) {
				NTP_ERR("GPIO pin is already allocated");
				//return;
			}

			if (gpio_is_valid(pd_pin)) {
				ntp8918->pdata->pd_gpio = gpio_to_desc(pd_pin);
				gpio_direction_output(pd_pin, 0);
				ntp8918->pdata->pd_pin_state = 1;
				NTP_DEBUG("pdata->pd_pin = %d", pd_pin);
			}
		}
	} else {
		ntp8918->pdata->pd_pin_state = 1;
		NTP_DEBUG("pd_gpio init value");
	}

	ntp8918->pdata->power_gpio = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_LOW);
	NTP_DEBUG("IS_ERR: %d IS_ERR_OR_NULL: %d", IS_ERR(ntp8918->pdata->power_gpio), IS_ERR_OR_NULL(ntp8918->pdata->power_gpio));
	if (IS_ERR_OR_NULL(ntp8918->pdata->power_gpio)) {
		NTP_DEBUG("Failed to allocate power gpio : %d, try the legacy mode again", PTR_ERR(ntp8918->pdata->power_gpio));
		power_pin = of_get_named_gpio(dev->of_node, "power_pin", 0); //control ntp8918 module vcc power.
		if (power_pin < 0) {
			NTP_ERR("fail to get power pin from dts!, no need power_pin");
			ret = -1;
		} else {
			ret = gpio_request(power_pin, "power_pin");
			if (ret < 0) {
				NTP_ERR("power pin GPIO is already allocated");
				//return;
			}

			if (gpio_is_valid(power_pin)) {
				ntp8918->pdata->power_gpio = gpio_to_desc(power_pin);
				gpio_direction_output(power_pin, 0);
				ntp8918->pdata->power_pin_state = 1;
				NTP_DEBUG("power_pin = %d", power_pin);
			}
		}
	} else {
		ntp8918->pdata->power_pin_state = 1;
		NTP_DEBUG("ntp8918]: power_gpio init value");
	}

	ntp8918->pdata->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	NTP_DEBUG("IS_ERR: %d IS_ERR_OR_NULL:%d", IS_ERR(ntp8918->pdata->reset_gpio), IS_ERR_OR_NULL(ntp8918->pdata->reset_gpio));
	if (IS_ERR_OR_NULL(ntp8918->pdata->reset_gpio)) {
		NTP_DEBUG("Failed to allocate reset gpio : %d, try the legacy mode again", PTR_ERR(ntp8918->pdata->reset_gpio));
		reset_pin = of_get_named_gpio(dev->of_node, "reset_pin", 0);
		if (reset_pin < 0) {
			NTP_ERR("fail to get reset pin from dts!");
			ret = -1;
		} else {
			ret = gpio_request(reset_pin, "reset_pin");
			if (ret < 0) {
				NTP_ERR("GPIO pin is already allocated");
				//return;
			}

			if (gpio_is_valid(reset_pin)) {
				ntp8918->pdata->reset_gpio = gpio_to_desc(reset_pin);
				gpio_direction_output(reset_pin, 0);
				ntp8918->pdata->reset_pin_state = 1;
				NTP_DEBUG("reset_pin = %d", reset_pin);
			}
		}
	} else {
		ntp8918->pdata->reset_pin_state = 1;
		NTP_DEBUG("reset_gpio init value");
	}

	//function configuration
	ret = of_property_read_u32(dev->of_node, "eq_power", &ntp8918->eq_power);
	if (ret) {
		ntp8918->eq_power = 3;  //default eq power is 3w
		NTP_ERR("not config eq_power,default: %d", ntp8918->eq_power);
	} else {
		NTP_DEBUG("config eq_power = %d", ntp8918->eq_power);
	}

	//work_mode: 0: BTC 1: PBTC
	ret = of_property_read_u32(dev->of_node, "work_mode", &ntp8918->work_mode);
	if (ret) {
		ntp8918->work_mode = 0;  //default work_mode 0
		 NTP_ERR("not config work_mode,default: %d", ntp8918->work_mode);
	} else {
		NTP_DEBUG("config work_mode = %d", ntp8918->work_mode);
	}

	// 1: low power 0: not low power
	ret = of_property_read_u32(dev->of_node, "low_power", &ntp8918->low_power);
	if (ret) {
		ntp8918->low_power = 0;  //default low_power
		NTP_ERR("not config low_power,default: %d", ntp8918->low_power);
	} else {
		NTP_DEBUG("config low_power = %d", ntp8918->low_power);
	}

	//dts: default not define or define spk_mute = <0> then ntp8918->spk_mute = 0 >| mute
	//define spk_mute = <1> >| unmute
	ret = of_property_read_u32(dev->of_node, "spk_mute", &ntp8918->spk_mute);
	if (ret) {
		ntp8918->spk_mute = 0;  //default mute
		NTP_ERR("not config spk_mute,default: %d", ntp8918->spk_mute);
	} else {
		NTP_DEBUG("config spk_mute = %d", ntp8918->spk_mute);
	}

	ntp8918_power_on_timing(ntp8918);
	ntp8918_assert_chip = assert_ntp8918(ntp8918);
	if ((1 == ntp8918->spk_mute) && (1 == ntp8918->pdata->pd_pin_state)) {
		//gpio_direction_output(ntp8918->pdata->pd_pin, 0);
		gpiod_set_value(ntp8918->pdata->pd_gpio, 1);
		NTP_DEBUG("spkmute is 0, speaker on");
	}
	NTP_FUNC_EXIT();
	return ret;
}

static int ntp8918_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct ntp8918_priv *ntp8918;
	struct ntp8918_platform_data *pdata;
	int ret;

	NTP_FUNC_ENTER();
	i2c_ntp8918 = i2c;

	ntp8918 = devm_kzalloc(&i2c->dev, sizeof(struct ntp8918_priv), GFP_KERNEL);
	if (!ntp8918) {
		NTP_ERR("devm_kzalloc is failed");
		return -ENOMEM;
	}

	ntp8918->regmap = devm_regmap_init_i2c(i2c, &ntp8918_regmap);
	if (IS_ERR(ntp8918->regmap)) {
		ret = PTR_ERR(ntp8918->regmap);
		NTP_ERR("Failed to allocate register map: %d", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, ntp8918);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct ntp8918_platform_data), GFP_KERNEL);
	if (!pdata) {
		NTP_ERR("failed to kzalloc for ntp8918 pdata");
		return -ENOMEM;
	}
	ntp8918->pdata = pdata;

	ret = ntp8918_parse_dt(ntp8918, &i2c->dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
    if (0 == ntp8918_assert_chip) {
		ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_ntp8918, &ntp8918_dai, 1);
    } else {
		ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_ntp8918_case0, &ntp8918_dai, 1);
	}
#else
	if (0 == ntp8918_assert_chip) {
		ret = devm_snd_soc_register_component(&i2c->dev, &soc_codec_dev_ntp8918, &ntp8918_dai, 1);
	} else {
		ret = devm_snd_soc_register_component(&i2c->dev, &soc_codec_dev_ntp8918_case0, &ntp8918_dai, 1);
	}
#endif

	if (ret != 0)
		NTP_ERR("Failed to register codec (%d)", ret);

	NTP_FUNC_EXIT("probe success, ret:(%d)", ret);
	return ret;
}

static int ntp8918_i2c_remove(struct i2c_client *client)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)//The Sei version is 4.9 or 5.4
	snd_soc_unregister_codec(&client->dev);
#else
	snd_soc_unregister_component(&client->dev);
#endif
	NTP_FUNC_EXIT();
	return 0;
}

static const struct i2c_device_id ntp8918_i2c_id[] = {
	{ "ntp8918", 0 },
	{}
};

static const struct of_device_id ntp8918_of_id[] = {
	{ .compatible = "NTP, ntp8918", },
	{ /* reserve */ }
};
MODULE_DEVICE_TABLE(of, ntp8918_of_id);

static struct i2c_driver ntp8918_i2c_driver = {
	.driver = {
		.name = "ntp8918",
		.of_match_table = ntp8918_of_id,
		.owner = THIS_MODULE,
	},
	.probe = ntp8918_i2c_probe,
	.remove = ntp8918_i2c_remove,
	.id_table = ntp8918_i2c_id,
};

module_i2c_driver(ntp8918_i2c_driver);

MODULE_DESCRIPTION("ASoC ntp8918 driver");
MODULE_AUTHOR("Mack zhang");
MODULE_LICENSE("GPL");
