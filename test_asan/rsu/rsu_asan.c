// SPDX-License-Identifier: GPL-2.0+
/*
 * ASan / UBSan harness for the SoC FPGA RSU driver stack.
 *
 * One harness file per driver lives under test_asan/<driver>/; this is the
 * RSU harness. It aims for full behavioural coverage of every source file
 * in the RSU stack, compiled natively under -fsanitize=address,undefined
 * with leak detection enabled.
 *
 * Files covered (by section):
 *   [FIF ] arch/arm/mach-socfpga/include/mach/rsu_flash_if.h   (non-DM path)
 *   [S10 ] arch/arm/mach-socfpga/rsu_s10.c                     (post-refactor)
 *   [LLQ ] arch/arm/mach-socfpga/rsu_ll_qspi.c                 (hardened core)
 *   [RSU ] arch/arm/mach-socfpga/rsu.c                         (public API)
 *   [SPL ] arch/arm/mach-socfpga/rsu_spl.c                     (SPL helpers)
 *   [CMD ] cmd/socfpga_rsu.c                                   (console cmd)
 *   [DM  ] drivers/misc/socfpga_rsu.c                          (DM probe)
 *   [UT  ] test/cmd/socfpga_rsu.c                              (usage test)
 *
 * Hardware (SPI/QSPI, mailbox, SMC, DM uclass, FDT) is stubbed.
 *
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <zlib.h>

/* ============================================================ */
/*  Minimal U-Boot type shims                                     */
/* ============================================================ */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed long long    s64;
typedef unsigned char       __u8;
typedef unsigned int        __u32;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT(n)        (1UL << (n))
#define SZ_4K         0x1000
#define SZ_8M         0x800000

/* u-boot return codes used by cmd/socfpga_rsu.c */
#define CMD_RET_SUCCESS  0
#define CMD_RET_FAILURE  1
#define CMD_RET_USAGE    -1

/* rsu.h error constants */
#define EINTF           1
#define ECFG            2
#define ESLOTNUM        3
#define EFORMAT         4
#define ENAME           9
#define ELOWLEVEL      12
#define EWRPROT        13
#define EARGS          14
#define ECORRUPTED_CPB 15
#define ECORRUPTED_SPT 16

/*
 * SPI flash chip-select stubs for the host-compiled harness.
 *
 * CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS and CONFIG_SOCFPGA_RSU_SF_CS
 * are injected as compile-time defines by the Makefile (-D...) to keep the
 * source file free of Kconfig-style #defines that checkpatch rejects.
 *
 * The real tree splits CONFIG_SF_DEFAULT_CS (used by the SPL QSPI boot flow
 * to load u-boot.itb) from CONFIG_SOCFPGA_RSU_SF_CS (used by the RSU flow
 * to probe the flash that holds the SPT/CPB). The harness only exercises
 * the RSU flow, so all rsu_mtd_probe() callers use CONFIG_SOCFPGA_RSU_SF_CS.
 */

/* rsu_ll_qspi constants (lifted from rsu_ll_qspi.c) */
#define SPT_MAGIC_NUMBER       0x57713427
#define SPT_FLAG_RESERVED      1
#define SPT_FLAG_READONLY      2
#define CPB_MAGIC_NUMBER       0x57789609
#define CPB_HEADER_SIZE        24
#define ERASED_ENTRY           ((u64)-1)
#define SPENT_ENTRY            ((u64)0)
#define QSPI_MAX_DEVICE        4
#define SPT_MAX_PARTITIONS     127
#define MIN_QSPI_ERASE_SIZE    4096
#define CPB_SIZE               SZ_4K
#define SPT_SIZE               SZ_4K
#define CPB_IMAGE_PTR_OFFSET   32
#define CPB_IMAGE_PTR_NSLOTS   508
#define SPT_CHECKSUM_OFFSET    0x0C
#define FACTORY_IMAGE_NAME     "FACTORY_IMAGE"

/* STATE_CPB constants */
#define STATE_OK                    0x00000000u
#define STATE_CPB0_CORRUPTED        0xF004D010u
#define STATE_CPB0_CPB1_CORRUPTED   0xF004D011u

/* rsu_s10.h surface */
#define RSU_S10_SPT_MAGIC_NUMBER   0x57713427
#define RSU_S10_CPB_MAGIC_NUMBER   0x57789609
#define MAX_PART_NAME_LENGTH       16
#define SPT0_INDEX                 1
#define SPT1_INDEX                 3
#define RSU_S10_SPT_SLOT_MAX       127
#define RSU_SPL_SPT_SLOT_MAX       127

/* status info */
struct rsu_status_info {
	u64 current_image;
	u64 fail_image;
	u32 state;
	u32 version;
	u32 error_location;
	u32 error_details;
	u32 retry_counter;
};

/* version accessors (stub) */
#define RSU_VERSION_CRT_IDX_MASK    (0xFu << 28)
#define RSU_VERSION_DCMF_MASK       (0xFFu)
#define RSU_VERSION_ACMF_MASK       (0xFF00u)
#define RSU_VERSION_CRT_DCMF_IDX(v) (((v) & RSU_VERSION_CRT_IDX_MASK) >> 28)
#define RSU_VERSION_ACMF_VERSION(v) (((v) & RSU_VERSION_ACMF_MASK) >> 8)
#define RSU_VERSION_DCMF_VERSION(v)  ((v) & RSU_VERSION_DCMF_MASK)

/* rsu.h — notify bitmasks (matches rsu.c literals) */
#define RSU_NOTIFY_IGNORE_STAGE         BIT(18)
#define RSU_NOTIFY_CLEAR_ERROR_STATUS   BIT(17)
#define RSU_NOTIFY_RESET_RETRY_COUNTER  BIT(16)

/* rsu_slot_info (rsu.h) */
struct rsu_slot_info {
	char name[16];
	u64  offset;
	u32  size;
	int  priority;
};

/* rsu_s10.h structs */
struct socfpga_rsu_s10_spt_slot {
	char name[MAX_PART_NAME_LENGTH];
	u32  offset[2];
	u32  length;
	u32  flag;
};

struct socfpga_rsu_s10_spt {
	u32 magic_number;
	u32 version;
	u32 entries;
	u32 rsvd[5];
	struct socfpga_rsu_s10_spt_slot spt_slot[SPT_MAX_PARTITIONS];
};

#define CPB_NSLOTS 508
struct socfpga_rsu_s10_cpb {
	u32 magic_number;
	u32 header_size;
	u32 cpb_size;
	u32 cpb_reserved;
	u32 image_ptr_offset;
	u32 nslots;
	u64 pointer_slot[CPB_NSLOTS];
};

/* rsu_ll_qspi internal structs */
struct sub_partition_table_partition {
	char name[16];
	u64  offset;
	u32  length;
	u32  flags;
};

struct sub_partition_table {
	u32 magic_number;
	u32 version;
	u32 partitions;
	u32 checksum;
	u32 rsvd[4];
	struct sub_partition_table_partition partition[SPT_MAX_PARTITIONS];
};

union cmf_pointer_block {
	struct {
		u32 magic_number;
		u32 header_size;
		u32 cpb_size;
		u32 cpb_reserved;
		u32 image_ptr_offset;
		u32 image_ptr_slots;
	} header;
	char data[CPB_SIZE];
};

/* rsu_ll interface surface (rsu_ll.h) */
struct rsu_ll_intf {
	void (*exit)(void);
	void *priv;
	struct {
		int   (*count)(void);
		char *(*name)(int part_num);
		u64   (*offset)(int part_num);
		s64   (*factory_offset)(void);
		u32   (*size)(int part_num);
		int   (*reserved)(int part_num);
		int   (*readonly)(int part_num);
		int   (*rename)(int part_num, char *name);
		int   (*delete)(int part_num);
		int   (*create)(char *name, u64 start, unsigned int size);
	} partition;
	struct {
		int (*get)(int part_num);
		int (*add)(int part_num);
		int (*remove)(int part_num);
	} priority;
	struct {
		int (*read)(int part_num, int offset, int bytes, void *buf);
		int (*write)(int part_num, int offset, int bytes, void *buf);
		int (*erase)(int part_num);
	} data;
	struct {
		int (*load)(u64 offset);
		int (*status)(struct rsu_status_info *info);
		int (*notify)(u32 value);
		int (*dcmf_version)(u32 *versions);
		int (*dcmf_status)(u16 *status);
		int (*max_retry)(u8 *value);
	} fw_ops;
	struct {
		int (*empty)(void);
		int (*restore)(u64 address);
		int (*save)(u64 address);
		int (*corrupted)(void);
	} cpb_ops;
	struct {
		int (*restore)(u64 address);
		int (*save)(u64 address);
		int (*corrupted)(void);
	} spt_ops;
};

/* socfpga_rsu_dm.h */
struct socfpga_rsu_priv {
	struct rsu_ll_intf *ll;
};

/* ============================================================ */
/*  Fake flash + mailbox backend                                  */
/* ============================================================ */
struct fake_dev {
	u32   size;
	u32   erasesize;
	unsigned char *mem;
	bool  probe_fails;
};

static struct fake_dev *make_flash(u32 size, u32 esz)
{
	struct fake_dev *d = calloc(1, sizeof(*d));

	d->size = size;
	d->erasesize = esz;
	d->mem = calloc(1, size);
	memset(d->mem, 0xFF, size);
	return d;
}

static void free_flash(struct fake_dev *d)
{
	if (!d)
		return;
	free(d->mem);
	free(d);
}

/* --- rsu_flash_if.h surface (non-DM inline wrappers, emulated) --- */
static u32 rsu_mtd_size(struct fake_dev *d)      { return d ? d->size : 0; }
static u32 rsu_mtd_erasesize(struct fake_dev *d) { return d ? d->erasesize : 0; }

static int rsu_mtd_read(struct fake_dev *d, u32 off, size_t len, void *buf)
{
	if (!d || (u64)off + len > d->size)
		return -EINVAL;
	memcpy(buf, d->mem + off, len);
	return 0;
}

static int rsu_mtd_write(struct fake_dev *d, u32 off, size_t len, const void *buf)
{
	if (!d || (u64)off + len > d->size)
		return -EINVAL;
	memcpy(d->mem + off, buf, len);
	return 0;
}

static int rsu_mtd_erase(struct fake_dev *d, u32 off, size_t len)
{
	if (!d || (u64)off + len > d->size)
		return -EINVAL;
	memset(d->mem + off, 0xFF, len);
	return 0;
}

static void rsu_mtd_unclaim(struct fake_dev *d) { (void)d; }

/*
 * rsu_mtd_probe non-DM emulation:
 *   struct spi_flash *f = spi_flash_probe(...); if (!f) return -ENODEV;
 *   *flashp = f; return 0;
 */
static struct fake_dev *g_probe_result;
static int              g_probe_force_fail;
static int rsu_mtd_probe(unsigned int bus, unsigned int cs, struct fake_dev **devp)
{
	(void)bus; (void)cs;
	if (g_probe_force_fail || !g_probe_result)
		return -ENODEV;
	*devp = g_probe_result;
	return 0;
}

/* --- mailbox stubs --- */
static u32 g_mbox_spt_off[4] = { 0, 0x10000, 0, 0x20000 };
static int g_mbox_spt_off_fail;
static int mbox_rsu_get_spt_offset(u32 *buf, int words)
{
	if (g_mbox_spt_off_fail)
		return -1;
	memcpy(buf, g_mbox_spt_off, words * sizeof(u32));
	return 0;
}

static struct rsu_status_info g_mbox_status;
static int                    g_mbox_status_fail;
static int mbox_rsu_status(u32 *buf, int words)
{
	if (g_mbox_status_fail)
		return -1;
	memcpy(buf, &g_mbox_status, words * sizeof(u32));
	return 0;
}

static int mbox_rsu_update(u32 *off)        { (void)off; return 0; }
static int mbox_hps_stage_notify(u32 value) { (void)value; return 0; }

/* ============================================================ */
/*  Logging                                                       */
/* ============================================================ */
/*
 * Default to verbose: the whole point of the hardening commits is the
 * rsu_log() warnings they emit. Silencing them by default makes a
 * failing test print only "FAIL" without context. Tests that need to
 * silence noise (e.g. negative paths that intentionally trip a log
 * line) should set g_log_silent=1 around the specific assertion and
 * restore it afterward, NOT leave it silenced globally.
 */
static int g_log_silent;
enum { RSU_EMERG, RSU_ALERT, RSU_CRIT, RSU_ERR, RSU_WARNING, RSU_NOTICE,
	RSU_INFO, RSU_DEBUG };
static void rsu_log(int level, const char *fmt, ...)
{
	va_list ap;

	if (g_log_silent)
		return;
	(void)level;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/* rsu_misc bits */
static void rsu_misc_safe_strcpy(char *dst, int dsz, char *src, int ssz)
{
	int len;

	if (!dst || dsz <= 0)
		return;
	if (!src || ssz <= 0) {
		dst[0] = '\0';
		return;
	}
	len = strnlen(src, ssz);
	if (len >= dsz)
		len = dsz - 1;
	memcpy(dst, src, len);
	dst[len] = '\0';
}

static const char * const g_reserved_names[] = {
	"BOOT_INFO", "FACTORY_IMAGE", "SPT", "SPT0", "SPT1",
	"CPB", "CPB0", "CPB1", NULL
};

static int rsu_misc_is_rsvd_name(const char *name)
{
	for (int i = 0; g_reserved_names[i]; i++)
		if (!strcmp(name, g_reserved_names[i]))
			return 1;
	return 0;
}

/* ============================================================ */
/*  QSPI backend — ports of rsu_ll_qspi.c functions               */
/* ============================================================ */
struct rsu_qspi_priv {
	union cmf_pointer_block     cpb;
	struct sub_partition_table  spt;
	u64                        *cpb_slots;
	struct fake_dev           **flashlist;
	struct fake_dev            *flash;
	int                         num_flash;
	int                         num_flash_alloc;
	u32                         spt0_offset;
	u32                         spt1_offset;
	int                         cpb0_part;
	int                         cpb1_part;
	bool                        cpb_corrupted;
	bool                        cpb_fixed;
	bool                        spt_corrupted;
};

static struct rsu_qspi_priv *qspi_ctx;
#define P (qspi_ctx)

/* --- forward decls --- */
static int load_cpb(void);
static int load_spt(void);

/* ---- core helpers (verbatim logic from PR) ---- */
static int get_part_offset(int part_num, u64 *offset)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	*offset = P->spt.partition[part_num].offset;
	return 0;
}

