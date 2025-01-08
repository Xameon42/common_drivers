/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HOST_REPORT_H__
#define __HOST_REPORT_H__

#include "host.h"
#include <linux/input.h>
#include <linux/kobject.h>
#include <sound/soc.h>

enum audio_event {
	AUDIO_NONE_EVENT = 0,
	AI_SOUND_MODE_EVENT,
	VAD_WAKEUP_MODE_EVENT,
};

#define MAX_UEVENT_LEN 64

struct audio_uevent {
	const enum audio_event type;
	int state;
	const char *env;
};

void host_dsp_vad_report(struct host_module *host);
void host_dsp_vad_input_device_init(struct host_module *host);

#endif /*_HOST_REPORT_H*/
