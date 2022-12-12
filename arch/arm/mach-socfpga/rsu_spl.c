// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 *
 */
#define DEBUG
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
#define SSBL_PART_PREFIX   "SSBL."
#define UBOOT_ENV_EXT ".env"
#define UBOOT_IMG_EXT ".img"
#define UBOOT_ITB_EXT ".itb"
#define UBOOT_ENV_PREFIX "u-boot_"
#define UBOOT_ENV_REDUND_PREFIX "u-boot-redund_"
#define UBOOT_PREFIX "u-boot_"
#define FACTORY_IMG_NAME "FACTORY_IM"

static int get_spl_slot(struct socfpga_rsu_s10_spt *rsu_spt,
			size_t rsu_spt_size, int *crt_spt_index)
{
	u32 rsu_spt0_offset = 0, rsu_spt1_offset = 0;
	u32 spt_offset[4] = {0};
	struct rsu_status_info rsu_status = {0};
	struct spi_flash *flash;
	int i;

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
	if (spi_flash_read(flash, rsu_spt0_offset, rsu_spt_size, rsu_spt)) {
		puts("RSU: Error - spi_flash_read failed!\n");
		return -EINVAL;
	}

	/* if spt0 does not have the correct magic number */
	if (rsu_spt->magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		/* read spt1 */
		if (spi_flash_read(flash, rsu_spt1_offset, rsu_spt_size, rsu_spt)) {
			printf("RSU: Error - spi_flash_read failed!\n");
			return -EINVAL;
		}

		/* bail out if spt1 does not have the correct magic number */
		if (rsu_spt->magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
			printf("RSU: Error: spt table magic number not match 0x%08x!\n",
			       rsu_spt->magic_number);
			return -EINVAL;
		}
	}

	/* display status */
	debug("RSU current image:  0x%08x\n", (u32)rsu_status.current_image);
	debug("RSU state:          0x%08x\n", rsu_status.state);
	debug("RSU error location: 0x%08x\n", rsu_status.error_location);
	debug("RSU error details:  0x%08x\n", rsu_status.error_details);

	/* display partitions */
	for (i = 0; i < rsu_spt->entries; i++) {
		debug("RSU: Partition '%s' start=0x%08x length=0x%08x\n",
		      rsu_spt->spt_slot[i].name, rsu_spt->spt_slot[i].offset[0],
		      rsu_spt->spt_slot[i].length);
	}

	/* locate the SPT entry for currently loaded image */
	for (i = 0; i < rsu_spt->entries; i++) {
		if (((rsu_status.current_image & RSU_ADDR_MASK) ==
			rsu_spt->spt_slot[i].offset[0]) &&
		   ((rsu_status.current_image >> RSU_ADDR_SHIFT) ==
			rsu_spt->spt_slot[i].offset[1])) {
			*crt_spt_index = i;
			return 0;
		}
	}

	puts("RSU: Error - could not locate SPL partition in the SPT table!\n");
	return -EINVAL;
}

