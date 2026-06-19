// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * Sandbox wrapper that #includes the production S10/Agilex RSU command
 * handlers (rsu_spt_cpb_list / rsu_update / rsu_dtb) so the sandbox
 * exercises the real source - not a re-implementation - per the
 * uboot-test-harness rule "the harness MUST #include the actual
 * production .c, not re-implement it".
 *
 * On real hardware these handlers are compiled into arch/arm/mach-
 * socfpga/ (Makefile gates them on CONFIG_CADENCE_QSPI). On sandbox
 * the sibling rsu_ll_sandbox.c provides -EOPNOTSUPP stubs for the SDM
 * mailbox calls (mbox_rsu_status/get_spt_offset/update) so the handlers
 * compile and exit on the "no firmware" path; the DM SPI-flash stack
 * comes from CONFIG_SPI_FLASH_SANDBOX.
 */

#include "../../arch/arm/mach-socfpga/rsu_s10.c"
