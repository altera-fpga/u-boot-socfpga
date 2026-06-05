// SPDX-License-Identifier: GPL-2.0+
/*
 * Cross-arch wrapper that includes the SoC FPGA RSU dispatcher.
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * The dispatcher in arch/arm/mach-socfpga/rsu.c is functionally
 * cross-arch except for three SMC-using helpers, which are guarded
 * with #if !defined(CONFIG_SANDBOX) at source level. Per the workspace
 * harness rule ("the harness MUST #include the actual production .c
 * file under test, NEVER re-implement production logic") we compile
 * the production source verbatim from this drivers/misc/ stub rather
 * than maintaining a sandbox-only re-implementation that could drift.
 *
 * On arm targets the kbuild rule that ships rsu.o lives in
 * arch/arm/mach-socfpga/Makefile and continues to compile the file
 * directly. This wrapper is built only when CONFIG_SOCFPGA_RSU_CORE
 * is selected - currently restricted to SANDBOX by Kconfig - so there
 * is no double-link risk.
 */

#include "../../arch/arm/mach-socfpga/rsu.c"