static int get_ssbl_slot(struct socfpga_rsu_s10_spt_slot *rsu_ssbl_slot)
{
	struct socfpga_rsu_s10_spt rsu_spt = {0};
	int crt_spt_index = -EINVAL;
	char *result;
	int i, ret;

	rsu_ssbl_slot->offset[0] = -EINVAL;

	ret = get_spl_slot(&rsu_spt, sizeof(rsu_spt), &crt_spt_index);
	if (ret) {
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
				     MAX_PART_NAME_LENGTH - strlen(SSBL_PART_PREFIX)) ||
			    !strncmp(result, rsu_spt.spt_slot[crt_spt_index].name,
				     strlen(FACTORY_IMG_NAME))) {
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

int rsu_spl_mmc_filename(char *filename, int max_size)
{
	struct socfpga_rsu_s10_spt rsu_spt = {0};
	int crt_spt_index = -EINVAL;
	int ret, len;

	if (!filename) {
		printf("RSU: filename is NULL!\n");
		return -ENOENT;
	}

	if ((strlen(UBOOT_PREFIX) + MAX_PART_NAME_LENGTH + strlen(UBOOT_ITB_EXT))
	     > max_size)
		return -ENAMETOOLONG;

	ret = get_spl_slot(&rsu_spt, sizeof(rsu_spt), &crt_spt_index);
	if (ret) {
		if (ret == -EOPNOTSUPP) {
			puts("RSU: Error - mbox_rsu_status failed! Check for RSU image.\n");
			return -EOPNOTSUPP;
		}

		/* should throw error if cannot find u-boot proper(SSBL) in MMC */
		panic("ERROR: could not find u-boot proper(SSBL): SSBL.%s!",
		      rsu_spt.spt_slot[crt_spt_index].name);
	}

	/* add 1 to the length to copy for NULL terminated string */
	strlcat(filename, UBOOT_PREFIX, strlen(UBOOT_PREFIX) + 1);
	strlcat(filename + strlen(UBOOT_PREFIX),
		rsu_spt.spt_slot[crt_spt_index].name,
		strlen(rsu_spt.spt_slot[crt_spt_index].name) + 1);
	len = strlen(UBOOT_PREFIX) + strlen(rsu_spt.spt_slot[crt_spt_index].name);
#if IS_ENABLED(CONFIG_SPL_LOAD_FIT)
	strlcat(filename + len,
		UBOOT_ITB_EXT, strlen(UBOOT_ITB_EXT) + 1);
#else
	strlcat(filename + len,
		UBOOT_IMG_EXT, strlen(UBOOT_IMG_EXT) + 1);
#endif
	printf("%s, filename: %s\n", __func__, filename);
	return 0;
}

int rsu_spl_mmc_env_name(char *filename, int max_size, bool redund)
{
	struct socfpga_rsu_s10_spt rsu_spt = {0};
	int crt_spt_index = -EINVAL;
	int ret, len;

	if (!filename) {
		printf("RSU: filename is NULL!\n");
		return -ENOENT;
	}

	if ((strlen(UBOOT_ENV_REDUND_PREFIX) + strlen(UBOOT_ENV_PREFIX) +
	    MAX_PART_NAME_LENGTH + strlen(UBOOT_ENV_EXT)) > max_size)
		return -ENAMETOOLONG;

	ret = get_spl_slot(&rsu_spt, sizeof(rsu_spt), &crt_spt_index);
	if (ret) {
		if (ret == -EOPNOTSUPP) {
			puts("RSU: Error - mbox_rsu_status failed! Check for RSU image.\n");
			return -EOPNOTSUPP;
		}

		/* should throw error if cannot find u-boot proper(SSBL) in MMC */
		printf("ERROR: could not find u-boot.env!");
		return ret;
	}

	if (redund) {
		strlcat(filename, UBOOT_ENV_REDUND_PREFIX,
			strlen(UBOOT_ENV_REDUND_PREFIX) + 1);
		strlcat(filename + strlen(UBOOT_ENV_REDUND_PREFIX),
			rsu_spt.spt_slot[crt_spt_index].name,
			strlen(rsu_spt.spt_slot[crt_spt_index].name) + 1);
		len = strlen(UBOOT_ENV_REDUND_PREFIX) +
		      strlen(rsu_spt.spt_slot[crt_spt_index].name);
	} else {
		strlcat(filename, UBOOT_ENV_PREFIX, strlen(UBOOT_ENV_PREFIX) + 1);
		strlcat(filename + strlen(UBOOT_ENV_PREFIX),
			rsu_spt.spt_slot[crt_spt_index].name,
			strlen(rsu_spt.spt_slot[crt_spt_index].name) + 1);
		len = strlen(UBOOT_ENV_PREFIX) +
		      strlen(rsu_spt.spt_slot[crt_spt_index].name);
	}
	strlcat(filename + len, UBOOT_ENV_EXT, strlen(UBOOT_ENV_EXT) + 1);

	printf("%s, filename: %s\n", __func__, filename);
	return 0;
}

u32 rsu_spl_ssbl_address(bool is_qspi_imge_check)
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
		if (is_qspi_imge_check) {
			panic("ERROR: could not find u-boot proper(SSBL) address!");
		} else {
			printf("ERROR: could not find u-boot env address!");
			return ret;
		}
	}

	printf("RSU: Success found SSBL at offset: %08x.\n",
	       rsu_ssbl_slot.offset[0]);
	return rsu_ssbl_slot.offset[0];
}

u32 rsu_spl_ssbl_size(bool is_qspi_imge_check)
{
	int ret;
	struct socfpga_rsu_s10_spt_slot rsu_ssbl_slot = {0};

	/* check for valid u-boot proper(SSBL) address for the size */
	ret = get_ssbl_slot(&rsu_ssbl_slot);
	if (ret) {
		if (ret == -EOPNOTSUPP) {
			printf("ERROR: Invalid address, could not retrieve SSBL size!");
			return 0;
		}

		/* should throw error if cannot find u-boot proper(SSBL) address */
		if (is_qspi_imge_check) {
			panic("ERROR: could not find u-boot proper(SSBL) address!");
		} else {
			printf("ERROR: could not find u-boot env address!");
			return ret;
		}
	}

	if (!rsu_ssbl_slot.length) {
		/* throw error if cannot find u-boot proper(SSBL) size */
		printf("ERROR: could not retrieve u-boot proper(SSBL) size!");
		return ret;
	}

	printf("RSU: Success found SSBL with length: %08x.\n",
	       rsu_ssbl_slot.length);
	return rsu_ssbl_slot.length;
}

unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	return rsu_spl_ssbl_address(true);
}
