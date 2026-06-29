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
#include <dt-bindings/clock/altr,agilex72-clkmgr.h>

#include "clk-agilex72.h"

DECLARE_GLOBAL_DATA_PTR;

static u64 agilex72_vco_hz(int idx)
{
	static const u64 fallback[AGILEX72_GPPLL_IDX_MAX] = {
		AGILEX72_GPPLL0_VCO_HZ,
		AGILEX72_GPPLL1_VCO_HZ,
		AGILEX72_GPPLL2_VCO_HZ,
	};

	if (idx < 0 || idx >= AGILEX72_GPPLL_IDX_MAX)
		return 0;

	return fallback[idx];
}

enum {
	AGILEX72_GPPLL0_C0_IDX,
	AGILEX72_GPPLL0_C1_IDX,
	AGILEX72_GPPLL0_C2_IDX,
	AGILEX72_GPPLL0_C3_IDX,
	AGILEX72_GPPLL1_C0_IDX,
	AGILEX72_GPPLL1_C1_IDX,
	AGILEX72_GPPLL2_C0_IDX,
	AGILEX72_GPPLL2_C1_IDX,
	AGILEX72_GPPLL_C_IDX_MAX,
};

static u32 agilex72_c_div(int idx)
{
	static const u32 fallback[AGILEX72_GPPLL_C_IDX_MAX] = {
		AGILEX72_GPPLL0_C0_DIV,
		AGILEX72_GPPLL0_C1_DIV,
		AGILEX72_GPPLL0_C2_DIV,
		AGILEX72_GPPLL0_C3_DIV,
		AGILEX72_GPPLL1_C0_DIV,
		AGILEX72_GPPLL1_C1_DIV,
		AGILEX72_GPPLL2_C0_DIV,
		AGILEX72_GPPLL2_C1_DIV,
	};

	if (idx < 0 || idx >= AGILEX72_GPPLL_C_IDX_MAX)
		return 1;

	return fallback[idx];
}

static u64 agilex72_gppll0_c0_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / agilex72_c_div(AGILEX72_GPPLL0_C0_IDX);
}

static u64 agilex72_gppll0_c1_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / agilex72_c_div(AGILEX72_GPPLL0_C1_IDX);
}

static u64 agilex72_gppll0_c2_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / agilex72_c_div(AGILEX72_GPPLL0_C2_IDX);
}

static u64 agilex72_gppll0_c3_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / agilex72_c_div(AGILEX72_GPPLL0_C3_IDX);
}

static u64 agilex72_gppll0_c4_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / AGILEX72_GPPLL0_C4_DIV;
}

static u64 agilex72_gppll0_c5_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / AGILEX72_GPPLL0_C5_DIV;
}

static u64 agilex72_gppll0_c6_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL0_IDX) / AGILEX72_GPPLL0_C6_DIV;
}

static u64 agilex72_gppll1_c0_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL1_IDX) / agilex72_c_div(AGILEX72_GPPLL1_C0_IDX);
}

static u64 agilex72_gppll1_c1_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL1_IDX) / agilex72_c_div(AGILEX72_GPPLL1_C1_IDX);
}

static u64 agilex72_gppll2_c0_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL2_IDX) / agilex72_c_div(AGILEX72_GPPLL2_C0_IDX);
}

static u64 agilex72_gppll2_c1_hz(void)
{
	return agilex72_vco_hz(AGILEX72_GPPLL2_IDX) / agilex72_c_div(AGILEX72_GPPLL2_C1_IDX);
}

struct socfpga_clk_plat {
	void __iomem *regs;
	u32 osc1_hz;
	u32 f2s_hz;
};

static u32 clkmgr_free_ctr_div(struct socfpga_clk_plat *plat, u32 off)
{
	u32 value = CM_REG_READL(plat, off) & AGILEX72_CLKMGR_FREE_CTR_CNT_MASK;

	return value + 1;
}

static u32 clkmgr_gpiodbclk_div(struct socfpga_clk_plat *plat)
{
	u32 value = CM_REG_READL(plat, AGILEX72_CLKMGR_GPIODIV) &
		    AGILEX72_CLKMGR_GPIODIV_GPIODBCLK_MASK;

	return value + 1;
}

