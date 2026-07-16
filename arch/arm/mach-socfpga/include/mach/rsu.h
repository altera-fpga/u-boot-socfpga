/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation
 *
 * The RSU public ABI (types, error codes, function prototypes) lives in
 * the cross-arch header <socfpga_rsu.h> so that sandbox stubs and tests
 * can use the same declarations without depending on arm-private types.
 * This file, and its siblings in this directory (rsu_ll.h, rsu_misc.h,
 * rsu_s10.h, rsu_flash_if.h), are now thin compatibility forwarders to
 * their <socfpga_rsu*.h> cross-arch equivalents, kept so existing
 * <asm/arch/rsu*.h> includes continue to compile unchanged.
 */

#ifndef __RSU_H__
#define __RSU_H__

#include <socfpga_rsu.h>

#endif /* __RSU_H__ */
