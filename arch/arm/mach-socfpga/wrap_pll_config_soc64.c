// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 *
 */

#include <asm/arch/clock_manager.h>
#include <asm/io.h>
#include <asm/arch/handoff_soc64.h>
#include <asm/arch/system_manager.h>
#include <linux/printk.h>

const struct cm_config * const cm_get_default_config(void)
{
#ifdef CONFIG_XPL_BUILD
	struct cm_config *cm_handoff_cfg = (struct cm_config *)
		(SOC64_HANDOFF_CLOCK + SOC64_HANDOFF_OFFSET_DATA);
	u32 *conversion = (u32 *)cm_handoff_cfg;
	u32 i;
	u32 handoff_clk = readl(SOC64_HANDOFF_CLOCK);

	pr_info("%s : line %d => Handoff section : [%c%c%c%c] at 0x%08lx\n\n",
		__FILE__, __LINE__, (handoff_clk >> 0) & 0xff,
		(handoff_clk >> 8) & 0xff, (handoff_clk >> 16) & 0xff,
		(handoff_clk >> 24) & 0xff, (uintptr_t)SOC64_HANDOFF_CLOCK);

	pr_info("%s: Handoff data =\n{\n", __func__);
	for (i = 0; i < (sizeof(*cm_handoff_cfg) / sizeof(u32)); i++)
		pr_info(" 0x%08x -> 0x%08x\n", i, readl(conversion + i));

	pr_info("}\n");

	if (swab32(handoff_clk) == SOC64_HANDOFF_MAGIC_CLOCK) {
		writel(swab32(handoff_clk), SOC64_HANDOFF_CLOCK);
		for (i = 0; i < (sizeof(*cm_handoff_cfg) / sizeof(u32)); i++)
			conversion[i] = swab32(conversion[i]);
		return cm_handoff_cfg;
	} else if (handoff_clk == SOC64_HANDOFF_MAGIC_CLOCK) {
		return cm_handoff_cfg;
	}
#endif
	return NULL;
}

const unsigned int cm_get_osc_clk_hz(void)
{
	u32 cached = 0;

#ifdef CONFIG_XPL_BUILD
	u32 clock = readl(SOC64_HANDOFF_CLOCK_OSC);

	if (sysmgr_hs_write(SYSMGR_SOC64_BOOT_SCRATCH_COLD1, clock))
		pr_warn("%s: sysmgr COLD1 write failed; osc clock not cached\n",
			__func__);
#endif
	if (sysmgr_hs_read(SYSMGR_SOC64_BOOT_SCRATCH_COLD1, &cached))
		pr_warn("%s: sysmgr COLD1 read failed; reporting 0 Hz\n",
			__func__);
	return cached;
}

const unsigned int cm_get_intosc_clk_hz(void)
{
	return CLKMGR_INTOSC_HZ;
}

const unsigned int cm_get_fpga_clk_hz(void)
{
	u32 cached = 0;

#ifdef CONFIG_XPL_BUILD
	u32 clock = readl(SOC64_HANDOFF_CLOCK_FPGA);

	if (sysmgr_hs_write(SYSMGR_SOC64_BOOT_SCRATCH_COLD2, clock))
		pr_warn("%s: sysmgr COLD2 write failed; FPGA clock not cached\n",
			__func__);
#endif
	if (sysmgr_hs_read(SYSMGR_SOC64_BOOT_SCRATCH_COLD2, &cached))
		pr_warn("%s: sysmgr COLD2 read failed; reporting 0 Hz\n",
			__func__);
	return cached;
}
