/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_AFBC_DBG_H__
#define __DI_AFBC_DBG_H__

void dbg_afbcd_bits_show(struct seq_file *s, enum EAFBC_DEC eidx);
//void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_afbce_bits_show(struct seq_file *s, enum EAFBC_ENC eidx);

#endif	/*__DI_AFBC_DBG_H__*/
