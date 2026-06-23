/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#ifndef _DRIVERS_CLK_ALTERA_AGILEX72_CLKMGR_H_
#define _DRIVERS_CLK_ALTERA_AGILEX72_CLKMGR_H_

#include <errno.h>
#include <linux/kconfig.h>
#include <linux/types.h>

struct agilex72_clkmgr_rate_state {
	bool valid;
	u64 gppll0_vco_hz;
	u64 gppll1_vco_hz;
	u64 gppll2_vco_hz;
	u32 gppll0_c0_div;
	u32 gppll0_c1_div;
	u32 gppll0_c2_div;
	u32 gppll0_c3_div;
	u32 gppll1_c0_div;
	u32 gppll1_c1_div;
	u32 gppll2_c0_div;
	u32 gppll2_c1_div;
};

const struct agilex72_clkmgr_rate_state *agilex72_clkmgr_rate_state(void);

struct agilex72_clkmgr_entry {
	const char *key;
	const char *value;
};

struct agilex72_clkmgr_reg_abs_entry {
	u32 addr;
	u32 value;
	u32 mask;
};

int agilex72_clkmgr_apply_handoff(const struct agilex72_clkmgr_entry *entries, int count);
int agilex72_clkmgr_apply_reg_abs(const struct agilex72_clkmgr_reg_abs_entry *entries,
				  int count);

int agilex72_handoff_parse_and_apply(const void *blob, size_t blob_max);

int agilex72_handoff_get_blob(const void **blob_out, size_t *size_out);

extern const u8 agilex72_handoff_blob[];
extern const u32 agilex72_handoff_blob_size;

void agilex72_clkmgr_secure_unlock(void);
void agilex72_clkmgr_pll_cfg5_rearm(void);
void agilex72_pll_enable(void);
int agilex72_pll_wait_lock(void);
int agilex72_clkmgr_audit_dv_preset_rates(bool check_clkouts);
void agilex72_clkmgr_bisect_reg_abs(u32 base, u32 count);
void agilex72_clkmgr_bisect_cfg5_rearm_done(void);
void agilex72_clkmgr_bisect_kv_milestone(const char *first_key, int ret);
void agilex72_clkmgr_refresh_vco_from_csr(void);
void agilex72_clkmgr_refresh_c_from_csr(void);
void agilex72_disable_boot_clk_bypass(void);

void agilex72_clkmgr_virtual_platform_minimal_init(void);

int agilex72_clkmgr_refresh_rates_from_csr_if_locked(void);

void agilex72_clkmgr_print_rate_state(void);
void agilex72_config_main_pll(unsigned long freq);
void agilex72_config_periph_pll(unsigned long freq);

#endif /* _DRIVERS_CLK_ALTERA_AGILEX72_CLKMGR_H_ */
