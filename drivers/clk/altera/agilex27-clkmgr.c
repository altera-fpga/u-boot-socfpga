// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <div64.h>
#include <errno.h>
#include <log.h>
#include <wait_bit.h>
#include <asm/io.h>
#include <hang.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>
#include <vsprintf.h>
#include <asm/arch/base_addr_soc64.h>

#include "agilex27-clkmgr.h"
#include "clk-agilex27.h"

static struct agilex27_clkmgr_rate_state agilex27_rate_state = {
	.valid = false,
	.gppll0_vco_hz = AGILEX27_GPPLL0_VCO_HZ,
	.gppll1_vco_hz = AGILEX27_GPPLL1_VCO_HZ,
	.gppll2_vco_hz = AGILEX27_GPPLL2_VCO_HZ,
	.gppll0_c0_div = AGILEX27_GPPLL0_C0_DIV,
	.gppll0_c1_div = AGILEX27_GPPLL0_C1_DIV,
	.gppll0_c2_div = AGILEX27_GPPLL0_C2_DIV,
	.gppll0_c3_div = AGILEX27_GPPLL0_C3_DIV,
	.gppll1_c0_div = AGILEX27_GPPLL1_C0_DIV,
	.gppll1_c1_div = AGILEX27_GPPLL1_C1_DIV,
	.gppll2_c0_div = AGILEX27_GPPLL2_C0_DIV,
	.gppll2_c1_div = AGILEX27_GPPLL2_C1_DIV,
};

static bool agilex27_clkmgr_pll_locked;

#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
static void agilex27_mmio_trace_write(const char *ctx, int idx, u32 val, u32 addr)
{
	if (idx >= 0)
		printf("AGILEX27_MMIO: %s[%d] writel(0x%08x, 0x%08x)\n", ctx, idx, val, addr);
	else
		printf("AGILEX27_MMIO: %s writel(0x%08x, 0x%08x)\n", ctx, val, addr);
}

static void agilex27_mmio_trace_read(const char *ctx, u32 addr, u32 val)
{
	printf("AGILEX27_MMIO: %s readl(0x%08x)->0x%08x\n", ctx, addr, val);
}
#endif

const struct agilex27_clkmgr_rate_state *
agilex27_clkmgr_rate_state(void)
{
	return &agilex27_rate_state;
}

typedef void (*agilex27_clkmgr_action_t)(const char *value);

static void agilex27_act_pll_enable(const char *value)
{
	(void)value;
	agilex27_pll_enable();
}

static void agilex27_act_pll_wait_lock(const char *value)
{
	(void)value;
	agilex27_pll_wait_lock();
}

static void agilex27_act_disable_boot_clk_bypass(const char *value)
{
	(void)value;

	if (!agilex27_clkmgr_pll_locked) {
		pr_err("agilex27-clkmgr: skipping boot-mode exit: PLLs not locked\n");
		return;
	}

	agilex27_disable_boot_clk_bypass();
}

static int agilex27_decode_freq(const char *key, const char *value,
				unsigned long *out)
{
	char *endp = NULL;
	unsigned long val;

	if (!value || !*value) {
		pr_warn("agilex27-clkmgr: key '%s' has empty value, skipping\n",
			key);
		return -EINVAL;
	}

	val = simple_strtoul(value, &endp, 0);
	if (endp == value || (endp && *endp)) {
		pr_warn("agilex27-clkmgr: key '%s' value '%s' is not a number\n",
			key, value);
		return -EINVAL;
	}

	*out = val;

	return 0;
}

static void agilex27_act_config_gppll1(const char *value)
{
	unsigned long freq = 0;

	if (agilex27_decode_freq("gppll1_freq", value, &freq))
		return;
	agilex27_rate_state.gppll1_vco_hz = (u64)freq;
	agilex27_rate_state.valid = true;
	agilex27_config_main_pll(freq);
}

static void agilex27_act_config_gppll0(const char *value)
{
	unsigned long freq = 0;

	if (agilex27_decode_freq("gppll0_freq", value, &freq))
		return;
	agilex27_rate_state.gppll0_vco_hz = (u64)freq;
	agilex27_rate_state.valid = true;
	agilex27_config_periph_pll(freq);
}

static void agilex27_act_config_gppll2(const char *value)
{
	unsigned long freq = 0;

	if (agilex27_decode_freq("gppll2_freq", value, &freq))
		return;
	agilex27_rate_state.gppll2_vco_hz = (u64)freq;
	agilex27_rate_state.valid = true;
	pr_debug("agilex27-clkmgr: gppll2_freq=%lu Hz (REG_ABS-driven)\n",
		 freq);
}

#define AGILEX27_C_DIV_MAX 1024

static int agilex27_decode_div(const char *key, const char *value, u32 *out)
{
	unsigned long val = 0;

	if (agilex27_decode_freq(key, value, &val))
		return -EINVAL;

	if (val == 0 || val > AGILEX27_C_DIV_MAX) {
		pr_warn("agilex27-clkmgr: key '%s' value %lu out of range [1, %u], skipping\n",
			key, val, AGILEX27_C_DIV_MAX);
		return -ERANGE;
	}

	*out = (u32)val;

	return 0;
}

static void agilex27_act_config_gppll0_c0_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll0_c0_div", value, &div))
		return;
	agilex27_rate_state.gppll0_c0_div = div;
	pr_debug("agilex27-clkmgr: gppll0_c0_div=%u\n", div);
}

static void agilex27_act_config_gppll0_c1_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll0_c1_div", value, &div))
		return;
	agilex27_rate_state.gppll0_c1_div = div;
	pr_debug("agilex27-clkmgr: gppll0_c1_div=%u\n", div);
}

static void agilex27_act_config_gppll0_c2_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll0_c2_div", value, &div))
		return;
	agilex27_rate_state.gppll0_c2_div = div;
	pr_debug("agilex27-clkmgr: gppll0_c2_div=%u\n", div);
}

static void agilex27_act_config_gppll0_c3_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll0_c3_div", value, &div))
		return;
	agilex27_rate_state.gppll0_c3_div = div;
	pr_debug("agilex27-clkmgr: gppll0_c3_div=%u\n", div);
}

static void agilex27_act_config_gppll1_c0_div(const char *value)
{
	u32 div;

	if (agilex27_decode_div("gppll1_c0_div", value, &div))
		return;
	agilex27_rate_state.gppll1_c0_div = div;
	pr_debug("agilex27-clkmgr: gppll1_c0_div=%u\n", div);
}

static void agilex27_act_config_gppll1_c1_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll1_c1_div", value, &div))
		return;
	agilex27_rate_state.gppll1_c1_div = div;
	pr_debug("agilex27-clkmgr: gppll1_c1_div=%u\n", div);
}

static void agilex27_act_config_gppll2_c0_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll2_c0_div", value, &div))
		return;
	agilex27_rate_state.gppll2_c0_div = div;
	pr_debug("agilex27-clkmgr: gppll2_c0_div=%u\n", div);
}

static void agilex27_act_config_gppll2_c1_div(const char *value)
{
	u32 div = 0;

	if (agilex27_decode_div("gppll2_c1_div", value, &div))
		return;
	agilex27_rate_state.gppll2_c1_div = div;
	pr_debug("agilex27-clkmgr: gppll2_c1_div=%u\n", div);
}

