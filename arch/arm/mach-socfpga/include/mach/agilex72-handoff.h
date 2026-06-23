/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#ifndef _MACH_AGILEX72_HANDOFF_H_
#define _MACH_AGILEX72_HANDOFF_H_

#include <linux/types.h>

#define AGILEX72_HANDOFF_MAGIC		0x47323741U

#define AGILEX72_HANDOFF_VERSION_1		1U
#define AGILEX72_HANDOFF_VERSION_CURRENT	AGILEX72_HANDOFF_VERSION_1

#define AGILEX72_HANDOFF_KEY_MAX		32U

enum agilex72_handoff_type {
	AGILEX72_HANDOFF_TYPE_KV		= 1,
	AGILEX72_HANDOFF_TYPE_REG_ABS		= 2,
	AGILEX72_HANDOFF_TYPE_KV_STRING	= 3,
};

struct agilex72_handoff_header {
	u32 magic;
	u32 version;
	u32 entry_count;
	u32 total_size;
	u32 crc32;
};

struct agilex72_handoff_entry {
	u32 type;
	u32 flags;
	u32 length;
	u32 data_offset;
};

struct agilex72_kv_payload_header {
	u32 version;
	u32 pair_count;
	u32 total_length;
};

struct agilex72_kv_entry {
	u32 key_len;
	u32 value_len;
};

struct agilex72_kv_string_entry {
	u32 key_len;
	u32 value_len;
};

struct agilex72_reg_abs_payload_header {
	u32 payload_version;
	u32 base_address;
	u32 entry_count;
	u32 flags;
};

struct agilex72_reg_bit_field_entry {
	u32 offset;
	u32 mask;
	u32 value;
};

void clk_mgr_init_from_blob(void);

#endif /* _MACH_AGILEX72_HANDOFF_H_ */