static u32 clkmgr_nocdiv_field_div(struct socfpga_clk_plat *plat, u32 reg, u32 shift)
{
	u32 value = CM_REG_READL(plat, reg) >> shift;

	value &= GENMASK(AGILEX72_CLKMGR_NOCDIV_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_nocdiv_div(struct socfpga_clk_plat *plat, u32 shift)
{
	return clkmgr_nocdiv_field_div(plat, AGILEX72_CLKMGR_NOCDIV, shift);
}

static u32 clkmgr_otherdiv_div(struct socfpga_clk_plat *plat, u32 shift)
{
	u32 value = CM_REG_READL(plat, AGILEX72_CLKMGR_OTHERDIV) >> shift;

	value &= GENMASK(AGILEX72_CLKMGR_NOCDIV_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_peripctr_div(struct socfpga_clk_plat *plat, u32 shift)
{
	u32 value = CM_REG_READL(plat, AGILEX72_CLKMGR_PERIPCTR) >> shift;

	value &= GENMASK(AGILEX72_CLKMGR_PERIPCTR_WIDTH - 1, 0);
	return 1U << value;
}

static u32 clkmgr_mainpll_apu_sysfreeclk_div(struct socfpga_clk_plat *plat)
{
	return clkmgr_nocdiv_field_div(plat, AGILEX72_CLKMGR_MAINPLL_NOCDIV,
				       AGILEX72_CLKMGR_MAINPLL_NOCDIV_APU_SYSFREECLK_SHIFT);
}

static bool clkmgr_in_bootmode(struct socfpga_clk_plat *plat)
{
	return !!(CM_REG_READL(plat, CLKMGR_STAT) & CLKMGR_STAT_BOOTMODE);
}

static bool clkmgr_csr_is_trusted(struct socfpga_clk_plat *plat)
{
	return plat && !clkmgr_in_bootmode(plat);
}

static u32 clkmgr_boot_clk_hz(struct socfpga_clk_plat *plat)
{
	u32 stat = CM_REG_READL(plat, CLKMGR_STAT);

	if (stat & CLKMGR_STAT_BOOTCLKSRC)
		return AGILEX72_INTOSC_DIV2_HZ;

	return plat->osc1_hz;
}

static u64 clkmgr_ctr_src_parent_hz(struct socfpga_clk_plat *plat, u32 ctr_off,
				    u64 pll_parent_hz)
{
	u32 src;

	if (!clkmgr_csr_is_trusted(plat))
		return pll_parent_hz;

	src = (CM_REG_READL(plat, ctr_off) & AGILEX72_CLKMGR_FREE_CTR_SRC_MASK)
		>> AGILEX72_CLKMGR_FREE_CTR_SRC_SHIFT;

	switch (src) {
	case 0:
		return pll_parent_hz;
	case 1:
		return plat->osc1_hz;
	case 2:
		return AGILEX72_INTOSC_DIV2_HZ;
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

struct agilex72_gated_desc {
	u32 ctr_off;
	u32 en_bit;
	u32 bypass_bit;
	u32 cntrst_bit;
	u32 cntrst_hold_div;
	u64 (*pll_parent_hz)(void);
	u32 fallback_hz;
};

static u32 clkmgr_gated_ctr_rate(struct socfpga_clk_plat *plat,
				 const struct agilex72_gated_desc *d,
				 bool check_gate)
{
	u64 parent;
	u32 div;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (check_gate &&
	    !(CM_REG_READL(plat, AGILEX72_CLKMGR_MAINPLL_EN) & d->en_bit))
		return 0;

	if (CM_REG_READL(plat, AGILEX72_CLKMGR_MAINPLL_BYPASS) & d->bypass_bit)
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return d->fallback_hz;

	parent = clkmgr_ctr_src_parent_hz(plat, d->ctr_off, d->pll_parent_hz());
	div = clkmgr_ctr_effective_div(plat, d->ctr_off,
				       AGILEX72_CLKMGR_EXTCNTRST,
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

	if (CM_REG_READL(plat, AGILEX72_CLKMGR_PERIPLL_BYPASS) & bypass_bit)
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return fallback_hz;

	parent = clkmgr_ctr_src_parent_hz(plat, ctr_off, pll_parent_hz);
	div = clkmgr_ctr_effective_div(plat, ctr_off,
				       AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
				       cntrst_bit, cntrst_hold_div);

	return (u32)(parent / div);
}

static const struct agilex72_gated_desc desc_comp0 = {
	.ctr_off = AGILEX72_CLKMGR_COMP0_CTR,
	.en_bit = AGILEX72_MAINPLL_EN_COMP0,
	.bypass_bit = AGILEX72_MAINPLL_BYPASS_COMP0,
	.cntrst_bit = AGILEX72_EXTCNTRST_COMP0,
	.cntrst_hold_div = AGILEX72_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex72_gppll1_c0_hz,
	.fallback_hz = AGILEX72_MPU_HZ,
};

static const struct agilex72_gated_desc desc_core2 = {
	.ctr_off = AGILEX72_CLKMGR_CORE2_CTR,
	.en_bit = AGILEX72_MAINPLL_EN_CORE2,
	.bypass_bit = AGILEX72_MAINPLL_BYPASS_CORE2,
	.cntrst_bit = AGILEX72_EXTCNTRST_CORE2,
	.cntrst_hold_div = AGILEX72_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex72_gppll2_c0_hz,
	.fallback_hz = AGILEX72_CORE_HZ,
};

static const struct agilex72_gated_desc desc_core3 = {
	.ctr_off = AGILEX72_CLKMGR_CORE3_CTR,
	.en_bit = AGILEX72_MAINPLL_EN_CORE3,
	.bypass_bit = AGILEX72_MAINPLL_BYPASS_CORE3,
	.cntrst_bit = AGILEX72_EXTCNTRST_CORE3,
	.cntrst_hold_div = AGILEX72_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex72_gppll2_c1_hz,
	.fallback_hz = AGILEX72_CORE_HZ,
};

static const struct agilex72_gated_desc desc_dsu = {
	.ctr_off = AGILEX72_CLKMGR_DSU_CTR,
	.en_bit = AGILEX72_MAINPLL_EN_DSU,
	.bypass_bit = AGILEX72_MAINPLL_BYPASS_DSU,
	.cntrst_bit = AGILEX72_EXTCNTRST_DSU,
	.cntrst_hold_div = AGILEX72_CNTRST_CPU_DIV,
	.pll_parent_hz = agilex72_gppll1_c1_hz,
	.fallback_hz = AGILEX72_GPPLL1_C1_HZ,
};

static const struct agilex72_gated_desc desc_ccu = {
	.ctr_off = AGILEX72_CLKMGR_CCU_FREE_CTR,
	.en_bit = AGILEX72_MAINPLL_EN_CCU,
	.bypass_bit = AGILEX72_MAINPLL_BYPASS_CCU,
	.cntrst_bit = AGILEX72_EXTCNTRST_CCU,
	.cntrst_hold_div = AGILEX72_CNTRST_CCU_DIV,
	.pll_parent_hz = agilex72_gppll0_c0_hz,
	.fallback_hz = AGILEX72_CCU_FREE_HZ,
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

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_LSP_MAIN_HZ;

	div = clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
				       AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
				       AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      agilex72_gppll0_c1_hz()) / div);
}

static u32 clk_get_lsp_sp_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base = agilex72_gppll0_c1_hz();
	u32 div_lspnoc, div_lspsp;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_LSP_SP_HZ;

	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0);
	div_lspsp = clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_LSPSP_SHIFT);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      base) / div_lspnoc / div_lspsp);
}

