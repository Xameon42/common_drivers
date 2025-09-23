/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_MEMC_DBG_H__
#define __FRC_MEMC_DBG_H__

ssize_t frc_bbd_final_line_param_show(struct class *class,
				struct class_attribute *attr, char *buf);
ssize_t frc_bbd_final_line_param_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t count);
ssize_t frc_vp_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_vp_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
ssize_t frc_logo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_logo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
ssize_t frc_iplogo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_iplogo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_melogo_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_melogo_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_sence_chg_detect_param_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t frc_sence_chg_detect_param_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);

ssize_t frc_fb_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_fb_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_me_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_me_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_search_rang_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_search_rang_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_pixel_lpf_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_pixel_lpf_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_me_rule_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_me_rule_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_film_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_film_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);

ssize_t frc_glb_ctrl_param_show(struct class *class, struct class_attribute *attr, char *buf);
ssize_t frc_glb_ctrl_param_store(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
#endif

