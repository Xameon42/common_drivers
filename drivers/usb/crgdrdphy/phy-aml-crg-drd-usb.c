// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/usb-v2.h>
#include "phy-aml-crg-drd.h"

bool aml_usb_phy2_dbg;
module_param(aml_usb_phy2_dbg, bool, 0644);

/* Reset usb controller. */
int amlogic_crg_drd_usbphy_reset(struct amlogic_usb_v2 *phy)
{
	static int	init_count;
	au2p_dbg(phy->dev, "%s initial: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));
	if (!init_count) {
		init_count++;
		if (phy->usb_reset_bit == 2)
			writel((readl(phy->reset_regs) |
				(0x1 << phy->usb_reset_bit)), phy->reset_regs);
		else
			writel((0x1 << phy->usb_reset_bit), phy->reset_regs);
	}
	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));
	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_reset);

int amlogic_crg_drd_usbphy_usb_hold_reset(struct amlogic_usb_v2 *phy, bool on)
{
	u32 val = 0;
	size_t mask = 0;
	u32 tmp = 0;

	au2p_dbg(phy->dev, "%s initial: 0x%x.\n", __func__,
			readl(phy->reset_regs + phy->reset_level));

	mask = (size_t)phy->reset_regs & 0xf;

	if (phy->usb_reset_bit != -1U)
		tmp |= BIT(phy->usb_reset_bit);
	if (phy->usb_comb_reset_bit != -1U)
		tmp |= BIT(phy->usb_comb_reset_bit);

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	if (on) {
		writel(val | tmp, (void __iomem	*)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	} else {
		writel(val & ~tmp, (void __iomem *)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	}

	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
			readl(phy->reset_regs + phy->reset_level));

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_usb_hold_reset);

/* Reset phy hw state machine. */
int amlogic_crg_drd_usbphy_reset_phycfg(struct amlogic_usb_v2 *phy, int cnt)
{
	u32 val, i = 0;
	u32 temp = 0;
	size_t mask = 0;

	au2p_dbg(phy->dev, "%s initial: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));
	mask = (size_t)phy->reset_regs & 0xf;

	for (i = 0; i < cnt; i++)
		temp = temp | (1 << phy->phy_reset_level_bit[i]);

	/* set usb phy to low power mode */
	val = readl((void __iomem		*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val & (~temp)), (void __iomem	*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	usleep_range(99, 100);
	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val | temp), (void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_reset_phycfg);

int amlogic_crg_drd_usbphy_hold_reset(struct amlogic_usb_v2 *phy, bool on)
{
	u32 val = 0, temp = 0;
	size_t mask = 0;
	int i = 0;

	au2p_dbg(phy->dev, "%s initial: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));
	mask = (size_t)phy->reset_regs & 0xf;

	for (i = 0; i < phy->portnum; i++)
		temp = temp | (1 << phy->phy_reset_level_bit[i]);

	val = readl((void __iomem		*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	if (on) {
		writel(val | temp, (void __iomem	*)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	} else {
		writel(val & ~temp, (void __iomem	*)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	}
	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_hold_reset);

int amlogic_crg_drd_usbphy_reg_hold_reset(struct amlogic_usb_v2 *phy, bool on)
{
	u32 val = 0, temp = 0;
	size_t mask = 0;
	int i = 0;

	au2p_dbg(phy->dev, "%s initial: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));
	mask = (size_t)phy->reset_regs & 0xf;

	for (i = 0; i < phy->portnum; i++) {
		if (phy->phy_reg_reset_level_bit[i] != -1U)
			temp = temp | (1 << phy->phy_reg_reset_level_bit[i]);
	}

	val = readl((void __iomem		*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	if (on) {
		writel(val | temp, (void __iomem	*)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	} else {
		writel(val & ~temp, (void __iomem	*)
			((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	}
	au2p_dbg(phy->dev, "%s after: 0x%x.\n", __func__,
				readl(phy->reset_regs + phy->reset_level));

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_reg_hold_reset);

/* Reset phy reg values. */
int amlogic_crg_drd_usbphy_reg_reset(struct amlogic_usb_v2 *phy)
{
	int ret = 0;

	ret = amlogic_crg_drd_usbphy_reg_hold_reset(phy, false);
	if (ret)
		goto done;
	ret = amlogic_crg_drd_usbphy_reg_hold_reset(phy, true);
	if (ret)
		goto err;
done:
	return ret;
err:
	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_crg_drd_usbphy_reg_reset);
