// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2016-2018, 2020, The Linux Foundation. All rights reserved.
 */

/*
 * Display Port PLL driver block diagram for branch clocks
 *
 * +------------------------+       +------------------------+
 * |   dp_phy_pll_link_clk  |       | dp_phy_pll_vco_div_clk |
 * +------------------------+       +------------------------+
 *             |                               |
 *             |                               |
 *             V                               V
 *        dp_link_clk                     dp_pixel_clk
 *
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/usb/usbpd.h>
#include "clk-regmap-mux.h"
#include "dp_debug.h"
#include "dp_pll.h"

#define DP_PHY_REVISION_ID0			0x0000
#define DP_PHY_REVISION_ID1			0x0004
#define DP_PHY_REVISION_ID2			0x0008
#define DP_PHY_REVISION_ID3			0x000C

#define DP_PHY_CFG				0x0010
#define DP_PHY_CFG_1				0x0014
#define DP_PHY_PD_CTL				0x0018
#define DP_PHY_MODE				0x001C

#define DP_PHY_AUX_CFG0				0x0020
#define DP_PHY_AUX_CFG1				0x0024
#define DP_PHY_AUX_CFG2				0x0028
#define DP_PHY_AUX_CFG3				0x002C
#define DP_PHY_AUX_CFG4				0x0030
#define DP_PHY_AUX_CFG5				0x0034
#define DP_PHY_AUX_CFG6				0x0038
#define DP_PHY_AUX_CFG7				0x003C
#define DP_PHY_AUX_CFG8				0x0040
#define DP_PHY_AUX_CFG9				0x0044
#define DP_PHY_AUX_INTERRUPT_MASK		0x0048
#define DP_PHY_AUX_INTERRUPT_CLEAR		0x004C
#define DP_PHY_AUX_BIST_CFG			0x0050

#ifdef DP_PHY_VCO_DIV
#undef DP_PHY_VCO_DIV
#define DP_PHY_VCO_DIV				0x0068
#endif

#define DP_PHY_TX0_TX1_LANE_CTL			0x006C

#define DP_PHY_TX2_TX3_LANE_CTL			0x0088
#define DP_PHY_SPARE0				0x00AC
#define DP_PHY_STATUS				0x00C0

/* Tx registers */
#define QSERDES_TX0_OFFSET			0x0400
#define QSERDES_TX1_OFFSET			0x0800

#define TXn_BIST_MODE_LANENO			0x0000
#define TXn_CLKBUF_ENABLE			0x0008
#define TXn_TX_EMP_POST1_LVL			0x000C

#define TXn_TX_DRV_LVL				0x001C

#define TXn_RESET_TSYNC_EN			0x0024
#define TXn_PRE_STALL_LDO_BOOST_EN		0x0028
#define TXn_TX_BAND				0x002C
#define TXn_SLEW_CNTL				0x0030
#define TXn_INTERFACE_SELECT			0x0034

#define TXn_RES_CODE_LANE_TX			0x003C
#define TXn_RES_CODE_LANE_RX			0x0040
#define TXn_RES_CODE_LANE_OFFSET_TX		0x0044
#define TXn_RES_CODE_LANE_OFFSET_RX		0x0048

#define TXn_DEBUG_BUS_SEL			0x0058
#define TXn_TRANSCEIVER_BIAS_EN			0x005C
#define TXn_HIGHZ_DRVR_EN			0x0060
#define TXn_TX_POL_INV				0x0064
#define TXn_PARRATE_REC_DETECT_IDLE_EN		0x0068

#define TXn_LANE_MODE_1				0x008C

#define TXn_TRAN_DRVR_EMP_EN			0x00C0
#define TXn_TX_INTERFACE_MODE			0x00C4

#define TXn_VMODE_CTRL1				0x00F0


