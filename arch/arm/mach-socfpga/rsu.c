/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <common.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_misc.h>
#include <asm/system.h>

/* RSU Notify Bitmasks */
#define RSU_NOTIFY_IGNORE_STAGE         BIT(18)
#define RSU_NOTIFY_CLEAR_ERROR_STATUS   BIT(17)
#define RSU_NOTIFY_RESET_RETRY_COUNTER  BIT(16)

struct rsu_ll_intf *ll_intf;

/**
 * rsu_init() - initialize flash driver, SPT and CPB data
 * @filename: NULL for qspi
 *
 * Returns: 0 on success, or error code
 */
int rsu_init(char *filename)
{
	int ret;

	if (ll_intf) {
		rsu_log(RSU_ERR, "ll_intf initialized\n");
		return -EINTF;
	}

	ret = rsu_ll_qspi_init(&ll_intf);
	if (ret) {
		rsu_exit();
		return -ENODEV;
	}

	return 0;
}

/**
 * rsu_exit() - free flash driver, clean SPT and CPB data
 */
void rsu_exit(void)
{
	if (ll_intf && ll_intf->exit)
		ll_intf->exit();

	ll_intf = NULL;
}

/**
 * rsu_slot_count() - get the number of slots defined
 *
 * Returns: the number of defined slots
 */
int rsu_slot_count(void)
{
	int partitions;
	int cnt = 0;
	int x;

	if (!ll_intf)
		return -EINTF;

	partitions = ll_intf->partition.count();

	for (x = 0; x < partitions; x++) {
		if (rsu_misc_is_slot(ll_intf, x))
			cnt++;
	}

	return cnt;
}

/**
 * rsu_slot_by_name() - get slot number based on name
 * @name: name of slot
 *
 * Return: slot number on success, or error code
 */
int rsu_slot_by_name(char *name)
{
	int partitions;
	int cnt = 0;
	int x;

	if (!ll_intf)
		return -EINTF;

	if (!name)
		return -EARGS;

	partitions = ll_intf->partition.count();

	for (x = 0; x < partitions; x++) {
		if (rsu_misc_is_slot(ll_intf, x)) {
			if (!strcmp(name, ll_intf->partition.name(x)))
				return cnt;
			cnt++;
		}
	}

	return -ENAME;
}

/**
 * rsu_slot_get_info() - get the attributes of a slot
 * @slot: slot number
 * @info: pointer to info structure to be filled in
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_get_info(int slot, struct rsu_slot_info *info)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (!info)
		return -EARGS;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -EINVAL;

	rsu_misc_safe_strcpy(info->name, sizeof(info->name),
			     ll_intf->partition.name(part_num),
			     sizeof(info->name));

	info->offset = ll_intf->partition.offset(part_num);
	info->size = ll_intf->partition.size(part_num);
	info->priority = ll_intf->priority.get(part_num);

	return 0;
}

/**
 * rsu_slot_size() - get the size of a slot
 * @slot: slot number
 *
 * Returns: the size of the slot in bytes, or error code
 */
int rsu_slot_size(int slot)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	return ll_intf->partition.size(part_num);
}

/**
 * rsu_slot_priority() - get the Decision CMF load priority of a slot
 * @slot: slot number
 *
 * Priority of zero means the slot has no priority and is disabled.
 * The slot with priority of one has the highest priority.
 *
 * Returns: the priority of the slot, or error code
 */
int rsu_slot_priority(int slot)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	return ll_intf->priority.get(part_num);
}

/**
 * rsu_slot_erase() - erase all data in a slot
 * @slot: slot number
 *
 * Erase all data in a slot to prepare for programming. Remove the slot
 * if it is in the CMF pointer block.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_erase(int slot)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (rsu_misc_writeprotected(slot)) {
		rsu_log(RSU_ERR, "Trying to erase a write protected slot\n");
		return -EWRPROT;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	if (ll_intf->data.erase(part_num))
		return -ELOWLEVEL;

	return 0;
}

/**
 * rsu_slot_program_buf() - program a slot from FPGA buffer data
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to program a slot using FPGA config data from
 * a buffer and then enter the slot into CPB.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_program_buf(int slot, void *buf, int size)
{
	int ret;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (!ll_intf)
		return -EINTF;

	if (rsu_cb_buf_init(buf, size)) {
		rsu_log(RSU_ERR, "Bad buf/size arguments\n");
		return -EARGS;
	}

	ret = rsu_cb_program_common(ll_intf, slot, rsu_cb_buf, 0);
	if (ret) {
		rsu_log(RSU_ERR, "fail to program buf data\n");
		return ret;
	}

	rsu_cb_buf_exit();
	return ret;
}

/**
 * rsu_slot_program_factory_update_buf() - program a slot using factory update
 *                                         data from a buffer
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer
 *
 * This function is used to program a slot using factory update data from a
 * buffer and then enter the slot into CPB.
 *
 * Returns 0 on success, or error code
 */
