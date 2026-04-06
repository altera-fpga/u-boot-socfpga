// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2018-2023 Intel Corporation
 *
 */

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_s10.h>
#include <command.h>
#include <spi.h>
#include <spi_flash.h>
#include <env.h>

DECLARE_GLOBAL_DATA_PTR;

#define RSU_S10_SPT_SLOT_MAX 127

static unsigned int rsu_s10_spt_entry_count(const struct socfpga_rsu_s10_spt *spt)
{
	if (spt->magic_number != RSU_S10_SPT_MAGIC_NUMBER)
		return 0;
	if (spt->entries > RSU_S10_SPT_SLOT_MAX)
		return RSU_S10_SPT_SLOT_MAX;
	return spt->entries;
}

static int rsu_print_status(void)
{
	struct rsu_status_info status_info;

	if (mbox_rsu_status((u32 *)&status_info, sizeof(status_info) / 4)) {
		puts("RSU: Firmware or flash content not supporting RSU\n");
		return -ENOTSUPP;
	}
	puts("RSU: Remote System Update Status\n");
	printf("Current Image\t: 0x%08llx\n", status_info.current_image);
	printf("Last Fail Image\t: 0x%08llx\n", status_info.fail_image);
	printf("State\t\t: 0x%08x\n", status_info.state);
	printf("Version\t\t: 0x%08x\n", status_info.version);
	printf("Error location\t: 0x%08x\n", status_info.error_location);
	printf("Error details\t: 0x%08x\n", status_info.error_details);
	if (RSU_VERSION_ACMF_VERSION(status_info.version) &&
	    RSU_VERSION_DCMF_VERSION(status_info.version))
		printf("Retry counter\t: 0x%08x\n", status_info.retry_counter);

	return 0;
}

static void rsu_print_spt_slot(const struct socfpga_rsu_s10_spt *spt,
			       unsigned int nentries)
{
	unsigned int i;

	puts("RSU: Sub-partition table content\n");
	for (i = 0; i < nentries; i++) {
		printf("%16s\tOffset: 0x%08x%08x\tLength: 0x%08x\tFlag : 0x%08x\n",
		       spt->spt_slot[i].name,
		       spt->spt_slot[i].offset[1],
		       spt->spt_slot[i].offset[0],
		       spt->spt_slot[i].length,
		       spt->spt_slot[i].flag);
	}
}

static void rsu_print_cpb_slot(const struct socfpga_rsu_s10_cpb *cpb)
{
	int i, j = 1;
	unsigned int nslots = cpb->nslots;

	if (nslots > ARRAY_SIZE(cpb->pointer_slot))
		nslots = ARRAY_SIZE(cpb->pointer_slot);

	puts("RSU: CMF pointer block's image pointer list\n");
	if (!nslots)
		return;
	for (i = (int)nslots - 1; i >= 0; i--) {
		if (cpb->pointer_slot[i] != ~0ULL &&
		    cpb->pointer_slot[i] != 0) {
			printf("Priority %d Offset: 0x%016llx nslot: %d\n",
			       j, cpb->pointer_slot[i], i);
			j++;
		}
	}
}

static u32 rsu_spt_slot_find_cpb(const struct socfpga_rsu_s10_spt *spt,
				 unsigned int nentries)
{
	unsigned int i;

	for (i = 0; i < nentries; i++) {
		if (strstr(spt->spt_slot[i].name, "CPB0"))
			return spt->spt_slot[i].offset[0];
	}
	puts("RSU: Cannot find SPT0 entry from sub-partition table\n");
	return 0;
}

/**
 * rsu_spt_cpb_list_inner() - read mailbox SPT offsets, flash SPT/CPB, print
 * @spt0_out: if non-NULL, set after successful mailbox read (for rsu dtb)
 * @spt1_out: if non-NULL, set after successful mailbox read
 */
static int rsu_spt_cpb_list_inner(int argc, char * const argv[],
				  u32 *spt0_out, u32 *spt1_out)
{
	u32 spt_offset[4];
	u32 cpb_offset;
	u32 spt0_off, spt1_off;
	int err;
	struct spi_flash *flash;
	struct socfpga_rsu_s10_spt spt = { 0 };
	struct socfpga_rsu_s10_cpb cpb = { 0 };
	unsigned int nentries;

	if (argc != 1)
		return CMD_RET_USAGE;

	err = rsu_print_status();
	if (err)
		return err;

	if (mbox_rsu_get_spt_offset(spt_offset, 4)) {
		puts("RSU: Error from mbox_rsu_get_spt_offset\n");
		return -ECOMM;
	}
	spt0_off = spt_offset[SPT0_INDEX];
	spt1_off = spt_offset[SPT1_INDEX];

	if (spt0_out)
		*spt0_out = spt0_off;
	if (spt1_out)
		*spt1_out = spt1_off;

	env_set_hex("rsu_sbt0", spt0_off);
	env_set_hex("rsu_sbt1", spt1_off);
	printf("RSU: Sub-partition table 0 offset 0x%08x\n", spt0_off);
	printf("RSU: Sub-partition table 1 offset 0x%08x\n", spt1_off);

	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("RSU: SPI probe failed.\n");
		return -ENODEV;
	}
	if (spi_flash_read(flash, spt0_off, sizeof(spt), &spt)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	if (spt.magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		printf("RSU: Sub-partition table magic number not match 0x%08x\n",
		       spt.magic_number);
		return -EFAULT;
	}

	nentries = rsu_s10_spt_entry_count(&spt);
	rsu_print_spt_slot(&spt, nentries);

	cpb_offset = rsu_spt_slot_find_cpb(&spt, nentries);
	if (!cpb_offset)
		return -ENXIO;
	printf("RSU: CMF pointer block offset 0x%08x\n", cpb_offset);

	if (spi_flash_read(flash, cpb_offset, sizeof(cpb), &cpb)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	if (cpb.magic_number != RSU_S10_CPB_MAGIC_NUMBER) {
		printf("RSU: CMF pointer block magic number not match 0x%08x\n",
		       cpb.magic_number);
		return -EFAULT;
	}

	rsu_print_cpb_slot(&cpb);

	return 0;
}

int rsu_spt_cpb_list(int argc, char * const argv[])
{
	return rsu_spt_cpb_list_inner(argc, argv, NULL, NULL);
}

int rsu_update(int argc, char * const argv[])
{
	u32 flash_offset[2];
	u64 addr;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], &endp, 16);

	flash_offset[0] = lower_32_bits(addr);
	flash_offset[1] = upper_32_bits(addr);

	printf("RSU: RSU update to 0x%08x%08x\n",
	       flash_offset[1], flash_offset[0]);
	mbox_rsu_update(flash_offset);
	return 0;
}

