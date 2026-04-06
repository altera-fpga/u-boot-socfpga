// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for Altera SoC FPGA rsu command (usage path).
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <command.h>
#include <test/cmd.h>
#include <test/ut.h>

static int cmd_ut_socfpga_rsu_usage(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_USAGE, run_command("rsu", 0));

	return 0;
}

CMD_TEST(cmd_ut_socfpga_rsu_usage, 0);
