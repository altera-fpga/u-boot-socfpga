/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <common.h>
#include <linux/compat.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_ll.h>
#include <asm/arch/rsu_misc.h>
#include <asm/types.h>
#include <u-boot/zlib.h>
#include <errno.h>
#include <exports.h>

#define LOG_BUF_SIZE	1024

static char *cb_buffer;
static int cb_buffer_togo;

static char *reserved_names[] = {
	"BOOT_INFO",
	"FACTORY_IMAGE",
	"SPT",
	"SPT0",
	"SPT1",
	"CPB",
	"CPB0",
	"CPB1",
	""
};

struct pointer_block {
	u32 num_ptrs;
	u32 RSVD0;
	u64 ptrs[4];
	u8 RSVD1[0xd4];
	u32 crc;
};

/**
 * swap_bits() - swap bits
 * @data: pointer point to data
 * @len: data length
 */
static void swap_bits(char *data, int len)
{
	int x, y;
	char tmp;

	for (x = 0; x < len; x++) {
		tmp = 0;
		for (y = 0; y < 8; y++) {
			tmp <<= 1;
			if (data[x] & 1)
				tmp |= 1;
			data[x] >>= 1;
		}
		data[x] = tmp;
	}
}

/**
 * rsu_misc_image_adjust() - adjust values in the pointer block
 *
 * @block - pointer to the start of the second 4KB block of an image
 * @info - rsu_slot_info structure for target slot
 *
 * Adjust values in the 256 byte pointer block for the offset of the
 * slot being programmed. The pointer block is in the second 4KB block
 * of the image. The pointer block contains a CRC of the entire 4KB block.
 *
 * Returns 0 on success and block was adjusted, or -1 on error
 */
int rsu_misc_image_adjust(void *block, struct rsu_slot_info *info)
{
	u32 calc_crc;
	int x;
	char *data = (char *)block;
	struct pointer_block *ptr_blk = (struct pointer_block *)(data
					 + IMAGE_PTR_START
					 - IMAGE_PTR_BLOCK);

	/*
	 * Check CRC on 4KB block before proceeding.  All bytes must be
	 * bit-swapped before they can used in zlib CRC32 library function
	 * The CRC value is stored in big endian in the bitstream.
	 */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, IMAGE_PTR_CRC - IMAGE_BLOCK_SZ);
	if (be32_to_cpu(ptr_blk->crc) != calc_crc) {
		rsu_log(RSU_ERR, "Bad CRC. Calc = %08X / Fm Block = %08x",
			calc_crc, be32_to_cpu(ptr_blk->crc));
		return -1;
	}
	swap_bits(block, IMAGE_BLOCK_SZ);

	/*
	 * Adjust main image pointers after they pass a validity test.
	 * Return -1 if an error is found, or 0 if the block looks OK
	 * (adjusted or not adjusted)
	 */
	if (ptr_blk->num_ptrs == 0)
		return 0;

	if (ptr_blk->num_ptrs > 4) {
		rsu_log(RSU_ERR, "Invalid number of pointers in block\n");
		return -1;
	}

	for (x = 0; x < ptr_blk->num_ptrs; x++) {
		if (ptr_blk->ptrs[x] > (u64)info->size) {
			rsu_log(RSU_DEBUG, "A ptr > 0x%llX, not adjust\n",
				(u64)info->size);

			for (x = 0; x < ptr_blk->num_ptrs; x++) {
				if (ptr_blk->ptrs[x] < info->offset ||
				    ptr_blk->ptrs[x] >=
				    (info->offset + info->size)) {
					rsu_log(RSU_ERR,
						"ptr not in the slot\n");
					return -1;
				}
			}

			return 0;
		}
	}

	for (x = 0; x < ptr_blk->num_ptrs; x++)
		ptr_blk->ptrs[x] += info->offset;

	/* Update CRC in block */
	swap_bits(block, IMAGE_BLOCK_SZ);
	calc_crc = crc32(0, block, IMAGE_PTR_CRC - IMAGE_BLOCK_SZ);
	ptr_blk->crc = cpu_to_be32(calc_crc);
	swap_bits(block, IMAGE_BLOCK_SZ);

	return 0;
}

/**
 * rsu_misc_is_rsvd_name() - check if a reserved name
 *
 * @name: name to check
 *
 * Returns 1 if a reserved name, or 0 for not
 */
int rsu_misc_is_rsvd_name(char *name)
{
	int x;

	for (x = 0; reserved_names[x][0] != '\0'; x++)
		if (strcmp(name, reserved_names[x]) == 0)
			return 1;

	return 0;
}