/* PLL register offset */
#define QSERDES_COM_ATB_SEL1			0x0000
#define QSERDES_COM_ATB_SEL2			0x0004
#define QSERDES_COM_FREQ_UPDATE			0x0008
#define QSERDES_COM_BG_TIMER			0x000C
#define QSERDES_COM_SSC_EN_CENTER		0x0010
#define QSERDES_COM_SSC_ADJ_PER1		0x0014
#define QSERDES_COM_SSC_ADJ_PER2		0x0018
#define QSERDES_COM_SSC_PER1			0x001C
#define QSERDES_COM_SSC_PER2			0x0020
#define QSERDES_COM_SSC_STEP_SIZE1		0x0024
#define QSERDES_COM_SSC_STEP_SIZE2		0x0028
#define QSERDES_COM_POST_DIV			0x002C
#define QSERDES_COM_POST_DIV_MUX		0x0030
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		0x0034
#define QSERDES_COM_CLK_ENABLE1			0x0038
#define QSERDES_COM_SYS_CLK_CTRL		0x003C
#define QSERDES_COM_SYSCLK_BUF_ENABLE		0x0040
#define QSERDES_COM_PLL_EN			0x0044
#define QSERDES_COM_PLL_IVCO			0x0048
#define QSERDES_COM_LOCK_CMP1_MODE0		0x004C
#define QSERDES_COM_LOCK_CMP2_MODE0		0x0050
#define QSERDES_COM_LOCK_CMP3_MODE0		0x0054

#define QSERDES_COM_CP_CTRL_MODE0		0x0078
#define QSERDES_COM_CP_CTRL_MODE1		0x007C
#define QSERDES_COM_PLL_RCTRL_MODE0		0x0084
#define QSERDES_COM_PLL_CCTRL_MODE0		0x0090
#define QSERDES_COM_PLL_CNTRL			0x009C

#define QSERDES_COM_SYSCLK_EN_SEL		0x00AC
#define QSERDES_COM_CML_SYSCLK_SEL		0x00B0
#define QSERDES_COM_RESETSM_CNTRL		0x00B4
#define QSERDES_COM_RESETSM_CNTRL2		0x00B8
#define QSERDES_COM_LOCK_CMP_EN			0x00C8
#define QSERDES_COM_LOCK_CMP_CFG		0x00CC


#define QSERDES_COM_DEC_START_MODE0		0x00D0
#define QSERDES_COM_DEC_START_MODE1		0x00D4
#define QSERDES_COM_DIV_FRAC_START1_MODE0	0x00DC
#define QSERDES_COM_DIV_FRAC_START2_MODE0	0x00E0
#define QSERDES_COM_DIV_FRAC_START3_MODE0	0x00E4

#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0	0x0108
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0	0x010C
#define QSERDES_COM_VCO_TUNE_CTRL		0x0124
#define QSERDES_COM_VCO_TUNE_MAP		0x0128
#define QSERDES_COM_VCO_TUNE1_MODE0		0x012C
#define QSERDES_COM_VCO_TUNE2_MODE0		0x0130

#define QSERDES_COM_CMN_STATUS			0x015C
#define QSERDES_COM_RESET_SM_STATUS		0x0160

#define QSERDES_COM_BG_CTRL			0x0170
#define QSERDES_COM_CLK_SELECT			0x0174
#define QSERDES_COM_HSCLK_SEL			0x0178
#define QSERDES_COM_CORECLK_DIV			0x0184
#define QSERDES_COM_SW_RESET			0x0188
#define QSERDES_COM_CORE_CLK_EN			0x018C
#define QSERDES_COM_C_READY_STATUS		0x0190
#define QSERDES_COM_CMN_CONFIG			0x0194
#define QSERDES_COM_SVS_MODE_CLK_SEL		0x019C

#define DP_PLL_POLL_SLEEP_US			500
#define DP_PLL_POLL_TIMEOUT_US			10000

#define DP_PHY_PLL_POLL_SLEEP_US		500
#define DP_PHY_PLL_POLL_TIMEOUT_US		10000

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

#define DP_VCO_HSCLK_RATE_1620MHZDIV1000	1620000UL
#define DP_VCO_HSCLK_RATE_2700MHZDIV1000	2700000UL
#define DP_VCO_HSCLK_RATE_5400MHZDIV1000	5400000UL

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_9720MHZDIV1000		9720000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

#define DP_PLL_NUM_CLKS		2

#define DP_14NM_C_READY		BIT(0)
#define DP_14NM_PLL_LOCKED	BIT(0)
#define DP_14NM_PHY_READY	(BIT(1) | BIT(0))

