// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <errno.h>
#include <hang.h>
#include <log.h>
#include <u-boot/crc.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <asm/arch/agilex72-handoff.h>

#include "agilex72-clkmgr.h"

#define AGILEX72_HANDOFF_MAX_REG_ABS_PER_SECTION	32
#define AGILEX72_HANDOFF_MAX_KV_PER_SECTION		8
#define AGILEX72_HANDOFF_VALUE_MAX			32

static int agilex72_handoff_validate_header(const struct agilex72_handoff_header *hdr,
					    size_t blob_max)
{
	if (hdr->magic != AGILEX72_HANDOFF_MAGIC) {
		pr_err("agilex72-handoff: bad magic 0x%08x (expected 0x%08x)\n",
		       hdr->magic, AGILEX72_HANDOFF_MAGIC);
		return -EINVAL;
	}

	if (hdr->version != AGILEX72_HANDOFF_VERSION_CURRENT) {
		pr_err("agilex72-handoff: unsupported version %u (parser knows %u)\n",
		       hdr->version, AGILEX72_HANDOFF_VERSION_CURRENT);
		return -EPROTO;
	}

	if (hdr->total_size < sizeof(*hdr)) {
		pr_err("agilex72-handoff: total_size %u < header %zu\n",
		       hdr->total_size, sizeof(*hdr));
		return -EINVAL;
	}

	if (hdr->total_size > blob_max) {
		pr_err("agilex72-handoff: total_size %u > slot %zu\n",
		       hdr->total_size, blob_max);
		return -EINVAL;
	}

	if ((size_t)hdr->entry_count * sizeof(struct agilex72_handoff_entry) +
	    sizeof(*hdr) > hdr->total_size) {
		pr_err("agilex72-handoff: entry_count %u overflows total_size %u\n",
		       hdr->entry_count, hdr->total_size);
		return -EINVAL;
	}

	/*
	 * crc32 == 0 means the producer omitted the checksum (e.g. a hand
	 * assembled blob); only validate when a CRC is actually present so
	 * corrupted/truncated blobs are still rejected on every build,
	 * including the embedded demo path.
	 */
	if (hdr->crc32) {
		u32 computed;

		computed = crc32(0, (const unsigned char *)hdr + sizeof(*hdr),
				 hdr->total_size - sizeof(*hdr));
		if (computed != hdr->crc32) {
			pr_err("agilex72-handoff: CRC mismatch (hdr=0x%08x computed=0x%08x)\n",
			       hdr->crc32, computed);
			return -EBADMSG;
		}
	}

	return 0;
}

static int agilex72_handoff_apply_reg_abs_section(const u8 *blob, u32 blob_size,
						  const struct agilex72_handoff_entry *e)
{
	struct agilex72_clkmgr_reg_abs_entry tmp[AGILEX72_HANDOFF_MAX_REG_ABS_PER_SECTION];
	const struct agilex72_reg_abs_payload_header *sec_hdr;
	const u8 *body;
	u32 i, count, base;
	int ret;

	if (e->length * 4U > blob_size - e->data_offset) {
		pr_err("agilex72-handoff: REG_ABS section runs past blob end\n");
		return -EINVAL;
	}
	if (e->length * 4U < sizeof(*sec_hdr)) {
		pr_err("agilex72-handoff: REG_ABS section too short for header\n");
		return -EINVAL;
	}

	sec_hdr = (const struct agilex72_reg_abs_payload_header *)(blob +
							     e->data_offset);
	count = sec_hdr->entry_count;
	base = sec_hdr->base_address;
	body = (const u8 *)(sec_hdr + 1);

	if (count > AGILEX72_HANDOFF_MAX_REG_ABS_PER_SECTION) {
		pr_err("agilex72-handoff: REG_ABS section has %u entries (max %u)\n",
		       count, AGILEX72_HANDOFF_MAX_REG_ABS_PER_SECTION);
		return -EOVERFLOW;
	}

	if ((u32)count * sizeof(struct agilex72_reg_bit_field_entry) +
	    sizeof(*sec_hdr) > e->length * 4U) {
		pr_err("agilex72-handoff: REG_ABS section length mismatch\n");
		return -EINVAL;
	}

	pr_debug("agilex72-handoff: REG_ABS @ base 0x%08x, %u entries\n",
		 base, count);

	for (i = 0; i < count; i++) {
		const struct agilex72_reg_bit_field_entry *rbf =
			(const struct agilex72_reg_bit_field_entry *)
			(body + i * sizeof(*rbf));

		tmp[i].addr = base + rbf->offset;
		tmp[i].value = rbf->value;
		tmp[i].mask = rbf->mask;
	}

	ret = agilex72_clkmgr_apply_reg_abs(tmp, (int)count);
	if (ret) {
		pr_err("agilex72-handoff: apply_reg_abs failed: %d\n", ret);
		return ret;
	}

	agilex72_clkmgr_bisect_reg_abs(base, count);
	return 0;
}

