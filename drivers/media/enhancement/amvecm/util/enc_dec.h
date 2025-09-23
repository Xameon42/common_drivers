/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef ENC_DEC_H
#define ENC_DEC_H

#define SAFE_SIZE(s) ((unsigned long)((s) * 2))
#define DECHUFF_MAXLEN (4913 * 3 * 4)

unsigned long huff64_encode(unsigned int *in, unsigned int inlen,
			    char *out);

/*
 * return values is out length
 */
unsigned long huff64_decode(char *in, unsigned int inlen,
			    unsigned int *out, unsigned int outlen);

#endif /* ENC_DEC_H */
#endif
