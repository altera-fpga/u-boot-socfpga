/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation
 *
 * The RSU public ABI (types, error codes, function prototypes) lives in
 * the cross-arch header <socfpga_rsu.h> so that sandbox stubs and tests
 * can use the same declarations without depending on arm-private types.
 * Arch-private RSU types (e.g. struct rsu_ll_intf) remain in this
 * directory in their own headers (rsu_ll.h, rsu_misc.h, ...).
 */

#ifndef __RSU_H__
#define __RSU_H__

#include <socfpga_rsu.h>

#endif /* __RSU_H__ */