static int agilex72_handoff_apply_kv_string_section(const u8 *blob, u32 blob_size,
						    const struct agilex72_handoff_entry *e)
{
	struct agilex72_clkmgr_entry kv[AGILEX72_HANDOFF_MAX_KV_PER_SECTION];
	char key_buf[AGILEX72_HANDOFF_MAX_KV_PER_SECTION][AGILEX72_HANDOFF_KEY_MAX + 1];
	char value_buf[AGILEX72_HANDOFF_MAX_KV_PER_SECTION][AGILEX72_HANDOFF_VALUE_MAX + 1];
	const struct agilex72_kv_payload_header *sec_hdr;
	const u8 *cur, *end;
	u32 i, count;
	int ret;

	if (e->length * 4U > blob_size - e->data_offset) {
		pr_err("agilex72-handoff: KV_STRING section runs past blob end\n");
		return -EINVAL;
	}
	if (e->length * 4U < sizeof(*sec_hdr)) {
		pr_err("agilex72-handoff: KV_STRING section too short for header\n");
		return -EINVAL;
	}

	sec_hdr = (const struct agilex72_kv_payload_header *)(blob + e->data_offset);
	count = sec_hdr->pair_count;
	cur = (const u8 *)(sec_hdr + 1);
	end = blob + e->data_offset + e->length * 4U;

	if (count > AGILEX72_HANDOFF_MAX_KV_PER_SECTION) {
		pr_err("agilex72-handoff: KV_STRING section has %u entries (max %u)\n",
		       count, AGILEX72_HANDOFF_MAX_KV_PER_SECTION);
		return -EOVERFLOW;
	}

	pr_debug("agilex72-handoff: KV_STRING section, %u entries\n", count);

	for (i = 0; i < count; i++) {
		u32 key_len, value_len, key_pad, value_pad;
		const struct agilex72_kv_string_entry *hdr;

		if (cur + sizeof(*hdr) > end) {
			pr_err("agilex72-handoff: KV_STRING entry header runs past end\n");
			return -EINVAL;
		}

		hdr = (const struct agilex72_kv_string_entry *)cur;
		key_len = hdr->key_len;
		value_len = hdr->value_len;
		key_pad = ALIGN(key_len, 4U);
		value_pad = ALIGN(value_len, 4U);

		if (key_len > AGILEX72_HANDOFF_KEY_MAX) {
			pr_err("agilex72-handoff: KV_STRING key_len %u > %u\n",
			       key_len, AGILEX72_HANDOFF_KEY_MAX);
			return -EINVAL;
		}
		if (value_len > AGILEX72_HANDOFF_VALUE_MAX) {
			pr_err("agilex72-handoff: KV_STRING value_len %u > %u\n",
			       value_len, AGILEX72_HANDOFF_VALUE_MAX);
			return -EINVAL;
		}

		cur += sizeof(*hdr);
		if (cur + key_pad + value_pad > end) {
			pr_err("agilex72-handoff: KV_STRING payload runs past end\n");
			return -EINVAL;
		}

		memcpy(key_buf[i], cur, key_len);
		key_buf[i][key_len] = '\0';
		cur += key_pad;

		memcpy(value_buf[i], cur, value_len);
		value_buf[i][value_len] = '\0';
		cur += value_pad;

		kv[i].key = key_buf[i];
		kv[i].value = value_buf[i];

		pr_debug("agilex72-handoff: KV [%u] %s = %s\n",
			 i, kv[i].key, kv[i].value);
	}

	if (count > 0 && !strcmp(key_buf[0], "pll_enable")) {
		agilex72_clkmgr_pll_cfg5_rearm();
		agilex72_clkmgr_bisect_cfg5_rearm_done();
	}

	ret = agilex72_clkmgr_apply_handoff(kv, (int)count);
	if (ret) {
		pr_err("agilex72-handoff: apply_handoff failed: %d\n", ret);
		return ret;
	}

	if (count > 0)
		agilex72_clkmgr_bisect_kv_milestone(key_buf[0], ret);

	return 0;
}

