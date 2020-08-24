/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <common.h>
#include <linux/errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_misc.h>
#include <spi.h>
#include <spi_flash.h>
#include <linux/sizes.h>
#include <u-boot/zlib.h>

#define SPT_MAGIC_NUMBER	0x57713427
#define SPT_FLAG_RESERVED	1
#define SPT_FLAG_READONLY	2

#define CPB_MAGIC_NUMBER	0x57789609
#define CPB_HEADER_SIZE		24

#define ERASED_ENTRY		((u64)-1)
#define SPENT_ENTRY		((u64)0)

#define SPT_VERSION		0
#define LIBRSU_VER		0

#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_STRATIX10)
#define DCMF_SIZE		0x040000
#define DCMF0_VERSION_OFFSET	0x000420
#define DCMF1_VERSION_OFFSET	0x040420
#define DCMF2_VERSION_OFFSET	0x080420
#define DCMF3_VERSION_OFFSET	0x0C0420
#define DCIO_MAX_RETRY_OFFSET   0x10018C
#else
#define DCMF_SIZE		0x080000
#define DCMF0_VERSION_OFFSET	0x000420
#define DCMF1_VERSION_OFFSET	0x080420
#define DCMF2_VERSION_OFFSET	0x100420
#define DCMF3_VERSION_OFFSET	0x180420
#define DCIO_MAX_RETRY_OFFSET   0x20018C
#endif

#define SPT_MAX_PARTITIONS 	127
#define MIN_QSPI_ERASE_SIZE	4096

#define CPB_SIZE		SZ_4K
#define CPB_IMAGE_PTR_OFFSET	32
#define CPB_IMAGE_PTR_NSLOTS	508

/**
 * struct sub_partition_table_partition - SPT partition structure
 * @name: sub-partition name
 * @offset: sub-partition start offset
 * @length: sub-partition length
 * @flags: sub-partition flags
 */
struct sub_partition_table_partition {
	char name[16];
	u64 offset;
	u32 length;
	u32 flags;
};

/**
 * struct sub_partition_table - sub partition table structure
 * @magic_number: the magic number
 * @version: version number
 * @partitions: number of entries
 * @revd: reserved
 * @sub_partition_table_partition.partition: SPT partition array
 */
struct sub_partition_table {
	u32 magic_number;
	u32 version;
	u32 partitions;
	u32 rsvd[5];
	struct sub_partition_table_partition partition[SPT_MAX_PARTITIONS];
};

/**
 * union cmf_pointer_block - CMF pointer block
 * @header.magic_number: CMF pointer block magic number
 * @header.header_size: size of CMF pointer block header
 * @header.cpb_size: size of CMF pointer block
 * @header.cpb_reserved: reserved
 * @header.image_ptr_offset: offset of image pointers
 * @header.image_ptr_slots: number of image pointer slots
 * @data: image pointer slot array
 */
union cmf_pointer_block {
	struct {
		u32 magic_number;
		u32 header_size;
		u32 cpb_size;
		u32 cpb_reserved;
		u32 image_ptr_offset;
		u32 image_ptr_slots;
	} header;
	char data[4 * 1024];
};

static union cmf_pointer_block cpb;
static struct sub_partition_table spt;
static u64 *cpb_slots;
struct spi_flash *flash;
static u32 spt0_offset;
static u32 spt1_offset;

static int cpb0_part = -1;
static int cpb1_part = -1;
static bool cpb_corrupted;

/**
 * get_part_offset() - get a selected partition offset
 * @part_num: the selected partition number
 * @offset: the partition offset
 *
 * Return: 0 on success, or -1 for error
 */
static int get_part_offset(int part_num, u64 *offset)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	*offset = spt.partition[part_num].offset;

	return 0;
}

/**
 * read_dev() - read data from flash
 * @offset: the offset which read from flash
 * @buf: buffer for read data
 * @len: the size of data which read from flash
 *
 * Return: 0 on success, or -ve for error
 */
static int read_dev(u64 offset, void *buf, int len)
{
	int ret;

	ret = spi_flash_read(flash, (u32)offset, len, buf);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	return 0;
}

/**
 * write_dev() - write data to flash
 * @offset: the offset which data will written to
 * @buf: the written data
 * @len: the size of data which write to flash
 *
 * Return: 0 on success, or -ve for error
 */
static int write_dev(u64 offset, void *buf, int len)
{
	int ret;

	ret = spi_flash_write(flash, (u32)offset, len, buf);
	if (ret) {
		rsu_log(RSU_ERR, "write flash error=%i\n", ret);
		return ret;
	}

	return 0;
}

/**
 * erase_dev() - erase data at flash
 * @offset: the offset from which data will be erased
 * @len: the size of data to be erased
 *
 * Return: 0 on success, or -ve for error
 */
static int erase_dev(u64 offset, int len)
{
	int ret;

	ret = spi_flash_erase(flash, (u32)offset, len);
	if (ret) {
		rsu_log(RSU_ERR, "erase flash error=%i\n", ret);
		return ret;
	}

	return 0;
}

/**
 * read_part() - read a selected partition data
 * @part_num: the selected partition number
 * @offset: the offset from which data will be read
 * @buf: buffer contains the read data
 * @len: the size of data to be read
 *
 * Return: 0 on success, or -ve for error
 */
