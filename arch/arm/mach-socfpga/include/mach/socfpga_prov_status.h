/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
 *
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 * U-Boot GET_PROVISION ownership helpers for ES-14722.
 * Do not copy this header or its names into TF-A (BSD-3-Clause tree).
 */

#ifndef _SOCFPGA_PROV_STATUS_H_
#define _SOCFPGA_PROV_STATUS_H_

#include <linux/types.h>
#include <stdbool.h>

/*
 * MBOX_FCS_GET_PROVISION / GET_PROV_DATA (0x7B) payload words, from
 * Mailbox_Command_and_Response §GET_PROV_DATA and Linux
 * fcs_get_provision_header. Same words on the raw SDM mailbox (SPL,
 * Proper SMC relay, ATF BL2).
 *
 *   word 0  provision-flow status this POR (0/1/2 + S). Not ownership.
 *   word 1  Intel key cancellation
 *   word 2  [7:0]  secure state / type: 0 = not OWNED, 1 = SHA-256,
 *                  2 = OWNED SHA-384
 *           [15:8] hash-count encoding: actual hashes = value + 1;
 *                  0xFF = no root hash programmed
 *   word 3+ first owner root hash
 *
 * QS-580661: older SDM rejects 0x7B with 0x85 when not owned.
 */
#define FCS_PROV_DATA_WORD_SIZE			44U
#define FCS_PROV_HEADER_WORD_SIZE		3U
#define FCS_PROV_HASHINFO_WORD			2U
#define FCS_PROV_SECURE_NOT_OWNED		0U
#define FCS_PROV_TYPE_HASH256			1U
#define FCS_PROV_TYPE_HASH384			2U
#define FCS_PROV_NUM_HASHES_NONE		0xFFU
#define FCS_PROV_OWNER_ROOT_HASH256_SIZE	32U
#define FCS_PROV_OWNER_ROOT_HASH384_SIZE	48U
#define FCS_PROV_OWNER_ROOT_HASH256_WORD_SIZE	8U
#define FCS_PROV_OWNER_ROOT_HASH384_WORD_SIZE	12U

enum socfpga_prov_state {
	SOCFPGA_PROV_OWNED = 0,
	SOCFPGA_PROV_NON_OWNED = 1,
	SOCFPGA_PROV_QUERY_ERROR = -1,
};

static inline bool fcs_prov_owner_root_hash_is_zero(const u8 *hash, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (hash[i])
			return false;
	}

	return true;
}

/*
 * Parse a successful GET_PROVISION response blob.
 * Caller must reject mailbox errors before calling this.
 * Word 0 is ignored as an ownership oracle.
 */
static inline int socfpga_prov_state_from_blob(const u32 *prov_data, u32 resp_len)
{
	u32 hashinfo;
	u8 type_hash, num_hashes;
	const u8 *owner_hash;
	size_t hash_len, hash_words;

	if (!prov_data || resp_len < FCS_PROV_HEADER_WORD_SIZE)
		return SOCFPGA_PROV_QUERY_ERROR;

	hashinfo = prov_data[FCS_PROV_HASHINFO_WORD];
	type_hash = hashinfo & 0xff;
	num_hashes = (hashinfo >> 8) & 0xff;

	if (type_hash == FCS_PROV_SECURE_NOT_OWNED ||
	    num_hashes == FCS_PROV_NUM_HASHES_NONE)
		return SOCFPGA_PROV_NON_OWNED;

	if (type_hash == FCS_PROV_TYPE_HASH384) {
		hash_len = FCS_PROV_OWNER_ROOT_HASH384_SIZE;
		hash_words = FCS_PROV_OWNER_ROOT_HASH384_WORD_SIZE;
	} else if (type_hash == FCS_PROV_TYPE_HASH256) {
		hash_len = FCS_PROV_OWNER_ROOT_HASH256_SIZE;
		hash_words = FCS_PROV_OWNER_ROOT_HASH256_WORD_SIZE;
	} else {
		return SOCFPGA_PROV_QUERY_ERROR;
	}

	if (resp_len < FCS_PROV_HEADER_WORD_SIZE + hash_words)
		return SOCFPGA_PROV_QUERY_ERROR;

	owner_hash = (const u8 *)&prov_data[FCS_PROV_HEADER_WORD_SIZE];

	if (fcs_prov_owner_root_hash_is_zero(owner_hash, hash_len))
		return SOCFPGA_PROV_NON_OWNED;

	return SOCFPGA_PROV_OWNED;
}

/*
 * Map GET_PROVISION mailbox return to ownership state.
 * SDM returns 0x85 when the device is not owned (QS-580661 / VAB_SDOS).
 * That is the unowned oracle for this command. Residual: if SDM ever
 * returns 0x85 on an OWNED device, bypass would still open (SDM-14722-03
 * class, now on 0x7B). Other mailbox errors stay QUERY_ERROR (fail closed).
 */
static inline int socfpga_prov_state_from_mbox_ret(int mbox_ret,
						   const u32 *prov_data,
						   u32 resp_len,
						   u32 not_allowed_code)
{
	if (mbox_ret == (int)not_allowed_code)
		return SOCFPGA_PROV_NON_OWNED;
	if (mbox_ret)
		return SOCFPGA_PROV_QUERY_ERROR;

	return socfpga_prov_state_from_blob(prov_data, resp_len);
}

#endif /* _SOCFPGA_PROV_STATUS_H_ */
