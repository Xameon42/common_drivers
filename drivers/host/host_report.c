// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

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
	pr_info("%s[%d] %s %d\n", __func__, __LINE__, env, ret);

	return ret;
}

static void host_send_event_to_wakeup_screen(struct host_module *host)
{
	input_event(host->host_dsp->input_device, EV_KEY, KEY_POWER, 1);
	input_sync(host->host_dsp->input_device);
	input_event(host->host_dsp->input_device, EV_KEY, KEY_POWER, 0);
	input_sync(host->host_dsp->input_device);
}

void host_dsp_vad_report(struct host_module *host)
{
	host_send_event_to_audio(host->dev, VAD_WAKEUP_MODE_EVENT, 1);
	host_send_event_to_wakeup_screen(host);
}

void host_dsp_vad_input_device_init(struct host_module *host)
{
	host->host_dsp->input_device->name = "vad_keypad";
	host->host_dsp->input_device->phys = "vad_keypad/input3";
	host->host_dsp->input_device->dev.parent = host->dev;
	host->host_dsp->input_device->id.bustype = BUS_ISA;
	host->host_dsp->input_device->id.vendor  = 0x0001;
	host->host_dsp->input_device->id.product = 0x0001;
	host->host_dsp->input_device->id.version = 0x0100;
	host->host_dsp->input_device->evbit[0] = BIT_MASK(EV_KEY);
	host->host_dsp->input_device->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	set_bit(KEY_POWER, host->host_dsp->input_device->keybit);
}

