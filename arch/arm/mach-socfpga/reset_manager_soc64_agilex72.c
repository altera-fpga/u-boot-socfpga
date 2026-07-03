// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 */

#include <errno.h>
#include <hang.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/secure.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smc_api.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/timer.h>
#include <dt-bindings/reset/altr,rst-mgr-s10.h>
#include <exports.h>
#include <linux/iopoll.h>
#include <linux/intel-smc.h>
#include <linux/printk.h>
DECLARE_GLOBAL_DATA_PTR;

#define TIMEOUT_300MS     300

static __always_inline int wait_for_bit(u32 *reg, const u32 mask, bool set,
					unsigned int timeout_ms)
{
	u32 val;
	int timeout = timeout_ms;

	while (1) {
		val = readl(reg);

		if (!set)
			val = ~val;

		if ((val & mask) == mask)
			return 0;

		if (!timeout)
			break;

		timeout--;
		__socfpga_udelay(1000);
	}

	return -ETIMEDOUT;
}

static __always_inline void socfpga_f2s_bridges_reset(int enable,
						      unsigned int mask)
{
	/* AGILEX72 HPS only has the F2SOC bridge. */
	u32 brg_mask = mask & RSTMGR_BRGMODRST_FPGA2SOC_MASK;

	if (!brg_mask)
		return;

	if (enable) {
		/* Bring the F2SOC bridge out of reset. */
		clrbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     brg_mask);
	} else {
		if (readl(socfpga_get_rstmgr_addr() +
			  RSTMGR_SOC64_BRGMODRST) & brg_mask) {
			/* Bridge cannot be reset twice */
			return;
		}

		/*
		 * AGILEX72 dropped HW fence & drain at the bridges; softlogic is
		 * responsible for quiescing all F2SOC initiators beforehand.
		 * Notify the fabric of the pending reset through the Reset
		 * Manager FPGA handshake, then assert the bridge reset.
		 */
		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKEN,
			     RSTMGR_HDSKEN_FPGAHSEN);
		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKREQ,
			     RSTMGR_HDSKREQ_FPGAHSREQ);

		/* Wait for FPGA to ack the handshake request */
		if (wait_for_bit((u32 *)(socfpga_get_rstmgr_addr() +
				 RSTMGR_SOC64_HDSKACK), RSTMGR_HDSKREQ_FPGAHSREQ,
				 true, TIMEOUT_300MS))
			pr_warn("agilex72-rstmgr: FPGA handshake ack timeout; asserting bridge reset anyway\n");

		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     brg_mask);
		clrbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKREQ,
			     RSTMGR_HDSKREQ_FPGAHSREQ);
	}
}

void socfpga_bridges_reset(int enable, unsigned int mask)
{
	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		u64 arg[2];
		int ret;

		/* Set bit-1 to indicate has mask value in arg[1]. */
		arg[0] = (enable & BIT(0)) | BIT(1);
		arg[1] = mask;

		ret = invoke_smc(INTEL_SIP_SMC_HPS_SET_BRIDGES, arg,
				 ARRAY_SIZE(arg), NULL, 0);
		if (ret)
			printf("Failed to %s the HPS bridges, check bridges availability. Status %d.\n",
			       enable ? "enable" : "disable", ret);
	} else {
		socfpga_f2s_bridges_reset(enable, mask);
	}
}

void __secure socfpga_bridges_reset_psci(int enable, unsigned int mask)
{
	socfpga_f2s_bridges_reset(enable, mask);
}
