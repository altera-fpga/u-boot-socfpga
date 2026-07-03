/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2013-2017, 2025 Altera Corporation <www.altera.com>
 */

#ifndef _SYSTEM_MANAGER_H_
#define _SYSTEM_MANAGER_H_

#include <asm/types.h>

/*
 * socfpga_sysmgr_region:
 *
 * Identifiers for System Manager (SYS_MGR) register regions in SoCFPGA devices.
 *
 * Region 0 is shared between:
 *   - Legacy SoCs (single SYS_MGR base)
 *   - New SoCs (LS core region)
 *
 * Additional regions are used for new SoCs:
 *   - SYS_MGR_HS_CORE
 *   - SYS_MGR_APU_CORE
 *
 * SYS_MGR_LS_CORE is provided as an alias for region 0 on new SoCs.
 */
enum socfpga_sysmgr_region {
	/* Region 0 = legacy base or LS core on new SoCs */
	SYS_MGR_ROOT = 0,

	/* Additional regions for new SoCs */
	SYS_MGR_HS_CORE,
	SYS_MGR_APU_CORE,

	SYS_MGR_REGION_MAX
};

/* Alias for readability on new SoCs */
#define SYS_MGR_LS_CORE SYS_MGR_ROOT

phys_addr_t socfpga_get_sysmgr_addr(void);
phys_addr_t socfpga_get_sysmgr_addr_region(enum socfpga_sysmgr_region region);

#if defined(CONFIG_ARCH_SOCFPGA_SOC64)
#include <asm/io.h>
#include <asm/arch/system_manager_soc64.h>

/*
 * Unified System Manager access helpers.
 *
 * Four access styles are supported and may be freely mixed:
 *   1. Direct MMIO via socfpga_get_sysmgr_addr[_region]() + offset using
 *      writel()/readl()/clrsetbits_le32(). Bypasses DM; correct in SPL.
 *   2. Region-aware helpers sysmgr_{ls,hs,apu}_{read,write,update}() --
 *      alias-resolved ("sysmgr-{ls,hs,apu}") and dispatched through
 *      altr_sysmgr_ops. Use from fixed-region call sites.
 *   3. Device-explicit sysmgr_dev_{read,write,update}() for callers
 *      that resolve a sysmgr phandle themselves (e.g. EMAC drivers
 *      walking `altr,sysmgr-syscon`).
 *   4. Region-agnostic sysmgr_{read,write,update}() trampolines that
 *      resolve the "sysmgr" device-tree alias. Retained for back-compat
 *      with single-region SoC64 callers. On the multi-region AGILEX72 family
 *      they target whichever node owns the "sysmgr" alias (LS-core in
 *      the current AGILEX72 Device Tree), which is the right default but is
 *      a foot-gun for HS- or APU-region work; new AGILEX72-capable code
 *      should call (2) or (3) with an explicit region/device instead.
 *
 * In SPL (CONFIG_XPL_BUILD) every helper compiles to direct MMIO using
 * the address resolved at platform init. In U-Boot proper the helpers
 * go through altr_sysmgr_ops, which dispatches between direct MMIO at
 * EL3 and INTEL_SIP_SMC_REG_* SMCs at lower ELs.
 *
 * Returns 0 on success, negative errno on failure (SPL always returns 0).
 */

struct udevice;

#if defined(CONFIG_XPL_BUILD)

static inline int sysmgr_write(u32 offset, u32 value)
{
	writel(value, socfpga_get_sysmgr_addr() + offset);
	return 0;
}

static inline int sysmgr_read(u32 offset, u32 *value)
{
	*value = readl(socfpga_get_sysmgr_addr() + offset);
	return 0;
}

static inline int sysmgr_update(u32 offset, u32 mask, u32 value)
{
	clrsetbits_le32(socfpga_get_sysmgr_addr() + offset,
			mask, value & mask);
	return 0;
}

/*
 * Mirror the U-Boot-proper fall-back: when the requested region is not
 * present in the platform (every SoC except AGILEX72 has region count 1),
 * resolve to the single sysmgr block. socfpga_get_sysmgr_addr_region()
 * returns 0 for an out-of-range index, which we use as the cue.
 */
static inline phys_addr_t _sysmgr_region_base(enum socfpga_sysmgr_region region)
{
	phys_addr_t base = socfpga_get_sysmgr_addr_region(region);

	return base ? base : socfpga_get_sysmgr_addr();
}

#define _SYSMGR_REGION_INLINE(_name, _region)				\
static inline int sysmgr_##_name##_write(u32 offset, u32 value)		\
{									\
	writel(value, _sysmgr_region_base(_region) + offset);		\
	return 0;							\
}									\
static inline int sysmgr_##_name##_read(u32 offset, u32 *value)		\
{									\
	*value = readl(_sysmgr_region_base(_region) + offset);		\
	return 0;							\
}									\
static inline int sysmgr_##_name##_update(u32 offset, u32 mask, u32 value) \
{									\
	clrsetbits_le32(_sysmgr_region_base(_region) + offset,		\
			mask, value & mask);				\
	return 0;							\
}

_SYSMGR_REGION_INLINE(ls,  SYS_MGR_LS_CORE)
_SYSMGR_REGION_INLINE(hs,  SYS_MGR_HS_CORE)
_SYSMGR_REGION_INLINE(apu, SYS_MGR_APU_CORE)