struct agilex27_clkmgr_action {
	const char *key;
	agilex27_clkmgr_action_t action;
};

static const struct agilex27_clkmgr_action agilex27_clkmgr_actions[] = {
	{ "pll_enable",			agilex27_act_pll_enable },
	{ "pll_wait_lock",		agilex27_act_pll_wait_lock },
	{ "boot_clk_bypass_disable",	agilex27_act_disable_boot_clk_bypass },

	{ "gppll0_freq",		agilex27_act_config_gppll0 },
	{ "peripheral_pll_freq",	agilex27_act_config_gppll0 },
	{ "gppll1_freq",		agilex27_act_config_gppll1 },
	{ "main_pll_freq",		agilex27_act_config_gppll1 },
	{ "gppll2_freq",		agilex27_act_config_gppll2 },

	{ "gppll0_c0_div",		agilex27_act_config_gppll0_c0_div },
	{ "gppll0_c1_div",		agilex27_act_config_gppll0_c1_div },
	{ "gppll0_c2_div",		agilex27_act_config_gppll0_c2_div },
	{ "gppll0_c3_div",		agilex27_act_config_gppll0_c3_div },
	{ "gppll1_c0_div",		agilex27_act_config_gppll1_c0_div },
	{ "gppll1_c1_div",		agilex27_act_config_gppll1_c1_div },
	{ "gppll2_c0_div",		agilex27_act_config_gppll2_c0_div },
	{ "gppll2_c1_div",		agilex27_act_config_gppll2_c1_div },
};

int agilex27_clkmgr_apply_handoff(const struct agilex27_clkmgr_entry *entries, int count)
{
	int i;

	if (count < 0)
		return -EINVAL;

	if (!entries && count > 0)
		return -EINVAL;

	if (count == 0) {
		pr_info("agilex27-clkmgr: empty handoff, falling back to hardcoded init\n");
		return 0;
	}

	pr_info("agilex27-clkmgr: applying handoff (%d entries)\n", count);

	for (i = 0; i < count; i++) {
		const struct agilex27_clkmgr_entry *e = &entries[i];
		const struct agilex27_clkmgr_action *a;
		size_t j;
		bool matched = false;

		if (!e->key) {
			pr_err("agilex27-clkmgr: NULL key at index %d, aborting\n", i);
			return -EINVAL;
		}

		for (j = 0; j < ARRAY_SIZE(agilex27_clkmgr_actions); j++) {
			a = &agilex27_clkmgr_actions[j];
			if (!strcmp(e->key, a->key)) {
				pr_debug("agilex27-clkmgr: [%d] %s = %s\n", i, e->key,
					 e->value ? e->value : "");
				a->action(e->value);
				matched = true;
				if (!strcmp(e->key, "pll_wait_lock") &&
				    !agilex27_clkmgr_pll_locked) {
					pr_err("agilex27-clkmgr: aborting handoff after PLL lock failure\n");
					return -ETIMEDOUT;
				}
				break;
			}
		}

		if (!matched)
			pr_warn("agilex27-clkmgr: unknown key '%s' at index %d, skipping\n",
				e->key, i);
	}

	pr_info("agilex27-clkmgr: handoff applied\n");

	return 0;
}

static bool agilex27_reg_abs_addr_allowed(u32 addr);

