# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# Copyright (C) 2012-2025 Altera Corporation <www.altera.com>
# Copyright (C) 2017-2024 Intel Corporation <www.intel.com>

obj-y	+= board.o
obj-y	+= clock_manager.o
obj-y	+= misc.o

ifdef CONFIG_ARCH_SOCFPGA_GEN5
obj-y	+= clock_manager_gen5.o
obj-y	+= misc_gen5.o
obj-y	+= reset_manager_gen5.o
obj-y	+= scan_manager.o
obj-y	+= system_manager_gen5.o
obj-y	+= timer.o
obj-y	+= wrap_pll_config.o
obj-y	+= fpga_manager.o
endif

ifdef CONFIG_ARCH_SOCFPGA_ARRIA10
obj-y	+= clock_manager_arria10.o
obj-y	+= misc_arria10.o
obj-y	+= pinmux_arria10.o
obj-y	+= reset_manager_arria10.o
endif

ifdef CONFIG_ARCH_SOCFPGA_STRATIX10
obj-y	+= clock_manager_s10.o
obj-y	+= lowlevel_init_soc64.o
obj-y	+= mailbox_s10.o
obj-y	+= misc_soc64.o
obj-y	+= mmu-arm64_s10.o
obj-y	+= reset_manager_soc64.o
obj-y	+= reset_manager_soc64_platform_families.o
obj-y	+= smmu_s10.o
obj-y	+= timer_s10.o
obj-y	+= wrap_handoff_soc64.o
obj-y	+= wrap_pll_config_soc64.o
ifndef CONFIG_XPL_BUILD
obj-y   += rsu.o
obj-y   += rsu_ll_qspi.o
obj-y   += rsu_misc.o
obj-$(CONFIG_CADENCE_QSPI) += rsu_s10.o
obj-$(CONFIG_ARMV8_PSCI) += psci.o
obj-$(CONFIG_ARMV8_PSCI) += smc_ecc_dbe_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_fpga_reconfig_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_registers_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_rsu_s10.o
endif
endif

ifdef CONFIG_ARCH_SOCFPGA_AGILEX
obj-y	+= clock_manager_agilex.o
obj-y	+= lowlevel_init_soc64.o
obj-y	+= mailbox_s10.o
obj-y	+= misc_soc64.o
obj-y	+= mmu-arm64_s10.o
obj-y	+= reset_manager_soc64.o
obj-y	+= reset_manager_soc64_platform_families.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= secure_vab.o
obj-y	+= smmu_s10.o
obj-y	+= timer_s10.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= vab.o
obj-y	+= wrap_handoff_soc64.o
obj-y	+= wrap_pll_config_soc64.o
obj-y	+= altera-sysmgr.o
ifndef CONFIG_XPL_BUILD
obj-y	+= rsu.o
obj-y	+= rsu_ll_qspi.o
obj-y	+= rsu_misc.o
obj-$(CONFIG_CADENCE_QSPI) += rsu_s10.o
obj-$(CONFIG_ARMV8_PSCI) += psci.o
obj-$(CONFIG_ARMV8_PSCI) += smc_ecc_dbe_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_fpga_reconfig_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_registers_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_rsu_s10.o
endif
endif

ifdef CONFIG_ARCH_SOCFPGA_AGILEX5
obj-y	+= clock_manager_agilex5.o
obj-y	+= mailbox_s10.o
obj-y	+= misc_soc64.o
obj-y	+= mmu-arm64_s10.o
obj-y	+= reset_manager_soc64.o
obj-y	+= reset_manager_soc64_platform_families.o
obj-y	+= wrap_handoff_soc64.o
obj-y	+= wrap_pll_config_soc64.o
obj-y	+= altera-sysmgr.o
obj-y	+= ccu_ncore3.o
obj-y	+= timer_s10.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= secure_vab.o
obj-y	+= smmu_agilex5_dv.o
obj-y	+= smmuv3_dv.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= vab.o
ifndef CONFIG_XPL_BUILD
obj-y   += rsu.o
obj-y   += rsu_ll_qspi.o
obj-y   += rsu_misc.o
obj-$(CONFIG_CADENCE_QSPI) += rsu_s10.o
obj-$(CONFIG_ARMV8_PSCI) += psci.o
obj-$(CONFIG_ARMV8_PSCI) += smc_ecc_dbe_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_fpga_reconfig_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_registers_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_rsu_s10.o
endif
ifneq ($(CONFIG_SPL_FIT_SIGNATURE),y)
$(warning Agilex5 SPL is being built without FIT signature verification (CONFIG_SPL_FIT_SIGNATURE=n). This image is NOT suitable for shipping firmware - see doc/board/intel/agilex5_socdk.rst for the defconfig variant matrix.)
endif
endif