static int read_part(int part_num, u64 offset, void *buf, int len)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	if (offset < 0 || len < 0 ||
	    (offset + len) > spt.partition[part_num].length)
		return -1;

	return read_dev(part_offset + offset, buf, len);
}

/**
 * write_part() - write a selected partition data
 * @part_num: the selected partition number
 * @offset: the offset to which data will be written
 * @buf: data to be written to
 * @len: the size of data to be written
 *
 * Return: 0 on success, or -ve for error
 */
static int write_part(int part_num, u64 offset, void *buf, int len)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	if (offset < 0 || len < 0 ||
	    (offset + len) > spt.partition[part_num].length)
		return -1;

	return write_dev(part_offset + offset, buf, len);
}

/**
 * erase_part() - erase a selected partition data
 * @part_num: the selected partition number
 *
 * Return: 0 on success, or -ve for error
 */
static int erase_part(int part_num)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;

	return erase_dev(part_offset, spt.partition[part_num].length);
}

/**
 * writeback_spt() - write back SPT
 *
 * Return: 0 on success, or -1 for error
 */
static int writeback_spt(void)
{
	int x;
	int updates = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "SPT0") &&
		    strcmp(spt.partition[x].name, "SPT1"))
			continue;

		if (erase_part(x)) {
			rsu_log(RSU_ERR, "failed to erase SPTx");
			return -1;
		}

		spt.magic_number = (u32)0xFFFFFFFF;
		if (write_part(x, 0, &spt, sizeof(spt))) {
			rsu_log(RSU_ERR, "failed to write SPTx table");
			return -1;
		}

		spt.magic_number = (u32)SPT_MAGIC_NUMBER;
		if (write_part(x, 0, &spt.magic_number,
			       sizeof(spt.magic_number))) {
			rsu_log(RSU_ERR, "failed to write SPTx magic #");
			return -1;
		}

		updates++;
	}

	if (updates != 2) {
		rsu_log(RSU_ERR, "didn't find two SPTs");
		return -1;
	}

	return 0;
}

/**
 * check_spt() - check if SPT is valid
 *
 * Return: 0 for valid SPT, or -1 for not
 */
static int check_spt(void)
{
	int x;
	int max_len = sizeof(spt.partition[0].name);
	int spt0_found = 0;
	int spt1_found = 0;
	int cpb0_found = 0;
	int cpb1_found = 0;

	/*
	 * Make sure the SPT names are '\0' terminated. Truncate last byte
	 * if the name uses all available bytes.  Perform validity check on
	 * entries.
	 */

	rsu_log(RSU_DEBUG,
		"MAX length of a name = %i bytes\n", max_len - 1);

	if (spt.version > SPT_VERSION) {
		rsu_log(RSU_WARNING, "SPT version %i is greater than %i\n",
			spt.version, SPT_VERSION);
		rsu_log(RSU_WARNING,
			"RSU Version %i - update to for newer features\n",
			LIBRSU_VER);
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strnlen(spt.partition[x].name, max_len) >= max_len)
			spt.partition[x].name[max_len - 1] = '\0';

		rsu_log(RSU_DEBUG, "RSU %-16s %016llX - %016llX (%X)\n",
			spt.partition[x].name, spt.partition[x].offset,
			(spt.partition[x].offset +
			spt.partition[x].length - 1),
			spt.partition[x].flags);

		if (strcmp(spt.partition[x].name, "SPT0") == 0)
			spt0_found = 1;
		else if (strcmp(spt.partition[x].name, "SPT1") == 0)
			spt1_found = 1;
		else if (strcmp(spt.partition[x].name, "CPB0") == 0)
			cpb0_found = 1;
		else if (strcmp(spt.partition[x].name, "CPB1") == 0)
			cpb1_found = 1;
	}

	if (!spt0_found || !spt1_found || !cpb0_found || !cpb1_found) {
		rsu_log(RSU_ERR, "Missing a critical entry in the SPT\n");
		return -1;
	}

	return 0;
}

/**
 * load_spt() - retrieve SPT from flash
 *
 * Return: 0 on success, or -1 for error
 */
