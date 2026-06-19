/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * RSU subset of the SoC FPGA SDM mailbox API.
 *
 * The full ARM-side mailbox header (<asm/arch/mailbox_s10.h>) declares
 * the entire SDM protocol; including it from sandbox would drag in
 * hardware registers, SMC stubs, and PSCI variants the sandbox cannot
 * link. This header carries only the three calls the cross-arch
 * handlers in arch/arm/mach-socfpga/rsu_s10.c actually need.
 *
 * On the ARM build the implementations live in
 * arch/arm/mach-socfpga/mailbox_s10.c (and are also re-declared in the
 * full header for backwards compatibility). On sandbox they are
 * provided by drivers/misc/rsu_ll_sandbox.c as -EOPNOTSUPP stubs - the
 * RSU SDM mailbox does not exist on a Linux host.
 */
#ifndef _SOCFPGA_MAILBOX_RSU_H_
#define _SOCFPGA_MAILBOX_RSU_H_

#include <linux/types.h>

int mbox_rsu_get_spt_offset(u32 *resp_buf, u32 resp_buf_len);
int mbox_rsu_status(u32 *resp_buf, u32 resp_buf_len);
int mbox_rsu_update(u32 *flash_offset);

#endif /* _SOCFPGA_MAILBOX_RSU_H_ */
