// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include "../aml_dvb.h"
//#include "demod_gt.h"
#include <linux/amlogic/aml_dvb_extern.h>
#include <linux/amlogic/tee.h>

#include "sc2_control.h"
#include "../dmx_log.h"

/*
 *Bit 0        cfg_ts_mpeg                    RW       default = 'h0
 *Bit 1        cfg_ts_like                    RW       default = 'h0
 *Bit 2        cfg_ts_null                    RW       default = 'h1
 *Bit 3:7      rsvd_0                         RW       default = 'h0
 *Bit 8:20     cfg_ts_like_pid                RW       default = 'h2d
 *Bit 21:31    rsvd_1                         RW       default = 'h0
 */
union ALP_CFG_FIELD {
	unsigned int data;
	struct {
		unsigned int cfg_ts_mpeg:1;
		unsigned int cfg_ts_like:1;
		unsigned int cfg_ts_null:1;
		unsigned int rsvd_0:5;
		unsigned int cfg_ts_like_pid:13;
		unsigned int rsvd_1:11;
	} b;
};

#define TSIN_NUM		4

#define dprint_i(fmt, args...)  \
	dprintk(LOG_ERROR, debug_frontend, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_frontend, "fend:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_frontend, "fend:" fmt, ## args)

MODULE_PARM_DESC(debug_frontend, "\n\t\t Enable debug frontend information");
static int debug_frontend;
module_param(debug_frontend, int, 0644);

static u8 enable_tsinb_clk;

ssize_t ts_setting_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	int i;
	struct aml_dvb *dvb = aml_get_dvb_device();
	int ctrl;

	//print hardware TS input
	r = sprintf(buf, "tsin:\n");
	buf += r;
	total += r;

	for (i = 0; i < TSIN_NUM; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		ctrl = ts->control;

		r = sprintf(buf, "tsin%d %s control: 0x%x\n", i,
			    ts->mode == AM_TS_DISABLE ? "disable" :
			    (ts->mode == AM_TS_SERIAL_3WIRE ?
			     "serial-3wire" :
			     (ts->mode == AM_TS_SERIAL_4WIRE ?
			      "serial-4wire" : "parallel")), ctrl);
		buf += r;
		total += r;
	}

	//stream ts
	r = sprintf(buf, "ts stream:\n");
	buf += r;
	total += r;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		r = sprintf(buf, "ts_sid:0x%0x header_len:%d ",
			    ts->ts_sid, ts->header_len);
		buf += r;
		total += r;

		r = sprintf(buf, "header[0]:0x%0x sid_offset:%d\n",
			    ts->header[0], ts->sid_offset);
		buf += r;
		total += r;
	}
	return total;
}

ssize_t ts_setting_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	int id, ctrl, r, mode;
	char mname[32];
	char pname[32];
	struct aml_ts_input *ts;
	struct aml_dvb *dvb = aml_get_dvb_device();

	r = sscanf(buf, "%d %s %x", &id, mname, &ctrl);
	if (r != 3)
		return -EINVAL;

	if (id < 0 || id >= TSIN_NUM)
		return -EINVAL;

	if ((mname[0] == 's') || (mname[0] == 'S')) {
		sprintf(pname, "s_ts%d", id);
		if (mname[1] == '3')
			mode = AM_TS_SERIAL_3WIRE;
		else
			mode = AM_TS_SERIAL_4WIRE;
	} else if ((mname[0] == 'p') || (mname[0] == 'P')) {
		sprintf(pname, "p_ts%d", id);
		mode = AM_TS_PARALLEL;
	} else {
		memset(pname, 0, sizeof(pname));
		mode = AM_TS_DISABLE;
	}
	if (mutex_lock_interruptible(&dvb->mutex))
		return -ERESTARTSYS;

	ts = &dvb->ts[id];

	if (mode != AM_TS_SERIAL_3WIRE && mode != AM_TS_SERIAL_4WIRE) {
		if (ts->pinctrl) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		}

		ts->pinctrl = devm_pinctrl_get_select(&dvb->pdev->dev, pname);
		ts->mode = mode;
		ts->control = ctrl;
	} else if (mode != AM_TS_DISABLE) {
		ts->mode = mode;
		ts->control = ctrl;

		dprint_i("id:%d, control:0x%0x\n", id, ctrl);
		demod_config_tsin_invert(id, ctrl);
	}

	mutex_unlock(&dvb->mutex);

	return count;
}