static int load_spt(void)
{
	int spt0_good = 0;
	int spt1_good = 0;
	int spt_size = spt1_offset - spt0_offset;

	rsu_log(RSU_DEBUG, "reading SPT1\n");
	if (read_dev(spt1_offset, &spt, sizeof(spt)) == 0 &&
	    spt.magic_number == SPT_MAGIC_NUMBER) {
		if (check_spt() == 0)
			spt1_good = 1;
		else
			rsu_log(RSU_ERR, "SPT1 validity check failed\n");
	} else {
		rsu_log(RSU_ERR, "Bad SPT1 magic number 0x%08X\n",
			spt.magic_number);
	}

	rsu_log(RSU_DEBUG, "reading SPT0\n");
	if (read_dev(spt0_offset, &spt, sizeof(spt)) == 0 &&
	    spt.magic_number == SPT_MAGIC_NUMBER) {
		if (check_spt() == 0)
			spt0_good = 1;
		else
			rsu_log(RSU_ERR, "SPT0 validity check failed\n");
	} else {
		rsu_log(RSU_ERR, "Bad SPT0 magic number 0x%08X\n",
			spt.magic_number);
	}

	if (spt0_good && spt1_good) {
		rsu_log(RSU_INFO, "SPTs are GOOD!!!\n");
		return 0;
	}

	if (spt0_good) {
		rsu_log(RSU_WARNING, "warning: Restoring SPT1\n");
		if (erase_dev(spt1_offset, spt_size)) {
			rsu_log(RSU_ERR, "Erase SPT1 region failed\n");
			return -1;
		}

		spt.magic_number = (u32)0xFFFFFFFF;
		if (write_dev(spt1_offset, &spt, sizeof(spt))) {
			rsu_log(RSU_ERR, "Unable to write SPT1 table\n");
			return -1;
		}

		spt.magic_number = (u32)SPT_MAGIC_NUMBER;
		if (write_dev(spt1_offset, &spt.magic_number,
			      sizeof(spt.magic_number))) {
			rsu_log(RSU_ERR, "Unable to wr SPT1 magic #\n");
			return -1;
		}

		return 0;
	}

	if (spt1_good) {
		if (read_dev(spt1_offset, &spt, sizeof(spt)) ||
		    spt.magic_number != SPT_MAGIC_NUMBER || check_spt()) {
			rsu_log(RSU_ERR, "Failed to load SPT1\n");
			return -1;
		}

		rsu_log(RSU_WARNING, "Restoring SPT0");

		if (erase_dev(spt0_offset, spt_size)) {
			rsu_log(RSU_ERR, "Erase SPT0 region failed\n");
			return -1;
		}

		spt.magic_number = (u32)0xFFFFFFFF;
		if (write_dev(spt0_offset, &spt, sizeof(spt))) {
			rsu_log(RSU_ERR, "Unable to write SPT0 table\n");
			return -1;
		}

		spt.magic_number = (u32)SPT_MAGIC_NUMBER;
		if (write_dev(spt0_offset, &spt.magic_number,
			      sizeof(spt.magic_number))) {
			rsu_log(RSU_ERR, "Unable to wr SPT0 magic #\n");
			return -1;
		}

		return 0;
	}

	rsu_log(RSU_ERR, "no valid SPT0 and SPT1 found\n");
	return -1;
}

/**
 * check_cpb() - check if CPB is valid
 *
 * Return: 0 for the valid CPB, or -ve on error
 */
