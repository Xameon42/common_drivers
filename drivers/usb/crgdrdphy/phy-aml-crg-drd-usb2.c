// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/gki_module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/usb/phy_companion.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
//#include <linux/amlogic/power_ctrl.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/cpu_version.h>
#include "phy-aml-crg-drd.h"

#define USB_SPEED_HIGH		0
#define USB_SPEED_HIGH_PLUS	1

#ifndef MESON_CPU_MAJOR_ID_T6D
#define MESON_CPU_MAJOR_ID_T6D 0x49
#endif
#define PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT_5_0 0x3f
#define PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0 0x7f

enum aml_crg_drd_usb2_phy_version {
	AML_CRG_DRD_USB2_PHY_V0,
	AML_CRG_DRD_USB2_PHY_V1,
};

struct aml_crg_drd_usb2_pdata {
	enum aml_crg_drd_usb2_phy_version ver;
};

#undef local_pdata
#define local_pdata(phy) ((struct aml_crg_drd_usb2_pdata *)(phy)->pdata)

struct amlogic_usb_v2	*g_crg_drd_phy2[3];
char name_crg[32];
static bool aml_usb2_phy_960m;

static int get_u2phy_speed(char *str)
{
	int ret;

	ret = kstrtobool(str, &aml_usb2_phy_960m);

	return ret;
}

__setup("usb2t_mode=", get_u2phy_speed);

static inline bool aml_crg_drd_usb2_hsp(struct amlogic_usb_v2 *phy)
{
	return phy->sw_hsp && aml_usb2_phy_960m;
}

static void usb_set_calibration_trim
	(void __iomem *reg, struct amlogic_usb_v2 *phy)
{
	u32 value = 0;
	u32 cali, i;
	u8 cali_en;

	if (!phy->usb_phy_trim_reg) {
		au2p_err(phy->dev, "No usb-phy-trim-reg\n");
		return;
	}

	cali = readl(phy->usb_phy_trim_reg);
	cali_en = (cali >> 12) & 0x1;
	cali = cali >> 8;
	if (cali_en) {
		cali = cali & 0xf;
		if (is_meson_s7d_cpu())
			cali = cali + 2;
		if (cali > 12)
			cali = 12;
	} else {
		cali = phy->pll_setting[4];
	}
	value = readl(reg + 0x10);
	value &= (~0xfff);
	for (i = 0; i < cali; i++)
		value |= (1 << i);

	writel(value, reg + 0x10);

	au2p_info(phy->dev, "phy trim value= 0x%08x\n", value);
}

static void set_trim_initvalue
(struct amlogic_usb_v2 *phy, void __iomem	*reg, int port)
{
	u32 val;

	if (!phy)
		return;

	val = readl(reg + 0x10);
	phy->phy_trim_initvalue[port] = val;
	val = readl(reg + 0x0c);
	phy->phy_0xc_initvalue[port] = val;
}

static void set_usb_phy_trim_tuning
	(struct usb_phy *x, int port, int default_val)
{
	void __iomem	*phy_reg_base;
	u32 value;
	struct amlogic_usb_v2	*aml_phy =
		container_of(x, struct amlogic_usb_v2, phy);

	if (!aml_phy)
		return;
	if (aml_phy->phy_version == 0)
		return;

	if (port > aml_phy->portnum)
		return;
	if (default_val == aml_phy->phy_trim_state[port]) {
		if (default_val == 0)
			default_val = 1;
		else
			return;
	}

	if (aml_phy->suspend_flag == 1) {
		aml_phy->phy_trim_state[port] = default_val;
		au2p_info(aml_phy->dev, "--phy has been shutdown\n");
		return;
	}

	phy_reg_base = aml_phy->phy_cfg[port];
	au2p_info(aml_phy->dev, "---%s port(%d) phy trim tuning cf(%ps)--\n",
		default_val ? "Recovery" : "Set",
		port, __builtin_return_address(0));
	if (!default_val) {
		value = readl(phy_reg_base + 0x0c);
		value |= 0x07;
		writel(value, phy_reg_base + 0x0c);
		writel(0x8000fff, phy_reg_base + 0x10);
		writel(0x78000, phy_reg_base + 0x34);
	} else {
		writel(aml_phy->phy_0xc_initvalue[port], phy_reg_base + 0x0c);
		writel(aml_phy->phy_trim_initvalue[port], phy_reg_base + 0x10);
		writel(0x78000, phy_reg_base + 0x34);
	}
	aml_phy->phy_trim_state[port] = default_val;
}

static void set_usb_pll_v0(struct amlogic_usb_v2 *phy, void __iomem	*reg)
{
	u32 val;

	/* TO DO set usb  PLL */
	writel((0x30000000 | (phy->pll_setting[0])), reg + 0x40);
	writel(phy->pll_setting[1], reg + 0x44);
	writel(phy->pll_setting[2], reg + 0x48);
	usleep_range(99, 100);
	writel((0x10000000 | (phy->pll_setting[0])), reg + 0x40);

	/**write 0x0c must write 0x78000 to 0x34**/
	writel(PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT_5_0, reg + 0xC);
	/* PHY Tune */
	if (g_crg_drd_phy2[phy->phy_id]) {
		if (g_crg_drd_phy2[phy->phy_id]->phy_version) {
			writel(phy->pll_setting[3], reg + 0x50);
			writel(0x2a, reg + 0x54);

	/**phy_version == 3, 0x10 set value in usb_set_calibration_trim**/
	/**phy_version == 3, 0x08 no longer contains the default value of 0x10**/
			if (g_crg_drd_phy2[phy->phy_id]->phy_version != 3) {
				val = readl(reg + 0x08);
				val &= 0xfff;
				if (val == 0)
					val = 0x800001f;
				writel(val | readl(reg + 0x10), reg + 0x10);
			}
			/*new disconnect threshold 0x38: t3 bit[27-28], t7,s4 bit[26-27] */
			val = readl(reg + 0x38);
			if (is_meson_s4_cpu() || is_meson_s4d_cpu() ||
				is_meson_t7_cpu()) {
				val &= ~0xc000000;
				val |= (phy->pll_dis_thred_enhance << 26 & 0xc000000);
			} else {
				val &= ~0x18000000;
				val |= (phy->pll_dis_thred_enhance << 27 & 0x18000000);
			}
			writel(val, reg + 0x38);
			writel(0x78000, reg + 0x34);
		} else {
			writel(phy->pll_setting[3], reg + 0x50);
			writel(phy->pll_setting[4], reg + 0x10);
			writel(0, reg + 0x38);
			writel(phy->pll_setting[5], reg + 0x34);
		}
	}
}

