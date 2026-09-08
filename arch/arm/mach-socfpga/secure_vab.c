// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 */

#include <log.h>
#include <malloc.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/secure_vab.h>
#include <asm/arch/socfpga_prov_status.h>
#include <asm/arch/smc_api.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/intel-smc.h>

#define CHUNKSZ_PER_WD_RESET		(256 * SZ_1K)

/*
 * Query device ownership via MBOX_FCS_GET_PROVISION (0x7B).
 *
 * NON_OWNED when SDM returns 0x85 for this command (not owned, QS-580661 /
 * VAB_SDOS), or when a successful blob has secure-state 0, hash-count
 * 0xFF (no programmed hash), or an all-zero first owner root hash.
 * Word 0 is this-POR provision-flow status, not OWNED — do not gate on it.
 * Word 2 [15:8] == 0 means one hash (value + 1), not an empty list.
 *
 * Trust boundary: same HPS↔SDM mailbox path as VAB; does not address MITM
 * on mailbox responses (ES-14722). 0x85 on 0x7B is still a generic denial
 * if SDM ever emits it on an OWNED device.
 */
int socfpga_query_prov_status(void)
{
	u32 prov_data[FCS_PROV_DATA_WORD_SIZE];
	u32 resp_len = FCS_PROV_DATA_WORD_SIZE;
	int ret;

	memset(prov_data, 0, sizeof(prov_data));

	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		ret = smc_send_mailbox(MBOX_FCS_GET_PROVISION, 0, NULL, 0,
				       &resp_len, prov_data);
	} else {
		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_FCS_GET_PROVISION,
				    MBOX_CMD_DIRECT, 0, NULL, 0, &resp_len,
				    prov_data);
	}

	return socfpga_prov_state_from_mbox_ret(ret, prov_data, resp_len,
			MBOX_RESP_NOT_ALLOWED_UNDER_SECURITY_SETTINGS);
}

/*
 * Read the length of the VAB certificate from the end of image
 * and calculate the actual image size (excluding the VAB certificate).
 */
static size_t get_img_size(u8 *img_buf, size_t img_buf_sz)
{
	u8 *img_buf_end = img_buf + img_buf_sz;
	u32 cert_sz = get_unaligned_le32(img_buf_end - sizeof(u32));
	u8 *p = img_buf_end - cert_sz - sizeof(u32);

	/* Ensure p is pointing within the img_buf */
	if (p < img_buf || p > (img_buf_end - VAB_CERT_HEADER_SIZE))
		return 0;

	if (get_unaligned_le32(p) == SDM_CERT_MAGIC_NUM)
		return (size_t)(p - img_buf);

	return 0;
}

/*
 * Vendor Authorized Boot (VAB) is a security feature for authenticating
 * the images such as U-Boot, ARM trusted Firmware, Linux kernel,
 * device tree blob and etc loaded from FIT. User can also trigger
 * the VAB authentication from U-Boot command.
 *
 * This function extracts the VAB certificate and signature block
 * appended at the end of the image, then send to Secure Device Manager
 * (SDM) for authentication. This function will validate the SHA384
 * of the image against the SHA384 hash stored in the VAB certificate
 * before sending the VAB certificate to SDM for authentication.
 *
 * RETURN
 * 0 if authentication success, or if SDM returns 0x85 and
 *   MBOX_FCS_GET_PROVISION confirms NON_OWNED (0x85 on 0x7B,
 *   secure-state 0, hash-count 0xFF, or all-zero first owner
 *   root hash)
 * negative error code if authentication fail
 */
