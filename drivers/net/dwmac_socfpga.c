// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Marek Vasut <marex@denx.de>
 *
 * Altera SoCFPGA EMAC extras
 */

#include <asm/arch/secure_reg_helper.h>
#include <clk.h>
#include <dm.h>
#include <fdt_support.h>
#include <phy.h>
#include <regmap.h>
#include <reset.h>
#include <syscon.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/secure_reg_helper.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include "designware.h"
#include "phy/intel_tse_pcs.h"

#define ENABLE_BRIDGE 1
#define ALL_BRIDGE_RELEASE_FROM_RESET_MASK ~0

struct dwmac_socfpga_plat {
	struct dw_eth_pdata	dw_eth_pdata;
	void			*phy_intf;
	u32			reg_shift;
	struct tse_pcs		pcs;
};

static void socfpga_tse_pcs_adjust_link(struct udevice *dev,
					struct phy_device *phydev)
{
	struct dwmac_socfpga_plat *pdata = dev_get_plat(dev);
	phys_addr_t tse_pcs_base = pdata->pcs.tse_pcs_base;
	phys_addr_t sgmii_adapter_base = pdata->pcs.sgmii_adapter_base;

	if (tse_pcs_base && sgmii_adapter_base)
		writew(SGMII_ADAPTER_DISABLE,
		       sgmii_adapter_base + SGMII_ADAPTER_CTRL_REG);

	if (tse_pcs_base && sgmii_adapter_base)
		tse_pcs_fix_mac_speed(&pdata->pcs, phydev, phydev->speed);
}

static int socfpga_dw_tse_pcs_init(struct udevice *dev)
{
	struct dwmac_socfpga_plat *pdata = dev_get_plat(dev);
	struct dw_eth_pdata *dw_pdata = &pdata->dw_eth_pdata;
	struct eth_pdata *eth_pdata = &pdata->dw_eth_pdata.eth_pdata;
	phys_addr_t tse_pcs_base = pdata->pcs.tse_pcs_base;
	phys_addr_t sgmii_adapter_base = pdata->pcs.sgmii_adapter_base;
	const fdt32_t *reg;
	int ret = 0;
	int parent, index, na, ns, offset, len;

	offset = fdtdec_lookup_phandle(gd->fdt_blob, dev_of_offset(dev),
				       "altr,gmii-to-sgmii-converter");
	if (offset > 0) {
		/* enable all HPS bridge */
		do_bridge_reset(ENABLE_BRIDGE, ALL_BRIDGE_RELEASE_FROM_RESET_MASK);

		parent = fdt_parent_offset(gd->fdt_blob, offset);
		if (parent < 0) {
			debug("s10-hps-bridge DT not found\n");
			return -ENODEV;
		}

		na = fdt_address_cells(gd->fdt_blob, parent);
		if (na < 1) {
			debug("bad #address-cells\n");
			return -EINVAL;
		}

		ns = fdt_size_cells(gd->fdt_blob, parent);
		if (ns < 1) {
			debug("bad #size-cells\n");
			return -EINVAL;
		}

		index = fdt_stringlist_search(gd->fdt_blob, offset, "reg-names",
					      "eth_tse_control_port");
		if (index < 0) {
			debug("fail to find eth_tse_control_port: %s\n",
			      fdt_strerror(index));
			return -ENOENT;
		}

		reg = fdt_getprop(gd->fdt_blob, offset, "reg", &len);
		if (!reg || (len <= (index * sizeof(fdt32_t) * (na + ns))))
			return -EINVAL;

		reg += index * (na + ns);
		tse_pcs_base = fdt_translate_address((void *)gd->fdt_blob,
						     offset, reg);
		if (tse_pcs_base == FDT_ADDR_T_NONE) {
			debug("tse pcs address not found\n");
			return -EINVAL;
		}

		index = fdt_stringlist_search(gd->fdt_blob, offset, "reg-names",
					      "gmii_to_sgmii_adapter_avalon_slave");
		if (index < 0) {
			debug("fail to find gmii_to_sgmii_adapter_avalon_slave: %s\n",
			      fdt_strerror(index));
			return -EINVAL;
		}

		reg = fdt_getprop(gd->fdt_blob, offset, "reg", &len);
		if (!reg || (len <= (index * sizeof(fdt32_t) * (na + ns))))
			return -EINVAL;

		reg += index * (na + ns);
		sgmii_adapter_base = fdt_translate_address((void *)gd->fdt_blob,
							   offset, reg);
		if (sgmii_adapter_base == FDT_ADDR_T_NONE) {
			debug("gmii-to-sgmii adapter address not found\n");
			return -EINVAL;
		}

		if (eth_pdata->phy_interface == PHY_INTERFACE_MODE_SGMII) {
			if (tse_pcs_init(tse_pcs_base, &pdata->pcs) != 0) {
				debug("Unable to intiialize TSE PCS\n");
				return -EINVAL;
			}
		}

		/* Clear sgmii_adapter_base first */
		writel(0, sgmii_adapter_base);

		pdata->pcs.tse_pcs_base = tse_pcs_base;
		pdata->pcs.sgmii_adapter_base = sgmii_adapter_base;
		dw_pdata->pcs_adjust_link = socfpga_tse_pcs_adjust_link;
	}
	return ret;
}

