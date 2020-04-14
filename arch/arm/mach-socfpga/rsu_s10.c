/*
 *  Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <linux/errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu_s10.h>
#include <command.h>
#include <spi.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

struct socfpga_rsu_s10_cpb rsu_cpb = {0};
struct socfpga_rsu_s10_spt rsu_spt = {0};
u32 rsu_spt0_offset = 0, rsu_spt1_offset = 0;

static int rsu_print_status(void)
{
	struct socfpga_rsu_s10_status rsu_status;

	if (mbox_rsu_status((u32 *)&rsu_status, sizeof(rsu_status) / 4)) {
		puts("RSU: Firmware or flash content not supporting RSU\n");
		return -ENOTSUPP;
	}
	puts("RSU: Remote System Update Status\n");
	printf("Current Image\t: 0x%08x\n", rsu_status.current_image[0]);
	printf("Last Fail Image\t: 0x%08x\n", rsu_status.last_failing_image[0]);
	printf("State\t\t: 0x%08x\n", rsu_status.state);
	printf("Version\t\t: 0x%08x\n", rsu_status.version);
	printf("Error locaton\t: 0x%08x\n", rsu_status.error_location);
	printf("Error details\t: 0x%08x\n", rsu_status.error_details);

	return 0;
}

static void rsu_print_spt_slot(void)
{
	int i;

	puts("RSU: Sub-partition table content\n");
	for (i = 0; i < rsu_spt.entries; i++) {
		printf("%16s\tOffset: 0x%08x%08x\tLength: 0x%08x\tFlag : 0x%08x\n",
		       rsu_spt.spt_slot[i].name,
		       rsu_spt.spt_slot[i].offset[1],
		       rsu_spt.spt_slot[i].offset[0],
		       rsu_spt.spt_slot[i].length,
		       rsu_spt.spt_slot[i].flag);
	}
}

static void rsu_print_cpb_slot(void)
{
	int i, j = 1;

	puts("RSU: CMF pointer block's image pointer list\n");
	for (i = rsu_cpb.nslots - 1; i >= 0; i--) {
		if (rsu_cpb.pointer_slot[i] != ~0 &&
		    rsu_cpb.pointer_slot[i] != 0) {
			printf("Priority %d Offset: 0x%016llx nslot: %d\n",
			       j, rsu_cpb.pointer_slot[i], i);
			j++;
		    }
	}
}

static u32 rsu_spt_slot_find_cpb(void)
{
	int i;

	for (i = 0; i < rsu_spt.entries; i++) {
		if (strstr(rsu_spt.spt_slot[i].name, "CPB0") != NULL)
			return rsu_spt.spt_slot[i].offset[0];
	}
	puts("RSU: Cannot find SPT0 entry from sub-partition table\n");
	return 0;
}

int rsu_spt_cpb_list(void)
{
	u32 spt_offset[4];
	u32 cpb_offset;
	int err;
	struct spi_flash *flash;

	/* print the RSU status */
	err = rsu_print_status();
	if (err)
		return err;

	/* retrieve the sub-partition table (spt) offset from SDM */
	if (mbox_rsu_get_spt_offset(spt_offset, 4)) {
		puts("RSU: Error from mbox_rsu_get_spt_offset\n");
		return -ECOMM;
	}
	rsu_spt0_offset = spt_offset[SPT0_INDEX];
	rsu_spt1_offset = spt_offset[SPT1_INDEX];

	/* update into U-Boot env so we can update into DTS later */
	env_set_hex("rsu_sbt0", rsu_spt0_offset);
	env_set_hex("rsu_sbt1", rsu_spt1_offset);
	printf("RSU: Sub-partition table 0 offset 0x%08x\n", rsu_spt0_offset);
	printf("RSU: Sub-partition table 1 offset 0x%08x\n", rsu_spt1_offset);

	/* retrieve sub-partition table (spt) from flash */
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("RSU: SPI probe failed.\n");
		return -ENODEV;
	}
	if (spi_flash_read(flash, rsu_spt0_offset, sizeof(rsu_spt), &rsu_spt)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	/* valid the sub-partition table (spt) magic number */
	if (rsu_spt.magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		printf("RSU: Sub-partition table magic number not match 0x%08x\n",
		       rsu_spt.magic_number);
		return -EFAULT;
	}

	/* list the sub-partition table (spt) content */
	rsu_print_spt_slot();

	/* locate where is CMF pointer block (cpb) */
	cpb_offset = rsu_spt_slot_find_cpb();
	if (!cpb_offset)
		return -ENXIO;
	printf("RSU: CMF pointer block offset 0x%08x\n", cpb_offset);

	/* retrieve CMF pointer block (cpb) from flash */
	if (spi_flash_read(flash, cpb_offset, sizeof(rsu_cpb), &rsu_cpb)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	/* valid the CMF pointer block (cpb) magic number */
	if (rsu_cpb.magic_number != RSU_S10_CPB_MAGIC_NUMBER) {
		printf("RSU: CMF pointer block magic number not match 0x%08x\n",
		       rsu_cpb.magic_number);
		return -EFAULT;
	}

	/* list the CMF pointer block (cpb) content */
	rsu_print_cpb_slot();

	return 0;
}

int rsu_update(int argc, char * const argv[])
{
	u32 flash_offset[2];
	u64 addr;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], &endp, 16);

	flash_offset[0] = addr & 0xFFFFFFFF;
	flash_offset[1] = (addr >> 32) & 0xFFFFFFFF;

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
	int err;

	/* Extracting RSU info from bitstream */
	err = rsu_spt_cpb_list();
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
	reg[0] = cpu_to_fdt32(rsu_spt0_offset);
	reg[1] = cpu_to_fdt32(end  - rsu_spt0_offset);

	/* update back to Linux DTB */
	return fdt_setprop(working_fdt, nodeoffset, "reg", reg, sizeof(reg));
}

int do_rsu(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];
	--argc;
	++argv;

	if (strcmp(cmd, "list") == 0)
		ret = rsu_spt_cpb_list();
	else if (strcmp(cmd, "update") == 0)
		ret = rsu_update(argc, argv);
	else if (strcmp(cmd, "dtb") == 0)
		ret = rsu_dtb(argc, argv);
	else if (strcmp(cmd, "dtb") == 0)
		ret = rsu_dtb(argc, argv);
	else
		return CMD_RET_USAGE;

	return ret;
}

U_BOOT_CMD(
	rsu, 3, 1, do_rsu,
	"SoCFPGA Stratix10 SoC Remote System Update",
	"list  - List down the available bitstreams in flash\n"
	"update <flash_offset> - Initiate SDM to load bitstream as specified\n"
	"		       by flash_offset\n"
	"dtb   - Update Linux DTB qspi-boot parition offset with spt0 value\n"
	""
);
