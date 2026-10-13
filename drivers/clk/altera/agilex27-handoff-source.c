// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <errno.h>
#include <hang.h>
#include <log.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <asm/arch/agilex27-handoff.h>

#include "agilex27-clkmgr.h"

int agilex27_handoff_get_blob(const void **blob_out, size_t *size_out)
{
	if (!blob_out || !size_out)
		return -EINVAL;

	if (IS_ENABLED(CONFIG_AGILEX27_CLKMGR_HANDOFF_EMBED_DEMO)) {
		*blob_out = agilex27_handoff_blob;
		*size_out = agilex27_handoff_blob_size;
		pr_debug("agilex27-handoff: source = embedded demo blob at %p, %u bytes\n",
			 agilex27_handoff_blob, agilex27_handoff_blob_size);
		return 0;
	}

	return -ENOSYS;
}

void clk_mgr_init_from_blob(void)
{
	const void *blob;
	size_t size;
	int ret;

	pr_debug("agilex27-clkmgr: handoff start\n");
	if (IS_ENABLED(CONFIG_AGILEX27_CLKMGR_HANDOFF_EMBED_DEMO)) {
		pr_info("agilex27-clkmgr: applying demo handoff (V9, SYSPRESET0 bin1)\n");
		pr_info("agilex27-clkmgr: GPPLL presets from DV; silicon validation pending\n");
	}

#if !IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)
	agilex27_clkmgr_secure_unlock();
	pr_debug("agilex27-clkmgr: stage1 unlock done\n");
#endif

	ret = agilex27_handoff_get_blob(&blob, &size);
	if (ret) {
		pr_err("agilex27-handoff: no blob source available (%d)\n", ret);
		hang();
	}

	ret = agilex27_handoff_parse_and_apply(blob, size);
	if (ret) {
		pr_err("agilex27-handoff: blob parse/apply failed: %d\n", ret);
		hang();
	}

	if (IS_ENABLED(CONFIG_AGILEX27_CLKMGR_HANDOFF_EMBED_DEMO))
		pr_info("agilex27-clkmgr: demo handoff applied (V9 SYSPRESET0 bin1)\n");
}
