/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _CVBS_LOG_H_
#define _CVBS_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#undef pr_fmt
#define pr_fmt(fmt) "cvbsout: " fmt

#define cvbs_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define cvbs_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define cvbs_log_dbg(fmt, ...) \
	pr_debug(fmt, ##__VA_ARGS__)

#endif
