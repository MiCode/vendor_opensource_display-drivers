// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "dsi_pll_14nm.h"

#define VCO_DELAY_USEC		1

#define MHZ_250         250000000UL
#define MHZ_500         500000000UL
#define MHZ_1000        1000000000UL
#define MHZ_1100        1100000000UL
#define MHZ_1900        1900000000UL
#define MHZ_3000        3000000000UL

#define DSI_PLL_POLL_MAX_READS                  15
#define DSI_PLL_POLL_TIMEOUT_US                 1000
#define MSM8996_DSI_PLL_REVISION_2		2

#define VCO_REF_CLK_RATE 19200000

#define CEIL(x, y)              (((x) + ((y)-1)) / (y))


struct dsi_pll_input {
	u32 fref;	/* 19.2 Mhz, reference clk */
	u32 fdata;	/* bit clock rate */
	u32 dsiclk_sel; /* 1, reg: 0x0014 */
	u32 n2div;	/* 1, reg: 0x0010, bit 4-7 */
	u32 ssc_en;	/* 1, reg: 0x0494, bit 0 */
	u32 ldo_en;	/* 0,  reg: 0x004c, bit 0 */

	/* fixed  */
	u32 refclk_dbler_en;	/* 0, reg: 0x04c0, bit 1 */
	u32 vco_measure_time;	/* 5, unknown */
	u32 kvco_measure_time;	/* 5, unknown */
	u32 bandgap_timer;	/* 4, reg: 0x0430, bit 3 - 5 */
	u32 pll_wakeup_timer;	/* 5, reg: 0x043c, bit 0 - 2 */
	u32 plllock_cnt;	/* 1, reg: 0x0488, bit 1 - 2 */
	u32 plllock_rng;	/* 1, reg: 0x0488, bit 3 - 4 */
	u32 ssc_center;		/* 0, reg: 0x0494, bit 1 */
	u32 ssc_adj_period;	/* 37, reg: 0x498, bit 0 - 9 */
	u32 ssc_spread;		/* 0.005  */
	u32 ssc_freq;		/* unknown */
	u32 pll_ie_trim;	/* 4, reg: 0x0400 */
	u32 pll_ip_trim;	/* 4, reg: 0x0404 */
	u32 pll_iptat_trim;	/* reg: 0x0410 */
	u32 pll_cpcset_cur;	/* 1, reg: 0x04f0, bit 0 - 2 */
	u32 pll_cpmset_cur;	/* 1, reg: 0x04f0, bit 3 - 5 */

	u32 pll_icpmset;	/* 4, reg: 0x04fc, bit 3 - 5 */
	u32 pll_icpcset;	/* 4, reg: 0x04fc, bit 0 - 2 */

	u32 pll_icpmset_p;	/* 0, reg: 0x04f4, bit 0 - 2 */
	u32 pll_icpmset_m;	/* 0, reg: 0x04f4, bit 3 - 5 */

	u32 pll_icpcset_p;	/* 0, reg: 0x04f8, bit 0 - 2 */
	u32 pll_icpcset_m;	/* 0, reg: 0x04f8, bit 3 - 5 */

	u32 pll_lpf_res1;	/* 3, reg: 0x0504, bit 0 - 3 */
	u32 pll_lpf_cap1;	/* 11, reg: 0x0500, bit 0 - 3 */
	u32 pll_lpf_cap2;	/* 1, reg: 0x0500, bit 4 - 7 */
	u32 pll_c3ctrl;		/* 2, reg: 0x04c4 */
	u32 pll_r3ctrl;		/* 1, reg: 0x04c4 */
};

struct dsi_pll_output {
	u32 pll_txclk_en;	/* reg: 0x04c0 */
	u32 dec_start;		/* reg: 0x0490 */
	u32 div_frac_start;	/* reg: 0x04b4, 0x4b8, 0x04bc */
	u32 ssc_period;		/* reg: 0x04a0, 0x04a4 */
	u32 ssc_step_size;	/* reg: 0x04a8, 0x04ac */
	u32 plllock_cmp;	/* reg: 0x047c, 0x0480, 0x0484 */
	u32 pll_vco_div_ref;	/* reg: 0x046c, 0x0470 */
	u32 pll_vco_count;	/* reg: 0x0474, 0x0478 */
	u32 pll_kvco_div_ref;	/* reg: 0x0440, 0x0444 */
	u32 pll_kvco_count;	/* reg: 0x0448, 0x044c */
	u32 pll_misc1;		/* reg: 0x04e8 */
	u32 pll_lpf2_postdiv;	/* reg: 0x0504 */
	u32 pll_resetsm_cntrl;	/* reg: 0x042c */
	u32 pll_resetsm_cntrl2;	/* reg: 0x0430 */
	u32 pll_resetsm_cntrl5;	/* reg: 0x043c */
	u32 pll_kvco_code;		/* reg: 0x0458 */

	u32 cmn_clk_cfg0;	/* reg: 0x0010 */
	u32 cmn_clk_cfg1;	/* reg: 0x0014 */
	u32 cmn_ldo_cntrl;	/* reg: 0x004c */

	u32 pll_postdiv;	/* vco */
	u32 pll_n1div;		/* vco */
	u32 pll_n2div;		/* hr_oclk3, pixel */
	u32 fcvo;
};

struct dsi_pll_14nm {
	struct dsi_pll_resource *rsc;
	struct dsi_pll_input in;
	struct dsi_pll_output out;
	int source_setup_done;
};

enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_MAX
};

enum {
	PLL_OUTPUT_NONE,
	PLL_OUTPUT_RIGHT,
	PLL_OUTPUT_LEFT,
	PLL_OUTPUT_BOTH
};