static u32 clk_get_lsp_mp_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base = agilex72_gppll0_c1_hz();
	u32 div_lspnoc, div_lspmp;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_LSP_MP_HZ;

	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0);
	div_lspmp = clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_LSPMP_SHIFT);
	return (u32)(clkmgr_ctr_src_parent_hz(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      base) / div_lspnoc / div_lspmp);
}

static u32 clk_get_sdmmc_clk_hz(struct socfpga_clk_plat *plat, u32 peripctr_shift)
{
	u32 rate;

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_SDMMC_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_MEMDEVPHY_FREE_CTR,
				     AGILEX72_PERIPLL_BYPASS_MEMDEVPHY,
				     AGILEX72_PERICTL_EXTCNTRST_MEMDEVPHY, 0,
				     agilex72_gppll0_c3_hz(),
				     AGILEX72_MEMPHY_HZ);
	return rate / clkmgr_peripctr_div(plat, peripctr_shift);
}

static bool clkmgr_perip_gate_enabled(struct socfpga_clk_plat *plat, u32 en_bit)
{
	return !!(CM_REG_READL(plat, AGILEX72_CLKMGR_PERIPLL_EN) & en_bit);
}

static u32 clkmgr_perip_gated_rate(struct socfpga_clk_plat *plat, u32 rate,
				   u32 en_bit, bool check_gate)
{
	if (check_gate && rate && !clkmgr_perip_gate_enabled(plat, en_bit))
		return 0;

	return rate;
}

static u32 clk_get_emaca_div_hz(struct socfpga_clk_plat *plat)
{
	u32 rate;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_EMAC_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACA_CTR,
				     AGILEX72_PERIPLL_BYPASS_EMACA,
				     AGILEX72_PERICTL_EXTCNTRST_EMACA, 0,
				     agilex72_gppll0_c0_hz(),
				     AGILEX72_EMAC_HZ);

	return rate / clkmgr_otherdiv_div(plat, AGILEX72_OTHERDIV_EMACA_SHIFT);
}