static const struct dp_pll_params pll_params[HSCLK_RATE_MAX] = {
	/* HSCLK_RATE_1620MHZ  */
	{0x2c, 0x40, 0x00, 0x00, 0x01, 0x69, 0x00, 0x80, 0x07, 0xbf, 0x21,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0f, 0x08, 0x0f, 0x12, 0x12
	},

	/* HSCLK_RATE_2700MHZ  */
	{0x24, 0x40, 0x00, 0x00, 0x01, 0x69, 0x00, 0x80, 0x07, 0x3f, 0x38,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0f, 0x08, 0x0f, 0x12, 0x12
	},

	/* HSCLK_RATE_5400MHZ  */
	{0x20, 0x40, 0x00, 0x00, 0x02, 0x8c, 0x00, 0x00, 0x0a, 0x7f, 0x70,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0f, 0x08, 0x0f, 0x12, 0x12
	},
};

static int set_vco_div(struct dp_pll *pll, unsigned long rate)
{
	u32 div, val;

	if (!pll)
		return -EINVAL;

	val = dp_pll_read(dp_phy, DP_PHY_VCO_DIV);
	val &= ~0x03;

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		div = 2;
		val |= 1;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		div = 4;
		val |= 2;
		break;
	default:
		/* No other link rates are supported */
		return -EINVAL;
	}

	dp_pll_write(dp_phy, DP_PHY_VCO_DIV, val);
	/* Make sure the PHY registers writes are done */
	wmb();

	/*
	 * Set the rate for the link and pixel clock sources so that the
	 * linux clock framework can appropriately compute the MND values
	 * whenever the pixel clock rate is set.
	 */
	clk_set_rate(pll->clk_data->clks[0], (pll->vco_rate / 10));
	clk_set_rate(pll->clk_data->clks[1], pll->vco_rate / div);

	DP_DEBUG("val=%#x div=%x link_clk rate=%lu vco_div_clk rate=%lu\n",
			val, div, pll->vco_rate / 10, pll->vco_rate / div);

	return 0;
}

static int dp_vco_pll_init_db_14nm(struct dp_pll_db *pdb,
			unsigned long rate)
{
	struct dp_pll *pll = pdb->pll;
	u32 spare_value = 0;

	spare_value = dp_pll_read(dp_phy, DP_PHY_SPARE0);
	pdb->lane_cnt = spare_value & 0x0F;
	pdb->orientation = (spare_value & 0xF0) >> 4;

	DP_DEBUG("spare_value=0x%x, ln_cnt=0x%x, orientation=0x%x\n",
			spare_value, pdb->lane_cnt, pdb->orientation);

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_9720MHZDIV1000);
		pdb->rate_idx = HSCLK_RATE_1620MHZ;
		break;
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->rate_idx = HSCLK_RATE_2700MHZ;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->rate_idx = HSCLK_RATE_5400MHZ;
		break;
	default:
		DP_ERR("unsupported rate %ld\n", rate);
		return -EINVAL;
	}

	return 0;
}

