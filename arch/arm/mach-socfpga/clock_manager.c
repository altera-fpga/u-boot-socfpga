// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2013-2017 Altera Corporation <www.altera.com>
 */

#include <asm/arch/clock_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <command.h>
#include <init.h>
#include <wait_bit.h>
#include <linux/printk.h>

DECLARE_GLOBAL_DATA_PTR;

void cm_wait_for_lock(u32 mask)
{
	u32 inter_val;
	u32 retry = 0;

	pr_info("%s : line %d => Is clock PLL locked? Checking ...\n",
		__FILE__, __LINE__);

	do {
#if defined(CONFIG_ARCH_SOCFPGA_GEN5)
		inter_val = readl(socfpga_get_clkmgr_addr() +
				  CLKMGR_INTER) & mask;
#else
		inter_val = readl(socfpga_get_clkmgr_addr() +
				  CLKMGR_STAT) & mask;
#endif
		/* Wait for stable lock */
		if (inter_val == mask)
			retry++;
		else
			retry = 0;
		if (retry >= 10)
			break;
	} while (1);

	pr_info("%s : line %d => Clock PLL is locked !\n",
		__FILE__, __LINE__);
}

/* function to poll in the fsm busy bit */
int cm_wait_for_fsm(void)
{
	return wait_for_bit_le32((const void *)(socfpga_get_clkmgr_addr() +
				 CLKMGR_STAT), CLKMGR_STAT_BUSY, false, 20000,
				 false);
}

int set_cpu_clk_info(void)
{
#if defined(CONFIG_ARCH_SOCFPGA_GEN5)
	/* Calculate the clock frequencies required for drivers */
	cm_get_l4_sp_clk_hz();
	cm_get_mmc_controller_clk_hz();
#endif

	gd->bd->bi_arm_freq = cm_get_mpu_clk_hz() / 1000000;
	gd->bd->bi_dsp_freq = 0;

#if defined(CONFIG_ARCH_SOCFPGA_GEN5)
	gd->bd->bi_ddr_freq = cm_get_sdram_clk_hz() / 1000000;
#else
	gd->bd->bi_ddr_freq = 0;
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_SOC64)
int cm_set_qspi_controller_clk_hz(u32 clk_hz)
{
	u32 reg;
	u32 clk_khz;

	/*
	 * Store QSPI ref clock and set into sysmgr boot register.
	 * Only clock freq in kHz degree is accepted due to limited bits[27:0]
	 * is reserved for storing the QSPI clock freq into boot scratch cold0
	 * register.
	 */
	if (clk_hz < 1000)
		return -EINVAL;

	clk_khz = clk_hz / 1000;
	printf("QSPI: Reference clock at %d kHz\n", clk_khz);

	/*
	 * Boot scratch registers sit in the High-Speed Core sysmgr block
	 * on AGILEX72; the per-region helper transparently falls back to the
	 * single sysmgr instance on every other SoC64.
	 */
	if (sysmgr_hs_read(SYSMGR_SOC64_BOOT_SCRATCH_COLD0, &reg)) {
		pr_warn("%s: sysmgr COLD0 read failed; cannot record QSPI refclk\n",
			__func__);
		return -EIO;
	}
	reg &= ~SYSMGR_SCRATCH_REG_0_QSPI_REFCLK_MASK;
	reg |= clk_khz & SYSMGR_SCRATCH_REG_0_QSPI_REFCLK_MASK;
	if (sysmgr_hs_write(SYSMGR_SOC64_BOOT_SCRATCH_COLD0, reg)) {
		pr_warn("%s: sysmgr COLD0 write failed; QSPI refclk not recorded\n",
			__func__);
		return -EIO;
	}

	return 0;
}

unsigned int cm_get_qspi_controller_clk_hz(void)
{
	u32 reg = 0;

	if (sysmgr_hs_read(SYSMGR_SOC64_BOOT_SCRATCH_COLD0, &reg)) {
		pr_warn("%s: sysmgr COLD0 read failed; reporting 0 Hz\n",
			__func__);
		return 0;
	}
	return (reg & SYSMGR_SCRATCH_REG_0_QSPI_REFCLK_MASK) * 1000;
}
#endif

#ifndef CONFIG_XPL_BUILD
static int do_showclocks(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	cm_print_clock_quick_summary();
	return 0;
}

U_BOOT_CMD(
	clocks,	CONFIG_SYS_MAXARGS, 1, do_showclocks,
	"display clocks",
	""
);
#endif