/* txhd2, s1a */
static void set_usb_pll_v1(struct amlogic_usb_v2 *phy, void __iomem	*reg)
{
#define USBPLL_RESET_BIT 18
#define USBPLL_LK_RESET_BIT 28
#define USBPLL_EN_BIT 11
	u32 retry = 5;
	u32 pll_val0 = phy->pll_setting[0],
		pll_val1 = phy->pll_setting[1];
__retry:
	writel(pll_val0, reg + 0x40);
	writel(pll_val1, reg + 0x44);
	usleep_range(4, 5);
	writel(pll_val1 | (1 << USBPLL_RESET_BIT), reg + 0x44);
	writel((pll_val0 | (1 << USBPLL_LK_RESET_BIT) | (1 << USBPLL_EN_BIT)), reg + 0x40);
	usleep_range(49, 50);
	writel(pll_val1, reg + 0x44);
	usleep_range(49, 50);
	writel((pll_val0 | (1 << USBPLL_EN_BIT)), reg + 0x40);
	usleep_range(49, 50);
	writel(phy->pll_setting[3], reg + 0x50);
	// wait for 200us
	usleep_range(199, 200);

	writel(PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0, reg + 0xC);
	usleep_range(199, 200);

	//check lock bit
	if (readl(reg + 0x40) >> 31)
		return;

	retry--;
	if (!retry)
		return;

	goto __retry;
}

/* s7 */
static void set_usb_pll_v2(struct amlogic_usb_v2 *phy, void __iomem	*reg)
{
#define USBPLL_LK_RST_BIT	28
#define USBPLL_EN_BIT		11
#define USB2_MPLL_EN_CTRL_BIT 1
#define USBPLL_RST_BIT		0
/* usb2_squelch_trim: usb2_reg_cfg[28]##reg32_03[2:0] (MSB->LSB) default 0b0111
 * usb2_disc_trim: usb2_reg_cfg[27]##reg32_03[6:4] (MSB->LSB) default 0b1000
 */
#define PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0_v2 0x7

	u32 retry = 5;
	u32 pll_val0 = phy->pll_setting[0],
		pll_val1 = phy->pll_setting[1];

__retry:
	writel(pll_val0 | (1 << USBPLL_LK_RST_BIT) |
		(1 << USB2_MPLL_EN_CTRL_BIT) | (1 << USBPLL_RST_BIT),
		(reg + 0x40));
	usleep_range(99, 100);
	writel(pll_val1, (reg + 0x44));
	usleep_range(99, 100);
	writel(pll_val0 | (1 << USBPLL_LK_RST_BIT) |
		(1 << USB2_MPLL_EN_CTRL_BIT) | (1 << USBPLL_RST_BIT) | (1 << USBPLL_EN_BIT),
		(reg + 0x40));
	usleep_range(99, 100);
	writel(pll_val0 | (1 << USBPLL_LK_RST_BIT) |
		(1 << USB2_MPLL_EN_CTRL_BIT) | (1 << USBPLL_EN_BIT), (reg + 0x40));
	usleep_range(99, 100);
	writel(pll_val0 | (1 << USB2_MPLL_EN_CTRL_BIT) | (1 << USBPLL_EN_BIT),
		(reg + 0x40));
	usleep_range(99, 100);
	writel(PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0_v2, reg + 0xC);
	// wait for 200us
	usleep_range(199, 200);
	//check lock bit
	if (readl((reg + 0x40)) >> 31)
		return;

	retry--;
	if (!retry)
		return;

	goto __retry;
}

/* s7d, s6 */
static void set_usb_pll_v3(struct amlogic_usb_v2 *phy, void __iomem *phy_reg_base)
{
#define USB2_MPPLL_EN_CTRL_BIT	27
#define USBPLL_BIAS_EN_BIT	26
#define USB2_PLL_RSTN_BIT	25
#define USB2_PLL_LOCK_EN_BIT	24
#define USB2_PLL_DONE		31
#define USB2_REG_CFG_DIS	27
/* S7D/S6
 * usb2_squelch_trim: reg32_03[3:0] (MSB->LSB) default 0b0111
 * usb2_disc_trim: reg32_03[6:4] (MSB->LSB) default 0b000
 */
#define PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0_v3 0x39
	u32 retry = 5;
	int plldone_i;
	u32 pll_val;
	/* default value
	 * USB2_MPPLL_EN_CTRL	0
	 * USBPLL_BIAS_EN	0
	 * USB2_PLL_RSTN	0
	 * USB2_PLL_LOCK_EN	0
	 */
	u32 def_val = 0x549540;
	void __iomem *pll_cfg = phy_reg_base + 0x40;

	pll_val = readl(pll_cfg);
	/*Enable PLL Control*/
	pll_val |= BIT(USB2_MPPLL_EN_CTRL_BIT);
	writel(pll_val, pll_cfg);
	usleep_range(10, 20);

	while (retry--) {
		/* Reset Register value */
		pll_val = def_val | BIT(USB2_MPPLL_EN_CTRL_BIT);
		writel(pll_val, pll_cfg);
		usleep_range(10, 20);
		/* assert usb_pll_bias_en */
		pll_val |= BIT(USBPLL_BIAS_EN_BIT);
		writel(pll_val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 30);
		/* assert usb_pll_rstn */
		pll_val |= BIT(USB2_PLL_RSTN_BIT);
		writel(pll_val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 30);
		/* assert usb_pll_lock_en */
		pll_val |= BIT(USB2_PLL_LOCK_EN_BIT);
		writel(pll_val, pll_cfg);
		/* check lock bit */
		for (plldone_i = 5; plldone_i > 0; plldone_i--) {
			usleep_range(20, 30);
			if (readl(pll_cfg) & BIT(USB2_PLL_DONE))
				goto okay;
		}
		au2p_dbg(phy->dev, "usb2 pll not locked, retry. val: 0x%08x\n", readl(pll_cfg));
	}
	au2p_err(phy->dev, "%s set pll failed, exit, val:0x%08x.\n", __func__, readl(pll_cfg));
	return;
okay:
	au2p_dbg(phy->dev, "usb2 pll init done, val: 0x%08x\n", readl(pll_cfg));

	/* The s7d default value 0b000 of usb2_disc_trim leads to hs handshake err.
	 * S6 shares the params with s7d.
	 */
	writel(PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0_v3, phy_reg_base + 0xC);
	/* The USB2_REG_CFG_DIS is not used but default set. Clear it.*/
	writel(readl(phy_reg_base + 0x38) & ~BIT(USB2_REG_CFG_DIS), phy_reg_base + 0x38);

	/* edgedrv cali for signal quality. */
	writel(phy->pll_setting[3], phy_reg_base + 0x50);
}



