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
#include "../../../drivers/clk/altera/clk-agilex72.h"

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

#if IS_ENABLED(CONFIG_AGILEX72_CLKMGR_RUNTIME_AUDIT) && !defined(CONFIG_XPL_BUILD)

#define CM_CONSUMER_AUDIT_TOL_PPM	100

static bool cm_consumer_hz_match(u64 expect, u64 got)
{
	u64 delta;

	if (!expect || !got)
		return false;

	delta = (expect > got) ? (expect - got) : (got - expect);
	return delta * 1000000ULL <= expect * CM_CONSUMER_AUDIT_TOL_PPM;
}

struct cm_consumer_audit_entry {
	u32 id;
	const char *name;
	u32 expect_hz;
};

static const char *const km_clk_id_names[AGILEX72_NUM_CLKS] = {
	[AGILEX72_OSC1]			= "osc1",
	[AGILEX72_CB_INTOSC_DIV2_CLK]	= "cb_intosc_div2",
	[AGILEX72_CB_INTOSC_DIV10_CLK]	= "cb_intosc_div10",
	[AGILEX72_F2S_FREE_CLK]		= "f2s_free",
	[AGILEX72_GPPLL0_CLK]		= "gppll0",
	[AGILEX72_GPPLL1_CLK]		= "gppll1",
	[AGILEX72_GPPLL2_CLK]		= "gppll2",
	[AGILEX72_GPPLL0_C0_CLK]		= "gppll0_c0",
	[AGILEX72_GPPLL0_C1_CLK]		= "gppll0_c1",
	[AGILEX72_GPPLL0_C2_CLK]		= "gppll0_c2",
	[AGILEX72_GPPLL0_C3_CLK]		= "gppll0_c3",
	[AGILEX72_GPPLL0_C4_CLK]		= "gppll0_c4",
	[AGILEX72_GPPLL0_C5_CLK]		= "gppll0_c5",
	[AGILEX72_GPPLL0_C6_CLK]		= "gppll0_c6",
	[AGILEX72_GPPLL1_C0_CLK]		= "gppll1_c0",
	[AGILEX72_GPPLL1_C1_CLK]		= "gppll1_c1",
	[AGILEX72_GPPLL2_C0_CLK]		= "gppll2_c0",
	[AGILEX72_GPPLL2_C1_CLK]		= "gppll2_c1",
	[AGILEX72_BOOT_CLK]			= "boot_clk",
	[AGILEX72_COMP0_FREE_CLK]		= "comp0_free",
	[AGILEX72_CORE2_FREE_CLK]		= "core2_free",
	[AGILEX72_CORE3_FREE_CLK]		= "core3_free",
	[AGILEX72_DSU_FREE_CLK]		= "dsu_free",
	[AGILEX72_CCU_FREE_CLK]		= "ccu_free",
	[AGILEX72_HSP_NOC_FREE_CLK]		= "hsp_noc_free",
	[AGILEX72_LSP_NOC_FREE_CLK]		= "lsp_noc_free",
	[AGILEX72_TRACE_FREE_CLK]		= "trace_free",
	[AGILEX72_EMAC_A_FREE_CLK]		= "emac_a_free",
	[AGILEX72_EMAC_B_FREE_CLK]		= "emac_b_free",
	[AGILEX72_EMAC_PTP_FREE_CLK]	= "emac_ptp_free",
	[AGILEX72_GPIO_DB_FREE_CLK]		= "gpio_db_free",
	[AGILEX72_USB31_FREE_CLK]		= "usb31_free",
	[AGILEX72_S2F_USER0_FREE_CLK]	= "s2f_user0_free",
	[AGILEX72_S2F_USER1_FREE_CLK]	= "s2f_user1_free",
	[AGILEX72_XSPI_PHY_FREE_CLK]	= "xspi_phy_free",
	[AGILEX72_MEMDEVICE_PHY_FREE_CLK]	= "memdevice_phy_free",
	[AGILEX72_COMP0_CLK]		= "comp0_clk",
	[AGILEX72_CORE2_CLK]		= "core2_clk",
	[AGILEX72_CORE3_CLK]		= "core3_clk",
	[AGILEX72_MPU_CLK]			= "mpu_clk",
	[AGILEX72_CCU_CLK]			= "ccu_clk",
	[AGILEX72_APU_SYS_FREE_CLK]		= "apu_sys_free",
	[AGILEX72_HSP_SYS_FREE_CLK]		= "hsp_sys_free",
	[AGILEX72_HSP_MAIN_FREE_CLK]	= "hsp_main_free",
	[AGILEX72_HSP_MAIN_CLK]		= "hsp_main",
	[AGILEX72_HSP_MP_CLK]		= "hsp_mp",
	[AGILEX72_HSP_SP_CLK]		= "hsp_sp",
	[AGILEX72_USB2OTG_HCLK]		= "usb2otg_hclk",
	[AGILEX72_LSP_SYS_FREE_CLK]		= "lsp_sys_free_wdt",
	[AGILEX72_LSP_MAIN_FREE_CLK]	= "lsp_main_free",
	[AGILEX72_LSP_MAIN_CLK]		= "lsp_main",
	[AGILEX72_LSP_MP_CLK]		= "lsp_mp",
	[AGILEX72_LSP_SP_CLK]		= "lsp_sp",
	[AGILEX72_SPIM_0_CLK]		= "spim0",
	[AGILEX72_SPIM_1_CLK]		= "spim1",
	[AGILEX72_SPIS_0_CLK]		= "spis0",
	[AGILEX72_SPIS_1_CLK]		= "spis1",
	[AGILEX72_DMA_0_CORE_CLK]		= "dma0_core",
	[AGILEX72_DMA_0_HS_CLK]		= "dma0_hs",
	[AGILEX72_DMA_1_CORE_CLK]		= "dma1_core",
	[AGILEX72_DMA_1_HS_CLK]		= "dma1_hs",
	[AGILEX72_I3C_0_CORE_CLK]		= "i3c0_core",
	[AGILEX72_I3C_1_CORE_CLK]		= "i3c1_core",
	[AGILEX72_I2C_0_PCLK]		= "i2c0_pclk",
	[AGILEX72_I2C_1_PCLK]		= "i2c1_pclk",
	[AGILEX72_I2C_EMAC0_PCLK]		= "i2c_emac0_pclk",
	[AGILEX72_I2C_EMAC1_PCLK]		= "i2c_emac1_pclk",
	[AGILEX72_I2C_EMAC2_PCLK]		= "i2c_emac2_pclk",
	[AGILEX72_UART_0_PCLK]		= "uart0_pclk",
	[AGILEX72_UART_1_PCLK]		= "uart1_pclk",
	[AGILEX72_UART_2_PCLK]		= "uart2_pclk",
	[AGILEX72_SPTIMER_0_PCLK]		= "sptimer0_pclk",
	[AGILEX72_SPTIMER_1_PCLK]		= "sptimer1_pclk",
	[AGILEX72_CS_AT_CLK]		= "cs_at",
	[AGILEX72_CS_PDBG_CLK]		= "cs_pdbg",
	[AGILEX72_CS_TRACE_CLK]		= "cs_trace",
	[AGILEX72_EMACA_DIV_CLK]		= "emaca_div",
	[AGILEX72_EMACB_DIV_CLK]		= "emacb_div",
	[AGILEX72_EMAC0_CLK]		= "emac0",
	[AGILEX72_EMAC1_CLK]		= "emac1",
	[AGILEX72_EMAC2_CLK]		= "emac2",
	[AGILEX72_EMAC_PTP_CLK]		= "emac_ptp",
	[AGILEX72_GPIO_DB_CLK]		= "gpio_db",
	[AGILEX72_USB31_SUSPEND_CLK]	= "usb31_suspend",
	[AGILEX72_USB31_BUS_CLK_EARLY]	= "usb31_bus_early",
	[AGILEX72_S2F_USER0_CLK]		= "s2f_user0",
	[AGILEX72_S2F_USER1_CLK]		= "s2f_user1",
	[AGILEX72_XSPI_PCLK]		= "xspi_pclk",
	[AGILEX72_XSPI_CLK]			= "xspi",
	[AGILEX72_XSPI_PHY_CLK]		= "xspi_phy",
	[AGILEX72_SDMMC0_SDPHY_REG_CLK]	= "sdmmc0_sdphy_reg",
	[AGILEX72_SDMMC1_SDPHY_REG_CLK]	= "sdmmc1_sdphy_reg",
	[AGILEX72_SDMMC0_SDMCLK]		= "sdmmc0_sdmclk",
	[AGILEX72_SDMMC1_SDMCLK]		= "sdmmc1_sdmclk",
	[AGILEX72_SDMMC0_PHY_CLK]		= "sdmmc0_phy",
	[AGILEX72_SDMMC1_PHY_CLK]		= "sdmmc1_phy",
};

