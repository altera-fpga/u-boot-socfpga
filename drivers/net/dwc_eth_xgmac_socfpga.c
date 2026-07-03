// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Intel Corporation
 */
#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/arch/system_manager.h>
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <dm/device_compat.h>
#include "dwc_eth_xgmac.h"

#define SOCFPGA_XGMAC_SYSCON_ARG_COUNT 2

phy_interface_t dwxgmac_of_get_mac_mode(struct udevice *dev)
{
	const char *mac_mode;
	int i;

	debug("%s(dev=%p):\n", __func__, dev);
	mac_mode = dev_read_string(dev, "mac-mode");
	if (!mac_mode)
		return PHY_INTERFACE_MODE_NA;

	for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++) {
		if (!strcmp(mac_mode, phy_interface_strings[i]))
			return i;
	}
	return PHY_INTERFACE_MODE_NA;
}

static int dwxgmac_socfpga_do_setphy(struct udevice *dev, u32 modereg)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	u32 modemask = SYSMGR_EMACGRP_CTRL_PHYSEL_MASK <<
		       xgmac->syscon_phy_regshift;
	int ret;

	/*
	 * Dispatch through sysmgr_dev_update() so the write targets the
	 * exact System Manager instance referenced by the XGMAC's
	 * "altr,sysmgr-syscon" phandle. See the matching comment in
	 * dwmac_socfpga_do_setphy() for the rationale; in short, this
	 * collapses the SPL direct-MMIO and U-Boot proper SMC paths into
	 * a single call site and lets the multi-instance AGILEX72 topology work
	 * with the same 2-cell phandle binding the upstream community
	 * already uses.
	 */
	ret = sysmgr_dev_update(xgmac->sysmgr, xgmac->syscon_phy_offset,
				modemask,
				modereg << xgmac->syscon_phy_regshift);
	if (ret) {
		dev_err(dev, "Failed to set PHY register: %d\n", ret);
		return ret;
	}

	return 0;
}

static int xgmac_probe_resources_socfpga(struct udevice *dev)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	struct ofnode_phandle_args args;
	phy_interface_t interface;
	phy_interface_t mac_mode;
	int ret;
	u32 modereg;

	interface = xgmac->config->interface(dev);
	mac_mode = dwxgmac_of_get_mac_mode(dev);

	if (mac_mode == PHY_INTERFACE_MODE_NA)
		mac_mode = interface;

	switch (mac_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII;
		break;
	default:
		dev_err(dev, "Unsupported PHY mode\n");
		return -EINVAL;
	}

	/* Get PHY syscon */
	ret = dev_read_phandle_with_args(dev, "altr,sysmgr-syscon", NULL,
					 SOCFPGA_XGMAC_SYSCON_ARG_COUNT,
					 0, &args);

	if (ret) {
		dev_err(dev, "Failed to get syscon: %d\n", ret);
		return ret;
	}

	if (args.args_count != SOCFPGA_XGMAC_SYSCON_ARG_COUNT) {
		dev_err(dev, "Invalid number of syscon args\n");
		return -EINVAL;
	}

	xgmac->syscon_phy_offset = args.args[0];
	xgmac->syscon_phy_regshift = args.args[1];

	/*
	 * Resolve the System Manager phandle target to a UCLASS_SYSCON
	 * device so PHY-mode writes dispatch through sysmgr_dev_update()
	 * against the exact instance referenced by DT. The driver does
	 * not assume a single-instance topology -- multi-instance AGILEX72
	 * works with the same code path.
	 */
	ret = uclass_get_device_by_ofnode(UCLASS_SYSCON, args.node,
					  &xgmac->sysmgr);
	if (ret) {
		dev_err(dev, "Failed to bind sysmgr device: %d\n", ret);
		return ret;
	}

	/* Get Reset Bulk */
	ret = reset_get_bulk(dev, &xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "Failed to get reset: %d\n", ret);
		return ret;
	}

	ret = reset_assert_bulk(&xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "XGMAC failed to assert reset: %d\n", ret);
		return ret;
	}

	ret = dwxgmac_socfpga_do_setphy(dev, modereg);
	if (ret)
		return ret;

	ret = reset_deassert_bulk(&xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "XGMAC failed to de-assert reset: %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "stmmaceth", &xgmac->clk_common);
	if (ret) {
		pr_err("clk_get_by_name(stmmaceth) failed: %d", ret);
		goto err_probe;
	}
	return 0;

err_probe:
	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int xgmac_get_enetaddr_socfpga(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	u32 hi_addr, lo_addr;

	debug("%s(dev=%p):\n", __func__, dev);

	/* Read the MAC Address from the hardawre */
	hi_addr	= readl(&xgmac->mac_regs->address0_high);
	lo_addr	= readl(&xgmac->mac_regs->address0_low);

	pdata->enetaddr[0] = lo_addr & 0xff;
	pdata->enetaddr[1] = (lo_addr >> 8) & 0xff;
	pdata->enetaddr[2] = (lo_addr >> 16) & 0xff;
	pdata->enetaddr[3] = (lo_addr >> 24) & 0xff;
	pdata->enetaddr[4] = hi_addr & 0xff;
	pdata->enetaddr[5] = (hi_addr >> 8) & 0xff;

	return !is_valid_ethaddr(pdata->enetaddr);
}

static int xgmac_start_resets_socfpga(struct udevice *dev)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = reset_assert_bulk(&xgmac->reset_bulk);
	if (ret < 0) {
		pr_err("xgmac reset assert failed: %d", ret);
		return ret;
	}

	udelay(2);

	ret = reset_deassert_bulk(&xgmac->reset_bulk);
	if (ret < 0) {
		pr_err("xgmac reset de-assert failed: %d", ret);
		return ret;
	}

	return 0;
}

static struct xgmac_ops xgmac_socfpga_ops = {
	.xgmac_inval_desc = xgmac_inval_desc_generic,
	.xgmac_flush_desc = xgmac_flush_desc_generic,
	.xgmac_inval_buffer = xgmac_inval_buffer_generic,
	.xgmac_flush_buffer = xgmac_flush_buffer_generic,
	.xgmac_probe_resources = xgmac_probe_resources_socfpga,
	.xgmac_remove_resources = xgmac_null_ops,
	.xgmac_stop_resets = xgmac_null_ops,
	.xgmac_start_resets = xgmac_start_resets_socfpga,
	.xgmac_stop_clks = xgmac_null_ops,
	.xgmac_start_clks = xgmac_null_ops,
	.xgmac_calibrate_pads = xgmac_null_ops,
	.xgmac_disable_calibration = xgmac_null_ops,
	.xgmac_get_enetaddr = xgmac_get_enetaddr_socfpga,
};

struct xgmac_config __maybe_unused xgmac_socfpga_config = {
	.reg_access_always_ok = false,
	.swr_wait = 50,
	.config_mac = XGMAC_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = XGMAC_MAC_MDIO_ADDRESS_CR_350_400,
	.axi_bus_width = XGMAC_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &xgmac_socfpga_ops
};
