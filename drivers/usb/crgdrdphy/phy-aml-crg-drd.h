/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PHY_AML_CRG_DRD_H
#define _PHY_AML_CRG_DRD_H

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb_v2, phy)
#define TUNING_DISCONNECT_THRESHOLD 0x3f

extern bool aml_usb_phy2_dbg;

#define au2p_dbg(dev, fmt, args...)	\
do {								\
	if (aml_usb_phy2_dbg)			\
		dev_err(dev, fmt, ## args);	\
} while (0)

#define au2p_info(dev, fmt, args...)	\
		dev_info(dev, fmt, ## args)

#define au2p_err(dev, fmt, args...)	\
		dev_err(dev, fmt, ## args)

int amlogic_crg_drd_usbphy_reset(struct amlogic_usb_v2 *phy);
int amlogic_crg_drd_usbphy_usb_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int amlogic_crg_drd_usbphy_reset_phycfg(struct amlogic_usb_v2 *phy, int cnt);
int amlogic_crg_drd_usbphy_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int amlogic_crg_drd_usbphy_reg_reset(struct amlogic_usb_v2 *phy);
int amlogic_crg_drd_usbphy_reg_hold_reset(struct amlogic_usb_v2 *phy, bool on);
void crg_exit(void);
int crg_init(void);
void crg_gadget_exit(void);
int crg_gadget_init(void);
int crg_otg_write_UDC(const char *udc_name);
#endif/* _PHY_AML_CRG_DRD_H */
