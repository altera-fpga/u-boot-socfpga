/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Native-host shim for U-Boot's <asm/types.h>. Provides the kernel-style
 * fixed-width integer typedefs that production U-Boot sources expect.
 * Avoids pulling in the actual ARM <asm/types.h> on the host build, which
 * would drag in a whole arch tree.
 */
#ifndef __TEST_ASAN_ASM_TYPES_H__
#define __TEST_ASAN_ASM_TYPES_H__

#include <stdint.h>
#include <stddef.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef uint8_t   __u8;
typedef uint16_t  __u16;
typedef uint32_t  __u32;
typedef uint64_t  __u64;

typedef unsigned long ulong;
typedef unsigned long phys_addr_t;
typedef unsigned long phys_size_t;

#endif /* __TEST_ASAN_ASM_TYPES_H__ */
