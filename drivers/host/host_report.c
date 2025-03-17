// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include "host_report.h"

struct audio_uevent audio_events[] = {
	{
		.type = VAD_WAKEUP_MODE_EVENT,
		.env = "vad_wakeup="
	},
	{
		.type = AUDIO_NONE_EVENT,
	}
};

static int host_send_event_to_audio(struct device *dev, enum audio_event type, int val)
{
	char env[MAX_UEVENT_LEN];
	struct audio_uevent *event = audio_events;
	char *envp[2];
	int ret = -1;

	for (event = audio_events; event->type != AUDIO_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == AUDIO_NONE_EVENT)
		return ret;

	event->state = val;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%d", event->env, val);

	ret = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	pr_debug("%s[%d] %s %d\n", __func__, __LINE__, env, ret);

	return ret;
}

static void host_send_event_to_wakeup_screen(struct host_dsp *host_dsp)
{
	input_event(host_dsp->input_device, EV_KEY, KEY_POWER, 1);
	input_sync(host_dsp->input_device);
	input_event(host_dsp->input_device, EV_KEY, KEY_POWER, 0);
	input_sync(host_dsp->input_device);
}

static void host_dsp_ffv_report(struct work_struct *work)
{
	struct host_dsp *host_dsp = container_of((struct delayed_work *)work,
						  struct host_dsp, host_ffv_work);

	host_send_event_to_audio(host_dsp->host->dev, VAD_WAKEUP_MODE_EVENT, 1);
	host_send_event_to_wakeup_screen(host_dsp);
}

void host_dsp_ffv_wq_init(struct host_dsp *host_dsp)
{
	host_dsp->host_ffv_wq = create_workqueue("host_ffv_wq");
	INIT_DEFERRABLE_WORK(&host_dsp->host_ffv_work, host_dsp_ffv_report);
}

void host_dsp_ffv_wq_start(struct host_dsp *host_dsp)
{
	queue_delayed_work(host_dsp->host_ffv_wq,
			   &host_dsp->host_ffv_work,
			   msecs_to_jiffies(DSP_FFV_REPORT_DELAY_MS));
}

void host_dsp_ffv_wq_stop(struct host_dsp *host_dsp)
{
	if (!host_dsp->host_ffv_wq)
		return;

	dev_dbg(host_dsp->host->dev, "[%s %d]\n", __func__, __LINE__);
	cancel_delayed_work_sync(&host_dsp->host_ffv_work);
	flush_workqueue(host_dsp->host_ffv_wq);
	destroy_workqueue(host_dsp->host_ffv_wq);
	host_dsp->host_ffv_wq = NULL;
}

void host_dsp_vad_report(struct host_dsp *host_dsp)
{
	host_dsp_ffv_wq_start(host_dsp);
}

void host_dsp_vad_input_device_init(struct host_dsp *host_dsp)
{
	host_dsp->input_device->name = "vad_keypad";
	host_dsp->input_device->phys = "vad_keypad/input3";
	host_dsp->input_device->dev.parent = host_dsp->host->dev;
	host_dsp->input_device->id.bustype = BUS_ISA;
	host_dsp->input_device->id.vendor  = 0x0001;
	host_dsp->input_device->id.product = 0x0001;
	host_dsp->input_device->id.version = 0x0100;
	host_dsp->input_device->evbit[0] = BIT_MASK(EV_KEY);
	host_dsp->input_device->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	set_bit(KEY_POWER, host_dsp->input_device->keybit);
}