/**
 * rsu_misc_is_slot() - check if a read only or reserved partition
 * @ll_intf: pointer to ll_intf
 * @part_num: partition number
 *
 * Return 1 if not read only or reserved, or 0 for is
 */
int rsu_misc_is_slot(struct rsu_ll_intf *ll_intf, int part_num)
{
	if (ll_intf->partition.readonly(part_num) ||
	    ll_intf->partition.reserved(part_num))
			return 0;

	if (rsu_misc_is_rsvd_name(ll_intf->partition.name(part_num)))
		return 0;

	return 1;
}

/**
 * rsu_misc_slot2part() - get partition number from the slot
 * @ll_intf: pointer to ll_intf
 * @slot: slot number
 *
 * Return 0 if success, or -1 for error
 */
int rsu_misc_slot2part(struct rsu_ll_intf *ll_intf, int slot)
{
	int partitions;
	int cnt = 0;

	partitions = ll_intf->partition.count();

	for (int x = 0; x < partitions; x++) {
		if (rsu_misc_is_slot(ll_intf, x)) {
			if (slot == cnt)
				return x;
			cnt++;
		}
	}

	return -1;
}

/**
 * rsu_misc_writeprotected() - check if a slot is protected
 * @slot: the number of slot to be checked
 *
 * Return 1 if a slot is protected, 0 for not
 */
int rsu_misc_writeprotected(int slot)
{
	char *protected;
	int protected_slot_numb;

	/* protect works only for slot 0-31 */
	if (slot > 31)
		return 0;

	protected = env_get("rsu_protected_slot");
	if (!strcmp(protected, "")) {
		/* user doesn't set protected slot */
		return 0;
	}

	protected_slot_numb = (int)simple_strtol(protected, NULL, 0);
	if (protected_slot_numb < 0 || protected_slot_numb > 31) {
		rsu_log(RSU_WARNING,
			"protected slot works only on the first 32 slots\n");
		return 0;
	}

	if (protected_slot_numb == slot)
		return 1;
	else
		return 0;
}

/**
 * rsu_misc_safe_strcpy() - buffer copy
 * @dst: pointer to dst
 * @dsz: dst buffer size
 * @src: pointer to src
 * @ssz: src buffer size
 */
void rsu_misc_safe_strcpy(char *dst, int dsz, char *src, int ssz)
{
	int len;

	if (!dst || dsz <= 0)
		return;

	if (!src || ssz <= 0) {
		dst[0] = '\0';
		return;
	}

	len = strnlen(src, ssz);
	if (len >= dsz)
		len = dsz - 1;

	memcpy(dst, src, len);
	dst[len] = '\0';
}

/**
 * rsu_cb_buf_init() - initialize buffer parameters
 * @buf: pointer to buf
 * @size: size of buffer
 *
 * Return 0 if success, or -1 for error
 */
int rsu_cb_buf_init(void *buf, int size)
{
	if (!buf || size <= 0)
		return -1;

	cb_buffer = (char *)buf;
	cb_buffer_togo = size;

	return 0;
}

/**
 * rsu_cb_buf_exit() - reset buffer parameters
 */
void rsu_cb_buf_exit(void)
{
	cb_buffer = NULL;
	cb_buffer_togo = -1;
}

/**
 * rsu_cb_buf() - copy data to buffer
 * @buf: pointer to data buffer
 * @len: size of data buffer
 *
 * Return the buffer data size
 */
int rsu_cb_buf(void *buf, int len)
{
	int read_len;

	if (!cb_buffer_togo)
		return 0;

	if (!cb_buffer || cb_buffer_togo < 0 || !buf || len < 0)
		return -1;

	if (cb_buffer_togo < len)
		read_len = cb_buffer_togo;
	else
		read_len = len;

	memcpy(buf, cb_buffer, read_len);

	cb_buffer += read_len;
	cb_buffer_togo -= read_len;

	if (!cb_buffer_togo)
		cb_buffer = NULL;

	return read_len;
}

/**
 * rsu_cb_program_common - callback to program flash
 * @ll_intf: pointer to ll_intf
 * @slot: slot number
 * @callback: callback function pointer
 * @rawdata: flag (raw data or not)
 *
 * Return 0 if success, or error code
 */
