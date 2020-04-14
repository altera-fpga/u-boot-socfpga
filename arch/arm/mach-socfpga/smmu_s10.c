/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/firewall.h>
#include <asm/arch/smmu_s10.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

int is_smmu_bypass(void)
{
	return readl(SOCFPGA_SMMU_ADDRESS + SMMU_SCR0) & SMMU_SCR0_CLIENTPD;
}

int is_smmu_stream_id_enabled(u32 stream_id)
{
	int i;
	u32 smrg_num;
	u32 smr, s2cr, sid_mask;
	u32 cb, cb_index, cb_num;

	if (is_smmu_bypass())
		return 0;

	/* Get number of Stream Mapping Register Groups */
	smrg_num = readl(SOCFPGA_SMMU_ADDRESS + SMMU_SIDR0) &
		   SMMU_SIDR0_NUMSMRG_MASK;

	/* Get number of Context Bank */
	cb_num = readl(SOCFPGA_SMMU_ADDRESS + SMMU_SIDR1) &
		 SMMU_SIDR1_NUMCB_MASK;

	for (i = 0; i < smrg_num; i++) {
		smr = readl(SOCFPGA_SMMU_ADDRESS + SMMU_GR0_SMR((u64)i));
		sid_mask = (smr & SMMU_SMR_MASK) >> 16;

		/* Skip if Stream ID is invalid or not matched */
		if (!(smr & SMMU_SMR_VALID) || (smr & sid_mask) != stream_id)
			continue;

		/* Get Context Bank index from valid matching Stream ID */
		s2cr = readl(SOCFPGA_SMMU_ADDRESS + SMMU_GR0_S2CR((u64)i));
		cb_index = s2cr & SMMU_S2CR_CBNDX;

		/* Skip if Context Bank is invalid or not Translation mode */
		if (cb_index >= cb_num || (s2cr & SMMU_S2CR_TYPE))
			continue;

		cb = readl(SOCFPGA_SMMU_ADDRESS + SMMU_GR0_CB((u64)cb_index,
							      SMMU_CB_SCTLR));
		/* Return MMU enable status for this Context Bank */
		return (cb & SMMU_CB_SCTLR_M);
	}

	return 0;
}
