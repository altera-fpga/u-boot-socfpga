/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <asm/io.h>
#include <asm/psci.h>
#include <errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/reset_manager_soc64.h>
#include <asm/arch/rsu_s10.h>
#include <asm/secure.h>

static u64 psci_cpu_on_64_cpuid __secure_data;
static u64 psci_cpu_on_64_entry_point __secure_data;

void __noreturn __secure psci_system_reset(void)
{
	if (smc_rsu_update_address)
		mbox_rsu_update_psci(&smc_rsu_update_address);
	else
		mbox_send_cmd_psci(MBOX_ID_UBOOT, MBOX_REBOOT_HPS,
				   MBOX_CMD_DIRECT, 0, NULL, 0, 0, NULL);

	while (1)
		;
}

/*
 * RESET2 is assumed to be a standard PSCI warm reset.
 *
 * reset_type bit[31]   == 1: Vendor-specific implementation, may utilize cookie
 *
 * reset_type bit[31]   == 0: Architectural reset.
 *            bit[30:0] == 0: Warm reset
 */
int __secure psci_system_reset2_64(u32 function_id, u32 reset_type, u64 cookie)
{
	if (reset_type != 0)
		return ARM_PSCI_RET_INVAL;

	l2_reset_cpu_psci();

	return ARM_PSCI_RET_INTERNAL_FAILURE; /* Never reached */
}

int __secure psci_features(u32 function_id, u32 psci_fid)
{
	switch (psci_fid) {
	case ARM_PSCI_0_2_FN_CPU_ON:
	case ARM_PSCI_0_2_FN_SYSTEM_RESET:
	case ARM_PSCI_1_0_FN64_SYSTEM_RESET2:
		return 0x0;
	}

	return ARM_PSCI_RET_NI;
}

/* This function will handle multiple core release based PSCI */
void __secure psci_cpu_on_64_mpidr(void)
{
	asm volatile(
		".align	5			\n"
		"1:	wfe			\n"
		"	ldr	x0, [%0]	\n"
		"	ldr	x1, [%1]	\n"
		"	mrs	x2, mpidr_el1	\n"
		"	and	x2, x2, #0xff	\n"
		"	cmp	x0, x2		\n"
		"	b.ne	1b		\n"
		"	br	x1		\n"
	: : "r"(&psci_cpu_on_64_cpuid), "r"(&psci_cpu_on_64_entry_point)
	: "x0", "x1", "x2", "memory", "cc");
}

int __secure psci_cpu_on_64(u32 function_id, u64 cpuid, u64 entry_point)
{
	/* Releases all secondary CPUs to jump into psci_cpu_on_64_mpidr */
	writeq(0, &psci_cpu_on_64_cpuid);
	writeq(0, &psci_cpu_on_64_entry_point);
	writeq((u64)&psci_cpu_on_64_mpidr, CPU_RELEASE_ADDR);

	/* to store in global so psci_cpu_on_64_mpidr function can refer */
	writeq(entry_point, &psci_cpu_on_64_entry_point);
	dsb();
	writeq(cpuid, &psci_cpu_on_64_cpuid);
	dsb();

	asm volatile("sev");

	return ARM_PSCI_RET_SUCCESS;
}