int agilex27_clkmgr_apply_reg_abs(const struct agilex27_clkmgr_reg_abs_entry *entries,
				  int count)
{
	int i;

	if (count < 0)
		return -EINVAL;

	if (!entries && count > 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	pr_debug("agilex27-clkmgr: applying %d REG_ABS entries\n", count);

	for (i = 0; i < count; i++) {
		const struct agilex27_clkmgr_reg_abs_entry *e = &entries[i];
		void __iomem *reg = (void __iomem *)(uintptr_t)e->addr;
		u32 val;

		if (!agilex27_reg_abs_addr_allowed(e->addr)) {
			pr_err("agilex27-clkmgr: REG_ABS addr 0x%08x outside CLKMGR/GPPLL blocks\n",
			       e->addr);
			return -EINVAL;
		}

		if (e->mask)
			val = (readl(reg) & ~e->mask) | (e->value & e->mask);
		else
			val = e->value;

		pr_debug("agilex27-clkmgr: writel(0x%08x, 0x%08x)\n",
			 val, e->addr);
#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
		agilex27_mmio_trace_write("reg_abs", i, val, e->addr);
#endif
		writel(val, reg);
	}

	return 0;
}

#define AGILEX27_CLKMGR_CTRL			(SOCFPGA_CLKMGR_ADDRESS + 0x000)
#define AGILEX27_CLKMGR_CTRL_BOOTMODE		BIT(0)
#define AGILEX27_CLKMGR_STAT			(SOCFPGA_CLKMGR_ADDRESS + 0x004)
#define AGILEX27_CLKMGR_STAT_BUSY		BIT(0)
#define AGILEX27_CLKMGR_STAT_PLL0_LOCKED	BIT(8)
#define AGILEX27_CLKMGR_STAT_PLL1_LOCKED	BIT(10)
#define AGILEX27_CLKMGR_STAT_PLL2_LOCKED	BIT(12)
#define AGILEX27_CLKMGR_STAT_ALL_LOCKED					\
	(AGILEX27_CLKMGR_STAT_PLL0_LOCKED | AGILEX27_CLKMGR_STAT_PLL1_LOCKED |	\
	 AGILEX27_CLKMGR_STAT_PLL2_LOCKED)

#define AGILEX27_CLKMGR_PLL2_GLOB		(SOCFPGA_CLKMGR_ADDRESS + 0x060)
#define AGILEX27_CLKMGR_PLL1_GLOB		(SOCFPGA_CLKMGR_ADDRESS + 0x080)
#define AGILEX27_CLKMGR_PLL0_GLOB		(SOCFPGA_CLKMGR_ADDRESS + 0x160)

#define AGILEX27_GPPLL_CFG1_OFF		0x004
#define AGILEX27_GPPLL_CFG2_OFF		0x008
#define AGILEX27_GPPLL_CFG5_OFF		0x014
#define AGILEX27_GPPLL_CFG23_OFF		0x05C
#define AGILEX27_GPPLL_CFG9_OFF		0x024
#define AGILEX27_GPPLL_CFG10_OFF		0x028
#define AGILEX27_GPPLL_CFG11_OFF		0x02C
#define AGILEX27_GPPLL_CFG12_OFF		0x030
#define AGILEX27_GPPLL_FRAC_SHIFT		24

#define AGILEX27_CLKMGR_PLLGLOB_PSRC_MASK	GENMASK(28, 27)
#define AGILEX27_PLLGLOB_PSRC_EOSC1		0
#define AGILEX27_PLLGLOB_PSRC_INTOSC	1
#define AGILEX27_PLLGLOB_PSRC_F2S		2

#define AGILEX27_GPPLL_VCO_MIN_HZ		600000000ULL
#define AGILEX27_GPPLL_VCO_MAX_HZ		5000000000ULL

#define AGILEX27_GPPLL_EOSC1_REF_HZ		100000000ULL
#define AGILEX27_GPPLL_INTOSC_REF_HZ	AGILEX27_GPPLL_EOSC1_REF_HZ

#define AGILEX27_GPPLL_CNT_DIV_MAX		512

enum agilex27_gppll_synth_mode {
	AGILEX27_GPPLL_SYNTH_EQ1 = 0,
	AGILEX27_GPPLL_SYNTH_EQ3,
	AGILEX27_GPPLL_SYNTH_UNSUPPORTED,
};

#define AGILEX27_PLL_GLOB_S1_CSR_EN		0x00008026
#define AGILEX27_PLL_GLOB_S2_BIAS_ON		0x00008036
#define AGILEX27_PLL_GLOB_S3_FRZREG_OFF	0x00008016
#define AGILEX27_PLL_GLOB_S4_IOTRI_ON		0x0000805E
#define AGILEX27_PLL_GLOB_S5_FBLVDS_ON	0x000080DE

#define AGILEX27_CLKPLL0_BASE			0x0915E000
#define AGILEX27_CLKPLL1_BASE			0x0915F000
#define AGILEX27_CLKPLL2_BASE			0x09160000
#define AGILEX27_CLKPLL_CFG5_OFFSET		0x014
#define AGILEX27_CLKPLL_CFG5_REARM_VAL	0x02003408
#define AGILEX27_REG_ABS_CLKMGR_SIZE		0x200
#define AGILEX27_REG_ABS_GPPLL_SIZE		0x80

static bool agilex27_reg_abs_addr_allowed(u32 addr)
{
	if (addr >= SOCFPGA_CLKMGR_ADDRESS &&
	    addr < SOCFPGA_CLKMGR_ADDRESS + AGILEX27_REG_ABS_CLKMGR_SIZE)
		return true;
	if (addr >= AGILEX27_CLKPLL0_BASE &&
	    addr < AGILEX27_CLKPLL0_BASE + AGILEX27_REG_ABS_GPPLL_SIZE)
		return true;
	if (addr >= AGILEX27_CLKPLL1_BASE &&
	    addr < AGILEX27_CLKPLL1_BASE + AGILEX27_REG_ABS_GPPLL_SIZE)
		return true;
	if (addr >= AGILEX27_CLKPLL2_BASE &&
	    addr < AGILEX27_CLKPLL2_BASE + AGILEX27_REG_ABS_GPPLL_SIZE)
		return true;

	return false;
}
#define AGILEX27_GPPLL_CFG5_REARM_STROBE	BIT(3)

#define AGILEX27_SECMGR_CLKMGR_UNLOCK_ADDR	0x09152068
#define AGILEX27_SECMGR_CLKMGR_UNLOCK_VAL	0x00000001

static inline void agilex27_writel(u32 val, u32 addr)
{
	pr_debug("agilex27-clkmgr: writel(0x%08x, 0x%08x)\n", val, addr);
#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
	agilex27_mmio_trace_write("clkmgr", -1, val, addr);
#endif
	writel(val, (void __iomem *)(uintptr_t)addr);
}

static inline u32 agilex27_readl(u32 addr)
{
	u32 val = readl((const void __iomem *)(uintptr_t)addr);

#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
	agilex27_mmio_trace_read("clkmgr", addr, val);
#endif
	return val;
}

static u64 agilex27_gppll_fref_hz(u32 glob_reg)
{
	u32 psrc;

	psrc = (agilex27_readl(glob_reg) &
		AGILEX27_CLKMGR_PLLGLOB_PSRC_MASK) >> 27;

	switch (psrc) {
	case AGILEX27_PLLGLOB_PSRC_INTOSC:
		return AGILEX27_GPPLL_INTOSC_REF_HZ;
	case AGILEX27_PLLGLOB_PSRC_F2S:
		return AGILEX27_F2S_FREE_HZ;
	case AGILEX27_PLLGLOB_PSRC_EOSC1:
	default:
		return AGILEX27_GPPLL_EOSC1_REF_HZ;
	}
}

static u32 agilex27_gppll_cnt_count(u32 nine_bit_field)
{
	u32 cnt = nine_bit_field & GENMASK(7, 0);

	return cnt ? cnt : 256;
}

static u32 agilex27_gppll_cnt_div_hi_lo(u32 hi, u32 lo, bool *bypass)
{
	u32 hi_cnt, lo_cnt, div;

	if (hi & BIT(8)) {
		*bypass = true;
		return 1;
	}

	*bypass = false;
	hi_cnt = agilex27_gppll_cnt_count(hi);
	lo_cnt = agilex27_gppll_cnt_count(lo);
	div = hi_cnt + lo_cnt;

	if (div < 1 || div > AGILEX27_GPPLL_CNT_DIV_MAX)
		return 0;

	return div;
}

static u32 agilex27_gppll_n_div(u32 cfg1)
{
	u32 n_hi = cfg1 & GENMASK(8, 0);
	u32 n_lo = (cfg1 >> 9) & GENMASK(8, 0);
	bool bypass;
	u32 div;

	div = agilex27_gppll_cnt_div_hi_lo(n_hi, n_lo, &bypass);

	return div ? div : 1;
}

static u32 agilex27_gppll_cfg5_for_synth_mode(u32 cfg5)
{
	if ((cfg5 & ~AGILEX27_GPPLL_CFG5_REARM_STROBE) ==
	    (AGILEX27_CLKPLL_CFG5_REARM_VAL &
	     ~AGILEX27_GPPLL_CFG5_REARM_STROBE))
		return cfg5 & ~AGILEX27_GPPLL_CFG5_REARM_STROBE;

	return cfg5;
}

static enum agilex27_gppll_synth_mode
agilex27_gppll_synth_mode(u32 cfg1, u32 cfg2, u32 cfg5)
{
	u32 cfg5_mode = agilex27_gppll_cfg5_for_synth_mode(cfg5);
	u32 cr_m_directfb = (cfg2 >> 26) & 1;
	u32 cr_fbclk_sel = (cfg5_mode >> 3) & GENMASK(1, 0);
	u32 crsel_m = (cfg1 >> 29) & GENMASK(1, 0);

	if (cr_m_directfb && !cr_fbclk_sel && !crsel_m)
		return AGILEX27_GPPLL_SYNTH_EQ1;

	if (!cr_m_directfb && !cr_fbclk_sel)
		return AGILEX27_GPPLL_SYNTH_EQ3;

	return AGILEX27_GPPLL_SYNTH_UNSUPPORTED;
}

static void agilex27_gppll_hang_bad_synth(const char *pll_name, u32 cfg1,
					  u32 cfg2, u32 cfg5,
					     enum agilex27_gppll_synth_mode mode)
{
	u32 cr_m_directfb = (cfg2 >> 26) & 1;
	u32 cr_fbclk_sel = (cfg5 >> 3) & GENMASK(1, 0);
	u32 crsel_m = (cfg1 >> 29) & GENMASK(1, 0);

	if (mode == AGILEX27_GPPLL_SYNTH_EQ3) {
		pr_err("agilex27-clkmgr: %s non-dedicated feedback (MAS eq. 3) — Fvco needs feedback C-div; no CSR mapping in tree\n",
		       pll_name);
	} else {
		pr_err("agilex27-clkmgr: %s unsupported GPPLL synthesis mode (cfg1=0x%08x cfg2=0x%08x cfg5=0x%08x)\n",
		       pll_name, cfg1, cfg2, cfg5);
	}

	pr_err("agilex27-clkmgr: cr_m_directfb=%u cr_fbclk_sel=%u crsel_m=%u cr_m_byp=%u\n",
	       cr_m_directfb, cr_fbclk_sel, crsel_m, !!(cfg1 & BIT(31)));
	hang();
}

static u64 agilex27_gppll_multiplier_q24(u32 cfg1, u32 cfg23)
{
	u32 m_csr = (cfg1 >> 20) & GENMASK(8, 0);
	u32 frac = cfg23 & GENMASK(23, 0);

	if (cfg1 & BIT(31))
		return 1ULL << AGILEX27_GPPLL_FRAC_SHIFT;

	if (frac)
		return ((u64)m_csr << AGILEX27_GPPLL_FRAC_SHIFT) + frac;

	return (u64)m_csr << AGILEX27_GPPLL_FRAC_SHIFT;
}

static u64 agilex27_gppll_vco_hz_from_regs(u32 pll_base, u32 glob_reg,
					   const char *pll_name)
{
	u32 cfg1 = agilex27_readl(pll_base + AGILEX27_GPPLL_CFG1_OFF);
	u32 cfg2 = agilex27_readl(pll_base + AGILEX27_GPPLL_CFG2_OFF);
	u32 cfg5 = agilex27_readl(pll_base + AGILEX27_GPPLL_CFG5_OFF);
	u32 cfg23 = agilex27_readl(pll_base + AGILEX27_GPPLL_CFG23_OFF);
	enum agilex27_gppll_synth_mode mode;
	u64 fref = agilex27_gppll_fref_hz(glob_reg);
	u64 mult = agilex27_gppll_multiplier_q24(cfg1, cfg23);
	u32 n_div = agilex27_gppll_n_div(cfg1);
	u64 vco;

	mode = agilex27_gppll_synth_mode(cfg1, cfg2, cfg5);
	if (mode != AGILEX27_GPPLL_SYNTH_EQ1) {
		if (IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)) {
			pr_warn("agilex27-clkmgr: %s synth mode %d not decoded on EMU (cfg1=0x%08x cfg2=0x%08x cfg5=0x%08x)\n",
				pll_name, mode, cfg1, cfg2, cfg5);
			return 0;
		}
		agilex27_gppll_hang_bad_synth(pll_name, cfg1, cfg2, cfg5, mode);
	}

	if (!fref || !n_div || !mult)
		return 0;

	vco = div64_u64(fref, n_div);
	vco = vco * (mult >> AGILEX27_GPPLL_FRAC_SHIFT) +
	      div64_u64(vco * (mult & GENMASK(23, 0)),
			1ULL << AGILEX27_GPPLL_FRAC_SHIFT);

	return vco;
}

