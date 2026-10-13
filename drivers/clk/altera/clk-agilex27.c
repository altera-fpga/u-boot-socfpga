// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 */

#include <config.h>
#include <log.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <errno.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/arch/clock_manager.h>
#include <clk-uclass.h>
#include <dt-bindings/clock/altr,agilex27-clkmgr.h>

#include "agilex27-clkmgr.h"
#include "clk-agilex27.h"

DECLARE_GLOBAL_DATA_PTR;

static u64 agilex27_vco_hz(int idx)
{
	const struct agilex27_clkmgr_rate_state *state;
	static const u64 fallback[AGILEX27_GPPLL_IDX_MAX] = {
		AGILEX27_GPPLL0_VCO_HZ,
		AGILEX27_GPPLL1_VCO_HZ,
		AGILEX27_GPPLL2_VCO_HZ,
	};

	if (idx < 0 || idx >= AGILEX27_GPPLL_IDX_MAX)
		return 0;

	state = agilex27_clkmgr_rate_state();
	if (!state || !state->valid)
		return fallback[idx];

	if (idx == AGILEX27_GPPLL0_IDX)
		return state->gppll0_vco_hz;
	if (idx == AGILEX27_GPPLL1_IDX)
		return state->gppll1_vco_hz;

	return state->gppll2_vco_hz;
}

enum {
	AGILEX27_GPPLL0_C0_IDX,
	AGILEX27_GPPLL0_C1_IDX,
	AGILEX27_GPPLL0_C2_IDX,
	AGILEX27_GPPLL0_C3_IDX,
	AGILEX27_GPPLL1_C0_IDX,
	AGILEX27_GPPLL1_C1_IDX,
	AGILEX27_GPPLL2_C0_IDX,
	AGILEX27_GPPLL2_C1_IDX,
	AGILEX27_GPPLL_C_IDX_MAX,
};

static u32 agilex27_c_div(int idx)
{
	const struct agilex27_clkmgr_rate_state *state;
	static const u32 fallback[AGILEX27_GPPLL_C_IDX_MAX] = {
		AGILEX27_GPPLL0_C0_DIV,
		AGILEX27_GPPLL0_C1_DIV,
		AGILEX27_GPPLL0_C2_DIV,
		AGILEX27_GPPLL0_C3_DIV,
		AGILEX27_GPPLL1_C0_DIV,
		AGILEX27_GPPLL1_C1_DIV,
		AGILEX27_GPPLL2_C0_DIV,
		AGILEX27_GPPLL2_C1_DIV,
	};
	u32 slot = 0;

	if (idx < 0 || idx >= AGILEX27_GPPLL_C_IDX_MAX)
		return 1;

	state = agilex27_clkmgr_rate_state();
	if (!state)
		return fallback[idx];

	switch (idx) {
	case AGILEX27_GPPLL0_C0_IDX:
		slot = state->gppll0_c0_div;
		break;
	case AGILEX27_GPPLL0_C1_IDX:
		slot = state->gppll0_c1_div;
		break;
	case AGILEX27_GPPLL0_C2_IDX:
		slot = state->gppll0_c2_div;
		break;
	case AGILEX27_GPPLL0_C3_IDX:
		slot = state->gppll0_c3_div;
		break;
	case AGILEX27_GPPLL1_C0_IDX:
		slot = state->gppll1_c0_div;
		break;
	case AGILEX27_GPPLL1_C1_IDX:
		slot = state->gppll1_c1_div;
		break;
	case AGILEX27_GPPLL2_C0_IDX:
		slot = state->gppll2_c0_div;
		break;
	case AGILEX27_GPPLL2_C1_IDX:
		slot = state->gppll2_c1_div;
		break;
	}

	return slot ? slot : fallback[idx];
}

static u64 agilex27_gppll0_c0_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / agilex27_c_div(AGILEX27_GPPLL0_C0_IDX);
}

static u64 agilex27_gppll0_c1_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / agilex27_c_div(AGILEX27_GPPLL0_C1_IDX);
}

static u64 agilex27_gppll0_c2_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / agilex27_c_div(AGILEX27_GPPLL0_C2_IDX);
}

static u64 agilex27_gppll0_c3_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / agilex27_c_div(AGILEX27_GPPLL0_C3_IDX);
}

static u64 agilex27_gppll0_c4_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / AGILEX27_GPPLL0_C4_DIV;
}

static u64 agilex27_gppll0_c5_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / AGILEX27_GPPLL0_C5_DIV;
}

static u64 agilex27_gppll0_c6_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL0_IDX) / AGILEX27_GPPLL0_C6_DIV;
}

static u64 agilex27_gppll1_c0_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL1_IDX) / agilex27_c_div(AGILEX27_GPPLL1_C0_IDX);
}

static u64 agilex27_gppll1_c1_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL1_IDX) / agilex27_c_div(AGILEX27_GPPLL1_C1_IDX);
}

static u64 agilex27_gppll2_c0_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL2_IDX) / agilex27_c_div(AGILEX27_GPPLL2_C0_IDX);
}

static u64 agilex27_gppll2_c1_hz(void)
{
	return agilex27_vco_hz(AGILEX27_GPPLL2_IDX) / agilex27_c_div(AGILEX27_GPPLL2_C1_IDX);
}

struct socfpga_clk_plat {
	void __iomem *regs;
	u32 osc1_hz;
	u32 f2s_hz;
};

