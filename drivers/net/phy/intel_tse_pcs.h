// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 */

#ifndef __TSE_PCS_H__
#define __TSE_PCS_H__

#define TSE_PCS_CONTROL_AN_EN_MASK			BIT(12)
#define TSE_PCS_CONTROL_REG				0x00
#define TSE_PCS_CONTROL_RESTART_AN_MASK			BIT(9)
#define TSE_PCS_IF_MODE_REG				0x28
#define TSE_PCS_LINK_TIMER_0_REG			0x24
#define TSE_PCS_LINK_TIMER_1_REG			0x26
#define TSE_PCS_SIZE					0x40
#define TSE_PCS_STATUS_AN_COMPLETED_MASK		BIT(5)
#define TSE_PCS_STATUS_LINK_MASK			BIT(2)
#define TSE_PCS_STATUS_REG				0x02
#define TSE_PCS_SGMII_SPEED_1000			BIT(3)
#define TSE_PCS_SGMII_SPEED_100				BIT(2)
#define TSE_PCS_SGMII_SPEED_10				0x0
#define TSE_PCS_SGMII_SPEED_MASK			GENMASK(3, 2)
#define TSE_PCS_SW_RST_MASK				BIT(15)
#define TSE_PCS_PARTNER_ABILITY_REG			0x0A
#define TSE_PCS_PARTNER_DUPLEX_FULL			BIT(12)
#define TSE_PCS_PARTNER_DUPLEX_HALF			0x0000
#define TSE_PCS_PARTNER_DUPLEX_MASK			BIT(12)
#define TSE_PCS_PARTNER_SPEED_MASK			GENMASK(11, 10)
#define TSE_PCS_PARTNER_SPEED_1000			BIT(11)
#define TSE_PCS_PARTNER_SPEED_100			BIT(10)
#define TSE_PCS_PARTNER_SPEED_10			0x0000
#define TSE_PCS_SGMII_LINK_TIMER_0			0x0D40
#define TSE_PCS_SGMII_LINK_TIMER_1			0x0003
#define TSE_PCS_SW_RESET_TIMEOUT			100
#define TSE_PCS_USE_SGMII_AN_MASK			BIT(1)
#define TSE_PCS_USE_SGMII_ENA				BIT(0)

#define SGMII_ADAPTER_CTRL_REG				0x00
#define SGMII_ADAPTER_DISABLE				0x0001
#define SGMII_ADAPTER_ENABLE				0x0000

#define AUTONEGO_LINK_TIMER				20

struct tse_pcs {
	struct device *dev;
	phys_addr_t tse_pcs_base;
	phys_addr_t sgmii_adapter_base;
	struct timer_list aneg_link_timer;
	int autoneg;
};

int tse_pcs_init(phys_addr_t base, struct tse_pcs *pcs);
void tse_pcs_fix_mac_speed(struct tse_pcs *pcs, struct phy_device *phy_dev,
			   unsigned int speed);

#endif /* __TSE_PCS_H__ */
