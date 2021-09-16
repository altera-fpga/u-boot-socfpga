// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Intel Corporation <www.intel.com>
 *
 */

#include <asm/arch/firewall.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <common.h>
#include <command.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <spi.h>
#include <spl.h>
#include <watchdog.h>

#ifdef CONFIG_SPL_BUILD

#define COPY_ENGINE_BASE		0xf9000000

#define HPS_SCRATCHPAD			0x150

#define HOST2HPS_IMG_XFR_SHDW		0x154
#define HOST2HPS_IMG_XFR_COMPLETE	BIT(0)

#define HPS2HOST_RSP			0x158
#define HPS2HOST_RSP_SSBL_VFY		GENMASK(1, 0)
#define HPS2HOST_RSP_KRNL_VFY		GENMASK(3, 2)
#define HPS2HOST_RSP_HPS_RDY		BIT(4)

void spl_board_init(void)
{
	u32 img_xfr;

	socfpga_bridges_reset(true, RSTMGR_BRGMODRST_SOC2FPGA_MASK |
				    RSTMGR_BRGMODRST_LWSOC2FPGA_MASK |
				    RSTMGR_BRGMODRST_FPGA2SOC_MASK |
				    RSTMGR_BRGMODRST_F2SDRAM0_MASK);

	printf("waiting for host to copy image\n");

	do {
		writel(HPS2HOST_RSP_HPS_RDY, COPY_ENGINE_BASE + HPS2HOST_RSP);

		WATCHDOG_RESET();

		img_xfr = readl(COPY_ENGINE_BASE + HOST2HPS_IMG_XFR_SHDW);
	} while (!(img_xfr & HOST2HPS_IMG_XFR_COMPLETE));

	writel(0, COPY_ENGINE_BASE + HPS2HOST_RSP);

	printf("image copied from host 0x%x\n", readl(COPY_ENGINE_BASE + HPS2HOST_RSP));
}
#else

#ifdef CONFIG_BOARD_LATE_INIT
#include <linux/delay.h>

#include "plldata.h"

#define PLL_SPI_BUS  1
#define PLL_SPI_CS   0
#define PLL_SPI_HZ   10e6
#define PLL_SPI_MODE 0
#define PLL_SPI_BITS 16

#define PLL_SPI_PAGE_REG           0x7f
#define PLL_SPI_FREQ_OFFSET_REG    0x0b
#define PLL_SPI_FREQ_OFFSET_CNT    4
#define PLL_SPI_PAGE(addr)         ((addr) >> 7)
#define PLL_SPI_VALUE(data)        ((data) & 0xff)
#define PLL_SPI_READ(addr)         (((addr) | 0x80) << 8)
#define PLL_SPI_WRITE(addr, value) ((((addr) & 0x7f) << 8) | (value))

#define PLL_FREQ_OFFSET_A0 0x180072B0
#define PLL_FREQ_OFFSET_A1 0x046AAAAB

enum socfpga_board {
	BOARD_UNKNOWN = 0,
	BOARD_A0,
	BOARD_A1,
};

static int zl_pll_page(struct spi_slave *slave, u16 addr, u8 *page)
{
	u8 p = PLL_SPI_PAGE(addr);
	u16 d = PLL_SPI_WRITE(PLL_SPI_PAGE_REG, p);

	if (page && p == *page)
		return 0;

	if (page)
		*page = p;

	debug("pll: page: %x\n", p);
	return spi_xfer(slave, PLL_SPI_BITS, &d, NULL, SPI_XFER_ONCE);
}

static int zl_pll_read_one(struct spi_slave *slave, u16 addr, u8 *val, u8 *page)
{
	int ret;
	u16 d;

	ret = zl_pll_page(slave, addr, page);
	if (ret)
		return ret;

	d = PLL_SPI_READ(addr);

	ret = spi_xfer(slave, PLL_SPI_BITS, &d, &d, SPI_XFER_ONCE);
	if (ret)
		return ret;

	*val = PLL_SPI_VALUE(d);
	debug("pll: 0x%x: %x\n", addr, *val);

	return 0;
}