int rsu_slot_program_factory_update_buf(int slot, void *buf, int size)
{
	return rsu_slot_program_buf(slot, buf, size);
}

/**
 * rsu_slot_program_buf_raw() - program a slot from raw buffer data
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to program a slot using raw data from a buffer,
 * the slot is not entered into CPB.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_program_buf_raw(int slot, void *buf, int size)
{
	int ret;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (rsu_cb_buf_init(buf, size)) {
		rsu_log(RSU_ERR, "Bad buf/size arguments\n");
		return -EARGS;
	}

	ret = rsu_cb_program_common(ll_intf, slot, rsu_cb_buf, 1);
	if (ret) {
		rsu_log(RSU_ERR, "fail to program raw data\n");
		return ret;
	}

	rsu_cb_buf_exit();
	return ret;
}

/**
 * rsu_slot_verify_buf() - verify FPGA config data in a slot
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to verify FPGA configuration data in a selected
 * slot against the provided buffer.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_verify_buf(int slot, void *buf, int size)
{
	int ret;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (rsu_cb_buf_init(buf, size)) {
		rsu_log(RSU_ERR, "Bad buf/size arguments\n");
		return -EARGS;
	}

	ret = rsu_cb_verify_common(ll_intf, slot, rsu_cb_buf, 0);
	if (ret) {
		rsu_log(RSU_ERR, "fail to verify buffer data\n");
		return ret;
	}

	rsu_cb_buf_exit();

	return ret;
}

/**
 * rsu_slot_verify_buf_raw() - verify raw data in a slot
 * @slot: slot number
 * @buf: pointer to data buffer
 * @size: bytes to read from buffer, in hex value
 *
 * This function is used to verify raw data in a selected slot against
 * the provided buffer.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_verify_buf_raw(int slot, void *buf, int size)
{
	int ret;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (rsu_cb_buf_init(buf, size)) {
		rsu_log(RSU_ERR, "Bad buf/size arguments\n");
		return -EARGS;
	}

	ret = rsu_cb_verify_common(ll_intf, slot, rsu_cb_buf, 1);
	if (ret) {
		rsu_log(RSU_ERR, "fail to verify raw data\n");
		return ret;
	}

	rsu_cb_buf_exit();
	return ret;
}

/**
 * rsu_slot_enable() - enable the selected slot
 * @slot: slot number
 *
 * Set the selected slot as the highest priority. It will be the first
 * slot to be tried after a power-on reset.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_enable(int slot)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	if (ll_intf->priority.add(part_num))
		return -ELOWLEVEL;

	return 0;
}

/**
 * rsu_slot_disable() - disable the selected slot
 * @slot: slot number
 *
 * Remove the selected slot from the priority scheme, but don't erase the
 * slot data so that it can be re-enabled.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_disable(int slot)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (ll_intf->priority.remove(part_num))
		return -ELOWLEVEL;

	return 0;
}

/**
 * rsu_slot_load() - load the selected slot
 * @slot: slot number
 *
 * This function is used to request the selected slot to be loaded
 * immediately. On success the system is rebooted after a short delay.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_load(int slot)
{
	int part_num;
	u64 offset;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	offset = ll_intf->partition.offset(part_num);

	return ll_intf->fw_ops.load(offset);
}

/**
 * rsu_slot_load_factory() - load the factory image
 *
 * This function is used to request the factory image to be loaded
 * immediately. On success, the system is rebooted after a short delay.
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_load_factory(void)
{
	int part_num;
	int partitions;
	u64 offset;
	char name[] = "FACTORY_IMAGE";

	if (!ll_intf)
		return -EINTF;

	partitions = ll_intf->partition.count();
	for (part_num = 0; part_num < partitions; part_num++) {
		if (!strcmp(name, ll_intf->partition.name(part_num)))
			break;
	}

	if (part_num >= partitions) {
		rsu_log(RSU_ERR, "No FACTORY_IMAGE partition defined\n");
		return -EFORMAT;
	}

	offset = ll_intf->partition.offset(part_num);
	return ll_intf->fw_ops.load(offset);
}

/**
 * rsu_slot_rename() - Rename the selected slot.
 * @slot: slot number
 * @name: new name for slot
 *
 * Returns: 0 on success, or error code
 */
