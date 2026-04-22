/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Minimal native-host shim for U-Boot's <linux/bitfield.h>. Provides
 * FIELD_GET / FIELD_PREP / __ffs as used by RSU production sources.
 */
#ifndef __TEST_ASAN_LINUX_BITFIELD_H__
#define __TEST_ASAN_LINUX_BITFIELD_H__

#include <linux/bitops.h>

static inline unsigned long __test_asan_ffs_ul(unsigned long mask)
{
	unsigned long shift = 0;

	if (!mask)
		return 0;
	while (!(mask & 1UL)) {
		mask >>= 1;
		shift++;
	}
	return shift;
}

#define FIELD_GET(_mask, _val)  \
	(((_val) & (_mask)) >> __test_asan_ffs_ul(_mask))

#define FIELD_PREP(_mask, _val) \
	(((unsigned long)(_val) << __test_asan_ffs_ul(_mask)) & (_mask))

#endif /* __TEST_ASAN_LINUX_BITFIELD_H__ */