enum {
	PLL_SOURCE_FROM_LEFT,
	PLL_SOURCE_FROM_RIGHT
};

enum {
	PLL_UNKNOWN,
	PLL_STANDALONE,
	PLL_SLAVE,
	PLL_MASTER
};

static inline bool dsi_pll_14nm_is_hw_revision(
		struct dsi_pll_resource *rsc)
{
	return (rsc->pll_revision == DSI_PLL_14NM) ?
		true : false;
}

static inline void dsi_pll_set_pll_post_div(struct dsi_pll_resource *pll,
		u32 pll_post_div)
{
	u32 n1div = 0;

	n1div = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG0);
	n1div &= ~0xf;
	n1div |= (pll_post_div & 0xf);

	DSI_PLL_REG_W(pll->phy_base, DSIPHY_CMN_CLK_CFG0, n1div);

	/* ensure n1 divider is programed */
	wmb();
}

static inline int dsi_pll_get_pll_post_div(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG0);

	return (reg_val & 0xf);
}

static inline void dsi_pll_set_dsi_clk(struct dsi_pll_resource *pll,
		u32 dsiclk_sel)
{
	u32 reg_val = 0;

	reg_val = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG1);
	reg_val |= dsiclk_sel;

	DSI_PLL_REG_W(pll->phy_base, DSIPHY_CMN_CLK_CFG1, reg_val);

	/* ensure dsi_clk src is programed */
	wmb();
}

static inline int dsi_pll_get_dsi_clk(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG1);

	return (reg_val & 0x1);
}

static inline void dsi_pll_set_pclk_div(struct dsi_pll_resource *pll,
		u32 pclk_div)
{
	u32 n2div = 0;

	n2div = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG0);
	n2div &= ~0xf0; /* bits 4 to 7 */
	n2div |= (pclk_div << 4);

	DSI_PLL_REG_W(pll->phy_base, DSIPHY_CMN_CLK_CFG0, n2div);

	/* ensure pclk (n2div) divider is programed */
	wmb();
}


static inline int dsi_pll_get_pclk_div(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, DSIPHY_CMN_CLK_CFG0);

	return ((reg_val & 0xF0) >> 4);
}

static struct dsi_pll_resource *pll_rsc_db[DSI_PLL_MAX];
static struct dsi_pll_14nm plls[DSI_PLL_MAX];

