// SPDX-License-Identifier: GPL-2.0+
/*
 * Inclusion shim that compiles arch/arm/mach-socfpga/rsu.c into a
 * native host binary under -fsanitize=address,undefined.
 *
 * The production source is #included into this translation unit. All
 * hardware and U-Boot dependencies (rsu_ll_qspi_init, mbox_*, smc_*,
 * ofnode_*, secure_ram_addr, etc.) are resolved by the stubs defined
 * below, so the production functions (rsu_init / rsu_exit /
 * rsu_slot_*) become global symbols in the harness binary and are
 * link-visible to test_asan/rsu/rsu_asan.c.
 *
 * Build
 * -----
 * Compiled by test_asan/rsu/Makefile with:
 *   -Itest_asan/rsu/include          (host shim headers, searched
 *                                     first so kernel and arch
 *                                     headers resolve to stubs)
 *   -Iarch/arm/mach-socfpga/include  (production rsu.h / rsu_ll.h
 *                                     via <asm/arch/...>)
 *
 * Stub Kconfig posture
 * --------------------
 * - CONFIG_SOCFPGA_RSU_DM    : NOT defined -> non-DM single-session
 *                              path, no DM uclass plumbing required.
 * - CONFIG_SPL_ATF           : defined -> rsu.c uses invoke_smc() for
 *                              DCMF version / status / max-retry copy
 *                              (simpler stub than the secure_ram_addr
 *                              path).
 * - CONFIG_XPL_BUILD         : NOT defined.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <asm/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/intel-smc.h>
#include <linux/kconfig.h>

/*
 * U-Boot kernel-style helpers that the production sources rely on
 * via transitive includes; on the host we provide them inline rather
 * than ship a full <linux/kernel.h> shim.
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/*
 * Production rsu_misc.h declares `int pow(u32, u32)` which collides
 * with libc's double pow(double, double) when compiled natively. We
 * rename the symbol the production tree is using, since we don't
 * actually link against it from the host build.
 */
#define pow rsu_pow_unused

/*
 * Kconfig posture for this TU is injected by the harness Makefile
 * (-DCONFIG_SPL_ATF=1) so that rsu.c selects its invoke_smc() paths
 * for DCMF version / status / max-retry copy. CONFIG_SOCFPGA_RSU_DM
 * is deliberately not defined, so rsu.c falls into the simpler
 * non-DM single-session branch.
 */

#include <asm/arch/rsu.h>
#include <asm/arch/rsu_ll.h>
#include <asm/arch/rsu_misc.h>
#include <asm/arch/smc_api.h>

/* ------------------------------------------------------------------ */
/*  External-symbol stubs that production rsu.c reaches for            */
/* ------------------------------------------------------------------ */

/*
 * rsu.c declares these as extern. The harness needs the storage to
 * exist somewhere or the link fails. We keep them static-equivalent
 * (file-scope in this TU) — rsu.c references them by name, which
 * resolves to these definitions at link time.
 */
u32 smc_rsu_dcmf_version[4];
u16 smc_rsu_dcmf_status[4];

/*
 * SMC bridge stub. The production paths used in rsu.c are:
 *   - INTEL_SIP_SMC_RSU_COPY_DCMF_VERSION
 *   - INTEL_SIP_SMC_RSU_COPY_DCMF_STATUS
 *   - INTEL_SIP_SMC_RSU_COPY_MAX_RETRY
 * The harness records the most recent invocation so tests can assert
 * the bridge was reached with the expected payload, without needing
 * an actual ATF / SDM round-trip.
 */
struct prod_smc_record {
	u32 func_id;
	u64 arg0;
	u64 arg1;
	int arg_len;
	int call_count;
};

static struct prod_smc_record g_prod_smc;

int invoke_smc(u32 func_id, u64 *args, int arg_len, u64 *ret_arg, int ret_len)
{
	(void)ret_arg;
	(void)ret_len;
	g_prod_smc.func_id    = func_id;
	g_prod_smc.arg_len    = arg_len;
	g_prod_smc.arg0       = arg_len > 0 ? args[0] : 0;
	g_prod_smc.arg1       = arg_len > 1 ? args[1] : 0;
	g_prod_smc.call_count++;
	return 0;
}

/* Test-side accessor (declared extern in rsu_asan.c). */
const struct prod_smc_record *prod_smc_last(void) { return &g_prod_smc; }
void prod_smc_reset(void) { memset(&g_prod_smc, 0, sizeof(g_prod_smc)); }

