/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 */

#ifndef _MACH_SOCFPGA_ALTERA_SYSMGR_H_
#define _MACH_SOCFPGA_ALTERA_SYSMGR_H_

#include <linux/types.h>

struct udevice;

/**
 * struct altr_sysmgr_ops - offset-based ops for the Altera System Manager
 * @read_offset:   Read the 32-bit register at byte @offset within the
 *                 device's register block. Stores the value in @value.
 *                 Returns 0 on success, negative errno on failure.
 * @write_offset:  Write @value to the 32-bit register at byte @offset
 *                 within the device's register block. Returns 0 on
 *                 success, negative errno on failure.
 * @update_offset: Read-modify-write the 32-bit register at byte @offset:
 *                 clear the bits set in @mask, then OR in (@value & @mask).
 *                 Returns 0 on success, negative errno on failure.
 *
 * All offsets are byte offsets relative to the device's own register
 * base (priv->regs). The implementation dispatches between direct MMIO
 * at EL3 and INTEL_SIP_SMC_REG_* at lower exception levels internally.
 * SMC-based paths return -EPROTONOSUPPORT when invoked at a lower EL
 * without ATF available.
 */
struct altr_sysmgr_ops {
	int (*read_offset)(struct udevice *dev, u32 offset, u32 *value);
	int (*write_offset)(struct udevice *dev, u32 offset, u32 value);
	int (*update_offset)(struct udevice *dev, u32 offset, u32 mask,
			     u32 value);
};

/**
 * struct altr_sysmgr_priv - per-device private state for altera-sysmgr
 * @regs: MMIO base of this System Manager region, as mapped from the
 *        device's "reg" property.
 */
struct altr_sysmgr_priv {
	void __iomem *regs;
};

/**
 * altr_sysmgr_get_ops() - retrieve the altr_sysmgr_ops table for @dev.
 * @dev: System Manager udevice (must be bound to altera-sysmgr).
 */
#define altr_sysmgr_get_ops(dev) \
	((struct altr_sysmgr_ops *)(dev)->driver->ops)

/**
 * altr_sysmgr_get_priv() - retrieve the altr_sysmgr_priv block for @dev.
 * @dev: System Manager udevice (must be bound to altera-sysmgr).
 */
#define altr_sysmgr_get_priv(dev) \
	((struct altr_sysmgr_priv *)(dev_get_priv(dev)))

#endif /* _MACH_SOCFPGA_ALTERA_SYSMGR_H_ */