static void amlogic_crg_drd_usb2_set_usb_vbus_power
	(struct gpio_desc **usb_gd, int pin, char is_power_on)
{
	struct amlogic_usb_v2 *phy = container_of(usb_gd, struct amlogic_usb_v2, usb_gpio_desc);

	au2p_dbg(phy->dev, "%s %d.\n", __func__, is_power_on);

	if (is_power_on)
		/*set vbus on by gpio*/
		gpiod_direction_output(*usb_gd, 1);
	else
		/*set vbus off by gpio first*/
		gpiod_direction_output(*usb_gd, 0);
}

static void amlogic_crg_drd_usb2_set_vbus_power
		(struct amlogic_usb_v2 *phy, char is_power_on)
{
	if (phy->vbus_power_pin != -1)
		amlogic_crg_drd_usb2_set_usb_vbus_power(&phy->usb_gpio_desc,
				   phy->vbus_power_pin, is_power_on);
}

static void amlogic_crg_drd_usb2_set_controller_power
		(struct amlogic_usb_v2 *phy, bool force, bool on)
{
	if (!phy || !(phy->pm_controller || force))
		return;

	amlogic_crg_drd_usbphy_usb_hold_reset(phy, on);
}

static int amlogic_crg_drd_usb2_init_v0(struct usb_phy *x)
{
	int i, j, cnt;

	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r0_v2 reg0;
	union u2p_r1_v2 reg1;
	u32 val;
	u32 temp = 0;
	u32 portnum = phy->portnum;
	size_t mask = 0;
	int ret;

	amlogic_crg_drd_usb2_set_vbus_power(phy, 1);

	mask = (size_t)phy->reset_regs & 0xf;

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB2_PHY_ENABLE) {
			ret = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
			if (ret) {
				au2p_err(phy->dev, "Failed to enable usb2 phy bus clock at %d\n",
							__LINE__);
			}
		}
	}

	amlogic_crg_drd_usb2_set_controller_power(phy, false, true);

	for (i = 0; i < portnum; i++)
		temp = temp | (1 << phy->phy_reset_level_bit[i]);

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val | temp), (void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	amlogic_crg_drd_usbphy_reset(phy);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + 0x20 * i + 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
		reg0.b.POR = 1;
		if (phy->suspend_flag == 0) {
			reg0.b.host_device = 1;
			reg0.b.IDPULLUP0 = 1;
			reg0.b.DRVVBUS0 = 1;
		}
		writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
	}

	usleep_range(9, 10);
	amlogic_crg_drd_usbphy_reset_phycfg(phy, phy->portnum);
	usleep_range(49, 50);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + 0x20 * i + 4 * j);
		}
		usb_set_calibration_trim(phy->phy_cfg[i], phy);

		/* ID DETECT: usb2_otg_aca_en set to 0 */
		/* usb2_otg_iddet_en set to 1 */
		writel(readl(phy->phy_cfg[i] + 0x54) & (~(1 << 2)),
			(phy->phy_cfg[i] + 0x54));

		reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
		cnt = 0;
		while (reg1.b.phy_rdy != 1) {
			reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
			/*we wait phy ready max 1ms, common is 100us*/
			if (cnt > 200)
				break;

			cnt++;
			udelay(5);
		}
	}

	/* step 7: pll setting */
	for (i = 0; i < phy->portnum; i++) {
		phy->set_usb_pll(phy, phy->phy_cfg[i]);
		set_trim_initvalue(phy, phy->phy_cfg[i], i);
	}

	if (phy->suspend_flag)
		phy->suspend_flag = 0;

	return 0;
}

int amlogic_crg_device_usb2_init_v0(struct amlogic_usb_v2 *phy)
{
	int i, j, cnt;
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r0_v2 reg0;
	union u2p_r1_v2 reg1;
	u32 val;
	u32 temp = 0;
	u32 portnum;
	size_t mask = 0;
	int ret;

	portnum = phy->portnum;

	amlogic_crg_drd_usb2_set_vbus_power(phy, 0);

	mask = (size_t)phy->reset_regs & 0xf;

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB2_PHY_ENABLE) {
			ret = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
			if (ret) {
				au2p_err(phy->dev, "Failed to enable usb2 phy bus clock at %d\n",
							__LINE__);
			}
		}
	}

	for (i = 0; i < portnum; i++)
		temp = temp | (1 << phy->phy_reset_level_bit[i]);

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val | temp), (void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	amlogic_crg_drd_usbphy_reset(phy);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
		reg0.b.host_device = 0;
		reg0.b.POR = 0;
		reg0.b.IDPULLUP0 = 1;
		reg0.b.DRVVBUS0 = 1;
		writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
	}

	usleep_range(9, 10);
	amlogic_crg_drd_usbphy_reset_phycfg(phy, phy->portnum);
	usleep_range(49, 50);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + 4 * j);
		}
		usb_set_calibration_trim(phy->phy_cfg[i], phy);

		/* ID DETECT: usb2_otg_aca_en set to 0 */
		/* usb2_otg_iddet_en set to 1 */
		writel(readl(phy->phy_cfg[i] + 0x54) & (~(1 << 2)),
			(phy->phy_cfg[i] + 0x54));

		reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
		cnt = 0;
		while (reg1.b.phy_rdy != 1) {
			reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
			/*we wait phy ready max 1ms, common is 100us*/
			if (cnt > 200)
				break;

			cnt++;
			udelay(5);
		}
	}

	/* step 7: pll setting */
	for (i = 0; i < phy->portnum; i++) {
		phy->set_usb_pll(phy, phy->phy_cfg[i]);
		set_trim_initvalue(phy, phy->phy_cfg[i], i);
	}

	if (phy->suspend_flag)
		phy->suspend_flag = 0;

	return 0;
}