int dp_config_vco_rate_14nm(struct dp_pll *pll,
		unsigned long rate)
{
	u32 res = 0;
	struct dp_pll_db *pdb = (struct dp_pll_db *)pll->priv;
	const struct dp_pll_params *params;

	res = dp_vco_pll_init_db_14nm(pdb, rate);
	if (res) {
		pr_err("VCO Init DB failed\n");
		return res;
	}

	dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x3d);

	/* Make sure the PHY register writes are done */
	wmb();

	/*
	 * Limiting rate to 5.4Gbps.
	 * 14nm PLL can only support upto HBR2.
	 */
	if (pdb->rate_idx < HSCLK_RATE_8100MHZ) {
		params = &pdb->pll_params[pdb->rate_idx];
	} else {
		DP_ERR("link rate not set\n");
		return -EINVAL;
	}

	dp_pll_write(dp_pll, QSERDES_COM_SVS_MODE_CLK_SEL, 0x01);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_EN_SEL, 0x37);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_SELECT, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_SYS_CLK_CTRL, 0x06);
	dp_pll_write(dp_pll, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x3f);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_ENABLE1, 0x0e);
	dp_pll_write(dp_pll, QSERDES_COM_BG_CTRL, 0x0f);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_SELECT, 0x30);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_IVCO, 0x0f);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_CCTRL_MODE0, 0x28);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	dp_pll_write(dp_pll, QSERDES_COM_CP_CTRL_MODE0, 0x0b);

	/* Parameters dependent on vco clock frequency */
	dp_pll_write(dp_pll, QSERDES_COM_HSCLK_SEL, params->hsclk_sel);
	dp_pll_write(dp_pll,
		QSERDES_COM_DEC_START_MODE0, params->dec_start_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START1_MODE0, params->div_frac_start1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START2_MODE0, params->div_frac_start2_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START3_MODE0, params->div_frac_start3_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_LOCK_CMP1_MODE0, params->lock_cmp1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_LOCK_CMP2_MODE0, params->lock_cmp2_mode0);

	/* hardcoding with 0x00 for lock_cmp3_mode0 */
	dp_pll_write(dp_pll, QSERDES_COM_LOCK_CMP3_MODE0, 0x00);

	dp_pll_write(dp_pll, QSERDES_COM_INTEGLOOP_GAIN0_MODE0, 0x40);
	dp_pll_write(dp_pll, QSERDES_COM_INTEGLOOP_GAIN1_MODE0, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_MAP, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_BG_TIMER, 0x08);
	dp_pll_write(dp_pll, QSERDES_COM_CORECLK_DIV, 0x05);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE1_MODE0, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE2_MODE0, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	wmb(); /* make sure write happens */

	dp_pll_write(dp_pll, QSERDES_COM_CORE_CLK_EN, 0x0f);
	wmb(); /* make sure write happens */

	if (pdb->rate_idx > HSCLK_RATE_1620MHZ) {
		dp_pll_write(dp_ln_tx0, TXn_LANE_MODE_1, 0xc4);
		dp_pll_write(dp_ln_tx1, TXn_LANE_MODE_1, 0xc4);
	} else {
		dp_pll_write(dp_ln_tx0, TXn_LANE_MODE_1, 0xc6);
		dp_pll_write(dp_ln_tx0, TXn_LANE_MODE_1, 0xc6);
	}

	if (pdb->orientation == ORIENTATION_CC2)
		dp_pll_write(dp_phy, DP_PHY_MODE, 0xc9);
	else
		dp_pll_write(dp_phy, DP_PHY_MODE, 0xd9);

	wmb(); /* make sure write happens */

	/* TX Lane configuration */
	dp_pll_write(dp_phy, DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	dp_pll_write(dp_phy, DP_PHY_TX2_TX3_LANE_CTL, 0x05);

	/* TX-0 register configuration */
	dp_pll_write(dp_ln_tx0, TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	dp_pll_write(dp_ln_tx0, TXn_VMODE_CTRL1, 0x40);
	dp_pll_write(dp_ln_tx0, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx0, TXn_INTERFACE_SELECT, 0x3d);
	dp_pll_write(dp_ln_tx0, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx0, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx0, TXn_TRAN_DRVR_EMP_EN, 0x03);
	dp_pll_write(dp_ln_tx0, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx0, TXn_TX_INTERFACE_MODE, 0x00);
	dp_pll_write(dp_ln_tx0, TXn_TX_EMP_POST1_LVL, 0x2b);
	dp_pll_write(dp_ln_tx0, TXn_TX_DRV_LVL, 0x2f);
	dp_pll_write(dp_ln_tx0, TXn_TX_BAND, 0x4);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_TX, 0x12);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_RX, 0x12);

	/* TX-1 register configuration */
	dp_pll_write(dp_ln_tx1, TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	dp_pll_write(dp_ln_tx1, TXn_VMODE_CTRL1, 0x40);
	dp_pll_write(dp_ln_tx1, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx1, TXn_INTERFACE_SELECT, 0x3d);
	dp_pll_write(dp_ln_tx1, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx1, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx1, TXn_TRAN_DRVR_EMP_EN, 0x03);
	dp_pll_write(dp_ln_tx1, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_TX_INTERFACE_MODE, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_TX_EMP_POST1_LVL, 0x2b);
	dp_pll_write(dp_ln_tx1, TXn_TX_DRV_LVL, 0x2f);
	dp_pll_write(dp_ln_tx1, TXn_TX_BAND, 0x4);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_TX, 0x12);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_RX, 0x12);
	wmb(); /* make sure write happens */

	set_vco_div(pll, rate);

	dp_pll_write(dp_pll,
		QSERDES_COM_CMN_CONFIG, 0x02);
	wmb(); /* make sure write happens */

	return res;
}

enum dp_14nm_pll_status {
	C_READY,
	FREQ_DONE,
	PLL_LOCKED,
	PHY_READY,
	TSYNC_DONE,
};

