// SPDX-License-Identifier: GPL-2.0+
/*
 * Host-side GET_PROVISION ownership decode tests (ES-14722).
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <linux/errno.h>
#include <string.h>
#include <test/socfpga.h>
#include <test/ut.h>

#include "../../arch/arm/mach-socfpga/include/mach/socfpga_prov_status.h"

#define PROV_0x85	0x85U

static int socfpga_test_prov_mbox_85_is_unowned(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	ut_asserteq(SOCFPGA_PROV_NON_OWNED,
		    socfpga_prov_state_from_mbox_ret((int)PROV_0x85, blob,
						     FCS_PROV_DATA_WORD_SIZE,
						     PROV_0x85));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_mbox_85_is_unowned, 0);

static int socfpga_test_prov_mbox_other_err(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	ut_asserteq(SOCFPGA_PROV_QUERY_ERROR,
		    socfpga_prov_state_from_mbox_ret(-ETIMEDOUT, blob,
						     FCS_PROV_DATA_WORD_SIZE,
						     PROV_0x85));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_mbox_other_err, 0);

static void fill_hashinfo(u32 *blob, u8 type, u8 num)
{
	blob[2] = (u32)type | ((u32)num << 8);
}

static int socfpga_test_prov_word0_not_owner(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	/* Word 0 = 0 (no provision this POR) must not imply NON_OWNED. */
	blob[0] = 0;
	blob[1] = 0;
	fill_hashinfo(blob, FCS_PROV_TYPE_HASH384, 1);
	blob[3] = 0xa5a5a5a5;
	ut_asserteq(SOCFPGA_PROV_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_word0_not_owner, 0);

static int socfpga_test_prov_zero_hash_unowned(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	blob[0] = 1;
	fill_hashinfo(blob, FCS_PROV_TYPE_HASH384, 1);
	ut_asserteq(SOCFPGA_PROV_NON_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_zero_hash_unowned, 0);

static int socfpga_test_prov_num_hashes_zero_is_one_hash(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	/*
	 * Mailbox spec: [15:8] == 0 means one hash (value + 1).
	 * SDM dump 0x4000, 0, 2 is OWNED SHA-384 with one hash.
	 */
	memset(blob, 0, sizeof(blob));
	blob[0] = 0x4000;
	fill_hashinfo(blob, FCS_PROV_TYPE_HASH384, 0);
	blob[3] = 0xa5a5a5a5;
	ut_asserteq(SOCFPGA_PROV_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_num_hashes_zero_is_one_hash, 0);

static int socfpga_test_prov_secure_state_zero(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	fill_hashinfo(blob, FCS_PROV_SECURE_NOT_OWNED, 0);
	ut_asserteq(SOCFPGA_PROV_NON_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_secure_state_zero, 0);

static int socfpga_test_prov_num_hashes_none(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	memset(blob, 0, sizeof(blob));
	fill_hashinfo(blob, FCS_PROV_TYPE_HASH384, FCS_PROV_NUM_HASHES_NONE);
	ut_asserteq(SOCFPGA_PROV_NON_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_num_hashes_none, 0);

static int socfpga_test_prov_short_blob(struct unit_test_state *uts)
{
	u32 blob[2] = { 0, 0 };

	ut_asserteq(SOCFPGA_PROV_QUERY_ERROR,
		    socfpga_prov_state_from_blob(blob, 2));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_short_blob, 0);

static int socfpga_test_prov_legacy_word1_is_not_type(struct unit_test_state *uts)
{
	u32 blob[FCS_PROV_DATA_WORD_SIZE];

	/*
	 * Old 2-word parser read type/count from word 1. A cancel-status
	 * of 0 there must not be treated as type 0 / QUERY_ERROR when
	 * word 2 has a valid SHA-384 hash.
	 */
	memset(blob, 0, sizeof(blob));
	blob[1] = 0;
	fill_hashinfo(blob, FCS_PROV_TYPE_HASH384, 1);
	blob[3] = 0x11111111;
	ut_asserteq(SOCFPGA_PROV_OWNED,
		    socfpga_prov_state_from_blob(blob, FCS_PROV_DATA_WORD_SIZE));
	return 0;
}

SOCFPGA_TEST(socfpga_test_prov_legacy_word1_is_not_type, 0);