static int amlogic_crg_drd_usb2_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static int amlogic_crg_drd_usb2_phy_set_mode(struct amlogic_usb_v2 *phy, int port,
			enum aml_usb_phy_mode mode)
{
	u32 val = 0;
	union u2p_r0_v2 reg0;
	void __iomem *cfg = phy->phy_cfg[port];

	switch (mode) {
	case AML_USB_PHY_MODE_USB_HOST:
		reg0.d32 = readl(&phy->u2p_aml_regs[port]->r0);
		if (phy->suspend_flag == 0) {
			reg0.b.host_device = 1;
			reg0.b.POR = 0;
			reg0.b.IDPULLUP0 = 1;
			reg0.b.DRVVBUS0 = 1;
		}
		writel(reg0.d32, &phy->u2p_aml_regs[port]->r0);
		break;
	case AML_USB_PHY_MODE_USB_DEVICE:
		reg0.d32 = readl(&phy->u2p_aml_regs[port]->r0);
		if (phy->suspend_flag == 0) {
			reg0.b.host_device = 0;
			reg0.b.POR = 0;
			reg0.b.IDPULLUP0 = 1;
			reg0.b.DRVVBUS0 = 1;
		}
		writel(reg0.d32, &phy->u2p_aml_regs[port]->r0);
		break;
	case AML_USB_PHY_MODE_USB_HS:
		/* Default HS. */
		break;
	case AML_USB_PHY_MODE_USB_HSP:
		au2p_dbg(phy->dev, "%s setmode %d.\n", __func__, mode);
		switch (phy->ic_ver) {
		case MESON_CPU_MAJOR_ID_T6D:
#define USB2_SEL_STRENGTH ((u32)GENMASK(30, 29))
#define USB2_SEL_STRENGTH_VAL(x) ((0x3 & (x)) << 29)
			/* usbpll_reve[0]: configure analog and pll to 960m usb2T mode. */
			val = readl(cfg + 0x44);
			val |= BIT(10);
			writel(val, cfg + 0x44);
			usleep_range(20, 30);
			/* Configure controller to 48M. */
			usleep_range(20, 30);
			val = readl(phy->regs + 0x84);
			if (phy->clk_mux == 2 || phy->clk_mux == 3) {
				/* Use SoC 48M ref_clk */
				val |= BIT(1);
			} else if (phy->clk_mux == 1) {
				/* Use phy PLL hs_clk. */
				val |= BIT(0);
			}
			writel(val, phy->regs + 0x84);
			break;
		default:
			break;
		}
		break;
	case AML_USB_PHY_MODE_USB_OTG:
		/* ID DETECT: usb2_otg_aca_en set to 0 */
		writel(readl(cfg + 0x54) & (~(1 << 2)), (cfg + 0x54));
		/* usb2_otg_iddet_en set to 1 by otg driver. */
		break;
	default:
		break;
	}
	return 0;
}

