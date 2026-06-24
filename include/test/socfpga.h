/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * SoC FPGA test suite registration macro.
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * The 'socfpga' suite is the umbrella for all sandbox unit tests
 * targeting Altera SoC FPGA drivers (RSU today; FPGA manager, SDM
 * mailbox, sysmgr, etc. as they get sandbox-ported). It mirrors the
 * existing per-subsystem suite macros (CMD_TEST, ENV_TEST, ...).
 *
 * Each test file under test/socfpga/<driver>.c registers its tests
 * with SOCFPGA_TEST(<name>, <flags>); the linker section
 * ll_entry(struct unit_test, ut_socfpga) makes them discoverable to
 * the 'ut socfpga' subcommand declared in test/cmd_ut.c.
 *
 * Test names should follow the U-Boot convention
 * 'socfpga_test_<driver>_<what>' so the suite's '<name>_test_'
 * prefix-strip (test/cmd_ut.c:run_suite) yields a clean per-driver
 * selector under 'ut socfpga <driver>_<what>'.
 */

#ifndef __TEST_SOCFPGA_H__
#define __TEST_SOCFPGA_H__

#include <test/test.h>

#define SOCFPGA_TEST(_name, _flags) UNIT_TEST(_name, _flags, socfpga)

#endif /* __TEST_SOCFPGA_H__ */