static int zl_pll_write_one(struct spi_slave *slave, u16 addr, u8 val, u8 *page)
{
	int ret;
	u16 d;

	ret = zl_pll_page(slave, addr, page);
	if (ret)
		return ret;

	debug("pll: 0x%x: %x\n", addr, val);
	d = PLL_SPI_WRITE(addr, val);

	return spi_xfer(slave, PLL_SPI_BITS, &d, NULL, SPI_XFER_ONCE);
}

static int zl_pll_detect(struct spi_slave *slave)
{
	u8 page = 0;
	int i, ret;

	union {
		__be32 u;
		u8 b[4];
	} data;

	/* read 4 freq_offset bytes from PLL chip */
	for (i = 0; i < PLL_SPI_FREQ_OFFSET_CNT; i++) {
		u16 addr = PLL_SPI_FREQ_OFFSET_REG + i;

		ret = zl_pll_read_one(slave, addr, &data.b[i], &page);
		if (ret)
			return ret;
	}

	/*
	 * do nothing if PLL is already configured (i.e. if freq_offset doesn't
	 * match default reset value
	 */
	switch (be32_to_cpu(data.u)) {
	case PLL_FREQ_OFFSET_A0: return BOARD_A0;
	case PLL_FREQ_OFFSET_A1: return BOARD_A1;
	default:
		printf("PLL: unknown freq_offset (0x%x); skipping initialization\n",
		       be32_to_cpu(data.u));
		return BOARD_UNKNOWN;
	}
}

static int zl_pll_write(struct spi_slave *slave)
{
	PLLStructItem_t *item = PLLStructItemList;
	u8 page = 0;
	int ret, i;

	/* write embedded config register values to PLL */
	for (i = 0; i < PLLStructItems; i++, item++) {
		switch (item->cmd) {
		case 'X':
			ret = zl_pll_write_one(slave, item->addr, item->value, &page);
			if (ret != 0)
				return ret;
			break;
		case 'W':
			debug("pll: delay %u\n", item->addr);
			udelay(item->addr);
			break;
		}
	}

	printf("PLL:   ready\n");

	return 0;
}

static int zl_pll_init(void)
{
	struct spi_slave *slave;
	struct udevice *dev;
	const char *bd;
	int ret;

	ret = spi_get_bus_and_cs(PLL_SPI_BUS, PLL_SPI_CS, PLL_SPI_HZ,
				 PLL_SPI_MODE, "spi_generic_drv", "board_pll",
				 &dev, &slave);
	if (ret != 0)
		return ret;

	ret = spi_claim_bus(slave);
	if (ret)
		goto done;

	ret = zl_pll_detect(slave);
	if (ret < BOARD_UNKNOWN)
		goto done;

	switch (ret) {
	case BOARD_UNKNOWN:
		ret = -ENODATA;
		goto done;
	case BOARD_A0:
		bd = "A0";
		break;
	case BOARD_A1:
		bd = "A1";
		break;
	}

	printf("PLL:   detected %s; starting initialization\n", bd);

	ret = zl_pll_write(slave);

done:
	spi_release_bus(slave);

	return ret;
}

