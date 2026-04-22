/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Native-host shim for U-Boot's <linux/intel-smc.h>. Provides only the
 * Intel SiP SMC IDs that the RSU production sources reference.
 */
#ifndef __TEST_ASAN_LINUX_INTEL_SMC_H__
#define __TEST_ASAN_LINUX_INTEL_SMC_H__

#define INTEL_SIP_SMC_RSU_COPY_DCMF_VERSION   0xC2000020
#define INTEL_SIP_SMC_RSU_COPY_DCMF_STATUS    0xC2000021
#define INTEL_SIP_SMC_RSU_COPY_MAX_RETRY      0xC2000022

#endif /* __TEST_ASAN_LINUX_INTEL_SMC_H__ */
