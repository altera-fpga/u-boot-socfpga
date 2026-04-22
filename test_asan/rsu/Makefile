# SPDX-License-Identifier: GPL-2.0+
#
# ASan + UBSan test harness for the SoC FPGA RSU driver stack.
#
# Source files exercised by this harness:
#   * arch/arm/mach-socfpga/include/mach/rsu_flash_if.h
#   * arch/arm/mach-socfpga/rsu.c                   (#include via rsu_prod_shim.c)
#   * arch/arm/mach-socfpga/rsu_s10.c               (clean-room port; A1 follow-up)
#   * arch/arm/mach-socfpga/rsu_ll_qspi.c           (clean-room port; A1 follow-up)
#   * arch/arm/mach-socfpga/rsu_spl.c               (clean-room port; A1 follow-up)
#   * cmd/socfpga_rsu.c                             (clean-room port; A1 follow-up)
#   * drivers/misc/socfpga_rsu.c                    (clean-room port; A1 follow-up)
#   * test/cmd/socfpga_rsu.c                        (clean-room port; A1 follow-up)
#
# Phase 1 of A1 (PR review) wires the production rsu.c into the
# harness via a host-side inclusion shim (rsu_prod_shim.c). Subsequent
# follow-ups will migrate the remaining 5 production .c files.
#
# Usage:
#   make            -- build + run under ASan + UBSan
#   make run        -- run only
#   make clean      -- remove build artefacts

# Always compile this native harness with the host toolchain. A bare
# `CC ?= gcc` would inherit the target cross-compiler when building
# U-Boot with CC exported, breaking this Makefile.
HOSTCC	?= gcc
CC	:= $(HOSTCC)

# Host-shim include tree comes FIRST so that production headers
# transitively resolve <asm/types.h>, <linux/bitops.h>, etc. through
# our native-host shims, not the ARM kernel headers.
SHIM_INCLUDES := -Iinclude

# Kconfig-style symbols are stubbed here (-D...) so the .c source stays
# free of `#define CONFIG_...` lines that checkpatch rejects.
HARNESS_CONFIG := -DCONFIG_SF_DEFAULT_BUS=0 \
                  -DCONFIG_SF_DEFAULT_CS=0 \
                  -DCONFIG_SOCFPGA_RSU_SF_CS=0 \
                  -DCONFIG_SPL_ATF=1 \
                  -DCONFIG_SYS_SPI_U_BOOT_OFFS=0x100000

CFLAGS  := -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
           -Wno-unused-function -Wno-unused-variable \
           -g -O1 -fno-omit-frame-pointer \
           -fsanitize=address -fsanitize=undefined \
           $(SHIM_INCLUDES) \
           $(HARNESS_CONFIG)
LDFLAGS := -fsanitize=address -fsanitize=undefined -lz

OBJS    := rsu_asan.o rsu_prod_shim.o
TARGET  := rsu_asan_test

.PHONY: all run clean

all: $(TARGET) run

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	@printf '\n==== Running RSU ASan/UBSan test harness ====\n\n'
	ASAN_OPTIONS=detect_stack_use_after_return=1:detect_leaks=1:halt_on_error=0 \
	UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 \
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
