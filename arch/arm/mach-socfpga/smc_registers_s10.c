/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/smc_s10.h>
#include <asm/secure.h>
#include <linux/intel-smc.h>

static void __secure smc_socfpga_register_read(unsigned long function_id,
					       unsigned long reg_addr)
{
	const u32 *pdata = (u32 *)reg_addr;

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (reg_addr) {
	case(0xF8011100):	/* ECCCTRL1 */
	case(0xF8011104):	/* ECCCTRL2 */
	case(0xF8011110):	/* ERRINTEN */
	case(0xF8011114):	/* ERRINTENS */
	case(0xF8011118):	/* ERRINTENR */
	case(0xF801111C):	/* INTMODE */
	case(0xF8011120):	/* INTSTAT */
	case(0xF8011124):	/* DIAGINTTEST */
	case(0xF801112C):	/* DERRADDRA */
	case(0xFFD12044):	/* EMAC0 */
	case(0xFFD12048):	/* EMAC1 */
	case(0xFFD1204C):	/* EMAC2 */
	case(0xFFD12090):	/* ECC_INT_MASK_VALUE */
	case(0xFFD12094):	/* ECC_INT_MASK_SET */
	case(0xFFD12098):	/* ECC_INT_MASK_CLEAR */
	case(0xFFD1209C):	/* ECC_INTSTATUS_SERR */
	case(0xFFD120A0):	/* ECC_INTSTATUS_DERR */
	case(0xFFD12220):	/* BOOT_SCRATCH_COLD8 */
	case(0xFFD12224):	/* BOOT_SCRATCH_COLD9 */
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1,
				   readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_REG_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, reg_addr);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_register_write(unsigned long function_id,
						unsigned long reg_addr,
						unsigned long value)
{
	const u32 *pdata = (u32 *)reg_addr;

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (reg_addr) {
	case(0xF8011100):	/* ECCCTRL1 */
	case(0xF8011104):	/* ECCCTRL2 */
	case(0xF8011110):	/* ERRINTEN */
	case(0xF8011114):	/* ERRINTENS */
	case(0xF8011118):	/* ERRINTENR */
	case(0xF801111C):	/* INTMODE */
	case(0xF8011120):	/* INTSTAT */
	case(0xF8011124):	/* DIAGINTTEST */
	case(0xF801112C):	/* DERRADDRA */
	case(0xFFD12044):	/* EMAC0 */
	case(0xFFD12048):	/* EMAC1 */
	case(0xFFD1204C):	/* EMAC2 */
	case(0xFFD12090):	/* ECC_INT_MASK_VALUE */
	case(0xFFD12094):	/* ECC_INT_MASK_SET */
	case(0xFFD12098):	/* ECC_INT_MASK_CLEAR */
	case(0xFFD1209C):	/* ECC_INTSTATUS_SERR */
	case(0xFFD120A0):	/* ECC_INTSTATUS_DERR */
	case(0xFFD12220):	/* BOOT_SCRATCH_COLD8 */
	case(0xFFD12224):	/* BOOT_SCRATCH_COLD9 */
		writel((u32)value, pdata);
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1, readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_REG_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, reg_addr);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_register_update(unsigned long function_id,
						 unsigned long reg_addr,
						 unsigned long mask,
						 unsigned long value)
{
	const u32 *pdata = (u32 *)reg_addr;

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (reg_addr) {
	case(0xF8011100):	/* ECCCTRL1 */
	case(0xF8011104):	/* ECCCTRL2 */
	case(0xF8011110):	/* ERRINTEN */
	case(0xF8011114):	/* ERRINTENS */
	case(0xF8011118):	/* ERRINTENR */
	case(0xF801111C):	/* INTMODE */
	case(0xF8011120):	/* INTSTAT */
	case(0xF8011124):	/* DIAGINTTEST */
	case(0xF801112C):	/* DERRADDRA */
	case(0xFFD12044):	/* EMAC0 */
	case(0xFFD12048):	/* EMAC1 */
	case(0xFFD1204C):	/* EMAC2 */
	case(0xFFD12090):	/* ECC_INT_MASK_VALUE */
	case(0xFFD12094):	/* ECC_INT_MASK_SET */
	case(0xFFD12098):	/* ECC_INT_MASK_CLEAR */
	case(0xFFD1209C):	/* ECC_INTSTATUS_SERR */
	case(0xFFD120A0):	/* ECC_INTSTATUS_DERR */
	case(0xFFD12220):	/* BOOT_SCRATCH_COLD8 */
	case(0xFFD12224):	/* BOOT_SCRATCH_COLD9 */
		clrsetbits_le32(pdata, (u32)mask, (u32)value);
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1, readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_REG_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, reg_addr);

	SMC_RET_REG_MEM(r);
}
DECLARE_SECURE_SVC(register_read, INTEL_SIP_SMC_REG_READ,
		   smc_socfpga_register_read);
DECLARE_SECURE_SVC(register_write, INTEL_SIP_SMC_REG_WRITE,
		   smc_socfpga_register_write);
DECLARE_SECURE_SVC(register_update, INTEL_SIP_SMC_REG_UPDATE,
		   smc_socfpga_register_update);
