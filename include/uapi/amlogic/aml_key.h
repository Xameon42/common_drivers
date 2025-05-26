/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_KEY_H_
#define _AML_KEY_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
#endif

enum user_id {
	DSC_LOC_DEC,
	DSC_NETWORK,
	DSC_LOC_ENC,

	CRYPTO_T0 = 0x100,
	CRYPTO_T1 = 0x101,
	CRYPTO_T2 = 0x102,
	CRYPTO_T3 = 0x103,
	CRYPTO_T4 = 0x104,
	CRYPTO_T5 = 0x105,
	CRYPTO_ANY = 0x106,
};

enum key_algo {
	KEY_ALGO_AES,
	KEY_ALGO_TDES,
	KEY_ALGO_DES,
	KEY_ALGO_CSA2,
	KEY_ALGO_CSA3,
	KEY_ALGO_NDL,
	KEY_ALGO_ND,
	KEY_ALGO_S17,
	KEY_ALGO_SM4,
	KEY_ALGO_MULTI2
};

struct key_descr {
	__u32 key_index;
	__u32 key_len;
	__u8 key[32];
};

struct key_config {
	__u32 key_index;
	__u32 key_userid;
	__u32 key_algo;
	__u32 ext_value;
};

struct key_alloc {
	__u32 is_iv;
	__u32 key_index;
};

#define KEY_ALLOC         _IOWR('o', 64, struct key_alloc)
#define KEY_FREE          _IO('o', 65)
#define KEY_SET           _IOR('o', 66, struct key_descr)
#define KEY_CONFIG        _IOR('o', 67, struct key_config)
#define KEY_GET_FLAG      _IOWR('o', 68, struct key_descr)
#endif