int socfpga_vendor_authentication(void **p_image, size_t *p_size)
{
	int retry_count = 20;
	u8 hash384[SHA384_SUM_LEN];
	u64 img_addr, mbox_data_addr;
	size_t img_sz, mbox_data_sz;
	u8 *cert_hash_ptr, *mbox_relocate_data_addr;
	u32 resp = 0, resp_len = 1;
	int ret;

	img_addr = (uintptr_t)*p_image;

	debug("Authenticating image at address 0x%016llx (%ld bytes)\n",
	      img_addr, *p_size);

	img_sz = get_img_size((u8 *)img_addr, *p_size);
	debug("img_sz = %ld\n", img_sz);

	if (!img_sz) {
		puts("VAB certificate not found in image!\n");
		return -ENOKEY;
	}

	if (!IS_ALIGNED(img_sz, sizeof(u32))) {
		printf("Image size (%ld bytes) not aliged to 4 bytes!\n",
		       img_sz);
		return -EBFONT;
	}

	/* Generate HASH384 from the image */
	sha384_csum_wd((u8 *)img_addr, img_sz, hash384, CHUNKSZ_PER_WD_RESET);

	cert_hash_ptr = (u8 *)(img_addr + img_sz + VAB_CERT_MAGIC_OFFSET +
			       VAB_CERT_FIT_SHA384_OFFSET);

	/*
	 * Compare the SHA384 found in certificate against the SHA384
	 * calculated from image
	 */
	if (memcmp(hash384, cert_hash_ptr, SHA384_SUM_LEN)) {
		puts("SHA384 not match!\n");
		return -EKEYREJECTED;
	}

	mbox_data_addr = img_addr + img_sz - sizeof(u32);
	/* Size in word (32bits) */
	mbox_data_sz = (ALIGN(*p_size - img_sz, sizeof(u32))) >> 2;

	debug("mbox_data_addr = 0x%016llx\n", mbox_data_addr);
	debug("mbox_data_sz = %ld words\n", mbox_data_sz);

	/*
	 * Relocate certificate to first memory block before trigger SMC call
	 * to send mailbox command because ATF only able to access first
	 * memory block.
	 */
	mbox_relocate_data_addr = (u8 *)malloc(mbox_data_sz * sizeof(u32));
	if (!mbox_relocate_data_addr) {
		puts("Out of memory for VAB certificate relocation!\n");
		return -ENOMEM;
	}

	memcpy(mbox_relocate_data_addr, (u8 *)mbox_data_addr, mbox_data_sz * sizeof(u32));
	*(u32 *)mbox_relocate_data_addr = 0;

	debug("mbox_relocate_data_addr = 0x%p\n", mbox_relocate_data_addr);

	do {
		if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
			/* Invoke SMC call to ATF to send the VAB certificate to SDM */
			ret  = smc_send_mailbox(MBOX_VAB_SRC_CERT, mbox_data_sz,
						(u32 *)mbox_relocate_data_addr, 0, &resp_len,
						&resp);
		} else {
			/* Send the VAB certficate to SDM for authentication */
			ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_VAB_SRC_CERT,
					    MBOX_CMD_DIRECT, mbox_data_sz,
					    (u32 *)mbox_relocate_data_addr, 0, &resp_len,
					    &resp);
		}
		/* If SDM is not available, just delay 50ms and retry again */
		if (ret == MBOX_RESP_DEVICE_BUSY)
			mdelay(50);
		else
			break;
	} while (--retry_count);

	/* Free the relocate certificate memory space */
	free(mbox_relocate_data_addr);

	/* Exclude the size of the VAB certificate from image size */
	*p_size = img_sz;

	debug("ret = 0x%08x, resp = 0x%08x, resp_len = %d\n", ret, resp,
	      resp_len);

	if (ret) {
		/*
		 * 0x85 = MBOX_RESP_NOT_ALLOWED_UNDER_SECURITY_SETTINGS — a
		 * generic SDM denial, not a verified "unprovisioned" oracle.
		 * Gate bypass on GET_PROVISION (0x7B): allow only NON_OWNED.
		 * Fail closed if the query fails or the device is OWNED.
		 * Does not address MITM on mailbox responses (ES-14722).
		 */
		if (ret == MBOX_RESP_NOT_ALLOWED_UNDER_SECURITY_SETTINGS) {
			int prov = socfpga_query_prov_status();

			if (prov == SOCFPGA_PROV_NON_OWNED) {
				printf("SDM VAB skipped (non-OWNED) at 0x%016llx (%ld bytes)\n",
				       img_addr, img_sz);
				return 0;
			}

			puts("VAB certificate authentication failed in SDM");
			puts(" (not allowed under security settings");
			if (prov == SOCFPGA_PROV_QUERY_ERROR)
				puts(", prov_status query failed");
			puts(")\n");
			return -EKEYREJECTED;
		}
		puts("VAB certificate authentication failed in SDM");
		if (ret == MBOX_RESP_DEVICE_BUSY) {
			puts(" (SDM busy timeout)\n");
			return -ETIMEDOUT;
		} else if (ret == MBOX_RESP_UNKNOWN) {
			puts(" (Not supported)\n");
			return -ESRCH;
		}
		puts("\n");
		return -EKEYREJECTED;
	} else {
		/* If Certificate Process Status has error */
		if (resp) {
			puts("VAB certificate process failed\n");
			return -ENOEXEC;
		}
	}

	printf("%s 0x%016llx (%ld bytes)\n",
	       "Image Authentication passed at address", img_addr, img_sz);

	return 0;
}
