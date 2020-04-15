/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation
 */

#ifndef __RSU_MISC_H__
#define __RSU_MISC_H__

#include <asm/arch/rsu_ll.h>

#define IMAGE_BLOCK_SZ	0x1000	/* Size of a bitstream data block */
#define IMAGE_PTR_BLOCK 0x1000	/* Offset to the PTR BLOCK */
#define IMAGE_PTR_START	0x1F00	/* Offset to main image pointers */
#define IMAGE_PTR_CRC	0x1FFC	/* Offset to CRC for PTR BLOCK */
#define IMAGE_PTR_END   0x1FFF  /* End of PTR BLOCK */

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

void rsu_log(const enum rsu_log_level level, const char *format, ...);
#endif