static int amlogic_crg_drd_usb2_phy_cali_disc_squelch
			(struct amlogic_usb_v2 *phy, int port)
{
	u32 val = 0;
	void __iomem *cfg = phy->phy_cfg[port];

	switch (phy->ic_ver) {
	case MESON_CPU_MAJOR_ID_T6D:
/* usb2_squelch_trim:
 *	usb2.0: reg32_14[28]##reg32_03[2:0] (MSB->LSB) default 0b0111.
 *	usb2.2(2t): reg32_23[31:28] (MSB->LSB) default 0b0000.
 * usb2_disc_trim:
 *	usb2.0 & usb2.2: reg32_14[27]##reg32_03[6:4] (MSB->LSB) default 0b1000.
 */
#define SQUELCH_960M ((u32)GENMASK(31, 28))
#define SQUELCH_960M_VAL(x) ((0xf & (x)) << 28)
#define DISCONNECT_MASK ((u32)GENMASK(6, 4))
#define DISCONNECT_VAL(x) ((0x7 & (x)) << 4)

		val = readl(cfg + 0xc);
		val = (val & ~DISCONNECT_MASK) | DISCONNECT_VAL(0x5);
		writel(val, cfg + 0xc);

		if (phy->portspeed == USB_SPEED_HIGH_PLUS) {
			/* Trimming for squelch(960M) detect threshold. */
			val = readl(cfg + 0x5c);
			val = (val & ~SQUELCH_960M) | SQUELCH_960M_VAL(0x9);
			writel(val, cfg + 0x5c);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int amlogic_crg_drd_usb2_phy_set_misc(struct amlogic_usb_v2 *phy, int port)
{
	u32 val = 0;
	void __iomem *cfg = phy->phy_cfg[port];

	switch (phy->ic_ver) {
	case MESON_CPU_MAJOR_ID_T6D:
#define USB2_SEL_STRENGTH ((u32)GENMASK(30, 29))
#define USB2_SEL_STRENGTH_VAL(x) ((0x3 & (x)) << 29)
#define ORW_USB2_EDGEDRV_EN BIT(13)
#define ORW_USB2_EDGEDRV_TRIM ((u32)GENMASK(15, 14))
#define ORW_USB2_EDGEDRV_TRIM_VAL(x) ((0x3 & (x)) << 14)

		if (phy->portspeed == USB_SPEED_HIGH_PLUS) {
			/* usb2_sel_strength 960M set to 1. */
			val = readl(cfg + 0x38);
			val = (val & ~USB2_SEL_STRENGTH) | USB2_SEL_STRENGTH_VAL(0x1);
			writel(val, cfg + 0x38);
			/* edgedrv cali for signal quality. */
			val = readl(cfg + 0x50);
			val = (val & ~ORW_USB2_EDGEDRV_TRIM) | ORW_USB2_EDGEDRV_TRIM_VAL(0x0) |
					ORW_USB2_EDGEDRV_EN;
			writel(val, cfg + 0x50);
		} else {
			/* edgedrv cali for signal quality. */
			val = readl(cfg + 0x50);
			val = (val & ~ORW_USB2_EDGEDRV_TRIM) | ORW_USB2_EDGEDRV_TRIM_VAL(0x1) |
					ORW_USB2_EDGEDRV_EN;
			writel(val, cfg + 0x50);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int amlogic_crg_drd_usb2_phy_cali(struct amlogic_usb_v2 *phy, int port)
{
	int ret;

	usb_set_calibration_trim(phy->phy_cfg[port], phy);
	ret = amlogic_crg_drd_usb2_phy_cali_disc_squelch(phy, port);
	if (ret)
		goto done;
	ret = amlogic_crg_drd_usb2_phy_set_misc(phy, port);
	if (ret)
		goto err;

done:
	return ret;
err:
	return ret;
}

static void amlogic_crg_drd_usb2_phy_set_pll(struct amlogic_usb_v2 *phy, int port)
{
	void __iomem *phy_reg_base = phy->phy_cfg[port];
	u32 retry = 5;
	int plldone_i;
	u32 val;
	void __iomem *pll_cfg = phy_reg_base + 0x40;

	switch (phy->ic_ver) {
	case MESON_CPU_MAJOR_ID_T6D:
#define USB_PLL_RST	BIT(0)
#define USB_MPLL_EN_CTRL BIT(1)
#define	USB_PLL_LK_RST BIT(28)
#define USB_PLL_EN BIT(11)
#define	USBPLL_LOCK_FLAG BIT(31)
		while (retry--) {
			/* Assert usb_pll_rst and usb_pll_lk_rst. */
			val = readl(pll_cfg);
			val |= USB_PLL_RST | USB_PLL_LK_RST;
			writel(val, pll_cfg);
			usleep_range(20, 30);
			/* Enable sw force control . */
			val = readl(pll_cfg);
			val |= USB_MPLL_EN_CTRL | USB_PLL_EN;
			writel(val, pll_cfg);
			usleep_range(20, 30);
			/* Deassert usb_pll_rst. */
			val = readl(pll_cfg);
			val &= ~USB_PLL_RST;
			writel(val, pll_cfg);
			usleep_range(20, 30);
			/* Deassert usb_pll_lk_rst. */
			val = readl(pll_cfg);
			val &= ~USB_PLL_LK_RST;
			writel(val, pll_cfg);
			/* Check lock bit. */
			for (plldone_i = 5; plldone_i > 0; plldone_i--) {
				usleep_range(20, 30);
				if (readl(pll_cfg) & USBPLL_LOCK_FLAG)
					goto okay;
			}
			au2p_dbg(phy->dev, "usb2 pll not locked, retry. val: 0x%08x\n",
							readl(pll_cfg));
		}
		break;
	default:
		break;
	}

	au2p_err(phy->dev, "%s set pll failed, exit, val:0x%08x.\n", __func__, readl(pll_cfg));
	return;
okay:
	au2p_dbg(phy->dev, "usb2 pll init done, val: 0x%08x\n", readl(pll_cfg));
}

static int amlogic_crg_drd_usb2_phy_wait_ready(struct amlogic_usb_v2 *phy,
		int port, unsigned int timeout)
{
	union u2p_r1_v2 reg1;
	unsigned int cnt = 0;
	int ret = 0;

	reg1.d32 = readl(&phy->u2p_aml_regs[port]->r1);

	while (reg1.b.phy_rdy != 1) {
		reg1.d32 = readl(&phy->u2p_aml_regs[port]->r1);
		/*we wait phy ready max 1ms, common is 100us*/
		if (cnt > timeout) {
			ret = -ETIMEDOUT;
			break;
		}
		cnt++;
		udelay(5);
	}

	return ret;
}

static int amlogic_crg_drd_usb2_init_v1(struct usb_phy *x)
{
	int i, ret;
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);

	amlogic_crg_drd_usb2_set_vbus_power(phy, 1);

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB2_PHY_ENABLE) {
			ret = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
			if (ret) {
				au2p_err(phy->dev, "Failed to enable usb2 phy bus clock at %d\n",
							__LINE__);
			}
		}
	}

	amlogic_crg_drd_usbphy_hold_reset(phy, false);
	usleep_range(49, 50);
	amlogic_crg_drd_usbphy_reg_hold_reset(phy, true);
	usleep_range(49, 50);
	amlogic_crg_drd_usbphy_reg_reset(phy);
	usleep_range(49, 50);

	au2p_dbg(phy->dev, "init r0~r2 0x%x 0x%x 0x%x.\n",
			readl(&phy->u2p_aml_regs[0]->r0),
			readl(&phy->u2p_aml_regs[0]->r1),
			readl(&phy->u2p_aml_regs[0]->r2));

	for (i = 0; i < phy->portnum; i++) {
		amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_HOST);
		if (phy->portspeed == USB_SPEED_HIGH_PLUS)
			amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_HSP);
		else
			amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_HS);
		amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_OTG);

		amlogic_crg_drd_usb2_phy_cali(phy, i);
		/* phy_cfg + 0xc is set in the amlogic_crg_drd_usb2_phy_cali_disc_squelch now. */
		set_trim_initvalue(phy, phy->phy_cfg[i], i);
	}

	amlogic_crg_drd_usbphy_hold_reset(phy, true);
	usleep_range(49, 50);

	for (i = 0; i < phy->portnum; i++) {
		ret = amlogic_crg_drd_usb2_phy_wait_ready(phy, i, 200);
		if (ret)
			au2p_err(phy->dev, " wait for ready timeout.\n");
	}

	for (i = 0; i < phy->portnum; i++)
		amlogic_crg_drd_usb2_phy_set_pll(phy, i);

	au2p_dbg(phy->dev, "end r0~r2 0x%x 0x%x 0x%x.\n",
			readl(&phy->u2p_aml_regs[0]->r0),
			readl(&phy->u2p_aml_regs[0]->r1),
			readl(&phy->u2p_aml_regs[0]->r2));

	if (phy->suspend_flag)
		phy->suspend_flag = 0;

	return ret;
}

static int amlogic_crg_drd_usb2_init(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	int ret = -EINVAL;

	if (local_pdata(phy)->ver == AML_CRG_DRD_USB2_PHY_V1)
		ret = amlogic_crg_drd_usb2_init_v1(x);
	else
		ret = amlogic_crg_drd_usb2_init_v0(x);

	return ret;
}

static void amlogic_crg_drd_usb2phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	int ret;

	if (local_pdata(phy)->ver == AML_CRG_DRD_USB2_PHY_V0)
		amlogic_crg_drd_usb2_set_controller_power(phy, false, false);

	ret = amlogic_crg_drd_usbphy_hold_reset(phy, false);
	ret = amlogic_crg_drd_usbphy_reg_hold_reset(phy, false);

	if (phy->suspend_flag  == 0)
		if (phy->phy.flags == AML_USB2_PHY_ENABLE)
			clk_bulk_disable_unprepare(phy->clk_num, phy->clks);

	phy->suspend_flag = 1;
}