static void dsi_pll_setup_config(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{

	pll->in.fref = 19200000;        /* 19.2 Mhz*/
	pll->in.fdata = 0;              /* bit clock rate */
	pll->in.dsiclk_sel = 1;         /* 1, reg: 0x0014 */
	pll->in.ssc_en = rsc->ssc_en;           /* 1, reg: 0x0494, bit 0 */
	pll->in.ldo_en = 0;             /* 0,  reg: 0x004c, bit 0 */

	/* fixed  input */
	pll->in.refclk_dbler_en = 0;    /* 0, reg: 0x04c0, bit 1 */
	pll->in.vco_measure_time = 5;   /* 5, unknown */
	pll->in.kvco_measure_time = 5;  /* 5, unknown */
	pll->in.bandgap_timer = 4;      /* 4, reg: 0x0430, bit 3 - 5 */
	pll->in.pll_wakeup_timer = 5;   /* 5, reg: 0x043c, bit 0 - 2 */
	pll->in.plllock_cnt = 1;        /* 1, reg: 0x0488, bit 1 - 2 */
	pll->in.plllock_rng = 0;        /* 0, reg: 0x0488, bit 3 - 4 */
	pll->in.ssc_center = rsc->ssc_center;/* 0, reg: 0x0494, bit 1 */
	pll->in.ssc_adj_period = 37;    /* 37, reg: 0x498, bit 0 - 9 */
	pll->in.ssc_spread = rsc->ssc_ppm / 1000;
	pll->in.ssc_freq = rsc->ssc_freq;

	pll->in.pll_ie_trim = 4;        /* 4, reg: 0x0400 */
	pll->in.pll_ip_trim = 4;        /* 4, reg: 0x0404 */
	pll->in.pll_cpcset_cur = 1;     /* 1, reg: 0x04f0, bit 0 - 2 */
	pll->in.pll_cpmset_cur = 1;     /* 1, reg: 0x04f0, bit 3 - 5 */
	pll->in.pll_icpmset = 7;        /* 4, reg: 0x04fc, bit 3 - 5 */
	pll->in.pll_icpcset = 7;        /* 4, reg: 0x04fc, bit 0 - 2 */
	pll->in.pll_icpmset_p = 0;      /* 0, reg: 0x04f4, bit 0 - 2 */
	pll->in.pll_icpmset_m = 0;      /* 0, reg: 0x04f4, bit 3 - 5 */
	pll->in.pll_icpcset_p = 0;      /* 0, reg: 0x04f8, bit 0 - 2 */
	pll->in.pll_icpcset_m = 0;      /* 0, reg: 0x04f8, bit 3 - 5 */
	pll->in.pll_lpf_res1 = 3;       /* 3, reg: 0x0504, bit 0 - 3 */
	pll->in.pll_lpf_cap1 = 11;      /* 11, reg: 0x0500, bit 0 - 3 */
	pll->in.pll_lpf_cap2 = 1;       /* 1, reg: 0x0500, bit 4 - 7 */
	pll->in.pll_iptat_trim = 7;
	pll->in.pll_c3ctrl = 2;         /* 2 */
	pll->in.pll_r3ctrl = 1;         /* 1 */
	pll->out.pll_postdiv = 1;
}


static void dsi_pll_calc_dec_frac(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{
	struct dsi_pll_input *pin = &pll->in;
	struct dsi_pll_output *pout = &pll->out;
	u64 multiplier = BIT(20);
	u64 dec_start_multiple, dec_start, pll_comp_val;
	s32 duration, div_frac_start;
	s64 vco_clk_rate = rsc->vco_current_rate;
	s64 fref = rsc->vco_ref_clk_rate;

	DSI_PLL_DBG(rsc, "vco_clk_rate=%lld ref_clk_rate=%lld\n",
			vco_clk_rate, fref);

	dec_start_multiple = div_s64(vco_clk_rate * multiplier, fref);
	div_s64_rem(dec_start_multiple, multiplier, &div_frac_start);

	dec_start = div_s64(dec_start_multiple, multiplier);

	pout->dec_start = (u32)dec_start;
	pout->div_frac_start = div_frac_start;

	if (pin->plllock_cnt == 0)
		duration = 1024;
	else if (pin->plllock_cnt == 1)
		duration = 256;
	else if (pin->plllock_cnt == 2)
		duration = 128;
	else
		duration = 32;

	pll_comp_val =  duration * dec_start_multiple;
	pll_comp_val =  div_u64(pll_comp_val, multiplier);
	do_div(pll_comp_val, 10);

	pout->plllock_cmp = (u32)pll_comp_val;

	pout->pll_txclk_en = 1;
}

static void dsi_pll_calc_ssc(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{
	u32 period, ssc_period;
	u32 ref, rem;
	s64 step_size;

	DSI_PLL_DBG(rsc, "vco=%lld ref=%lld\n",
			rsc->vco_current_rate, rsc->vco_ref_clk_rate);

	ssc_period = pll->in.ssc_freq / 500;
	period = (unsigned long)rsc->vco_ref_clk_rate / 1000;
	ssc_period  = CEIL(period, ssc_period);
	ssc_period -= 1;
	pll->out.ssc_period = ssc_period;

	DSI_PLL_DBG(rsc, "ssc, freq=%d spread=%d period=%d\n",
			pll->in.ssc_freq, pll->in.ssc_spread, pll->out.ssc_period);

	step_size = (u32)rsc->vco_current_rate;
	ref = rsc->vco_ref_clk_rate;
	ref /= 1000;
	step_size = div_s64(step_size, ref);
	step_size <<= 20;
	step_size = div_s64(step_size, 1000);
	step_size *= pll->in.ssc_spread;
	step_size = div_s64(step_size, 1000);
	step_size *= (pll->in.ssc_adj_period + 1);

	rem = 0;
	step_size = div_s64_rem(step_size, ssc_period + 1, &rem);

	if (rem)
		step_size++;

	DSI_PLL_DBG(rsc, "step_size=%lld\n", step_size);

	step_size &= 0x0ffff;   /* take lower 16 bits */

	pll->out.ssc_step_size = step_size;
}

static void dsi_pll_ssc_commit(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_input *pin = &pll->in;
	struct dsi_pll_output *pout = &pll->out;
	char data;

	data = pin->ssc_adj_period;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJ_PER1, data);
	data = (pin->ssc_adj_period >> 8);
	data &= 0x03;
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJ_PER2, data);

	data = pout->ssc_period;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_SSC_PER1, data);
	data = (pout->ssc_period >> 8);
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_SSC_PER2, data);

	data = pout->ssc_step_size;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEP_SIZE1, data);
	data = (pout->ssc_step_size >> 8);
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEP_SIZE2, data);

	data = (pin->ssc_center & 0x01);
	data <<= 1;
	data |= 0x01; /* enable */
	DSI_PLL_REG_W(pll_base, PLL_SSC_EN_CENTER, data);

	wmb();  /* make sure register committed */
}


static bool dsi_pll_14nm_lock_status(struct dsi_pll_resource *rsc)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((rsc->pll_base +
					PLL_RESET_SM_READY_STATUS),
				status,
				((status & BIT(5)) > 0),
				DSI_PLL_POLL_MAX_READS,
				DSI_PLL_POLL_TIMEOUT_US)) {
		DSI_PLL_DBG(rsc, "status=%x failed to Lock\n",
				status);
		pll_locked = false;
	} else if (readl_poll_timeout_atomic((rsc->pll_base +
					PLL_RESET_SM_READY_STATUS),
				status,
				((status & BIT(0)) > 0),
				DSI_PLL_POLL_MAX_READS,
				DSI_PLL_POLL_TIMEOUT_US)) {
		DSI_PLL_DBG(rsc, "status=%x PLL not ready\n",
				status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

static void dsi_pll_start_14nm(struct dsi_pll_resource *rsc)
{
	DSI_PLL_REG_W(rsc->pll_base, PLL_VREF_CFG1, 0x10);
	DSI_PLL_REG_W(rsc->phy_base, DSIPHY_CMN_PLL_CNTRL, 1);
}

static void dsi_pll_stop_14nm(struct dsi_pll_resource *rsc)
{
	DSI_PLL_REG_W(rsc->phy_base, DSIPHY_CMN_PLL_CNTRL, 0);
}

static void dsi_pll_unprepare_stub(struct clk_hw *hw) { }

static int dsi_pll_prepare_stub(struct clk_hw *hw)
{
	return 0;
}

static int dsi_pll_set_rate_stub(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	return 0;
}


static long dsi_pll_byteclk_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct dsi_pll_clk *pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll_res = pll->priv;

	return pll_res->byteclk_rate;
}


static long dsi_pll_pclk_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct dsi_pll_clk *pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll_res = pll->priv;

	return pll_res->pclk_rate;
}