static int dwmac_socfpga_ofdata_to_platdata(struct udevice *dev)
{
	struct dwmac_socfpga_plat *pdata = dev_get_plat(dev);
	struct regmap *regmap;
	struct ofnode_phandle_args args;
	void *range;
	int ret;

	ret = dev_read_phandle_with_args(dev, "altr,sysmgr-syscon", NULL,
					 2, 0, &args);
	if (ret) {
		dev_err(dev, "Failed to get syscon: %d\n", ret);
		return ret;
	}

	if (args.args_count != 2) {
		dev_err(dev, "Invalid number of syscon args\n");
		return -EINVAL;
	}

	regmap = syscon_node_to_regmap(args.node);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to get regmap: %d\n", ret);
		return ret;
	}

	range = regmap_get_range(regmap, 0);
	if (!range) {
		dev_err(dev, "Failed to get regmap range\n");
		return -ENOMEM;
	}

	pdata->phy_intf = range + args.args[0];
	pdata->reg_shift = args.args[1];

	return designware_eth_of_to_plat(dev);
}

static int dwmac_socfpga_do_setphy(struct udevice *dev, u32 modereg)
{
	struct dwmac_socfpga_plat *pdata = dev_get_plat(dev);
	u32 modemask = SYSMGR_EMACGRP_CTRL_PHYSEL_MASK << pdata->reg_shift;

#if !defined(CONFIG_XPL_BUILD) && defined(CONFIG_SPL_ATF)
	u32 index = ((u64)pdata->phy_intf - socfpga_get_sysmgr_addr() -
		     SYSMGR_SOC64_EMAC0) >> 2;

	u32 id = SOCFPGA_SECURE_REG_SYSMGR_SOC64_EMAC0 + index;

	int ret = socfpga_secure_reg_update32(id,
					     modemask,
					     modereg << pdata->reg_shift);
	if (ret) {
		dev_err(dev, "Failed to set PHY register via SMC call\n");
		return ret;
	}
#else
	clrsetbits_le32(pdata->phy_intf, modemask,
			modereg << pdata->reg_shift);
#endif

	return 0;
}

static int dwmac_socfpga_probe(struct udevice *dev)
{
	struct dwmac_socfpga_plat *pdata = dev_get_plat(dev);
	struct eth_pdata *edata = &pdata->dw_eth_pdata.eth_pdata;
	struct reset_ctl_bulk reset_bulk;
	int ret;
	u32 modereg;

	switch (edata->phy_interface) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
		break;
	case PHY_INTERFACE_MODE_SGMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII;
		break;
	default:
		dev_err(dev, "Unsupported PHY mode\n");
		return -EINVAL;
	}

	ret = reset_get_bulk(dev, &reset_bulk);
	if (ret) {
		dev_err(dev, "Failed to get reset: %d\n", ret);
		return ret;
	}

	reset_assert_bulk(&reset_bulk);

	ret = dwmac_socfpga_do_setphy(dev, modereg);
	if (ret)
		return ret;

	reset_release_bulk(&reset_bulk);

	socfpga_dw_tse_pcs_init(dev);

	return designware_eth_probe(dev);
}

static const struct udevice_id dwmac_socfpga_ids[] = {
	{ .compatible = "altr,socfpga-stmmac" },
	{ }
};

U_BOOT_DRIVER(dwmac_socfpga) = {
	.name		= "dwmac_socfpga",
	.id		= UCLASS_ETH,
	.of_match	= dwmac_socfpga_ids,
	.of_to_plat = dwmac_socfpga_ofdata_to_platdata,
	.probe		= dwmac_socfpga_probe,
	.ops		= &designware_eth_ops,
	.priv_auto	= sizeof(struct dw_eth_dev),
	.plat_auto	= sizeof(struct dwmac_socfpga_plat),
	.flags		= DM_FLAG_ALLOC_PRIV_DMA,
};