static u32 clkmgr_free_ctr_div(struct socfpga_clk_plat *plat, u32 off)
{
	u32 value = CM_REG_READL(plat, off) & AGILEX27_CLKMGR_FREE_CTR_CNT_MASK;

	return value + 1;
}

static u32 clkmgr_gpiodbclk_div(struct socfpga_clk_plat *plat)
{
	u32 value = CM_REG_READL(plat, AGILEX27_CLKMGR_GPIODIV) &
		    AGILEX27_CLKMGR_GPIODIV_GPIODBCLK_MASK;

	return value + 1;
}

static u32 clkmgr_nocdiv_field_div(struct socfpga_clk_plat *plat, u32 reg, u32 shift)
{
	u32 value = CM_REG_READL(plat, reg) >> shift;

	value &= GENMASK(AGILEX27_CLKMGR_NOCDIV_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_nocdiv_div(struct socfpga_clk_plat *plat, u32 shift)
{
	return clkmgr_nocdiv_field_div(plat, AGILEX27_CLKMGR_NOCDIV, shift);
}

static u32 clkmgr_otherdiv_div(struct socfpga_clk_plat *plat, u32 shift)
{
	u32 value = CM_REG_READL(plat, AGILEX27_CLKMGR_OTHERDIV) >> shift;

	value &= GENMASK(AGILEX27_CLKMGR_NOCDIV_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_peripctr_div(struct socfpga_clk_plat *plat, u32 shift)
{
	u32 value = CM_REG_READL(plat, AGILEX27_CLKMGR_PERIPCTR) >> shift;

	value &= GENMASK(AGILEX27_CLKMGR_PERIPCTR_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_mainpll_apu_sysfreeclk_div(struct socfpga_clk_plat *plat)
{
	return clkmgr_nocdiv_field_div(plat, AGILEX27_CLKMGR_MAINPLL_NOCDIV,
				       AGILEX27_CLKMGR_MAINPLL_NOCDIV_APU_SYSFREECLK_SHIFT);
}

static bool clkmgr_csr_is_trusted(void)
{
	const struct agilex27_clkmgr_rate_state *state = agilex27_clkmgr_rate_state();

	return state && state->valid;
}

static bool clkmgr_in_bootmode(struct socfpga_clk_plat *plat)
{
	return !!(CM_REG_READL(plat, CLKMGR_STAT) & CLKMGR_STAT_BOOTMODE);
}

static u32 clkmgr_boot_clk_hz(struct socfpga_clk_plat *plat)
{
	u32 stat = CM_REG_READL(plat, CLKMGR_STAT);

	if (stat & CLKMGR_STAT_BOOTCLKSRC)
		return AGILEX27_INTOSC_DIV2_HZ;

	return plat->osc1_hz;
}

static u64 clkmgr_ctr_src_parent_hz(struct socfpga_clk_plat *plat, u32 ctr_off,
				    u64 pll_parent_hz)
{
	u32 src;

	if (!clkmgr_csr_is_trusted())
		return pll_parent_hz;

	src = (CM_REG_READL(plat, ctr_off) & AGILEX27_CLKMGR_FREE_CTR_SRC_MASK)
		>> AGILEX27_CLKMGR_FREE_CTR_SRC_SHIFT;

	switch (src) {
	case 0:
		return pll_parent_hz;
	case 1:
		return plat->osc1_hz;
	case 2:
		return AGILEX27_INTOSC_DIV2_HZ;
	case 3:
		return plat->f2s_hz;
	default:
		return pll_parent_hz;
	}
}

static u32 clkmgr_ctr_effective_div(struct socfpga_clk_plat *plat, u32 ctr_off,
				    u32 cntrst_reg, u32 cntrst_bit,
				    u32 cntrst_hold_div)
{
	if (CM_REG_READL(plat, cntrst_reg) & cntrst_bit) {
		if (cntrst_hold_div)
			return cntrst_hold_div;
		return 1;
	}

	return clkmgr_free_ctr_div(plat, ctr_off);
}

struct agilex27_gated_desc {
	u32 ctr_off;
	u32 en_bit;
	u32 bypass_bit;
	u32 cntrst_bit;
	u32 cntrst_hold_div;
	u64 (*pll_parent_hz)(void);
	u32 fallback_hz;
};

static u32 clkmgr_gated_ctr_rate(struct socfpga_clk_plat *plat,
				 const struct agilex27_gated_desc *d,
				 bool check_gate)
{
	u64 parent;
	u32 div;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (check_gate &&
	    !(CM_REG_READL(plat, AGILEX27_CLKMGR_MAINPLL_EN) & d->en_bit))
		return 0;

	if (CM_REG_READL(plat, AGILEX27_CLKMGR_MAINPLL_BYPASS) & d->bypass_bit)
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return d->fallback_hz;

	parent = clkmgr_ctr_src_parent_hz(plat, d->ctr_off, d->pll_parent_hz());
	div = clkmgr_ctr_effective_div(plat, d->ctr_off,
				       AGILEX27_CLKMGR_EXTCNTRST,
				       d->cntrst_bit, d->cntrst_hold_div);

	return (u32)(parent / div);
}

static u32 clkmgr_perip_ctr_rate(struct socfpga_clk_plat *plat, u32 ctr_off,
				 u32 bypass_bit, u32 cntrst_bit,
				 u32 cntrst_hold_div, u64 pll_parent_hz,
				 u32 fallback_hz)
{
	u64 parent;
	u32 div;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (CM_REG_READL(plat, AGILEX27_CLKMGR_PERIPLL_BYPASS) & bypass_bit)
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return fallback_hz;

	parent = clkmgr_ctr_src_parent_hz(plat, ctr_off, pll_parent_hz);
	div = clkmgr_ctr_effective_div(plat, ctr_off,
				       AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
				       cntrst_bit, cntrst_hold_div);

	return (u32)(parent / div);
}

static const struct agilex27_gated_desc desc_comp0 = {
	.ctr_off = AGILEX27_CLKMGR_COMP0_CTR,
	.en_bit = AGILEX27_MAINPLL_EN_COMP0,
	.bypass_bit = AGILEX27_MAINPLL_BYPASS_COMP0,
	.cntrst_bit = AGILEX27_EXTCNTRST_COMP0,
	.cntrst_hold_div = AGILEX27_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex27_gppll1_c0_hz,
	.fallback_hz = AGILEX27_MPU_HZ,
};

static const struct agilex27_gated_desc desc_core2 = {
	.ctr_off = AGILEX27_CLKMGR_CORE2_CTR,
	.en_bit = AGILEX27_MAINPLL_EN_CORE2,
	.bypass_bit = AGILEX27_MAINPLL_BYPASS_CORE2,
	.cntrst_bit = AGILEX27_EXTCNTRST_CORE2,
	.cntrst_hold_div = AGILEX27_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex27_gppll2_c0_hz,
	.fallback_hz = AGILEX27_CORE_HZ,
};

static const struct agilex27_gated_desc desc_core3 = {
	.ctr_off = AGILEX27_CLKMGR_CORE3_CTR,
	.en_bit = AGILEX27_MAINPLL_EN_CORE3,
	.bypass_bit = AGILEX27_MAINPLL_BYPASS_CORE3,
	.cntrst_bit = AGILEX27_EXTCNTRST_CORE3,
	.cntrst_hold_div = AGILEX27_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex27_gppll2_c1_hz,
	.fallback_hz = AGILEX27_CORE_HZ,
};

static const struct agilex27_gated_desc desc_dsu = {
	.ctr_off = AGILEX27_CLKMGR_DSU_CTR,
	.en_bit = AGILEX27_MAINPLL_EN_DSU,
	.bypass_bit = AGILEX27_MAINPLL_BYPASS_DSU,
	.cntrst_bit = AGILEX27_EXTCNTRST_DSU,
	.cntrst_hold_div = AGILEX27_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex27_gppll1_c1_hz,
	.fallback_hz = AGILEX27_GPPLL1_C1_HZ,
};

static const struct agilex27_gated_desc desc_ccu = {
	.ctr_off = AGILEX27_CLKMGR_CCU_FREE_CTR,
	.en_bit = AGILEX27_MAINPLL_EN_CCU,
	.bypass_bit = AGILEX27_MAINPLL_BYPASS_CCU,
	.cntrst_bit = AGILEX27_EXTCNTRST_CCU,
	.cntrst_hold_div = AGILEX27_CNTRST_CCU_DIV,
	.pll_parent_hz = agilex27_gppll0_c0_hz,
	.fallback_hz = AGILEX27_CCU_FREE_HZ,
};

static u32 clk_get_ccu_free_clk_hz(struct socfpga_clk_plat *plat)
{
	return clkmgr_gated_ctr_rate(plat, &desc_ccu, false);
}

static u32 clk_get_lsp_main_clk_hz(struct socfpga_clk_plat *plat)
{
	u32 div;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_LSP_MAIN_HZ;

	div = clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
				       AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
				       AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
				       AGILEX27_CNTRST_HOLD_DIV_NONE);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      agilex27_gppll0_c1_hz()) / div);
}

static u32 clk_get_lsp_sp_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base = agilex27_gppll0_c1_hz();
	u32 div_lspnoc, div_lspsp;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_LSP_SP_HZ;

	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
					      AGILEX27_CNTRST_HOLD_DIV_NONE);
	div_lspsp = clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_LSPSP_SHIFT);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      base) / div_lspnoc / div_lspsp);
}