static bool agilex27_gppll_vco_sane(u64 hz)
{
	return hz >= AGILEX27_GPPLL_VCO_MIN_HZ &&
	       hz <= AGILEX27_GPPLL_VCO_MAX_HZ;
}

struct agilex27_gppll_ccnt_layout {
	u8 crhi_sh;
	u8 crsel_sh;
	u8 crlo_sh;
};

static const struct agilex27_gppll_ccnt_layout
agilex27_gppll_ccnt_layout[] = {
	/* cfg_9: C0 — crhi[8:0]@0, crsel@9, crlo[8:0]@23 */
	{ 0, 9, 23 },
	/* cfg_10: C1 — crsel@0, crlo@14, crhi@23 */
	{ 23, 0, 14 },
	/* cfg_11/12: C2/C3 — crlo@0, crhi@9, crsel@18 */
	{ 9, 18, 0 },
	{ 9, 18, 0 },
};

struct agilex27_gppll_ccnt_slot {
	const char *pll_name;
	const char *cout_name;
	u32 pll_base;
	u32 cfg_off;
	u8 layout_idx;
	u32 *div_slot;
};

static const char *agilex27_gppll_crsel_name(u32 crsel)
{
	switch (crsel) {
	case 0:
		return "VCOPH(00)";
	case 1:
		return "CASCADEIN(01)";
	case 2:
		return "TCLK(10)";
	case 3:
		return "VSS/DLY(11)";
	default:
		return "?";
	}
}

static u32 agilex27_gppll_c_div_vcoph(u32 cfg, u8 layout_idx, bool *bypass)
{
	const struct agilex27_gppll_ccnt_layout *l;
	u32 crsel, crhi, crlo, hi, lo, div;

	if (layout_idx >= ARRAY_SIZE(agilex27_gppll_ccnt_layout))
		return 0;

	l = &agilex27_gppll_ccnt_layout[layout_idx];
	crsel = (cfg >> l->crsel_sh) & GENMASK(1, 0);
	crhi = (cfg >> l->crhi_sh) & GENMASK(8, 0);
	crlo = (cfg >> l->crlo_sh) & GENMASK(8, 0);

	if (crsel != 0)
		return 0;

	if (crhi & BIT(8)) {
		*bypass = true;
		return 1;
	}

	*bypass = false;
	hi = agilex27_gppll_cnt_count(crhi);
	lo = agilex27_gppll_cnt_count(crlo);

	div = hi + lo;

	if (div < 1 || div > AGILEX27_C_DIV_MAX)
		return 0;

	return div;
}

static void agilex27_gppll_ccnt_hang_bad_crsel(const struct agilex27_gppll_ccnt_slot *state,
					       u32 cfg, u32 crsel)
{
	const struct agilex27_gppll_ccnt_layout *l;
	u32 crhi, crlo;

	l = &agilex27_gppll_ccnt_layout[state->layout_idx];
	crhi = (cfg >> l->crhi_sh) & GENMASK(8, 0);
	crlo = (cfg >> l->crlo_sh) & GENMASK(8, 0);

	pr_err("agilex27-clkmgr: %s %s CRSEL=%u (%s) unsupported for clk rate decode (cfg=0x%08x crhi=0x%03x crlo=0x%03x)\n",
	       state->pll_name, state->cout_name, crsel,
	       agilex27_gppll_crsel_name(crsel), cfg, crhi, crlo);
	pr_err("agilex27-clkmgr: GPPLL MAS documents VCOPH divide only for CRSEL=00; other modes need architect formula (F7)\n");
	hang();
}

static bool agilex27_gppll_refresh_one_ccnt(const struct agilex27_gppll_ccnt_slot *state)
{
	const struct agilex27_gppll_ccnt_layout *l;
	u32 cfg, crsel, div;
	bool bypass = false;
	u32 kv_div;

	cfg = agilex27_readl(state->pll_base + state->cfg_off);
	l = &agilex27_gppll_ccnt_layout[state->layout_idx];
	crsel = (cfg >> l->crsel_sh) & GENMASK(1, 0);

	if (crsel != 0) {
		agilex27_gppll_ccnt_hang_bad_crsel(state, cfg, crsel);
		return false;
	}

	div = agilex27_gppll_c_div_vcoph(cfg, state->layout_idx, &bypass);
	if (!div) {
		pr_warn("agilex27-clkmgr: %s %s C-div CSR decode failed (cfg=0x%08x), keeping prior value %u\n",
			state->pll_name, state->cout_name, cfg, *state->div_slot);
		return false;
	}

	kv_div = *state->div_slot;
	*state->div_slot = div;

	if (kv_div && kv_div != div)
		pr_warn("agilex27-clkmgr: %s %s C-div CSR=%u%s differs from handoff KV=%u\n",
			state->pll_name, state->cout_name, div,
			bypass ? " bypass" : "", kv_div);
	else
		pr_debug("agilex27-clkmgr: %s %s C-div %u%s (cfg=0x%08x)\n",
			 state->pll_name, state->cout_name, div,
			 bypass ? " bypass" : "", cfg);

	return true;
}

