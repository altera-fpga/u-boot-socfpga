/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024-2026 Altera Corporation <www.altera.com>
 *
 * RSU SPI NOR access: driver model (DM_SPI_FLASH) vs legacy
 * spi_flash_probe(). Sandbox is DM_SPI_FLASH (provided by
 * CONFIG_SPI_FLASH_SANDBOX) so no third branch is required.
 *
 * Header was previously arch/arm/mach-socfpga/include/mach/rsu_flash_if.h;
 * moved here so the cross-arch dispatcher / S10 handlers can compile in
 * the sandbox.
 */
#ifndef __SOCFPGA_RSU_FLASH_IF_H__
#define __SOCFPGA_RSU_FLASH_IF_H__

#include <linux/errno.h>
#include <linux/kconfig.h>
#include <linux/types.h>
#include <spi.h>
#include <spi_flash.h>

#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
#include <dm/device.h>
#include <linux/mtd/spi-nor.h>

static inline u32 rsu_mtd_size(struct udevice *dev)
{
	struct spi_nor *nor = dev_get_uclass_priv(dev);

	return nor ? nor->size : 0;
}

static inline int rsu_mtd_read(struct udevice *dev, u32 off, size_t len,
			       void *buf)
{
	return spi_flash_read_dm(dev, off, len, buf);
}

static inline int rsu_mtd_write(struct udevice *dev, u32 off, size_t len,
				const void *buf)
{
	return spi_flash_write_dm(dev, off, len, buf);
}

static inline int rsu_mtd_erase(struct udevice *dev, u32 off, size_t len)
{
	return spi_flash_erase_dm(dev, off, len);
}

static inline int rsu_mtd_probe(unsigned int bus, unsigned int cs,
				struct udevice **devp)
{
	struct udevice *spi_bus;
	struct spi_slave *slave;
	int ret;

	/*
	 * spi_flash_probe_bus_cs() does not apply CONFIG_SF_DEFAULT_SPEED
	 * or CONFIG_SF_DEFAULT_MODE, so the bus stays at DT
	 * spi-max-frequency. _spi_get_bus_and_cs() sets both.
	 */
	ret = _spi_get_bus_and_cs(bus, cs, CONFIG_SF_DEFAULT_SPEED,
				  CONFIG_SF_DEFAULT_MODE, "jedec_spi_nor",
				  "spi_flash", &spi_bus, &slave);
	if (ret)
		return ret;

	*devp = slave->dev;
	return 0;
}

static inline void rsu_mtd_unclaim(struct udevice *dev)
{
	(void)dev;
}

#else

static inline u32 rsu_mtd_size(struct spi_flash *flash)
{
	return flash->size;
}

static inline int rsu_mtd_read(struct spi_flash *flash, u32 off, size_t len,
			       void *buf)
{
	return spi_flash_read(flash, off, len, buf);
}

static inline int rsu_mtd_write(struct spi_flash *flash, u32 off, size_t len,
				const void *buf)
{
	return spi_flash_write(flash, off, len, buf);
}

static inline int rsu_mtd_erase(struct spi_flash *flash, u32 off, size_t len)
{
	return spi_flash_erase(flash, off, len);
}

static inline int rsu_mtd_probe(unsigned int bus, unsigned int cs,
				struct spi_flash **flashp)
{
	struct spi_flash *f;

	f = spi_flash_probe(bus, cs, CONFIG_SF_DEFAULT_SPEED,
			    CONFIG_SF_DEFAULT_MODE);
	if (!f)
		return -ENODEV;
	*flashp = f;
	return 0;
}

static inline void rsu_mtd_unclaim(struct spi_flash *flash)
{
	spi_flash_free(flash);
}

#endif

#endif /* __SOCFPGA_RSU_FLASH_IF_H__ */
