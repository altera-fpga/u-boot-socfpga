// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Intel Corporation <www.intel.com>
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 */
#include <errno.h>
#include <inttypes.h>
#include <asm/arch/handoff_soc64.h>
#include <asm/io.h>
#include <linux/printk.h>

#ifndef __ASSEMBLY__
#include <asm/types.h>
enum endianness {
	LITTLE_ENDIAN = 0,
	BIG_ENDIAN,
	UNKNOWN_ENDIANNESS
};
#endif

static enum endianness check_endianness(u32 handoff)
{
	switch (handoff) {
	case SOC64_HANDOFF_MAGIC_BOOT:
	case SOC64_HANDOFF_MAGIC_MUX:
	case SOC64_HANDOFF_MAGIC_IOCTL:
	case SOC64_HANDOFF_MAGIC_FPGA:
	case SOC64_HANDOFF_MAGIC_DELAY:
	case SOC64_HANDOFF_MAGIC_SDRAM:
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_AGILEX5)
	case SOC64_HANDOFF_MAGIC_PERI:
#else
	case SOC64_HANDOFF_MAGIC_MISC:
#endif
		return BIG_ENDIAN;
	case SOC64_HANDOFF_MAGIC_CLOCK:
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_N5X)
		return LITTLE_ENDIAN;
#else
		return BIG_ENDIAN;
#endif
#if IS_ENABLED(CONFIG_ARCH_SOCFPGA_N5X)
	case SOC64_HANDOFF_DDR_UMCTL2_MAGIC:
		return LITTLE_ENDIAN;
	case SOC64_HANDOFF_DDR_PHY_MAGIC:
		return LITTLE_ENDIAN;
	case SOC64_HANDOFF_DDR_PHY_INIT_ENGINE_MAGIC:
		return LITTLE_ENDIAN;
#endif
	default:
		return UNKNOWN_ENDIANNESS;
	}
}

static int getting_endianness(void *handoff_address, enum endianness *endian_t)
{
	/* Checking handoff data is little endian ? */
	*endian_t = check_endianness(readl(handoff_address));

	if (*endian_t == UNKNOWN_ENDIANNESS) {
		/* Trying to check handoff data is big endian? */
		*endian_t = check_endianness(swab32(readl(handoff_address)));
		if (*endian_t == UNKNOWN_ENDIANNESS) {
			pr_info("%s : line %d => Cannot find endianness ", __FILE__,
				__LINE__);
			pr_info("at addr 0x%p\n", (u32 *)handoff_address);
			return -EPERM;
		}
	}

	return 0;
}

int socfpga_get_handoff_size(void *handoff_address)
{
	u32 size;
	int ret;
	enum endianness endian_t;

	ret = getting_endianness(handoff_address, &endian_t);
	if (ret)
		return ret;

	size = readl(handoff_address + SOC64_HANDOFF_OFFSET_LENGTH);
	if (endian_t == BIG_ENDIAN)
		size = swab32(size);

	size = (size - SOC64_HANDOFF_OFFSET_DATA) / sizeof(u32);

	pr_info("%s : line %d => handoff address = 0x%p handoff size = 0x%08x\n", __FILE__,
		__LINE__, (u32 *)handoff_address, size);

	return size;
}

int socfpga_handoff_read(void *handoff_address, void *table, u32 table_len)
{
	u32 temp;
	u32 *table_x32 = table;
	u32 i = 0;
	int ret;
	enum endianness endian_t;

	ret = getting_endianness(handoff_address, &endian_t);
	if (ret)
		return ret;

	pr_info("%s : line %d => Handoff table address = 0x%p, ", __FILE__,
		__LINE__, table_x32);
	pr_info("Table length = 0x%x\n\n", table_len);

	temp = readl(handoff_address);
	pr_info("%s : line %d => Handoff section : [%c%c%c%c] at 0x%08lx\n\n",
		__FILE__, __LINE__, (temp >> 0) & 0xff, (temp >> 8) & 0xff,
		(temp >> 16) & 0xff, (temp >> 24) & 0xff,
		(uintptr_t)handoff_address);

	pr_info("%s: Handoff data =\n{\n", __func__);

	temp = readl(handoff_address + SOC64_HANDOFF_OFFSET_DATA +
		    (i * sizeof(u32)));

	if (endian_t == BIG_ENDIAN)
		*table_x32 = swab32(temp);
	else if (endian_t == LITTLE_ENDIAN)
		*table_x32 = temp;

	pr_info(" 0x%08x -> 0x%08x ", i, *table_x32);

	for (i = 1; i < table_len; i++) {
		table_x32++;

		temp = readl(handoff_address +
			     SOC64_HANDOFF_OFFSET_DATA +
			     (i * sizeof(u32)));

		if (endian_t == BIG_ENDIAN)
			*table_x32 = swab32(temp);
		else if (endian_t == LITTLE_ENDIAN)
			*table_x32 = temp;

		if (!(i % 2))
			pr_info(" 0x%08x -> 0x%08x ", i,
				*table_x32);
		else
			pr_info("0x%08x\n", *table_x32);
	}
	pr_info("}\n");

	return 0;
}
