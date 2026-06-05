// SPDX-License-Identifier: GPL-2.0+
/*
 * Cross-arch wrapper that includes the SoC FPGA RSU misc helpers.
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * arch/arm/mach-socfpga/rsu_misc.c is fully generic - it only pulls
 * in <asm/arch/rsu*.h> (which are forwarders to <socfpga_rsu*.h>) and
 * <asm/types.h>. By including the production source verbatim we get
 * rsu_log(), rsu_misc_*(), swap_bits(), rsu_pow() etc. on the sandbox
 * binary without maintaining a separate copy.
 *
 * Built only when CONFIG_SOCFPGA_RSU_CORE is selected (currently only
 * on SANDBOX); the arm Makefile still owns the canonical compile.
 */

#include "../../arch/arm/mach-socfpga/rsu_misc.c"