static unsigned long dsi_pll_byteclk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dsi_pll_clk *byte_pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll = NULL;

	pll = byte_pll->priv;

	return pll->byteclk_rate;
}

static unsigned long dsi_pll_pclk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dsi_pll_clk *pix_pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll = NULL;

	pll = pix_pll->priv;

	return pll->pclk_rate;
}

static const struct clk_ops pll_byteclk_ops = {
	.recalc_rate = dsi_pll_byteclk_recalc_rate,
	.set_rate = dsi_pll_set_rate_stub,
	.round_rate = dsi_pll_byteclk_round_rate,
	.prepare = dsi_pll_prepare_stub,
	.unprepare = dsi_pll_unprepare_stub,
};

static const struct clk_ops pll_pclk_ops = {
	.recalc_rate = dsi_pll_pclk_recalc_rate,
	.set_rate = dsi_pll_set_rate_stub,
	.round_rate = dsi_pll_pclk_round_rate,
	.prepare = dsi_pll_prepare_stub,
	.unprepare = dsi_pll_unprepare_stub,
};

/*
 * Clock tree for generating DSI byte and pclk.
 *
 *
 *  +-------------------------------+		+----------------------------+
 *  |    dsi_phy_pll_out_byteclk    |		|    dsi_phy_pll_out_dsiclk  |
 *  +---------------+---------------+		+--------------+-------------+
 *                  |                                          |
 *                  |                                          |
 *                  v                                          v
 *            dsi_byte_clk                                  dsi_pclk
 *
 *
 */

static struct dsi_pll_clk dsi0_phy_pll_out_byteclk = {
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_pll_out_byteclk",
		.ops = &pll_byteclk_ops,
	},
};


static struct dsi_pll_clk dsi0_phy_pll_out_dsiclk = {
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_pll_out_dsiclk",
		.ops = &pll_pclk_ops,
	},
};

static struct dsi_pll_clk dsi1_phy_pll_out_byteclk = {
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_pll_out_byteclk",
		.ops = &pll_byteclk_ops,
	},
};

static struct dsi_pll_clk dsi1_phy_pll_out_dsiclk = {
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_pll_out_dsiclk",
		.ops = &pll_pclk_ops,
	},
};

int dsi_pll_clock_register_14nm(struct platform_device *pdev,
		struct dsi_pll_resource *pll_res)
{
	int rc = 0, ndx;
	struct clk *clk;
	struct clk_onecell_data *clk_data;
	int num_clks = 4;
	int const ssc_freq_default = 31500; /* default h/w recommended value */
	int const ssc_ppm_default = 5000; /* default h/w recommended value */

	if (!pdev || !pdev->dev.of_node ||
			!pll_res || !pll_res->pll_base || !pll_res->phy_base) {
		DSI_PLL_ERR(pll_res, "Invalid params\n");
		return -EINVAL;
	}

	ndx = pll_res->index;

	if (ndx >= DSI_PLL_MAX) {
		DSI_PLL_ERR(pll_res, "not supported\n");
		return -EINVAL;
	}


	pll_rsc_db[ndx] = pll_res;
	plls[ndx].rsc = pll_res;
	pll_res->priv = &plls[ndx];
	pll_res->vco_delay = VCO_DELAY_USEC;
	pll_res->vco_min_rate = 600000000;
	pll_res->vco_ref_clk_rate = 19200000UL;

	if (pll_res->ssc_en) {
		if (!pll_res->ssc_freq)
			pll_res->ssc_freq = ssc_freq_default;
		if (!pll_res->ssc_ppm)
			pll_res->ssc_ppm = ssc_ppm_default;
	}

