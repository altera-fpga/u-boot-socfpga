// SPDX-License-Identifier: GPL-2.0+
/*
 * Sandbox low-level RSU backend.
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * On real hardware rsu_init() (arch/arm/mach-socfpga/rsu.c) calls
 * rsu_ll_qspi_init(), which probes the Cadence QSPI controller, loads
 * the SPT and CPB from flash, and wires them to a struct rsu_ll_intf.
 * None of that infrastructure exists on the sandbox, so this file
 * provides a RAM-backed implementation of the same symbol that:
 *
 *   - Reports an empty SPT (0 partitions) and an empty CPB. That is
 *     enough to make rsu_init() succeed, rsu_slot_count() return 0,
 *     and the dispatcher's `if (corrupted())` guards pass cleanly.
 *   - Returns -ENOSYS for write-side callbacks.
 *
 * No per-session state is allocated. struct rsu_ll_intf's callback
 * signatures do not pass an `intf` argument (see arch/arm/mach-
 * socfpga/include/mach/rsu_ll.h replacement at include/socfpga_rsu_ll.h),
 * so a `priv` pointer would be a writer-without-readers per the
 * socfpga-rsu-architecture rule. The production arm backend in
 * arch/arm/mach-socfpga/rsu_ll_qspi.c works around the same limitation
 * via a file-scoped singleton; the sandbox stub has nothing to track,
 * so it skips the slot entirely.
 *
 * A real RAM-backed SPT/CPB mock - capable of exercising
 * partition_create(), load_spt(), check_cpb(), and the u64 overflow
 * guards in rsu_ll_qspi.c (partition_create() integer-overflow check,
 * read/write/erase bounds checks) - should replace these write-path
 * stubs, not coexist with them. That mock is also the right place to
 * introduce an honest per-session state container (either via a
 * file-scoped struct or by extending struct rsu_ll_intf with an `intf`
 * argument on every callback).
 */

#include <linux/errno.h>
#include <socfpga_rsu_ll.h>

static struct rsu_ll_intf sandbox_intf;

/*
 * Writable storage for sandbox_part_name(). The rsu_ll_intf::name
 * callback is typed `char *(*)(int)` because the production qspi
 * backend (arch/arm/mach-socfpga/rsu_ll_qspi.c) returns a pointer
 * into the on-flash SPT, which is writable. Returning a string
 * literal here would trip -Wwrite-strings and imply the caller may
 * write into read-only storage; widening the ABI to `const char *`
 * is a separate, broader change. The buffer is reachable only via
 * a hypothetical name() call against a non-existent slot - the
 * sandbox always reports partition.count == 0 - so it stays as a
 * one-byte empty placeholder.
 */
static char sandbox_empty_name[1] = "";

static int sandbox_part_count(void)
{
	return 0;
}

static char *sandbox_part_name(int part_num)
{
	(void)part_num;
	return sandbox_empty_name;
}

static u64 sandbox_part_offset(int part_num)
{
	(void)part_num;
	return 0;
}

static s64 sandbox_part_factory_offset(void)
{
	/* Same convention as arch/arm/mach-socfpga/rsu_ll_qspi.c:factory_offset(). */
	return -ENOENT;
}

static u32 sandbox_part_size(int part_num)
{
	(void)part_num;
	return 0;
}

static int sandbox_part_reserved(int part_num)
{
	(void)part_num;
	return 0;
}

static int sandbox_part_readonly(int part_num)
{
	(void)part_num;
	return 0;
}

static int sandbox_part_rename(int part_num, char *name)
{
	(void)part_num;
	(void)name;
	return -ENOSYS;
}

static int sandbox_part_delete(int part_num)
{
	(void)part_num;
	return -ENOSYS;
}

static int sandbox_part_create(char *name, u64 start, unsigned int size)
{
	(void)name;
	(void)start;
	(void)size;
	return -ENOSYS;
}

static int sandbox_prio_get(int part_num)
{
	(void)part_num;
	return 0;
}

static int sandbox_prio_add(int part_num)
{
	(void)part_num;
	return -ENOSYS;
}

static int sandbox_prio_remove(int part_num)
{
	(void)part_num;
	return -ENOSYS;
}

static int sandbox_data_read(int part_num, int offset, int bytes, void *buf)
{
	(void)part_num;
	(void)offset;
	(void)bytes;
	(void)buf;
	return -ENOSYS;
}

static int sandbox_data_write(int part_num, int offset, int bytes, void *buf)
{
	(void)part_num;
	(void)offset;
	(void)bytes;
	(void)buf;
	return -ENOSYS;
}

static int sandbox_data_erase(int part_num)
{
	(void)part_num;
	return -ENOSYS;
}

static int sandbox_fw_load(u64 offset)
{
	(void)offset;
	return -ENOSYS;
}

static int sandbox_fw_status(struct rsu_status_info *info)
{
	if (!info)
		return -EINVAL;

	info->current_image = 0;
	info->fail_image = 0;
	info->state = 0;
	info->version = 0;
	info->error_location = 0;
	info->error_details = 0;
	info->retry_counter = 0;
	return 0;
}