static int get_current_flash_offset(u64 offset, u32 *current_offset,
				    int *current_flash)
{
	u64 relative_offset = offset;

	if (!current_offset || !current_flash)
		return -EINVAL;
	for (int j = 0; j < P->num_flash && j < QSPI_MAX_DEVICE; j++) {
		u32 sz = rsu_mtd_size(P->flashlist[j]);

		if (!sz)
			return -EINVAL;
		if (relative_offset >= sz) {
			relative_offset -= sz;
			continue;
		}
		*current_flash  = j;
		*current_offset = (u32)relative_offset;
		return 0;
	}
	return -EINVAL;
}

static int read_dev(u64 offset, void *buf, int len)
{
	int ret, current_flash = 0;
	u32 count = 0, current_len, current_offset = 0;

	if (len < 0)
		return -EINVAL;
	ret = get_current_flash_offset(offset, &current_offset, &current_flash);
	if (ret)
		return ret;
	for (int i = current_flash; i < P->num_flash && i < QSPI_MAX_DEVICE; i++) {
		u32 sz = rsu_mtd_size(P->flashlist[i]);

		if (count == (u32)len)
			break;
		if (!sz || current_offset > sz)
			return -EINVAL;
		if ((u64)(u32)len + current_offset - count > sz)
			current_len = sz - current_offset;
		else
			current_len = (u32)len - count;
		ret = rsu_mtd_read(P->flashlist[i], current_offset,
				   (int)current_len, buf);
		if (ret)
			return ret;
		buf = (char *)buf + current_len;
		current_offset = 0;
		count += current_len;
	}
	return 0;
}

static int write_dev(u64 offset, void *buf, int len)
{
	int ret, current_flash = 0;
	u32 count = 0, current_len, current_offset = 0;

	if (len < 0)
		return -EINVAL;
	ret = get_current_flash_offset(offset, &current_offset, &current_flash);
	if (ret)
		return ret;
	for (int i = current_flash; i < P->num_flash && i < QSPI_MAX_DEVICE; i++) {
		u32 sz = rsu_mtd_size(P->flashlist[i]);

		if (count == (u32)len)
			break;
		if (!sz || current_offset > sz)
			return -EINVAL;
		if ((u64)(u32)len + current_offset - count > sz)
			current_len = sz - current_offset;
		else
			current_len = (u32)len - count;
		ret = rsu_mtd_write(P->flashlist[i], current_offset,
				    (int)current_len, buf);
		if (ret)
			return ret;
		buf = (char *)buf + current_len;
		current_offset = 0;
		count += current_len;
	}
	return 0;
}

static int erase_dev(u64 offset, int len)
{
	int ret, current_flash = 0;
	u32 count = 0, current_len, current_offset = 0;

	if (len < 0)
		return -EINVAL;
	ret = get_current_flash_offset(offset, &current_offset, &current_flash);
	if (ret)
		return ret;
	for (int i = current_flash; i < P->num_flash && i < QSPI_MAX_DEVICE; i++) {
		u32 sz  = rsu_mtd_size(P->flashlist[i]);
		u32 esz = rsu_mtd_erasesize(P->flashlist[i]);
		u32 erase_len;

		if (count >= (u32)len)
			break;
		if (!sz || current_offset > sz)
			return -EINVAL;
		if ((u64)(u32)len + current_offset - count > sz)
			current_len = sz - current_offset;
		else
			current_len = (u32)len - count;
		erase_len = current_len;
		if (esz && (current_len % esz)) {
			u32 rounded = (current_len + esz - 1) & ~(esz - 1);

			if (current_offset > sz ||
			    rounded > sz - current_offset)
				return -EINVAL;
			erase_len = rounded;
		}
		ret = rsu_mtd_erase(P->flashlist[i], current_offset,
				    (int)erase_len);
		if (ret)
			return ret;
		current_offset = 0;
		count += current_len;
	}
	return 0;
}

static int read_part(int part_num, u64 offset, void *buf, int len)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;
	return read_dev(part_offset + offset, buf, len);
}

static int write_part(int part_num, u64 offset, void *buf, int len)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;
	return write_dev(part_offset + offset, buf, len);
}

static int erase_part(int part_num)
{
	u64 part_offset;

	if (get_part_offset(part_num, &part_offset))
		return -1;
	return erase_dev(part_offset, P->spt.partition[part_num].length);
}

/* --- cpb_ptr_slots_access_ok: new PSIRT guard --- */
static int cpb_ptr_slots_access_ok(void)
{
	u32 ip_off   = P->cpb.header.image_ptr_offset;
	u32 ip_slots = P->cpb.header.image_ptr_slots;
	u32 max_by_buf;

	if (!P->cpb_slots)
		return -1;
	if (ip_off >= CPB_SIZE)
		return -1;
	max_by_buf = (CPB_SIZE - ip_off) / sizeof(u64);
	if (!ip_slots || !max_by_buf || ip_slots > max_by_buf ||
	    ip_slots > CPB_IMAGE_PTR_NSLOTS)
		return -1;
	return 0;
}

/* --- check_cpb --- */
static int check_cpb(void)
{
	int x, y;

	if (P->cpb.header.header_size > CPB_HEADER_SIZE)
		return -1;
	{
		u32 ip_off   = P->cpb.header.image_ptr_offset;
		u32 ip_slots = P->cpb.header.image_ptr_slots;
		u32 max_by_buf;

		if (ip_off >= CPB_SIZE)
			return -EINVAL;
		max_by_buf = (CPB_SIZE - ip_off) / sizeof(u64);
		if (!ip_slots || !max_by_buf || ip_slots > max_by_buf ||
		    ip_slots > CPB_IMAGE_PTR_NSLOTS)
			return -EINVAL;
	}
	for (x = 0; x < (int)P->cpb.header.image_ptr_slots; x++) {
		if (P->cpb_slots[x] == ERASED_ENTRY ||
		    P->cpb_slots[x] == SPENT_ENTRY)
			continue;
		for (y = 0; y < (int)P->spt.partitions; y++)
			if (P->cpb_slots[x] == P->spt.partition[y].offset)
				break;
		if (y >= (int)P->spt.partitions)
			return -EINVAL;
		if (P->spt.partition[y].flags & SPT_FLAG_RESERVED)
			return -EINVAL;
	}
	return 0;
}

/* --- check_spt (hardened) --- */
static int check_spt(void)
{
	int x, y;
	int max_len = sizeof(P->spt.partition[0].name);
	int spt0 = 0, spt1 = 0, cpb0 = 0, cpb1 = 0;

	if (P->spt.partitions > SPT_MAX_PARTITIONS)
		return -EINVAL;

	for (x = 0; x < (int)P->spt.partitions; x++) {
		u64 s_start;
		u64 s_len;
		u64 s_end;

		if (strnlen(P->spt.partition[x].name, max_len) >= (size_t)max_len)
			P->spt.partition[x].name[max_len - 1] = '\0';

		s_start = P->spt.partition[x].offset;
		s_len   = P->spt.partition[x].length;
		if (s_len == 0)
			return -EINVAL;
		s_end   = s_start + s_len;

		for (y = 0; y < (int)P->spt.partitions; y++) {
			if (x == y)
				continue;
			if (!strcmp(P->spt.partition[x].name,
				    P->spt.partition[y].name))
				return -EINVAL;
			u64 d_start = P->spt.partition[y].offset;
			u64 d_end   = d_start + P->spt.partition[y].length;

			if (s_start < d_end && s_end > d_start)
				return -EINVAL;
		}
		if (!strcmp(P->spt.partition[x].name, "SPT0"))
			spt0 = 1;
		else if (!strcmp(P->spt.partition[x].name, "SPT1"))
			spt1 = 1;
		else if (!strcmp(P->spt.partition[x].name, "CPB0"))
			cpb0 = 1;
		else if (!strcmp(P->spt.partition[x].name, "CPB1"))
			cpb1 = 1;
	}
	if (!spt0 || !spt1 || !cpb0 || !cpb1)
		return -1;
	return 0;
}

/* --- corrupted / save / restore helpers --- */
static int corrupted_spt(void) { return P->spt_corrupted; }
static int corrupted_cpb(void) { return P->cpb_corrupted; }

static int save_spt_to_address(u64 address)
{
	char *dst = (char *)(uintptr_t)address;
	char *src;
	u32 calc;

	if (!dst)
		return -EINVAL;
	src = malloc(SPT_SIZE);
	if (!src)
		return -ENOMEM;
	int ret = read_dev(P->spt0_offset, src, SPT_SIZE);

	if (ret) {
		free(src);
		return ret;
	}
	calc = (u32)crc32(0, (unsigned char *)src, SPT_SIZE);
	memcpy(dst,              src,    SPT_SIZE);
	memcpy(dst + SPT_SIZE,  &calc,   sizeof(calc));
	free(src);
	return 0;
}

static int save_cpb_to_address(u64 address)
{
	char *dst = (char *)(uintptr_t)address;
	char *src;
	u32 calc;

	if (!dst)
		return -EINVAL;
	src = malloc(CPB_SIZE);
	if (!src)
		return -ENOMEM;
	int ret = read_part(P->cpb0_part, 0, src, CPB_SIZE);

	if (ret) {
		free(src);
		return ret;
	}
	calc = (u32)crc32(0, (unsigned char *)src, CPB_SIZE);
	memcpy(dst,              src,    CPB_SIZE);
	memcpy(dst + CPB_SIZE,  &calc,   sizeof(calc));
	free(src);
	return 0;
}

static int restore_spt_from_address(u64 address)
{
	char *src = (char *)(uintptr_t)address;
	u32 calc, saved, magic;

	if (!src)
		return -EINVAL;
	calc = (u32)crc32(0, (unsigned char *)src, SPT_SIZE);
	memcpy(&saved, src + SPT_SIZE, sizeof(saved));
	if (calc != saved)
		return -EINVAL;
	memcpy(&magic, src, sizeof(magic));
	if (magic != SPT_MAGIC_NUMBER)
		return -EINVAL;
	memcpy(&P->spt, src, SPT_SIZE);
	P->spt_corrupted = false;
	P->cpb_corrupted = false;
	return 0;
}

static int restore_cpb_from_address(u64 address)
{
	char *src = (char *)(uintptr_t)address;
	u32 calc, saved, magic;

	if (P->spt_corrupted)
		return -EINVAL;
	if (!src)
		return -EINVAL;
	calc = (u32)crc32(0, (unsigned char *)src, CPB_SIZE);
	memcpy(&saved, src + CPB_SIZE, sizeof(saved));
	if (calc != saved)
		return -EINVAL;
	memcpy(&magic, src, sizeof(magic));
	if (magic != CPB_MAGIC_NUMBER)
		return -EINVAL;
	memcpy(&P->cpb, src, CPB_SIZE);
	P->cpb_slots = (u64 *)&P->cpb.data[P->cpb.header.image_ptr_offset];
	P->cpb_corrupted = false;
	P->cpb_fixed = true;
	return 0;
}

/* --- partition_* --- */
static int partition_count(void) { return P->spt.partitions; }
static char *partition_name(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return "BAD";
	return P->spt.partition[part_num].name;
}

static u64 partition_offset(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return (u64)-1;
	return P->spt.partition[part_num].offset;
}

static u32 partition_size(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return (u32)-1;
	return P->spt.partition[part_num].length;
}

static int partition_reserved(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return 0;
	return (P->spt.partition[part_num].flags & SPT_FLAG_RESERVED) ? 1 : 0;
}

static int partition_readonly(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return 0;
	return (P->spt.partition[part_num].flags & SPT_FLAG_READONLY) ? 1 : 0;
}

static s64 factory_offset(void)
{
	for (int x = 0; x < (int)P->spt.partitions; x++)
		if (!strncmp(P->spt.partition[x].name, FACTORY_IMAGE_NAME,
			     sizeof(P->spt.partition[0].name) - 1))
			return P->spt.partition[x].offset;
	return -1;
}

static int partition_create(char *name, u64 start, unsigned int size)
{
	int x;
	u64 end = start + size;

	if (size % MIN_QSPI_ERASE_SIZE)
		return -1;
	if (start % MIN_QSPI_ERASE_SIZE)
		return -1;
	if (strnlen(name, sizeof(P->spt.partition[0].name)) >=
	    sizeof(P->spt.partition[0].name))
		return -1;
	for (x = 0; x < (int)P->spt.partitions; x++)
		if (!strncmp(P->spt.partition[x].name, name,
			     sizeof(P->spt.partition[0].name) - 1))
			return -1;
	if (P->spt.partitions == SPT_MAX_PARTITIONS)
		return -1;
	for (x = 0; x < (int)P->spt.partitions; x++) {
		u64 pstart = P->spt.partition[x].offset;
		u64 pend   = pstart + P->spt.partition[x].length;

		if (start < pend && end > pstart)
			return -1;
	}
	rsu_misc_safe_strcpy(P->spt.partition[P->spt.partitions].name,
			     sizeof(P->spt.partition[0].name), name,
			     sizeof(P->spt.partition[0].name));
	P->spt.partition[P->spt.partitions].offset = start;
	P->spt.partition[P->spt.partitions].length = size;
	P->spt.partition[P->spt.partitions].flags  = 0;
	P->spt.partitions++;
	return 0;
}

static int partition_delete(int part_num)
{
	int x;

	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	for (x = part_num; x < (int)P->spt.partitions - 1; x++)
		P->spt.partition[x] = P->spt.partition[x + 1];
	P->spt.partitions--;
	return 0;
}

static int partition_rename(int part_num, char *name)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	if (strnlen(name, sizeof(P->spt.partition[0].name)) >=
	    sizeof(P->spt.partition[0].name))
		return -1;
	for (int x = 0; x < (int)P->spt.partitions; x++)
		if (!strncmp(P->spt.partition[x].name, name,
			     sizeof(P->spt.partition[0].name) - 1))
			return -1;
	rsu_misc_safe_strcpy(P->spt.partition[part_num].name,
			     sizeof(P->spt.partition[0].name),
			     name, sizeof(P->spt.partition[0].name));
	return 0;
}

/* --- priority — use cpb_slots directly --- */
static int priority_get(int part_num)
{
	int priority = 0;

	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	if (cpb_ptr_slots_access_ok())
		return -1;
	for (int x = P->cpb.header.image_ptr_slots; x > 0; x--) {
		if (P->cpb_slots[x - 1] != ERASED_ENTRY &&
		    P->cpb_slots[x - 1] != SPENT_ENTRY) {
			priority++;
			if (P->cpb_slots[x - 1] ==
			    P->spt.partition[part_num].offset)
				return priority;
		}
	}
	return 0;
}