static void set_dvb_ts(struct platform_device *pdev,
		       struct aml_dvb *advb, int i, const char *str)
{
	char buf[32];
	u8 control = 0;

	if (i >= TSIN_NUM) {
		dprint("set tsin %d invalid\n", i);
		return;
	}
	if (!strcmp(str, "serial-3wire")) {
		pr_dbg("ts%d:%s\n", i, str);

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_3WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		control = advb->ts[i].control;
		demod_config_in(i, DEMOD_3WIRE);
		demod_config_tsin_invert(i, control);
	} else if (!strcmp(str, "serial-4wire")) {
		pr_dbg("ts%d:%s\n", i, str);

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_4WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		control = advb->ts[i].control;
		demod_config_in(i, DEMOD_4WIRE);
		demod_config_tsin_invert(i, control);
	} else if (!strcmp(str, "parallel")) {
		pr_dbg("ts%d:%s\n", i, str);

		/*internal demod will use tsin_b/tsin_c parallel*/
//		if (i != 1) {
//			dprint("error %s:parallel should be ts1\n", buf);
//			return;
//		}
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "p_ts%d", i);
		advb->ts[i].mode = AM_TS_PARALLEL;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		demod_config_in(i, DEMOD_PARALLEL);
	} else {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].pinctrl = NULL;
	}

	/* t5m need set this bit, open ts_inb clock */
	if (i == 1 && get_cpu_type() >= MESON_CPU_MAJOR_ID_T5M) {
		demod_config_tsinb_clk(1);
		enable_tsinb_clk = 1;
	}

	if (i == 3 &&
		(get_cpu_type() == MESON_CPU_MAJOR_ID_T3X ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_S6))
		demod_config_tsin_clk(3, 0);

	if (IS_ERR_OR_NULL(advb->ts[i].pinctrl))
		pr_dbg("ts%d:pinctrl:%p Fail.\n",
				i, advb->ts[i].pinctrl);
	else
		pr_dbg("ts%d:pinctrl:%p OK.\n",
				i, advb->ts[i].pinctrl);
}

static void ts_process(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, j = 0;
	char buf[32];
	const char *str;
	u32 value;
	u32 data[32] = { 0 };
	struct aml_dvb *advb = aml_get_dvb_device();
	int cpu_type;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].ts_sid = -1;
		advb->ts[i].pinctrl = NULL;
		advb->ts[i].header_len = 0;

		memset(&advb->ts[i].header, 0, sizeof(advb->ts[i].header));

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_sid", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].ts_sid = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_header_len", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].header_len = value;

		if (advb->ts[i].header_len) {
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "ts%d_header", i);
			ret = of_property_read_u32_array(pdev->dev.of_node,
							 buf, data,
							 advb->ts[i].header_len);
			if (!ret) {
				for (j = 0; j < advb->ts[i].header_len; ++j)
					advb->ts[i].header[j] = data[j];
			}
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_sid_offset", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret && advb->ts[i].header_len) {
			advb->ts[i].sid_offset = value;
			advb->ts[i].ts_sid = advb->ts[i].header[value];
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_control", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret) {
			pr_dbg("%s: 0x%x\n", buf, value);
			advb->ts[i].control = value;
		} else {
			pr_dbg("read error:%s: 0x%x\n", buf, value);
		}

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d", i);
		ret = of_property_read_string(pdev->dev.of_node, buf, &str);
		if (!ret)
			set_dvb_ts(pdev, advb, i, str);
	}
	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tsinb");
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		ret = tee_demux_config_pad(0xfe0040a8, value);
		dprint("tsinb=%d, ret:%d\n", value, ret);
		cpu_type = get_cpu_type();
		if (cpu_type == MESON_CPU_MAJOR_ID_S4D)
			demod_config_tsin_clk(3, 0);
		else if (cpu_type == MESON_CPU_MAJOR_ID_S7D)
			demod_config_tsin_clk(1, 0);
	} else {
		dprint("no tsinb setting\n");
	}
}

int frontend_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		ts_process(pdev);
#endif

	return 0;
}

