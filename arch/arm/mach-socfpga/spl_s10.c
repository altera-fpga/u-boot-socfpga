// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2023 Intel Corporation <www.intel.com>
 *
 */

#include <hang.h>
#include <init.h>
#include <log.h>
#include <asm/io.h>
#include <asm/utils.h>
#include <debug_uart.h>
#include <image.h>
#include <spl.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smmu_s10.h>
#include <asm/arch/system_manager.h>
#include <wdt.h>
#include <dm/uclass.h>

void board_init_f(ulong dummy)
{
	const struct cm_config *cm_default_cfg = cm_get_default_config();
	int ret;
	struct udevice *dev;

	ret = spl_early_init();
	if (ret)
		hang();

	socfpga_get_managers_addr();

	/* Ensure watchdog is paused when debugging is happening */
	writel(SYSMGR_WDDBG_PAUSE_ALL_CPU,
	       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_WDDBG);

	/*
	 * Enable watchdog as early as possible before initializing other
	 * component.
	 */
	if (CONFIG_IS_ENABLED(WDT))
		initr_watchdog();

	/* ensure all processors are not released prior Linux boot */
	writeq(0, CPU_RELEASE_ADDR);

	socfpga_per_reset(SOCFPGA_RESET(OSC1TIMER0), 0);
	timer_init();

	mbox_init();

	mbox_hps_stage_notify(HPS_EXECUTION_STATE_FSBL);

	sysmgr_pinmux_init();

	/* configuring the HPS clocks */
	cm_basic_init(cm_default_cfg);

#ifdef CONFIG_DEBUG_UART
	socfpga_per_reset(SOCFPGA_RESET(UART0), 0);
	debug_uart_init();
#endif

	preloader_console_init();
	print_reset_info();
	cm_print_clock_quick_summary();

	ret = uclass_get_device_by_name(UCLASS_NOP, "socfpga-system-mgr-firewall", &dev);
	if (ret) {
		printf("System manager firewall configuration failed: %d\n", ret);
		hang();
	}

	ret = uclass_get_device_by_name(UCLASS_NOP, "socfpga-l3interconnect-firewall", &dev);
	if (ret) {
		printf("L3 interconnect firewall configuration failed: %d\n", ret);
		hang();
	}

#if CONFIG_IS_ENABLED(ALTERA_SDRAM)
		ret = uclass_get_device(UCLASS_RAM, 0, &dev);
		if (ret) {
			debug("DRAM init failed: %d\n", ret);
			hang();
		}
#endif

#ifdef CONFIG_CADENCE_QSPI
	mbox_qspi_open();
#endif
}
