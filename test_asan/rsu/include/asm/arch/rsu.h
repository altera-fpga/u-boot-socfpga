/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Native-host wrapper that forwards to the real U-Boot RSU header.
 * Using the host-shim include tree first (-Itest_asan/rsu/include)
 * means the transitive includes from the production header
 * (<asm/types.h>, <linux/bitops.h>, <linux/bitfield.h>) resolve to our
 * shims, not to the ARM kernel headers, so the file compiles natively.
 */
#ifndef __TEST_ASAN_ASM_ARCH_RSU_H__
#define __TEST_ASAN_ASM_ARCH_RSU_H__

#include "../../../../../arch/arm/mach-socfpga/include/mach/rsu.h"

#endif /* __TEST_ASAN_ASM_ARCH_RSU_H__ */
