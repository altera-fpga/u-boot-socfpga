// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <net.h>
#include <phy.h>
#include <timer.h>
#include <wait_bit.h>
#include <asm/io.h>
#include <asm/omap_common.h>
#include <asm/omap_musb.h>
#include <linux/compat.h>
#include <linux/ethtool.h>
#include "intel_tse_pcs.h"

static int tse_pcs_reset(phys_addr_t base, struct tse_pcs *pcs)
{
	int ret;
	u16 val;
	phys_addr_t reg = base + TSE_PCS_CONTROL_REG;

	val = readw(reg);
	val |= TSE_PCS_SW_RST_MASK;

	writew(val, reg);

	ret = wait_for_bit_le16(&reg, TSE_PCS_SW_RST_MASK, false,
				TSE_PCS_SW_RESET_TIMEOUT, false);

	if (ret) {
		debug("%s: PCS could not get out of sw reset\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

int tse_pcs_init(phys_addr_t base, struct tse_pcs *pcs)
{
	int ret = 0;

	writew(TSE_PCS_USE_SGMII_ENA, base + TSE_PCS_IF_MODE_REG);
	writew(TSE_PCS_SGMII_LINK_TIMER_0, base + TSE_PCS_LINK_TIMER_0_REG);
	writew(TSE_PCS_SGMII_LINK_TIMER_1, base + TSE_PCS_LINK_TIMER_1_REG);

	ret = tse_pcs_reset(base, pcs);
	if (!ret)
		writew(SGMII_ADAPTER_ENABLE,
		       pcs->sgmii_adapter_base + SGMII_ADAPTER_CTRL_REG);

	return ret;
}

static void pcs_link_timer_callback(struct tse_pcs *pcs)
{
	u16 val = 0;
	phys_addr_t tse_pcs_base = pcs->tse_pcs_base;
	phys_addr_t sgmii_adapter_base = pcs->sgmii_adapter_base;

	val = readw(tse_pcs_base + TSE_PCS_STATUS_REG);
	val &= TSE_PCS_STATUS_LINK_MASK;

	if (!val) {
		printf("Adapter: Link fail to establish\n");
	} else {
		printf("Adapter: Link is established\n");
		writew(SGMII_ADAPTER_ENABLE,
		       sgmii_adapter_base + SGMII_ADAPTER_CTRL_REG);

	}
}

static void auto_nego_timer_callback(struct tse_pcs *pcs)
{
	u16 val = 0;
	u16 speed = 0;
	u16 duplex = 0;
	phys_addr_t tse_pcs_base = pcs->tse_pcs_base;
	phys_addr_t sgmii_adapter_base = pcs->sgmii_adapter_base;

	val = readw(tse_pcs_base + TSE_PCS_STATUS_REG);
	val &= TSE_PCS_STATUS_AN_COMPLETED_MASK;

	if (!val) {
		printf("Auto-negotioation failed\n");
	} else {
		printf("Adapter: Auto Negotiation is completed\n");
		val = readw(tse_pcs_base + TSE_PCS_PARTNER_ABILITY_REG);
		speed = val & TSE_PCS_PARTNER_SPEED_MASK;
		duplex = val & TSE_PCS_PARTNER_DUPLEX_MASK;

		if (duplex == TSE_PCS_PARTNER_DUPLEX_FULL) {
			if (speed == TSE_PCS_PARTNER_SPEED_10)
				printf("Adapter: Link Partner is Up - 10/Full\n");
			else if (speed == TSE_PCS_PARTNER_SPEED_100)
				printf("Adapter: Link Partner is Up - 100/Full\n");
			else if (speed == TSE_PCS_PARTNER_SPEED_1000)
				printf("Adapter: Link Partner is Up - 1000/Full\n");
		} else if (duplex == TSE_PCS_PARTNER_DUPLEX_HALF) {
			printf("Adapter does not support Half Duplex\n");
		} else {
			printf("Adapter: Invalid Partner Speed and Duplex\n");
		}

		if (duplex == TSE_PCS_PARTNER_DUPLEX_FULL &&
		    (speed == TSE_PCS_PARTNER_SPEED_10 ||
		     speed == TSE_PCS_PARTNER_SPEED_100 ||
		     speed == TSE_PCS_PARTNER_SPEED_1000))
			writew(SGMII_ADAPTER_ENABLE,
			       sgmii_adapter_base + SGMII_ADAPTER_CTRL_REG);
	}
}

void tse_pcs_fix_mac_speed(struct tse_pcs *pcs, struct phy_device *phy_dev,
			   unsigned int speed)
{
	phys_addr_t tse_pcs_base = pcs->tse_pcs_base;
	phys_addr_t sgmii_adapter_base = pcs->sgmii_adapter_base;
	u32 val, start;

	writew(SGMII_ADAPTER_ENABLE,
	       sgmii_adapter_base + SGMII_ADAPTER_CTRL_REG);
	pcs->autoneg = phy_dev->autoneg;

	if (pcs->autoneg == AUTONEG_ENABLE) {
		val = readw(tse_pcs_base + TSE_PCS_CONTROL_REG);
		val |= TSE_PCS_CONTROL_AN_EN_MASK;
		writew(val, tse_pcs_base + TSE_PCS_CONTROL_REG);

		val = readw(tse_pcs_base + TSE_PCS_IF_MODE_REG);
		val |= TSE_PCS_USE_SGMII_AN_MASK;
		writew(val, tse_pcs_base + TSE_PCS_IF_MODE_REG);

		val = readw(tse_pcs_base + TSE_PCS_CONTROL_REG);
		val |= TSE_PCS_CONTROL_RESTART_AN_MASK;

		tse_pcs_reset(tse_pcs_base, pcs);

	} else if (pcs->autoneg == AUTONEG_DISABLE) {
		val = readw(tse_pcs_base + TSE_PCS_CONTROL_REG);
		val &= ~TSE_PCS_CONTROL_AN_EN_MASK;
		writew(val, tse_pcs_base + TSE_PCS_CONTROL_REG);

		val = readw(tse_pcs_base + TSE_PCS_IF_MODE_REG);
		val &= ~TSE_PCS_USE_SGMII_AN_MASK;
		writew(val, tse_pcs_base + TSE_PCS_IF_MODE_REG);

		val = readw(tse_pcs_base + TSE_PCS_IF_MODE_REG);
		val &= ~TSE_PCS_SGMII_SPEED_MASK;

		switch (speed) {
		case 1000:
			val |= TSE_PCS_SGMII_SPEED_1000;
			break;
		case 100:
			val |= TSE_PCS_SGMII_SPEED_100;
			break;
		case 10:
			val |= TSE_PCS_SGMII_SPEED_10;
			break;
		default:
			return;
		}
		writew(val, tse_pcs_base + TSE_PCS_IF_MODE_REG);

		tse_pcs_reset(tse_pcs_base, pcs);
	}

	start = get_timer(0);
	while (1) {
		if (get_timer(start) >= AUTONEGO_LINK_TIMER) {
			if (pcs->autoneg == AUTONEG_ENABLE)
				auto_nego_timer_callback(pcs);
			else if (pcs->autoneg == AUTONEG_DISABLE)
				pcs_link_timer_callback(pcs);
			return;
		}
		udelay(100);
	}
}
