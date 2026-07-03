// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 * U-Boot proper implementations of the unified System Manager access
 * helpers declared in <asm/arch/system_manager.h>. SPL uses the static
 * inline fast paths from the same header; this TU is built only for
 * U-Boot proper.
 *
 * Caller error-handling convention:
 *   - Config writes that gate hardware bring-up propagate the errno.
 *   - Boot-time query reads fail closed after pr_warn().
 *   - Debug-only reads log with debug() and continue.
 */

#define LOG_CATEGORY UCLASS_SYSCON

#include <dm.h>
#include <log.h>
#include <asm/arch/altera-sysmgr.h>
#include <asm/arch/system_manager.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <linux/errno.h>

/*
 * Resolve a sysmgr instance by alias. AGILEX72 has three ("sysmgr-{ls,hs,apu}"),
 * other SoC64s have one ("sysmgr"). Lookup walks DT every call to avoid
 * a function-static cache lifecycle invariant. Driver identity is
 * verified because UCLASS_SYSCON is generic.
 */
static struct udevice *sysmgr_get_dev_by_alias(const char *alias)
{
	struct udevice *dev;
	ofnode node;
	int ret;

	node = ofnode_get_aliases_node(alias);
	if (!ofnode_valid(node)) {
		debug("%s: '%s' alias not present in DT\n", __func__, alias);
		return NULL;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_SYSCON, node, &dev);
	if (ret || !dev) {
		debug("%s: '%s' alias did not resolve to a UCLASS_SYSCON device (ret %d)\n",
		      __func__, alias, ret);
		return NULL;
	}

	if (dev->driver != DM_DRIVER_GET(altr_sysmgr)) {
		debug("%s: '%s' alias resolved to %s, not altr_sysmgr\n",
		      __func__, alias, dev->driver ? dev->driver->name : "?");
		return NULL;
	}

	return dev;
}

int sysmgr_dev_write(struct udevice *dev, u32 offset, u32 value)
{
	struct altr_sysmgr_ops *ops;

	if (!dev)
		return -ENODEV;

	ops = altr_sysmgr_get_ops(dev);
	if (!ops || !ops->write_offset)
		return -ENOSYS;

	return ops->write_offset(dev, offset, value);
}

int sysmgr_dev_read(struct udevice *dev, u32 offset, u32 *value)
{
	struct altr_sysmgr_ops *ops;

	if (!dev)
		return -ENODEV;

	ops = altr_sysmgr_get_ops(dev);
	if (!ops || !ops->read_offset)
		return -ENOSYS;

	return ops->read_offset(dev, offset, value);
}

int sysmgr_dev_update(struct udevice *dev, u32 offset, u32 mask, u32 value)
{
	struct altr_sysmgr_ops *ops;

	if (!dev)
		return -ENODEV;

	ops = altr_sysmgr_get_ops(dev);
	if (!ops || !ops->update_offset)
		return -ENOSYS;

	return ops->update_offset(dev, offset, mask, value);
}

/*
 * Per-region resolver. AGILEX72 has per-region aliases; on single-instance
 * SoCs we transparently fall back to "sysmgr" so callers can use
 * sysmgr_{ls,hs,apu}_*() uniformly. Per-register offsets differ per
 * platform via system_manager_soc64*.h, so the fallback never
 * misroutes.
 */
static struct udevice *sysmgr_get_dev_or_default(const char *alias)
{
	struct udevice *dev = sysmgr_get_dev_by_alias(alias);

	if (!dev)
		dev = sysmgr_get_dev_by_alias("sysmgr");

	return dev;
}

/* Trampolines through sysmgr_dev_*() for the alias-keyed variants. */
int sysmgr_write(u32 offset, u32 value)
{
	return sysmgr_dev_write(sysmgr_get_dev_by_alias("sysmgr"),
				offset, value);
}

int sysmgr_read(u32 offset, u32 *value)
{
	return sysmgr_dev_read(sysmgr_get_dev_by_alias("sysmgr"),
			       offset, value);
}

int sysmgr_update(u32 offset, u32 mask, u32 value)
{
	return sysmgr_dev_update(sysmgr_get_dev_by_alias("sysmgr"),
				 offset, mask, value);
}

#define _SYSMGR_REGION_API(_region, _alias)				\
int sysmgr_##_region##_write(u32 offset, u32 value)			\
{									\
	return sysmgr_dev_write(sysmgr_get_dev_or_default(_alias),	\
				offset, value);				\
}									\
int sysmgr_##_region##_read(u32 offset, u32 *value)			\
{									\
	return sysmgr_dev_read(sysmgr_get_dev_or_default(_alias),	\
			       offset, value);				\
}									\
int sysmgr_##_region##_update(u32 offset, u32 mask, u32 value)		\
{									\
	return sysmgr_dev_update(sysmgr_get_dev_or_default(_alias),	\
				 offset, mask, value);			\
}

_SYSMGR_REGION_API(ls,  "sysmgr-ls")
_SYSMGR_REGION_API(hs,  "sysmgr-hs")
_SYSMGR_REGION_API(apu, "sysmgr-apu")

#undef _SYSMGR_REGION_API
