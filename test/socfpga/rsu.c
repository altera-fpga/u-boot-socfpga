// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for Altera SoC FPGA rsu command (usage path).
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 *
 * Suite-local tests are registered with SOCFPGA_TEST(...) and surface
 * under the 'socfpga' suite as 'rsu_<what>' (the suite prefix
 * 'socfpga_test_' is stripped by the runner; see
 * test/cmd_ut.c:run_suite and test/test-main.c:test_matches).
 */

#include <command.h>
#include <test/socfpga.h>
#include <test/ut.h>

/*
 * run_command() collapses every non-zero command return (including
 * CMD_RET_USAGE = -1 and CMD_RET_FAILURE = 1) to 1 before handing
 * control back to its caller, so a "run_command() returned 1" assert
 * cannot on its own distinguish "the parser rejected argv up front"
 * (CMD_RET_USAGE) from "the backend was touched and failed later"
 * (CMD_RET_FAILURE). To pin down the CMD_RET_USAGE branch each test
 * below pairs the ==1 assert with ut_assert_skip_to_linen("rsu - "),
 * which scans the captured console for the first line of U-Boot's
 * cmd_usage() banner. That banner is only emitted on the
 * CMD_RET_USAGE path, so its presence proves the parser fired before
 * any RSU state was touched. UTF_CONSOLE on each SOCFPGA_TEST enables
 * the console capture the helper inspects.
 */
static int socfpga_test_rsu_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rsu", 0));
	ut_assert_skip_to_linen("rsu - ");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_usage, UTF_CONSOLE);

/*
 * Malformed numeric arguments (non-digit characters, overflow, trailing junk)
 * must be rejected by the subcommand handlers with CMD_RET_USAGE *before* any
 * RSU state is touched. This exercises the rsu_parse_num() / rsu_parse_slot()
 * /rsu_parse_hex_* helpers without requiring a working RSU backend, because
 * parsing is performed up-front and the rsu_init() call is never reached on
 * the error path. The skip_to_linen("rsu - ") asserts after each run_command
 * pin the failure to the CMD_RET_USAGE branch (see comment above).
 */
static int socfpga_test_rsu_bad_slot(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rsu slot_get_info foo", 0));
	ut_assert_skip_to_linen("rsu - ");
	ut_asserteq(1, run_command("rsu slot_get_info 12xyz", 0));
	ut_assert_skip_to_linen("rsu - ");
	ut_asserteq(1, run_command("rsu slot_get_info 99999999999", 0));
	ut_assert_skip_to_linen("rsu - ");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_bad_slot, UTF_CONSOLE);

static int socfpga_test_rsu_bad_size(struct unit_test_state *uts)
{
	/*
	 * slot_program_buf takes <slot> <buffer> <size>; an INT_MAX+1 size
	 * must be rejected with CMD_RET_USAGE (would otherwise be silently
	 * cast to a negative int, which is how buffer overflow bugs start).
	 */
	ut_asserteq(1, run_command("rsu slot_program_buf 0 0x1000 80000000", 0));
	ut_assert_skip_to_linen("rsu - ");
	/* Garbage in the size field. */
	ut_asserteq(1, run_command("rsu slot_program_buf 0 0x1000 qqq", 0));
	ut_assert_skip_to_linen("rsu - ");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_bad_size, UTF_CONSOLE);