	dsi_pll_setup_config(pll_res->priv, pll_res);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
			GFP_KERNEL);

	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);

	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	/* Establish client data */
	if (ndx == 0) {
		dsi0_phy_pll_out_byteclk.priv = pll_res;
		dsi0_phy_pll_out_dsiclk.priv = pll_res;

		clk = devm_clk_register(&pdev->dev,
				&dsi0_phy_pll_out_byteclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
					"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}

		clk_data->clks[0] = clk;

		clk = devm_clk_register(&pdev->dev,
				&dsi0_phy_pll_out_dsiclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
					"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[1] = clk;

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	} else {
		dsi1_phy_pll_out_byteclk.priv = pll_res;
		dsi1_phy_pll_out_dsiclk.priv = pll_res;

		clk = devm_clk_register(&pdev->dev,
				&dsi1_phy_pll_out_byteclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[2] = clk;

		clk = devm_clk_register(&pdev->dev,
				&dsi1_phy_pll_out_dsiclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[3] = clk;

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	}

	if (!rc) {
		DSI_PLL_INFO(pll_res, "Registered clocks successfully\n");

		return rc;
	}

clk_register_fail:
	return rc;

}

/*
 * pll_source_finding:
 * Both GLBL_TEST_CTRL and CLKBUFLR_EN are configured
 * at mdss_dsi_14nm_phy_config()
 */
static int pll_source_finding(struct dsi_pll_resource *rsc)
{
	u32 clk_buf_en;
	u32 glbl_test_ctrl;

	glbl_test_ctrl = DSI_PLL_REG_R(rsc->phy_base,
			DSIPHY_CMN_GLBL_TEST_CTRL);

	clk_buf_en = DSI_PLL_REG_R(rsc->pll_base,
			PLL_CLKBUFLR_EN);

	glbl_test_ctrl &= BIT(2);
	glbl_test_ctrl >>= 2;

	clk_buf_en &= (PLL_OUTPUT_RIGHT | PLL_OUTPUT_LEFT);

	if ((glbl_test_ctrl == PLL_SOURCE_FROM_LEFT) &&
			(clk_buf_en == PLL_OUTPUT_BOTH))
		return PLL_MASTER;

	if ((glbl_test_ctrl == PLL_SOURCE_FROM_RIGHT) &&
			(clk_buf_en == PLL_OUTPUT_NONE))
		return PLL_SLAVE;

	if ((glbl_test_ctrl == PLL_SOURCE_FROM_LEFT) &&
			(clk_buf_en == PLL_OUTPUT_RIGHT))
		return PLL_STANDALONE;

	DSI_PLL_ERR(rsc, "Error pll source finding, clk_buf_en=%x glbl_test_ctrl=%x\n",
			clk_buf_en, glbl_test_ctrl);

	return PLL_UNKNOWN;
}


static void pll_source_setup(struct dsi_pll_resource *rsc)
{
	int status;
	struct dsi_pll_14nm *pll = (struct dsi_pll_14nm *)rsc->priv;
	struct dsi_pll_resource *orsc = pll_rsc_db[DSI_PLL_1];

	if (pll->source_setup_done)
		return;

	pll->source_setup_done++;

	status = pll_source_finding(rsc);

	if (status == PLL_STANDALONE || status == PLL_UNKNOWN)
		return;

	if (status == PLL_MASTER)
		rsc->slave = orsc;

	DSI_PLL_DBG(rsc, "Slave PLL %s\n",
			rsc->slave ? "configured" : "absent");
}

static int dsi_pll_14nm_set_byteclk_div(struct dsi_pll_resource *pll,
		bool commit)
{
	int i = 0;
	int table_size;
	u32 pll_post_div = 0, phy_post_div = 0;
	struct dsi_pll_div_table *table;
	u64 bitclk_rate;
	struct dsi_pll_14nm *pll_priv = pll->priv;
	struct dsi_pll_output *pout = &pll_priv->out;

	bitclk_rate = pll->byteclk_rate * 8;

	table = pll_14nm_dphy;
	table_size = ARRAY_SIZE(pll_14nm_dphy);

	for (i = 0; i < table_size; i++) {
		if ((table[i].min_hz <= bitclk_rate) &&
				(bitclk_rate <= table[i].max_hz)) {
			pll_post_div = table[i].pll_div;
			/*
			 * For 14nm we need only pll_post_div,
			 * So phy_post_div will be always 0.
			 */
			phy_post_div = table[i].phy_div;

			DSI_PLL_DBG(pll, "pll_post_div:%u  phy_post_div:%u",
					pll_post_div, phy_post_div);
			break;
		}
	}

	if (commit)
		dsi_pll_set_pll_post_div(pll, pll_post_div);

	pout->pll_n1div = pll_post_div;
	pll->vco_rate = bitclk_rate * pll_post_div;
	DSI_PLL_DBG(pll, "pll->vco_rate:%lld pout->pll_n1div:0x%x",
			pll->vco_rate, pout->pll_n1div);

	return 0;
}


static int dsi_pll_calc_dphy_pclk_div(struct dsi_pll_resource *pll)
{
	u32 m_val, n_val; /* M and N values of MND trio */
	u32 pclk_div;

	DSI_PLL_DBG(pll, "pll->bpp:%d pll->lanes:%d",
			pll->bpp, pll->lanes);

	if (pll->bpp == 18 && pll->lanes == 1) {
		/* RGB666_packed */
		m_val = 2;
		n_val = 9;
	} else if (pll->bpp == 18 && pll->lanes == 2) {
		/* RGB666_packed */
		m_val = 2;
		n_val = 9;
	} else if (pll->bpp == 18 && pll->lanes == 4) {
		/* RGB666_packed */
		m_val = 4;
		n_val = 9;
	} else if (pll->bpp == 16 && pll->lanes == 3) {
		/* RGB565 */
		m_val = 3;
		n_val = 8;
	} else {
		m_val = 1;
		n_val = 1;
	}

	/* Calculating pclk_div assuming dsiclk_sel to be 1 */
	pclk_div = pll->bpp;
	pclk_div = mult_frac(pclk_div, m_val, n_val);

	do_div(pclk_div, 2);
	do_div(pclk_div, pll->lanes);

	DSI_PLL_DBG(pll, "pclk_div:%u", pclk_div);

	return pclk_div;
}

static int dsi_pll_14nm_set_pclk_div(struct dsi_pll_resource *pll, bool commit)
{
	int dsiclk_sel = 0, pclk_div = 0;
	u64 pclk_src_rate;
	u32 pll_post_div;
	struct dsi_pll_14nm *pll_priv = pll->priv;
	struct dsi_pll_output *pout = &pll_priv->out;

	pll_post_div = dsi_pll_get_pll_post_div(pll);

	pclk_src_rate = div_u64(pll->vco_rate, pll_post_div);

	/* set dsiclk_sel=1 so that n2div *= 2 */
	dsiclk_sel = 0x1;

	pclk_src_rate = div_u64(pclk_src_rate, 2);

	pclk_div = dsi_pll_calc_dphy_pclk_div(pll);
	pout->pll_n2div = pclk_div;

	pll->pclk_rate = div_u64(pclk_src_rate, pclk_div);

	DSI_PLL_DBG(pll, "pclk rate: %llu, dsi_clk: %d, pclk_div: %d pout->pll_n2div:0x%x\n",
			pll->pclk_rate, dsiclk_sel, pclk_div, pout->pll_n2div);

	if (commit) {
		dsi_pll_set_dsi_clk(pll, dsiclk_sel);
		dsi_pll_set_pclk_div(pll, pclk_div);
	}

	return 0;
}

static u32 pll_14nm_kvco_slop(u32 vrate)
{
	u32 slop = 0;

	if (vrate > 1300000000UL && vrate <= 1800000000UL)
		slop =  600;
	else if (vrate > 1800000000UL && vrate < 2300000000UL)
		slop = 400;
	else if (vrate > 2300000000UL && vrate < 2600000000UL)
		slop = 280;

	return slop;
}

static void pll_14nm_calc_vco_count(struct dsi_pll_14nm *pll,
		s64 vco_clk_rate, s64 fref)
{
	struct dsi_pll_input *pin = &pll->in;
	struct dsi_pll_output *pout = &pll->out;
	u64 data;
	u32 cnt;

	data = fref * pin->vco_measure_time;
	do_div(data, 1000000);
	data &= 0x03ff; /* 10 bits */
	data -= 2;
	pout->pll_vco_div_ref = data;

	data = (unsigned long)vco_clk_rate / 1000000;   /* unit is Mhz */
	data *= pin->vco_measure_time;

	do_div(data, 10);
	pout->pll_vco_count = data; /* reg: 0x0474, 0x0478 */

	data = fref * pin->kvco_measure_time;
	do_div(data, 1000000);
	data &= 0x03ff; /* 10 bits */
	data -= 1;
	pout->pll_kvco_div_ref = data;

	cnt = pll_14nm_kvco_slop(vco_clk_rate);
	cnt *= 2;
	cnt /= 100;
	cnt *= pin->kvco_measure_time;
	pout->pll_kvco_count = cnt;

	pout->pll_misc1 = 16;
	pout->pll_resetsm_cntrl = 48;
	pout->pll_resetsm_cntrl2 = pin->bandgap_timer << 3;
	pout->pll_resetsm_cntrl5 = pin->pll_wakeup_timer;
	pout->pll_kvco_code = 0;
}


static void pll_db_commit_common(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_input *pin = &pll->in;
	struct dsi_pll_output *pout = &pll->out;
	char data;

	/* confgiure the non frequency dependent pll registers */
	data = 0;
	DSI_PLL_REG_W(pll_base, PLL_SYSCLK_EN_RESET, data);

	/* DSIPHY_PLL_CLKBUFLR_EN updated at dsi phy */

	data = pout->pll_txclk_en;
	DSI_PLL_REG_W(pll_base, PLL_TXCLK_EN, data);

	data = pout->pll_resetsm_cntrl;
	DSI_PLL_REG_W(pll_base, PLL_RESETSM_CNTRL, data);
	data = pout->pll_resetsm_cntrl2;
	DSI_PLL_REG_W(pll_base, PLL_RESETSM_CNTRL2, data);
	data = pout->pll_resetsm_cntrl5;
	DSI_PLL_REG_W(pll_base, PLL_RESETSM_CNTRL5, data);

	data = pout->pll_vco_div_ref;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_VCO_DIV_REF1, data);
	data = (pout->pll_vco_div_ref >> 8);
	data &= 0x03;
	DSI_PLL_REG_W(pll_base, PLL_VCO_DIV_REF2, data);

	data = pout->pll_kvco_div_ref;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_KVCO_DIV_REF1, data);
	data = (pout->pll_kvco_div_ref >> 8);
	data &= 0x03;
	DSI_PLL_REG_W(pll_base, PLL_KVCO_DIV_REF2, data);

	data = pout->pll_misc1;
	DSI_PLL_REG_W(pll_base, PLL_PLL_MISC1, data);

	data = pin->pll_ie_trim;
	DSI_PLL_REG_W(pll_base, PLL_IE_TRIM, data);

	data = pin->pll_ip_trim;
	DSI_PLL_REG_W(pll_base, PLL_IP_TRIM, data);

	data = ((pin->pll_cpmset_cur << 3) | pin->pll_cpcset_cur);
	DSI_PLL_REG_W(pll_base, PLL_CP_SET_CUR, data);

	data = ((pin->pll_icpcset_p << 3) | pin->pll_icpcset_m);
	DSI_PLL_REG_W(pll_base, PLL_PLL_ICPCSET, data);

	data = ((pin->pll_icpmset_p << 3) | pin->pll_icpcset_m);
	DSI_PLL_REG_W(pll_base, PLL_PLL_ICPMSET, data);

	data = ((pin->pll_icpmset << 3) | pin->pll_icpcset);
	DSI_PLL_REG_W(pll_base, PLL_PLL_ICP_SET, data);

	data = ((pll->in.pll_lpf_cap2 << 4) | pll->in.pll_lpf_cap1);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LPF1, data);

	data = pin->pll_iptat_trim;
	DSI_PLL_REG_W(pll_base, PLL_IPTAT_TRIM, data);

	data = (pll->in.pll_c3ctrl | (pll->in.pll_r3ctrl << 4));
	DSI_PLL_REG_W(pll_base, PLL_PLL_CRCTRL, data);
}