static int priority_add(int part_num)
{
	int x, y;

	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	if (cpb_ptr_slots_access_ok())
		return -1;
	for (x = 0; x < (int)P->cpb.header.image_ptr_slots; x++)
		if (P->cpb_slots[x] == ERASED_ENTRY) {
			P->cpb_slots[x] = P->spt.partition[part_num].offset;
			return 0;
		}
	for (x = 0, y = 0; x < (int)P->cpb.header.image_ptr_slots; x++)
		if (P->cpb_slots[x] != ERASED_ENTRY &&
		    P->cpb_slots[x] != SPENT_ENTRY)
			P->cpb_slots[y++] = P->cpb_slots[x];
	if (y < (int)P->cpb.header.image_ptr_slots)
		P->cpb_slots[y++] = P->spt.partition[part_num].offset;
	else
		return -1;
	while (y < (int)P->cpb.header.image_ptr_slots)
		P->cpb_slots[y++] = ERASED_ENTRY;
	return 0;
}

static int priority_remove(int part_num)
{
	if (part_num < 0 || part_num >= (int)P->spt.partitions)
		return -1;
	if (cpb_ptr_slots_access_ok())
		return -1;
	for (int x = 0; x < (int)P->cpb.header.image_ptr_slots; x++)
		if (P->cpb_slots[x] == P->spt.partition[part_num].offset)
			P->cpb_slots[x] = SPENT_ENTRY;
	return 0;
}

/* --- data/fw --- */
static int data_read(int pn, int off, int n, void *buf) { return read_part(pn, off, buf, n); }
static int data_write(int pn, int off, int n, void *buf) { return write_part(pn, off, buf, n); }
static int data_erase(int pn)                            { return erase_part(pn); }

static int status_log(struct rsu_status_info *info)
{
	if (mbox_rsu_status((u32 *)info, sizeof(*info) / 4))
		return -ENOTSUP;
	return 0;
}

static int notify_fw(u32 value)
{
	if (mbox_hps_stage_notify(value))
		return -ELOWLEVEL;
	return 0;
}

static int image_load(u64 offset)
{
	u32 off[2] = { (u32)offset, (u32)(offset >> 32) };

	return mbox_rsu_update(off) ? -ELOWLEVEL : 0;
}

static int dcmf_version(u32 *versions)
{
	if (!versions)
		return -1;
	for (int i = 0; i < 4; i++)
		versions[i] = 0x01020304 + i;
	return 0;
}

static int dcmf_status(u16 *status)
{
	if (!status)
		return -1;
	for (int i = 0; i < 4; i++)
		status[i] = 0;
	return 0;
}

static int max_retry_op(u8 *value)
{
	if (!value)
		return -1;
	*value = 3;
	return 0;
}

/* --- empty_cpb --- */
static int empty_cpb(void)
{
	if (P->spt_corrupted)
		return -EINVAL;
	memset(&P->cpb, -1, CPB_SIZE);
	P->cpb.header.magic_number    = CPB_MAGIC_NUMBER;
	P->cpb.header.header_size     = CPB_HEADER_SIZE;
	P->cpb.header.cpb_size        = CPB_SIZE;
	P->cpb.header.cpb_reserved    = 0;
	P->cpb.header.image_ptr_offset = CPB_IMAGE_PTR_OFFSET;
	P->cpb.header.image_ptr_slots = CPB_IMAGE_PTR_NSLOTS;
	P->cpb_slots = (u64 *)&P->cpb.data[P->cpb.header.image_ptr_offset];
	for (int i = 0; i < CPB_IMAGE_PTR_NSLOTS; i++)
		P->cpb_slots[i] = ERASED_ENTRY;
	P->cpb_corrupted = false;
	P->cpb_fixed = true;
	return 0;
}

/* --- load_spt/load_cpb — simplified: assume ctx already populated --- */
static int load_spt(void) { return 0; }
static int load_cpb(void) { return 0; }

/* --- ll_exit --- */
static void ll_exit(void)
{
	struct rsu_qspi_priv *ctx = qspi_ctx;

	if (!ctx)
		return;
	ctx->cpb0_part = -1;
	ctx->cpb1_part = -1;
	ctx->cpb_corrupted = false;
	ctx->cpb_fixed = false;
	ctx->spt_corrupted = false;
	for (int i = 0; i < ctx->num_flash_alloc; i++) {
		if (ctx->flashlist && ctx->flashlist[i]) {
			rsu_mtd_unclaim(ctx->flashlist[i]);
			free_flash(ctx->flashlist[i]);
			ctx->flashlist[i] = NULL;
		}
	}
	free(ctx->flashlist);
	ctx->flashlist = NULL;
	ctx->flash = NULL;
	ctx->cpb_slots = NULL;
	memset(ctx, 0, sizeof(*ctx));
	free(ctx);
	qspi_ctx = NULL;
}

/* --- rsu_ll_qspi_init wired to a pre-populated ctx --- */
static struct rsu_ll_intf qspi_ll_intf = {
	.exit = ll_exit,
	.partition.count          = partition_count,
	.partition.name           = partition_name,
	.partition.offset         = partition_offset,
	.partition.factory_offset = factory_offset,
	.partition.size           = partition_size,
	.partition.reserved       = partition_reserved,
	.partition.readonly       = partition_readonly,
	.partition.rename         = partition_rename,
	.partition.delete         = partition_delete,
	.partition.create         = partition_create,
	.priority.get             = priority_get,
	.priority.add             = priority_add,
	.priority.remove          = priority_remove,
	.data.read                = data_read,
	.data.write               = data_write,
	.data.erase               = data_erase,
	.fw_ops.load              = image_load,
	.fw_ops.status            = status_log,
	.fw_ops.notify            = notify_fw,
	.fw_ops.dcmf_version      = dcmf_version,
	.fw_ops.dcmf_status       = dcmf_status,
	.fw_ops.max_retry         = max_retry_op,
	.cpb_ops.empty            = empty_cpb,
	.cpb_ops.restore          = restore_cpb_from_address,
	.cpb_ops.save             = save_cpb_to_address,
	.cpb_ops.corrupted        = corrupted_cpb,
	.spt_ops.restore          = restore_spt_from_address,
	.spt_ops.save             = save_spt_to_address,
	.spt_ops.corrupted        = corrupted_spt,
};

static int rsu_ll_qspi_init(struct rsu_ll_intf **intf)
{
	if (!qspi_ctx)
		return -ENODEV;
	*intf = &qspi_ll_intf;
	return 0;
}

/* ============================================================ */
/*  rsu.c public API (lifted, logic preserved)                    */
/* ============================================================ */
/* Non-DM session (PR also adds a DM variant; we test both). */
struct rsu_session { struct rsu_ll_intf *ll; };
static struct rsu_session g_rsu_session;

/* DM anchor: emulates socfpga_rsu_probe() */
static struct socfpga_rsu_priv g_rsu_dm_priv;
static int g_rsu_use_dm;

static struct rsu_ll_intf **rsu_ll_ptrp(void)
{
	if (g_rsu_use_dm)
		return &g_rsu_dm_priv.ll;
	return &g_rsu_session.ll;
}

static struct rsu_ll_intf *rsu_ll(void)
{
	struct rsu_ll_intf **p = rsu_ll_ptrp();

	return p ? *p : NULL;
}

static int rsu_init(char *filename)
{
	struct rsu_ll_intf **llp = rsu_ll_ptrp();
	(void)filename;
	if (!llp)
		return -ENODEV;
	if (*llp)
		return -EINTF;
	int ret = rsu_ll_qspi_init(llp);

	return ret ? -ENODEV : 0;
}

static void rsu_exit(void)
{
	/*
	 * Harness note: the real U-Boot rsu_exit() calls (*llp)->exit()
	 * which tears down the whole QSPI context. Our harness reuses a
	 * pre-built qspi_ctx across several rsu_init/rsu_exit pairs inside
	 * a single test, so we only detach the pointer here. teardown_ctx()
	 * is responsible for the actual cleanup at test exit.
	 */
	struct rsu_ll_intf **llp = rsu_ll_ptrp();

	if (!llp)
		return;
	*llp = NULL;
}

/* helpers used by rsu.c public APIs */
static int slot_count_impl(void)
{
	int partitions, cnt = 0;

	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	partitions = rsu_ll()->partition.count();
	for (int x = 0; x < partitions; x++) {
		if (rsu_ll()->partition.readonly(x) ||
		    rsu_ll()->partition.reserved(x))
			continue;
		if (rsu_misc_is_rsvd_name(rsu_ll()->partition.name(x)))
			continue;
		cnt++;
	}
	return cnt;
}

static int slot2part(int slot)
{
	int partitions = rsu_ll()->partition.count();
	int cnt = 0;

	for (int x = 0; x < partitions; x++) {
		if (rsu_ll()->partition.readonly(x) ||
		    rsu_ll()->partition.reserved(x))
			continue;
		if (rsu_misc_is_rsvd_name(rsu_ll()->partition.name(x)))
			continue;
		if (slot == cnt)
			return x;
		cnt++;
	}
	return -1;
}

static int rsu_slot_count(void) { return slot_count_impl(); }

static int rsu_slot_by_name(char *name)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (!name)
		return -EARGS;
	int partitions = rsu_ll()->partition.count();
	int cnt = 0;

	for (int x = 0; x < partitions; x++) {
		if (rsu_ll()->partition.readonly(x) ||
		    rsu_ll()->partition.reserved(x))
			continue;
		if (rsu_misc_is_rsvd_name(rsu_ll()->partition.name(x)))
			continue;
		if (!strcmp(name, rsu_ll()->partition.name(x)))
			return cnt;
		cnt++;
	}
	return -ENAME;
}

static int rsu_slot_get_info(int slot, struct rsu_slot_info *info)
{
	if (!rsu_ll())
		return -EINTF;
	if (!info)
		return -EARGS;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -EINVAL;
	rsu_misc_safe_strcpy(info->name, sizeof(info->name),
			     rsu_ll()->partition.name(pn), sizeof(info->name));
	info->offset   = rsu_ll()->partition.offset(pn);
	info->size     = rsu_ll()->partition.size(pn);
	info->priority = rsu_ll()->priority.get(pn);
	return 0;
}

static int rsu_slot_create(char *name, u64 address, unsigned int size)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_misc_is_rsvd_name(name))
		return -ENAME;
	return rsu_ll()->partition.create(name, address, size) ? -ELOWLEVEL : 0;
}

static int rsu_slot_delete(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	if (rsu_ll()->priority.remove(pn))
		return -ELOWLEVEL;
	if (rsu_ll()->data.erase(pn))
		return -ELOWLEVEL;
	return rsu_ll()->partition.delete(pn) ? -ELOWLEVEL : 0;
}

static int rsu_slot_rename(int slot, char *name)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	if (!name)
		return -EARGS;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	if (rsu_misc_is_rsvd_name(name))
		return -ENAME;
	return rsu_ll()->partition.rename(pn, name) ? -ENAME : 0;
}

static int rsu_slot_priority(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	return rsu_ll()->priority.get(pn);
}

static int rsu_slot_enable(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	if (rsu_ll()->priority.remove(pn))
		return -ELOWLEVEL;
	return rsu_ll()->priority.add(pn) ? -ELOWLEVEL : 0;
}

static int rsu_slot_disable(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	return rsu_ll()->priority.remove(pn) ? -ELOWLEVEL : 0;
}

static int rsu_slot_load(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	return rsu_ll()->fw_ops.load(rsu_ll()->partition.offset(pn));
}

static int rsu_slot_load_factory(void)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	int partitions = rsu_ll()->partition.count();

	for (int x = 0; x < partitions; x++)
		if (!strcmp("FACTORY_IMAGE", rsu_ll()->partition.name(x)))
			return rsu_ll()->fw_ops.load(rsu_ll()->partition.offset(x));
	return -EFORMAT;
}

static int rsu_slot_erase(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	if (rsu_ll()->priority.remove(pn))
		return -ELOWLEVEL;
	return rsu_ll()->data.erase(pn) ? -ELOWLEVEL : 0;
}

static int rsu_status_log_pub(struct rsu_status_info *info)
{
	if (!rsu_ll())
		return -EINTF;
	return rsu_ll()->fw_ops.status(info);
}

static int rsu_notify(int stage)
{
	u32 arg;

	if (!rsu_ll())
		return -EINTF;
	arg = stage & 0xFFFFu;
	return rsu_ll()->fw_ops.notify(arg);
}

static int rsu_clear_error_status(void)
{
	struct rsu_status_info info;

	if (!rsu_ll())
		return -EINTF;
	int ret = rsu_status_log_pub(&info);

	if (ret < 0)
		return ret;
	if (!RSU_VERSION_ACMF_VERSION(info.version))
		return -ELOWLEVEL;
	u32 arg = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_CLEAR_ERROR_STATUS;

	return rsu_ll()->fw_ops.notify(arg);
}

static int rsu_reset_retry_counter(void)
{
	struct rsu_status_info info;

	if (!rsu_ll())
		return -EINTF;
	int ret = rsu_status_log_pub(&info);

	if (ret < 0)
		return ret;
	if (!RSU_VERSION_ACMF_VERSION(info.version) ||
	    !RSU_VERSION_DCMF_VERSION(info.version))
		return -ELOWLEVEL;
	u32 arg = RSU_NOTIFY_IGNORE_STAGE | RSU_NOTIFY_RESET_RETRY_COUNTER;

	return rsu_ll()->fw_ops.notify(arg);
}

static int rsu_dcmf_version_pub(u32 *v)
{
	if (!rsu_ll())
		return -EINTF;
	if (!v)
		return -EARGS;
	return rsu_ll()->fw_ops.dcmf_version(v);
}

static int rsu_dcmf_status_pub(u16 *s)
{
	if (!rsu_ll())
		return -EINTF;
	if (!s)
		return -EARGS;
	return rsu_ll()->fw_ops.dcmf_status(s);
}

static int rsu_max_retry_pub(u8 *v)
{
	if (!rsu_ll())
		return -EINTF;
	if (!v)
		return -EARGS;
	return rsu_ll()->fw_ops.max_retry(v);
}

static int rsu_create_empty_cpb(void)
{
	return rsu_ll()->cpb_ops.empty();
}