static int amlogic_crg_device_usb2_init_v1(struct amlogic_usb_v2 *phy)
{
	int i, ret;

	amlogic_crg_drd_usb2_set_vbus_power(phy, 0);

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB2_PHY_ENABLE) {
			ret = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
			if (ret) {
				au2p_err(phy->dev, "Failed to enable usb2 phy bus clock at %d\n",
							__LINE__);
			}
		}
	}

	amlogic_crg_drd_usbphy_hold_reset(phy, false);
	usleep_range(49, 50);
	amlogic_crg_drd_usbphy_reg_hold_reset(phy, true);
	usleep_range(49, 50);
	if (!phy->suspend_flag) {
		amlogic_crg_drd_usbphy_reg_reset(phy);
		usleep_range(49, 50);
	}

	au2p_dbg(phy->dev, "init r0~r2 0x%x 0x%x 0x%x.\n",
			readl(&phy->u2p_aml_regs[0]->r0),
			readl(&phy->u2p_aml_regs[0]->r1),
			readl(&phy->u2p_aml_regs[0]->r2));

	for (i = 0; i < phy->portnum; i++) {
		amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_DEVICE);
		if (phy->portspeed == USB_SPEED_HIGH_PLUS)
			amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_HSP);
		else
			amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_HS);
		amlogic_crg_drd_usb2_phy_set_mode(phy, i, AML_USB_PHY_MODE_USB_OTG);

		amlogic_crg_drd_usb2_phy_cali(phy, i);
		/* phy_cfg + 0xc is set in the amlogic_crg_drd_usb2_phy_cali_disc_squelch now. */
		set_trim_initvalue(phy, phy->phy_cfg[i], i);
	}

	amlogic_crg_drd_usbphy_hold_reset(phy, true);
	usleep_range(49, 50);

	for (i = 0; i < phy->portnum; i++) {
		ret = amlogic_crg_drd_usb2_phy_wait_ready(phy, i, 200);
		if (ret)
			au2p_err(phy->dev, " wait for ready timeout.\n");
	}

	for (i = 0; i < phy->portnum; i++)
		amlogic_crg_drd_usb2_phy_set_pll(phy, i);

	au2p_dbg(phy->dev, "end r0~r2 0x%x 0x%x 0x%x.\n",
			readl(&phy->u2p_aml_regs[0]->r0),
			readl(&phy->u2p_aml_regs[0]->r1),
			readl(&phy->u2p_aml_regs[0]->r2));

	if (phy->suspend_flag)
		phy->suspend_flag = 0;

	return ret;
}

int amlogic_crg_device_usb2_init(u32 phy_id)
{
	struct amlogic_usb_v2 *phy = g_crg_drd_phy2[phy_id];

	int ret = -EINVAL;

	if (local_pdata(phy)->ver == AML_CRG_DRD_USB2_PHY_V1)
		ret = amlogic_crg_device_usb2_init_v1(phy);
	else
		ret = amlogic_crg_device_usb2_init_v0(phy);

	return ret;
}
EXPORT_SYMBOL(amlogic_crg_device_usb2_init);

int amlogic_crg_device_usb2_shutdown(u32 phy_id)
{
	struct amlogic_usb_v2 *phy;
	int ret = 0;

	phy = g_crg_drd_phy2[phy_id];

	ret = amlogic_crg_drd_usbphy_hold_reset(phy, false);
	ret = amlogic_crg_drd_usbphy_reg_hold_reset(phy, false);

	if (phy->suspend_flag  == 0)
		if (phy->phy.flags == AML_USB2_PHY_ENABLE)
			clk_bulk_disable_unprepare(phy->clk_num, phy->clks);

	phy->suspend_flag = 1;

	return ret;
}
EXPORT_SYMBOL(amlogic_crg_device_usb2_shutdown);

int amlogic_crg_host_power(struct usb_phy *p, bool force, bool on)
{
	struct amlogic_usb_v2 *phy;

	if (!p->label || strcmp("amlogic-crg-drd-phy2", p->label))
		return -EINVAL;

	phy = phy_to_amlusb(p);

	amlogic_crg_drd_usb2_set_controller_power(phy, force, on);
	return 0;
}

int amlogic_crg_device_power(u32 phy_id, bool force, bool on)
{
	struct amlogic_usb_v2 *phy = g_crg_drd_phy2[phy_id];

	if (!phy)
		return -EINVAL;

	amlogic_crg_drd_usb2_set_controller_power(phy, force, on);
	return 0;
}