static const char *km_clk_tree_name(u32 id)
{
	if (id < AGILEX72_NUM_CLKS && km_clk_id_names[id])
		return km_clk_id_names[id];

	return "unknown";
}

void cm_print_runtime_clock_tree(void)
{
	u32 id;
	int reported = 0;

	printf("AGILEX72_CLK_TREE: DM socfpga_clk_get_rate() id 0..%u\n",
	       AGILEX72_NUM_CLKS - 1);

	for (id = 0; id < AGILEX72_NUM_CLKS; id++) {
		ulong hz = cm_get_rate_dm(id);
		const char *name = km_clk_tree_name(id);

		if (!hz) {
			printf("AGILEX72_CLK_TREE: id=%2u %-22s --\n", id, name);
			continue;
		}

		reported++;
		if (hz >= 1000000UL)
			printf("AGILEX72_CLK_TREE: id=%2u %-22s %8lu MHz\n",
			       id, name, hz / 1000000UL);
		else
			printf("AGILEX72_CLK_TREE: id=%2u %-22s %8lu kHz\n",
			       id, name, hz / 1000UL);
	}

	printf("AGILEX72_CLK_TREE: %d/%u bindings reported non-zero Hz\n",
	       reported, AGILEX72_NUM_CLKS);
}

int cm_audit_runtime_clock_trees(void)
{
	int ret;

	printf("AGILEX72_CLK_TREE: production handoff path - runtime audit start\n");

	printf("AGILEX72_CLK_TREE: GPPLL CSR layer (handoff rate_state)\n");
	agilex72_clkmgr_print_rate_state();

	cm_print_runtime_clock_tree();

	ret = cm_audit_consumer_clock_rates();

	printf("AGILEX72_CLK_TREE: production handoff path - runtime audit done\n");

	return ret;
}

