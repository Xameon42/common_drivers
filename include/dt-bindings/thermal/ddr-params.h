/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DDR_PARAMS_H__
#define __DDR_PARAMS_H__

#define DDR_PROCESS_SHIFT       0
#define DDR_TYPE_SHIFT          8
#define DDR_SIP_SHIFT           12
#define DDR_RANK0_SHIFT         20
#define DDR_RANK1_SHIFT         21
#define DDR_RANK_32BIT          0
#define DDR_RANK_16BIT          1

#define DDR_PROCESS_FLAG        1
#define DDR_TYPE_FLAG           2
#define DDR_SIP_FLAG            4
#define DDR_RANK0_FLAG          8
#define DDR_RANK1_FLAG          16
#define DDR_PROCESS_TYPE_FLAG   (DDR_PROCESS_FLAG | DDR_TYPE_FLAG)
#define DDR_SIP_TYPE_FLAG       (DDR_SIP_FLAG | DDR_TYPE_FLAG)

#define DDR_DMC                 0
#define DDR_THERMAL             1

#define TYPE_DDR3               0
#define TYPE_DDR4               1
#define TYPE_LPDDR4             2
#define TYPE_LPDDR3             3
#define TYPE_LPDDR2             4
#define TYPE_LPDDR4X            5
#define TYPE_LPDDR5             6
#define TYPE_MAX                (TYPE_LPDDR5 + 1)

#define SIP                     1
#define NOSIP                   0

#define DDR4_PARTICLE           (TYPE_DDR4 << DDR_TYPE_SHIFT)
#define LPDDR4_PARTICLE         (TYPE_LPDDR4 << DDR_TYPE_SHIFT)
#define LPDDR4X_PARTICLE        (TYPE_LPDDR4X << DDR_TYPE_SHIFT)
#define LPDDR5_PARTICLE         (TYPE_LPDDR5 << DDR_TYPE_SHIFT)
#define SIP_PARTICLE            (SIP << DDR_SIP_SHIFT)
#define NOSIP_PARTICLE          (NOSIP << DDR_SIP_SHIFT)
#define RANK0_16BIT_PARTICLE    (DDR_RANK_16BIT << DDR_RANK0_SHIFT)
#define RANK0_32BIT_PARTICLE    (DDR_RANK_32BIT << DDR_RANK0_SHIFT)
#define RANK1_16BIT_PARTICLE    (DDR_RANK_16BIT << DDR_RANK1_SHIFT)
#define RANK1_32BIT_PARTICLE    (DDR_RANK_32BIT << DDR_RANK1_SHIFT)
#define DDR4_NOSIP_PARTICLE     (DDR4_PARTICLE | NOSIP_PARTICLE)
#define DDR4_SIP_PARTICLE       (DDR4_PARTICLE | SIP_PARTICLE)
#define LPDDR4_NOSIP_PARTICLE   (LPDDR4_PARTICLE | NOSIP_PARTICLE)
#define LPDDR4_SIP_PARTICLE     (LPDDR4_PARTICLE | SIP_PARTICLE)
#endif