static u32 clk_get_emacb_div_hz(struct socfpga_clk_plat *plat)
{
	u32 rate;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_EMACB_HZ;

	rate = clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACB_CTR,
				     AGILEX72_PERIPLL_BYPASS_EMACB,
				     AGILEX72_PERICTL_EXTCNTRST_EMACB, 0,
				     agilex72_gppll0_c0_hz(),
				     AGILEX72_EMACB_HZ);

	return rate / clkmgr_otherdiv_div(plat, AGILEX72_OTHERDIV_EMACB_SHIFT);
}

static u32 clk_get_emac_data_hz(struct socfpga_clk_plat *plat, u32 emac_idx)
{
	u32 ctl = CM_REG_READL(plat, AGILEX72_CLKMGR_EMACCTL);
	u32 shift = AGILEX72_EMACCTL_EMAC0SEL_SHIFT + emac_idx;

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

	rate = clkmgr_perip_ctr_rate(plat, ctr_off, bypass_bit, cntrst_bit, 0,
				     agilex72_gppll0_c1_hz(),
				     AGILEX72_S2F_USER_HZ);

	return clkmgr_perip_gated_rate(plat, rate, en_bit, check_gate);
}

static u32 clk_get_lsp_sys_free_clk_hz(struct socfpga_clk_plat *plat)
{
	u64 base;
	u32 div_lspnoc, div_lspsys;

	if (clkmgr_in_bootmode(plat))
		return clkmgr_boot_clk_hz(plat);

	if (!clkmgr_csr_is_trusted(plat))
		return (u32)AGILEX72_LSP_SYS_HZ;

	base = clkmgr_ctr_src_parent_hz(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					agilex72_gppll0_c1_hz());
	div_lspnoc = clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
					      AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
					      AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0);
	div_lspsys = clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_LSPSYS_SHIFT);
	return (u32)(base / div_lspnoc / div_lspsys);
}

static u32 clk_get_emac_clk_hz(struct socfpga_clk_plat *plat, u32 emac_id)
{
	u32 rate;

	if (emac_id == AGILEX72_EMAC_PTP_CLK) {
		if (!clkmgr_csr_is_trusted(plat))
			return (u32)AGILEX72_EMAC_PTP_HZ;

		rate = clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACPTP_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_EMACPTP,
					     AGILEX72_PERICTL_EXTCNTRST_EMACPTP, 0,
					    agilex72_gppll0_c0_hz(),
					    AGILEX72_EMAC_PTP_HZ);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX72_PERIPLL_EN_EMACPTP, true);
	}

	if (emac_id == AGILEX72_EMACA_DIV_CLK)
		return clk_get_emaca_div_hz(plat);
	if (emac_id == AGILEX72_EMACB_DIV_CLK)
		return clk_get_emacb_div_hz(plat);

	switch (emac_id) {
	case AGILEX72_EMAC0_CLK:
		rate = clk_get_emac_data_hz(plat, 0);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX72_PERIPLL_EN_EMAC0, true);
	case AGILEX72_EMAC1_CLK:
		rate = clk_get_emac_data_hz(plat, 1);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX72_PERIPLL_EN_EMAC1, true);
	case AGILEX72_EMAC2_CLK:
		rate = clk_get_emac_data_hz(plat, 2);
		return clkmgr_perip_gated_rate(plat, rate,
					       AGILEX72_PERIPLL_EN_EMAC2, true);
	default:
		return (u32)AGILEX72_EMAC_HZ;
	}
}