void agilex27_clkmgr_refresh_c_from_csr(void)
{
	static const struct agilex27_gppll_ccnt_slot slots[] = {
		{
			"GPPLL0", "C0", AGILEX27_CLKPLL0_BASE,
			AGILEX27_GPPLL_CFG9_OFF, 0,
			&agilex27_rate_state.gppll0_c0_div,
		},
		{
			"GPPLL0", "C1", AGILEX27_CLKPLL0_BASE,
			AGILEX27_GPPLL_CFG10_OFF, 1,
			&agilex27_rate_state.gppll0_c1_div,
		},
		{
			"GPPLL0", "C2", AGILEX27_CLKPLL0_BASE,
			AGILEX27_GPPLL_CFG11_OFF, 2,
			&agilex27_rate_state.gppll0_c2_div,
		},
		{
			"GPPLL0", "C3", AGILEX27_CLKPLL0_BASE,
			AGILEX27_GPPLL_CFG12_OFF, 3,
			&agilex27_rate_state.gppll0_c3_div,
		},
		{
			"GPPLL1", "C0", AGILEX27_CLKPLL1_BASE,
			AGILEX27_GPPLL_CFG9_OFF, 0,
			&agilex27_rate_state.gppll1_c0_div,
		},
		{
			"GPPLL1", "C1", AGILEX27_CLKPLL1_BASE,
			AGILEX27_GPPLL_CFG10_OFF, 1,
			&agilex27_rate_state.gppll1_c1_div,
		},
		{
			"GPPLL2", "C0", AGILEX27_CLKPLL2_BASE,
			AGILEX27_GPPLL_CFG9_OFF, 0,
			&agilex27_rate_state.gppll2_c0_div,
		},
		{
			"GPPLL2", "C1", AGILEX27_CLKPLL2_BASE,
			AGILEX27_GPPLL_CFG10_OFF, 1,
			&agilex27_rate_state.gppll2_c1_div,
		},
	};
	size_t i;
	bool any = false;

	for (i = 0; i < ARRAY_SIZE(slots); i++) {
		if (agilex27_gppll_refresh_one_ccnt(&slots[i]))
			any = true;
	}

	if (any)
		pr_debug("agilex27-clkmgr: GPPLL C-dividers refreshed from CSR (crsel==VCOPH)\n");
}

void agilex27_clkmgr_refresh_vco_from_csr(void)
{
	static const struct {
		u32 pll_base;
		u32 glob_reg;
		u64 *vco_hz;
		const char *name;
	} plls[] = {
		{
			AGILEX27_CLKPLL0_BASE,
			AGILEX27_CLKMGR_PLL0_GLOB,
			&agilex27_rate_state.gppll0_vco_hz,
			"GPPLL0",
		},
		{
			AGILEX27_CLKPLL1_BASE,
			AGILEX27_CLKMGR_PLL1_GLOB,
			&agilex27_rate_state.gppll1_vco_hz,
			"GPPLL1",
		},
		{
			AGILEX27_CLKPLL2_BASE,
			AGILEX27_CLKMGR_PLL2_GLOB,
			&agilex27_rate_state.gppll2_vco_hz,
			"GPPLL2",
		},
	};
	size_t i;
	bool any = false;

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		u32 cfg1 = agilex27_readl(plls[i].pll_base +
					      AGILEX27_GPPLL_CFG1_OFF);
		u32 cfg23 = agilex27_readl(plls[i].pll_base +
					       AGILEX27_GPPLL_CFG23_OFF);
		u32 m_csr = (cfg1 >> 20) & GENMASK(8, 0);
		u32 frac = cfg23 & GENMASK(23, 0);
		u64 vco = agilex27_gppll_vco_hz_from_regs(plls[i].pll_base,
							      plls[i].glob_reg,
							      plls[i].name);

		if (!(cfg1 & BIT(31)) && !frac && m_csr >= 32)
			pr_warn("agilex27-clkmgr: %s CRHI_M=0x%x (>=32, integer-only) uses strict MAS decode; verify preset encoding\n",
				plls[i].name, m_csr);

		if (!agilex27_gppll_vco_sane(vco)) {
			pr_warn("agilex27-clkmgr: %s VCO decode out of range (%llu Hz), cfg1=0x%08x cfg23=0x%08x\n",
				plls[i].name, vco, cfg1, cfg23);
			continue;
		}

		*plls[i].vco_hz = vco;
		any = true;
		pr_debug("agilex27-clkmgr: %s VCO %llu Hz (cfg1=0x%08x cfg23=0x%08x)\n",
			 plls[i].name, vco, cfg1, cfg23);
	}

	if (any)
		agilex27_rate_state.valid = true;
}

void agilex27_clkmgr_secure_unlock(void)
{
	pr_debug("agilex27-clkmgr: secure-mgr unlock\n");
	agilex27_writel(AGILEX27_SECMGR_CLKMGR_UNLOCK_VAL, AGILEX27_SECMGR_CLKMGR_UNLOCK_ADDR);
}

void agilex27_clkmgr_pll_cfg5_rearm(void)
{
	int pass;

	pr_debug("agilex27-clkmgr: PLL cfg5 re-arm (two passes)\n");
	for (pass = 0; pass < 2; pass++) {
		agilex27_writel(AGILEX27_CLKPLL_CFG5_REARM_VAL,
				AGILEX27_CLKPLL0_BASE + AGILEX27_CLKPLL_CFG5_OFFSET);
		agilex27_writel(AGILEX27_CLKPLL_CFG5_REARM_VAL,
				AGILEX27_CLKPLL1_BASE + AGILEX27_CLKPLL_CFG5_OFFSET);
		agilex27_writel(AGILEX27_CLKPLL_CFG5_REARM_VAL,
				AGILEX27_CLKPLL2_BASE + AGILEX27_CLKPLL_CFG5_OFFSET);
	}
}

#define AGILEX27_HANDOFF_CLKMGR_TOP_ABS_COUNT	9U

void agilex27_clkmgr_bisect_reg_abs(u32 base, u32 count)
{
	if (base == AGILEX27_CLKPLL0_BASE)
		printf("AGILEX27_BISECT: stage2 pll0 preset done (%u regs)\n", count);
	else if (base == AGILEX27_CLKPLL1_BASE)
		printf("AGILEX27_BISECT: stage3 pll1 preset done\n");
	else if (base == AGILEX27_CLKPLL2_BASE)
		printf("AGILEX27_BISECT: stage4 pll2 preset done\n");
	else if (base == SOCFPGA_CLKMGR_ADDRESS)
		printf("AGILEX27_BISECT: stage%u clkmgr_top%s done\n",
		       count == AGILEX27_HANDOFF_CLKMGR_TOP_ABS_COUNT ? 8 : 9,
		       count == AGILEX27_HANDOFF_CLKMGR_TOP_ABS_COUNT ? "" : " dividers");
}

void agilex27_clkmgr_bisect_cfg5_rearm_done(void)
{
	printf("AGILEX27_BISECT: stage5 cfg5 rearm done\n");
}