#undef _SYSMGR_REGION_INLINE

#else /* !CONFIG_XPL_BUILD */

int sysmgr_write(u32 offset, u32 value);
int sysmgr_read(u32 offset, u32 *value);
int sysmgr_update(u32 offset, u32 mask, u32 value);

int sysmgr_ls_write(u32 offset, u32 value);
int sysmgr_ls_read(u32 offset, u32 *value);
int sysmgr_ls_update(u32 offset, u32 mask, u32 value);

int sysmgr_hs_write(u32 offset, u32 value);
int sysmgr_hs_read(u32 offset, u32 *value);
int sysmgr_hs_update(u32 offset, u32 mask, u32 value);

int sysmgr_apu_write(u32 offset, u32 value);
int sysmgr_apu_read(u32 offset, u32 *value);
int sysmgr_apu_update(u32 offset, u32 mask, u32 value);

int sysmgr_dev_write(struct udevice *dev, u32 offset, u32 value);
int sysmgr_dev_read(struct udevice *dev, u32 offset, u32 *value);
int sysmgr_dev_update(struct udevice *dev, u32 offset, u32 mask, u32 value);

#endif /* CONFIG_XPL_BUILD */

#else
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX	BIT(0)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO	BIT(1)
#define SYSMGR_ECC_OCRAM_EN	BIT(0)
#define SYSMGR_ECC_OCRAM_SERR	BIT(3)
#define SYSMGR_ECC_OCRAM_DERR	BIT(4)
#define SYSMGR_FPGAINTF_USEFPGA	0x1
#define SYSMGR_FPGAINTF_SPIM0	BIT(0)
#define SYSMGR_FPGAINTF_SPIM1	BIT(1)
#define SYSMGR_FPGAINTF_EMAC0	BIT(2)
#define SYSMGR_FPGAINTF_EMAC1	BIT(3)
#define SYSMGR_FPGAINTF_NAND	BIT(4)
#define SYSMGR_FPGAINTF_SDMMC	BIT(5)

#define SYSMGR_SDMMC_DRVSEL_SHIFT	0

/* EMAC Group Bit definitions */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII	0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII		0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII		0x2

#define SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB			0
#define SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB			2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK			0x3

/* For dedicated IO configuration */
/* Voltage select enums */
#define VOLTAGE_SEL_3V		0x0
#define VOLTAGE_SEL_1P8V	0x1
#define VOLTAGE_SEL_2P5V	0x2

/* Input buffer enable */
#define INPUT_BUF_DISABLE	0
#define INPUT_BUF_1P8V		1
#define INPUT_BUF_2P5V3V	2

/* Weak pull up enable */
#define WK_PU_DISABLE		0
#define WK_PU_ENABLE		1

/* Pull up slew rate control */
#define PU_SLW_RT_SLOW		0
#define PU_SLW_RT_FAST		1
#define PU_SLW_RT_DEFAULT	PU_SLW_RT_SLOW

/* Pull down slew rate control */
#define PD_SLW_RT_SLOW		0
#define PD_SLW_RT_FAST		1
#define PD_SLW_RT_DEFAULT	PD_SLW_RT_SLOW

/* Drive strength control */
#define PU_DRV_STRG_DEFAULT	0x10
#define PD_DRV_STRG_DEFAULT	0x10

/* bit position */
#define PD_DRV_STRG_LSB		0
#define PD_SLW_RT_LSB		5
#define PU_DRV_STRG_LSB		8
#define PU_SLW_RT_LSB		13
#define WK_PU_LSB		16
#define INPUT_BUF_LSB		17
#define BIAS_TRIM_LSB		19
#define VOLTAGE_SEL_LSB		0

#define ALT_SYSMGR_NOC_H2F_SET_MSK	BIT(0)
#define ALT_SYSMGR_NOC_LWH2F_SET_MSK	BIT(4)
#define ALT_SYSMGR_NOC_F2H_SET_MSK	BIT(8)
#define ALT_SYSMGR_NOC_F2SDR0_SET_MSK	BIT(16)
#define ALT_SYSMGR_NOC_F2SDR1_SET_MSK	BIT(20)
#define ALT_SYSMGR_NOC_F2SDR2_SET_MSK	BIT(24)
#define ALT_SYSMGR_NOC_TMO_EN_SET_MSK	BIT(0)

#define ALT_SYSMGR_ECC_INTSTAT_SERR_OCRAM_SET_MSK	BIT(1)
#define ALT_SYSMGR_ECC_INTSTAT_DERR_OCRAM_SET_MSK	BIT(1)

#if defined(CONFIG_ARCH_SOCFPGA_GEN5)
#include <asm/arch/system_manager_gen5.h>
#elif defined(CONFIG_ARCH_SOCFPGA_ARRIA10)
#include <asm/arch/system_manager_arria10.h>
#endif

#define SYSMGR_GET_BOOTINFO_BSEL(bsel)		\
		(((bsel) >> SYSMGR_BOOTINFO_BSEL_SHIFT) & 7)
#include <linux/bitops.h>
#endif
#endif /* _SYSTEM_MANAGER_H_ */
