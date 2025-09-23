/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_DATA_H__
#define __DI_DATA_H__

/**********************************************************
 * fifo for uchar
 *
 **********************************************************/
#define UFI256_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    256,		\
	    GFP_KERNEL)

#define UFI128_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    128,		\
	    GFP_KERNEL)

#define UFI64_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    64,			\
	    GFP_KERNEL)

#define UFI32_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    32,			\
	    GFP_KERNEL)

#define UFI16_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    16,			\
	    GFP_KERNEL)

#define UFI8_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    8,			\
	    GFP_KERNEL)

#endif	/*__DI_DATA_H__*/
