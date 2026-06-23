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
#include <asm/arch/agilex72-handoff.h>

#include "agilex72-clkmgr.h"

int agilex72_handoff_get_blob(const void **blob_out, size_t *size_out)
{
	if (!blob_out || !size_out)
		return -EINVAL;

	if (IS_ENABLED(CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO)) {
		*blob_out = agilex72_handoff_blob;
		*size_out = agilex72_handoff_blob_size;
		pr_debug("agilex72-handoff: source = embedded demo blob at %p, %u bytes\n",
			 agilex72_handoff_blob, agilex72_handoff_blob_size);
		return 0;
	}

	return -ENOSYS;
}

void clk_mgr_init_from_blob(void)
{
	const void *blob;
	size_t size;
	int ret;

	pr_debug("agilex72-clkmgr: handoff start\n");
	if (IS_ENABLED(CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO)) {
		pr_info("agilex72-clkmgr: applying demo handoff (V9, SYSPRESET0 bin1)\n");
		pr_info("agilex72-clkmgr: GPPLL presets from DV; silicon validation pending\n");
	}

#if !IS_ENABLED(CONFIG_TARGET_SOCFPGA_EMU)
	agilex72_clkmgr_secure_unlock();
	pr_debug("agilex72-clkmgr: stage1 unlock done\n");
#endif

	ret = agilex72_handoff_get_blob(&blob, &size);
	if (ret) {
		pr_err("agilex72-handoff: no blob source available (%d)\n", ret);
		hang();
	}

	ret = agilex72_handoff_parse_and_apply(blob, size);
	if (ret) {
		pr_err("agilex72-handoff: blob parse/apply failed: %d\n", ret);
		hang();
	}

	if (IS_ENABLED(CONFIG_AGILEX72_CLKMGR_HANDOFF_EMBED_DEMO))
		pr_info("agilex72-clkmgr: demo handoff applied (V9 SYSPRESET0 bin1)\n");
}