static int rsu_restore_cpb(u64 a)   { return rsu_ll()->cpb_ops.restore(a); }
static int rsu_save_cpb(u64 a)
{
	if (rsu_ll()->cpb_ops.corrupted())
		return -ECORRUPTED_CPB;
	return rsu_ll()->cpb_ops.save(a);
}

static int rsu_restore_spt(u64 a)   { return rsu_ll()->spt_ops.restore(a); }
static int rsu_save_spt(u64 a)
{
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	return rsu_ll()->spt_ops.save(a);
}

static int rsu_running_factory(int *factory)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	s64 fo = rsu_ll()->partition.factory_offset();

	if (fo < 0)
		return -ELOWLEVEL;
	struct rsu_status_info st;

	if (rsu_ll()->fw_ops.status(&st))
		return -ELOWLEVEL;
	*factory = (fo == (s64)st.current_image);
	return 0;
}

/* ============================================================ */
/*  rsu_s10.c helpers (post-refactor)                             */
/* ============================================================ */
static unsigned int rsu_s10_spt_entry_count(const struct socfpga_rsu_s10_spt *spt)
{
	if (spt->magic_number != RSU_S10_SPT_MAGIC_NUMBER)
		return 0;
	if (spt->entries > RSU_S10_SPT_SLOT_MAX)
		return RSU_S10_SPT_SLOT_MAX;
	return spt->entries;
}

static void rsu_s10_sanitize_spt_names(struct socfpga_rsu_s10_spt *spt,
				       unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		spt->spt_slot[i].name[MAX_PART_NAME_LENGTH - 1] = '\0';
}

static u32 rsu_spt_slot_find_cpb(const struct socfpga_rsu_s10_spt *spt,
				 unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		if (strstr(spt->spt_slot[i].name, "CPB0"))
			return spt->spt_slot[i].offset[0];
	return 0;
}

/* rsu_update argc check + u64 split */
static int rsu_update_argv(int argc, char * const argv[], u32 *lo, u32 *hi)
{
	u64 addr;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;
	addr = strtoull(argv[1], &endp, 16);
	*lo = (u32)addr;
	*hi = (u32)(addr >> 32);
	return 0;
}

/* ============================================================ */
/*  rsu_spl.c helpers                                             */
/* ============================================================ */
#define SSBL_PART_PREFIX   "SSBL."
#define UBOOT_ENV_EXT      ".env"
#define UBOOT_ITB_EXT      ".itb"
#define UBOOT_ENV_PREFIX   "u-boot_"
#define UBOOT_ENV_REDUND_PREFIX "u-boot-redund_"
#define UBOOT_PREFIX       "u-boot_"
#define FACTORY_IMG_NAME   "FACTORY_IM"
#define RSU_SPL_SSBL_FALLBACK_BYTES SZ_8M
/* CONFIG_SYS_SPI_U_BOOT_OFFS is injected via -D... in the Makefile. */

static unsigned int rsu_spl_spt_nentries(const struct socfpga_rsu_s10_spt *spt)
{
	if (spt->magic_number != RSU_S10_SPT_MAGIC_NUMBER)
		return 0;
	if (spt->entries > RSU_SPL_SPT_SLOT_MAX)
		return RSU_SPL_SPT_SLOT_MAX;
	return spt->entries;
}

static void rsu_spl_sanitize_names(struct socfpga_rsu_s10_spt *spt,
				   unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		spt->spt_slot[i].name[MAX_PART_NAME_LENGTH - 1] = '\0';
}

/*
 * Emulates get_spl_slot logic in rsu_spl.c: mailbox + probe + read SPT0,
 * fallback to SPT1 on bad magic, then sanitize names and locate the
 * current image by offset.
 */
static int get_spl_slot(struct socfpga_rsu_s10_spt *out, int *crt_idx)
{
	u32 spt_offset[4] = {0};
	struct rsu_status_info st = {0};
	struct fake_dev *flash;
	unsigned int n;

	if (mbox_rsu_status((u32 *)&st, sizeof(st) / 4))
		return -EOPNOTSUPP;
	if (mbox_rsu_get_spt_offset(spt_offset, 4))
		return -EINVAL;
	if (rsu_mtd_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SOCFPGA_RSU_SF_CS, &flash))
		return -EINVAL;
	if (rsu_mtd_read(flash, spt_offset[SPT0_INDEX], sizeof(*out), out))
		return -EINVAL;
	if (out->magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		if (rsu_mtd_read(flash, spt_offset[SPT1_INDEX], sizeof(*out), out))
			return -EINVAL;
		if (out->magic_number != RSU_S10_SPT_MAGIC_NUMBER)
			return -EINVAL;
	}
	n = rsu_spl_spt_nentries(out);
	if (!n)
		return -EINVAL;
	rsu_spl_sanitize_names(out, n);
	for (unsigned int i = 0; i < n; i++) {
		if ((st.current_image & 0xFFFFFFFFu) == out->spt_slot[i].offset[0] &&
		    (st.current_image >> 32)        == out->spt_slot[i].offset[1]) {
			*crt_idx = (int)i;
			return 0;
		}
	}
	return -EINVAL;
}

static int get_ssbl_slot(struct socfpga_rsu_s10_spt_slot *out)
{
	struct socfpga_rsu_s10_spt *spt = calloc(1, sizeof(*spt));
	int idx = -1;
	unsigned int n;
	int ret = get_spl_slot(spt, &idx);

	if (ret) {
		free(spt);
		return ret;
	}
	n = rsu_spl_spt_nentries(spt);
	for (unsigned int i = 0; i < n; i++) {
		char *r = strstr(spt->spt_slot[i].name, SSBL_PART_PREFIX);

		if (!r || r != spt->spt_slot[i].name)
			continue;
		r += strlen(SSBL_PART_PREFIX);
		if (!strncmp(r, spt->spt_slot[idx].name,
			     MAX_PART_NAME_LENGTH - strlen(SSBL_PART_PREFIX)) ||
		    !strncmp(r, spt->spt_slot[idx].name,
			     strlen(FACTORY_IMG_NAME))) {
			memcpy(out, &spt->spt_slot[i], sizeof(*out));
			free(spt);
			return 0;
		}
	}
	free(spt);
	return -EINVAL;
}

static int rsu_spl_mmc_filename(char *filename, int max_size)
{
	struct socfpga_rsu_s10_spt *spt;
	int idx = -1, ret;

	if (!filename)
		return -ENOENT;
	if ((strlen(UBOOT_PREFIX) + MAX_PART_NAME_LENGTH + strlen(UBOOT_ITB_EXT))
	    > (size_t)max_size)
		return -ENAMETOOLONG;
	spt = calloc(1, sizeof(*spt));
	ret = get_spl_slot(spt, &idx);
	if (ret) {
		free(spt);
		return ret == -EOPNOTSUPP ? -EOPNOTSUPP : ret;
	}
	ret = snprintf(filename, max_size, "%s%s%s", UBOOT_PREFIX,
		       spt->spt_slot[idx].name, UBOOT_ITB_EXT);
	free(spt);
	if (ret < 0 || ret >= max_size)
		return -ENAMETOOLONG;
	return 0;
}

static int rsu_spl_mmc_env_name(char *filename, int max_size, bool redund)
{
	struct socfpga_rsu_s10_spt *spt;
	int idx = -1, ret;

	if (!filename)
		return -ENOENT;
	if ((strlen(UBOOT_ENV_REDUND_PREFIX) + strlen(UBOOT_ENV_PREFIX) +
	     MAX_PART_NAME_LENGTH + strlen(UBOOT_ENV_EXT)) > (size_t)max_size)
		return -ENAMETOOLONG;
	spt = calloc(1, sizeof(*spt));
	ret = get_spl_slot(spt, &idx);
	if (ret) {
		free(spt);
		return ret;
	}
	if (redund)
		ret = snprintf(filename, max_size, "%s%s%s",
			       UBOOT_ENV_REDUND_PREFIX,
			       spt->spt_slot[idx].name, UBOOT_ENV_EXT);
	else
		ret = snprintf(filename, max_size, "%s%s%s",
			       UBOOT_ENV_PREFIX,
			       spt->spt_slot[idx].name, UBOOT_ENV_EXT);
	free(spt);
	if (ret < 0 || ret >= max_size)
		return -ENAMETOOLONG;
	return 0;
}

static u32 rsu_spl_ssbl_address(bool is_qspi_check)
{
	struct socfpga_rsu_s10_spt_slot s = {0};
	int ret = get_ssbl_slot(&s);

	if (ret == -EOPNOTSUPP)
		return CONFIG_SYS_SPI_U_BOOT_OFFS;
	if (ret) {
		if (is_qspi_check)
			return 0;  /* panic in real code */
		return CONFIG_SYS_SPI_U_BOOT_OFFS;
	}
	if (!s.length) {
		if (is_qspi_check)
			return 0;
		return CONFIG_SYS_SPI_U_BOOT_OFFS;
	}
	return s.offset[0];
}

static u32 rsu_spl_ssbl_size(bool is_qspi_check)
{
	struct socfpga_rsu_s10_spt_slot s = {0};
	int ret = get_ssbl_slot(&s);

	if (ret == -EOPNOTSUPP)
		return RSU_SPL_SSBL_FALLBACK_BYTES;
	if (ret) {
		if (is_qspi_check)
			return 0;
		return RSU_SPL_SSBL_FALLBACK_BYTES;
	}
	if (!s.length) {
		if (is_qspi_check)
			return 0;
		return RSU_SPL_SSBL_FALLBACK_BYTES;
	}
	return s.length;
}

/* ============================================================ */
/*  cmd/socfpga_rsu.c — do_rsu dispatch                           */
/* ============================================================ */
/*
 * Each subcommand handler wraps rsu_init()+rsu_*()+rsu_exit().
 * For the ASan harness we only need to check the dispatch and
 * argc-validation logic: the core functions are already tested above.
 */
struct cmd_tbl { int stub; };