static int sandbox_fw_notify(u32 value)
{
	(void)value;
	return -ENOSYS;
}

static int sandbox_fw_dcmf_version(u32 *versions)
{
	int i;

	if (!versions)
		return -EINVAL;
	for (i = 0; i < 4; i++)
		versions[i] = 0;
	return 0;
}

static int sandbox_fw_dcmf_status(u16 *status)
{
	int i;

	if (!status)
		return -EINVAL;
	for (i = 0; i < 4; i++)
		status[i] = 0;
	return 0;
}

static int sandbox_fw_max_retry(u8 *value)
{
	if (!value)
		return -EINVAL;
	*value = 0;
	return 0;
}

static int sandbox_cpb_empty(void)
{
	return 0;
}

static int sandbox_cpb_restore(u64 address)
{
	(void)address;
	return -ENOSYS;
}

static int sandbox_cpb_save(u64 address)
{
	(void)address;
	return -ENOSYS;
}

static int sandbox_cpb_corrupted(void)
{
	return 0;
}

static int sandbox_spt_restore(u64 address)
{
	(void)address;
	return -ENOSYS;
}

static int sandbox_spt_save(u64 address)
{
	(void)address;
	return -ENOSYS;
}

static int sandbox_spt_corrupted(void)
{
	return 0;
}

int rsu_ll_qspi_init(struct rsu_ll_intf **intf)
{
	if (!intf)
		return -EINVAL;

	/*
	 * sandbox_intf.priv intentionally left NULL: see file header.
	 * sandbox_intf.exit intentionally left NULL: dispatcher's
	 * rsu_exit() null-checks the callback (see arch/arm/mach-
	 * socfpga/rsu.c:rsu_exit), and the sandbox has no per-session
	 * allocations to release.
	 */
	sandbox_intf.partition.count = sandbox_part_count;
	sandbox_intf.partition.name = sandbox_part_name;
	sandbox_intf.partition.offset = sandbox_part_offset;
	sandbox_intf.partition.factory_offset = sandbox_part_factory_offset;
	sandbox_intf.partition.size = sandbox_part_size;
	sandbox_intf.partition.reserved = sandbox_part_reserved;
	sandbox_intf.partition.readonly = sandbox_part_readonly;
	sandbox_intf.partition.rename = sandbox_part_rename;
	sandbox_intf.partition.delete = sandbox_part_delete;
	sandbox_intf.partition.create = sandbox_part_create;

	sandbox_intf.priority.get = sandbox_prio_get;
	sandbox_intf.priority.add = sandbox_prio_add;
	sandbox_intf.priority.remove = sandbox_prio_remove;

	sandbox_intf.data.read = sandbox_data_read;
	sandbox_intf.data.write = sandbox_data_write;
	sandbox_intf.data.erase = sandbox_data_erase;

	sandbox_intf.fw_ops.load = sandbox_fw_load;
	sandbox_intf.fw_ops.status = sandbox_fw_status;
	sandbox_intf.fw_ops.notify = sandbox_fw_notify;
	sandbox_intf.fw_ops.dcmf_version = sandbox_fw_dcmf_version;
	sandbox_intf.fw_ops.dcmf_status = sandbox_fw_dcmf_status;
	sandbox_intf.fw_ops.max_retry = sandbox_fw_max_retry;

	sandbox_intf.cpb_ops.empty = sandbox_cpb_empty;
	sandbox_intf.cpb_ops.restore = sandbox_cpb_restore;
	sandbox_intf.cpb_ops.save = sandbox_cpb_save;
	sandbox_intf.cpb_ops.corrupted = sandbox_cpb_corrupted;

	sandbox_intf.spt_ops.restore = sandbox_spt_restore;
	sandbox_intf.spt_ops.save = sandbox_spt_save;
	sandbox_intf.spt_ops.corrupted = sandbox_spt_corrupted;

	*intf = &sandbox_intf;
	return 0;
}

/*
 * Subcommand backends below are declared in <rsu_console.h> and
 * implemented for real hardware by arch/arm/mach-socfpga/rsu_s10.c.
 * They are NOT part of struct rsu_ll_intf - the cmd dispatcher in
 * cmd/socfpga_rsu.c routes 'rsu update', 'rsu dtb' and 'rsu
 * spt_cpb_list' to these helpers directly. The sandbox build needs
 * equivalents so cmd/socfpga_rsu.c links. They return CMD_RET_FAILURE
 * because the wrapper has already validated argc/argv - the failure
 * is "no SDM mailbox on this host", not "user typo".
 */
int rsu_spt_cpb_list(int argc, char * const argv[])
{
	(void)argc;
	(void)argv;
	return CMD_RET_FAILURE;
}

int rsu_update(int argc, char * const argv[])
{
	(void)argc;
	(void)argv;
	return CMD_RET_FAILURE;
}

int rsu_dtb(int argc, char * const argv[])
{
	(void)argc;
	(void)argv;
	return CMD_RET_FAILURE;
}
