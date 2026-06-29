// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <clk.h>
#include <config.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/system_manager.h>
#include <dt-bindings/clock/altr,agilex72-clkmgr.h>

DECLARE_GLOBAL_DATA_PTR;

static ulong cm_get_rate_dm(u32 id)
{
	struct udevice *dev;
	struct clk clk;
	ulong rate;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_CLK,
					  DM_DRIVER_GET(socfpga_agilex72_clk),
					  &dev);
	if (ret)
		return 0;

	clk.id = id;
	ret = clk_request(dev, &clk);
	if (ret < 0)
		return 0;

	rate = clk_get_rate(&clk);

	if ((rate == (unsigned long)-ENOSYS) ||
	    (rate == (unsigned long)-ENXIO) ||
	    (rate == (unsigned long)-EIO)) {
		debug("%s id %u: clk_get_rate err: %ld\n",
		      __func__, id, rate);
		return 0;
	}

	return rate;
}

static u32 cm_get_rate_dm_khz(u32 id)
{
	return cm_get_rate_dm(id) / 1000;
}

unsigned long cm_get_mpu_clk_hz(void)
{
	return cm_get_rate_dm(AGILEX72_MPU_CLK);
}

unsigned long cm_get_core2_clk_hz(void)
{
	return cm_get_rate_dm(AGILEX72_CORE2_CLK);
}

unsigned long cm_get_core3_clk_hz(void)
{
	return cm_get_rate_dm(AGILEX72_CORE3_CLK);
}

unsigned int cm_get_lsp_sys_free_clk_hz(void)
{
	return cm_get_rate_dm(AGILEX72_LSP_SYS_FREE_CLK);
}

void cm_print_clock_quick_summary(void)
{
	printf("A520 comp0  %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_MPU_CLK));
	printf("A720 core2  %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_CORE2_CLK));
	printf("A720 core3  %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_CORE3_CLK));
	printf("LSP main    %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_LSP_MAIN_CLK));
	printf("LSP sys fr  %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_LSP_SYS_FREE_CLK));
	printf("LSP MP      %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_LSP_MP_CLK));
	printf("LSP SP      %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_LSP_SP_CLK));
	printf("SDMMC0      %8d kHz\n",
	       cm_get_rate_dm_khz(AGILEX72_SDMMC0_SDMCLK));
}
