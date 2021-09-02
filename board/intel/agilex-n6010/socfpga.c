// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Intel Corporation <www.intel.com>
 *
 */

#include <asm/arch/firewall.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <common.h>
#include <command.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <spi.h>
#include <spl.h>
#include <watchdog.h>

#ifdef CONFIG_SPL_BUILD

#define COPY_ENGINE_BASE		0xf9000000

#define HPS_SCRATCHPAD			0x150

#define HOST2HPS_IMG_XFR_SHDW		0x154
#define HOST2HPS_IMG_XFR_COMPLETE	BIT(0)

#define HPS2HOST_RSP			0x158
#define HPS2HOST_RSP_SSBL_VFY		GENMASK(1, 0)
#define HPS2HOST_RSP_KRNL_VFY		GENMASK(3, 2)
#define HPS2HOST_RSP_HPS_RDY		BIT(4)

void spl_board_init(void)
{
	u32 img_xfr;

	socfpga_bridges_reset(true, RSTMGR_BRGMODRST_SOC2FPGA_MASK |
				    RSTMGR_BRGMODRST_LWSOC2FPGA_MASK |
				    RSTMGR_BRGMODRST_FPGA2SOC_MASK |
				    RSTMGR_BRGMODRST_F2SDRAM0_MASK);

	printf("waiting for host to copy image\n");

	do {
		writel(HPS2HOST_RSP_HPS_RDY, COPY_ENGINE_BASE + HPS2HOST_RSP);

		WATCHDOG_RESET();

		img_xfr = readl(COPY_ENGINE_BASE + HOST2HPS_IMG_XFR_SHDW);
	} while (!(img_xfr & HOST2HPS_IMG_XFR_COMPLETE));

	writel(0, COPY_ENGINE_BASE + HPS2HOST_RSP);

	printf("image copied from host 0x%x\n", readl(COPY_ENGINE_BASE + HPS2HOST_RSP));
}
#endif