static const struct cm_consumer_audit_entry cm_consumer_audit_table[] = {
	/* Roots */
	{ AGILEX72_OSC1,			"osc1",			AGILEX72_OSC1_HZ },
	{ AGILEX72_CB_INTOSC_DIV2_CLK,	"cb_intosc_div2",	AGILEX72_INTOSC_DIV2_HZ },
	{ AGILEX72_CB_INTOSC_DIV10_CLK,	"cb_intosc_div10",	AGILEX72_INTOSC_DIV10_HZ },
	{ AGILEX72_F2S_FREE_CLK,		"f2s_free",		AGILEX72_F2S_FREE_HZ },
	{ AGILEX72_BOOT_CLK,		"boot_clk",		0 },

	/* GPPLL primary outputs */
	{ AGILEX72_GPPLL0_CLK,		"gppll0",
	  (u32)AGILEX72_GPPLL0_VCO_HZ },
	{ AGILEX72_GPPLL1_CLK,		"gppll1",
	  (u32)AGILEX72_GPPLL1_VCO_HZ },
	{ AGILEX72_GPPLL2_CLK,		"gppll2",
	  (u32)AGILEX72_GPPLL2_VCO_HZ },
	{ AGILEX72_GPPLL0_C0_CLK,		"gppll0_c0",
	  (u32)AGILEX72_GPPLL0_C0_HZ },
	{ AGILEX72_GPPLL0_C1_CLK,		"gppll0_c1",
	  (u32)AGILEX72_GPPLL0_C1_HZ },
	{ AGILEX72_GPPLL0_C2_CLK,		"gppll0_c2",
	  (u32)AGILEX72_GPPLL0_C2_HZ },
	{ AGILEX72_GPPLL0_C3_CLK,		"gppll0_c3",
	  (u32)AGILEX72_GPPLL0_C3_HZ },
	{ AGILEX72_GPPLL0_C4_CLK,		"gppll0_c4",
	  (u32)AGILEX72_GPPLL0_C4_HZ },
	{ AGILEX72_GPPLL0_C5_CLK,		"gppll0_c5",
	  (u32)AGILEX72_GPPLL0_C5_HZ },
	{ AGILEX72_GPPLL0_C6_CLK,		"gppll0_c6",
	  (u32)AGILEX72_GPPLL0_C6_HZ },
	{ AGILEX72_GPPLL1_C0_CLK,		"gppll1_c0",
	  (u32)AGILEX72_GPPLL1_C0_HZ },
	{ AGILEX72_GPPLL1_C1_CLK,		"gppll1_c1",
	  (u32)AGILEX72_GPPLL1_C1_HZ },
	{ AGILEX72_GPPLL2_C0_CLK,		"gppll2_c0",
	  (u32)AGILEX72_GPPLL2_C0_HZ },
	{ AGILEX72_GPPLL2_C1_CLK,		"gppll2_c1",
	  (u32)AGILEX72_GPPLL2_C1_HZ },

	/* CPU / cluster */
	{ AGILEX72_MPU_CLK,			"mpu_clk (comp0)",	(u32)AGILEX72_MPU_HZ },
	{ AGILEX72_COMP0_CLK,		"comp0_clk",		(u32)AGILEX72_MPU_HZ },
	{ AGILEX72_COMP0_FREE_CLK,		"comp0_free",		(u32)AGILEX72_MPU_HZ },
	{ AGILEX72_DSU_FREE_CLK,		"dsu_free",
	  (u32)AGILEX72_GPPLL1_C1_HZ },
	{ AGILEX72_CORE2_CLK,		"core2_clk",		(u32)AGILEX72_GPPLL2_C0_HZ },
	{ AGILEX72_CORE3_CLK,		"core3_clk",		(u32)AGILEX72_GPPLL2_C1_HZ },
	{ AGILEX72_CORE2_FREE_CLK,		"core2_free",
	  (u32)AGILEX72_GPPLL2_C0_HZ },
	{ AGILEX72_CORE3_FREE_CLK,		"core3_free",
	  (u32)AGILEX72_GPPLL2_C1_HZ },
	{ AGILEX72_CCU_FREE_CLK,		"ccu_free",		(u32)AGILEX72_CCU_FREE_HZ },
	{ AGILEX72_CCU_CLK,			"ccu_clk",		(u32)AGILEX72_CCU_FREE_HZ },
	{ AGILEX72_APU_SYS_FREE_CLK,	"apu_sys_free",		(u32)AGILEX72_APU_SYS_FREE_HZ },

	/* HSP / LSP NoC primary stages + leaves */
	{ AGILEX72_HSP_NOC_FREE_CLK,	"hsp_noc_free",		(u32)AGILEX72_HSP_MAIN_HZ },
	{ AGILEX72_LSP_NOC_FREE_CLK,	"lsp_noc_free",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_HSP_MAIN_FREE_CLK,	"hsp_main_free",	(u32)AGILEX72_HSP_MAIN_HZ },
	{ AGILEX72_HSP_MAIN_CLK,		"hsp_main",		(u32)AGILEX72_HSP_MAIN_HZ },
	{ AGILEX72_HSP_SYS_FREE_CLK,		"hsp_sys_free",		(u32)AGILEX72_HSP_SYS_HZ },
	{ AGILEX72_HSP_MP_CLK,		"hsp_mp",		(u32)AGILEX72_HSP_MP_HZ },
	{ AGILEX72_HSP_SP_CLK,		"hsp_sp",		(u32)AGILEX72_HSP_SP_HZ },
	{ AGILEX72_USB31_BUS_CLK_EARLY,	"usb31_bus_early",	(u32)AGILEX72_HSP_MAIN_HZ },
	{ AGILEX72_USB2OTG_HCLK,		"usb2otg_hclk",		(u32)AGILEX72_HSP_MP_HZ },
	{ AGILEX72_DMA_0_HS_CLK,		"dma0_hs",		(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_DMA_1_HS_CLK,		"dma1_hs",		(u32)AGILEX72_LSP_MP_HZ },

	/* LSP NoC + watchdog alias */
	{ AGILEX72_LSP_MAIN_FREE_CLK,	"lsp_main_free",	(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_LSP_MAIN_CLK,		"lsp_main",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_LSP_SYS_FREE_CLK,	"lsp_sys_free (wdt)",	(u32)AGILEX72_LSP_SYS_HZ },
	{ AGILEX72_LSP_MP_CLK,		"lsp_mp",		(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_LSP_SP_CLK,		"lsp_sp",		(u32)AGILEX72_LSP_SP_HZ },

	/* LSP-attached peripherals (timers, UART, I2C, SPI, DMA, XSPI) */
	{ AGILEX72_XSPI_CLK,		"xspi",			(u32)AGILEX72_XSPIPHY_HZ },
	{ AGILEX72_XSPI_PCLK,		"xspi_pclk",		(u32)AGILEX72_XSPIPHY_HZ },
	{ AGILEX72_DMA_0_CORE_CLK,		"dma0_core",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_DMA_1_CORE_CLK,		"dma1_core",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_I3C_0_CORE_CLK,		"i3c0_core",		(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_I3C_1_CORE_CLK,		"i3c1_core",		(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_SPIM_0_CLK,		"spim0",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_SPIM_1_CLK,		"spim1",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_SPIS_0_CLK,		"spis0",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_SPIS_1_CLK,		"spis1",		(u32)AGILEX72_LSP_MAIN_HZ },
	{ AGILEX72_I2C_0_PCLK,		"i2c0_pclk",		(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_I2C_1_PCLK,		"i2c1_pclk",		(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_I2C_EMAC0_PCLK,		"i2c_emac0_pclk",	(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_I2C_EMAC1_PCLK,		"i2c_emac1_pclk",	(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_I2C_EMAC2_PCLK,		"i2c_emac2_pclk",	(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_UART_0_PCLK,		"uart0_pclk",		(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_UART_1_PCLK,		"uart1_pclk",		(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_UART_2_PCLK,		"uart2_pclk",		(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_SPTIMER_0_PCLK,		"sptimer0_pclk",	(u32)AGILEX72_LSP_SP_HZ },
	{ AGILEX72_SPTIMER_1_PCLK,		"sptimer1_pclk",	(u32)AGILEX72_LSP_SP_HZ },

	/* EMAC / USB31 ref */
	{ AGILEX72_EMAC_A_FREE_CLK,		"emac_a_free",		(u32)AGILEX72_EMAC_HZ },
	{ AGILEX72_EMAC_B_FREE_CLK,		"emac_b_free",		(u32)AGILEX72_EMACB_HZ },
	{ AGILEX72_EMAC_PTP_FREE_CLK,	"emac_ptp_free",	(u32)AGILEX72_EMAC_PTP_HZ },
	{ AGILEX72_EMAC0_CLK,		"emac0",		(u32)AGILEX72_EMAC_HZ },
	{ AGILEX72_EMAC1_CLK,		"emac1",		(u32)AGILEX72_EMAC_HZ },
	{ AGILEX72_EMAC2_CLK,		"emac2",		(u32)AGILEX72_EMAC_HZ },
	{ AGILEX72_EMACA_DIV_CLK,		"emaca_div",		(u32)AGILEX72_EMAC_HZ },
	{ AGILEX72_EMACB_DIV_CLK,		"emacb_div",		(u32)AGILEX72_EMACB_HZ },
	{ AGILEX72_EMAC_PTP_CLK,		"emac_ptp",		(u32)AGILEX72_EMAC_PTP_HZ },
	{ AGILEX72_USB31_FREE_CLK,		"usb31_ref",
	  (u32)AGILEX72_USB31_REF_HZ },
	{ AGILEX72_USB31_SUSPEND_CLK,	"usb31_suspend",
	  (u32)AGILEX72_USB31_REF_HZ },

	/* SDMMC / PHY */
	{ AGILEX72_SDMMC0_SDMCLK,		"sdmmc0_sdmclk",	(u32)AGILEX72_SDMMC_HZ },
	{ AGILEX72_SDMMC1_SDMCLK,		"sdmmc1_sdmclk",	(u32)AGILEX72_SDMMC_HZ },
	{ AGILEX72_SDMMC0_PHY_CLK,		"sdmmc0_phy",		(u32)AGILEX72_SDMMC_HZ },
	{ AGILEX72_SDMMC1_PHY_CLK,		"sdmmc1_phy",		(u32)AGILEX72_SDMMC_HZ },
	{ AGILEX72_SDMMC0_SDPHY_REG_CLK,	"sdmmc0_sdphy_reg",	(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_SDMMC1_SDPHY_REG_CLK,	"sdmmc1_sdphy_reg",	(u32)AGILEX72_LSP_MP_HZ },
	{ AGILEX72_MEMDEVICE_PHY_FREE_CLK,	"memdevice_phy_free",	(u32)AGILEX72_MEMPHY_HZ },
	{ AGILEX72_XSPI_PHY_FREE_CLK,	"xspi_phy_free",	(u32)AGILEX72_XSPIPHY_HZ },
	{ AGILEX72_XSPI_PHY_CLK,		"xspi_phy",		(u32)AGILEX72_XSPIPHY_HZ },

	/* GPIO debounce, trace, CoreSight, S2F */
	{ AGILEX72_GPIO_DB_FREE_CLK,	"gpio_db_free",		(u32)AGILEX72_GPIO_DB_FREE_HZ },
	{ AGILEX72_GPIO_DB_CLK,		"gpio_db",		(u32)AGILEX72_GPIO_DB_HZ },
	{ AGILEX72_TRACE_FREE_CLK,		"trace_free",		(u32)AGILEX72_CS_TRACE_HZ },
	{ AGILEX72_CS_AT_CLK,		"cs_at",		(u32)AGILEX72_CS_AT_HZ },
	{ AGILEX72_CS_PDBG_CLK,		"cs_pdbg",		(u32)AGILEX72_CS_PDBG_HZ },
	{ AGILEX72_CS_TRACE_CLK,		"cs_trace",		(u32)AGILEX72_CS_TRACE_HZ },
	{ AGILEX72_S2F_USER0_FREE_CLK,	"s2f_user0_free",	(u32)AGILEX72_S2F_USER_HZ },
	{ AGILEX72_S2F_USER1_FREE_CLK,	"s2f_user1_free",	(u32)AGILEX72_S2F_USER_HZ },
	{ AGILEX72_S2F_USER0_CLK,		"s2f_user0",		(u32)AGILEX72_S2F_USER_HZ },
	{ AGILEX72_S2F_USER1_CLK,		"s2f_user1",		(u32)AGILEX72_S2F_USER_HZ },
};

int cm_audit_consumer_clock_rates(void)
{
	size_t i;
	int checked = 0;
	int fails = 0;
	int skipped = 0;

	printf("AGILEX72_CONSUMER_AUDIT: DM socfpga_clk_get_rate vs V9 SYSPRESET0 bin1\n");

	for (i = 0; i < ARRAY_SIZE(cm_consumer_audit_table); i++) {
		const struct cm_consumer_audit_entry *e = &cm_consumer_audit_table[i];
		ulong got = cm_get_rate_dm(e->id);
		const char *verdict;

		if (!got) {
			printf("AGILEX72_CONSUMER_AUDIT: %s id=%u SKIP (no rate)\n",
			       e->name, e->id);
			skipped++;
			continue;
		}

		checked++;
		if (!e->expect_hz) {
			printf("AGILEX72_CONSUMER_AUDIT: %s runtime %lu Hz INFO (no fixed golden)\n",
			       e->name, got);
			continue;
		}

		verdict = cm_consumer_hz_match(e->expect_hz, got) ? "PASS" : "FAIL";
		printf("AGILEX72_CONSUMER_AUDIT: %s expect %u Hz runtime %lu Hz %s\n",
		       e->name, e->expect_hz, got, verdict);
		if (!cm_consumer_hz_match(e->expect_hz, got))
			fails++;
	}

	printf("AGILEX72_CONSUMER_AUDIT: VERDICT %s (%d checked, %d mismatch, %d skipped)\n",
	       fails ? "FAIL" : "PASS", checked, fails, skipped);

	return fails ? -EINVAL : 0;
}

#endif

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
