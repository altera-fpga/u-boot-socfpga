/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation
 */

#ifndef __RSU_H__
#define __RSU_H__

#include <asm/types.h>

/* RSU Error Codes */
#define EINTF		1
#define ECFG		2
#define ESLOTNUM	3
#define EFORMAT		4
#define EERASE		5
#define EPROGRAM	6
#define ECMP		7
#define ESIZE		8
#define ENAME		9
#define EFILEIO		10
#define ECALLBACK	11
#define ELOWLEVEL	12
#define EWRPROT		13
#define EARGS		14

/**
 * struct rsu_status_info - firmware status log info structure
 * @current_image:address of image currently running in flash
 * @fail_image: address of failed image in flash
 * @state: the state of RSU system
 * @version: the version number of RSU firmware
 * @error_location: the error offset inside the failed image
 * @error_details: error code
 *
 * This structure is used to capture firmware status log information
 */
struct rsu_status_info {
	u64 current_image;
	u64 fail_image;
	u32 state;
	u32 version;
	u32 error_location;
	u32 error_details;
};

/**
 * struct rsu_slot_info - slot information structure
 * @name: a slot name
 * @offset: a slot offset
 * @size: the size of a slot
 * @priority: the priority of a slot
 *
 * This structure is used to capture the slot information details
 */
struct rsu_slot_info {
	char name[16];
	u64 offset;
	u32 size;
	int priority;
};

/**
 * rsu_init() - initialize flash driver, SPT and CPB data
 * @filename: NULL for qspi
 *
 * Returns: 0 on success, or error code
 */
int rsu_init(char *filename);

/**
 * rsu_exit() - free flash driver, clean SPT and CPB data
 */
void rsu_exit(void);

/**
 * rsu_slot_count() - get the number of slots defined
 *
 * Returns: the number of defined slots
 */
int rsu_slot_count(void);

/**
 * rsu_slot_by_name() - get slot number based on name
 * @name: name of slot
 *
 * Return:slot number on success, or error code
 */
int rsu_slot_by_name(char *name);

/**
 * rsu_slot_get_info() - get the attributes of a slot
 * @slot: slot number
 * @info: pointer to info structure to be filled in
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_get_info(int slot, struct rsu_slot_info *info);

/**
 * rsu_slot_size() - get the size of a slot
 * @slot: slot number
 *
 * Returns: the size of the slot in bytes, or error code
 */
int rsu_slot_size(int slot);

/**
 * rsu_slot_priority() - get the Decision CMF load priority of a slot
 * @slot: slot number
 *
 * Priority of zero means the slot has no priority and is disabled.
 * The slot with priority of one has the highest priority.
 *
 * Returns: the priority of the slot, or error code
 */
int rsu_slot_priority(int slot);

/**
 * rsu_slot_erase() - erase all data in a slot
 * @slot: slot number
 *
 * Erase all data in a slot to prepare for programming. Remove the slot
 * if it is in the CMF pointer block.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_erase(int slot);

/**
 * rsu_slot_program_buf() - program a slot from FPGA buffer data
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to program a slot using FPGA config data from a
 * buffer and then enter the slot into CPB.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_program_buf(int slot, void *buf, int size);

/**
 * rsu_slot_program_buf_raw() - program a slot from raw buffer data
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to program a slot using raw data from a buffer,
 * and then enter this slot into CPB.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_program_buf_raw(int slot, void *buf, int size);

/**
 * rsu_slot_verify_buf() - verify FPGA config data in a slot against a
 * buffer
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_verify_buf(int slot, void *buf, int size);

/**
 * rsu_slot_verify_buf_raw() - verify raw data in a slot against a buffer
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_verify_buf_raw(int slot, void *buf, int size);

/**
 * rsu_slot_enable() - enable the selected slot
 * @slot: slot number
 *
 * Set the selected slot as the highest priority. It will be the first
 * slot to be tried after a power-on reset.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_enable(int slot);

/**
 * rsu_slot_disable() - disable the selected slot
 * @slot: slot number
 *
 * Remove the selected slot from the priority scheme, but don't erase the
 * slot data so that it can be re-enabled.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_disable(int slot);

/**
 * rsu_slot_load() - load the selected slot
 * @slot: slot number
 *
 * This function is used to request the selected slot to be loaded
 * immediately. On success, after a small delay, the system is rebooted.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_load(int slot);

/**
 * rsu_slot_load_factory() - load the factory image
 *
 * This function is used to request the factory image to be loaded
 * immediately. On success, after a small delay, the system is rebooted.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_load_factory(void);

/**
 * rsu_slot_rename() - Rename the selected slot.
 * @slot: slot number
 * @name: new name for slot
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_rename(int slot, char *name);

/**
 * rsu_status_log() - Copy firmware status log to info struct
 * @info: pointer to info struct to fill in
 *
 * Return 0 on success, or error code
 */
int rsu_status_log(struct rsu_status_info *info);
#endif