static u32 clk_get_lsp_mp_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base = agilex27_gppll0_c1_hz();
	u32 div_lspnoc, div_lspmp;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_LSP_MP_HZ;

	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
					      AGILEX27_CNTRST_HOLD_DIV_NONE);
	div_lspmp = clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_LSPMP_SHIFT);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      base) / div_lspnoc / div_lspmp);
}

static u32 clk_get_sdmmc_clk_hz(struct socfpga_clk_plat *plat, u32 peripctr_shift)
{
	u32 rate;

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_SDMMC_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_MEMDEVPHY_FREE_CTR,
				     AGILEX27_PERIPLL_BYPASS_MEMDEVPHY,
				     AGILEX27_PERICTL_EXTCNTRST_MEMDEVPHY,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c3_hz(),
				     AGILEX27_MEMPHY_HZ);
	return rate / clkmgr_peripctr_div(plat, peripctr_shift);
}

static u32 clk_get_hsp_mp_clk_hz(struct socfpga_clk_plat *plat)
{
	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_HSP_MP_HZ;

	return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
				     AGILEX27_PERIPLL_BYPASS_NOC,
				     AGILEX27_PERICTL_EXTCNTRST_NONE,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c0_hz(),
				     AGILEX27_HSP_MAIN_HZ) /
	       clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_HSPMP_SHIFT);
}