ifdef CONFIG_ARCH_SOCFPGA_AGILEX7M
obj-y	+= clock_manager_agilex.o
obj-y	+= lowlevel_init_soc64.o
obj-y	+= mailbox_s10.o
obj-y	+= misc_soc64.o
obj-y	+= mmu-arm64_s10.o
obj-y	+= reset_manager_soc64.o
obj-y	+= reset_manager_soc64_platform_families.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= secure_vab.o
obj-y	+= smmu_s10.o
obj-y	+= timer_s10.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= vab.o
obj-y	+= wrap_handoff_soc64.o
obj-y	+= wrap_pll_config_soc64.o
obj-y	+= altera-sysmgr.o
ifndef CONFIG_XPL_BUILD
obj-y	+= rsu.o
obj-y	+= rsu_ll_qspi.o
obj-y	+= rsu_misc.o
obj-$(CONFIG_CADENCE_QSPI) += rsu_s10.o
endif
endif

ifdef CONFIG_ARCH_SOCFPGA_N5X
obj-y	+= clock_manager_n5x.o
obj-y	+= lowlevel_init_soc64.o
obj-y	+= mailbox_s10.o
obj-y	+= misc_soc64.o
obj-y	+= mmu-arm64_s10.o
obj-y	+= reset_manager_soc64.o
obj-y	+= reset_manager_soc64_platform_families.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= secure_vab.o
obj-y	+= smmu_s10.o
obj-y	+= timer_s10.o
obj-$(CONFIG_SOCFPGA_SECURE_VAB_AUTH)	+= vab.o
obj-y	+= wrap_handoff_soc64.o
obj-y	+= wrap_pll_config_soc64.o
ifndef CONFIG_XPL_BUILD
obj-y   += rsu.o
obj-y   += rsu_ll_qspi.o
obj-y   += rsu_misc.o
obj-$(CONFIG_CADENCE_QSPI) += rsu_s10.o
obj-$(CONFIG_ARMV8_PSCI) += psci.o
obj-$(CONFIG_ARMV8_PSCI) += smc_ecc_dbe_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_registers_s10.o
obj-$(CONFIG_ARMV8_PSCI) += smc_rsu_s10.o
endif
endif

ifdef CONFIG_XPL_BUILD
ifdef CONFIG_ARCH_SOCFPGA_GEN5
obj-y	+= spl_gen5.o
obj-y	+= freeze_controller.o
obj-y	+= wrap_iocsr_config.o
obj-y	+= wrap_pinmux_config.o
obj-y	+= wrap_sdram_config.o
endif
ifdef CONFIG_ARCH_SOCFPGA_ARRIA10
obj-y	+= spl_a10.o
endif
ifdef CONFIG_ARCH_SOCFPGA_STRATIX10
obj-y	+= spl_s10.o
obj-y	+= spl_soc64.o
endif
ifdef CONFIG_ARCH_SOCFPGA_AGILEX
obj-y	+= spl_agilex.o
obj-y	+= spl_soc64.o
endif
ifdef CONFIG_ARCH_SOCFPGA_N5X
obj-y	+= spl_n5x.o
obj-y	+= spl_soc64.o
endif
ifdef CONFIG_ARCH_SOCFPGA_AGILEX5
obj-y	+= spl_soc64.o
obj-y	+= spl_agilex5.o
endif
ifdef CONFIG_ARCH_SOCFPGA_AGILEX7M
obj-y	+= spl_agilex7m.o
obj-y	+= spl_soc64.o
endif
else
obj-$(CONFIG_SPL_ATF) += secure_reg_helper.o
obj-$(CONFIG_SPL_ATF) += smc_api.o
endif

ifdef CONFIG_SOCFPGA_RSU_MULTIBOOT
obj-y	+= rsu_spl.o
endif

ifdef CONFIG_ARCH_SOCFPGA_GEN5
# QTS-generated config file wrappers
CFLAGS_wrap_iocsr_config.o	+= -I$(srctree)/board/$(BOARDDIR)
CFLAGS_wrap_pinmux_config.o	+= -I$(srctree)/board/$(BOARDDIR)
CFLAGS_wrap_pll_config.o	+= -I$(srctree)/board/$(BOARDDIR)
CFLAGS_wrap_sdram_config.o	+= -I$(srctree)/board/$(BOARDDIR)
endif