static void pll_db_commit_14nm(struct dsi_pll_14nm *pll,
		struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	void __iomem *phy_base = rsc->phy_base;
	struct dsi_pll_input *pin = &pll->in;
	struct dsi_pll_output *pout = &pll->out;
	char data;

	data = pout->cmn_ldo_cntrl;
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_LDO_CNTRL, data);

	pll_db_commit_common(pll, rsc);

	/* de assert pll start and apply pll sw reset */
	/* stop pll */
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_PLL_CNTRL, 0);

	/* pll sw reset */
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_CTRL_1, 0x20);
	wmb();  /* make sure register committed */
	udelay(10);

	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_CTRL_1, 0);
	wmb();  /* make sure register committed */

	data = pll->in.dsiclk_sel; /* set dsiclk_sel = 1  */
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_CLK_CFG1, data);

	data = 0xff; /* data, clk, pll normal operation */
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_CTRL_0, data);

	/* confgiure the frequency dependent pll registers */
	data = pout->dec_start;
	DSI_PLL_REG_W(pll_base, PLL_DEC_START, data);

	data = pout->div_frac_start;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_DIV_FRAC_START1, data);
	data = (pout->div_frac_start >> 8);
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_DIV_FRAC_START2, data);
	data = (pout->div_frac_start >> 16);
	data &= 0x0f;
	DSI_PLL_REG_W(pll_base, PLL_DIV_FRAC_START3, data);

	data = pout->plllock_cmp;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_PLLLOCK_CMP1, data);
	data = (pout->plllock_cmp >> 8);
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_PLLLOCK_CMP2, data);
	data = (pout->plllock_cmp >> 16);
	data &= 0x03;
	DSI_PLL_REG_W(pll_base, PLL_PLLLOCK_CMP3, data);

	data = ((pin->plllock_cnt << 1) | (pin->plllock_rng << 3));
	DSI_PLL_REG_W(pll_base, PLL_PLLLOCK_CMP_EN, data);

	data = pout->pll_vco_count;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_VCO_COUNT1, data);
	data = (pout->pll_vco_count >> 8);
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_VCO_COUNT2, data);

	data = pout->pll_kvco_count;
	data &= 0x0ff;
	DSI_PLL_REG_W(pll_base, PLL_KVCO_COUNT1, data);
	data = (pout->pll_kvco_count >> 8);
	data &= 0x03;
	DSI_PLL_REG_W(pll_base, PLL_KVCO_COUNT2, data);

	/*
	 * tx_band = pll_postdiv
	 * 0: divided by 1 <== for now
	 * 1: divided by 2
	 * 2: divided by 4
	 * 3: divided by 8
	 */
	data = (((pout->pll_postdiv - 1) << 4) | pll->in.pll_lpf_res1);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LPF2_POSTDIV, data);

	data = (pout->pll_n1div | (pout->pll_n2div << 4));
	DSI_PLL_REG_W(phy_base, DSIPHY_CMN_CLK_CFG0, data);

	if (rsc->ssc_en)
		dsi_pll_ssc_commit(pll, rsc);

	wmb();  /* make sure register committed */
}