static int amlogic_crg_drd_usb2_probe(struct platform_device *pdev)
{
	struct amlogic_usb_v2 *phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	struct resource *reset_mem;
	struct resource *phy_cfg_mem[4];
	void __iomem	*phy_base;
	void __iomem	*reset_base = NULL;
	void __iomem	*phy_cfg_base[4] = {NULL, NULL, NULL, NULL};
	unsigned int	usb_clk_reg;
	void __iomem	*usb_clk_reg_base = NULL;
	unsigned int clk_regsize = 0;
	unsigned int	phy_trim_reg;
	void __iomem	*usb_phy_trim_reg = NULL;
	int portnum = 0;
	int phy_version = 0;
	int portspeed = USB_SPEED_HIGH;
	int reset_level = 0x84;
	const void *prop;
	int i = 0;
	int retval;
	u32 pll_setting[8] = {0};
	u32 pll_disconnect_enhance;
	u32 pll_ver;
	u32 ic_ver = get_cpu_type();
	u32 phy_reset_level_bit[USB_PHY_MAX_NUMBER] = {-1};
	u32 usb_reset_bit = -1U;
	u32 usb_comb_reset_bit = -1U;
	u32 phy_reg_reset_level_bit[USB_PHY_MAX_NUMBER] = {-1};
	u32 otg_phy_index = 1;
	u32 phy_id = 0;
	u32 val;
	u32 usbclk_div = 0;
	const char *gpio_name = NULL;
	int gpio_vbus_power_pin = -1;
	struct gpio_desc *usb_gd = NULL;
	bool pm_controller = false;
	bool sw_hsp = false;

	gpio_name = of_get_property(dev->of_node, "gpio-vbus-power", NULL);
	if (gpio_name) {
		gpio_vbus_power_pin = 1;
		usb_gd = devm_gpiod_get_index(&pdev->dev,
					 NULL, 0, GPIOD_OUT_LOW);
		if (IS_ERR(usb_gd))
			return -1;
	}

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum) {
		dev_err(&pdev->dev, "This phy has no usb port\n");
		return -ENOMEM;
	}

	prop = of_get_property(dev->of_node, "usb-busclk_ctl_div", NULL);
	if (prop) {
		usbclk_div = of_read_ulong(prop, 1);
		dev_info(&pdev->dev, "usb_clk_ctl_div=%u\n", usbclk_div);
	}

	prop = of_get_property(dev->of_node, "version", NULL);
	if (prop)
		phy_version = of_read_ulong(prop, 1);
	else
		phy_version = 0;

	prop = of_get_property(dev->of_node, "reset-level", NULL);
	if (prop)
		reset_level = of_read_ulong(prop, 1);
	else
		reset_level = 0x84;

	prop = of_get_property(dev->of_node, "otg-phy-index", NULL);
	if (prop)
		otg_phy_index = of_read_ulong(prop, 1);
	else
		otg_phy_index = 1;

	prop = of_get_property(dev->of_node, "phy-id", NULL);
	if (prop)
		phy_id = of_read_ulong(prop, 1);
	else
		phy_id = 0;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->pdata = of_device_get_match_data(dev);
	if (!phy->pdata)
		return -EINVAL;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = ioremap(phy_mem->start, resource_size(phy_mem));
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (reset_mem) {
		reset_base = ioremap(reset_mem->start,
			resource_size(reset_mem));
		if (IS_ERR(reset_base))
			return PTR_ERR(reset_base);
	}

	for (i = 0; i < portnum; i++) {
		phy_cfg_mem[i] = platform_get_resource
					(pdev, IORESOURCE_MEM, 2 + i);
		if (phy_cfg_mem[i]) {
			phy_cfg_base[i] = ioremap(phy_cfg_mem[i]->start,
				resource_size(phy_cfg_mem[i]));
			if (IS_ERR(phy_cfg_base[i]))
				return PTR_ERR(phy_cfg_base[i]);
		}
	}

	retval = of_property_read_u32(dev->of_node, "usb-phy-trim-reg",
				      &phy_trim_reg);
	if (retval >= 0) {
		usb_phy_trim_reg = ioremap((resource_size_t)phy_trim_reg,
					   4);
	}

	retval = of_property_read_u32(dev->of_node, "usb-clk-reg",
				      &usb_clk_reg);
	if (retval >= 0) {
		retval = of_property_read_u32(dev->of_node,
					      "usb-clkreg-size",
					      &clk_regsize);
		if (retval >= 0) {
			usb_clk_reg_base = ioremap((resource_size_t)usb_clk_reg,
						   (unsigned long)clk_regsize);
			if (usb_clk_reg_base) {
				if (usbclk_div) {
					writel(usbclk_div, usb_clk_reg_base);
				} else {
					val = readl(usb_clk_reg_base);
					val |= (0x1 << 8) | (0x1 << 9)
						| (0 << 0);
					writel(val, usb_clk_reg_base);
				}
			}
		}
	}

	for (i = 0; i < portnum; i++) {
		memset(name_crg, 0, 32 * sizeof(char));
		sprintf(name_crg, "phy%d-reset-level-bit", i);
		prop = of_get_property(dev->of_node, name_crg, NULL);
		if (prop)
			phy_reset_level_bit[i] = of_read_ulong(prop, 1);
		else
			phy_reset_level_bit[i] = 16 + i;

		memset(name_crg, 0, 32 * sizeof(char));
		sprintf(name_crg, "phy%d-reg-reset-level-bit", i);
		prop = of_get_property(dev->of_node, name_crg, NULL);
		if (prop)
			phy_reg_reset_level_bit[i] = of_read_ulong(prop, 1);
		else
			phy_reg_reset_level_bit[i] = -1U;
	}

	prop = of_get_property(dev->of_node, "usb-reset-bit", NULL);
	if (prop)
		usb_reset_bit = of_read_ulong(prop, 1);

	prop = of_get_property(dev->of_node, "usb-comb-reset-bit", NULL);
	if (prop)
		usb_comb_reset_bit = of_read_ulong(prop, 1);

	pm_controller = of_property_read_bool(dev->of_node, "pm-controller");

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-1", &pll_setting[0]);

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-2", &pll_setting[1]);

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-3", &pll_setting[2]);

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-4", &pll_setting[3]);

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-5", &pll_setting[4]);

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-6", &pll_setting[5]);

	retval = of_property_read_u32(dev->of_node,
			"pll-setting-7", &pll_setting[6]);

	retval = of_property_read_u32(dev->of_node,
			"pll-setting-8", &pll_setting[7]);

	retval = of_property_read_u32(dev->of_node,
		"dis-thred-enhance", &pll_disconnect_enhance);
	if (retval < 0)
		pll_disconnect_enhance = 0;

	retval = of_property_read_u32(dev->of_node,
		"pll-version", &pll_ver);
	if (retval < 0)
		pll_ver = 0;

	retval = of_property_read_s32(dev->of_node,
		"portspeed", &portspeed);
	if (retval < 0)
		portspeed = USB_SPEED_HIGH;

	sw_hsp = of_property_read_bool(dev->of_node, "sw-hsp");

	retval = of_property_read_u32(dev->of_node,
			"clk-mux", &phy->clk_mux);
	if (retval < 0)
		phy->clk_mux = 0;

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->reset_regs = reset_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-crg-drd-phy2";
	phy->phy.init		= amlogic_crg_drd_usb2_init;
	phy->phy.set_suspend	= amlogic_crg_drd_usb2_suspend;
	phy->phy.shutdown	= amlogic_crg_drd_usb2phy_shutdown;
	phy->phy_trim_tuning	= set_usb_phy_trim_tuning;
	phy->phy.type		= USB_PHY_TYPE_USB2;
	phy->pll_setting[0] = pll_setting[0];
	phy->pll_setting[1] = pll_setting[1];
	phy->pll_setting[2] = pll_setting[2];
	phy->pll_setting[3] = pll_setting[3];
	phy->pll_setting[4] = pll_setting[4];
	phy->pll_setting[5] = pll_setting[5];
	phy->pll_setting[6] = pll_setting[6];
	phy->pll_setting[7] = pll_setting[7];
	phy->pll_dis_thred_enhance = pll_disconnect_enhance;
	phy->pll_ver = pll_ver;
	phy->ic_ver = ic_ver;
	phy->suspend_flag = 0;
	phy->phy_version = phy_version;
	phy->phy_version = phy_version;
	phy->otg_phy_index = otg_phy_index;
	phy->reset_level = reset_level;
	phy->usb_reset_bit = usb_reset_bit;
	phy->usb_comb_reset_bit = usb_comb_reset_bit;
	phy->pm_controller = pm_controller;
	phy->sw_hsp = sw_hsp;
	phy->usb_phy_trim_reg = usb_phy_trim_reg;
	phy->phy_id = phy_id;
	phy->portspeed = portspeed;
	phy->vbus_power_pin = gpio_vbus_power_pin;
	phy->usb_gpio_desc = usb_gd;
	for (i = 0; i < portnum; i++) {
		phy->phy_cfg[i] = phy_cfg_base[i];
		/* set port default tuning state */
		phy->phy_cfg_state[i] = 1;
		phy->phy_trim_state[i] = 1;
		phy->phy_reset_level_bit[i] = phy_reset_level_bit[i];
		phy->phy_reg_reset_level_bit[i] = phy_reg_reset_level_bit[i];
		phy->u2p_aml_regs[i] = phy->regs + 0x20 * i;
	}

	if  (aml_crg_drd_usb2_hsp(phy)) {
		phy->portspeed = USB_SPEED_HIGH_PLUS;
		/* Choose 48M soc digital clk src when the port is at HS mode. */
		if (phy->clk_mux == 0)
			phy->clk_mux = 2;
		au2p_info(dev, "usb2t_mode %d forces portspeed to %d.",
						aml_usb2_phy_960m, portspeed);
	}

	if (phy->clk_mux != 0 && phy->portspeed != USB_SPEED_HIGH_PLUS) {
		au2p_info(dev, "wrong clk-mux=%d in normal HS mode, default to 0.",
						phy->clk_mux);
		phy->clk_mux = 0;
	}

	if (phy->portspeed == USB_SPEED_HIGH_PLUS && phy->clk_mux == 0) {
		phy->clk_mux = 2;
		au2p_info(dev, "clk-mux=0 in HSP mode, default to %d.", phy->clk_mux);
	}

	/* Don't touch any clks not used. */
	switch (phy->clk_mux) {
	/* Normal HS mode. */
	case 0:
		phy->clks[0].id = "crg_general";
		phy->clk_num = 1;
		break;
	/* HSP mode.*/
	/* 48M phy digital clk src. */
	case 1:
		phy->clks[0].id = "crg_general";
		phy->clk_num = 1;
		break;
	/* 48M soc digital clk src. */
	case 2:
		phy->clks[0].id = "crg_general";
		phy->clks[1].id = "clk_soc_u2drd_48m_pre";
		phy->clks[2].id = "clk_soc_u2drd_48m";
		phy->clk_num = 3;
		break;
	/* 48M soc analog clk src. */
	case 3:
		phy->clks[0].id = "crg_general";
		phy->clks[1].id = "clk_soc_u2drd_48m";
		phy->clk_num = 2;
		break;
	}

	au2p_info(&pdev->dev, "USB2 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n"
						 "USB2 phy mode: %d, clk mux: %d\n",
						(unsigned long)phy_mem->start,
						(unsigned long)phy_base,
						phy->portspeed, phy->clk_mux);

	retval = devm_clk_bulk_get(dev, phy->clk_num, phy->clks);
	if (retval) {
		au2p_err(dev, "Failed to get usb2 phy bus clock\n");
		return retval;
	}

	for (i = 0; i < AML_USB_PHY_MAX_CLK_NUMBER; i++)
		au2p_dbg(dev, "%px %s.\n", (void *)phy->clks[i].clk, phy->clks[i].id);

	if (phy->clk_mux == 2) {
		if (phy->clks[1].clk)
			clk_set_rate(phy->clks[1].clk, 480000000);
	}

	retval = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
	if (retval) {
		au2p_err(dev, "Failed to enable usb2 phy bus clock\n");
		return retval;
	}

	phy->phy.flags = AML_USB2_PHY_ENABLE;

	switch (phy->pll_ver) {
	case 0:
		phy->set_usb_pll = set_usb_pll_v0;
		break;
	case 1:
		phy->set_usb_pll = set_usb_pll_v1;
		break;
	case 2:
		phy->set_usb_pll = set_usb_pll_v2;
		break;
	case 3:
		phy->set_usb_pll = set_usb_pll_v3;
		break;
	default:
		au2p_err(phy->dev, "No matching pll-version.");
		return -EINVAL;
	}

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	g_crg_drd_phy2[phy_id] = phy;

	amlogic_crg_drd_usb2_init(&phy->phy);

	return 0;
}

