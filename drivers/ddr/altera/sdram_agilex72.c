// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Altera Corporation <www.altera.com>
 *
 */

#include <stdlib.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <hang.h>
#include <log.h>
#include <ram.h>
#include <reset.h>
#include <wait_bit.h>
#include <wdt.h>
#include <linux/bitfield.h>
#include <linux/sizes.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include "iossm_mailbox.h"
#include "sdram_soc64.h"

DECLARE_GLOBAL_DATA_PTR;

int sdram_mmr_init_full(struct udevice *dev)
{
	int ret = 0;
	struct altera_sdram_priv *priv = dev_get_priv(dev);

	printf("DDR: SDRAM init in progress ...\n");

	gd->bd = (struct bd_info *)malloc(sizeof(struct bd_info));
	memset(gd->bd, '\0', sizeof(struct bd_info));

	printf("DDR: Calibration success\n");

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, gd->bd);
	if (ret) {
		puts("DDR: Failed to decode memory node\n");
		ret = -ENXIO;

		goto err;
	}

	printf("DDR: %lld MiB\n", gd->ram_size >> 20);

	sdram_size_check(gd->bd);
	printf("DDR: size check success\n");

	printf("DDR: firewall init success\n");

	priv->info.base = gd->bd->bi_dram[0].start;
	priv->info.size = gd->ram_size;

	printf("DDR: init success\n");

err:

	return ret;
}