static ulong socfpga_clk_get_rate(struct clk *clk)
{
	struct socfpga_clk_plat *plat = dev_get_plat(clk->dev);

	switch (clk->id) {
	case AGILEX72_OSC1:
		return plat->osc1_hz;
	case AGILEX72_CB_INTOSC_DIV2_CLK:
		return AGILEX72_INTOSC_DIV2_HZ;
	case AGILEX72_CB_INTOSC_DIV10_CLK:
		return AGILEX72_INTOSC_DIV10_HZ;
	case AGILEX72_F2S_FREE_CLK:
		return plat->f2s_hz;
	case AGILEX72_BOOT_CLK:
		return clkmgr_boot_clk_hz(plat);

	case AGILEX72_GPPLL0_CLK:
		return agilex72_vco_hz(AGILEX72_GPPLL0_IDX);
	case AGILEX72_GPPLL1_CLK:
		return agilex72_vco_hz(AGILEX72_GPPLL1_IDX);
	case AGILEX72_GPPLL2_CLK:
		return agilex72_vco_hz(AGILEX72_GPPLL2_IDX);

	case AGILEX72_HSP_NOC_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MAIN_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_HSP_MAIN_HZ);
	case AGILEX72_LSP_NOC_FREE_CLK:
		return clk_get_lsp_main_clk_hz(plat);

	case AGILEX72_MPU_CLK:
	case AGILEX72_COMP0_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_comp0, true);
	case AGILEX72_COMP0_FREE_CLK:
	case AGILEX72_GPPLL1_C0_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_comp0, false);
	case AGILEX72_GPPLL1_C1_CLK:
		return agilex72_gppll1_c1_hz();
	case AGILEX72_DSU_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_dsu, false);
	case AGILEX72_CORE2_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core2, true);
	case AGILEX72_CORE2_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core2, false);
	case AGILEX72_CORE3_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core3, true);
	case AGILEX72_CORE3_FREE_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_core3, false);
	case AGILEX72_GPPLL2_C0_CLK:
		return agilex72_gppll2_c0_hz();
	case AGILEX72_GPPLL2_C1_CLK:
		return agilex72_gppll2_c1_hz();

	/* LSP (Low-Speed Peripheral) NoC. */
	case AGILEX72_LSP_MAIN_FREE_CLK:
		return clk_get_lsp_main_clk_hz(plat);
	case AGILEX72_LSP_MAIN_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_LSP_MAIN, true);
	case AGILEX72_LSP_SYS_FREE_CLK:
		return clk_get_lsp_sys_free_clk_hz(plat);
	case AGILEX72_LSP_MP_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_LSP_MP, true);
	case AGILEX72_XSPI_CLK:
	case AGILEX72_XSPI_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_XSPI0, true);
	case AGILEX72_DMA_0_CORE_CLK:
	case AGILEX72_DMA_1_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_main_clk_hz(plat),
					       clk->id == AGILEX72_DMA_0_CORE_CLK ?
					       AGILEX72_PERIPLL_EN_DMA0 :
					       AGILEX72_PERIPLL_EN_DMA1, true);
	case AGILEX72_DMA_0_HS_CLK:
	case AGILEX72_DMA_1_HS_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       clk->id == AGILEX72_DMA_0_HS_CLK ?
					       AGILEX72_PERIPLL_EN_DMA0 :
					       AGILEX72_PERIPLL_EN_DMA1, true);
	case AGILEX72_I3C_0_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I3C0, true);
	case AGILEX72_I3C_1_CORE_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_mp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I3C1, true);
	case AGILEX72_LSP_SP_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_LSP_SP, true);
	case AGILEX72_SPIM_0_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPIM0, true);
	case AGILEX72_SPIM_1_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPIM1, true);
	case AGILEX72_SPIS_0_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPIS0, true);
	case AGILEX72_SPIS_1_CLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPIS1, true);
	case AGILEX72_I2C_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I2C0, true);
	case AGILEX72_I2C_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I2C1, true);
	case AGILEX72_I2C_EMAC0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I2C2, true);
	case AGILEX72_I2C_EMAC1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I2C3, true);
	case AGILEX72_I2C_EMAC2_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_I2C4, true);
	case AGILEX72_UART_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_UART0, true);
	case AGILEX72_UART_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_UART1, true);
	case AGILEX72_UART_2_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_UART2, true);
	case AGILEX72_SPTIMER_0_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPTIMER0, true);
	case AGILEX72_SPTIMER_1_PCLK:
		return clkmgr_perip_gated_rate(plat, clk_get_lsp_sp_clk_hz(plat),
					       AGILEX72_PERIPLL_EN_SPTIMER1, true);
	case AGILEX72_HSP_MAIN_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MAIN_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_HSP_MAIN_HZ);
	case AGILEX72_HSP_MAIN_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MAIN_HZ;
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					      agilex72_gppll0_c0_hz(),
					      AGILEX72_HSP_MAIN_HZ),
			AGILEX72_PERIPLL_EN_LSP_MAIN, true);
	case AGILEX72_USB31_BUS_CLK_EARLY:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MAIN_HZ;
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					      agilex72_gppll0_c0_hz(),
					      AGILEX72_HSP_MAIN_HZ),
			AGILEX72_PERIPLL_EN_USB31, true);
	case AGILEX72_USB2OTG_HCLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MAIN_HZ;
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					      agilex72_gppll0_c0_hz(),
					      AGILEX72_HSP_MAIN_HZ),
			AGILEX72_PERIPLL_EN_USB0, true);
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					      agilex72_gppll0_c0_hz(),
					      AGILEX72_HSP_MAIN_HZ) /
			clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_HSPMP_SHIFT),
			AGILEX72_PERIPLL_EN_DMA1, true);
	case AGILEX72_HSP_SYS_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_SYS_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_HSP_MAIN_HZ) /
		       clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_HSPSYS_SHIFT);
	case AGILEX72_HSP_MP_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_MP_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_HSP_MAIN_HZ) /
		       clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_HSPMP_SHIFT);
	case AGILEX72_HSP_SP_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_HSP_SP_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_NOC_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_NOC, 0, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_HSP_MAIN_HZ) /
		       clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_HSPSP_SHIFT);

	case AGILEX72_CCU_FREE_CLK:
		return clk_get_ccu_free_clk_hz(plat);
	case AGILEX72_CCU_CLK:
		return clkmgr_gated_ctr_rate(plat, &desc_ccu, true);
	case AGILEX72_APU_SYS_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_APU_SYS_FREE_HZ;
		return clk_get_ccu_free_clk_hz(plat) /
		       clkmgr_mainpll_apu_sysfreeclk_div(plat);

	case AGILEX72_SDMMC0_SDMCLK:
	case AGILEX72_SDMMC0_PHY_CLK:
	case AGILEX72_SDMMC0_SDPHY_REG_CLK:
		return clkmgr_perip_gated_rate(plat,
			clk_get_sdmmc_clk_hz(plat,
					     AGILEX72_CLKMGR_PERIPCTR_SDMMC0_SHIFT),
			AGILEX72_PERIPLL_EN_SDMMC0,
			clk->id == AGILEX72_SDMMC0_SDMCLK);
	case AGILEX72_SDMMC1_SDMCLK:
	case AGILEX72_SDMMC1_PHY_CLK:
	case AGILEX72_SDMMC1_SDPHY_REG_CLK:
		return clkmgr_perip_gated_rate(plat,
			clk_get_sdmmc_clk_hz(plat,
					     AGILEX72_CLKMGR_PERIPCTR_SDMMC1_SHIFT),
			AGILEX72_PERIPLL_EN_SDMMC1,
			clk->id == AGILEX72_SDMMC1_SDMCLK);

	/* EMAC */
	case AGILEX72_EMAC_A_FREE_CLK:
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACA_CTR,
					     AGILEX72_PERIPLL_BYPASS_EMACA,
					     AGILEX72_PERICTL_EXTCNTRST_EMACA, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_EMAC_HZ);
	case AGILEX72_EMAC_B_FREE_CLK:
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACB_CTR,
					     AGILEX72_PERIPLL_BYPASS_EMACB,
					     AGILEX72_PERICTL_EXTCNTRST_EMACB, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_EMACB_HZ);
	case AGILEX72_EMAC_PTP_FREE_CLK:
		if (!clkmgr_csr_is_trusted())
			return (u32)AGILEX72_EMAC_PTP_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_EMACPTP_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_EMACPTP,
					     AGILEX72_PERICTL_EXTCNTRST_EMACPTP,
					     AGILEX72_CNTRST_HOLD_DIV_NONE,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_EMAC_PTP_HZ);
	case AGILEX72_EMAC0_CLK:
	case AGILEX72_EMAC1_CLK:
	case AGILEX72_EMAC2_CLK:
	case AGILEX72_EMAC_PTP_CLK:
	case AGILEX72_EMACA_DIV_CLK:
	case AGILEX72_EMACB_DIV_CLK:
		return clk_get_emac_clk_hz(plat, clk->id);

	case AGILEX72_USB31_SUSPEND_CLK:
	case AGILEX72_USB31_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_USB31_REF_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_USB31_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_USB31,
					     AGILEX72_PERICTL_EXTCNTRST_USB31, 0,
					     agilex72_gppll0_c0_hz(),
					     AGILEX72_USB31_REF_HZ);

	case AGILEX72_MEMDEVICE_PHY_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_MEMPHY_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_MEMDEVPHY_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_MEMDEVPHY,
					     AGILEX72_PERICTL_EXTCNTRST_MEMDEVPHY, 0,
					     agilex72_gppll0_c3_hz(),
					     AGILEX72_MEMPHY_HZ);
	case AGILEX72_XSPI_PHY_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_XSPIPHY_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_XSPIPHY_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_XSPIPHY,
					     AGILEX72_PERICTL_EXTCNTRST_XSPIPHY, 0,
					     agilex72_gppll0_c3_hz(),
					     AGILEX72_XSPIPHY_HZ);
	case AGILEX72_XSPI_PHY_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_XSPIPHY_HZ;
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_XSPIPHY_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_XSPIPHY,
					      AGILEX72_PERICTL_EXTCNTRST_XSPIPHY, 0,
					      agilex72_gppll0_c3_hz(),
					      AGILEX72_XSPIPHY_HZ) /
			clkmgr_peripctr_div(plat, AGILEX72_CLKMGR_PERIPCTR_XSPIPHY_SHIFT),
			AGILEX72_PERIPLL_EN_XSPI0, true);

	case AGILEX72_GPIO_DB_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_GPIO_DB_FREE_HZ;
		return (u32)(clkmgr_ctr_src_parent_hz(plat,
						      AGILEX72_CLKMGR_GPIODB_FREE_CTR,
						      agilex72_gppll0_c1_hz()) /
			     clkmgr_free_ctr_div(plat,
						 AGILEX72_CLKMGR_GPIODB_FREE_CTR));
	case AGILEX72_GPIO_DB_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_GPIO_DB_HZ;
		if (!clkmgr_perip_gate_enabled(plat, AGILEX72_PERIPLL_EN_GPIODB))
			return 0;
		return (u32)(clkmgr_ctr_src_parent_hz(plat,
						      AGILEX72_CLKMGR_GPIODB_FREE_CTR,
						      agilex72_gppll0_c1_hz()) /
			     clkmgr_free_ctr_div(plat,
						 AGILEX72_CLKMGR_GPIODB_FREE_CTR) /
			     clkmgr_gpiodbclk_div(plat));

	case AGILEX72_CS_AT_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_CS_AT_HZ;
		return clkmgr_perip_gated_rate(plat,
			(u32)(clkmgr_ctr_src_parent_hz(plat,
				AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
				agilex72_gppll0_c1_hz()) /
			clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
						 AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
						 AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0) /
			clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_CSAT_SHIFT)),
			AGILEX72_PERIPLL_EN_CS, true);
	case AGILEX72_CS_PDBG_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_CS_PDBG_HZ;
		return clkmgr_perip_gated_rate(plat,
			(u32)(clkmgr_ctr_src_parent_hz(plat,
				AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
				agilex72_gppll0_c1_hz()) /
			clkmgr_ctr_effective_div(plat, AGILEX72_CLKMGR_LSPNOC_FREE_CTR,
						 AGILEX72_CLKMGR_PERICTL_EXTCNTRST,
						 AGILEX72_PERICTL_EXTCNTRST_LSPNOC, 0) /
			clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_CSPDBG_SHIFT)),
			AGILEX72_PERIPLL_EN_CS, true);
	case AGILEX72_TRACE_FREE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_CS_TRACE_HZ;
		return clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_TRACE_FREE_CTR,
					     AGILEX72_PERIPLL_BYPASS_TRACE,
					     AGILEX72_PERICTL_EXTCNTRST_TRACE, 0,
					     agilex72_gppll0_c2_hz(),
					     AGILEX72_CS_TRACE_HZ);
	case AGILEX72_CS_TRACE_CLK:
		if (!clkmgr_csr_is_trusted(plat))
			return AGILEX72_CS_TRACE_HZ;
		return clkmgr_perip_gated_rate(plat,
			clkmgr_perip_ctr_rate(plat, AGILEX72_CLKMGR_TRACE_FREE_CTR,
					      AGILEX72_PERIPLL_BYPASS_TRACE,
					      AGILEX72_PERICTL_EXTCNTRST_TRACE, 0,
					      agilex72_gppll0_c2_hz(),
					      AGILEX72_CS_TRACE_HZ) /
			clkmgr_nocdiv_div(plat, AGILEX72_CLKMGR_NOCDIV_CSTRACE_SHIFT),
			AGILEX72_PERIPLL_EN_CS, true);

	/* SoC-to-FPGA user clocks */
	case AGILEX72_S2F_USER0_FREE_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX72_CLKMGR_S2FUSER0_CTR,
					 AGILEX72_PERIPLL_BYPASS_S2FUSER0,
					 AGILEX72_PERICTL_EXTCNTRST_S2FUSER0,
					 AGILEX72_PERIPLL_EN_S2FUSER0, false);
	case AGILEX72_S2F_USER0_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX72_CLKMGR_S2FUSER0_CTR,
					 AGILEX72_PERIPLL_BYPASS_S2FUSER0,
					 AGILEX72_PERICTL_EXTCNTRST_S2FUSER0,
					 AGILEX72_PERIPLL_EN_S2FUSER0, true);
	case AGILEX72_S2F_USER1_FREE_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX72_CLKMGR_S2FUSER1_CTR,
					 AGILEX72_PERIPLL_BYPASS_S2FUSER1,
					 AGILEX72_PERICTL_EXTCNTRST_S2FUSER1,
					 AGILEX72_PERIPLL_EN_S2FUSER1, false);
	case AGILEX72_S2F_USER1_CLK:
		return clk_get_s2f_user_hz(plat, AGILEX72_CLKMGR_S2FUSER1_CTR,
					 AGILEX72_PERIPLL_BYPASS_S2FUSER1,
					 AGILEX72_PERICTL_EXTCNTRST_S2FUSER1,
					 AGILEX72_PERIPLL_EN_S2FUSER1, true);

	/* GPPLL primary outputs (for diagnostic / introspection use) */
	case AGILEX72_GPPLL0_C0_CLK:
		return agilex72_gppll0_c0_hz();
	case AGILEX72_GPPLL0_C1_CLK:
		return agilex72_gppll0_c1_hz();
	case AGILEX72_GPPLL0_C2_CLK:
		return agilex72_gppll0_c2_hz();
	case AGILEX72_GPPLL0_C3_CLK:
		return agilex72_gppll0_c3_hz();
	case AGILEX72_GPPLL0_C4_CLK:
		return agilex72_gppll0_c4_hz();
	case AGILEX72_GPPLL0_C5_CLK:
		return agilex72_gppll0_c5_hz();
	case AGILEX72_GPPLL0_C6_CLK:
		return agilex72_gppll0_c6_hz();

	default:
		return -ENXIO;
	}
}