static int check_cpb(void)
{
	int x, y;

	if (cpb.header.header_size > CPB_HEADER_SIZE) {
		rsu_log(RSU_WARNING,
			"CPB header is larger than expected\n");
		return -1;
	}

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] == ERASED_ENTRY ||
		    cpb_slots[x] == SPENT_ENTRY)
			continue;

		for (y = 0; y < spt.partitions; y++) {
			if (cpb_slots[x] == spt.partition[y].offset) {
				rsu_log(RSU_DEBUG, "cpb_slots[%i] = %s\n",
					x, spt.partition[y].name);
				break;
			}
		}

		if (y >= spt.partitions) {
			rsu_log(RSU_ERR, "CPB is not included in SPT\n");
			rsu_log(RSU_DEBUG, "cpb_slots[%i] = %016llX ???",
				x, cpb_slots[x]);
			return -EINVAL;
		}

		if (spt.partition[y].flags & SPT_FLAG_RESERVED) {
			rsu_log(RSU_ERR, "CPB is included in SPT ");
			rsu_log(RSU_ERR, "but it is reserved\n");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * check_both_cpb() - check if both CPBs are same
 *
 * Return: 0 for the identical CPBs, or -ve on error
 */
static int check_both_cpb(void)
{
	int ret;
	char *cpb0_data;
	char *cpb1_data;

	cpb0_data = (char *)malloc(CPB_SIZE);
	if (!cpb0_data) {
		rsu_log(RSU_ERR, "failed to allocate cpb0_data\n");
		return -ENOMEM;
	}

	cpb1_data = (char *)malloc(CPB_SIZE);
	if (!cpb1_data) {
		rsu_log(RSU_ERR, "failed to allocate cpb1_data\n");
		free(cpb0_data);
		return -ENOMEM;
	}

	ret = read_part(cpb0_part, 0, cpb0_data, CPB_SIZE);
	if (ret) {
		rsu_log(RSU_ERR, "failed to read cpb0_data\n");
		goto ops_error;
	}

	ret = read_part(cpb1_part, 0, cpb1_data, CPB_SIZE);
	if (ret) {
		rsu_log(RSU_ERR, "failed to read cpb1_data\n");
		goto ops_error;
	}

	ret = memcmp(cpb0_data, cpb1_data, CPB_SIZE);

ops_error:
	free(cpb1_data);
	free(cpb0_data);
	return ret;
}

/**
 * save_cpb_to_address() - save cpb to the address
 * @address: the address which cpb is saved to
 *
 * Return: 0 for successful operation, or -ve on error
 */
static int save_cpb_to_address(u64 address)
{
	int ret;
	char *cpb_data_dst = (char *)address;
	char *cpb_data_src;
	u32 calc_crc;

	if (!cpb_data_dst) {
		rsu_log(RSU_ERR, "failed due to invalid address");
		return -EINVAL;
	}

	cpb_data_src = (char *)malloc(CPB_SIZE);
	if (!cpb_data_src) {
		rsu_log(RSU_ERR, "failed to allocate cpb_data_src\n");
		return -ENOMEM;
	}

	ret = read_part(cpb0_part, 0, cpb_data_src, CPB_SIZE);
	if (ret) {
		rsu_log(RSU_ERR, "failed to read CPB data\n");
		free(cpb_data_src);
		return ret;
	}

	calc_crc = crc32(0, (void *)cpb_data_src, CPB_SIZE);
	rsu_log(RSU_DEBUG, "%s - calc_crc is 0x%x\n", __func__, calc_crc);
	memcpy(cpb_data_dst, cpb_data_src, CPB_SIZE);
	memcpy(cpb_data_dst + CPB_SIZE, &calc_crc, sizeof(calc_crc));

	free(cpb_data_src);
	return ret;
}

/**
 * corrupted_cpb() - check if cpb is corrupted
 *
 * Return: 1 for the corrupted cpb , or 0 for not
 */
static int corrupted_cpb(void)
{
	return cpb_corrupted;
}

/**
 * load_cpb() - retrieve CPB from flash
 *
 * Return: 0 on success, or -1 for error
 */
static int load_cpb(void)
{
	int x;
	int cpb0_good = 0;
	int cpb1_good = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") == 0)
			cpb0_part = x;
		else if (strcmp(spt.partition[x].name, "CPB1") == 0)
			cpb1_part = x;

		if (cpb0_part >= 0 && cpb1_part >= 0)
			break;
	}

	if (cpb0_part < 0 || cpb1_part < 0) {
		rsu_log(RSU_ERR, "Missing CPB0/1 partition\n");
		return -1;
	}

	rsu_log(RSU_DEBUG, "Reading CPB1\n");
	if (read_part(cpb1_part, 0, &cpb, sizeof(cpb)) == 0 &&
	    cpb.header.magic_number == CPB_MAGIC_NUMBER) {
		cpb_slots = (u64 *)
			     &cpb.data[cpb.header.image_ptr_offset];
		if (check_cpb() == 0)
			cpb1_good = 1;
	} else {
		rsu_log(RSU_ERR, "Bad CPB1 is bad\n");
	}

	rsu_log(RSU_DEBUG, "Reading CPB0\n");
	if (read_part(cpb0_part, 0, &cpb, sizeof(cpb)) == 0 &&
	    cpb.header.magic_number == CPB_MAGIC_NUMBER) {
		cpb_slots = (u64 *)
			     &cpb.data[cpb.header.image_ptr_offset];
		if (check_cpb() == 0)
			cpb0_good = 1;
	} else {
		rsu_log(RSU_ERR, "Bad CPB0 is bad\n");
	}

	if (cpb0_good && cpb1_good) {
		if (check_both_cpb()) {
			rsu_log(RSU_ERR, "unmatched CPB0/1 data");
			cpb_corrupted = true;
			return -EINVAL;
		}
		rsu_log(RSU_INFO, "CPBs are GOOD!!!\n");
		cpb_slots = (u64 *)
			     &cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	if (cpb0_good) {
		rsu_log(RSU_WARNING, "Restoring CPB1\n");
		if (erase_part(cpb1_part)) {
			rsu_log(RSU_ERR, "Failed erase CPB1\n");
			return -1;
		}

		cpb.header.magic_number = (u32)0xFFFFFFFF;
		if (write_part(cpb1_part, 0, &cpb, sizeof(cpb))) {
			rsu_log(RSU_ERR, "Unable to write CPB1 table\n");
			return -1;
		}

		cpb.header.magic_number = (u32)CPB_MAGIC_NUMBER;
		if (write_part(cpb1_part, 0, &cpb.header.magic_number,
			       sizeof(cpb.header.magic_number))) {
			rsu_log(RSU_ERR, "Unable to write CPB1 magic number\n");
			return -1;
		}

		cpb_slots = (u64 *)&cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	if (cpb1_good) {
		if (read_part(cpb1_part, 0, &cpb, sizeof(cpb)) ||
		    cpb.header.magic_number != CPB_MAGIC_NUMBER) {
			rsu_log(RSU_ERR, "Unable to load CPB1\n");
			return -1;
		}

		rsu_log(RSU_WARNING, "Restoring CPB0\n");
		if (erase_part(cpb0_part)) {
			rsu_log(RSU_ERR, "Failed erase CPB0\n");
			return -1;
		}

		cpb.header.magic_number = (u32)0xFFFFFFFF;
		if (write_part(cpb0_part, 0, &cpb, sizeof(cpb))) {
			rsu_log(RSU_ERR, "Unable to write CPB0 table\n");
			return -1;
		}

		cpb.header.magic_number = (u32)CPB_MAGIC_NUMBER;
		if (write_part(cpb0_part, 0, &cpb.header.magic_number,
			       sizeof(cpb.header.magic_number))) {
			rsu_log(RSU_ERR, "Unable to write CPB0 magic number\n");
			return -1;
		}

		cpb_slots = (u64 *)&cpb.data[cpb.header.image_ptr_offset];
		return 0;
	}

	cpb_corrupted = true;
	rsu_log(RSU_ERR, "No valid CPB0 or CPB1 found\n");
	return -1;
}

/**
 * update_cpb() - update CPB at flash
 *
 * Return: 0 on success, or -1 for error
 */
static int update_cpb(int slot, u64 ptr)
{
	int x;
	int updates = 0;

	if (slot < 0 || slot > cpb.header.image_ptr_slots)
		return -1;

	if ((cpb_slots[slot] & ptr) != ptr)
		return -1;

	cpb_slots[slot] = ptr;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") &&
		    strcmp(spt.partition[x].name, "CPB1"))
			continue;

		if (write_part(x, 0, &cpb, sizeof(cpb)))
			return -1;

		updates++;
	}

	if (updates != 2) {
		rsu_log(RSU_ERR, "Did not find two CPBs\n");
		return -1;
	}

	return 0;
}

