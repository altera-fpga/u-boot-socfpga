/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Altera SoC FPGA Remote System Update - generic helpers and logging.
 *
 * Copyright (C) 2019 Intel Corporation
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * Cross-arch so the same dispatcher (rsu.c) and the sandbox stub
 * backend can both link. The arm-private forwarder lives at
 * arch/arm/mach-socfpga/include/mach/rsu_misc.h.
 */

#ifndef __SOCFPGA_RSU_MISC_H__
#define __SOCFPGA_RSU_MISC_H__

#include <socfpga_rsu_ll.h>

/*
 * A bitstream is a recurrent data structure composed of sections. Each section
 * consists of 4KB blocks. The first block in a section is called the main
 * descriptor and the first 32bit value in that descriptor identifies the
 * section type, with 0x62294895 denoting a CMF section.
 * The second block in a section is called a signature block. The last 256 bytes
 * of the signature block are called the main image pointer, and contains up to
 * four pointers to other sections in the bitstream. The entire signature block,
 * including the main pointer area is protected by a 32-bit CRC.
 *
 * The slot size is used to determine if the bitstream was generated using a
 * slot offset address of zero. The main image pointers of all the CMF sections
 * identified in the bitstream are updated when programming into a slot if all
 * of the pointers are less than the slot size.
 */

#define IMAGE_BLOCK_SZ      0x1000      /* Bitstream block size */
#define SIG_BLOCK_PTR_OFFS  0x0F00      /* Signature block pointer offset */
#define SIG_BLOCK_CRC_OFFS  0x0FFC      /* Signature block CRC offset */
#define CMF_MAGIC           0x62294895  /* Magic identifier for CMF sections */

/* log level */
enum rsu_log_level {
	RSU_EMERG = 0,
	RSU_ALERT,
	RSU_CRIT,
	RSU_ERR,
	RSU_WARNING,
	RSU_NOTICE,
	RSU_INFO,
	RSU_DEBUG
};

typedef int (*rsu_data_callback)(void *buf, int size);
int rsu_cb_buf_init(void *buf, int size);
void rsu_cb_buf_exit(void);
int rsu_cb_buf(void *buf, int len);
int rsu_cb_program_common(struct rsu_ll_intf *ll_intf, int slot,
			  rsu_data_callback callback, int rawdata);
int rsu_cb_verify_common(struct rsu_ll_intf *ll_intf, int slot,
			 rsu_data_callback callback, int rawdata);

int rsu_misc_is_rsvd_name(char *name);
int rsu_misc_is_slot(struct rsu_ll_intf *ll_intf, int part_num);
int rsu_misc_slot2part(struct rsu_ll_intf *ll_intf, int slot);
int rsu_misc_writeprotected(int slot);
void rsu_misc_safe_strcpy(char *dst, int dsz, char *src, int ssz);
int rsu_misc_spt_checksum_enabled(void);

void rsu_log(const enum rsu_log_level level, const char *format, ...);
int smc_store_max_retry(u32 value);

void swap_bits(char *data, int size);
/*
 * RSU-scoped integer power. Named with the rsu_ prefix to avoid
 * colliding with libm's double pow(double, double) when this header
 * is consumed by hosted builds (e.g. the sandbox ut socfpga suite).
 */
int rsu_pow(u32 x, u32 y);

#endif /* __SOCFPGA_RSU_MISC_H__ */
