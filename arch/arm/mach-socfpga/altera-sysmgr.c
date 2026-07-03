// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 */

/*
 * This driver supports the SOCFPGA System Manager Register block which
 * aggregates different peripheral function into one area.
 *
 * On 64 bit ARM parts, the system manager only can be accessed during
 * EL3 mode. At lower exception level a SMC call is required to perform
 * the read and write operation.
 *
 * The driver registers under UCLASS_SYSCON so that it can be discovered by
 * generic syscon consumers (for example, the EMAC drivers via
 * "altr,sysmgr-syscon" phandles). When CONFIG_SYSCON is enabled, the
 * syscon uclass automatically builds a regmap from the device's "reg"
 * property; consumers that call syscon_node_to_regmap() therefore work
 * transparently. This driver itself only needs the base address and so
 * reads it directly via dev_read_addr_ptr() to remain functional in SPL
 * builds where the regmap framework may not be available.
 */

#define LOG_CATEGORY UCLASS_SYSCON

#include <dm.h>
#include <log.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/altera-sysmgr.h>
#include <asm/arch/smc_api.h>
#include <asm/arch/system_manager.h>
#include <dm/device_compat.h>
#include <dm/read.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/intel-smc.h>

/*
 * Privileged sysmgr access policy on SoCFPGA SoC64
 * -------------------------------------------------
 * SPL always boots and runs at EL3, so direct MMIO is always safe in SPL.
 * After SPL hands control off (U-Boot proper at EL2 with CONFIG_SPL_ATF)
 * some System Manager registers are only reachable through BL31. Each op
 * therefore tries the SMC first and, only if BL31 rejects the register,
 * falls back to direct MMIO:
 *
 *   1. current_el() == 3      -> direct MMIO (SPL / no-ATF proper).
 *   2. INTEL_SIP_SMC_REG_* ok -> use the BL31 result.
 *   3. BL31 rejected          -> fall back to direct MMIO.
 */

/*
 * Offset-based ops. The @offset argument is relative to the device's own
 * register base (priv->regs); the implementation computes the absolute
 * address and dispatches per the access policy documented above.
 */
static int altr_sysmgr_read_offset(struct udevice *dev, u32 offset, u32 *value)
{
	struct altr_sysmgr_priv *priv = dev_get_priv(dev);
	void __iomem *addr = priv->regs + offset;
	u64 args[1];
	u64 ret_arg;
	int ret = 0;

	if (current_el() == 3) {
		*value = readl(addr);
		return 0;
	}

	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		args[0] = (u64)(uintptr_t)addr;
		ret = invoke_smc(INTEL_SIP_SMC_REG_READ, args, 1, &ret_arg, 1);
		if (!ret) {
			*value = (u32)ret_arg;
			return 0;
		}
		/* BL31 rejected the register: fall back to direct MMIO. */
		*value = readl(addr);
		return 0;
	}

	*value = readl(addr);
	return 0;
}

static int altr_sysmgr_write_offset(struct udevice *dev, u32 offset, u32 value)
{
	struct altr_sysmgr_priv *priv = dev_get_priv(dev);
	void __iomem *addr = priv->regs + offset;
	u64 args[2];
	int ret;

	if (current_el() == 3) {
		writel(value, addr);
		return 0;
	}

	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		args[0] = (u64)(uintptr_t)addr;
		args[1] = value;
		ret = invoke_smc(INTEL_SIP_SMC_REG_WRITE, args, 2, NULL, 0);
		if (!ret)
			return 0;
		/* BL31 rejected the register: fall back to direct MMIO. */
		writel(value, addr);
		return 0;
	}

	writel(value, addr);
	return 0;
}

static int altr_sysmgr_update_offset(struct udevice *dev, u32 offset,
				     u32 mask, u32 value)
{
	struct altr_sysmgr_priv *priv = dev_get_priv(dev);
	void __iomem *addr = priv->regs + offset;
	u64 args[3];
	int ret;

	if (current_el() == 3) {
		clrsetbits_le32(addr, mask, value & mask);
		return 0;
	}

	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		args[0] = (u64)(uintptr_t)addr;
		args[1] = mask;
		args[2] = value & mask;
		ret = invoke_smc(INTEL_SIP_SMC_REG_UPDATE, args, 3, NULL, 0);
		if (!ret)
			return 0;
		/* BL31 rejected the register: fall back to direct MMIO. */
		clrsetbits_le32(addr, mask, value & mask);
		return 0;
	}

	clrsetbits_le32(addr, mask, value & mask);
	return 0;
}

static int altr_sysmgr_probe(struct udevice *dev)
{
	struct altr_sysmgr_priv *altr_priv = dev_get_priv(dev);

	debug("%s: %s(dev=%p):\n", __func__, dev->name, dev);

	/*
	 * Read the base address directly from the "reg" property. Using
	 * dev_read_addr_ptr() avoids a hard dependency on the regmap
	 * framework, which is desirable because SPL on SoCFPGA SoC64
	 * platforms uses direct MMIO via the inline sysmgr_*() helpers
	 * and does not necessarily enable CONFIG_SPL_REGMAP.
	 *
	 * Consumers that need a regmap (for example EMAC drivers that
	 * call syscon_node_to_regmap()) still get one transparently:
	 * the syscon uclass pre_probe path builds a regmap from the
	 * same "reg" property when CONFIG_SYSCON is enabled.
	 */
	altr_priv->regs = dev_read_addr_ptr(dev);
	if (!altr_priv->regs) {
		pr_err("%s: failed to read base address\n", dev->name);
		return -EINVAL;
	}

	return 0;
}

static const struct altr_sysmgr_ops sysmgr_ops = {
	.read_offset	= altr_sysmgr_read_offset,
	.write_offset	= altr_sysmgr_write_offset,
	.update_offset	= altr_sysmgr_update_offset,
};

static const struct udevice_id altr_sysmgr_ids[] = {
	{ .compatible = "altr,sys-mgr-s10" },
	{ .compatible = "altr,sys-mgr" },
	{ },
};

U_BOOT_DRIVER(altr_sysmgr) = {
	.name	= "altr_sysmgr",
	.id	= UCLASS_SYSCON,
	.of_match = altr_sysmgr_ids,
	.probe	= altr_sysmgr_probe,
	.ops	= &sysmgr_ops,
	.priv_auto = sizeof(struct altr_sysmgr_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
