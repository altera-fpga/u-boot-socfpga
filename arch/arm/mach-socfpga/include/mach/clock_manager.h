/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *  Copyright (C) 2013-2025 Altera Corporation <www.altera.com>
 */

#ifndef _CLOCK_MANAGER_H_
#define _CLOCK_MANAGER_H_

#include <linux/types.h>

phys_addr_t socfpga_get_clkmgr_addr(void);

#ifndef __ASSEMBLY__
void cm_wait_for_lock(u32 mask);
int cm_wait_for_fsm(void);
void cm_print_clock_quick_summary(void);
#if IS_ENABLED(CONFIG_AGILEX27_CLKMGR_RUNTIME_AUDIT)
void cm_print_runtime_clock_tree(void);
int cm_audit_consumer_clock_rates(void);
int cm_audit_runtime_clock_trees(void);
#else
static inline int cm_audit_runtime_clock_trees(void)
{
	return 0;
}
#endif
unsigned long cm_get_mpu_clk_hz(void);
unsigned int cm_get_qspi_controller_clk_hz(void);

#if defined(CONFIG_ARCH_SOCFPGA_SOC64)
int cm_set_qspi_controller_clk_hz(u32 clk_hz);
#endif

/*
 * Agilex 27 Simics / CONFIG_TARGET_SOCFPGA_EMU: skip full GPPLL preset MMIO
 * (hangs the VP) and only unlock + exit boot mode. See spl_agilex27.c.
 */
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX27)
/*
 * Production handoff path (embedded demo blob or non-VP-minimal EMU): not the
 * Simics VP shortcut that skips full GPPLL preset programming.
 */
static inline bool agilex27_clkmgr_production_handoff_path(void)
{
	if (IS_ENABLED(CONFIG_AGILEX27_CLKMGR_HANDOFF_EMBED_DEMO))
		return true;

	return !(IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU) &&
		 IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX27_SOCDK));
}

void agilex27_clkmgr_virtual_platform_minimal_init(void);
int agilex27_clkmgr_refresh_rates_from_csr_if_locked(void);
void agilex27_clkmgr_print_rate_state(void);
void agilex27_clkmgr_secure_unlock(void);
#else
static inline void agilex27_clkmgr_virtual_platform_minimal_init(void)
{
}

static inline int agilex27_clkmgr_refresh_rates_from_csr_if_locked(void)
{
	return 0;
}

static inline void agilex27_clkmgr_print_rate_state(void)
{
}
#endif
#endif

#if defined(CONFIG_ARCH_SOCFPGA_GEN5)
#include <asm/arch/clock_manager_gen5.h>
#elif defined(CONFIG_ARCH_SOCFPGA_ARRIA10)
#include <asm/arch/clock_manager_arria10.h>
#elif defined(CONFIG_ARCH_SOCFPGA_STRATIX10)
#include <asm/arch/clock_manager_s10.h>
#elif IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX) || IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX7M)
#include <asm/arch/clock_manager_agilex.h>
#elif IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5)
#include <asm/arch/clock_manager_agilex5.h>
#elif IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX27)
#include <asm/arch/clock_manager_agilex27.h>
#elif IS_ENABLED(CONFIG_ARCH_SOCFPGA_N5X)
#include <asm/arch/clock_manager_n5x.h>
#endif

#endif /* _CLOCK_MANAGER_H_ */