char *dp_14nm_pll_get_status_name(enum dp_14nm_pll_status status)
{
	switch (status) {
	case C_READY:
		return "C_READY";
	case FREQ_DONE:
		return "FREQ_DONE";
	case PLL_LOCKED:
		return "PLL_LOCKED";
	case PHY_READY:
		return "PHY_READY";
	case TSYNC_DONE:
		return "TSYNC_DONE";
	default:
		return "unknown";
	}
}

static bool dp_14nm_pll_get_status(struct dp_pll *pll,
		enum dp_14nm_pll_status status)
{
	u32 reg, state, bit;
	void __iomem *base;
	bool success = true;

	switch (status) {
	case PLL_LOCKED:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_C_READY_STATUS;
		bit = DP_14NM_PLL_LOCKED;
		break;
	case PHY_READY:
		base = dp_pll_get_base(dp_phy);
		reg = DP_PHY_STATUS;
		bit = DP_14NM_PHY_READY;
		break;
	default:
		return false;
	}

	if (readl_poll_timeout_atomic((base + reg), state,
			((state & bit) > 0),
			DP_PHY_PLL_POLL_SLEEP_US,
			DP_PHY_PLL_POLL_TIMEOUT_US)) {
		DP_ERR("%s failed, status=%x\n",
			dp_14nm_pll_get_status_name(status), state);

		success = false;
	}

	return success;
}

static int dp_pll_enable_14nm(struct dp_pll *pll)
{
	int rc = 0;

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x05);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x09);
	wmb(); /* Make sure the PHY register writes are done */

	dp_pll_write(dp_pll,
		QSERDES_COM_RESETSM_CNTRL, 0x20);
	wmb();	/* Make sure the PLL register writes are done */

	udelay(900); /* hw recommended delay for full PU */

	if (!dp_14nm_pll_get_status(pll, PLL_LOCKED)) {
		rc = -EINVAL;
		goto lock_err;
	}

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x19);
	wmb();	/* Make sure the PHY register writes are done */

	udelay(10); /* hw recommended delay */

	if (!dp_14nm_pll_get_status(pll, PHY_READY)) {
		rc = -EINVAL;
		DP_DEBUG("PLL is locked failed...\n");
		goto lock_err;
	}

	DP_DEBUG("PLL is locked\n");

	dp_pll_write(dp_ln_tx0, TXn_TRANSCEIVER_BIAS_EN, 0x3f);
	dp_pll_write(dp_ln_tx0, TXn_HIGHZ_DRVR_EN, 0x10);
	dp_pll_write(dp_ln_tx1, TXn_TRANSCEIVER_BIAS_EN, 0x3f);
	dp_pll_write(dp_ln_tx1, TXn_HIGHZ_DRVR_EN, 0x10);

	dp_pll_write(dp_ln_tx0, TXn_TX_POL_INV, 0x0a);
	dp_pll_write(dp_ln_tx1, TXn_TX_POL_INV, 0x0a);

	/*
	 * Switch DP Mainlink clock (cc_dpphy_link_clk) from DP
	 * controller side with final frequency
	 */
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x18);
	wmb();	/* Make sure the PHY register writes are done */
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x19);
	wmb();	/* Make sure the PHY register writes are done */

lock_err:
	return rc;
}

static void dp_pll_disable_14nm(struct dp_pll *pll)
{
	/* Assert DP PHY power down */
	dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x2);

	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();
}

static int dp_vco_set_rate_14nm(struct dp_pll *pll, unsigned long rate)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	DP_DEBUG("DP lane CLK rate=%ld\n", rate);

	rc = dp_config_vco_rate_14nm(pll, rate);
	if (rc < 0) {
		DP_ERR("Failed to set clk rate\n");
		return rc;
	}

	return rc;
}