/**
 * writeback_cpb() - write CPB back to flash
 *
 * Return: 0 on success, or -1 for error
 */
static int writeback_cpb(void)
{
	int x;
	int updates = 0;

	for (x = 0; x < spt.partitions; x++) {
		if (strcmp(spt.partition[x].name, "CPB0") &&
		    strcmp(spt.partition[x].name, "CPB1"))
			continue;

		if (erase_part(x)) {
			rsu_log(RSU_ERR, "Unable to ease CPBx\n");
			return -1;
		}

		cpb.header.magic_number = (u32)0xFFFFFFFF;
		if (write_part(x, 0, &cpb, sizeof(cpb))) {
			rsu_log(RSU_ERR, "Unable to write CPBx table\n");
			return -1;
		}

		cpb.header.magic_number = (u32)CPB_MAGIC_NUMBER;
		if (write_part(x, 0, &cpb.header.magic_number,
			       sizeof(cpb.header.magic_number))) {
			rsu_log(RSU_ERR,
				"Unable to write CPBx magic number\n");
			return -1;
		}

		updates++;
	}

	if (updates != 2) {
		rsu_log(RSU_ERR, "Did not find two CPBs\n");
		return -1;
	}

	return 0;
}

/**
 * empty_cpb() - create a cpb with header field only
 *
 * Return: 0 for successful operation, or -ve on error
 */
static int empty_cpb(void)
{
	int ret;
	struct cpb_header {
		u32 magic_number;
		u32 header_size;
		u32 cpb_size;
		u32 cpb_reserved;
		u32 image_ptr_offset;
		u32 image_ptr_slots;
	} *c_header;

	if (spt_corrupted) {
		rsu_log(RSU_ERR, "corrupted SPT ---");
		rsu_log(RSU_ERR, "run rsu restore_spt <address> first\n");
		return -EINVAL;
	}

	c_header = (struct cpb_header *)malloc(sizeof(struct cpb_header));
	if (!c_header) {
		rsu_log(RSU_ERR, "failed to allocate cpb_header\n");
		return -ENOMEM;
	}

	c_header->magic_number = CPB_MAGIC_NUMBER;
	c_header->header_size = CPB_HEADER_SIZE;
	c_header->cpb_size = CPB_SIZE;
	c_header->cpb_reserved = 0;
	c_header->image_ptr_offset = CPB_IMAGE_PTR_OFFSET;
	c_header->image_ptr_slots = CPB_IMAGE_PTR_NSLOTS;

	memset(&cpb, -1, CPB_SIZE);
	memcpy(&cpb, c_header, sizeof(*c_header));

	ret = writeback_cpb();
	if (ret) {
		rsu_log(RSU_ERR, "failed to write back cpb\n");
		goto ops_error;
	}

	cpb_slots = (u64 *)&cpb.data[cpb.header.image_ptr_offset];
	cpb_corrupted = false;

ops_error:
	free(c_header);
	return ret;
}

/**
 * restore_cpb_from_address() - restore the cpb from an address
 * @address: the address which cpb is restored from
 *
 * Return: 0 for successful operation, or -ve on error
 */
static int restore_cpb_from_address(u64 address)
{
	int ret;
	u32 calc_crc;
	u32 crc_from_saved;
	u32 magic_number;
	char *cpb_data = (char *)address;

	if (spt_corrupted) {
		rsu_log(RSU_ERR, "corrupted SPT --");
		rsu_log(RSU_ERR, "run rsu restore_spt <address> first\n");
		return -EINVAL;
	}

	if (!cpb_data) {
		rsu_log(RSU_ERR, "failed due to invalid address\n");
		return -EINVAL;
	}

	calc_crc = crc32(0, (void *)cpb_data, CPB_SIZE);
	rsu_log(RSU_DEBUG, "%s - calc_crc is 0x%x\n", __func__, calc_crc);
	memcpy(&crc_from_saved, cpb_data + CPB_SIZE, sizeof(crc_from_saved));
	rsu_log(RSU_DEBUG, "%s - crc_from_saved is 0x%x\n", __func__,
		crc_from_saved);

	if (calc_crc != crc_from_saved) {
		rsu_log(RSU_ERR, "saved data is corrupted\n");
		return -EINVAL;
	}

	/*
	 * check the magic number to prevent user from accidentally
	 * restoring SPB
	 */
	memcpy(&magic_number, cpb_data, sizeof(magic_number));
	if (magic_number != CPB_MAGIC_NUMBER) {
		rsu_log(RSU_ERR, "failure due to mismatch magic number\n");
		return -EINVAL;
	}

	memcpy(&cpb, cpb_data, CPB_SIZE);
	ret = writeback_cpb();
	if (ret) {
		rsu_log(RSU_ERR, "failed to write back cpb\n");
		return ret;
	}

	cpb_slots = (u64 *)&cpb.data[cpb.header.image_ptr_offset];

	cpb_corrupted = false;
	return 0;
}

