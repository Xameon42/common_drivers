/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef AI_PQ_H
#define AI_PQ_H

#define SLOWER_BIT 10

/*adaptive dnlp parameters for ai pq*/
struct adap_dnlp_param_s {
	/*base value, from db*/
	int dnlp_final_gain;
	/*offset from customer*/
	int offset;
};

/*adaptive saturation parameters for ai pq*/
struct adap_satur_param_s {
	/*base value, from db*/
	int satur_hue_mab;
	/*offset from customer*/
	int offset;
};

/*adaptive peaking parameters for ai pq*/
struct adap_peaking_param_s {
	/*base value, from db*/
	int sr0_hp_final_gain;
	int sr0_bp_final_gain;
	int sr0_final_pgains;
	int sr0_final_ngains;
	int sr0_final_dgains;
	int sr0_final_cgains;
	int sr1_hp_final_gain;
	int sr1_bp_final_gain;
	/*offset from customer*/
	int offset;
};

/*adaptive parameters for ai pq*/
struct adap_param_setting_s {
	struct adap_dnlp_param_s dnlp_param;
	struct adap_satur_param_s satur_param;
	struct adap_peaking_param_s peaking_param;
};

extern struct adap_param_setting_s adaptive_param;

int aipq_saturation_hue_get_base_val(void);
int ai_detect_scene_init(void);
int adaptive_param_init(void);

/*temporary solution for base value*/
int aipq_base_dnlp_param(unsigned int final_gain);
int aipq_base_peaking_param(unsigned int reg,
			    unsigned int mask,
			    unsigned int value);
int aipq_base_satur_param(int value);
#endif
#endif