int rsu_slot_rename(int slot, char *name)
{
	int part_num;

	if (!ll_intf)
		return -EINTF;

	if (slot < 0 || slot >= rsu_slot_count()) {
		rsu_log(RSU_ERR, "invalid slot number\n");
		return -ESLOTNUM;
	}

	if (!name)
		return -EARGS;

	part_num = rsu_misc_slot2part(ll_intf, slot);
	if (part_num < 0)
		return -ESLOTNUM;

	if (rsu_misc_is_rsvd_name(name)) {
		rsu_log(RSU_ERR, "Partition rename uses a reserved name\n");
		return -ENAME;
	}

	if (ll_intf->partition.rename(part_num, name))
		return -ENAME;

	return 0;
}

/**
 * rsu_status_log() - Copy firmware status log to info struct
 * @info: pointer to info struct to fill in
 *
 * Return 0 on success, or error code
 */
int rsu_status_log(struct rsu_status_info *info)
{
	if (!ll_intf)
		return -EINTF;

	return ll_intf->fw_ops.status(info);
}

/**
 * rsu_notify() - report HPS software execution stage as a 16bit number
 * @stage: software execution stage
 *
 * Returns: 0 on success, or error code
 */
int rsu_notify(int stage)
{
	u32 arg;

	if (!ll_intf)
		return -EINTF;

	arg = stage & GENMASK(15, 0);
	return ll_intf->fw_ops.notify(arg);
}

/**
 * rsu_clear_error_status() - clear errors from the current RSU status log
 *
 * Returns: 0 on success, or error code
 */
int rsu_clear_error_status(void)
{
	struct rsu_status_info info;
	u32 arg;
	int ret;

	if (!ll_intf)
		return -EINTF;

	ret = rsu_status_log(&info);
	if (ret < 0)
		return ret;

	if (!RSU_VERSION_ACMF_VERSION(info.version))
		return -ELOWLEVEL;

	arg = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_CLEAR_ERROR_STATUS;
	return ll_intf->fw_ops.notify(arg);
}

/**
 * rsu_reset_retry_counter() - reset the retry counter
 *
 * This function is used to request the retry counter to be reset, so that the
 * currently running image may be tried again after the next watchdog timeout.
 *
 * Returns: 0 on success, or error code
 */
int rsu_reset_retry_counter(void)
{
	struct rsu_status_info info;
	u32 arg;
	int ret;

	if (!ll_intf)
		return -EINTF;

	ret = rsu_status_log(&info);
	if (ret < 0)
		return ret;

	if (!RSU_VERSION_ACMF_VERSION(info.version) ||
	    !RSU_VERSION_DCMF_VERSION(info.version))
		return -ELOWLEVEL;

	arg = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_RESET_RETRY_COUNTER;
	return ll_intf->fw_ops.notify(arg);
}

/**
 * rsu_dcmf_version() - retrieve the decision firmware version
 * @versions: pointer to where the four DCMF versions will be stored
 *
 * This function is used to retrieve the version of each of the four DCMF copies
 * in flash and also report the values to the SMC handler.
 *
 * Returns: 0 on success, or error code
 */
int rsu_dcmf_version(u32 *versions)
{
	int ret;

	if (!ll_intf)
		return -EINTF;

	if (!versions)
		return -EARGS;

	ret = ll_intf->fw_ops.dcmf_version(versions);
	if (ret)
		return ret;

#if !defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_ATF)
	return -EINTF;
#else
	return smc_store_dcmf_version(versions);
#endif
}