int agilex72_handoff_parse_and_apply(const void *blob, size_t blob_max)
{
	const struct agilex72_handoff_header *hdr;
	const u8 *blob_u8;
	const struct agilex72_handoff_entry *entries;
	u32 i;
	int ret;

	if (!blob) {
		pr_err("agilex72-handoff: NULL blob\n");
		return -EINVAL;
	}
	if (blob_max < sizeof(*hdr)) {
		pr_err("agilex72-handoff: slot %zu < header %zu\n",
		       blob_max, sizeof(*hdr));
		return -EINVAL;
	}
	if ((uintptr_t)blob & 3) {
		pr_err("agilex72-handoff: blob ptr 0x%lx not word-aligned\n",
		       (unsigned long)(uintptr_t)blob);
		return -EINVAL;
	}

	blob_u8 = blob;
	hdr = blob;

	ret = agilex72_handoff_validate_header(hdr, blob_max);
	if (ret)
		return ret;

	pr_info("agilex72-handoff: applying blob magic=0x%08x ver=%u entries=%u size=%u\n",
		hdr->magic, hdr->version, hdr->entry_count, hdr->total_size);

	entries = (const struct agilex72_handoff_entry *)(blob_u8 + sizeof(*hdr));

	for (i = 0; i < hdr->entry_count; i++) {
		const struct agilex72_handoff_entry *e = &entries[i];

		if (e->data_offset >= hdr->total_size) {
			pr_err("agilex72-handoff: entry[%u] data_offset 0x%x past blob end\n",
			       i, e->data_offset);
			return -EINVAL;
		}
		if (e->data_offset & 3) {
			pr_err("agilex72-handoff: entry[%u] data_offset 0x%x not word-aligned\n",
			       i, e->data_offset);
			return -EINVAL;
		}

		switch (e->type) {
		case AGILEX72_HANDOFF_TYPE_REG_ABS:
			ret = agilex72_handoff_apply_reg_abs_section(blob_u8,
								     hdr->total_size,
							       e);
			break;
		case AGILEX72_HANDOFF_TYPE_KV_STRING:
			ret = agilex72_handoff_apply_kv_string_section(blob_u8,
								       hdr->total_size,
								 e);
			break;
		case AGILEX72_HANDOFF_TYPE_KV:
			pr_debug("agilex72-handoff: entry[%u] KV (skipped by clkmgr parser)\n",
				 i);
			ret = 0;
			break;
		default:
			pr_warn("agilex72-handoff: entry[%u] unknown type %u, skipping\n",
				i, e->type);
			ret = 0;
			break;
		}

		if (ret) {
			pr_err("agilex72-handoff: entry[%u] (type %u) failed: %d\n",
			       i, e->type, ret);
			return ret;
		}
	}

	pr_info("agilex72-handoff: blob applied (%u entries)\n", hdr->entry_count);
	return 0;
}