void frontend_config_ts_sid(void)
{
	int i;
	int sid = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

	for (i = 0; i < TSIN_NUM; i++) {
		if (advb->ts[i].mode == AM_TS_DISABLE)
			continue;
		if (advb->ts[i].header_len == 0) {
			if (advb->ts[i].ts_sid != -1) {
				sid = advb->ts[i].ts_sid;
				demod_config_single(i, sid);
				demod_config_fifo(i, 5 * 188);
			}
		} else {
			demod_config_multi(i, advb->ts[i].header_len / 4,
					   advb->ts[i].header[0],
					   advb->ts[i].sid_offset);
			demod_config_fifo(i, 5 * 188);
		}
	}
}

int frontend_remove(void)
{

	return 0;
}

int frontend_control_tsin_clk(int state)
{
	if (enable_tsinb_clk) {
		if (state)
			demod_config_tsinb_clk(1);
		else
			demod_config_tsinb_clk(0);
	}
	return 0;
}

int alp_tlv_probe(struct platform_device *pdev)
{
	char buf[32];
	int ret;
	u32 used = 0;
	union ALP_CFG_FIELD data;
	int i;

	if (get_dmx_version() < 6)
		return 0;

	for (i = 0; i < TSIN_NUM; i++) {
		memset(buf, 0, 32);
		used = 0;
		snprintf(buf, sizeof(buf), "alp_tlv%d", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &used);
		if (!ret && used == 0) {
			continue;
		} else {
			if (i == 0 || i == 2) {
				pr_dbg("alp_tlv not support\n");
				return 0;
			}
			if (i == 1)
				dprint("alp_tlv enable at tsinB\n");
			else
				dprint("alp_tlv enable at tsinD\n");
			data.data = alp_tlv_get_config(i);
			data.b.cfg_ts_like = 1;
			alp_tlv_set_config(i, data.data);
		}
	}
	return 0;
}

ssize_t alp_tlv_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	union ALP_CFG_FIELD data;

	if (get_dmx_version() < 6) {
		r = sprintf(buf, "this chip not support alp/tlv\n");
		buf += r;
		total += r;
		return total;
	}
	/*APL1*/
	data.data = 0;
	data.data = alp_tlv_get_config(1);
	r = sprintf(buf, "tsinb pid:0x%0x ts null:%d ",
		data.b.cfg_ts_like_pid, data.b.cfg_ts_null);
	buf += r;
	total += r;
	r = sprintf(buf, "ts like:%d ts mpeg:%d\n",
		data.b.cfg_ts_like, data.b.cfg_ts_mpeg);
	buf += r;
	total += r;

	/*APL3*/
	data.data = 0;
	data.data = alp_tlv_get_config(3);
	r = sprintf(buf, "tsind pid:0x%0x ts null:%d ",
		data.b.cfg_ts_like_pid, data.b.cfg_ts_null);
	buf += r;
	total += r;
	r = sprintf(buf, "ts like:%d ts mpeg:%d\n",
		data.b.cfg_ts_like, data.b.cfg_ts_mpeg);
	buf += r;
	total += r;

	return total;
}

ssize_t alp_tlv_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	int id, ctrl, r;
	char mname[32];
	union ALP_CFG_FIELD data;

	r = sscanf(buf, "%d %s %x", &id, mname, &ctrl);
	if (r != 3)
		goto error_hint;

	if (id == 0 || id == 2)
		goto error_hint;

	data.data = 0;
	data.data = alp_tlv_get_config(id);
	if (strcmp(mname, "pid") == 0)
		data.b.cfg_ts_like_pid = ctrl;
	else if (strcmp(mname, "ts_null") == 0)
		data.b.cfg_ts_null = ctrl;
	else if (strcmp(mname, "ts_like") == 0)
		data.b.cfg_ts_like = ctrl;
	else if (strcmp(mname, "ts_mpeg") == 0)
		data.b.cfg_ts_mpeg = ctrl;
	else
		goto error_hint;

	alp_tlv_set_config(id, data.data);
	return count;
error_hint:
	dprint("1/3 pid xx\n");
	dprint("1/3 ts_null xx\n");
	dprint("1/3 ts_like xx\n");
	dprint("1/3 ts_mpeg xx\n");
	return -EINVAL;
}