/**
 * partition_count() - get the partition count
 *
 * Return: the number of partition at flash
 */
static int partition_count(void)
{
	return spt.partitions;
}

/**
 * partition_name() - get a selected partition name
 * @part_num: the selected partition number
 *
 * Return: partition name on success, or "BAD" on error
 */
static char *partition_name(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return "BAD";

	return spt.partition[part_num].name;
}

/**
 * partition_offset() - get a selected partition offset
 * @part_num: the selected partition number
 *
 * Return: offset on success, or -1 on error
 */
static u64 partition_offset(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	return spt.partition[part_num].offset;
}

/**
 * partition_size() - get a selected partition size
 * @part_num: the selected partition number
 *
 * Return: the partition size for success, or -1 for error
 */
static u32 partition_size(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	return spt.partition[part_num].length;
}

/**
 * partition_reserved() - check if a selected partition is reserved
 * @part_num: the selected partition number
 *
 * Return: 1 for reserved partition, or 0 for not
 */
static int partition_reserved(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return 0;

	return (spt.partition[part_num].flags & SPT_FLAG_RESERVED) ? 1 : 0;
}

/**
 * partition_readonly() - check if a selected partition is read only
 * @part_num: the selected partition number
 *
 * Return: 1 for read only partition, or 0 for not
 */
static int partition_readonly(int part_num)
{
	if (part_num < 0 || part_num >= spt.partitions)
		return 0;

	return (spt.partition[part_num].flags & SPT_FLAG_READONLY) ? 1 : 0;
}

/**
 * partition_rename() - rename the selected partition name
 * @part_num: the selected partition
 * @name: the new name
 *
 * Return: 0 for success, or -1 on error
 */
static int partition_rename(int part_num, char *name)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	if (strnlen(name, sizeof(spt.partition[0].name)) >=
	    sizeof(spt.partition[0].name)) {
		rsu_log(RSU_ERR,
			"Partition name is too long - limited to %li",
			sizeof(spt.partition[0].name) - 1);
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strncmp(spt.partition[x].name, name,
			    sizeof(spt.partition[0].name) - 1) == 0) {
			rsu_log(RSU_ERR,
				"Partition rename already in use\n");
			return -1;
		}
	}

	rsu_misc_safe_strcpy(spt.partition[part_num].name,
			     sizeof(spt.partition[0].name),
			     name, sizeof(spt.partition[0].name));

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

/**
 * partition_delete() - delete a partition
 * @part_num: the selected partition
 *
 * Return: 0 for success, or -1 on error
 */
static int partition_delete(int part_num)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions) {
		rsu_log(RSU_ERR, "Invalid partition number\n");
		return -1;
	}

	for (x = part_num; x < spt.partitions; x++)
		spt.partition[x] = spt.partition[x + 1];

	spt.partitions--;

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

/**
 * partition_create() - create a partition
 * @name: partition name
 * @start: partition start address
 * @size: partition size
 *
 * Return: 0 for success, or -1 on error
 */
static int partition_create(char *name, u64 start, unsigned int size)
{
	int x;
	u64 end = start + size;

	if (size % MIN_QSPI_ERASE_SIZE) {
		rsu_log(RSU_ERR, "Invalid partition size\n");
		return -1;
	}

	if (start % MIN_QSPI_ERASE_SIZE) {
		rsu_log(RSU_ERR, "Invalid partition address\n");
		return -1;
	}

	if (strnlen(name, sizeof(spt.partition[0].name)) >=
	    sizeof(spt.partition[0].name)) {
		rsu_log(RSU_ERR, "Partition name is too long - limited to %i\n",
			sizeof(spt.partition[0].name) - 1);
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		if (strncmp(spt.partition[x].name, name,
			    sizeof(spt.partition[0].name) - 1) == 0) {
			rsu_log(RSU_ERR, "Partition name already in use\n");
			return -1;
		}
	}

	if (spt.partitions == SPT_MAX_PARTITIONS) {
		rsu_log(RSU_ERR, "Partition table is full\n");
		return -1;
	}

	for (x = 0; x < spt.partitions; x++) {
		u64 pstart = spt.partition[x].offset;
		u64 pend = spt.partition[x].offset +
			     spt.partition[x].length;

		if (start < pend && end > pstart) {
			rsu_log(RSU_ERR, "Partition overlap\n");
			return -1;
		}
	}

	rsu_misc_safe_strcpy(spt.partition[spt.partitions].name,
			     sizeof(spt.partition[0].name), name,
			     sizeof(spt.partition[0].name));
	spt.partition[spt.partitions].offset = start;
	spt.partition[spt.partitions].length = size;
	spt.partition[spt.partitions].flags = 0;

	spt.partitions++;

	if (writeback_spt())
		return -1;

	if (load_spt())
		return -1;

	return 0;
}

/**
 * priority_get() - get the selected partition's priority
 * @part_num: the selected partition number
 *
 * Return: 0 for success, or -1 on error
 */