static int do_zl_read(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct spi_slave *slave;
	struct udevice *dev;
	u32 count = 1;
	int ret, i;
	u16 addr;
	u8 value;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = spi_get_bus_and_cs(PLL_SPI_BUS, PLL_SPI_CS, PLL_SPI_HZ,
				 PLL_SPI_MODE, "spi_generic_drv", "board_pll",
				 &dev, &slave);
	if (ret != 0)
		return CMD_RET_FAILURE;

	addr = simple_strtoul(argv[1], NULL, 16);

	if (argc == 3)
		count = simple_strtoul(argv[2], NULL, 10);

	for (i = 0; i < count; i++) {
		ret = zl_pll_read_one(slave, addr, &value, NULL);
		if (ret)
			break;

		printf("%02x: %02x\n", addr, value);
		addr++;
	}

	spi_release_bus(slave);

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int do_zl_write(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct spi_slave *slave;
	struct udevice *dev;
	u32 count = 1;
	u32 idx = 2;
	int ret, i;
	u16 addr;
	u8 value;

	if (argc < 3)
		return CMD_RET_USAGE;

	ret = spi_get_bus_and_cs(PLL_SPI_BUS, PLL_SPI_CS, PLL_SPI_HZ,
				 PLL_SPI_MODE, "spi_generic_drv", "board_pll",
				 &dev, &slave);
	if (ret != 0)
		return CMD_RET_FAILURE;

	addr = simple_strtoul(argv[1], NULL, 16);

	if (argc > 3) {
		idx = 3;
		count = simple_strtoul(argv[2], NULL, 10);
	}

	if (argc < (idx + count))
		return CMD_RET_USAGE;

	for (i = 0; i < count; i++) {
		value = simple_strtoul(argv[idx], NULL, 16);
		ret = zl_pll_write_one(slave, addr, value, NULL);
		if (ret)
			break;

		printf("%02x: %02x\n", addr, value);
		addr++;
		idx++;
	}

	spi_release_bus(slave);

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

#ifdef CONFIG_SYS_LONGHELP
static char zl_help_text[] =
	"read <address> [<count>] - read register(s) from address(es)\n"
	"write <address> [<count>] <value> [<value> ..] - "
	"write value(s) to auto-incremented address";
#endif

U_BOOT_CMD_WITH_SUBCMDS(zl, "read/write zarlink registers", zl_help_text,
			U_BOOT_SUBCMD_MKENT(read,  CONFIG_SYS_MAXARGS, 1, do_zl_read),
			U_BOOT_SUBCMD_MKENT(write, CONFIG_SYS_MAXARGS, 1, do_zl_write));

int board_late_init(void)
{
	struct udevice *dev;
	int ret = 0;

	ret = uclass_get_device(UCLASS_SPI, 0, &dev);
	if (ret)
		printf("SPI:   init failed: %d\n", ret);
	else
		printf("SPI:   initialized\n");

	ret = uclass_get_device(UCLASS_GPIO, 0, &dev);
	if (ret)
		printf("GPIO0: init failed: %d\n", ret);
	else
		printf("GPIO0: initialized\n");

	ret = uclass_get_device(UCLASS_GPIO, 1, &dev);
	if (ret)
		printf("GPIO1: init failed: %d\n", ret);
	else
		printf("GPIO1: initialized\n");

	ret = zl_pll_init();
	if (ret != 0)
		printf("PLL:   error: %i\n", ret);
	else
		printf("PLL:   configured\n");

	return 0;
}

static const char *gpio_list[][2] = {
	{ "lol-gpios", "lock" },
	{ "ho-gpios",  "holdover" },
};

static int do_pll(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[])
{
	ofnode node;
	int ret, i;

	node = ofnode_path("/pll-gpios");
	if (!ofnode_valid(node))
		return CMD_RET_FAILURE;

	for (i = 0; i < ARRAY_SIZE(gpio_list); i++) {
		const char *label = gpio_list[i][0];
		const char *name = gpio_list[i][1];
		struct gpio_desc gpio;

		ret = gpio_request_by_name_nodev(node, label, 0, &gpio, GPIOD_IS_IN);
		if (ret)
			return CMD_RET_FAILURE;

		printf("%s: %i\n", name, dm_gpio_get_value(&gpio));
		dm_gpio_free(NULL, &gpio);
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(pll, 1, 1, do_pll,
	   "query pll status",
	   "pll  - show PLL lock and holdover indicators");
#endif
#endif