void agilex27_clkmgr_bisect_kv_milestone(const char *first_key, int ret)
{
	if (!strcmp(first_key, "pll_enable"))
		printf("AGILEX27_BISECT: stage6 pll_enable+lock ret=%d\n", ret);
	else if (!strcmp(first_key, "gppll0_c0_div")) {
		printf("AGILEX27_BISECT: stage7 rate_state kv done\n");
		if (IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)) {
			ret = agilex27_clkmgr_audit_dv_preset_rates(true);
			printf("AGILEX27_BISECT: stage7 DV clkout audit ret=%d\n", ret);
			agilex27_clkmgr_print_rate_state();
		}
	} else if (!strcmp(first_key, "boot_clk_bypass_disable")) {
		printf("AGILEX27_BISECT: stage10 bootmode exit done\n");
	}
}

#define AGILEX27_PLL_STAGE_DELAY_US		5

void agilex27_pll_enable(void)
{
	static const u32 pll_globs[] = {
		AGILEX27_CLKMGR_PLL0_GLOB,
		AGILEX27_CLKMGR_PLL1_GLOB,
		AGILEX27_CLKMGR_PLL2_GLOB,
	};
	static const u32 stages[] = {
		AGILEX27_PLL_GLOB_S1_CSR_EN,
		AGILEX27_PLL_GLOB_S2_BIAS_ON,
		AGILEX27_PLL_GLOB_S3_FRZREG_OFF,
		AGILEX27_PLL_GLOB_S4_IOTRI_ON,
		AGILEX27_PLL_GLOB_S5_FBLVDS_ON,
	};
	size_t state, p;

	pr_debug("agilex27-clkmgr: PLL enable (5 stages x 3 PLLs)\n");
	for (state = 0; state < ARRAY_SIZE(stages); state++) {
		for (p = 0; p < ARRAY_SIZE(pll_globs); p++)
			agilex27_writel(stages[state], pll_globs[p]);

		if (state == 1 || state == 2) {
			pr_debug("agilex27-clkmgr: PLL stage %zu->%zu udelay(%u)\n",
				 state + 1, state + 2, AGILEX27_PLL_STAGE_DELAY_US);
			udelay(AGILEX27_PLL_STAGE_DELAY_US);
		}
	}
}

#define AGILEX27_PLL_LOCK_TIMEOUT_MS		50

#define AGILEX27_RATE_AUDIT_TOL_PPM		100

static bool agilex27_rate_hz_match(u64 expect, u64 got)
{
	u64 delta;

	if (!expect || !got)
		return false;

	delta = (expect > got) ? (expect - got) : (got - expect);
	return delta * 1000000ULL <= expect * AGILEX27_RATE_AUDIT_TOL_PPM;
}

static void agilex27_rate_audit_print_hz(const char *tag, const char *pll,
					 const char *out, u64 expect, u64 got)
{
	const char *verdict = agilex27_rate_hz_match(expect, got) ?
			      "PASS" : "FAIL";

	printf("AGILEX27_RATE_AUDIT: %s %s expect %llu Hz CSR %llu Hz %s\n",
	       pll, out, expect, got, verdict);
}

