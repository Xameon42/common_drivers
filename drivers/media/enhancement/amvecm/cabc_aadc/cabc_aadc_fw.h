/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef CABC_AADC_H
#define CABC_AADC_H

#define AAD_DEBUG 0x1
#define CABC_DEBUG 0X2
#define PRE_GAM_DEBUG 0x4

void aml_cabc_alg_process(struct work_struct *work);
void aml_cabc_alg_bypass(struct work_struct *work);
void db_cabc_param_set(struct db_cabc_param_s *db_cabc_param_data);
void db_aad_param_set(struct db_aad_param_s *db_aad_param_data);
int cabc_aad_debug(char **param);
ssize_t cabc_aad_print(char *buf);
int *vf_hist_get(void);
int fw_en_get(void);
#endif
#endif
