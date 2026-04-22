/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Minimal native-host shim for U-Boot's <asm/arch/smc_api.h>. Only
 * declares invoke_smc(), since that's the SMC bridge that the RSU
 * production code reaches into when CONFIG_SPL_ATF is enabled.
 *
 * For the host harness we keep this stub deliberately small; the real
 * SMC machinery (SDM mailbox, ATF handover, secure-RAM mapping) is not
 * exercised on host.
 */
#ifndef __TEST_ASAN_ASM_ARCH_SMC_API_H__
#define __TEST_ASAN_ASM_ARCH_SMC_API_H__

#include <asm/types.h>

int invoke_smc(u32 func_id, u64 *args, int arg_len, u64 *ret_arg, int ret_len);

#endif /* __TEST_ASAN_ASM_ARCH_SMC_API_H__ */