static u32 clk_get_xspi_phy_clk_hz(struct socfpga_clk_plat *plat)
{
	u32 rate;

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_XSPIPHY_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_XSPIPHY_FREE_CTR,
				     AGILEX27_PERIPLL_BYPASS_XSPIPHY,
				     AGILEX27_PERICTL_EXTCNTRST_XSPIPHY,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c3_hz(),
				     AGILEX27_XSPIPHY_HZ);

	return rate / clkmgr_peripctr_div(plat, AGILEX27_CLKMGR_PERIPCTR_XSPIPHY_SHIFT);
}

static bool clkmgr_perip_gate_enabled(struct socfpga_clk_plat *plat, u32 en_bit)
{
	return !!(CM_REG_READL(plat, AGILEX27_CLKMGR_PERIPLL_EN) & en_bit);
}

static bool clkmgr_perip_ennoc_gate_enabled(struct socfpga_clk_plat *plat, u32 en_bit)
{
	return !!(CM_REG_READL(plat, AGILEX27_CLKMGR_PERIPLL_ENNOC) & en_bit);
}

static u32 clkmgr_perip_gated_rate(struct socfpga_clk_plat *plat, u32 rate,
				   u32 en_bit, bool check_gate)
{
	if (check_gate && rate && !clkmgr_perip_gate_enabled(plat, en_bit))
		return 0;

	return rate;
}

static u32 clkmgr_ennoc_gated_rate(struct socfpga_clk_plat *plat, u32 rate,
				   u32 en_bit, bool check_gate)
{
	if (check_gate && rate && !clkmgr_perip_ennoc_gate_enabled(plat, en_bit))
		return 0;

	return rate;
}

static u32 clk_get_emaca_div_hz(struct socfpga_clk_plat *plat)
{
	u32 rate;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_EMAC_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACA_CTR,
				     AGILEX27_PERIPLL_BYPASS_EMACA,
				     AGILEX27_PERICTL_EXTCNTRST_EMACA,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c0_hz(),
				     AGILEX27_EMAC_HZ);

	return rate / clkmgr_otherdiv_div(plat, AGILEX27_OTHERDIV_EMACA_SHIFT);
}

static u32 clk_get_emacb_div_hz(struct socfpga_clk_plat *plat)
{
	u32 rate;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_EMACB_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACB_CTR,
				     AGILEX27_PERIPLL_BYPASS_EMACB,
				     AGILEX27_PERICTL_EXTCNTRST_EMACB,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c0_hz(),
				     AGILEX27_EMACB_HZ);

	return rate / clkmgr_otherdiv_div(plat, AGILEX27_OTHERDIV_EMACB_SHIFT);
}

static u32 clk_get_emac_data_hz(struct socfpga_clk_plat *plat, u32 emac_idx)
{
	u32 ctl = CM_REG_READL(plat, AGILEX27_CLKMGR_EMACCTL);
	u32 shift = AGILEX27_EMACCTL_EMAC0SEL_SHIFT + emac_idx;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if ((ctl >> shift) & 1)
		return clk_get_emacb_div_hz(plat);

	return clk_get_emaca_div_hz(plat);
}

static u32 clk_get_s2f_user_hz(struct socfpga_clk_plat *plat, u32 ctr_off,
			       u32 bypass_bit, u32 cntrst_bit, u32 en_bit,
			       bool check_gate)
{
	u32 rate;

	rate = clkmgr_perip_ctr_rate(plat, ctr_off, bypass_bit, cntrst_bit,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
				     agilex27_gppll0_c1_hz(),
				     AGILEX27_S2F_USER_HZ);

	return clkmgr_perip_gated_rate(plat, rate, en_bit, check_gate);
}

static u32 clk_get_lsp_sys_free_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base;
	u32 div_lspnoc, div_lspsys;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted())
		return (u32)AGILEX27_LSP_SYS_HZ;

	base = clkmgr_ctr_src_parent_hz(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					agilex27_gppll0_c1_hz());
	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
					      AGILEX27_CNTRST_HOLD_DIV_NONE);
	div_lspsys = clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_LSPSYS_SHIFT);
	return (u32)(base / div_lspnoc / div_lspsys);
}

static u32 clk_get_emac_clk_hz(struct socfpga_clk_plat *plat, u32 emac_id)
{
	u32 rate;

	if (emac_id == AGILEX27_EMAC_PTP_CLK) {
		if (!clkmgr_csr_is_trusted())
			return (u32)AGILEX27_EMAC_PTP_HZ;

		rate = clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACPTP_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_EMACPTP,
					     AGILEX27_PERICTL_EXTCNTRST_EMACPTP,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_EMAC_PTP_HZ);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX27_PERIPLL_EN_EMACPTP, true);
	}

	if (emac_id == AGILEX27_EMACA_DIV_CLK)
		return clk_get_emaca_div_hz(plat);
	if (emac_id == AGILEX27_EMACB_DIV_CLK)
		return clk_get_emacb_div_hz(plat);

	switch (emac_id) {
	case AGILEX27_EMAC0_CLK:
		rate = clk_get_emac_data_hz(plat, 0);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX27_PERIPLL_EN_EMAC0, true);
	case AGILEX27_EMAC1_CLK:
		rate = clk_get_emac_data_hz(plat, 1);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX27_PERIPLL_EN_EMAC1, true);
	case AGILEX27_EMAC2_CLK:
		rate = clk_get_emac_data_hz(plat, 2);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX27_PERIPLL_EN_EMAC2, true);
	default:
		return (u32)AGILEX27_EMAC_HZ;
	}
}