static int clkmgr_check_mainpll_gate(struct socfpga_clk_plat *plat, u32 en_bit)
{
	if (!(CM_REG_READL(plat, AGILEX72_CLKMGR_MAINPLL_EN) & en_bit))
		return -EIO;

	return 0;
}

static int clkmgr_check_perip_gate(struct socfpga_clk_plat *plat, u32 en_bit)
{
	if (!clkmgr_perip_gate_enabled(plat, en_bit))
		return -EIO;

	return 0;
}

static int socfpga_clk_enable(struct clk *clk)
{
	struct socfpga_clk_plat *plat = dev_get_plat(clk->dev);

	switch (clk->id) {
	case AGILEX72_MPU_CLK:
	case AGILEX72_COMP0_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX72_MAINPLL_EN_COMP0);
	case AGILEX72_CORE2_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX72_MAINPLL_EN_CORE2);
	case AGILEX72_CORE3_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX72_MAINPLL_EN_CORE3);
	case AGILEX72_CCU_CLK:
		return clkmgr_check_mainpll_gate(plat, AGILEX72_MAINPLL_EN_CCU);
	case AGILEX72_LSP_MAIN_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_LSP_MAIN);
	case AGILEX72_LSP_MP_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_LSP_MP);
	case AGILEX72_LSP_SP_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_LSP_SP);
	case AGILEX72_HSP_MAIN_CLK:
	case AGILEX72_USB31_BUS_CLK_EARLY:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_LSP_MAIN);
	case AGILEX72_USB2OTG_HCLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_USB0);
	case AGILEX72_EMAC0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_EMAC0);
	case AGILEX72_EMAC1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_EMAC1);
	case AGILEX72_EMAC2_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_EMAC2);
	case AGILEX72_EMAC_PTP_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_EMACPTP);
	case AGILEX72_GPIO_DB_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_GPIODB);
	case AGILEX72_SPIM_0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SPIM0);
	case AGILEX72_SPIM_1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SPIM1);
	case AGILEX72_SPIS_0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SPIS0);
	case AGILEX72_SPIS_1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SPIS1);
	case AGILEX72_DMA_0_CORE_CLK:
	case AGILEX72_DMA_0_HS_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_DMA0);
	case AGILEX72_DMA_1_CORE_CLK:
	case AGILEX72_DMA_1_HS_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_DMA1);
	case AGILEX72_XSPI_CLK:
	case AGILEX72_XSPI_PCLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_XSPI0);
	case AGILEX72_XSPI_PHY_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_XSPI0);
	case AGILEX72_SDMMC0_SDMCLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SDMMC0);
	case AGILEX72_SDMMC1_SDMCLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_SDMMC1);
	case AGILEX72_S2F_USER0_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_S2FUSER0);
	case AGILEX72_S2F_USER1_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_S2FUSER1);
	case AGILEX72_CS_AT_CLK:
	case AGILEX72_CS_PDBG_CLK:
	case AGILEX72_CS_TRACE_CLK:
		return clkmgr_check_perip_gate(plat, AGILEX72_PERIPLL_EN_CS);
	default:
		return 0;
	}
}

static int socfpga_clk_probe(struct udevice *dev)
{
	return 0;
}

static void clkmgr_read_fixed_input_rates(struct udevice *dev,
					  struct socfpga_clk_plat *plat)
{
	ofnode parent, node;

	plat->osc1_hz = AGILEX72_OSC1_HZ;
	plat->f2s_hz = AGILEX72_F2S_FREE_HZ;

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
	{ .compatible = "altr,agilex72-clkmgr" },
	{}
};

U_BOOT_DRIVER(socfpga_agilex72_clk) = {
	.name		= "clk-agilex72",
	.id		= UCLASS_CLK,
	.of_match	= socfpga_clk_match,
	.ops		= &socfpga_clk_ops,
	.probe		= socfpga_clk_probe,
	.of_to_plat = socfpga_clk_of_to_plat,
	.plat_auto	= sizeof(struct socfpga_clk_plat),
};