/* ------------------------------------------------------------------ */
/*  rsu_misc / rsu_cb stubs                                            */
/* ------------------------------------------------------------------ */
/*
 * rsu.c calls these helpers via the rsu_misc.h surface. The harness
 * gives them small, deterministic implementations so that production
 * rsu.c's own logic (slot indexing, name validation, callback
 * plumbing) is what's actually exercised — not a re-implementation
 * of the helpers themselves. Helpers ARE the next migration target;
 * once test_asan/rsu/ pulls in rsu_misc.c via #include the same way
 * we do for rsu.c here, these stubs disappear.
 */
void rsu_log(const enum rsu_log_level level, const char *fmt, ...)
{
	va_list ap;

	(void)level;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void rsu_misc_safe_strcpy(char *dst, int dsz, char *src, int ssz)
{
	int len;

	if (!dst || dsz <= 0)
		return;
	if (!src || ssz <= 0) {
		dst[0] = '\0';
		return;
	}
	len = (int)strnlen(src, (size_t)ssz);
	if (len >= dsz)
		len = dsz - 1;
	memcpy(dst, src, (size_t)len);
	dst[len] = '\0';
}

static const char * const g_prod_reserved_names[] = {
	"BOOT_INFO", "FACTORY_IMAGE", "SPT", "SPT0", "SPT1",
	"CPB", "CPB0", "CPB1", NULL
};

int rsu_misc_is_rsvd_name(char *name)
{
	int i;

	if (!name)
		return 0;
	for (i = 0; g_prod_reserved_names[i]; i++)
		if (!strcmp(name, g_prod_reserved_names[i]))
			return 1;
	return 0;
}

int rsu_misc_is_slot(struct rsu_ll_intf *ll, int part_num)
{
	if (!ll || !ll->partition.name || !ll->partition.readonly ||
	    !ll->partition.reserved)
		return 0;
	if (ll->partition.readonly(part_num))
		return 0;
	if (ll->partition.reserved(part_num))
		return 0;
	if (rsu_misc_is_rsvd_name(ll->partition.name(part_num)))
		return 0;
	return 1;
}

int rsu_misc_slot2part(struct rsu_ll_intf *ll, int slot)
{
	int partitions, x, cnt = 0;

	if (!ll || !ll->partition.count)
		return -1;
	partitions = ll->partition.count();
	for (x = 0; x < partitions; x++) {
		if (!rsu_misc_is_slot(ll, x))
			continue;
		if (cnt == slot)
			return x;
		cnt++;
	}
	return -1;
}

int rsu_misc_writeprotected(int slot)
{
	(void)slot;
	return 0;
}

int rsu_misc_spt_checksum_enabled(void)
{
	return 1;
}

/*
 * rsu_cb_* are buffer-callback plumbing for rsu_slot_program_buf /
 * rsu_slot_verify_buf. The harness only needs them to compile; the
 * tests that call program/verify cover the parameter-validation
 * surface of rsu.c, not the callback streaming itself.
 */
static void *g_prod_cb_buf;
static int   g_prod_cb_size;
static int   g_prod_cb_off;

int rsu_cb_buf_init(void *buf, int size)
{
	if (!buf || size <= 0)
		return -1;
	g_prod_cb_buf  = buf;
	g_prod_cb_size = size;
	g_prod_cb_off  = 0;
	return 0;
}

int rsu_cb_buf(void *buf, int len)
{
	int copy;

	if (!buf || len <= 0 || !g_prod_cb_buf)
		return 0;
	copy = g_prod_cb_size - g_prod_cb_off;
	if (copy > len)
		copy = len;
	if (copy <= 0)
		return 0;
	memcpy(buf, (char *)g_prod_cb_buf + g_prod_cb_off, (size_t)copy);
	g_prod_cb_off += copy;
	return copy;
}

void rsu_cb_buf_exit(void)
{
	g_prod_cb_buf  = NULL;
	g_prod_cb_size = 0;
	g_prod_cb_off  = 0;
}

int rsu_cb_program_common(struct rsu_ll_intf *ll, int slot,
			  rsu_data_callback callback, int rawdata)
{
	(void)ll;
	(void)slot;
	(void)callback;
	(void)rawdata;
	return 0;
}

int rsu_cb_verify_common(struct rsu_ll_intf *ll, int slot,
			 rsu_data_callback callback, int rawdata)
{
	(void)ll;
	(void)slot;
	(void)callback;
	(void)rawdata;
	return 0;
}

int smc_store_max_retry(u32 value)
{
	(void)value;
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Low-level QSPI backend init stub                                   */
/* ------------------------------------------------------------------ */
/*
 * Tests register a backend by setting g_prod_ll_intf to a pointer
 * they own; rsu_ll_qspi_init() then hands that pointer back to
 * production rsu.c. NULL return = simulated init failure (used to
 * verify the errno-preservation logic in rsu_init()).
 */
static struct rsu_ll_intf *g_prod_ll_intf;
static int                 g_prod_ll_init_errno;

void prod_set_backend(struct rsu_ll_intf *intf, int init_errno)
{
	g_prod_ll_intf       = intf;
	g_prod_ll_init_errno = init_errno;
}

int rsu_ll_qspi_init(struct rsu_ll_intf **intfp)
{
	if (!intfp)
		return -EINVAL;
	if (g_prod_ll_init_errno)
		return g_prod_ll_init_errno;
	*intfp = g_prod_ll_intf;
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Drop the production source into this translation unit.            */
/* ------------------------------------------------------------------ */
/*
 * Everything above must be in scope before this include: production
 * rsu.c uses the symbols, types, and CONFIG_* posture defined here.
 */
#include "../../arch/arm/mach-socfpga/rsu.c"

/* ------------------------------------------------------------------ */
/*  prod_* accessor namespace                                          */
/* ------------------------------------------------------------------ */
/*
 * test_asan/rsu/rsu_asan.c has its own file-scope `static int
 * rsu_init(...)` clean-room port (and friends). Inside that TU,
 * unqualified calls to rsu_init() bind to the static, not to the
 * production global defined just above by the included rsu.c.
 *
 * New tests want to exercise the *production* code under ASan/UBSan,
 * so we expose thin wrappers under a prod_* namespace. The harness
 * TU declares them extern and calls them from [PROD] tests. The
 * existing 49 tests still use the static clean-room port and remain
 * unchanged.
 *
 * Once Phase 2 of A1 retires the clean-room ports entirely, the
 * harness can call rsu_init() / rsu_exit() / rsu_slot_*() directly
 * and these wrappers go away.
 */
int prod_rsu_init(char *filename) { return rsu_init(filename); }
void prod_rsu_exit(void) { rsu_exit(); }
int prod_rsu_slot_count(void) { return rsu_slot_count(); }
int prod_rsu_slot_by_name(char *name) { return rsu_slot_by_name(name); }
int prod_rsu_slot_size(int slot) { return rsu_slot_size(slot); }
int prod_rsu_slot_priority(int slot) { return rsu_slot_priority(slot); }
int prod_rsu_slot_erase(int slot) { return rsu_slot_erase(slot); }
int prod_rsu_slot_enable(int slot) { return rsu_slot_enable(slot); }
int prod_rsu_slot_disable(int slot) { return rsu_slot_disable(slot); }
int prod_rsu_slot_load(int slot) { return rsu_slot_load(slot); }
int prod_rsu_slot_load_factory(void) { return rsu_slot_load_factory(); }
int prod_rsu_slot_rename(int slot, char *n) { return rsu_slot_rename(slot, n); }
int prod_rsu_slot_delete(int slot) { return rsu_slot_delete(slot); }
int prod_rsu_notify(int stage) { return rsu_notify(stage); }
int prod_rsu_clear_error_status(void) { return rsu_clear_error_status(); }
int prod_rsu_reset_retry_counter(void) { return rsu_reset_retry_counter(); }
int prod_rsu_dcmf_version(u32 *v) { return rsu_dcmf_version(v); }
int prod_rsu_max_retry(u8 *v) { return rsu_max_retry(v); }
int prod_rsu_dcmf_status(u16 *st) { return rsu_dcmf_status(st); }
int prod_rsu_create_empty_cpb(void) { return rsu_create_empty_cpb(); }
int prod_rsu_restore_cpb(u64 a) { return rsu_restore_cpb(a); }
int prod_rsu_save_cpb(u64 a) { return rsu_save_cpb(a); }
int prod_rsu_restore_spt(u64 a) { return rsu_restore_spt(a); }
int prod_rsu_save_spt(u64 a) { return rsu_save_spt(a); }

int prod_rsu_slot_get_info(int slot, struct rsu_slot_info *info)
{
	return rsu_slot_get_info(slot, info);
}

int prod_rsu_slot_create(char *name, u64 address, unsigned int size)
{
	return rsu_slot_create(name, address, size);
}

int prod_rsu_status_log(struct rsu_status_info *info)
{
	return rsu_status_log(info);
}

int prod_rsu_running_factory(int *factory)
{
	return rsu_running_factory(factory);
}
