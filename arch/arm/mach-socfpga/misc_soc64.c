// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 */

#include <altera.h>
#include <env.h>
#include <errno.h>
#include <init.h>
#include <log.h>
#include <asm/arch/board.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smc_api.h>
#include <asm/arch/smmuv3_dv.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/bitfield.h>
#include <mach/clock_manager.h>

#define RSU_DEFAULT_LOG_LEVEL  7

/* Agilex5 Sub Device Jtag ID List */
#define A3690_JTAG_ID	0x036090DD
#define A3694_JTAG_ID	0x436090DD
#define A36C0_JTAG_ID	0x0360C0DD
#define A36C4_JTAG_ID	0x4360C0DD
#define A36D0_JTAG_ID	0x0360D0DD
#define A36D4_JTAG_ID	0x4360D0DD
#define A36F0_JTAG_ID	0x0360F0DD
#define A36F4_JTAG_ID	0x4360F0DD
#define A3610_JTAG_ID	0x036010DD
#define A3614_JTAG_ID	0x436010DD
#define A3630_JTAG_ID	0x036030DD
#define A3634_JTAG_ID	0x436030DD

#define JTAG_ID_MASK	0xCFF0FFFF

/*
 * FPGA programming support for SoC FPGA Stratix 10
 */
static Altera_desc altera_fpga[] = {
	{
		/* Family */
		Intel_FPGA_SDM_Mailbox,
		/* Interface type */
		secure_device_manager_mailbox,
		/* No limitation as additional data will be ignored */
		-1,
		/* No device function table */
		NULL,
		/* Base interface address specified in driver */
		NULL,
		/* No cookie implementation */
		0
	},
};

u32 socfpga_get_jtag_id(void)
{
	u32 jtag_id;

	jtag_id = readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_BOOT_SCRATCH_COLD4);

	if (!jtag_id) {
		debug("Failed to read JTAG ID. Default JTAG ID to A36F4_JTAG_ID.\n");
		jtag_id = A36F4_JTAG_ID;
	}

	debug("%s: jtag_id: 0x%x\n", __func__, jtag_id);

	return jtag_id;
}

/*
 * The Agilex5 platform has enabled the bloblist feature, and the bloblist
 * address and size are initialized based on the defconfig settings.
 * During the SPL phase, this function is used to prevent the bloblist
 * from initializing its address and size with the saved boot parameters,
 * which may have been incorrectly set.
 */
void save_boot_params(unsigned long r0, unsigned long r1, unsigned long r2,
		      unsigned long r3)
{
	save_boot_params_ret();
}

/*
 * Print CPU information
 */
#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	printf("CPU: Altera FPGA SoCFPGA Platform (ARMv8 64bit Cortex-%s)\n",
	       IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5) ? "A55/A76" : "A53");

	return 0;
}
#endif

#ifdef CONFIG_ARCH_MISC_INIT
int arch_misc_init(void)
{
#if !IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX5_EMU)
	char qspi_string[13];
	char level[4];
	unsigned long id;

	snprintf(level, sizeof(level), "%u", RSU_DEFAULT_LOG_LEVEL);
	sprintf(qspi_string, "<0x%08x>", cm_get_qspi_controller_clk_hz());
	env_set("qspi_clock", qspi_string);

	/* for RSU, set log level to default if log level is not set */
	if (!env_get("rsu_log_level"))
		env_set("rsu_log_level", level);

	/* Export board_id as environment variable */
	id = socfpga_get_board_id();
	env_set_ulong("board_id", id);
#endif

	return 0;
}
#endif

int arch_early_init_r(void)
{
	socfpga_fpga_add(&altera_fpga[0]);

	return 0;
}

#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5)
bool is_agilex5_reva_workaround_required(void)
{
	u32 reg;
	bool status;

	reg = readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_BOOT_SCRATCH_POR1);
	debug("%s: SYSMGR_SOC64_BOOT_SCRATCH_POR1: 0x%x\n", __func__, reg);

	status = FIELD_GET(ALT_SYSMGR_SCRATCH_REG_POR_1_REVA_WORKAROUND_MASK, reg);
	debug("%s: Agilex 5 Rev A workaround status: 0x%x\n", __func__, status);

	return status;
}
#endif

/* Return 1 if FPGA is ready otherwise return 0 */
int is_fpga_config_ready(void)
{
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5)
	if (is_agilex5_reva_workaround_required()) {
		return (readl(socfpga_get_sysmgr_addr() +
				SYSMGR_SOC64_BOOT_SCRATCH_POR1) &
				ALT_SYSMGR_SCRATCH_REG_POR_1_REVA_WORKAROUND_USER_MODE_MASK);
	}
#endif

	return (readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_FPGA_CONFIG) &
		SYSMGR_FPGACONFIG_READY_MASK) == SYSMGR_FPGACONFIG_READY_MASK;
}

void do_bridge_reset(int enable, unsigned int mask)
{
	/* Check FPGA status before bridge enable */
	if (!is_fpga_config_ready()) {
		puts("FPGA not ready. Bridge reset aborted!\n");
		return;
	}

	socfpga_bridges_reset(enable, mask);
}

void do_qspi_ownership_quirk(void)
{
	if (IS_ENABLED(CONFIG_CADENCE_QSPI) && IS_ENABLED(CONFIG_SPL_ATF)) {
		int ret = 0;

		ret = env_get_yesno("returnQSPI");
		if (ret == 1) {
			/* FCS Attestation:return QSPI ownership to SDM if needed */
			ret = smc_send_mailbox(MBOX_QSPI_CLOSE, 0, NULL,
					       0, 0, NULL);
			if (ret)
				printf("close QSPI failed, (err=%d)\n", ret);
		}
	}
}


void arch_preboot_os(void)
{
	do_qspi_ownership_quirk();
	mbox_hps_stage_notify(HPS_EXECUTION_STATE_OS);
}

int misc_init_r(void)
{
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5)
	if (is_agilex5_reva_workaround_required())
		return smmu_sdm_init();
#endif

	return 0;
}
