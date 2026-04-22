/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Native-host shim for U-Boot's <linux/kconfig.h>. Provides IS_ENABLED()
 * with the kernel-style "1 if symbol is defined to 1, 0 otherwise"
 * semantics. The harness Makefile injects (or omits) the relevant
 * CONFIG_* defines via -D flags.
 *
 * For the RSU host build we deliberately leave CONFIG_SOCFPGA_RSU_DM
 * undefined (so the production code falls into the non-DM path) which
 * keeps the host build free of DM-uclass plumbing.
 */
#ifndef __TEST_ASAN_LINUX_KCONFIG_H__
#define __TEST_ASAN_LINUX_KCONFIG_H__

/*
 * Kernel-style IS_ENABLED: evaluates to 1 if CONFIG_FOO is defined to
 * 1, and 0 if undefined or defined to 0.
 *
 * The two-step indirection (__is_defined -> ___is_defined) is load-
 * bearing: the inner ## token-paste in ___is_defined would otherwise
 * suppress argument expansion of `option`, so a -DCONFIG_FOO=1 caller
 * would paste __ARG_PLACEHOLDER_CONFIG_FOO (undefined) instead of
 * __ARG_PLACEHOLDER_1 (defined to "0,") and IS_ENABLED would always
 * return 0. Mirror linux/kconfig.h exactly.
 */
#define __ARG_PLACEHOLDER_1           0,
#define __take_second_arg(__ignored, val, ...) val

#define ___is_defined(arg1_or_junk)   __take_second_arg(arg1_or_junk 1, 0)
#define __is_defined(x)               ___is_defined(__ARG_PLACEHOLDER_##x)

#define IS_ENABLED(option) __is_defined(option)
#define IS_BUILTIN(option) IS_ENABLED(option)
#define IS_MODULE(option)  0
#define IS_REACHABLE(option) IS_ENABLED(option)

#endif /* __TEST_ASAN_LINUX_KCONFIG_H__ */
