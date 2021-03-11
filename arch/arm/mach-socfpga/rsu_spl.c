// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 *
 */
#include <common.h>
#include <linux/errno.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_s10.h>
#include <asm/arch/rsu_spl.h>

#define SSBL_PART_PREFIX	"SSBL."
#define RSU_ADDR_MASK	0xFFFFFFFF
#define RSU_ADDR_SHIFT	32

static int get_ssbl_slot(struct socfpga_rsu_s10_spt_slot *rsu_ssbl_slot)
{
	struct socfpga_rsu_s10_spt rsu_spt = {0};
	u32 rsu_spt0_offset = 0, rsu_spt1_offset = 0;
	u32 spt_offset[4];
	struct rsu_status_info rsu_status;
	int crt_spt_index = -EINVAL;
	char *result;
	struct spi_flash *flash;
	int i;

	rsu_ssbl_slot->offset[0] = -EINVAL;

	/* get rsu status */
	if (mbox_rsu_status((u32 *)&rsu_status, sizeof(rsu_status) / 4)) {
		puts("RSU: Error - mbox_rsu_status failed!\n");
		return -EOPNOTSUPP;
	}

	/* get spt offsets */
	if (mbox_rsu_get_spt_offset(spt_offset, 4)) {
		puts("RSU: Error - mbox_rsu_get_spt_offset failed!\n");
		return -EINVAL;
	}

	rsu_spt0_offset = spt_offset[SPT0_INDEX];
	rsu_spt1_offset = spt_offset[SPT1_INDEX];

	/* initialize flash */
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("RSU: Error - spi_flash_probe failed!\n");
		return -EINVAL;
	}

	/* read spt0 */
	if (spi_flash_read(flash, rsu_spt0_offset, sizeof(rsu_spt), &rsu_spt)) {
		puts("RSU: Error - spi_flash_read failed!\n");
		return -EINVAL;
	}

	/* if spt0 does not have the correct magic number */
	if (rsu_spt.magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		/* read spt1 */
		if (spi_flash_read(flash, rsu_spt1_offset, sizeof(rsu_spt), &rsu_spt)) {
			printf("RSU: Error - spi_flash_read failed!\n");
			return -EINVAL;
		}

		/* bail out if spt1 does not have the correct magic number */
		if (rsu_spt.magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
			printf("RSU: Error: spt table magic number not match 0x%08x!\n",
			       rsu_spt.magic_number);
			return -EINVAL;
		}
	}

	/* display status */
	debug("RSU current image:  0x%08x\n", (u32)rsu_status.current_image);
	debug("RSU state:          0x%08x\n", rsu_status.state);
	debug("RSU error location: 0x%08x\n", rsu_status.error_location);
	debug("RSU error details:  0x%08x\n", rsu_status.error_details);

	/* display partitions */
	for (i = 0; i < rsu_spt.entries; i++) {
		debug("RSU: Partition '%s' start=0x%08x length=0x%08x\n",
		      rsu_spt.spt_slot[i].name, rsu_spt.spt_slot[i].offset[0],
		      rsu_spt.spt_slot[i].length);
	}

	/* locate the SPT entry for currently loaded image */
	for (i = 0; i < rsu_spt.entries; i++) {
		if (((rsu_status.current_image & RSU_ADDR_MASK) ==
			rsu_spt.spt_slot[i].offset[0]) &&
		   ((rsu_status.current_image >> RSU_ADDR_SHIFT) ==
			rsu_spt.spt_slot[i].offset[1])) {
			crt_spt_index = i;
			break;
		}
	}

	if (crt_spt_index == -EINVAL) {
		puts("RSU: Error - could not locate partition in the SPT table!\n");
		return -EINVAL;
	}

	/* locate the u-boot proper(SSBL) partition and return its address */
	for (i = 0; i < rsu_spt.entries; i++) {
		/* get the substring ptr to the first occurrence of SSBL. prefix */
		result = strstr(rsu_spt.spt_slot[i].name, SSBL_PART_PREFIX);

		/* skip if not found the SSBL prefix */
		if (!result)
			continue;

		/* check if the prefix is located at the first */
		if (result == rsu_spt.spt_slot[i].name) {
			/* move to the substring after SSBL. prefix */
			result += strlen(SSBL_PART_PREFIX);

			/* compare SPL's spt name after the prefix */
			if (!strncmp(result, rsu_spt.spt_slot[crt_spt_index].name,
				     MAX_PART_NAME_LENGTH - strlen(SSBL_PART_PREFIX))) {
				printf("RSU: found SSBL partition %s at address 0x%08x.\n",
				       result, (int)rsu_spt.spt_slot[i].offset[0]);
				memcpy(rsu_ssbl_slot, &rsu_spt.spt_slot[i],
				       sizeof(struct socfpga_rsu_s10_spt_slot));

				return 0;
			}
		}
	}

	/* fail to find u-boot proper(SSBL) */
	printf("RSU: Error - could not find u-boot proper partition SSBL.%s!\n",
	       rsu_spt.spt_slot[crt_spt_index].name);

	return -EINVAL;
}

u32 rsu_spl_ssbl_address(void)
{
	int ret;
	struct socfpga_rsu_s10_spt_slot rsu_ssbl_slot = {0};

	ret = get_ssbl_slot(&rsu_ssbl_slot);
	if (ret) {
		if (ret == -EOPNOTSUPP) {
			puts("RSU: Error - mbox_rsu_status failed! Check for RSU image.\n");
			return CONFIG_SYS_SPI_U_BOOT_OFFS;
		}

		/* should throw error if cannot find u-boot proper(SSBL) address */
		panic("ERROR: could not find u-boot proper(SSBL) address!");
	}

	printf("RSU: Success found SSBL at offset: %08x.\n", rsu_ssbl_slot.offset[0]);
	return rsu_ssbl_slot.offset[0];
}

u32 rsu_spl_ssbl_size(void)
{
	struct socfpga_rsu_s10_spt_slot rsu_ssbl_slot = {0};

	/* check for valid u-boot proper(SSBL) address for the size */
	if (get_ssbl_slot(&rsu_ssbl_slot) == -EOPNOTSUPP) {
		printf("ERROR: Invalid address, could not retrieve SSBL size!");
		return 0;
	}

	if (!rsu_ssbl_slot.length) {
		/* throw error if cannot find u-boot proper(SSBL) size */
		panic("ERROR: could not retrieve u-boot proper(SSBL) size!");
	}

	printf("RSU: Success found SSBL with length: %08x.\n", rsu_ssbl_slot.length);
	return rsu_ssbl_slot.length;
}

unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	return rsu_spl_ssbl_address();
}