static int dp_pll_configure(struct dp_pll *pll, unsigned long rate)
{
	int rc = 0;

	if (!pll || !rate) {
		DP_ERR("invalid input parameters rate = %lu\n", rate);
		return -EINVAL;
	}

	rate = rate * 10;

	if (rate <= DP_VCO_HSCLK_RATE_1620MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000;
	else if (rate <= DP_VCO_HSCLK_RATE_2700MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	else if (rate <= DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000;
	else
		rate = DP_VCO_HSCLK_RATE_8100MHZDIV1000;

	pll->vco_rate = rate;
	DP_DEBUG("pll->vco_rate: %llu", pll->vco_rate);

	rc = dp_vco_set_rate_14nm(pll, rate);
	if (rc < 0) {
		DP_ERR("pll rate %s set failed\n", rate);
		pll->vco_rate = 0;
		return rc;
	}

	DP_DEBUG("pll rate %lu set success\n", rate);
	return rc;
}

static int dp_pll_prepare(struct dp_pll *pll)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	rc = dp_pll_enable_14nm(pll);
	if (rc < 0)
		DP_ERR("ndx=%d failed to enable dp pll\n", pll->index);

	return rc;
}

static int dp_pll_unprepare(struct dp_pll *pll)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameter\n");
		return -EINVAL;
	}

	dp_pll_disable_14nm(pll);
	pll->vco_rate = 0;

	return rc;
}

static unsigned long dp_pll_link_clk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;
	unsigned long rate = 0;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	rate = pll->vco_rate / 10;

	return rate;
}

static long dp_pll_link_clk_round(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	rate = pll->vco_rate / 10;

	return rate;
}

static unsigned long dp_pll_vco_div_clk_get_rate(struct dp_pll *pll)
{
	if (pll->vco_rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (pll->vco_rate / 4);
	else
		return (pll->vco_rate / 2);
}

static unsigned long dp_pll_vco_div_clk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	return dp_pll_vco_div_clk_get_rate(pll);
}

static long dp_pll_vco_div_clk_round(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	return dp_pll_vco_div_clk_recalc_rate(hw, *parent_rate);
}

static const struct clk_ops pll_link_clk_ops = {
	.recalc_rate = dp_pll_link_clk_recalc_rate,
	.round_rate = dp_pll_link_clk_round,
};

static const struct clk_ops pll_vco_div_clk_ops = {
	.recalc_rate = dp_pll_vco_div_clk_recalc_rate,
	.round_rate = dp_pll_vco_div_clk_round,
};

static struct dp_pll_vco_clk dp_phy_pll_clks[DP_PLL_NUM_CLKS] = {
	{
	.hw.init = &(struct clk_init_data) {
		.name = "dp_phy_pll_link_clk",
		.ops = &pll_link_clk_ops,
		},
	},
	{
	.hw.init = &(struct clk_init_data) {
		.name = "dp_phy_pll_vco_div_clk",
		.ops = &pll_vco_div_clk_ops,
		},
	},
};

static struct dp_pll_db dp_pdb;

int dp_pll_clock_register_14nm(struct dp_pll *pll)
{
	int rc = 0;
	struct platform_device *pdev;
	struct dp_pll_vco_clk *pll_clks;

	if (!pll) {
		DP_ERR("pll data not initialized\n");
		return -EINVAL;
	}
	pdev = pll->pdev;

	pll->clk_data = kzalloc(sizeof(*pll->clk_data), GFP_KERNEL);
	if (!pll->clk_data)
		return -ENOMEM;

	pll->clk_data->clks = kcalloc(DP_PLL_NUM_CLKS, sizeof(struct clk *),
			GFP_KERNEL);
	if (!pll->clk_data->clks) {
		kfree(pll->clk_data);
		return -ENOMEM;
	}

	pll->clk_data->clk_num = DP_PLL_NUM_CLKS;
	pll->priv = &dp_pdb;
	dp_pdb.pll = pll;

	dp_pdb.pll_params = pll_params;

	pll->pll_cfg = dp_pll_configure;
	pll->pll_prepare = dp_pll_prepare;
	pll->pll_unprepare = dp_pll_unprepare;

	pll_clks = dp_phy_pll_clks;

	rc = dp_pll_clock_register_helper(pll, pll_clks, DP_PLL_NUM_CLKS);
	if (rc) {
		DP_ERR("Clock register failed rc=%d\n", rc);
		goto clk_reg_fail;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, pll->clk_data);
	if (rc) {
		DP_ERR("Clock add provider failed rc=%d\n", rc);
		goto clk_reg_fail;
	}

	DP_DEBUG("success\n");
	return rc;

clk_reg_fail:
	dp_pll_clock_unregister_14nm(pll);
	return rc;
}

void dp_pll_clock_unregister_14nm(struct dp_pll *pll)
{
	kfree(pll->clk_data->clks);
	kfree(pll->clk_data);
}