static int priority_get(int part_num)
{
	int x;
	int priority = 0;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = cpb.header.image_ptr_slots; x > 0; x--) {
		if (cpb_slots[x - 1] != ERASED_ENTRY &&
		    cpb_slots[x - 1] != SPENT_ENTRY) {
			priority++;
			if (cpb_slots[x - 1] ==
			    spt.partition[part_num].offset)
				return priority;
		}
	}

	return 0;
}

/**
 * priority_add() - enable the selected partition's priority
 * @part_num: the selected partition number
 *
 * Return: 0 for success, or  -1 on error
 */
static int priority_add(int part_num)
{
	int x;
	int y;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] == ERASED_ENTRY) {
			if (update_cpb(x,
				       spt.partition[part_num].offset)) {
				load_cpb();
				return -1;
			}
			return load_cpb();
		}
	}

	rsu_log(RSU_DEBUG, "Compressing CPB\n");

	for (x = 0, y = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] != ERASED_ENTRY &&
		    cpb_slots[x] != SPENT_ENTRY) {
			cpb_slots[y++] = cpb_slots[x];
		}
	}

	if (y < cpb.header.image_ptr_slots)
		cpb_slots[y++] = spt.partition[part_num].offset;
	else
		return -1;

	while (y < cpb.header.image_ptr_slots)
		cpb_slots[y++] = ERASED_ENTRY;

	if (writeback_cpb() || load_cpb())
		return -1;

	return 0;
}

/**
 * priority_remove() - remove the selected partition's priority
 * @part_num: the selected partition number
 *
 * Return: 0 for success, or -1 on error
 */
static int priority_remove(int part_num)
{
	int x;

	if (part_num < 0 || part_num >= spt.partitions)
		return -1;

	for (x = 0; x < cpb.header.image_ptr_slots; x++) {
		if (cpb_slots[x] == spt.partition[part_num].offset)
			if (update_cpb(x, SPENT_ENTRY)) {
				load_cpb();
				return -1;
			}
	}

	return load_cpb();
}

/**
 * data_read() - read data from flash
 * @part_num: partition number
 * @offset: offset which data will be read from
 * @bytes: data size in byte which will be read
 * @buf: pointer to buffer contains to be read data
 *
 * Return: 0 for success, or error code
 */
static int data_read(int part_num, int offset, int bytes, void *buf)
{
	return read_part(part_num, offset, buf, bytes);
}

/**
 * data_write() - write data to flash
 * @part_num: partition number
 * @part_num: offset which data will be written to
 * @bytes: data size in bytes which will be written
 * @buf: pointer to buffer contains to be written data
 *
 * Return: 0 for success, or error code
 */
static int data_write(int part_num, int offset, int bytes, void *buf)
{
	return write_part(part_num, offset, buf, bytes);
}

/**
 * data_erase() - erase flash data
 * @part_num: partition number
 *
 * Return: 0 for success, or error code
 */
static int data_erase(int part_num)
{
	return erase_part(part_num);
}

/**
 * image_load() - load production or factory image
 * @offset: the image offset
 *
 * Return: 0 for success, or error code
 */
static int image_load(u64 offset)
{
	u32 flash_offset[2];

	flash_offset[0] = lower_32_bits(offset);
	flash_offset[1] = upper_32_bits(offset);

	rsu_log(RSU_DEBUG, "RSU_DEBUG: RSU updated to 0x%08x%08x\n",
		flash_offset[1], flash_offset[0]);

	if (mbox_rsu_update(flash_offset))
		return -ELOWLEVEL;

	return 0;
}

/**
 * status_log() - get firmware status info
 * @info: pointer to rsu_status_info
 *
 * Return: 0 for success, or error code
 */
static int status_log(struct rsu_status_info *info)
{
	if (mbox_rsu_status((u32 *)info,
			    sizeof(struct rsu_status_info) / 4)) {
		rsu_log(RSU_ERR,
			"RSU: Firmware or flash content not supporting RSU\n");
		return -ENOTSUPP;
	}

	return 0;
}

/**
 * notify_fw() - call the firmware notify command
 * @value: the notification value
 *
 * Return: 0 for success, or error code
 */
static int notify_fw(u32 value)
{
	rsu_log(RSU_DEBUG, "RSU_DEBUG: notified with 0x%08x.\n", value);

	if (mbox_hps_stage_notify(value))
		return -ELOWLEVEL;

	return 0;
}

/**
 * dcmf_version() - retrieve the decision firmware version
 * @versions: pointer to where the four DCMF versions will be stored
 *
 * This function is used to retrieve the version of each of the four DCMF copies
 * in flash.
 *
 * Returns: 0 on success, or error code
 */
static int dcmf_version(__u32 *versions)
{
	int ret;

	if (!versions)
		return -1;

	ret = spi_flash_read(flash, DCMF0_VERSION_OFFSET, 4, &versions[0]);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	ret = spi_flash_read(flash, DCMF1_VERSION_OFFSET, 4, &versions[1]);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	ret = spi_flash_read(flash, DCMF2_VERSION_OFFSET, 4, &versions[2]);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	ret = spi_flash_read(flash, DCMF3_VERSION_OFFSET, 4, &versions[3]);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	return 0;
}