int rsu_dtb(int argc, char * const argv[])
{
	char flash0_string[100];
	int nodeoffset, parentoffset, fdt_flash0_offset, len, end;
	const fdt32_t *val;
	const __be32 *rsu_handle = NULL;
	u32 alt_phandle = 0;
	u32 reg[2];
	u32 spt0_off = 0;
	u32 spt1_off __always_unused = 0;
	int err;

	/* Extracting RSU info from bitstream */
	err = rsu_spt_cpb_list_inner(argc, argv, &spt0_off, &spt1_off);
	/*
	 * The shared inner helper returns CMD_RET_USAGE (positive) when
	 * argv has extra tokens. Surface that to the command framework
	 * directly instead of treating it as SPT/CPB corruption and
	 * stomping on the live DTB.
	 */
	if (err == CMD_RET_USAGE)
		return CMD_RET_USAGE;
	if (err == -ENOTSUPP)
		return 0;
	else if ((err == -ECOMM) || (err == -ENODEV) || (err == -EIO))
		return err;
	else if (err) {
		/*
		 * There was corruption occurred in SPT or CPB, doesn't
		 * return error & let load process continue. So that Linux
		 * can recovery the corrupted SPT or CPB.
		 */
		puts("Corrupted SPT or CPB, Linux will recovery them\n");
	}

	/* Retrieve the soc partition node from Linux DTB as start offset */
	parentoffset = fdt_path_offset(working_fdt, "/soc");
	if (parentoffset < 0) {
		printf("DTB: /soc node not found. Check the dtb and fdt addr.\n");
		return -ENODEV;
	}

	/* Retrieve the QSPI partition node from Linux DTB */
	nodeoffset = fdt_node_offset_by_compatible(working_fdt, parentoffset,
						   "fixed-partitions");
	if (nodeoffset < 0) {
		printf("DTB: QSPI fixed-partitions node not found.\n");
		return -ENODEV;
	}

	/* Retrieve rsu_handle from Linux DTB */
	rsu_handle = fdt_getprop(working_fdt, nodeoffset, "rsu-handle", NULL);
	if (rsu_handle)
		alt_phandle = be32_to_cpup(rsu_handle);

	/* check the rsu phandle exists */
	if (!alt_phandle) {
		printf("DTB: phandle node not found.\n");
		return -ENODEV;
	}

	/* Get the offset of the phandle */
	nodeoffset = fdt_node_offset_by_phandle(working_fdt, alt_phandle);
	if (nodeoffset < 0) {
		printf("DTB: phandle node not found.\n");
		return -ENODEV;
	}

	/* Extract the flash0's reg from Linux DTB */
	fdt_flash0_offset = fdt_get_path(working_fdt, nodeoffset, flash0_string,
					 sizeof(flash0_string));
	if (fdt_flash0_offset < 0) {
		puts("DTB: qspi_boot alias node not found. Check your dts\n");
		return -ENODEV;
	}
	printf("DTB: qspi_boot node at %s\n", flash0_string);

	/* locate the boot partition */
	nodeoffset = fdt_path_offset(working_fdt, flash0_string);
	if (nodeoffset < 0) {
		printf("DTB: %s node not found\n", flash0_string);
		return -ENODEV;
	}

	/* determine initial end address of boot partition */
	val = fdt_getprop(working_fdt, nodeoffset, "reg", &len);
	if (!val) {
		printf("DTB: %s.reg was not found\n", flash0_string);
		return -ENODEV;
	}
	if (len != 2 * sizeof(fdt32_t)) {
		printf("DTB: %s.reg has incorrect length\n", flash0_string);
		return -ENODEV;
	}
	reg[0] = fdt32_to_cpu(val[0]);
	reg[1] = fdt32_to_cpu(val[1]);
	end = reg[0] + reg[1];

	/* align to 64Kb flash sector size */
	end = roundup(end, 64 * 1024);

	/* assemble new reg value for boot partition */
	reg[0] = cpu_to_fdt32(spt0_off);
	reg[1] = cpu_to_fdt32(end  - spt0_off);

	/* update back to Linux DTB */
	return fdt_setprop(working_fdt, nodeoffset, "reg", reg, sizeof(reg));
}