int agilex27_clkmgr_audit_dv_preset_rates(bool check_clkouts)
{
	static const struct {
		const char *name;
		u32 pll_base;
		u64 expect_vco;
		u32 expect_c0_div, expect_c1_div, expect_c2_div, expect_c3_div;
		u64 expect_c0_hz, expect_c1_hz, expect_c2_hz, expect_c3_hz;
	} plls[] = {
		{
			"GPPLL0", AGILEX27_CLKPLL0_BASE,
			AGILEX27_GPPLL0_VCO_HZ,
			AGILEX27_GPPLL0_C0_DIV, AGILEX27_GPPLL0_C1_DIV,
			AGILEX27_GPPLL0_C2_DIV, AGILEX27_GPPLL0_C3_DIV,
			AGILEX27_GPPLL0_C0_HZ, AGILEX27_GPPLL0_C1_HZ,
			AGILEX27_GPPLL0_C2_HZ, AGILEX27_GPPLL0_C3_HZ,
		},
		{
			"GPPLL1", AGILEX27_CLKPLL1_BASE,
			AGILEX27_GPPLL1_VCO_HZ,
			AGILEX27_GPPLL1_C0_DIV, AGILEX27_GPPLL1_C1_DIV,
			0, 0,
			AGILEX27_GPPLL1_C0_HZ, AGILEX27_GPPLL1_C1_HZ,
			0, 0,
		},
		{
			"GPPLL2", AGILEX27_CLKPLL2_BASE,
			AGILEX27_GPPLL2_VCO_HZ,
			AGILEX27_GPPLL2_C0_DIV, AGILEX27_GPPLL2_C1_DIV,
			0, 0,
			AGILEX27_GPPLL2_C0_HZ, AGILEX27_GPPLL2_C1_HZ,
			0, 0,
		},
	};
	const struct agilex27_clkmgr_rate_state *state =
		agilex27_clkmgr_rate_state();
	u32 stat = agilex27_readl(AGILEX27_CLKMGR_STAT);
	int fails = 0;
	size_t i;

	printf("AGILEX27_RATE_AUDIT: DV SYSPRESET0 bin1 vs CSR MAS decode ");
	printf("(stat=0x%08x lock pll0=%d pll1=%d pll2=%d)\n",
	       stat,
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL0_LOCKED),
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL1_LOCKED),
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL2_LOCKED));

	if (!state || !state->valid) {
		printf("AGILEX27_RATE_AUDIT: VCO decode invalid — raw cfg CSRs:\n");
		for (i = 0; i < ARRAY_SIZE(plls); i++) {
			u32 cfg1 = agilex27_readl(plls[i].pll_base +
						      AGILEX27_GPPLL_CFG1_OFF);
			u32 cfg23 = agilex27_readl(plls[i].pll_base +
						     AGILEX27_GPPLL_CFG23_OFF);

			printf("  %s cfg1=0x%08x cfg23=0x%08x\n",
			       plls[i].name, cfg1, cfg23);
		}
		printf("AGILEX27_RATE_AUDIT: VERDICT FAIL (CSR VCO decode)\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		u32 cfg1 = agilex27_readl(plls[i].pll_base +
					      AGILEX27_GPPLL_CFG1_OFF);
		u32 cfg23 = agilex27_readl(plls[i].pll_base +
					     AGILEX27_GPPLL_CFG23_OFF);
		u64 vco;
		u64 c0_hz, c1_hz, c2_hz, c3_hz;

		printf("AGILEX27_RATE_AUDIT: %s cfg1=0x%08x cfg23=0x%08x\n",
		       plls[i].name, cfg1, cfg23);

		if (i == 0)
			vco = state->gppll0_vco_hz;
		else if (i == 1)
			vco = state->gppll1_vco_hz;
		else
			vco = state->gppll2_vco_hz;

		agilex27_rate_audit_print_hz("VCO", plls[i].name, "",
					     plls[i].expect_vco, vco);
		if (!agilex27_rate_hz_match(plls[i].expect_vco, vco))
			fails++;

		if (!check_clkouts)
			continue;

		if (i == 0) {
			c0_hz = vco / (state->gppll0_c0_div ? state->gppll0_c0_div : 1);
			c1_hz = vco / (state->gppll0_c1_div ? state->gppll0_c1_div : 1);
			c2_hz = vco / (state->gppll0_c2_div ? state->gppll0_c2_div : 1);
			c3_hz = vco / (state->gppll0_c3_div ? state->gppll0_c3_div : 1);
			printf("AGILEX27_RATE_AUDIT: GPPLL0 C div KV %u/%u/%u/%u expect %u/%u/%u/%u\n",
			       state->gppll0_c0_div, state->gppll0_c1_div,
			       state->gppll0_c2_div, state->gppll0_c3_div,
			       plls[i].expect_c0_div, plls[i].expect_c1_div,
			       plls[i].expect_c2_div, plls[i].expect_c3_div);
			if (state->gppll0_c0_div != plls[i].expect_c0_div ||
			    state->gppll0_c1_div != plls[i].expect_c1_div ||
			    state->gppll0_c2_div != plls[i].expect_c2_div ||
			    state->gppll0_c3_div != plls[i].expect_c3_div) {
				printf("AGILEX27_RATE_AUDIT: GPPLL0 C-div KV FAIL\n");
				fails++;
			}
			agilex27_rate_audit_print_hz("C0", "GPPLL0", "",
						     plls[i].expect_c0_hz, c0_hz);
			agilex27_rate_audit_print_hz("C1", "GPPLL0", "",
						     plls[i].expect_c1_hz, c1_hz);
			agilex27_rate_audit_print_hz("C2", "GPPLL0", "",
						     plls[i].expect_c2_hz, c2_hz);
			agilex27_rate_audit_print_hz("C3", "GPPLL0", "",
						     plls[i].expect_c3_hz, c3_hz);
			if (!agilex27_rate_hz_match(plls[i].expect_c0_hz, c0_hz) ||
			    !agilex27_rate_hz_match(plls[i].expect_c1_hz, c1_hz) ||
			    !agilex27_rate_hz_match(plls[i].expect_c2_hz, c2_hz) ||
			    !agilex27_rate_hz_match(plls[i].expect_c3_hz, c3_hz))
				fails++;
		} else if (i == 1) {
			c0_hz = vco / (state->gppll1_c0_div ? state->gppll1_c0_div : 1);
			c1_hz = vco / (state->gppll1_c1_div ? state->gppll1_c1_div : 1);
			printf("AGILEX27_RATE_AUDIT: GPPLL1 C div KV %u/%u expect %u/%u\n",
			       state->gppll1_c0_div, state->gppll1_c1_div,
			       plls[i].expect_c0_div, plls[i].expect_c1_div);
			if (state->gppll1_c0_div != plls[i].expect_c0_div ||
			    state->gppll1_c1_div != plls[i].expect_c1_div) {
				printf("AGILEX27_RATE_AUDIT: GPPLL1 C-div KV FAIL\n");
				fails++;
			}
			agilex27_rate_audit_print_hz("C0", "GPPLL1", "",
						     plls[i].expect_c0_hz, c0_hz);
			agilex27_rate_audit_print_hz("C1", "GPPLL1", "",
						     plls[i].expect_c1_hz, c1_hz);
			if (!agilex27_rate_hz_match(plls[i].expect_c0_hz, c0_hz) ||
			    !agilex27_rate_hz_match(plls[i].expect_c1_hz, c1_hz))
				fails++;
		} else {
			c0_hz = vco / (state->gppll2_c0_div ? state->gppll2_c0_div : 1);
			c1_hz = vco / (state->gppll2_c1_div ? state->gppll2_c1_div : 1);
			printf("AGILEX27_RATE_AUDIT: GPPLL2 C div KV %u/%u expect %u/%u\n",
			       state->gppll2_c0_div, state->gppll2_c1_div,
			       plls[i].expect_c0_div, plls[i].expect_c1_div);
			if (state->gppll2_c0_div != plls[i].expect_c0_div ||
			    state->gppll2_c1_div != plls[i].expect_c1_div) {
				printf("AGILEX27_RATE_AUDIT: GPPLL2 C-div KV FAIL\n");
				fails++;
			}
			agilex27_rate_audit_print_hz("C0", "GPPLL2", "",
						     plls[i].expect_c0_hz, c0_hz);
			agilex27_rate_audit_print_hz("C1", "GPPLL2", "",
						     plls[i].expect_c1_hz, c1_hz);
			if (!agilex27_rate_hz_match(plls[i].expect_c0_hz, c0_hz) ||
			    !agilex27_rate_hz_match(plls[i].expect_c1_hz, c1_hz))
				fails++;
		}
	}

	printf("AGILEX27_RATE_AUDIT: VERDICT %s (%d mismatch)\n",
	       fails ? "FAIL" : "PASS", fails);

	return fails ? -EINVAL : 0;
}

int agilex27_pll_wait_lock(void)
{
	int err;

	agilex27_clkmgr_pll_locked = false;

	pr_debug("agilex27-clkmgr: PLL wait-for-lock (poll stat[8/10/12])\n");

#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
	{
		u32 stat = readl((const void *)(uintptr_t)AGILEX27_CLKMGR_STAT);

		printf("AGILEX27_MMIO: pll_wait_lock start stat=0x%08x (poll @0x%08x)\n",
		       stat, (u32)AGILEX27_CLKMGR_STAT);
	}
#endif

	err = wait_for_bit_le32((const void *)(uintptr_t)AGILEX27_CLKMGR_STAT,
				AGILEX27_CLKMGR_STAT_ALL_LOCKED, true,
				AGILEX27_PLL_LOCK_TIMEOUT_MS, false);
	if (err) {
		u32 stat = readl((const void *)(uintptr_t)AGILEX27_CLKMGR_STAT);

#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_MMIO_TRACE)
		agilex27_mmio_trace_read("pll_wait_lock timeout", AGILEX27_CLKMGR_STAT,
					 stat);
#endif
		pr_err("agilex27-clkmgr: PLL lock timeout, stat=0x%08x (locked: pll0=%d pll1=%d pll2=%d)\n",
		       stat,
		       !!(stat & AGILEX27_CLKMGR_STAT_PLL0_LOCKED),
		       !!(stat & AGILEX27_CLKMGR_STAT_PLL1_LOCKED),
		       !!(stat & AGILEX27_CLKMGR_STAT_PLL2_LOCKED));
		return -ETIMEDOUT;
	}

	agilex27_clkmgr_refresh_vco_from_csr();
	agilex27_clkmgr_refresh_c_from_csr();
	agilex27_clkmgr_pll_locked = true;

	return 0;
}

#define AGILEX27_CLKMGR_BUSY_SETTLE_US	1
#define AGILEX27_CLKMGR_BUSY_TIMEOUT_MS	20

