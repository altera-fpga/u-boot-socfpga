/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/smc_s10.h>
#include <asm/arch/system_manager.h>
#include <asm/secure.h>
#include <linux/intel-smc.h>

#define S10_WARM_RESET_WFI_FLAG BIT(31)
#define SYSMGR_ECC_DBE_COLD_RST_MASK	0x00030002

bool __secure is_ecc_dbe_cold_reset(void)
{
	return !!(readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD8) &
		  SYSMGR_ECC_DBE_COLD_RST_MASK);
}

/* ECC DBEs require a Reset but just notify here */
static void __secure smc_socfpga_ecc_dbe_nofify(unsigned long function_id,
						unsigned long DBE)
{
	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	/* If the warm reset flag is set, spin in WFI here. */
	if (DBE & S10_WARM_RESET_WFI_FLAG) {
		asm volatile(
			/* Put all cores into WFI mode */
			"wfi_loop2:\n"
			"	wfi\n"
			"	b	wfi_loop2\n"
			: : : );
	}

	SMC_RET_REG_MEM(r);
}

DECLARE_SECURE_SVC(ecc_db_error, INTEL_SIP_SMC_ECC_DBE,
		   smc_socfpga_ecc_dbe_nofify);
