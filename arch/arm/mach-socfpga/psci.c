/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <asm/io.h>
#include <asm/psci.h>
#include <errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/secure.h>

void __noreturn __secure psci_system_reset(void)
{
	mbox_send_cmd_psci(MBOX_ID_UBOOT, MBOX_REBOOT_HPS,
			   MBOX_CMD_DIRECT, 0, NULL, 0, 0, NULL);

	while (1)
		;
}
