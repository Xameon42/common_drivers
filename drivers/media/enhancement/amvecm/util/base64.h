/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef BASE64_H
#define BASE64_H

/*
 * out is null-terminated encode string.
 * return values is out length, exclusive terminating `\0'
 */
unsigned long base64_encode(unsigned char *in,
			    unsigned int inlen,
			    char *out);

/*
 * return values is out length
 */
unsigned long base64_decode(char *in, unsigned int inlen,
			    unsigned char *out);

#endif /* BASE64_H */
#endif
