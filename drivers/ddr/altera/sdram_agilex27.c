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

/*
 * TODO ubootAgilex27: restore the OCRAM double-bit-error status helper
 * below once ECC recovery is wired up:
 *
 * static bool hps_ocram_dbe_status(void)
 * {
 *     u32 reg = readl(BOOT_SCRATCH_COLD3_REG);
 *
 *     if (reg & ALT_SYSMGR_SCRATCH_REG_3_OCRAM_DBE_MASK)
 *         return true;
 *
 *     return false;
 * }
 */

int sdram_mmr_init_full(struct udevice *dev)
{
	int __maybe_unused i;
	int ret = 0;
	phys_size_t __maybe_unused hw_size;
	struct altera_sdram_plat __maybe_unused *plat = dev_get_plat(dev);
	struct altera_sdram_priv *priv = dev_get_priv(dev);
	//struct io96b_info *io96b_ctrl = malloc(sizeof(*io96b_ctrl));

	//u32 reg = readl(BOOT_SCRATCH_COLD3_REG);
	//enum reset_type reset_t = get_reset_type(reg);
	//bool full_mem_init = false;

	/* DDR initialization progress status tracking */
	//bool is_ddr_hang_be4_rst = is_ddr_init_hang();

	printf("DDR: SDRAM init in progress ...\n");
	//ddr_init_inprogress(true);

	gd->bd = (struct bd_info *)malloc(sizeof(struct bd_info));
	memset(gd->bd, '\0', sizeof(struct bd_info));

	//debug("DDR: Address MPFE 0x%llx\n", plat->mpfe_base_addr);

	/* Populating DDR handoff data */
	//debug("DDR: Checking SDRAM configuration in progress ...\n");
	//populate_ddr_handoff(dev, io96b_ctrl);

	/* Configuring Interleave/Non-interleave ccu registers */
	//config_ccu_mgr(dev);

	/* Configure if polling is needed for IO96B GEN PLL locked */
	//io96b_ctrl->ckgen_lock = true;

	/* Ensure calibration status passing */
	//init_mem_cal(io96b_ctrl);

	printf("DDR: Calibration success\n");

	/* Initiate IOSSM mailbox */
	//io96b_mb_init(io96b_ctrl);

	/* DDR type, DDR size and ECC status) */
	//ret = get_mem_technology(io96b_ctrl);
	//if (ret) {
	//	printf("DDR: Failed to get DDR type\n");

	//	goto err;
	//}

	//ret = get_mem_width_info(io96b_ctrl);
	//if (ret) {
	//	printf("DDR: Failed to get DDR size\n");

	//	goto err;
	//}

	//ret = ecc_enable_status(io96b_ctrl);
	//if (ret) {
	//	printf("DDR: Failed to get ECC enabled status\n");

	//	goto err;
	//}

	//hw_size = io96b_ctrl->overall_size;

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, gd->bd);
	if (ret) {
		puts("DDR: Failed to decode memory node\n");
		ret = -ENXIO;

		goto err;
	}

	//if (io96b_ctrl->inline_ecc)
	//	hw_size = CALC_INLINE_ECC_HW_SIZE(hw_size);

	/*
	 * TODO ubootAgilex27: restore the io96b-based DRAM size validation
	 * below once the io96b controller data is wired up:
	 *
	 * if (gd->ram_size > hw_size) {
	 *     printf("DDR: Warning: DRAM size from device tree (%lld MiB)"
	 *            " exceeds\n", gd->ram_size >> 20);
	 *     printf(" the actual hardware capacity(%lld MiB). Memory"
	 *            " configuration will be\n", hw_size >> 20);
	 *     printf(" adjusted to match the detected hardware size.\n");
	 *     gd->ram_size = 0;
	 * }
	 *
	 * if (gd->ram_size > 0 && gd->ram_size != hw_size) {
	 *     printf("DDR: Warning: DRAM size from device tree (%lld MiB)\n",
	 *            gd->ram_size >> 20);
	 *     printf(" mismatch with hardware capacity(%lld MiB).\n",
	 *            hw_size >> 20);
	 * }
	 *
	 * if (gd->ram_size == 0 && hw_size > 0) {
	 *     phys_size_t remaining_size, size_counter = 0;
	 *     u8 config_dram_banks;
	 *
	 *     if (CONFIG_NR_DRAM_BANKS > MEMORY_BANK_MAX_COUNT) {
	 *         printf("DDR: Warning: CONFIG_NR_DRAM_BANKS(%d) is bigger"
	 *                " than Max Memory Bank count(%d).\n",
	 *                CONFIG_NR_DRAM_BANKS, MEMORY_BANK_MAX_COUNT);
	 *         printf(" Max Memory Bank count is in use instead of"
	 *                " CONFIG_NR_DRAM_BANKS.\n");
	 *         config_dram_banks = MEMORY_BANK_MAX_COUNT;
	 *     } else {
	 *         config_dram_banks = CONFIG_NR_DRAM_BANKS;
	 *     }
	 *
	 *     for (i = 0; i < config_dram_banks; i++) {
	 *         remaining_size = hw_size - size_counter;
	 *         if (remaining_size <= dram_bank_info[i].max_size) {
	 *             gd->bd->bi_dram[i].start = dram_bank_info[i].start;
	 *             gd->bd->bi_dram[i].size = remaining_size;
	 *             debug("Memory bank[%d]  Starting address: 0x%llx"
	 *                   "  size: 0x%llx\n", i,
	 *                   gd->bd->bi_dram[i].start,
	 *                   gd->bd->bi_dram[i].size);
	 *             break;
	 *         }
	 *
	 *         gd->bd->bi_dram[i].start = dram_bank_info[i].start;
	 *         gd->bd->bi_dram[i].size = dram_bank_info[i].max_size;
	 *         debug("Memory bank[%d]  Starting address: 0x%llx"
	 *               "  size: 0x%llx\n", i,
	 *               gd->bd->bi_dram[i].start,
	 *               gd->bd->bi_dram[i].size);
	 *         size_counter += gd->bd->bi_dram[i].size;
	 *     }
	 *
	 *     gd->ram_size = hw_size;
	 * }
	 */

	printf("DDR: %lld MiB\n", gd->ram_size >> 20);

	/*
	 * Is HPS cold or warm reset? If yes, skip full memory initialization
	 * if ECC enabled to preserve memory content.
	 *
	 * TODO ubootAgilex27: restore the ECC recovery path below once the
	 * io96b controller data is wired up:
	 *
	 * if (io96b_ctrl->ecc_status) {
	 *     if (ecc_interrupt_status(io96b_ctrl)) {
	 *         if (CONFIG_IS_ENABLED(WDT)) {
	 *             struct udevice *wdt;
	 *
	 *             printf("DDR: ECC error recover start now\n");
	 *             ret = uclass_first_device_err(UCLASS_WDT, &wdt);
	 *             if (ret) {
	 *                 printf("DDR: Failed to trigger watchdog reset\n");
	 *                 hang();
	 *             }
	 *
	 *             wdt_expire_now(wdt, 0);
	 *         }
	 *         hang();
	 *     }
	 *
	 *     full_mem_init = hps_ocram_dbe_status() | is_ddr_hang_be4_rst;
	 *     if (full_mem_init ||
	 *         !(reset_t == WARM_RESET || reset_t == COLD_RESET)) {
	 *         ret = bist_mem_init_start(io96b_ctrl);
	 *         if (ret) {
	 *             printf("DDR: Failed to fully initialize DDR memory\n");
	 *             goto err;
	 *         }
	 *     }
	 *
	 *     printf("SDRAM-ECC: Initialized success\n");
	 * }
	 */
	sdram_size_check(gd->bd);
	printf("DDR: size check success\n");

//	sdram_set_firewall(gd->bd);

	/* Firewall setting for MPFE CSR */
	//config_firewall_mpfe_csr(dev);

	printf("DDR: firewall init success\n");

	priv->info.base = gd->bd->bi_dram[0].start;
	priv->info.size = gd->ram_size;

	/* Ending DDR driver initialization success tracking */
	//ddr_init_inprogress(false);

	printf("DDR: init success\n");

err:

	return ret;
}