int rsu_cb_program_common(struct rsu_ll_intf *ll_intf, int slot,
			  rsu_data_callback callback, int rawdata)
{
	int part_num;
	int offset;
	unsigned char buf[IMAGE_BLOCK_SZ];
	unsigned char vbuf[IMAGE_BLOCK_SZ];
	int cnt, c, done;
	int x;
	struct rsu_slot_info info;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0)
		return -ESLOTNUM;

	if (rsu_misc_writeprotected(slot)) {
		rsu_log(RSU_ERR,
			"Trying to program a write protected slot\n");
		return -EWRPROT;
	}

	if (rsu_slot_get_info(slot, &info)) {
		rsu_log(RSU_ERR, "Unable to read slot info\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.get(part_num) > 0) {
		rsu_log(RSU_ERR,
			"Trying to program a slot already in use\n");
		return -EPROGRAM;
	}

	if (!callback)
		return -EARGS;

	offset = 0;
	done = 0;

	while (!done) {
		cnt = 0;
		while (cnt < IMAGE_BLOCK_SZ) {
			c = callback(buf + cnt, IMAGE_BLOCK_SZ - cnt);
			if (c == 0) {
				done = 1;
				break;
			} else if (c < 0) {
				return -ECALLBACK;
			}
			cnt += c;
		}

		if (cnt == 0)
			break;

		if (!rawdata && offset == IMAGE_PTR_BLOCK &&
		    cnt == IMAGE_BLOCK_SZ &&
		    rsu_misc_image_adjust(buf, &info))
			return -EPROGRAM;

		if ((offset + cnt) > ll_intf->partition.size(part_num)) {
			rsu_log(RSU_ERR,
				"Trying to program too much data into slot\n");
			return -ESIZE;
		}

		if (ll_intf->data.write(part_num, offset, cnt, buf))
			return -ELOWLEVEL;

		if (ll_intf->data.read(part_num, offset, cnt, vbuf))
			return -ELOWLEVEL;

		for (x = 0; x < cnt; x++)
			if (vbuf[x] != buf[x]) {
				rsu_log(RSU_DEBUG,
					"Expect %02X, got %02X @ 0x%08X\n",
					buf[x], vbuf[x], offset + x);
				return -ECMP;
			}

		offset += cnt;
	}

	if (!rawdata && ll_intf->priority.add(part_num))
		return -ELOWLEVEL;

	return 0;
}

/**
 * rsu_cb_verify_common() - callback for data verification
 * @ll_intf: pointer to ll_intf
 * @slot: slot number
 * @callback: callback function pointer
 * @rawdata: flag (raw data or not)
 *
 * Return 0 if success, or error code
 */
int rsu_cb_verify_common(struct rsu_ll_intf *ll_intf, int slot,
			 rsu_data_callback callback, int rawdata)
{
	int part_num;
	int offset;
	unsigned char buf[IMAGE_BLOCK_SZ];
	unsigned char vbuf[IMAGE_BLOCK_SZ];
	int cnt, c, done;
	int x;

	if (!ll_intf)
		return -EINTF;

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (!rawdata && ll_intf->priority.get(part_num) <= 0) {
		rsu_log(RSU_ERR, "Trying to verify a slot not in use\n");
		return -EERASE;
	}

	if (!callback)
		return -EARGS;

	offset = 0;
	done = 0;

	while (!done) {
		cnt = 0;
		while (cnt < IMAGE_BLOCK_SZ) {
			c = callback(buf + cnt, IMAGE_BLOCK_SZ - cnt);
			if (c == 0) {
				done = 1;
				break;
			} else if (c < 0) {
				return -ECALLBACK;
			}

			cnt += c;
		}

		if (cnt == 0)
			break;

		if (ll_intf->data.read(part_num, offset, cnt, vbuf))
			return -ELOWLEVEL;

		for (x = 0; x < cnt; x++) {
			if (!rawdata && (offset + x) >= IMAGE_PTR_START &&
			    (offset + x) <= IMAGE_PTR_END)
				continue;

			if (vbuf[x] != buf[x]) {
				rsu_log(RSU_ERR,
					"Expect %02X, got %02X @ 0x%08X",
					buf[x], vbuf[x], offset + x);
				return -ECMP;
			}
		}
		offset += cnt;
	}

	return 0;
}

/*
 * rsu_log() - display rsu log message
 * @level: log level
 * @format: log message format
 */
void rsu_log(const enum rsu_log_level level, const char *format, ...)
{
	va_list args;
	int log_level;
	char printbuffer[LOG_BUF_SIZE];

	log_level = (int)simple_strtol(env_get("rsu_log_level"), NULL, 0);

	if (level >= log_level)
		return;

	va_start(args, format);
	vscnprintf(printbuffer, sizeof(printbuffer), format, args);
	va_end(args);
	puts(printbuffer);
}
