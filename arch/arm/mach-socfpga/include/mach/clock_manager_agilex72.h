/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#ifndef _CLOCK_MANAGER_AGILEX72_
#define _CLOCK_MANAGER_AGILEX72_

#include <asm/arch/clock_manager_soc64.h>

#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

#define CM_REG_READL(plat, reg)				\
	readl((plat)->regs + (reg))

#define CM_REG_WRITEL(plat, data, reg)			\
	writel(data, (plat)->regs + (reg))

#define CM_REG_CLRBITS(plat, reg, clear)		\
	clrbits_le32((plat)->regs + (reg), (clear))

#define CM_REG_SETBITS(plat, reg, set)			\
	setbits_le32((plat)->regs + (reg), (set))

#define CLKMGR_CTRL				0x0
#define CLKMGR_STAT				0x4
#define CLKMGR_CTRL_BOOTMODE			BIT(0)
#define CLKMGR_STAT_BUSY			BIT(0)
#define CLKMGR_STAT_BOOTMODE			BIT(24)
#define CLKMGR_STAT_BOOTCLKSRC			BIT(25)
#define CLKMGR_CTRL_SWCTRLBTCLKSEL		BIT(9)

#ifndef __ASSEMBLY__
unsigned long cm_get_core2_clk_hz(void);
unsigned long cm_get_core3_clk_hz(void);
#endif

#endif /* _CLOCK_MANAGER_AGILEX72_ */