static ulong socfpga_clk_get_rate(struct clk *clk)
{
	struct socfpga_clk_plat *plat = dev_get_plat(clk->dev);

	switch (clk->id) {
	case AGILEX27_OSC1:
		return plat->osc1_hz;
	case AGILEX27_CB_INTOSC_DIV2_CLK:
		return AGILEX27_INTOSC_DIV2_HZ;
	case AGILEX27_CB_INTOSC_DIV10_CLK:
		return AGILEX27_INTOSC_DIV10_HZ;
	case AGILEX27_F2S_FREE_CLK:
		return plat->f2s_hz;
	case AGILEX27_BOOT_CLK:
		return clkmgr_boot_clk_hz(plat);

	case AGILEX27_GPPLL0_CLK:
		return agilex27_vco_hz(AGILEX27_GPPLL0_IDX);
	case AGILEX27_GPPLL1_CLK:
		return agilex27_vco_hz(AGILEX27_GPPLL1_IDX);
	case AGILEX27_GPPLL2_CLK:
		return agilex27_vco_hz(AGILEX27_GPPLL2_IDX);

	case AGILEX27_HSP_NOC_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_MAIN_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
				     AGILEX27_PERICTL_EXTCNTRST_NONE,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_HSP_MAIN_HZ);
	case AGILEX27_LSP_NOC_FREE_CLK:
		return clk_get_lsp_main_clk_hz(plat);

	case AGILEX27_MPU_CLK:
	case AGILEX27_COMP0_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_comp0, true);
	case AGILEX27_COMP0_FREE_CLK:
	case AGILEX27_GPPLL1_C0_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_comp0, false);
	case AGILEX27_GPPLL1_C1_CLK:
		return agilex27_gppll1_c1_hz();
	case AGILEX27_DSU_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_dsu, false);
	case AGILEX27_CORE2_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core2, true);
	case AGILEX27_CORE2_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core2, false);
	case AGILEX27_CORE3_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core3, true);
	case AGILEX27_CORE3_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core3, false);
	case AGILEX27_GPPLL2_C0_CLK:
		return agilex27_gppll2_c0_hz();
	case AGILEX27_GPPLL2_C1_CLK:
		return agilex27_gppll2_c1_hz();

	/* LSP (Low-Speed Peripheral) NoC. */
	case AGILEX27_LSP_MAIN_FREE_CLK:
		return clk_get_lsp_main_clk_hz(plat);
	case AGILEX27_LSP_MAIN_CLK:
		return clkmgr_ennoc_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX27_PERIPLL_ENNOC_MAIN, true);
	case AGILEX27_LSP_SYS_FREE_CLK:
		return clk_get_lsp_sys_free_clk_hz(plat);
	case AGILEX27_LSP_MP_CLK:
		return clkmgr_ennoc_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_ENNOC_MP, true);
	case AGILEX27_XSPI_CLK:
	case AGILEX27_XSPI_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_xspi_phy_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_XSPI0, true);
	case AGILEX27_DMA_0_CORE_CLK:
	case AGILEX27_DMA_1_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       clk->id == AGILEX27_DMA_0_CORE_CLK ?
					       AGILEX27_PERIPLL_EN_DMA0 :
					       AGILEX27_PERIPLL_EN_DMA1, true);
	case AGILEX27_DMA_0_HS_CLK:
	case AGILEX27_DMA_1_HS_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       clk->id == AGILEX27_DMA_0_HS_CLK ?
					       AGILEX27_PERIPLL_EN_DMA0 :
					       AGILEX27_PERIPLL_EN_DMA1, true);
	case AGILEX27_I3C_0_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I3C0, true);
	case AGILEX27_I3C_1_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I3C1, true);
	case AGILEX27_LSP_SP_CLK:
		return clkmgr_ennoc_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_ENNOC_SP, true);
	case AGILEX27_SPIM_0_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPIM0, true);
	case AGILEX27_SPIM_1_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPIM1, true);
	case AGILEX27_SPIS_0_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPIS0, true);
	case AGILEX27_SPIS_1_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPIS1, true);
	case AGILEX27_I2C_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I2C0, true);
	case AGILEX27_I2C_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I2C1, true);
	case AGILEX27_I2C_EMAC0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I2C2, true);
	case AGILEX27_I2C_EMAC1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I2C3, true);
	case AGILEX27_I2C_EMAC2_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_I2C4, true);
	case AGILEX27_UART_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_UART0, true);
	case AGILEX27_UART_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_UART1, true);
	case AGILEX27_UART_2_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_UART2, true);
	case AGILEX27_SPTIMER_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPTIMER0, true);
	case AGILEX27_SPTIMER_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SPTIMER1, true);
	case AGILEX27_HSP_MAIN_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_MAIN_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
				     AGILEX27_PERICTL_EXTCNTRST_NONE,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_HSP_MAIN_HZ);
	case AGILEX27_HSP_MAIN_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_MAIN_HZ;
		return clkmgr_ennoc_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
					     AGILEX27_PERICTL_EXTCNTRST_NONE,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					      agilex27_gppll0_c0_hz(),
					      AGILEX27_HSP_MAIN_HZ),
			AGILEX27_PERIPLL_ENNOC_MAIN, true);
	case AGILEX27_USB31_BUS_CLK_EARLY:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_MAIN_HZ;
		return clkmgr_ennoc_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
					     AGILEX27_PERICTL_EXTCNTRST_NONE,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					      agilex27_gppll0_c0_hz(),
					      AGILEX27_HSP_MAIN_HZ),
			AGILEX27_PERIPLL_ENNOC_USB31, true);
	case AGILEX27_USB2OTG_HCLK:
		return clkmgr_ennoc_gated_rate(plat, clk_get_hsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_ENNOC_USB0, true);
	case AGILEX27_HSP_SYS_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_SYS_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
				     AGILEX27_PERICTL_EXTCNTRST_NONE,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_HSP_MAIN_HZ) /
		       clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_HSPSYS_SHIFT);
	case AGILEX27_HSP_MP_CLK:
		return clk_get_hsp_mp_clk_hz(plat);
	case AGILEX27_HSP_SP_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_HSP_SP_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_NOC_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_NOC,
				     AGILEX27_PERICTL_EXTCNTRST_NONE,
				     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_HSP_MAIN_HZ) /
		       clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_HSPSP_SHIFT);

	case AGILEX27_CCU_FREE_CLK:
		return clk_get_ccu_free_clk_hz(plat);
	case AGILEX27_CCU_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_ccu, true);
	case AGILEX27_APU_SYS_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_APU_SYS_FREE_HZ;
		return clk_get_ccu_free_clk_hz(plat) /
		       clkmgr_mainpll_apu_sysfreeclk_div(plat);

	case AGILEX27_SDMMC0_SDMCLK:
	case AGILEX27_SDMMC0_PHY_CLK:
		return clkmgr_perip_gated_rate(plat,
			clk_get_sdmmc_clk_hz(plat,
					     AGILEX27_CLKMGR_PERIPCTR_SDMMC0_SHIFT),
			AGILEX27_PERIPLL_EN_SDMMC0,
			clk->id == AGILEX27_SDMMC0_SDMCLK);
	case AGILEX27_SDMMC0_SDPHY_REG_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SDMMC0, false);
	case AGILEX27_SDMMC1_SDMCLK:
	case AGILEX27_SDMMC1_PHY_CLK:
		return clkmgr_perip_gated_rate(plat,
			clk_get_sdmmc_clk_hz(plat,
					     AGILEX27_CLKMGR_PERIPCTR_SDMMC1_SHIFT),
			AGILEX27_PERIPLL_EN_SDMMC1,
			clk->id == AGILEX27_SDMMC1_SDMCLK);
	case AGILEX27_SDMMC1_SDPHY_REG_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_SDMMC1, false);

	/* EMAC */
	case AGILEX27_EMAC_A_FREE_CLK:
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACA_CTR,
					     AGILEX27_PERIPLL_BYPASS_EMACA,
					     AGILEX27_PERICTL_EXTCNTRST_EMACA,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_EMAC_HZ);
	case AGILEX27_EMAC_B_FREE_CLK:
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACB_CTR,
					     AGILEX27_PERIPLL_BYPASS_EMACB,
					     AGILEX27_PERICTL_EXTCNTRST_EMACB,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_EMACB_HZ);
	case AGILEX27_EMAC_PTP_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return (u32)AGILEX27_EMAC_PTP_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_EMACPTP_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_EMACPTP,
					     AGILEX27_PERICTL_EXTCNTRST_EMACPTP,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_EMAC_PTP_HZ);
	case AGILEX27_EMAC0_CLK:
	case AGILEX27_EMAC1_CLK:
	case AGILEX27_EMAC2_CLK:
	case AGILEX27_EMAC_PTP_CLK:
	case AGILEX27_EMACA_DIV_CLK:
	case AGILEX27_EMACB_DIV_CLK:
		return clk_get_emac_clk_hz(plat, clk->id);

	case AGILEX27_USB31_SUSPEND_CLK:
	case AGILEX27_USB31_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_USB31_REF_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_USB31_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_USB31,
					     AGILEX27_PERICTL_EXTCNTRST_USB31,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c0_hz(),
					     AGILEX27_USB31_REF_HZ);

	case AGILEX27_MEMDEVICE_PHY_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_MEMPHY_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_MEMDEVPHY_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_MEMDEVPHY,
					     AGILEX27_PERICTL_EXTCNTRST_MEMDEVPHY,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c3_hz(),
					     AGILEX27_MEMPHY_HZ);
	case AGILEX27_XSPI_PHY_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_XSPIPHY_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_XSPIPHY_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_XSPIPHY,
					     AGILEX27_PERICTL_EXTCNTRST_XSPIPHY,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c3_hz(),
					     AGILEX27_XSPIPHY_HZ);
	case AGILEX27_XSPI_PHY_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_xspi_phy_clk_hz(plat),
					       AGILEX27_PERIPLL_EN_XSPI0_PHY, true);

	case AGILEX27_GPIO_DB_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_GPIO_DB_FREE_HZ;
		return (u32)(clkmgr_ctr_src_parent_hz(plat,
						      AGILEX27_CLKMGR_GPIODB_FREE_CTR,
						      agilex27_gppll0_c1_hz()) /
			     clkmgr_free_ctr_div(plat,
						 AGILEX27_CLKMGR_GPIODB_FREE_CTR));
	case AGILEX27_GPIO_DB_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_GPIO_DB_HZ;
		if (!clkmgr_perip_gate_enabled(plat, AGILEX27_PERIPLL_EN_GPIODB))
			return 0;
		return (u32)(clkmgr_ctr_src_parent_hz(plat,
						      AGILEX27_CLKMGR_GPIODB_FREE_CTR,
						      agilex27_gppll0_c1_hz()) /
			     clkmgr_free_ctr_div(plat,
						 AGILEX27_CLKMGR_GPIODB_FREE_CTR) /
			     clkmgr_gpiodbclk_div(plat));

	case AGILEX27_CS_AT_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_CS_AT_HZ;
		return clkmgr_ennoc_gated_rate(plat,
			(u32)(clkmgr_ctr_src_parent_hz(plat,
				AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
				agilex27_gppll0_c1_hz()) /
			clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
						 AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
						 AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
						 AGILEX27_CNTRST_HOLD_DIV_NONE) /
			clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_CSAT_SHIFT)),
			AGILEX27_PERIPLL_ENNOC_CS, true);
	case AGILEX27_CS_PDBG_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_CS_PDBG_HZ;
		return clkmgr_ennoc_gated_rate(plat,
			(u32)(clkmgr_ctr_src_parent_hz(plat,
				AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
				agilex27_gppll0_c1_hz()) /
			clkmgr_ctr_effective_div(plat, AGILEX27_CLKMGR_LSPNOC_FREE_CTR,
						 AGILEX27_CLKMGR_PERICTL_EXTCNTRST,
						 AGILEX27_PERICTL_EXTCNTRST_LSPNOC,
						 AGILEX27_CNTRST_HOLD_DIV_NONE) /
			clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_CSPDBG_SHIFT)),
			AGILEX27_PERIPLL_ENNOC_CS, true);
	case AGILEX27_TRACE_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_CS_TRACE_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_TRACE_FREE_CTR,
					     AGILEX27_PERIPLL_BYPASS_TRACE,
					     AGILEX27_PERICTL_EXTCNTRST_TRACE,
					     AGILEX27_CNTRST_HOLD_DIV_NONE,
					     agilex27_gppll0_c2_hz(),
					     AGILEX27_CS_TRACE_HZ);
	case AGILEX27_CS_TRACE_CLK:
		if (!clkmgr_csr_is_trusted())
			return AGILEX27_CS_TRACE_HZ;
		return clkmgr_ennoc_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX27_CLKMGR_TRACE_FREE_CTR,
					      AGILEX27_PERIPLL_BYPASS_TRACE,
					      AGILEX27_PERICTL_EXTCNTRST_TRACE,
					      AGILEX27_CNTRST_HOLD_DIV_NONE,
					      agilex27_gppll0_c2_hz(),
					      AGILEX27_CS_TRACE_HZ) /
			clkmgr_nocdiv_div(plat, AGILEX27_CLKMGR_NOCDIV_CSTRACE_SHIFT),
			AGILEX27_PERIPLL_ENNOC_CS, true);

	/* SoC-to-FPGA user clocks */
	case AGILEX27_S2F_USER0_FREE_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX27_CLKMGR_S2FUSER0_CTR,
					 AGILEX27_PERIPLL_BYPASS_S2FUSER0,
					 AGILEX27_PERICTL_EXTCNTRST_S2FUSER0,
					 AGILEX27_PERIPLL_EN_S2FUSER0, false);
	case AGILEX27_S2F_USER0_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX27_CLKMGR_S2FUSER0_CTR,
					 AGILEX27_PERIPLL_BYPASS_S2FUSER0,
					 AGILEX27_PERICTL_EXTCNTRST_S2FUSER0,
					 AGILEX27_PERIPLL_EN_S2FUSER0, true);
	case AGILEX27_S2F_USER1_FREE_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX27_CLKMGR_S2FUSER1_CTR,
					 AGILEX27_PERIPLL_BYPASS_S2FUSER1,
					 AGILEX27_PERICTL_EXTCNTRST_S2FUSER1,
					 AGILEX27_PERIPLL_EN_S2FUSER1, false);
	case AGILEX27_S2F_USER1_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX27_CLKMGR_S2FUSER1_CTR,
					 AGILEX27_PERIPLL_BYPASS_S2FUSER1,
					 AGILEX27_PERICTL_EXTCNTRST_S2FUSER1,
					 AGILEX27_PERIPLL_EN_S2FUSER1, true);

	/* GPPLL primary outputs (for diagnostic / introspection use) */
	case AGILEX27_GPPLL0_C0_CLK:
		return agilex27_gppll0_c0_hz();
	case AGILEX27_GPPLL0_C1_CLK:
		return agilex27_gppll0_c1_hz();
	case AGILEX27_GPPLL0_C2_CLK:
		return agilex27_gppll0_c2_hz();
	case AGILEX27_GPPLL0_C3_CLK:
		return agilex27_gppll0_c3_hz();
	case AGILEX27_GPPLL0_C4_CLK:
		return agilex27_gppll0_c4_hz();
	case AGILEX27_GPPLL0_C5_CLK:
		return agilex27_gppll0_c5_hz();
	case AGILEX27_GPPLL0_C6_CLK:
		return agilex27_gppll0_c6_hz();

	default:
		return 0;
	}
}