static int dsi_pll_read_stored_trim_codes(struct dsi_pll_resource *pll_res,
						unsigned long vco_clk_rate)
{
	int i;
	bool found = false;

	if (!pll_res->dfps) {
		pr_err("%s: Invalid dfps\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < pll_res->dfps->vco_rate_cnt; i++) {
		struct dfps_codes_info *codes_info =
		&pll_res->dfps->codes_dfps[i];

		DSI_PLL_DBG(pll_res, "valid=%d vco_rate=%d, code %x %x %x\n",
			codes_info->is_valid, codes_info->clk_rate,
			codes_info->pll_codes.pll_codes_1,
			codes_info->pll_codes.pll_codes_2,
			codes_info->pll_codes.pll_codes_3);

		if (vco_clk_rate == codes_info->clk_rate &&
				codes_info->is_valid) {
			pll_res->cache_pll_trim_codes[0] =
				codes_info->pll_codes.pll_codes_1;
			pll_res->cache_pll_trim_codes[1] =
				codes_info->pll_codes.pll_codes_2;
			pll_res->cache_pll_trim_codes[2] =
				codes_info->pll_codes.pll_codes_3;
			found = true;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	return 0;
}


static void pll_dynamic_refresh_14nm(struct dsi_pll_14nm *pll, struct dsi_pll_resource *rsc)
{
	struct dsi_pll_output *pout = &pll->out;
	u32 data = 0;

	data = (pout->pll_n1div | (pout->pll_n2div << 4));
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL19,
		DSIPHY_CMN_CLK_CFG0, DSIPHY_CMN_CLK_CFG1,
		data, 1);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL20,
		DSIPHY_CMN_CTRL_0, DSIPHY_PLL_SYSCLK_EN_RESET,
		0xFF, 0x0);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL21,
		DSIPHY_PLL_DEC_START, DSIPHY_PLL_DIV_FRAC_START1,
		pout->dec_start, (pout->div_frac_start & 0x0FF));
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL22,
		DSIPHY_PLL_DIV_FRAC_START2, DSIPHY_PLL_DIV_FRAC_START3,
		((pout->div_frac_start >> 8) & 0x0FF),
		((pout->div_frac_start >> 16) & 0x0F));
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL23,
		DSIPHY_PLL_PLLLOCK_CMP1, DSIPHY_PLL_PLLLOCK_CMP2,
		(pout->plllock_cmp & 0x0FF),
		((pout->plllock_cmp >> 8) & 0x0FF));
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL24,
		DSIPHY_PLL_PLLLOCK_CMP3, DSIPHY_PLL_PLL_VCO_TUNE,
		((pout->plllock_cmp >> 16) & 0x03),
		(rsc->cache_pll_trim_codes[1] | BIT(7))); /* VCO tune*/
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL25,
		DSIPHY_PLL_KVCO_CODE, DSIPHY_PLL_RESETSM_CNTRL,
		(rsc->cache_pll_trim_codes[0] | BIT(5)), 0x38);
		DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
	DSI_DYNAMIC_REFRESH_PLL_CTRL26,
		DSIPHY_PLL_PLL_LPF2_POSTDIV, DSIPHY_CMN_PLL_CNTRL,
		(((pout->pll_postdiv - 1) << 4) | pll->in.pll_lpf_res1), 0x01);
		DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL27,
	DSIPHY_CMN_PLL_CNTRL, DSIPHY_CMN_PLL_CNTRL,
		0x01, 0x01);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL28,
		DSIPHY_CMN_PLL_CNTRL, DSIPHY_CMN_PLL_CNTRL,
		0x01, 0x01);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_CTRL29,
		DSIPHY_CMN_PLL_CNTRL, DSIPHY_CMN_PLL_CNTRL,
		0x01, 0x01);
	DSI_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR, 0x0000001E);
		DSI_PLL_REG_W(rsc->dyn_pll_base,
	DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2, 0x001FFE00);

	/*
	* Ensure all the dynamic refresh registers are written before
	* dynamic refresh to change the fps is triggered
	*/
	wmb();
}

