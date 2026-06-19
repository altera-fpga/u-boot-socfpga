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
#include <socfpga_rsu.h>
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

/*
 * Backend-routing smoke tests below require CONFIG_SOCFPGA_RSU_CORE,
 * i.e. the cross-arch dispatcher in arch/arm/mach-socfpga/rsu.c
 * compiled into the sandbox binary via drivers/misc/socfpga_rsu_core.c,
 * plus the RAM-backed empty-SPT backend in drivers/misc/rsu_ll_sandbox.c.
 * They exercise the rsu_init() -> rsu_ll_qspi_init() -> dispatcher path
 * that the parsing tests above intentionally avoid.
 */
#if IS_ENABLED(CONFIG_SOCFPGA_RSU_CORE)

/*
 * `rsu slot_count` must succeed on the sandbox backend and report 0
 * partitions. A non-zero count would indicate the backend is leaking
 * state across rsu_init()/rsu_exit() cycles, or that the dispatcher
 * misroutes partition.count().
 */
static int socfpga_test_rsu_slot_count_zero(struct unit_test_state *uts)
{
	ut_assertok(run_command("rsu slot_count", 0));
	ut_assert_nextlinen("Number of slots = 0");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_slot_count_zero, UTF_CONSOLE);

/*
 * Direct dispatcher invocation: rsu_slot_count() must return 0 (no
 * slots) without the cmd plumbing. This catches dispatcher regressions
 * that the cmd path would mask by collapsing every non-zero return.
 */
static int socfpga_test_rsu_init_exits_clean(struct unit_test_state *uts)
{
	ut_asserteq(0, rsu_init(NULL));
	ut_asserteq(0, rsu_slot_count());
	rsu_exit();
	/* Second init must succeed too - dispatcher self-heals stale state. */
	ut_asserteq(0, rsu_init(NULL));
	ut_asserteq(0, rsu_slot_count());
	rsu_exit();

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_init_exits_clean, 0);

/*
 * Slot lookup by name on an empty SPT must report -ENAME, NOT -EINTF
 * (which would indicate rsu_init() failed) and NOT -EARGS (which
 * would indicate a name validation bug). This locks in the
 * "init succeeded, table empty" semantics.
 */
static int socfpga_test_rsu_slot_by_name_empty(struct unit_test_state *uts)
{
	char name[] = "does_not_exist";

	ut_asserteq(0, rsu_init(NULL));
	ut_asserteq(-ENAME, rsu_slot_by_name(name));
	rsu_exit();

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_slot_by_name_empty, 0);

#endif /* CONFIG_SOCFPGA_RSU_CORE */

/*
 * Handler-routing smoke tests below require the production S10/Agilex
 * command handlers (arch/arm/mach-socfpga/rsu_s10.c) compiled into the
 * sandbox via drivers/misc/socfpga_rsu_s10_handlers.c. The sandbox LL
 * backend stubs the SDM mailbox calls to -EOPNOTSUPP, so each test
 * verifies that the *production* handler reaches that stub and reports
 * the expected error message - not that the cmd dispatcher's parser
 * intercepted the call early. A future refactor that breaks the
 * handler wiring will print a different error or crash, which is
 * exactly the regression we want to catch.
 */
#if IS_ENABLED(CONFIG_SOCFPGA_RSU_S10_HANDLERS)

static int socfpga_test_rsu_list_no_firmware(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rsu list", 0));
	ut_assert_nextlinen("RSU: Firmware or flash content not supporting RSU");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_list_no_firmware, UTF_CONSOLE);

static int socfpga_test_rsu_update_no_firmware(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rsu update 0xdeadbeef", 0));
	ut_assert_nextlinen("RSU: RSU update to 0x00000000deadbeef");
	ut_assert_nextlinen("RSU: mbox_rsu_update failed");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_update_no_firmware, UTF_CONSOLE);

/*
 * rsu_update has its own argv parser (rsu_parse_hex_u64 in rsu_s10.c)
 * separate from the cmd-dispatcher's rsu_parse_num. Trailing junk and
 * a missing argument must both be rejected before mbox_rsu_update is
 * ever called - i.e. the sandbox stub must not print its error banner.
 *
 * Per the top-of-file note: run_command() collapses CMD_RET_USAGE and
 * CMD_RET_FAILURE to the same return value, so we pair each run with
 * ut_assert_skip_to_linen("rsu - ") to prove the parser rejected argv
 * up front (CMD_RET_USAGE prints the cmd_usage() banner) rather than
 * the sandbox mailbox stub firing later and returning -EOPNOTSUPP.
 * UTF_CONSOLE on SOCFPGA_TEST enables the console capture.
 */
static int socfpga_test_rsu_update_bad_arg(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("rsu update", 0));
	ut_assert_skip_to_linen("rsu - ");

	ut_asserteq(1, run_command("rsu update 12xyz", 0));
	ut_assert_skip_to_linen("rsu - ");

	return 0;
}

SOCFPGA_TEST(socfpga_test_rsu_update_bad_arg, UTF_CONSOLE);

#endif /* CONFIG_SOCFPGA_RSU_S10_HANDLERS */