static int clkmgr_check_mainpll_gate(struct socfpga_clk_plat *plat, u32 en_bit)
{
	if (!(CM_REG_READL(plat, AGILEX27_CLKMGR_MAINPLL_EN) & en_bit))
		return -EIO;

	return 0;
}

static int clkmgr_check_perip_gate(struct socfpga_clk_plat *plat, u32 en_bit)
{
	if (!clkmgr_perip_gate_enabled(plat, en_bit))
		return -EIO;

	return 0;
}

static int clkmgr_check_ennoc_gate(struct socfpga_clk_plat *plat, u32 en_bit)
{
	if (!clkmgr_perip_ennoc_gate_enabled(plat, en_bit))
		return -EIO;

	return 0;
}

static int socfpga_clk_enable(struct clk *clk)
{
	struct socfpga_clk_plat *plat = dev_get_plat(clk->dev);

	switch (clk->id) {
	case AGILEX27_MPU_CLK:
	case AGILEX27_COMP0_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX27_MAINPLL_EN_COMP0);
	case AGILEX27_CORE2_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX27_MAINPLL_EN_CORE2);
	case AGILEX27_CORE3_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX27_MAINPLL_EN_CORE3);
	case AGILEX27_CCU_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX27_MAINPLL_EN_CCU);
	case AGILEX27_LSP_MAIN_CLK:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_MAIN);
	case AGILEX27_LSP_MP_CLK:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_MP);
	case AGILEX27_LSP_SP_CLK:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_SP);
	case AGILEX27_HSP_MAIN_CLK:
	case AGILEX27_USB31_BUS_CLK_EARLY:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_MAIN);
	case AGILEX27_USB2OTG_HCLK:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_USB0);
	case AGILEX27_EMAC0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_EMAC0);
	case AGILEX27_EMAC1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_EMAC1);
	case AGILEX27_EMAC2_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_EMAC2);
	case AGILEX27_EMAC_PTP_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_EMACPTP);
	case AGILEX27_GPIO_DB_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_GPIODB);
	case AGILEX27_SPIM_0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SPIM0);
	case AGILEX27_SPIM_1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SPIM1);
	case AGILEX27_SPIS_0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SPIS0);
	case AGILEX27_SPIS_1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SPIS1);
	case AGILEX27_DMA_0_CORE_CLK:
	case AGILEX27_DMA_0_HS_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_DMA0);
	case AGILEX27_DMA_1_CORE_CLK:
	case AGILEX27_DMA_1_HS_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_DMA1);
	case AGILEX27_XSPI_CLK:
	case AGILEX27_XSPI_PCLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_XSPI0);
	case AGILEX27_XSPI_PHY_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_XSPI0_PHY);
	case AGILEX27_SDMMC0_SDMCLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SDMMC0);
	case AGILEX27_SDMMC1_SDMCLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_SDMMC1);
	case AGILEX27_S2F_USER0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_S2FUSER0);
	case AGILEX27_S2F_USER1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX27_PERIPLL_EN_S2FUSER1);
	case AGILEX27_CS_AT_CLK:
	case AGILEX27_CS_PDBG_CLK:
	case AGILEX27_CS_TRACE_CLK:
		return clkmgr_check_ennoc_gate(plat, AGILEX27_PERIPLL_ENNOC_CS);
	default:
		return 0;
	}
}