/**
 * dcmf_status() - determine if decision dcmfs are corrupted
 * @status: pointer to where the status will be stored
 *
 * This function is used to determine whether decision firmware copies are
 * corrupted in flash, with the currently used decision firmware being used as
 * reference. The status is an array of 4 values, one for each decision
 * firmware copy. A 0 means the copy is fine, anything else means the copy is
 * corrupted.
 *
 * Returns: 0 on success, or error code
 */
static int dcmf_status(u16 *status)
{
	int ret;
	struct rsu_status_info rsu_status;
	char *buffa = NULL;
	char *buffb = NULL;
	int crt_dcmf;
	int idx;

	ret = status_log(&rsu_status);
	if (ret) {
		rsu_log(RSU_ERR, "status_log error");
		return -1;
	}
	crt_dcmf = RSU_VERSION_CRT_DCMF_IDX(rsu_status.version);

	buffa = (char *)malloc(DCMF_SIZE);
	if (!buffa) {
		rsu_log(RSU_ERR, "malloc error");
		return -1;
	}

	buffb = (char *)malloc(DCMF_SIZE);
	if (!buffb) {
		rsu_log(RSU_ERR, "malloc error");
		ret = -1;
		goto ret_val;
	}

	ret = spi_flash_read(flash, crt_dcmf * DCMF_SIZE, DCMF_SIZE, buffa);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		goto ret_val;
	}

	for (idx = 0; idx < 4; idx++) {
		int i;

		status[idx] = 0;

		if (idx == crt_dcmf)
			continue;

		ret = spi_flash_read(flash, idx * DCMF_SIZE, DCMF_SIZE, buffb);
		if (ret) {
			rsu_log(RSU_ERR, "read flash error=%i\n", ret);
			goto ret_val;
		}

		for (i = 0; i < DCMF_SIZE; i++)
			if (buffa[i] != buffb[i]) {
				status[idx] = 1;
				break;
			}
	}

ret_val:
	if (buffa)
		free(buffa);
	if (buffb)
		free(buffb);
	return ret;
}

/**
 * max_retry() - retrieve the max_retry parameter
 * @value: pointer to where the max_retry will be stored
 *
 * This function is used to retrieve the max_retry parameter from the decision
 * firmware data section.
 *
 * Returns: 0 on success, or error code
 */
static int max_retry(__u8 *value)
{
	int ret;
	__u8 tmp;

	if (!value)
		return -1;

	ret = spi_flash_read(flash, DCIO_MAX_RETRY_OFFSET, 1, &tmp);
	if (ret) {
		rsu_log(RSU_ERR, "read flash error=%i\n", ret);
		return ret;
	}

	/* Add one, to make value consistent with Quartus view */
	*value = tmp + 1;

	return ret;
}

static void ll_exit(void)
{
	cpb0_part = -1;
	cpb1_part = -1;
	cpb_corrupted = false;

	if (flash) {
		spi_flash_free(flash);
		flash = NULL;
	}
}

static struct rsu_ll_intf qspi_ll_intf = {
	.exit = ll_exit,

	.partition.count = partition_count,
	.partition.name = partition_name,
	.partition.offset = partition_offset,
	.partition.size = partition_size,
	.partition.reserved = partition_reserved,
	.partition.readonly = partition_readonly,
	.partition.rename = partition_rename,
	.partition.delete = partition_delete,
	.partition.create = partition_create,

	.priority.get = priority_get,
	.priority.add = priority_add,
	.priority.remove = priority_remove,

	.data.read = data_read,
	.data.write = data_write,
	.data.erase = data_erase,

	.fw_ops.load = image_load,
	.fw_ops.status = status_log,
	.fw_ops.notify = notify_fw,
	.fw_ops.dcmf_version = dcmf_version,
	.fw_ops.dcmf_status = dcmf_status,
	.fw_ops.max_retry = max_retry,

	.cpb_ops.empty = empty_cpb,
	.cpb_ops.restore = restore_cpb_from_address,
	.cpb_ops.save = save_cpb_to_address,
	.cpb_ops.corrupted = corrupted_cpb
};

int rsu_ll_qspi_init(struct rsu_ll_intf **intf)
{
	u32 spt_offset[4];

	/* retrieve data from flash */
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		rsu_log(RSU_ERR, "SPI probe failed.\n");
		return -ENODEV;
	}

	/* get the offset from firmware */
	if (mbox_rsu_get_spt_offset(spt_offset, 4)) {
		rsu_log(RSU_ERR, "Error from mbox_rsu_get_spt_offset\n");
		return -ECOMM;
	}

	spt0_offset = spt_offset[1];
	spt1_offset = spt_offset[3];
	rsu_log(RSU_DEBUG, "SPT0 offset 0x%08x\n", spt0_offset);
	rsu_log(RSU_DEBUG, "SPT1 offset 0x%08x\n", spt1_offset);

	if (load_spt()) {
		rsu_log(RSU_ERR, "Bad SPT\n");
		return -1;
	}

	if (spt_corrupted) {
		cpb_corrupted = true;
	} else if (load_cpb() && !cpb_corrupted) {
		rsu_log(RSU_ERR, "Bad CPB\n");
		return -1;
	}

	*intf = &qspi_ll_intf;

	return 0;
}
