/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <asm/arch/smc_s10.h>
#include <asm/secure.h>
#include <linux/intel-smc.h>

/* ECC DBEs require a Reset but just notify here */
static void __secure smc_socfpga_ecc_dbe_nofify(unsigned long function_id,
						unsigned long DBE)
{
	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	/* Add future DBE handling here */

	SMC_RET_REG_MEM(r);
}

DECLARE_SECURE_SVC(ecc_db_error, INTEL_SIP_SMC_ECC_DBE,
		   smc_socfpga_ecc_dbe_nofify);
