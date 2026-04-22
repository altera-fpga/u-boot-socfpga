/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Native-host stub for U-Boot's <asm/system.h>. The RSU production
 * sources transitively include this for cache / memory barrier
 * primitives that aren't reachable on a native host build, so we
 * provide an empty header.
 */
#ifndef __TEST_ASAN_ASM_SYSTEM_H__
#define __TEST_ASAN_ASM_SYSTEM_H__

#endif /* __TEST_ASAN_ASM_SYSTEM_H__ */
