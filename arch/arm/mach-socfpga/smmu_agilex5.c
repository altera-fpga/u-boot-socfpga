// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation. All rights reserved
 *
 */

#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/firewall.h>
#include <asm/arch/smmu_agilex5.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

static inline void setup_smmu_firewall(void)
{
	u32 i;

	/* Off the DDR secure transaction for all TBU supported peripherals */
	for (i = SYSMGR_DMA0_SID_ADDR; i < SYSMGR_TSN2_SID_ADDR; i +=
	     SOCFPGA_NEXT_TBU_PERIPHERAL) {
		/* skip this, future use register */
		if (i == SYSMGR_USB3_SID_ADDR)
			continue;

		writel(SECURE_TRANS_RESET, (uintptr_t)i);
	}
}

void socfpga_init_smmu(void)
{
	setup_smmu_firewall();
}