static int dsi_pll_14nm_dynamic_clk_vco_set_rate(struct dsi_pll_resource *pll_res)
{
	struct dsi_pll_14nm *pll;
	u32 rate;
	int rc = 0;

	pll = pll_res->priv;
	if (!pll) {
		DSI_PLL_ERR(pll_res, "pll configuration not found\n");
		return -EINVAL;
	}
	rate = pll_res->vco_rate;
	rc = dsi_pll_read_stored_trim_codes(pll_res, rate);
	if (rc) {
		DSI_PLL_ERR(pll_res, "cannot find pll codes rate=%ld\n", rate);
		return -EINVAL;
	}

	DSI_PLL_DBG(pll_res, "rate=%lu vco_ref_clk_rate=%lu\n",
			pll_res->vco_rate, pll_res->vco_ref_clk_rate);

	pll_res->vco_current_rate = pll_res->vco_rate;

	dsi_pll_setup_config(pll_res->priv, pll_res);
	dsi_pll_calc_dec_frac(pll, pll_res);

	pll_14nm_calc_vco_count(pll, pll_res->vco_current_rate,
			pll_res->vco_ref_clk_rate);

	pll_dynamic_refresh_14nm(pll, pll_res);
	return 0;
}

static int dsi_pll_14nm_vco_set_rate(struct dsi_pll_resource *pll_res)
{
	struct dsi_pll_14nm *pll;

	pll = pll_res->priv;
	if (!pll) {
		DSI_PLL_ERR(pll_res, "pll configuration not found\n");
		return -EINVAL;
	}

	DSI_PLL_DBG(pll_res, "rate=%lu\n", pll_res->vco_rate);

	pll_res->vco_current_rate = pll_res->vco_rate;

	dsi_pll_calc_dec_frac(pll, pll_res);

	if (pll_res->ssc_en)
		dsi_pll_calc_ssc(pll, pll_res);

	pll_14nm_calc_vco_count(pll, pll_res->vco_current_rate,
			pll_res->vco_ref_clk_rate);

	/* commit master itself */
	pll_db_commit_14nm(pll, pll_res);

	return 0;
}


static int dsi_pll_14nm_enable(struct dsi_pll_resource *rsc)
{
	int rc = 0;

	if (!rsc) {
		DSI_PLL_ERR(rsc, "Invalid PLL resources\n");
		return -EINVAL;
	}

	dsi_pll_start_14nm(rsc);

	/*
	 * both DSIPHY_PLL_CLKBUFLR_EN and DSIPHY_CMN_GLBL_TEST_CTRL
	 * enabled at mdss_dsi_14nm_phy_config()
	 */

	rc = dsi_pll_14nm_lock_status(rsc);
	if (!rc) {
		DSI_PLL_ERR(rsc, "DSI PLL ndx=%d lock failed\n");
		rc = -EINVAL;
		goto init_lock_err;
	}

	DSI_PLL_DBG(rsc, "Lock success\n");
	return 0;

init_lock_err:
	return rc;
}

static int dsi_pll_14nm_disable(struct dsi_pll_resource *rsc)
{
	DSI_PLL_DBG(rsc, "stop PLL\n");

	dsi_pll_stop_14nm(rsc);

	/* flush, ensure all register writes are done*/
	wmb();

	return 0;
}

int dsi_pll_14nm_configure(void *pll, bool commit)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = (struct dsi_pll_resource *)pll;

	pll_source_setup(rsc);

	dsi_pll_14nm_set_byteclk_div(rsc, commit);
	dsi_pll_14nm_set_pclk_div(rsc, commit);

	if (commit) {
		rc = dsi_pll_14nm_vco_set_rate(rsc);
	} else {
		rc = dsi_pll_14nm_dynamic_clk_vco_set_rate(rsc);
	}

	return 0;
}

int dsi_pll_14nm_toggle(void *pll, bool prepare)
{

	int rc = 0;
	struct dsi_pll_resource *pll_res = (struct dsi_pll_resource *)pll;

	if (!pll_res) {
		DSI_PLL_ERR(pll_res, "dsi pll resources are not available\n");
		return -EINVAL;
	}

	if (prepare) {
		rc = dsi_pll_14nm_enable(pll_res);
		if (rc)
			DSI_PLL_ERR(pll_res, "enable failed: %d\n", rc);
	} else {
		rc = dsi_pll_14nm_disable(pll_res);
		if (rc)
			DSI_PLL_ERR(pll_res, "disable failed: %d\n", rc);
	}

	return rc;
}
