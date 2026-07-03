// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 */

#include <init.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <hang.h>
#include <spl.h>
#include <asm/arch/base_addr_soc64.h>
#include <asm/arch/agilex72-handoff.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <wdt.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

/* EMU + AGILEX72_SOCDK without EMBED_DEMO — see board_init_f(). */
static bool spl_agilex72_clkmgr_vp_minimal(void)
{
	if (IS_ENABLED(CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO))
		return false;

	return IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU) &&
	       IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX72_SOCDK);
}

u32 reset_flag(u32 flag)
{
	/* Check rstmgr.stat for warm reset status */
	u32 status = readl(SOCFPGA_RSTMGR_ADDRESS);

	/* Check whether any L4 watchdogs or SDM had triggered warm reset */
	u32 warm_reset_mask = RSTMGR_L4WD_MPU_WARMRESET_MASK;

	if (status & warm_reset_mask)
		return 0;

	return 1;
}

static inline void enable_interleaving_normal_noncachable(void)
{
	u64 val;

	asm volatile("mrs	%0, S3_0_C15_C3_4\n"	/* Read CLUSTERECTLR */
		"orr	%0, %0, #1\n"		/* Set bit[0] */
		"msr	S3_0_C15_C3_4, %0\n"	/* Write back CLUSTERECTLR */
		"isb\n"				/* Synchronize context on sysreg write */
		: "=r"(val)
		:
		: "memory");
}

void board_init_f(ulong dummy)
{
	int ret;
	struct udevice *dev;

	/* Enable Async */
	asm volatile("msr daifclr, #4");

	enable_interleaving_normal_noncachable();

#ifdef CONFIG_SPL_BUILD
	spl_save_restore_data();
#endif

	ret = spl_early_init();
	if (ret)
		hang();

#if IS_ENABLED(CONFIG_AGILEX72_CLKMGR_MMIO_TRACE)
	/*
	 * MMIO_TRACE needs the console up before the CLKMGR handoff register
	 * writes so the trace is visible; the normal preloader_console_init()
	 * below runs after the clock driver probe. The demo handoff path
	 * unlocks the CLKMGR block itself, so this unlock is only for trace.
	 */
	agilex72_clkmgr_secure_unlock();
	preloader_console_init();
#endif

	socfpga_get_sys_mgr_addr();
	socfpga_get_managers_addr();

	if (!IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)) {
		/* Ensure watchdog is paused when debugging is happening */
		writel(SYSMGR_WDDBG_PAUSE_ALL_CPU,
		       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_WDDBG);
	}

	timer_init();

	/*
	 * Agilex 72 CLKMGR bring-up dispatch (first match wins):
	 *
	 *   0. spl_agilex72_clkmgr_vp_minimal(): EMU + AGILEX72_SOCDK without
	 *	embed demo — GPLLs pre-locked; unlock, boot-mode exit, and
	 *	CSR VCO decode only.
	 *
	 *   1. CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO: binary blob via
	 *	clk_mgr_init_from_blob() (REG_ABS + KV_STRING parser).
	 *
	 *   2. No producer enabled: hang().
	 */
	if (spl_agilex72_clkmgr_vp_minimal()) {
		agilex72_clkmgr_virtual_platform_minimal_init();
	} else if (IS_ENABLED(CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO)) {
		clk_mgr_init_from_blob();
	} else {
		printf("AGILEX72: no CLKMGR handoff producer configured\n");
		hang();
	}
	ret = uclass_get_device(UCLASS_CLK, 0, &dev);
	if (ret) {
		debug("Clock init failed: %d\n", ret);
		hang();
	}

	if (!IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)) {
		/*
		 * Enable watchdog as early as possible before initializing other
		 * component. Watchdog need to be enabled after clock driver because
		 * it will retrieve the clock frequency from clock driver.
		 */
		if (CONFIG_IS_ENABLED(WDT))
			initr_watchdog();
	}

	preloader_console_init();
	print_reset_info();

	if (IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU) &&
	    IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX72_SOCDK))
		agilex72_clkmgr_print_rate_state();

	cm_print_clock_quick_summary();

	/*
	 * Program the CCU master/region setup via the device tree.
	 */

	ret = uclass_get_device_by_name(UCLASS_NOP, "socfpga-ccu", &dev);
	if (ret) {
		printf("CCU settings init failed: %d\n", ret);
		hang();
	}

	if (IS_ENABLED(CONFIG_SPL_ALTERA_SDRAM)) {
		ret = uclass_get_device(UCLASS_RAM, 0, &dev);
		if (ret) {
			debug("DRAM init failed: %d\n", ret);
			hang();
		}
	}
}