void agilex27_disable_boot_clk_bypass(void)
{
	int err;

	pr_debug("agilex27-clkmgr: leave boot-clk bypass (CTRL=0, then poll BUSY)\n");
	agilex27_writel(0x00000000, AGILEX27_CLKMGR_CTRL);

	udelay(AGILEX27_CLKMGR_BUSY_SETTLE_US);

	err = wait_for_bit_le32((const void *)(uintptr_t)AGILEX27_CLKMGR_STAT,
				AGILEX27_CLKMGR_STAT_BUSY, false,
				AGILEX27_CLKMGR_BUSY_TIMEOUT_MS, false);
	if (err) {
		u32 stat = readl((const void *)(uintptr_t)AGILEX27_CLKMGR_STAT);

		pr_err("agilex27-clkmgr: BUSY did not clear after BOOTMODE exit, stat=0x%08x\n",
		       stat);
		hang();
	}

	clrbits_le32((void *)(uintptr_t)(SOCFPGA_CLKMGR_ADDRESS +
					 AGILEX27_CLKMGR_EXTCNTRST),
		     AGILEX27_EXTCNTRST_CPU_RELEASE);
	clrbits_le32((void *)(uintptr_t)(SOCFPGA_CLKMGR_ADDRESS +
					 AGILEX27_CLKMGR_PERICTL_EXTCNTRST),
		     AGILEX27_PERICTL_EXTCNTRST_RELEASE);
}

void agilex27_clkmgr_virtual_platform_minimal_init(void)
{
	agilex27_clkmgr_secure_unlock();
	agilex27_disable_boot_clk_bypass();
	agilex27_clkmgr_refresh_rates_from_csr_if_locked();
}

/*
 * Simics / EMU: GPLLs arrive pre-programmed and locked. Derive
 * rate_state from CSR readback without preset or pll_enable MMIO.
 */
int agilex27_clkmgr_refresh_rates_from_csr_if_locked(void)
{
	u32 stat = agilex27_readl(AGILEX27_CLKMGR_STAT);

	if (IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)) {
		/*
		 * Simics VP: VCO from cfg_1/cfg_23 only. Skip
		 * refresh_c_from_csr() — non-VCOPH crsel can hang().
		 * C dividers remain at compile-time defaults in rate_state.
		 */
		agilex27_clkmgr_refresh_vco_from_csr();
		if (!agilex27_rate_state.valid) {
			printf("clkmgr: GPPLL VCO CSR decode failed (stat=0x%08x)\n", stat);
			return -EINVAL;
		}
		agilex27_clkmgr_pll_locked = true;
	} else if ((stat & AGILEX27_CLKMGR_STAT_ALL_LOCKED) ==
		   AGILEX27_CLKMGR_STAT_ALL_LOCKED) {
		agilex27_clkmgr_refresh_vco_from_csr();
		agilex27_clkmgr_refresh_c_from_csr();
		agilex27_clkmgr_pll_locked = true;
	} else {
		return agilex27_pll_wait_lock();
	}

	pr_info("agilex27-clkmgr: rates from CSR (no GPPLL reprogram)\n");
	pr_info("  GPPLL0 VCO %llu Hz  C %u/%u/%u/%u\n",
		agilex27_rate_state.gppll0_vco_hz,
		agilex27_rate_state.gppll0_c0_div,
		agilex27_rate_state.gppll0_c1_div,
		agilex27_rate_state.gppll0_c2_div,
		agilex27_rate_state.gppll0_c3_div);
	pr_info("  GPPLL1 VCO %llu Hz  C %u/%u\n",
		agilex27_rate_state.gppll1_vco_hz,
		agilex27_rate_state.gppll1_c0_div,
		agilex27_rate_state.gppll1_c1_div);
	pr_info("  GPPLL2 VCO %llu Hz  C %u/%u\n",
		agilex27_rate_state.gppll2_vco_hz,
		agilex27_rate_state.gppll2_c0_div,
		agilex27_rate_state.gppll2_c1_div);

	return 0;
}

void agilex27_clkmgr_print_rate_state(void)
{
	const struct agilex27_clkmgr_rate_state *state = agilex27_clkmgr_rate_state();
	u32 stat = agilex27_readl(AGILEX27_CLKMGR_STAT);
	u64 hz;

	printf("clkmgr: CLKMGR stat=0x%08x (pll0=%d pll1=%d pll2=%d)\n",
	       stat,
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL0_LOCKED),
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL1_LOCKED),
	       !!(stat & AGILEX27_CLKMGR_STAT_PLL2_LOCKED));

	if (!state || !state->valid) {
		static const struct {
			u32 base;
			const char *name;
		} plls[] = {
			{ AGILEX27_CLKPLL0_BASE, "GPPLL0" },
			{ AGILEX27_CLKPLL1_BASE, "GPPLL1" },
			{ AGILEX27_CLKPLL2_BASE, "GPPLL2" },
		};
		size_t i;

		printf("GPPLL CSR rates: VCO decode failed — raw CSRs:\n");
		for (i = 0; i < ARRAY_SIZE(plls); i++) {
			u32 cfg1 = agilex27_readl(plls[i].base + AGILEX27_GPPLL_CFG1_OFF);
			u32 cfg23 = agilex27_readl(plls[i].base + AGILEX27_GPPLL_CFG23_OFF);

			printf("  %s cfg1=0x%08x cfg23=0x%08x\n", plls[i].name, cfg1, cfg23);
		}
		return;
	}

	printf("GPPLL CSR rates (Simics/VP readback, no reprogram):\n");
	printf("  GPPLL0 VCO %llu kHz  C %u/%u/%u/%u\n",
	       state->gppll0_vco_hz / 1000,
	       state->gppll0_c0_div, state->gppll0_c1_div,
	       state->gppll0_c2_div, state->gppll0_c3_div);
	hz = state->gppll0_vco_hz / (state->gppll0_c0_div ? state->gppll0_c0_div : 1);
	printf("    C0 %llu kHz  C1 %llu kHz  C2 %llu kHz  C3 %llu kHz\n",
	       hz / 1000,
	       (state->gppll0_vco_hz / (state->gppll0_c1_div ? state->gppll0_c1_div : 1)) / 1000,
	       (state->gppll0_vco_hz / (state->gppll0_c2_div ? state->gppll0_c2_div : 1)) / 1000,
	       (state->gppll0_vco_hz / (state->gppll0_c3_div ? state->gppll0_c3_div : 1)) / 1000);
	printf("  GPPLL1 VCO %llu kHz  C %u/%u\n",
	       state->gppll1_vco_hz / 1000,
	       state->gppll1_c0_div, state->gppll1_c1_div);
	hz = state->gppll1_vco_hz / (state->gppll1_c0_div ? state->gppll1_c0_div : 1);
	printf("    comp0/DSU parent %llu kHz / %llu kHz\n",
	       hz / 1000,
	       (state->gppll1_vco_hz / (state->gppll1_c1_div ? state->gppll1_c1_div : 1)) / 1000);
	printf("  GPPLL2 VCO %llu kHz  C %u/%u\n",
	       state->gppll2_vco_hz / 1000,
	       state->gppll2_c0_div, state->gppll2_c1_div);
	hz = state->gppll2_vco_hz / (state->gppll2_c0_div ? state->gppll2_c0_div : 1);
	printf("    core2/3 parent %llu kHz / %llu kHz\n",
	       hz / 1000,
	       (state->gppll2_vco_hz / (state->gppll2_c1_div ? state->gppll2_c1_div : 1)) / 1000);
}

void agilex27_config_main_pll(unsigned long freq)
{
	pr_debug("agilex27-clkmgr: main_pll_freq=%lu Hz (REG_ABS-driven)\n", freq);
}

void agilex27_config_periph_pll(unsigned long freq)
{
	pr_debug("agilex27-clkmgr: peripheral_pll_freq=%lu Hz (REG_ABS-driven)\n",
		 freq);
}
