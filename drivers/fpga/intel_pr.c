/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <wait_bit.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/libfdt.h>
#include <fdtdec.h>

DECLARE_GLOBAL_DATA_PTR;

#define FREEZE_CSR_STATUS_OFFSET		0
#define FREEZE_CSR_CTRL_OFFSET			4
#define FREEZE_CSR_ILLEGAL_REQ_OFFSET		8
#define FREEZE_CSR_REG_VERSION			12

#define FREEZE_CSR_STATUS_FREEZE_REQ_DONE	BIT(0)
#define FREEZE_CSR_STATUS_UNFREEZE_REQ_DONE	BIT(1)

#define FREEZE_CSR_CTRL_FREEZE_REQ		BIT(0)
#define FREEZE_CSR_CTRL_RESET_REQ		BIT(1)
#define FREEZE_CSR_CTRL_UNFREEZE_REQ		BIT(2)

#define FREEZE_TIMEOUT				20000

static int intel_get_freeze_br_addr(fdt_addr_t *addr, unsigned int region)
{
	int offset;
	char freeze_br[12];
	struct fdt_resource r;
	int ret;

	snprintf(freeze_br, sizeof(freeze_br), "freeze_br%d", region);

	const char *alias = fdt_get_alias(gd->fdt_blob, freeze_br);
	if (!alias)	{
		printf("alias %s not found in dts\n", freeze_br);
		return -ENODEV;
	}

	offset = fdt_path_offset(gd->fdt_blob, alias);
	if (offset < 0) {
		printf("%s not found in dts\n", alias);
		return -ENODEV;
	}

	ret = fdt_get_resource(gd->fdt_blob, offset, "reg", 0, &r);
	if (ret) {
		printf("%s has no 'reg' property!\n", freeze_br);
		return ret;
	}

	*addr = r.start;

	return ret;
}

static int intel_freeze_br_do_freeze(unsigned int region)
{
	u32 status;
	int ret;
	fdt_addr_t addr;

	ret = intel_get_freeze_br_addr(&addr, region);
	if (ret)
		return ret;

	status = readl(addr + FREEZE_CSR_STATUS_OFFSET);

	if (status & FREEZE_CSR_STATUS_FREEZE_REQ_DONE)
		return 0;
	else if (!(status & FREEZE_CSR_STATUS_UNFREEZE_REQ_DONE))
		return -EINVAL;

	writel(FREEZE_CSR_CTRL_FREEZE_REQ, addr + FREEZE_CSR_CTRL_OFFSET);

	return wait_for_bit_le32((const u32 *)(addr +
				 FREEZE_CSR_STATUS_OFFSET),
				 FREEZE_CSR_STATUS_FREEZE_REQ_DONE, true,
				 FREEZE_TIMEOUT, false);
}

static int intel_freeze_br_do_unfreeze(unsigned int region)
{
	u32 status;
	int ret;
	fdt_addr_t addr;

	ret = intel_get_freeze_br_addr(&addr, region);
	if (ret)
		return ret;

	status = readl(addr + FREEZE_CSR_STATUS_OFFSET);

	if (status & FREEZE_CSR_STATUS_UNFREEZE_REQ_DONE)
		return 0;
	else if (!(status & FREEZE_CSR_STATUS_FREEZE_REQ_DONE))
		return -EINVAL;

	writel(FREEZE_CSR_CTRL_UNFREEZE_REQ, addr + FREEZE_CSR_CTRL_OFFSET);

	return wait_for_bit_le32((const u32 *)(addr +
				 FREEZE_CSR_STATUS_OFFSET),
				 FREEZE_CSR_STATUS_UNFREEZE_REQ_DONE, true,
				 FREEZE_TIMEOUT, false);
}

static int do_pr(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;
	char *region;
	unsigned int region_num = 0;

	if (argc != 2 && argc != 3)
		return CMD_RET_USAGE;

	cmd = argv[1];
	if (argc == 3)
		region_num = simple_strtoul(argv[2], &region, 0);

	if (strcmp(cmd, "start") == 0)
		return intel_freeze_br_do_freeze(region_num);
	else if (strcmp(cmd, "end") == 0)
		return intel_freeze_br_do_unfreeze(region_num);

	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	pr, 3, 1, do_pr,
	"SoCFPGA partial reconfiguration control",
	"start [region] - Start the partial reconfiguration by freeze the "
	"PR region\n"
	"end [region] - End the partial reconfiguration by unfreeze the PR "
	"region\n"
	""
);
