/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation
 */

#ifndef __RSU_LL_H__
#define __RSU_LL_H__

#include <asm/arch/rsu.h>
#include <asm/types.h>

/**
 * struct rsu_ll_intf - RSU low level interface
 * @exit: exit low level interface
 * @partition.count: get number of slots
 * @@partition.name: get name of a slot
 * @partition.offset: get offset of a slot
 * @partition.size: get size of a slot
 * @partition.reserved: check if a slot is reserved
 * @partition.readonly: check if a slot is read only
 * @partition.rename: rename a slot
 * @priority.get: get priority
 * @priority.add: add priority
 * @priority.remove: remove priority
 * @data.read: read data from flash device
 * @data.write: write date to flash device
 * @data.erase: erase date from flash device
 * @fw_ops.load: inform firmware to load image
 * @fw_ops.status: get status from firmware
 */
struct rsu_ll_intf {
	void (*exit)(void);

	struct {
		int (*count)(void);
		char* (*name)(int part_num);
		u64 (*offset)(int part_num);
		u32 (*size)(int part_num);
		int (*reserved)(int part_num);
		int (*readonly)(int part_num);
		int (*rename)(int part_num, char *name);
	} partition;

	struct {
		int (*get)(int part_num);
		int (*add)(int part_num);
		int (*remove)(int part_num);
	} priority;

	struct {
		int (*read)(int part_num, int offset, int bytes, void *buf);
		int (*write)(int part_num, int offset, int bytes, void *buf);
		int (*erase)(int part_num);
	} data;

	struct {
		int (*load)(u64 offset);
		int (*status)(struct rsu_status_info *info);
	} fw_ops;
};

/**
 * rsu_ll_qspi_init() - low level qspi interface init
 * @intf: pointer to pointer of low level interface
 *
 * Return: 0 on success, or error code
 */
int rsu_ll_qspi_init(struct rsu_ll_intf **intf);
#endif

