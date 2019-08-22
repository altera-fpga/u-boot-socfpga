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
/* Master CPU (CPU0) is ON. slave CPUs are OFF */
static u64 psci_affinity_info_64_state __secure_data =
	((PSCI_AFFINITY_LEVEL_OFF << 6) | (PSCI_AFFINITY_LEVEL_OFF << 4) |
	(PSCI_AFFINITY_LEVEL_OFF << 2) | PSCI_AFFINITY_LEVEL_ON);

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
		"	lsl	x0, x0, #0x01	\n"
		/* Set current CPU state to ON */
		"2:	ldaxr	x2, [%2]	\n"
		"	mov	x3, #0x03	\n"
		"	lsl	x3, x3, x0	\n"
		"	bic	x2, x2, x3	\n"
		"	mov	x3, %[state_on]	\n"
		"	lsl	x3, x3, x0	\n"
		"	orr	x2, x2, x3	\n"
		"	stlxr	w3, x2, [%2]	\n"
		"	cbnz	w3, 2b		\n"
		/* Jump to entry point */
		"	br	x1		\n"
	: : "r"(&psci_cpu_on_64_cpuid),
	"r"(&psci_cpu_on_64_entry_point),
	"r"(&psci_affinity_info_64_state),
	[state_on] "I" (PSCI_AFFINITY_LEVEL_ON)
	: "x0", "x1", "x2", "x3", "memory", "cc");
}

void __secure psci_cpu_off(u32 function_id)
{
	asm volatile(
		"	str	xzr, [%0]	\n"
		"	str	xzr, [%1]	\n"
		/* Set current CPU state to OFF */
		"	mrs	x1, mpidr_el1	\n"
		"	and	x1, x1, #0xff	\n"
		"	lsl	x1, x1, #0x01	\n"
		"1:	ldaxr	x2, [%3]	\n"
		"	mov	x3, #0x03	\n"
		"	lsl	x3, x3, x1	\n"
		"	bic	x2, x2, x3	\n"
		"	mov	x3, %[state_off]\n"
		"	lsl	x3, x3, x1	\n"
		"	orr	x2, x2, x3	\n"
		"	stlxr	w3, x2, [%3]	\n"
		"	cbnz	w3, 1b		\n"
		"	mrs	x2, spsr_el3	\n"
		"	bic	x2, x2, #0x0f	\n"
		"	mov	x3, #0x09	\n"
		"	orr	x2, x2, x3	\n"
		"	msr	spsr_el3, x2	\n"
		/* Switch to EL2 when return from exception */
		"	msr	elr_el3, %2	\n"
		"	eret			\n"
	: : "r"(&psci_cpu_on_64_cpuid),
	"r"(&psci_cpu_on_64_entry_point),
	"r"(psci_cpu_on_64_mpidr),
	"r"(&psci_affinity_info_64_state),
	[state_off] "I" (PSCI_AFFINITY_LEVEL_OFF)
	: "x0", "x1", "x2", "x3", "memory", "cc");
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

/* Return the state of current CPU */
int __secure psci_affinity_info_64(u32 function_id, u64 cpuid)
{
	return (psci_affinity_info_64_state >> (cpuid << 1)) & 0x03;
}