static int amlogic_crg_drd_usb2_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_crg_drd_usb2_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_crg_drd_usb2_runtime_resume(struct device *dev)
{
	unsigned int ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_crg_drd_usb2_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_crg_drd_usb2_runtime_suspend,
		amlogic_crg_drd_usb2_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_crg_drd_usb2_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF

static const struct aml_crg_drd_usb2_pdata aml_phy_v0_data = {
	.ver = AML_CRG_DRD_USB2_PHY_V0,
};

static const struct aml_crg_drd_usb2_pdata aml_phy_v1_data = {
	.ver = AML_CRG_DRD_USB2_PHY_V1,
};

static const struct of_device_id amlogic_crg_drd_usb2_id_table[] = {
	{ .compatible = "amlogic, amlogic-crg-drd-usb2", .data = &aml_phy_v0_data },
	{ .compatible = "amlogic, amlogic-crg-drd-usb2-v0", .data = &aml_phy_v0_data },
	{ .compatible = "amlogic, amlogic-crg-drd-usb2-v1", .data = &aml_phy_v1_data },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_crg_drd_usb2_id_table);
#endif

static struct platform_driver amlogic_crg_drd_usb2_driver = {
	.probe		= amlogic_crg_drd_usb2_probe,
	.remove		= amlogic_crg_drd_usb2_remove,
	.driver		= {
		.name	= "amlogic-crg-drd-usb2",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_crg_drd_usb2_id_table),
	},
};

#if 0
module_platform_driver(amlogic_crg_drd_usb2_driver);

MODULE_ALIAS("platform: amlogic_crg_drd");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic crg drd USB2 phy driver");
MODULE_LICENSE("GPL v2");
#else
int __init amlogic_crg_drd_usb2_drv_init(void)
{
	return platform_driver_register(&amlogic_crg_drd_usb2_driver);
}
#endif
