/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Minimal native-host shim for U-Boot's <linux/bitops.h>. Defines BIT
 * and GENMASK in a form compatible with the kernel headers, sufficient
 * for the RSU production sources we currently include.
 */
#ifndef __TEST_ASAN_LINUX_BITOPS_H__
#define __TEST_ASAN_LINUX_BITOPS_H__

#include <asm/types.h>

/*
 * Match the host's actual `unsigned long` width so the GENMASK shift
 * stays in range on both 32-bit and 64-bit build hosts. Hard-coding 64
 * here would invoke UB in the (BITS_PER_LONG - 1 - (h)) shift on a
 * 32-bit host and silently produce wrong masks.
 */
#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#endif

#define BIT(nr)  (1UL << (nr))
#define BIT_ULL(nr) (1ULL << (nr))

#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
	(((~0ULL) - (1ULL << (l)) + 1) & (~0ULL >> (64 - 1 - (h))))

#endif /* __TEST_ASAN_LINUX_BITOPS_H__ */