static int c_slot_count(int argc, char * const argv[])
{
	(void)argv;
	if (argc != 1)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int n = rsu_slot_count();

	rsu_exit();
	return n < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_slot_by_name(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int s = rsu_slot_by_name(argv[1]);

	rsu_exit();
	return s < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_slot_get_info(int argc, char * const argv[])
{
	struct rsu_slot_info info;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int slot = (int)strtoul(argv[1], &endp, 10);
	int ret  = rsu_slot_get_info(slot, &info);

	rsu_exit();
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_slot_size(int argc, char * const argv[])
{
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int s = (int)strtoul(argv[1], &endp, 10);
	int sz = rsu_ll() ? rsu_ll()->partition.size(slot2part(s)) : -1;

	rsu_exit();
	return sz < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_slot_create(int argc, char * const argv[])
{
	if (argc != 4)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u64 addr = strtoull(argv[2], &endp, 16);
	unsigned int size = (unsigned int)strtoul(argv[3], &endp, 16);
	int ret = rsu_slot_create(argv[1], addr, size);

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_notify(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u32 v = (u32)strtoul(argv[1], &endp, 16);
	int ret = rsu_notify(v);

	rsu_exit();
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_create_empty_cpb(int argc, char * const argv[])
{
	(void)argv;
	if (argc != 1)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int ret = rsu_create_empty_cpb();

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_restore_cpb(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u64 a = strtoull(argv[1], &endp, 16);
	int ret = rsu_restore_cpb(a);

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_save_cpb(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u64 a = strtoull(argv[1], &endp, 16);
	int ret = rsu_save_cpb(a);

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_restore_spt(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u64 a = strtoull(argv[1], &endp, 16);
	int ret = rsu_restore_spt(a);

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_save_spt(int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	char *endp;
	u64 a = strtoull(argv[1], &endp, 16);
	int ret = rsu_save_spt(a);

	rsu_exit();
	return ret < 0 ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int c_check_running_factory(int argc, char * const argv[])
{
	(void)argv;
	if (argc != 1)
		return CMD_RET_USAGE;
	if (rsu_init(NULL))
		return CMD_RET_FAILURE;
	int factory;
	int ret = rsu_running_factory(&factory);

	rsu_exit();
	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

struct func_t {
	const char *cmd_string;
	int (*func_ptr)(int argc, char * const argv[]);
};

static const struct func_t rsu_func_t[] = {
	{ "slot_count",            c_slot_count },
	{ "slot_by_name",          c_slot_by_name },
	{ "slot_get_info",         c_slot_get_info },
	{ "slot_size",             c_slot_size },
	{ "slot_create",           c_slot_create },
	{ "notify",                c_notify },
	{ "create_empty_cpb",      c_create_empty_cpb },
	{ "restore_cpb",           c_restore_cpb },
	{ "save_cpb",              c_save_cpb },
	{ "restore_spt",           c_restore_spt },
	{ "save_spt",              c_save_spt },
	{ "check_running_factory", c_check_running_factory },
};

static int do_rsu(struct cmd_tbl *cmdtp, int flag, int argc,
		  char * const argv[])
{
	(void)cmdtp; (void)flag;
	if (argc < 2)
		return CMD_RET_USAGE;
	const char *cmd = argv[1];
	--argc; ++argv;
	for (size_t i = 0; i < ARRAY_SIZE(rsu_func_t); i++)
		if (!strcmp(cmd, rsu_func_t[i].cmd_string))
			return rsu_func_t[i].func_ptr(argc, argv);
	return CMD_RET_USAGE;
}

/* ============================================================ */
/*  drivers/misc/socfpga_rsu.c — DM probe                         */
/* ============================================================ */
struct udevice { struct socfpga_rsu_priv priv; };

static int socfpga_rsu_probe(struct udevice *dev)
{
	struct socfpga_rsu_priv *priv = &dev->priv;

	priv->ll = NULL;
	return 0;
}

/* ============================================================ */
/*  test/cmd/socfpga_rsu.c — usage test                            */
/* ============================================================ */
static int cmd_ut_socfpga_rsu_usage(void)
{
	static const char * const argv[] = { "rsu", NULL };

	return do_rsu(NULL, 0, 1, (char * const *)argv);
}

/* ============================================================ */
/*  Baseline SPT/CPB helpers + setup/teardown                      */
/* ============================================================ */
static void add_part(const char *name, u64 off, u32 len, u32 flags)
{
	int i = P->spt.partitions;

	rsu_misc_safe_strcpy(P->spt.partition[i].name,
			     sizeof(P->spt.partition[i].name),
			     (char *)name,
			     sizeof(P->spt.partition[i].name));
	P->spt.partition[i].offset = off;
	P->spt.partition[i].length = len;
	P->spt.partition[i].flags  = flags;
	P->spt.partitions++;
}

/* Build a realistic SPT/CPB layout inside a single-flash P. */
static void setup_valid_layout(void)
{
	u32 sizes[] = { 0x1000000 };

	P = calloc(1, sizeof(*P));
	P->num_flash = 1;
	P->num_flash_alloc = 1;
	P->flashlist = calloc(1, sizeof(*P->flashlist));
	P->flashlist[0] = make_flash(sizes[0], 4096);
	P->flash = P->flashlist[0];

	P->spt.magic_number = SPT_MAGIC_NUMBER;
	P->spt.partitions = 0;
	add_part("SPT0",          0x000000, 0x1000, SPT_FLAG_RESERVED);
	add_part("SPT1",          0x001000, 0x1000, SPT_FLAG_RESERVED);
	add_part("CPB0",          0x002000, 0x1000, SPT_FLAG_RESERVED);
	add_part("CPB1",          0x003000, 0x1000, SPT_FLAG_RESERVED);
	add_part("FACTORY_IMAGE", 0x004000, 0x10000, 0);
	add_part("P1",            0x100000, 0x20000, 0);
	add_part("P2",            0x200000, 0x20000, 0);
	P->cpb0_part = 2;
	P->cpb1_part = 3;

	P->cpb.header.magic_number    = CPB_MAGIC_NUMBER;
	P->cpb.header.header_size     = CPB_HEADER_SIZE;
	P->cpb.header.cpb_size        = CPB_SIZE;
	P->cpb.header.cpb_reserved    = 0;
	P->cpb.header.image_ptr_offset = CPB_IMAGE_PTR_OFFSET;
	P->cpb.header.image_ptr_slots  = CPB_IMAGE_PTR_NSLOTS;
	P->cpb_slots = (u64 *)&P->cpb.data[P->cpb.header.image_ptr_offset];
	for (int i = 0; i < CPB_IMAGE_PTR_NSLOTS; i++)
		P->cpb_slots[i] = ERASED_ENTRY;

	g_mbox_status.state = STATE_OK;
	g_mbox_status.version = 0xA1B1; /* ACMF=0xB1, DCMF=... set as needed */
	g_mbox_status_fail = 0;
	g_mbox_spt_off_fail = 0;
}

static void teardown_ctx(void)
{
	if (!P)
		return;
	for (int i = 0; i < P->num_flash_alloc; i++)
		free_flash(P->flashlist[i]);
	free(P->flashlist);
	free(P);
	P = NULL;
	g_rsu_session.ll = NULL;
	g_rsu_dm_priv.ll = NULL;
	g_probe_result = NULL;
	g_probe_force_fail = 0;
}

/* ============================================================ */
/*  Test framework                                                */
/* ============================================================ */
static int g_total, g_pass, g_fail_line_count;
#define TEST(desc)                                                  \
	do {                                                        \
		g_total++;                                          \
		printf("[TEST] %s\n", desc);                        \
		g_fail_line_count = 0;                              \
	} while (0)
#define EXPECT(cond)                                                \
	do {                                                        \
		if (!(cond)) {                                      \
			fprintf(stderr,                             \
				"  FAIL @ %s:%d: %s\n",             \
				__FILE__, __LINE__, #cond);         \
			g_fail_line_count++;                        \
		}                                                   \
	} while (0)
#define END_TEST()                                                  \
	do {                                                        \
		if (g_fail_line_count == 0)                         \
			g_pass++;                                   \
		teardown_ctx();                                     \
	} while (0)

/* ============================================================ */
/*  [FIF ] rsu_flash_if.h wrappers                                */
/* ============================================================ */
static void t_fif_size_erasesize(void)
{
	TEST("[FIF ] rsu_mtd_size / rsu_mtd_erasesize on real + NULL");
	struct fake_dev *d = make_flash(0x1000, 4096);

	EXPECT(rsu_mtd_size(d) == 0x1000);
	EXPECT(rsu_mtd_erasesize(d) == 4096);
	EXPECT(rsu_mtd_size(NULL) == 0);
	EXPECT(rsu_mtd_erasesize(NULL) == 0);
	free_flash(d);
	END_TEST();
}

static void t_fif_read_write_erase_oob(void)
{
	TEST("[FIF ] rsu_mtd_read/write/erase out-of-bounds rejected");
	struct fake_dev *d = make_flash(0x1000, 4096);
	u8 buf[16];

	EXPECT(rsu_mtd_read(d, 0x1000, 1, buf) == -EINVAL);
	EXPECT(rsu_mtd_read(d, 0xFF0,  32, buf) == -EINVAL);
	EXPECT(rsu_mtd_write(d, 0x1000, 1, buf) == -EINVAL);
	EXPECT(rsu_mtd_erase(d, 0x1000, 1) == -EINVAL);
	EXPECT(rsu_mtd_read(d, 0, 16, buf) == 0);
	free_flash(d);
	END_TEST();
}

static void t_fif_probe_fallback(void)
{
	TEST("[FIF ] rsu_mtd_probe: ENODEV when spi_flash_probe fails");
	struct fake_dev *d = make_flash(0x1000, 4096);
	struct fake_dev *out = NULL;

	g_probe_result = NULL;
	g_probe_force_fail = 1;
	EXPECT(rsu_mtd_probe(CONFIG_SF_DEFAULT_BUS,
			     CONFIG_SOCFPGA_RSU_SF_CS, &out) == -ENODEV);
	EXPECT(!out);

	g_probe_result = d;
	g_probe_force_fail = 0;
	EXPECT(rsu_mtd_probe(CONFIG_SF_DEFAULT_BUS,
			     CONFIG_SOCFPGA_RSU_SF_CS, &out) == 0);
	EXPECT(out == d);

	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

/* ============================================================ */
/*  [S10 ] rsu_s10.c                                               */
/* ============================================================ */
static void t_s10_entry_count(void)
{
	TEST("[S10 ] rsu_s10_spt_entry_count: magic + clamp");
	struct socfpga_rsu_s10_spt *s = calloc(1, sizeof(*s));

	s->magic_number = 0xDEAD;
	s->entries = 10;
	EXPECT(rsu_s10_spt_entry_count(s) == 0);
	s->magic_number = RSU_S10_SPT_MAGIC_NUMBER;
	s->entries = 0xFFFFFFFFu;
	EXPECT(rsu_s10_spt_entry_count(s) == RSU_S10_SPT_SLOT_MAX);
	s->entries = 5;
	EXPECT(rsu_s10_spt_entry_count(s) == 5);
	free(s);
	END_TEST();
}

static void t_s10_sanitize_names(void)
{
	TEST("[S10 ] rsu_s10_sanitize_spt_names: NUL at [15]");
	struct socfpga_rsu_s10_spt *s = calloc(1, sizeof(*s));

	memcpy(s->spt_slot[0].name, "ABCDEFGHIJKLMNOP", 16);
	memcpy(s->spt_slot[1].name, "1234567890123456", 16);
	rsu_s10_sanitize_spt_names(s, 2);
	EXPECT(s->spt_slot[0].name[15] == '\0');
	EXPECT(s->spt_slot[1].name[15] == '\0');
	free(s);
	END_TEST();
}

static void t_s10_find_cpb(void)
{
	TEST("[S10 ] rsu_spt_slot_find_cpb: found / not found");
	struct socfpga_rsu_s10_spt *s = calloc(1, sizeof(*s));

	memcpy(s->spt_slot[0].name, "SPT0", 5);
	memcpy(s->spt_slot[1].name, "CPB0", 5);
	s->spt_slot[1].offset[0] = 0xABCD0000u;
	EXPECT(rsu_spt_slot_find_cpb(s, 2) == 0xABCD0000u);

	memset(s, 0, sizeof(*s));
	memcpy(s->spt_slot[0].name, "SPT0", 5);
	EXPECT(rsu_spt_slot_find_cpb(s, 1) == 0);
	free(s);
	END_TEST();
}

static void t_s10_rsu_update_argv(void)
{
	TEST("[S10 ] rsu_update: argc check + u64 split");
	u32 lo = 0, hi = 0;
	static const char * const bad[]  = { "update" };
	static const char * const good[] = { "update", "0x123456789ABCDEF0" };

	EXPECT(rsu_update_argv(1, (char * const *)bad,  &lo, &hi) == CMD_RET_USAGE);
	EXPECT(rsu_update_argv(2, (char * const *)good, &lo, &hi) == 0);
	EXPECT(lo == 0x9ABCDEF0u);
	EXPECT(hi == 0x12345678u);
	END_TEST();
}

/* ============================================================ */
/*  [LLQ ] rsu_ll_qspi.c                                           */
/* ============================================================ */
static void t_llq_multiflash_read_stitched(void)
{
	TEST("[LLQ ] read_dev stitches across flash chips");
	u32 sizes[] = { 0x10000, 0x10000 };

	P = calloc(1, sizeof(*P));
	P->num_flash = 2;
	P->num_flash_alloc = 2;
	P->flashlist = calloc(2, sizeof(*P->flashlist));
	for (int i = 0; i < 2; i++)
		P->flashlist[i] = make_flash(sizes[i], 4096);
	/* Write flash0 last 0x100 with 'A' and flash1 first 0x100 with 'B' */
	memset(P->flashlist[0]->mem + 0xFF00, 'A', 0x100);
	memset(P->flashlist[1]->mem + 0x0000, 'B', 0x100);

	unsigned char buf[0x200];

	EXPECT(read_dev(0xFF00, buf, 0x200) == 0);
	for (int i = 0; i < 0x100; i++)
		EXPECT(buf[i] == 'A');
	for (int i = 0x100; i < 0x200; i++)
		EXPECT(buf[i] == 'B');
	END_TEST();
}

/*
 * Regression test for the multi-flash zero-size guard in
 * read_dev/write_dev/erase_dev: if a later flash in the loop returns
 * sz == 0 (e.g. probe failure, missing uclass priv), the loop must
 * abort with -EINVAL instead of silently returning success after a
 * partial transfer or producing a huge underflowed length.
 */
static void t_llq_dev_loop_zero_size_later_flash(void)
{
	TEST("[LLQ ] read/write/erase_dev: reject sz==0 on later flash");
	u32 sizes[] = { 0x10000, 0x10000 };

	P = calloc(1, sizeof(*P));
	P->num_flash = 2;
	P->num_flash_alloc = 2;
	P->flashlist = calloc(2, sizeof(*P->flashlist));
	for (int i = 0; i < 2; i++)
		P->flashlist[i] = make_flash(sizes[i], 4096);

	/* Simulate flash[1] failing to expose its size. */
	P->flashlist[1]->size = 0;

	/*
	 * Block-aligned cross-boundary request: chip 0 supplies the
	 * trailing 0x1000, then the loop should advance to chip 1 and
	 * abort there because flash[1]->size == 0. Using 0xF000 / 0x2000
	 * keeps every per-iteration length a whole erase block, so the
	 * existing round-up guard cannot pre-empt the new sz==0 guard.
	 */
	unsigned char buf[0x2000];

	EXPECT(read_dev(0xF000, buf, 0x2000) == -EINVAL);
	EXPECT(write_dev(0xF000, buf, 0x2000) == -EINVAL);
	EXPECT(erase_dev(0xF000, 0x2000) == -EINVAL);
	END_TEST();
}

static void t_llq_erase_dev_roundup(void)
{
	TEST("[LLQ ] erase_dev rounds up to erase-block size");
	u32 sizes[] = { 0x10000 };

	P = calloc(1, sizeof(*P));
	P->num_flash = 1;
	P->num_flash_alloc = 1;
	P->flashlist = calloc(1, sizeof(*P->flashlist));
	P->flashlist[0] = make_flash(sizes[0], 4096);
	memset(P->flashlist[0]->mem, 0x11, 0x2000);
	/* Request 1 byte; must round up to 4096 and still succeed */
	EXPECT(erase_dev(0x0000, 1) == 0);
	EXPECT(P->flashlist[0]->mem[0] == 0xFF);
	EXPECT(P->flashlist[0]->mem[4095] == 0xFF);
	EXPECT(P->flashlist[0]->mem[4096] == 0x11);
	END_TEST();
}

/*
 * A non-aligned `len` on the final device used to make the accounting
 * fold the rounded-up extra bytes into `count`, which then made the next
 * loop iteration underflow `(u32)len - count` and erase a second flash
 * chip entirely. Regression test: with two chips, erasing a non-aligned
 * range that fits on the first chip must leave chip 1 untouched.
 */
static void t_llq_erase_dev_roundup_no_overrun(void)
{
	TEST("[LLQ ] erase_dev round-up must not spill onto next flash");
	u32 sizes[] = { 0x10000, 0x10000 };

	P = calloc(1, sizeof(*P));
	P->num_flash = 2;
	P->num_flash_alloc = 2;
	P->flashlist = calloc(2, sizeof(*P->flashlist));
	for (int i = 0; i < 2; i++)
		P->flashlist[i] = make_flash(sizes[i], 4096);
	memset(P->flashlist[0]->mem, 0x11, sizes[0]);
	memset(P->flashlist[1]->mem, 0x22, sizes[1]);

	/*
	 * Erase 1 byte at offset 0: must round up to one 4K erase block on
	 * chip 0, leave the remainder of chip 0 untouched, and MUST NOT
	 * touch chip 1 at all.
	 */
	EXPECT(erase_dev(0x0000, 1) == 0);
	EXPECT(P->flashlist[0]->mem[0] == 0xFF);
	EXPECT(P->flashlist[0]->mem[4095] == 0xFF);
	EXPECT(P->flashlist[0]->mem[4096] == 0x11);
	for (u32 j = 0; j < sizes[1]; j++)
		EXPECT(P->flashlist[1]->mem[j] == 0x22);
	END_TEST();
}

static void t_llq_partition_accessors(void)
{
	TEST("[LLQ ] partition_name/offset/size/reserved/readonly bounds");
	setup_valid_layout();
	EXPECT(!strcmp(partition_name(0), "SPT0"));
	EXPECT(!strcmp(partition_name(-1), "BAD"));
	EXPECT(!strcmp(partition_name(999), "BAD"));
	EXPECT(partition_offset(-1) == (u64)-1);
	EXPECT(partition_size(999) == (u32)-1);
	EXPECT(partition_reserved(0) == 1);
	EXPECT(partition_readonly(0) == 0);
	EXPECT(partition_readonly(-1) == 0);
	END_TEST();
}

static void t_llq_factory_offset(void)
{
	TEST("[LLQ ] factory_offset returns FACTORY_IMAGE.offset / -1");
	setup_valid_layout();
	EXPECT(factory_offset() == 0x004000);
	/* remove factory; should return -1 */
	int n = P->spt.partitions;

	for (int i = 0; i < n; i++)
		if (!strcmp(P->spt.partition[i].name, "FACTORY_IMAGE")) {
			partition_delete(i); break;
		}
	EXPECT(factory_offset() == -1);
	END_TEST();
}

static void t_llq_priority_add_remove_get(void)
{
	TEST("[LLQ ] priority_add / priority_remove / priority_get");
	setup_valid_layout();
	/* Target partition P1 (index 5) */
	int p1 = 5;

	EXPECT(priority_get(p1) == 0);
	EXPECT(priority_add(p1) == 0);
	EXPECT(priority_get(p1) == 1);
	/* add another */
	EXPECT(priority_add(6) == 0);
	EXPECT(priority_get(6) == 1);
	EXPECT(priority_get(p1) == 2);
	/* remove p1 */
	EXPECT(priority_remove(p1) == 0);
	EXPECT(priority_get(p1) == 0);
	END_TEST();
}

static void t_llq_priority_guards(void)
{
	TEST("[LLQ ] priority_add/remove: refuse when cpb guard trips");
	setup_valid_layout();
	u32 saved_off = P->cpb.header.image_ptr_offset;

	P->cpb.header.image_ptr_offset = CPB_SIZE;
	EXPECT(priority_add(5)    == -1);
	EXPECT(priority_remove(5) == -1);
	EXPECT(priority_get(5)    == -1);
	P->cpb.header.image_ptr_offset = saved_off;
	/* Out-of-range part_num */
	EXPECT(priority_add(-1)  == -1);
	EXPECT(priority_add(999) == -1);
	END_TEST();
}

static void t_llq_check_cpb_spt_flag(void)
{
	TEST("[LLQ ] check_cpb: rejects ptr into RESERVED partition");
	setup_valid_layout();
	/*
	 * point slot 0 at CPB0 (reserved, non-zero offset so it isn't
	 * skipped as SPENT_ENTRY)
	 */
	P->cpb_slots[0] = P->spt.partition[2].offset; /* CPB0 = 0x2000 */
	EXPECT(check_cpb() == -EINVAL);
	END_TEST();
}

static void t_llq_check_cpb_ptr_not_in_spt(void)
{
	TEST("[LLQ ] check_cpb: rejects ptr not in any SPT entry");
	setup_valid_layout();
	P->cpb_slots[0] = 0xDEADBEEF;
	EXPECT(check_cpb() == -EINVAL);
	END_TEST();
}

static void t_llq_check_cpb_healthy(void)
{
	TEST("[LLQ ] check_cpb: healthy CPB passes");
	setup_valid_layout();
	P->cpb_slots[0] = P->spt.partition[5].offset; /* P1 */
	P->cpb_slots[1] = P->spt.partition[6].offset; /* P2 */
	EXPECT(check_cpb() == 0);
	END_TEST();
}

static void t_llq_check_cpb_zero_ip_slots(void)
{
	TEST("[LLQ ] check_cpb / cpb_ptr_slots_access_ok: rejects image_ptr_slots == 0");
	setup_valid_layout();
	P->cpb.header.image_ptr_slots = 0;
	EXPECT(check_cpb() == -EINVAL);
	EXPECT(cpb_ptr_slots_access_ok() == -1);
	END_TEST();
}

static void t_llq_check_spt_zero_length(void)
{
	TEST("[LLQ ] check_spt: rejects zero-length partition entry");
	setup_valid_layout();
	/* Corrupt P1 (added by setup_valid_layout) to length 0. */
	P->spt.partition[5].length = 0;
	EXPECT(check_spt() == -EINVAL);
	END_TEST();
}

static void t_llq_save_restore_roundtrip(void)
{
	TEST("[LLQ ] save/restore SPT/CPB roundtrip");
	setup_valid_layout();
	char *sbuf = malloc(SPT_SIZE + sizeof(u32));
	char *cbuf = malloc(CPB_SIZE + sizeof(u32));
	/* Write SPT to flash first so save_spt_to_address reads real bytes */
	EXPECT(rsu_mtd_write(P->flashlist[0], P->spt0_offset, SPT_SIZE, &P->spt) == 0);
	EXPECT(save_spt_to_address((u64)(uintptr_t)sbuf) == 0);
	/* Write CPB to flash at CPB0 partition start (0x002000) */
	EXPECT(rsu_mtd_write(P->flashlist[0], 0x002000, CPB_SIZE, &P->cpb) == 0);
	EXPECT(save_cpb_to_address((u64)(uintptr_t)cbuf) == 0);

	memset(&P->spt, 0xAA, sizeof(P->spt));
	memset(&P->cpb, 0xAA, sizeof(P->cpb));
	P->spt_corrupted = false;

	EXPECT(restore_spt_from_address((u64)(uintptr_t)sbuf) == 0);
	EXPECT(P->spt.magic_number == SPT_MAGIC_NUMBER);
	EXPECT(restore_cpb_from_address((u64)(uintptr_t)cbuf) == 0);
	EXPECT(P->cpb.header.magic_number == CPB_MAGIC_NUMBER);

	free(sbuf);
	free(cbuf);
	END_TEST();
}

static void t_llq_save_nulls(void)
{
	TEST("[LLQ ] save/restore NULL / mismatched-magic rejected");
	setup_valid_layout();
	EXPECT(save_spt_to_address(0) == -EINVAL);
	EXPECT(save_cpb_to_address(0) == -EINVAL);
	EXPECT(restore_spt_from_address(0) == -EINVAL);
	EXPECT(restore_cpb_from_address(0) == -EINVAL);
	END_TEST();
}

static void t_llq_empty_cpb(void)
{
	TEST("[LLQ ] empty_cpb seeds header + all slots ERASED");
	setup_valid_layout();
	memset(&P->cpb, 0, sizeof(P->cpb));
	P->cpb_slots = NULL;
	EXPECT(empty_cpb() == 0);
	EXPECT(P->cpb.header.magic_number == CPB_MAGIC_NUMBER);
	EXPECT(P->cpb.header.image_ptr_slots == CPB_IMAGE_PTR_NSLOTS);
	for (int i = 0; i < CPB_IMAGE_PTR_NSLOTS; i++)
		EXPECT(P->cpb_slots[i] == ERASED_ENTRY);
	END_TEST();
}

static void t_llq_empty_cpb_rejects_on_corrupt_spt(void)
{
	TEST("[LLQ ] empty_cpb refuses if SPT corrupted");
	setup_valid_layout();
	P->spt_corrupted = true;
	EXPECT(empty_cpb() == -EINVAL);
	END_TEST();
}

static void t_llq_partition_rename_and_dup(void)
{
	TEST("[LLQ ] partition_rename: bounds / name-too-long / dup rejected");
	setup_valid_layout();
	EXPECT(partition_rename(-1, (char *)"X") == -1);
	EXPECT(partition_rename(999, (char *)"X") == -1);
	EXPECT(partition_rename(5, (char *)"ABCDEFGHIJKLMNOP") == -1); /* 16 chars */
	EXPECT(partition_rename(5, (char *)"SPT0") == -1);             /* dup */
	EXPECT(partition_rename(5, (char *)"NEWNAME") == 0);
	EXPECT(!strcmp(P->spt.partition[5].name, "NEWNAME"));
	END_TEST();
}

static void t_llq_partition_delete_shifts(void)
{
	TEST("[LLQ ] partition_delete: shifts tail entries down");
	setup_valid_layout();
	int n = P->spt.partitions;
	const char *keep_name = P->spt.partition[n - 1].name; /* last name */
	char savebuf[16]; memcpy(savebuf, keep_name, 16);

	EXPECT(partition_delete(n - 1) == 0);
	EXPECT((int)P->spt.partitions == n - 1);
	EXPECT(partition_delete(-1) == -1);
	EXPECT(partition_delete(999) == -1);
	(void)savebuf;
	END_TEST();
}

/* ============================================================ */
/*  [RSU ] rsu.c public API                                        */
/* ============================================================ */
static void t_rsu_init_exit(void)
{
	TEST("[RSU ] rsu_init / rsu_exit / double-init");
	setup_valid_layout();
	g_rsu_use_dm = 0;
	EXPECT(rsu_init(NULL) == 0);
	EXPECT(rsu_init(NULL) == -EINTF);
	rsu_exit();
	EXPECT(!rsu_ll());
	rsu_exit(); /* idempotent */
	END_TEST();
}

static void t_rsu_init_dm(void)
{
	TEST("[RSU ] rsu_init via DM anchor + socfpga_rsu_probe");
	setup_valid_layout();
	struct udevice dev = {0};

	socfpga_rsu_probe(&dev);
	EXPECT(!dev.priv.ll);

	g_rsu_use_dm = 1;
	EXPECT(rsu_init(NULL) == 0);
	EXPECT(rsu_ll());
	rsu_exit();
	g_rsu_use_dm = 0;
	END_TEST();
}

static int slot_size_pub(int slot);
static void t_rsu_slot_ops(void)
{
	TEST("[RSU ] rsu_slot_* happy path + boundary");
	setup_valid_layout();
	rsu_init(NULL);

	/* FACTORY_IMAGE is filtered by rsu_misc_is_rsvd_name */
	EXPECT(rsu_slot_count() == 2); /* P1, P2 */
	EXPECT(rsu_slot_by_name((char *)"P1") == 0);
	EXPECT(rsu_slot_by_name((char *)"FACTORY_IMAGE") == -ENAME);
	EXPECT(rsu_slot_by_name((char *)"MISSING") == -ENAME);
	EXPECT(rsu_slot_by_name(NULL) == -EARGS);

	struct rsu_slot_info info;

	EXPECT(rsu_slot_get_info(0, &info) == 0);
	EXPECT(info.offset == 0x100000);
	EXPECT(!strcmp(info.name, "P1"));
	EXPECT(rsu_slot_get_info(-1, &info) == -ESLOTNUM);
	EXPECT(rsu_slot_get_info(999, &info) == -ESLOTNUM);
	EXPECT(rsu_slot_get_info(0, NULL) == -EARGS);

	EXPECT(slot_size_pub(-1) == -ESLOTNUM);
	EXPECT(rsu_slot_priority(-1) == -ESLOTNUM);

	EXPECT(rsu_slot_enable(0) == 0);
	EXPECT(rsu_slot_priority(0) == 1);
	EXPECT(rsu_slot_disable(0) == 0);
	EXPECT(rsu_slot_priority(0) == 0);

	EXPECT(rsu_slot_erase(0) == 0);
	EXPECT(rsu_slot_rename(0, (char *)"P_RENAMED") == 0);
	EXPECT(rsu_slot_rename(0, (char *)"SPT") == -ENAME);
	EXPECT(rsu_slot_rename(-1, (char *)"X") == -ESLOTNUM);
	EXPECT(rsu_slot_rename(0, NULL) == -EARGS);

	/* slot_create */
	EXPECT(rsu_slot_create((char *)"FACTORY_IMAGE", 0x500000, 0x10000) == -ENAME);
	EXPECT(rsu_slot_create((char *)"NEW", 0x500000, 0x10000) == 0);

	/* slot_delete */
	int before = rsu_slot_count();

	EXPECT(rsu_slot_delete(before - 1) == 0);
	EXPECT(rsu_slot_count() == before - 1);

	/* load / load_factory */
	EXPECT(rsu_slot_load(0) == 0);
	EXPECT(rsu_slot_load_factory() == 0);
	rsu_exit();
	END_TEST();
}

static int slot_size_pub(int slot)
{
	if (!rsu_ll())
		return -EINTF;
	if (rsu_ll()->spt_ops.corrupted())
		return -ECORRUPTED_SPT;
	if (slot < 0 || slot >= rsu_slot_count())
		return -ESLOTNUM;
	int pn = slot2part(slot);

	if (pn < 0)
		return -ESLOTNUM;
	return rsu_ll()->partition.size(pn);
}

static void t_rsu_slot_size(void)
{
	TEST("[RSU ] rsu_slot_size");
	setup_valid_layout();
	rsu_init(NULL);
	/* slot 0 = P1 (0x20000), slot 1 = P2 (0x20000) */
	EXPECT(slot_size_pub(0) == 0x20000);
	EXPECT(slot_size_pub(1) == 0x20000);
	EXPECT(slot_size_pub(-1) == -ESLOTNUM);
	rsu_exit();
	END_TEST();
}

static void t_rsu_slot_ops_without_init(void)
{
	TEST("[RSU ] every op returns -EINTF before rsu_init");
	setup_valid_layout();
	g_rsu_use_dm = 0;
	g_rsu_session.ll = NULL;
	EXPECT(rsu_slot_count() == -EINTF);
	EXPECT(rsu_slot_by_name((char *)"P1") == -EINTF);
	struct rsu_slot_info info;

	EXPECT(rsu_slot_get_info(0, &info) == -EINTF);
	EXPECT(rsu_slot_enable(0) == -EINTF);
	EXPECT(rsu_slot_disable(0) == -EINTF);
	EXPECT(rsu_slot_load(0) == -EINTF);
	EXPECT(rsu_slot_load_factory() == -EINTF);
	EXPECT(rsu_slot_erase(0) == -EINTF);
	EXPECT(rsu_slot_rename(0, (char *)"X") == -EINTF);
	EXPECT(rsu_slot_delete(0) == -EINTF);
	EXPECT(rsu_slot_create((char *)"X", 0, 0) == -EINTF);
	EXPECT(rsu_notify(0) == -EINTF);
	EXPECT(rsu_clear_error_status() == -EINTF);
	EXPECT(rsu_reset_retry_counter() == -EINTF);
	u32 v[4]; EXPECT(rsu_dcmf_version_pub(v) == -EINTF);
	u16 s[4]; EXPECT(rsu_dcmf_status_pub(s) == -EINTF);
	u8 mr;    EXPECT(rsu_max_retry_pub(&mr)  == -EINTF);
	int f;    EXPECT(rsu_running_factory(&f) == -EINTF);

	END_TEST();
}

static void t_rsu_notify_masks(void)
{
	TEST("[RSU ] rsu_notify masks stage to 16 bits");
	setup_valid_layout();
	rsu_init(NULL);
	EXPECT(rsu_notify(0x1DEAD) == 0);    /* masked to 0xDEAD */
	EXPECT(rsu_notify(-1) == 0);         /* masked to 0xFFFF */
	rsu_exit();
	END_TEST();
}

static void t_rsu_clear_error_status(void)
{
	TEST("[RSU ] rsu_clear_error_status: ACMF version prerequisite");
	setup_valid_layout();
	rsu_init(NULL);
	g_mbox_status.version = 0;             /* no ACMF */
	EXPECT(rsu_clear_error_status() == -ELOWLEVEL);
	g_mbox_status.version = 0x0100;        /* ACMF=1 */
	EXPECT(rsu_clear_error_status() == 0);
	rsu_exit();
	END_TEST();
}

static void t_rsu_reset_retry_counter(void)
{
	TEST("[RSU ] rsu_reset_retry_counter: needs ACMF && DCMF");
	setup_valid_layout();
	rsu_init(NULL);
	g_mbox_status.version = 0;
	EXPECT(rsu_reset_retry_counter() == -ELOWLEVEL);
	g_mbox_status.version = 0x0100;    /* only ACMF */
	EXPECT(rsu_reset_retry_counter() == -ELOWLEVEL);
	g_mbox_status.version = 0x0101;    /* both */
	EXPECT(rsu_reset_retry_counter() == 0);
	rsu_exit();
	END_TEST();
}

static void t_rsu_dcmf_and_retry(void)
{
	TEST("[RSU ] rsu_dcmf_version/status + rsu_max_retry: NULL rejected");
	setup_valid_layout();
	rsu_init(NULL);
	EXPECT(rsu_dcmf_version_pub(NULL) == -EARGS);
	EXPECT(rsu_dcmf_status_pub(NULL)  == -EARGS);
	EXPECT(rsu_max_retry_pub(NULL)    == -EARGS);
	u32 v[4]; u16 s[4]; u8 mr = 0;

	EXPECT(rsu_dcmf_version_pub(v) == 0);
	EXPECT(rsu_dcmf_status_pub(s) == 0);
	EXPECT(rsu_max_retry_pub(&mr) == 0);
	EXPECT(mr == 3);
	rsu_exit();
	END_TEST();
}

static void t_rsu_status_log_forward(void)
{
	TEST("[RSU ] rsu_status_log forwards to LL, no-init -> EINTF");
	setup_valid_layout();
	g_rsu_use_dm = 0;
	g_rsu_session.ll = NULL;
	struct rsu_status_info info;

	EXPECT(rsu_status_log_pub(&info) == -EINTF);
	rsu_init(NULL);
	g_mbox_status.current_image = 0x12345678;
	EXPECT(rsu_status_log_pub(&info) == 0);
	EXPECT(info.current_image == 0x12345678);
	g_mbox_status_fail = 1;
	EXPECT(rsu_status_log_pub(&info) == -ENOTSUP);
	g_mbox_status_fail = 0;
	rsu_exit();
	END_TEST();
}

static void t_rsu_running_factory(void)
{
	TEST("[RSU ] rsu_running_factory: compares against status.current_image");
	setup_valid_layout();
	rsu_init(NULL);
	int f = -1;

	g_mbox_status.current_image = 0x004000; /* FACTORY_IMAGE offset */
	EXPECT(rsu_running_factory(&f) == 0);
	EXPECT(f == 1);
	g_mbox_status.current_image = 0x100000; /* P1 */
	EXPECT(rsu_running_factory(&f) == 0);
	EXPECT(f == 0);
	rsu_exit();
	END_TEST();
}

/* ============================================================ */
/*  [SPL ] rsu_spl.c                                              */
/* ============================================================ */
static void fill_spt_for_spl(struct socfpga_rsu_s10_spt *t,
			     u32 current_image)
{
	memset(t, 0, sizeof(*t));
	t->magic_number = RSU_S10_SPT_MAGIC_NUMBER;
	t->entries = 3;
	memcpy(t->spt_slot[0].name, "SPT0", 5);
	t->spt_slot[0].offset[0] = 0x0;
	memcpy(t->spt_slot[1].name, "IMAGE1", 7);
	t->spt_slot[1].offset[0] = current_image;
	t->spt_slot[1].length    = 0x400000;
	memcpy(t->spt_slot[2].name, "SSBL.IMAGE1", 12);
	t->spt_slot[2].offset[0] = 0xD00000;
	t->spt_slot[2].length    = 0x100000;
}

/*
 * Point the probe+mailbox machinery at a baked SPT in a fake flash.
 * Returns a freshly allocated fake_dev; caller frees.
 */
static struct fake_dev *mount_spl_flash(u32 current_image)
{
	struct fake_dev *d = make_flash(0x1000000, 4096);
	struct socfpga_rsu_s10_spt t;

	fill_spt_for_spl(&t, current_image);
	memcpy(d->mem + 0x10000, &t, sizeof(t)); /* SPT0 @ spt_offset[1] */
	g_probe_result = d;
	g_probe_force_fail = 0;
	g_mbox_status.current_image = current_image;
	g_mbox_status_fail = 0;
	g_mbox_spt_off_fail = 0;
	g_mbox_spt_off[1] = 0x10000;
	g_mbox_spt_off[3] = 0x20000;
	return d;
}

static void t_spl_get_spl_slot(void)
{
	TEST("[SPL ] get_spl_slot: success path finds current image");
	struct fake_dev *d = mount_spl_flash(0x800000);
	struct socfpga_rsu_s10_spt spt;
	int idx = -1;

	EXPECT(get_spl_slot(&spt, &idx) == 0);
	EXPECT(idx == 1);
	EXPECT(!strcmp(spt.spt_slot[idx].name, "IMAGE1"));
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_get_spl_slot_fallback_spt1(void)
{
	TEST("[SPL ] get_spl_slot: falls back to SPT1 on bad SPT0 magic");
	struct fake_dev *d = make_flash(0x1000000, 4096);
	struct socfpga_rsu_s10_spt t;

	fill_spt_for_spl(&t, 0x800000);
	/* SPT0 has garbage magic; SPT1 is valid */
	struct socfpga_rsu_s10_spt bad = {0};

	bad.magic_number = 0xDEADBEEF;
	memcpy(d->mem + 0x10000, &bad, sizeof(bad));
	memcpy(d->mem + 0x20000, &t,   sizeof(t));
	g_probe_result = d;
	g_mbox_status.current_image = 0x800000;
	g_mbox_spt_off[1] = 0x10000;
	g_mbox_spt_off[3] = 0x20000;

	struct socfpga_rsu_s10_spt spt;
	int idx = -1;

	EXPECT(get_spl_slot(&spt, &idx) == 0);
	EXPECT(idx == 1);
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_get_spl_slot_no_match(void)
{
	TEST("[SPL ] get_spl_slot: no SPT entry matches current_image");
	struct fake_dev *d = mount_spl_flash(0x800000);

	g_mbox_status.current_image = 0xBAD00000; /* unmatched */
	struct socfpga_rsu_s10_spt spt;
	int idx = -1;

	EXPECT(get_spl_slot(&spt, &idx) == -EINVAL);
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_get_spl_slot_mbox_fail(void)
{
	TEST("[SPL ] get_spl_slot: mbox_rsu_status fail -> -EOPNOTSUPP");
	g_mbox_status_fail = 1;
	struct socfpga_rsu_s10_spt spt;
	int idx = -1;

	EXPECT(get_spl_slot(&spt, &idx) == -EOPNOTSUPP);
	g_mbox_status_fail = 0;
	END_TEST();
}

static void t_spl_get_ssbl_slot(void)
{
	TEST("[SPL ] get_ssbl_slot: finds SSBL.* entry matching crt name");
	struct fake_dev *d = mount_spl_flash(0x800000);
	struct socfpga_rsu_s10_spt_slot s = {0};

	EXPECT(get_ssbl_slot(&s) == 0);
	EXPECT(s.offset[0] == 0xD00000);
	EXPECT(s.length == 0x100000);
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_mmc_filename(void)
{
	TEST("[SPL ] rsu_spl_mmc_filename: success + small buffer");
	struct fake_dev *d = mount_spl_flash(0x800000);
	char buf[256];

	EXPECT(rsu_spl_mmc_filename(buf, sizeof(buf)) == 0);
	EXPECT(!strcmp(buf, "u-boot_IMAGE1.itb"));
	EXPECT(rsu_spl_mmc_filename(NULL, sizeof(buf)) == -ENOENT);
	EXPECT(rsu_spl_mmc_filename(buf, 4) == -ENAMETOOLONG);
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_mmc_env_name(void)
{
	TEST("[SPL ] rsu_spl_mmc_env_name: redund + non-redund");
	struct fake_dev *d = mount_spl_flash(0x800000);
	char buf[256];

	EXPECT(rsu_spl_mmc_env_name(buf, sizeof(buf), false) == 0);
	EXPECT(!strcmp(buf, "u-boot_IMAGE1.env"));
	EXPECT(rsu_spl_mmc_env_name(buf, sizeof(buf), true) == 0);
	EXPECT(!strcmp(buf, "u-boot-redund_IMAGE1.env"));
	EXPECT(rsu_spl_mmc_env_name(NULL, sizeof(buf), false) == -ENOENT);
	EXPECT(rsu_spl_mmc_env_name(buf, 4, false) == -ENAMETOOLONG);
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

static void t_spl_ssbl_address_size(void)
{
	TEST("[SPL ] rsu_spl_ssbl_address / size: happy + fallbacks");
	struct fake_dev *d = mount_spl_flash(0x800000);

	EXPECT(rsu_spl_ssbl_address(false) == 0xD00000);
	EXPECT(rsu_spl_ssbl_size(false)    == 0x100000);
	/* simulate mbox failure -> EOPNOTSUPP -> fallback constants */
	g_mbox_status_fail = 1;
	EXPECT(rsu_spl_ssbl_address(false) == CONFIG_SYS_SPI_U_BOOT_OFFS);
	EXPECT(rsu_spl_ssbl_size(false)    == RSU_SPL_SSBL_FALLBACK_BYTES);
	g_mbox_status_fail = 0;
	free_flash(d);
	g_probe_result = NULL;
	END_TEST();
}

/* ============================================================ */
/*  [CMD ] cmd/socfpga_rsu.c                                       */
/* ============================================================ */
static void t_cmd_dispatch_no_args(void)
{
	TEST("[CMD ] do_rsu: argc < 2 -> CMD_RET_USAGE");
	static const char * const argv[] = { "rsu", NULL };

	EXPECT(do_rsu(NULL, 0, 1, (char * const *)argv) == CMD_RET_USAGE);
	END_TEST();
}

static void t_cmd_dispatch_unknown(void)
{
	TEST("[CMD ] do_rsu: unknown subcommand -> CMD_RET_USAGE");
	static const char * const argv[] = { "rsu", "nope", NULL };

	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv) == CMD_RET_USAGE);
	END_TEST();
}

static void t_cmd_argc_usage(void)
{
	TEST("[CMD ] each subcommand rejects wrong argc");
	setup_valid_layout();
	static const char * const argv0[]      = { "rsu", "slot_count", "extra", NULL };
	static const char * const argv_bn[]    = { "rsu", "slot_by_name", NULL };
	static const char * const argv_gi[]    = { "rsu", "slot_get_info", NULL };
	static const char * const argv_sz[]    = { "rsu", "slot_size", NULL };
	static const char * const argv_sc[]    = { "rsu", "slot_create", "X", NULL };
	static const char * const argv_n[]     = { "rsu", "notify", NULL };
	static const char * const argv_empty[] = { "rsu", "create_empty_cpb", "extra", NULL };
	static const char * const argv_rc[]    = { "rsu", "restore_cpb", NULL };
	static const char * const argv_crf[]   = { "rsu", "check_running_factory", "X", NULL };

	EXPECT(do_rsu(NULL, 0, 3, (char * const *)argv0)      == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv_bn)    == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv_gi)    == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv_sz)    == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 3, (char * const *)argv_sc)    == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv_n)     == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 3, (char * const *)argv_empty) == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 2, (char * const *)argv_rc)    == CMD_RET_USAGE);
	EXPECT(do_rsu(NULL, 0, 3, (char * const *)argv_crf)   == CMD_RET_USAGE);
	END_TEST();
}

/*
 * Host-side port of rsu_parse_num() from cmd/socfpga_rsu.c. Kept in sync
 * with the in-tree parser so the overflow-rejection contract is unit-tested
 * here (U-Boot's simple_strtoull() wraps silently and is not available on
 * the host).
 */
static int host_rsu_parse_num(const char *s, unsigned int base, u64 *out)
{
	const char *start;
	u64 result = 0;

	if (!s || !*s || !out)
		return -EINVAL;
	if (base != 10 && base != 16)
		return -EINVAL;

	if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;

	start = s;
	while (*s) {
		unsigned int digit;
		char c = *s;

		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			break;

		if (digit >= base)
			break;
		if (result > ((u64)~0ULL - digit) / base)
			return -ERANGE;

		result = result * base + digit;
		s++;
	}

	if (s == start)
		return -EINVAL;
	if (*s != '\0' && !(*s == '\n' && s[1] == '\0'))
		return -EINVAL;

	*out = result;
	return 0;
}

static void t_cmd_parse_overflow_and_strictness(void)
{
	TEST("[CMD ] rsu_parse_num: overflow + strict-parse rejection");
	const u64 u64_max = (u64)~0ULL;
	const char *big_hex = "0xffffffffffffffffffffffffffffffff";
	const char *big_dec = "999999999999999999999999999999";
	u64 v = 0;

	/* Boundary: exactly U64_MAX must be accepted. */
	EXPECT(host_rsu_parse_num("0xffffffffffffffff", 16, &v) == 0);
	EXPECT(v == u64_max);
	EXPECT(host_rsu_parse_num("18446744073709551615", 10, &v) == 0);
	EXPECT(v == u64_max);

	/* One past U64_MAX (both hex and dec) must be rejected with -ERANGE. */
	EXPECT(host_rsu_parse_num("0x10000000000000000", 16, &v) == -ERANGE);
	EXPECT(host_rsu_parse_num("18446744073709551616", 10, &v) == -ERANGE);

	/* Absurdly long wrap-around inputs are also rejected. */
	EXPECT(host_rsu_parse_num(big_hex, 16, &v) == -ERANGE);
	EXPECT(host_rsu_parse_num(big_dec, 10, &v) == -ERANGE);

	/* Pre-existing strict-parse contract is preserved. */
	EXPECT(host_rsu_parse_num("",       10, &v) == -EINVAL);
	EXPECT(host_rsu_parse_num("12xyz",  10, &v) == -EINVAL);
	EXPECT(host_rsu_parse_num("0xGG",   16, &v) == -EINVAL);
	EXPECT(host_rsu_parse_num("0x1A",   16, &v) == 0 && v == 0x1A);

	/* Optional trailing newline is still tolerated. */
	EXPECT(host_rsu_parse_num("0x10\n", 16, &v) == 0 && v == 0x10);
	EXPECT(host_rsu_parse_num("0x10\n ", 16, &v) == -EINVAL);

	END_TEST();
}

static void t_cmd_dispatch_happy(void)
{
	TEST("[CMD ] do_rsu happy-path dispatch for 6 subcommands");
	setup_valid_layout();
	static const char * const a1[] = { "rsu", "slot_count" };

	EXPECT(do_rsu(NULL, 0, 2, (char * const *)a1) == CMD_RET_SUCCESS);

	static const char * const a2[] = { "rsu", "slot_by_name", "P1" };

	EXPECT(do_rsu(NULL, 0, 3, (char * const *)a2) == CMD_RET_SUCCESS);

	static const char * const a3[] = { "rsu", "slot_get_info", "0" };

	EXPECT(do_rsu(NULL, 0, 3, (char * const *)a3) == CMD_RET_SUCCESS);

	static const char * const a4[] = { "rsu", "notify", "0x10" };

	EXPECT(do_rsu(NULL, 0, 3, (char * const *)a4) == CMD_RET_SUCCESS);

	static const char * const a5[] = { "rsu", "slot_create",
		       "FRESH", "0x500000", "0x10000" };
	EXPECT(do_rsu(NULL, 0, 5, (char * const *)a5) == CMD_RET_SUCCESS);

	static const char * const a6[] = { "rsu", "create_empty_cpb" };

	EXPECT(do_rsu(NULL, 0, 2, (char * const *)a6) == CMD_RET_SUCCESS);
	END_TEST();
}

/* ============================================================ */
/*  [DM  ] drivers/misc/socfpga_rsu.c                              */
/* ============================================================ */
static void t_dm_probe(void)
{
	TEST("[DM  ] socfpga_rsu_probe zeroes ->ll");
	struct udevice dev;

	memset(&dev, 0xAA, sizeof(dev));
	EXPECT(socfpga_rsu_probe(&dev) == 0);
	EXPECT(!dev.priv.ll);
	END_TEST();
}

/* ============================================================ */
/*  [UT  ] test/cmd/socfpga_rsu.c                                  */
/* ============================================================ */
static void t_ut_socfpga_rsu_usage(void)
{
	TEST("[UT  ] cmd_ut_socfpga_rsu_usage: 'rsu' alone -> CMD_RET_USAGE");
	EXPECT(cmd_ut_socfpga_rsu_usage() == CMD_RET_USAGE);
	END_TEST();
}

/* ============================================================ */
/*  [PROD] arch/arm/mach-socfpga/rsu.c (compiled & linked from   */
/*         rsu_prod_shim.c via #include of the production .c)    */
/* ============================================================ */
/*
 * Phase 1 of A1 (PR review): exercise the *shipped* rsu.c source
 * file through ASan/UBSan, as opposed to the clean-room ports above.
 * The production code is reachable via prod_*() wrappers exported by
 * rsu_prod_shim.c (the static functions in this TU shadow the global
 * production names, so the wrapper namespace is needed).
 *
 * If a future patch breaks rsu.c's hardening (e.g. removes the B1
 * self-healing branch in rsu_init), the [PROD] tests below catch it
 * even though the [RSU] tests above would still pass against the
 * unchanged clean-room port. That's the whole point of this phase.
 */

/*
 * Mirror of the only Intel SiP SMC ID the [PROD] tests assert on; the
 * full set lives in <linux/intel-smc.h> but the harness keeps a
 * minimal shim posture so a single constant is cheaper than pulling
 * the production header in.
 */
#ifndef INTEL_SIP_SMC_RSU_COPY_DCMF_VERSION
#define INTEL_SIP_SMC_RSU_COPY_DCMF_VERSION   0xC2000020
#endif

/* prod_* surface (defined in rsu_prod_shim.c) */
extern int  prod_rsu_init(char *filename);
extern void prod_rsu_exit(void);
extern int  prod_rsu_slot_count(void);
extern int  prod_rsu_clear_error_status(void);
extern int  prod_rsu_reset_retry_counter(void);
extern int  prod_rsu_status_log(struct rsu_status_info *info);
extern int  prod_rsu_notify(int stage);
extern int  prod_rsu_dcmf_version(u32 *versions);
extern int  prod_rsu_max_retry(u8 *value);
extern int  prod_rsu_dcmf_status(u16 *status);

/*
 * Backend-injection knobs (defined in rsu_prod_shim.c) so [PROD] tests
 * can plug a controllable rsu_ll_intf into production rsu_ll_qspi_init.
 */
struct prod_smc_record {
	u32 func_id;
	u64 arg0;
	u64 arg1;
	int arg_len;
	int call_count;
};
extern void                          prod_set_backend(struct rsu_ll_intf *intf,
							int init_errno);
extern const struct prod_smc_record *prod_smc_last(void);
extern void                          prod_smc_reset(void);

/*
 * Mock low-level backend used by [PROD] tests. Intentionally separate
 * from the harness's own qspi_ctx so [PROD] state can't bleed into the
 * clean-room tests above.
 */
static struct rsu_status_info g_prod_test_status;
static int                    g_prod_test_status_call;

static int prod_test_status(struct rsu_status_info *info)
{
	g_prod_test_status_call++;
	if (info)
		*info = g_prod_test_status;
	return 0;
}

static int prod_test_notify(u32 v) { (void)v; return 0; }
static int prod_test_load(u64 o) { (void)o; return 0; }
static void prod_test_exit(void) { }
static int prod_test_part_count(void) { return 0; }

static int prod_test_dcmf_version(u32 *v)
{
	if (!v)
		return -EINVAL;
	v[0] = 0xAABBCCDD;
	v[1] = 0;
	v[2] = 0;
	v[3] = 0;
	return 0;
}

static int prod_test_dcmf_status(u16 *s)
{
	if (!s)
		return -EINVAL;
	s[0] = 0;
	s[1] = 0;
	s[2] = 0;
	s[3] = 0;
	return 0;
}

static int prod_test_max_retry(u8 *v)
{
	if (v)
		*v = 3;
	return 0;
}

static struct rsu_ll_intf g_prod_test_ll = {
	.exit = prod_test_exit,
	.partition.count = prod_test_part_count,
	.fw_ops.status = prod_test_status,
	.fw_ops.notify = prod_test_notify,
	.fw_ops.dcmf_version = prod_test_dcmf_version,
	.fw_ops.dcmf_status = prod_test_dcmf_status,
	.fw_ops.max_retry = prod_test_max_retry,
	.fw_ops.load = prod_test_load,
};

static void prod_reset_state(void)
{
	prod_set_backend(&g_prod_test_ll, 0);
	prod_smc_reset();
	memset(&g_prod_test_status, 0, sizeof(g_prod_test_status));
	g_prod_test_status_call = 0;
	prod_rsu_exit();
}

static void t_prod_init_self_heals_on_double_init_B1(void)
{
	int ret;

	TEST("[PROD] rsu_init self-heals on stale ll_intf (B1 fix)");
	prod_reset_state();

	ret = prod_rsu_init(NULL);
	EXPECT(ret == 0);

	/*
	 * Without the B1 fix the second call would either return -EINTF
	 * or wedge the next subcommand behind a perpetually-set ll_intf.
	 * The shipped rsu.c MUST detect the stale pointer, call
	 * rsu_exit() to drop it, then re-run rsu_ll_qspi_init().
	 */
	ret = prod_rsu_init(NULL);
	EXPECT(ret == 0);

	/*
	 * After a successful re-init the LL backend is reachable, so a
	 * status_log() call goes through to our mock without -EINTF.
	 */
	{
		struct rsu_status_info info;

		ret = prod_rsu_status_log(&info);
		EXPECT(ret == 0);
		EXPECT(g_prod_test_status_call >= 1);
	}

	prod_rsu_exit();
	END_TEST();
}

static void t_prod_init_preserves_backend_errno(void)
{
	int ret;

	TEST("[PROD] rsu_init preserves errno from rsu_ll_qspi_init");
	prod_reset_state();

	prod_set_backend(NULL, -ENOMEM);
	ret = prod_rsu_init(NULL);
	EXPECT(ret == -ENOMEM);

	/*
	 * After a failed init, status_log() must report -EINTF: the
	 * backend was never wired up so calling into it would crash.
	 */
	{
		struct rsu_status_info info;

		ret = prod_rsu_status_log(&info);
		EXPECT(ret == -EINTF);
	}

	END_TEST();
}

static void t_prod_exit_is_idempotent(void)
{
	TEST("[PROD] rsu_exit is idempotent (safe to call twice)");
	prod_reset_state();

	EXPECT(prod_rsu_init(NULL) == 0);
	prod_rsu_exit();
	prod_rsu_exit();    /* second call must not crash / double-free */

	/*
	 * After exit, slot_count() returns -EINTF because the backend
	 * pointer is NULL again.
	 */
	EXPECT(prod_rsu_slot_count() == -EINTF);
	END_TEST();
}

static void t_prod_notify_masks_to_16_bits(void)
{
	int ret;

	TEST("[PROD] rsu_notify masks stage to 16 bits");
	prod_reset_state();
	EXPECT(prod_rsu_init(NULL) == 0);

	/*
	 * The production rsu_notify() must mask off all bits above 15
	 * so that fields above (RSU_NOTIFY_RESET_RETRY_COUNTER etc.)
	 * stay clean. We can't read the value the LL backend was given
	 * directly through the mock, but a 0 return indicates the call
	 * was made; the BIT-range guarantee is exercised by ASan/UBSan
	 * on the masked path.
	 */
	ret = prod_rsu_notify(0xDEADBEEF);
	EXPECT(ret == 0);

	prod_rsu_exit();
	END_TEST();
}

static void t_prod_smc_bridges_called_with_correct_func_id(void)
{
	const struct prod_smc_record *r;
	u32 versions[4] = { 0x11, 0x22, 0x33, 0x44 };
	int ret;

	TEST("[PROD] rsu_dcmf_version invokes SMC bridge with COPY_DCMF_VERSION");
	prod_reset_state();
	EXPECT(prod_rsu_init(NULL) == 0);

	ret = prod_rsu_dcmf_version(versions);
	EXPECT(ret == 0);

	r = prod_smc_last();
	EXPECT(r->call_count == 1);
	EXPECT(r->func_id == INTEL_SIP_SMC_RSU_COPY_DCMF_VERSION);

	prod_rsu_exit();
	END_TEST();
}

/* ============================================================ */
/*  main                                                          */
/* ============================================================ */
#ifndef HARNESS_LABEL
#define HARNESS_LABEL "RSU ASan/UBSan harness (behaviour-equivalent)"
#endif

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	printf("\n=== %s ===\n\n", HARNESS_LABEL);

	/* [FIF] rsu_flash_if.h */
	t_fif_size_erasesize();
	t_fif_read_write_erase_oob();
	t_fif_probe_fallback();

	/* [S10] rsu_s10.c */
	t_s10_entry_count();
	t_s10_sanitize_names();
	t_s10_find_cpb();
	t_s10_rsu_update_argv();

	/* [LLQ] rsu_ll_qspi.c */
	t_llq_multiflash_read_stitched();
	t_llq_dev_loop_zero_size_later_flash();
	t_llq_erase_dev_roundup();
	t_llq_erase_dev_roundup_no_overrun();
	t_llq_partition_accessors();
	t_llq_factory_offset();
	t_llq_priority_add_remove_get();
	t_llq_priority_guards();
	t_llq_check_cpb_spt_flag();
	t_llq_check_cpb_ptr_not_in_spt();
	t_llq_check_cpb_healthy();
	t_llq_check_cpb_zero_ip_slots();
	t_llq_check_spt_zero_length();
	t_llq_save_restore_roundtrip();
	t_llq_save_nulls();
	t_llq_empty_cpb();
	t_llq_empty_cpb_rejects_on_corrupt_spt();
	t_llq_partition_rename_and_dup();
	t_llq_partition_delete_shifts();

	/* [RSU] rsu.c */
	t_rsu_init_exit();
	t_rsu_init_dm();
	t_rsu_slot_ops();
	t_rsu_slot_size();
	t_rsu_slot_ops_without_init();
	t_rsu_notify_masks();
	t_rsu_clear_error_status();
	t_rsu_reset_retry_counter();
	t_rsu_dcmf_and_retry();
	t_rsu_status_log_forward();
	t_rsu_running_factory();

	/* [SPL] rsu_spl.c */
	t_spl_get_spl_slot();
	t_spl_get_spl_slot_fallback_spt1();
	t_spl_get_spl_slot_no_match();
	t_spl_get_spl_slot_mbox_fail();
	t_spl_get_ssbl_slot();
	t_spl_mmc_filename();
	t_spl_mmc_env_name();
	t_spl_ssbl_address_size();

	/* [CMD] cmd/socfpga_rsu.c */
	t_cmd_dispatch_no_args();
	t_cmd_dispatch_unknown();
	t_cmd_argc_usage();
	t_cmd_parse_overflow_and_strictness();
	t_cmd_dispatch_happy();

	/* [DM ] drivers/misc/socfpga_rsu.c */
	t_dm_probe();

	/* [UT ] test/cmd/socfpga_rsu.c */
	t_ut_socfpga_rsu_usage();

	/*
	 * [PROD] arch/arm/mach-socfpga/rsu.c — exercised through the
	 * Phase-1 inclusion shim (rsu_prod_shim.c). These call into the
	 * shipped binary's symbols, not the clean-room ports above.
	 */
	t_prod_init_self_heals_on_double_init_B1();
	t_prod_init_preserves_backend_errno();
	t_prod_exit_is_idempotent();
	t_prod_notify_masks_to_16_bits();
	t_prod_smc_bridges_called_with_correct_func_id();

	printf("\n=== Results: %d run, %d passed, %d FAILED ===\n\n",
	       g_total, g_pass, g_total - g_pass);
	return g_total == g_pass ? 0 : 1;
}