static int socfpga_clk_probe(struct udevice *dev)
{
	int ret = agilex27_clkmgr_apply_handoff(NULL, 0);

	if (ret)
		debug("agilex27-clkmgr: handoff returned %d (continuing with fallback)\n",
		      ret);

	return 0;
}

static void clkmgr_read_fixed_input_rates(struct udevice *dev,
					  struct socfpga_clk_plat *plat)
{
	ofnode parent, node;

	plat->osc1_hz = AGILEX27_OSC1_HZ;
	plat->f2s_hz = AGILEX27_F2S_FREE_HZ;

	parent = ofnode_get_parent(dev_ofnode(dev));
	if (!ofnode_valid(parent))
		return;

	node = ofnode_find_subnode(parent, "osc1");
	if (ofnode_valid(node))
		ofnode_read_u32(node, "clock-frequency", &plat->osc1_hz);

	node = ofnode_find_subnode(parent, "f2s-free-clk");
	if (ofnode_valid(node))
		ofnode_read_u32(node, "clock-frequency", &plat->f2s_hz);
}

static int socfpga_clk_of_to_plat(struct udevice *dev)
{
	struct socfpga_clk_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;
	plat->regs = (void __iomem *)addr;
	clkmgr_read_fixed_input_rates(dev, plat);

	return 0;
}

static struct clk_ops socfpga_clk_ops = {
	.enable		= socfpga_clk_enable,
	.get_rate	= socfpga_clk_get_rate,
};

static const struct udevice_id socfpga_clk_match[] = {
	{ .compatible = "altr,agilex27-clkmgr" },
	{}
};

U_BOOT_DRIVER(socfpga_agilex27_clk) = {
	.name		= "clk-agilex27",
	.id		= UCLASS_CLK,
	.of_match	= socfpga_clk_match,
	.ops		= &socfpga_clk_ops,
	.probe		= socfpga_clk_probe,
	.of_to_plat = socfpga_clk_of_to_plat,
	.plat_auto	= sizeof(struct socfpga_clk_plat),
};
