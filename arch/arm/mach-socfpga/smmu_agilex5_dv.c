// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation. All rights reserved
 *
 */

#include <common.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/firewall.h>
#include <asm/arch/smmu_agilex5.h>
#include <asm/arch/system_manager.h>
#include <linux/sizes.h>

/* TF SMMU workaround */
#include <asm/arch/smmuv3_dv.h>

DECLARE_GLOBAL_DATA_PTR;

#define HPS_SDM_SP_OFST 0xFF8D0000
#define SDM_I2C_CSR_OFST 0x00000108//i2c slave address register
#define SDM_I2C_DATA_OFST 0x00000110
#define CSR_WRITE_MASK 0x3FF // only 10 bits are rw
#define TCR_RES1      ((1 << 31) | (1 << 23)) // Reserved, set as 1.
#define TCR_PS_1TiB   (24 << 16)             // Physical Address Size of 1 TiB maximum.
#define TCR_TG0_4KiB  (0x0 << 14)             // Granule size of 4 KiB
#define TCR_TG0_64KiB  0x1            // Granule size of 64 KiB
#define TCR_SH0_OUTER (0x2 << 12)             // Shareability for TTBR0
#define TCR_ORGN0_WBA (0x1 << 10)             // Outer cacheablility
#define TCR_IRGN0_WBA (0x1 <<  8)             // Inner cacheability
#define TCR_T0SZ_1TiB (24 <<   0)             // Size offset of memory region as 2^(64 - val), set for 1 TiB.
#define GEN_MAIR_INDEX(index, value) (((value) & 0xff) << (8 * index))

// Strongly ordered memory:
//  - no Gather, no Reorder, no Early write acknowledgements.
#define MAIR_ATTR_DEVICE_nGnRnE (0x00)

// Device memory:
//  - no Gather, no Reorder, Early write acknowledgements. This is suitable for performant device memory.
#define MAIR_ATTR_DEVICE_nGnRE  (0x04)

// Cacheable memory, most performant options:
//  - Inner and Outer are: Write Back Allocate for Reads and Writes.
#define OUTER_WB       (1 << 6) // Write back
#define OUTER_NONTRANS (1 << 7) // non-transient (hint to the cache, ignored on Cortex-A53)
#define OUTER_WALLOC   (1 << 4) // Write allocate
#define OUTER_RALLOC   (1 << 5) // Read allocate
#define INNER_WB       (1 << 2)
#define INNER_NONTRANS (1 << 3)
#define INNER_WALLOC   (1 << 0)
#define INNER_RALLOC   (1 << 1)

#define SMMU_CD_AA64_AARCH64 (0x1)
#define zero  (0x00000000)
#define SMMU_CD_SIZEOF (0x40)

#define SMMU_MASTER_MAX 0xB
#define SDM_ID          0x000A
#define STE_SDM_SZ	(SZ_512 * SMMU_MASTER_MAX)

#define CD_SZ             			SZ_1K
#define EQ_SZ                       SZ_512
#define CMDQ_SZ                     SZ_512

#define TT_3LV                      3
#define TT_SZ                       (SZ_4K * TT_3LV)

#define STBase  (void*)0x73000
/* 0x74600 */
#define EQBase  (void*)(STBase + STE_SDM_SZ)
/* 0x74800 */
#define CQBase  (void*)(EQBase +  EQ_SZ)
/* 0x74A00 */
#define CDBase  (void*)(CQBase + CMDQ_SZ)

#define TTlv0  (uintptr_t)0x75000
/* 0x78000 */
#define TTlv1  (uintptr_t)(TTlv0 + TT_SZ)
/* 0x7b000 */
#define TTlv2  (uintptr_t)(TTlv1 + TT_SZ)

#define PAGE_4KB        (0x1000)
#define BLOCK_64KB       (0x10000)
#define BLOCK_2MB       (0x200000)
#define BLOCK_512MB       (0x20000000)
#define BLOCK_1GB       (0x40000000ul)
#define BLOCK_512GB     (0x8000000000)
#define BLOCK_4TB     (0x40000000000)

//#define TTBase  (void*)&ttbr0_el3_l0_table
#define TTBase  (void*)(TTlv0)

#define SMMU_CD_NO_AF_FAULTS (0x1)
#define SMMU_CD_IPS_32               (0x0)
#define SMMU_CD_IPS_40               (0x2)
#define SMMU_CD_TERMINATE            (0x6)
#define SMMU_ST_BYPASS				 (0x4)
#define SMMU_ST_STAGE1_ONLY          (0x5)
#define SMMUBase  	       (void*)0x16000000

#define MAIR_ATTR_MEMORY_IO_WBRWA (OUTER_WB | OUTER_NONTRANS | OUTER_WALLOC | OUTER_RALLOC | INNER_WB | INNER_NONTRANS | INNER_WALLOC | INNER_RALLOC)

// Default MAIR attribute
#define MAIR_ATTR_DEFAULT  MAIR_ATTR_DEVICE_nGnRnE

/* Stream ID configuration value for Agilex5 */
#define TSN0			0x00010001
#define TSN1			0x00020002
#define TSN2			0x00030003
#define NAND			0x00040004
#define SDMMC			0x00050005
#define USB0			0x00060006
#define USB1			0x00070007
#define DMA0			0x00080008
#define DMA1			0x00090009
#define SDM			0x000A000A
#define CORE_SIGHT_DEBUG	0x000B000B

#define alt_write_hword(addr, val)	(writew(val, addr))
#define alt_write_word(addr, val)	(writel(val, addr))
#define alt_read_hword(addr)		(readw(addr))
#define alt_read_word(addr)		(readl(addr))


#define PTGEN_L1_EMIT_BLOCK(block, upper, lower) ((uint64_t)(((upper) & (0xffful << 52)) | ((uint64_t)(block) & 0xffffc0000000) | ((lower) & (0x3ff << 2)) | 1)	)
#define MEMSPACE0_DEFAULT_EMIT    PTGEN_L1_EMIT_BLOCK

#define PTGEN_LOWER_NOT_GLOBAL(bit)   (((bit)  & 0x1) << 11)
#define PTGEN_LOWER_ACCESS_FLAG(flag) (((flag) & 0x1) << 10)
#define PTGEN_LOWER_SHARABILITY(sh)   (((sh)   & 0x3) <<  8)
#define PTGEN_LOWER_ACCESS_PERM(ap)   (((ap)   & 0x3) <<  6)   // This is for AP[2:1]. AP[0] is always 0. This is why the field overlaps with NS.
#define PTGEN_LOWER_NON_SECURE(ns)    (((ns)   & 0x1) <<  5)
#define PTGEN_LOWER_ATTR_INDEX(attr)  (((attr) & 0x7) <<  2)

#define PTGEN_LOWER_SHARABILITY_E_NON   (0x0)
#define PTGEN_LOWER_SHARABILITY_E_OUTER (0x2)
#define PTGEN_LOWER_SHARABILITY_E_INNER (0x3)

#define PTGEN_LOWER_ACCESS_EL3_RW (0 << 2)
#define PTGEN_LOWER_ACCESS_EL3_R0 (1 << 2)

// Attributes to be used only with Table entries
#define PTGEN_UPPER_TABLE_DEFAULT ((uint64_t) 0)
#define PTGEN_UPPER_TABLE_S     	 ((uint64_t) 0ul << 63)
#define PTGEN_UPPER_TABLE_NS      ((uint64_t) 1ul << 63)
#define PTGEN_UPPER_TABLE_AP(ap)  ((uint64_t) ((ap) & (0x3)) << 61)
#define PTGEN_UPPER_TABLE_XN      ((uint64_t) 1ul << 60)
#define PTGEN_UPPER_TABLE_PXN     ((uint64_t) 0 << 59) //SM update

// Attributes to be used only with Block entries
#define PTGEN_UPPER_BLOCK_DEFAULT    ((uint64_t) 0)
#define PTGEN_UPPER_BLOCK_UXN_XN     (1ul << 54)
#define PTGEN_UPPER_BLOCK_PXN        (0ul << 53)   //SM update
#define PTGEN_UPPER_BLOCK_CONTIGUOUS (1ul << 52)

#define PTGEN_LOWER_MEMORY_SHARED_DEFAULT PTGEN_LOWER_MEMORY_SHARED_INNER

//
// Shareability for non Memory does not apply. Those locations are automatically marked outer
// shareable.
//
#define PTGEN_LOWER_MEMORY_SHARED_INNER (PTGEN_LOWER_MEMORY | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_INNER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER (PTGEN_LOWER_MEMORY | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER_EL0_RW (PTGEN_LOWER_MEMORY_EL0_RW | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER_EL1_EX (PTGEN_LOWER_MEMORY_EL1_EX | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER_EL1_RO (PTGEN_LOWER_MEMORY_EL1_RO | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER_EL1_EL0_RO (PTGEN_LOWER_MEMORY_EL1_EL0_RO | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER (PTGEN_LOWER_MEMORY | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))
#define PTGEN_LOWER_MEMORY_SHARED_OUTER_NS (PTGEN_LOWER_MEMORY_NONSECURE | PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_OUTER))

// Attributes to be used only with Block entries. No option for Table entries.
#define PTGEN_LOWER_MEMORY_EL3    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_ACCESS_PERM(0b00))
#define PTGEN_LOWER_MEMORY_EL2    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_ACCESS_PERM(0b00))
#define PTGEN_LOWER_MEMORY_EL0_RW    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_NON_SECURE(0) | PTGEN_LOWER_ACCESS_PERM(0b01))		//Refer to tiny paragraph in D5.5.1 in AARCHv8 Spec that doesn't allow privlidged execution in EL1, when AP is 0b10
#define PTGEN_LOWER_MEMORY_EL1_EX    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_NON_SECURE(0) | PTGEN_LOWER_ACCESS_PERM(0b00)) //Enable Read/Write Access to Memory in EL0
#define PTGEN_LOWER_MEMORY_EL1_RO    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_ACCESS_PERM(0b10)) //Enable Read/Write Access to Memory in EL0
#define PTGEN_LOWER_MEMORY_EL1_EL0_RO    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_ACCESS_PERM(0b11)) //Enable Read/Write Access to Memory in EL0


#define PTGEN_LOWER_MEMORY    (PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1)| PTGEN_LOWER_ACCESS_PERM(0b00))
#define PTGEN_LOWER_DEVICE    (PTGEN_LOWER_ATTR_INDEX(1) | PTGEN_LOWER_ACCESS_FLAG(1))
#define PTGEN_LOWER_DEVICE_EL0_RW    (PTGEN_LOWER_ATTR_INDEX(1) | PTGEN_LOWER_ACCESS_FLAG(1) | PTGEN_LOWER_NON_SECURE(0) |  PTGEN_LOWER_ACCESS_PERM(0b01) )
#define PTGEN_LOWER_STRONG    (PTGEN_LOWER_ATTR_INDEX(0) | PTGEN_LOWER_ACCESS_FLAG(1))

#define PTGEN_LOWER_MEMORY_NONSECURE (PTGEN_LOWER_NON_SECURE(1) |PTGEN_LOWER_ATTR_INDEX(2) | PTGEN_LOWER_ACCESS_FLAG(1))

static volatile int derr_flag = 0; 
static volatile uint32_t D1Cnt;

// static void derr_exception_handler(void *arg) {
  // puts("slave error received\n");
  // derr_flag ++;
// }

static void ttlv0_init(void)
{
	writeq(((uint64_t)(((PTGEN_UPPER_TABLE_DEFAULT) & (0xffful << 52)) | ((uint64_t)(TTlv1) & 0xfffffffff000) | 3)), TTlv0);
}

static void ttlv1_init(void)
{
	writeq(((uint64_t)(((PTGEN_UPPER_TABLE_DEFAULT) & (0xffful << 52)) | ((uint64_t)(TTlv2) & 0xfffffffff000) | 3)), TTlv1);
}

static void ttlv2_init(void)
{
     int i;

     for (i = 0; i < 0x100; i++) {

	writeq((uint64_t)(((0) & (0xffful << 52)) | ((uint64_t)(0x80000000 + (i * BLOCK_2MB)) & 0xffffffe00000) | ((PTGEN_LOWER_MEMORY_SHARED_DEFAULT) & (0x3ff << 2)) | 1), TTlv2 + (i * 8));
     }
}

static void tt_init(void)
{
	ttlv0_init();
	ttlv1_init();
	ttlv2_init();
}
// void lw_bridge_reset_disable() {
   // //res_man_deassert_bridge_reset(RstMgr, 0x1<<1);       // FIXME USE SOCAL
   // alt_write_word(ALT_RSTMGR_BRGMODRST_ADDR, alt_read_word(ALT_RSTMGR_BRGMODRST_ADDR) & ALT_RSTMGR_BRGMODRST_LWSOC2FPGA_CLR_MSK);
// }
int smmu_sdm_init(void)
{
	int i;
    //HANDLE_t SysMgr;
    //HANDLE_t RstMgr;
    
    printf("Starting HPS SMMU test for SDM.\n");
    // lw_bridge_reset_disable();
  
    // RstMgr = rstmgr_init();
    // SysMgr = sysmgr_init();
    //sysmgr_dma_mgr_ns_set(sys_mgr_p, 0);	
     
    //Opens bridges for DMA to communicate with FPGA
    // res_man_deassert_lwh2f_bridge(RstMgr);

    // init_cpu0_gic(); //set up doorbell register with SDM.
    
    // sysmgr_fpga2soc_sec_set(SysMgr, 1);
     
    //set bridge enable register
    // puts("Set bridge enable register.\n");
    // enable_f2sdram_bridge(); 
    // set_f2soc_intfcsel();
    // set_f2soc_bridge_enable();
    // set_dualemif0_dualport0();
    
    // res_man_deassert_bridge_reset(RstMgr,ALT_RSTMGR_BRGMODRST_LWSOC2FPGA_SET_MSK);
    // res_man_deassert_bridge_reset(RstMgr,ALT_RSTMGR_BRGMODRST_FPGA2SOC_SET_MSK);
    // res_man_deassert_bridge_reset(RstMgr,ALT_RSTMGR_BRGMODRST_SOC2FPGA_SET_MSK);
    
    // printf("PER0MODRST rst done\n");
    // alt_write_word(0x10D11024, alt_read_word(0x10D11024) & 0xffbff8f8);


    // //OCRAM region0 set to non-secure
    //alt_write_word(0x108CC418, alt_read_word(0x108CC418) & 0x0);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION0ADDR_BASE_ADDR, 0x80000000);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION0ADDR_LIMIT_ADDR, 0xBFFFFFFF);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION1ADDR_BASE_ADDR, 0xC0000000);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION1ADDR_LIMIT_ADDR, 0xFFFFFFFF);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION2ADDR_BASE_ADDR, 0x80000000);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION2ADDR_BASEEXT_ADDR, 0x8);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION2ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION2ADDR_LIMITEXT_ADDR, 0xB);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION3ADDR_BASE_ADDR, 0x0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION3ADDR_BASEEXT_ADDR, 0xC);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION3ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION3ADDR_LIMITEXT_ADDR, 0xF);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION4ADDR_BASE_ADDR, 0x0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION4ADDR_BASEEXT_ADDR, 0x88);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION4ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION4ADDR_LIMITEXT_ADDR, 0x9F);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION5ADDR_BASE_ADDR, 0x0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION5ADDR_BASEEXT_ADDR, 0xA0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION5ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION5ADDR_LIMITEXT_ADDR, 0xBF);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION6ADDR_BASE_ADDR, 0x0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION6ADDR_BASEEXT_ADDR, 0xC0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION6ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION6ADDR_LIMITEXT_ADDR, 0xDF);

     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION7ADDR_BASE_ADDR, 0x0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION7ADDR_BASEEXT_ADDR, 0xE0);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION7ADDR_LIMIT_ADDR, 0xFFFFFFFF);
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_MPUREGION7ADDR_LIMITEXT_ADDR, 0xFF);
     
     // alt_write_word(ALT_SOC_NOC_FW_DDR_CCU_DMI0_INST_0_DDR_SCR_ENABLE_SET_ADDR, 0xffff);
     // asm("DSB sy");

    //sysmgr_fpgaintf_glbl_ctl_set(sys_mgr_p, 1);
    //alt_write_word(0x108CC418, alt_read_word(0x108CC418) & 0x0);
   // alt_write_word(ALT_SYSMGR_CORE_DMA_TBU_STREAM_CTRL_REG_0_DMA0_ADDR, 0x3f);
    /* Disable substream ID since it's only represent a process identifier */
    //alt_write_word(0x10D12184, 0x00000003);// sdm_tbu_stream_ctrl_reg_1_sdm
    /* Setup stream ID for SDM TBU */
    //alt_write_word(0x10D121B0, SDM);
    /* According CCU HAS, only NCAIU0(FPGA2SoC) supporting cache stashing */
    // alt_write_word(0x10D12158, 0xffffffff);// Disables SDM stash
    printf("sdm_tbu_stream_ctrl_reg_1_sdm : 0x%x\n", readl(0x10D12184));
   printf("smmuSetBaseAddr()\n"); 
    smmuSetBaseAddr(SMMUBase);
     memset(STBase, 0, STE_SDM_SZ);
     //memset(STBase, 0, 512);
     printf("smmuInitStreamTable()\n"); 
     smmuInitStreamTable(STBase /*Base addr of stream table*/,
                      SMMU_ST_READ_ALLOCATE,
                      4 /*SteamID width, 2^4 =>16 entries*/);


     // Set up event queue
     printf("main(): Installing Event Queue\n");
     memset(EQBase, 0, EQ_SZ);
     smmuInitEventQueue(EQBase /*Base addr of event queue*/,
                        SMMU_EQ_WRITE_ALLOCATE,
                        4 /*Log2 of CQ size, 2^4=>16 entries*/); //32B in size


     // Set up command queue
     printf("main(): Installing Command Queue\n");
     memset(CQBase, 0, CMDQ_SZ);
     smmuInitCommandQueue(CQBase /*Base addr of command queue*/,
                          SMMU_CQ_READ_ALLOCATE,
                          5 /*Log2 of CQ size, 2^5=>32 entries*/); //16B in size

    //alt_clrbits_word(MY_ALT_RSTMGR_PER0MODRST_ADDR, MY_ALT_RSTMGR_PER0MODRST_DMA_SET_MSK);    
    //log_info("Released DMA reset\n");  
//smmuInitTableAttributes(SMMU_CR1_WB_CACHEABLE/*inner*/, SMMU_CR1_WB_CACHEABLE/*outer*/, SMMU_CR1_system);
  //smmuInitTableAttributes_S(SMMU_CR1_NON_CACHEABLE/*inner*/, SMMU_CR1_NON_CACHEABLE/*outer*/, SMMU_CR1_NON_CACHEABLE);
  smmuInitTableAttributes(SMMU_CR1_WB_CACHEABLE/*inner*/, SMMU_CR1_WB_CACHEABLE/*outer*/, SMMU_CR1_NON_CACHEABLE);


 // Issue commands to invalidate TLBs
  printf("Issue commands to invalidate TLBs\n");
  smmuInvalidateAll();

  // Enable SMMU
  printf("main(): Enabling SMMU (Non-secure only)\n");
  smmuEnableCommandProcessing();
  printf("smmuEnableCommandProcessing\n");  
  smmuEnableEventQueue();
  printf("smmuEnableEventQueue\n");  
  smmuEnableTranslation();
   printf("smmuEnableTranslation()\n"); 
    
  printf("main(): Intializing a context descriptor\n");  
  memset(CDBase, 0, CD_SZ);
  smmuInitBasicCD(CDBase,
                  TTBase,
                  TCR_T0SZ_1TiB, 
                  TCR_TG0_4KiB, //TCR_TG0_64KiB
                  SMMU_CD_NO_AF_FAULTS, 
                  SMMU_CD_IPS_40, 
                  SMMU_CD_AA64_AARCH64, 
                  1,//ASID//
                  SMMU_CD_TERMINATE);

  
  printf("main(): Initializing Stream Table entry as S1, referencing the context descriptor.\n");
  
	for (i = 1; i < SMMU_MASTER_MAX + 1; i++)
	{
		  smmuInitBasicSTE(i /* 65535 StreamID */,
                   SMMU_ST_BYPASS,
                   CDBase /*S1ContextPtr*/,
                   0 /*S1CDMax*/,
                   SMMU_ST_NO_STALLS,
                   SMMU_ST_STRW_EL2, //SMMU_ST_STRW_EL1,
                   0 /*S2VMID*/,
                   0 /*S2T0SZ*/,
                   0 /*S2TTB*/,
                   0 /*SL0*/,
                   0 /*S2TG*/,
                   0 /*S2PS*/);   
	}

  smmuInitBasicSTE(SDM_ID /* SDM StreamID */,
                   SMMU_ST_STAGE1_ONLY,
                   CDBase /*S1ContextPtr*/,
                   0 /*S1CDMax*/,
                   SMMU_ST_NO_STALLS,
                   SMMU_ST_STRW_EL2, //SMMU_ST_STRW_EL1,
                   0 /*S2VMID*/,
                   0 /*S2T0SZ*/,
                   0 /*S2TTB*/,
                   0 /*SL0*/,
                   0 /*S2TG*/,
                   0 /*S2PS*/);                

  //write DMA TBU stream ID same as we configure in TCU


 printf("main(): Finish Streme table setup\n");
//  smmuInvalidateSTE(SMM_CMD_NS_QUEUE, 0 /*StreamID*/, SMMU_CMD_SSEC_NS, SMMU_CMD_BRANCH);
  smmuInvalidateSTE(SMM_CMD_NS_QUEUE, 10 /*StreamID*/, SMMU_CMD_SSEC_NS, SMMU_CMD_BRANCH);
 printf("main(): Finish invalidate STE\n");
 // smmuSyncCmdQueue(SMM_CMD_NS_QUEUE, SMMU_SYNC_SIG_NONE, 0 /*MSH*/, 0 /*MSIAttr*/, 0 /*MSI*/, (void*)0 /*msi_addr*/);
  smmuSyncCmdQueue(SMM_CMD_NS_QUEUE, SMMU_SYNC_SIG_NONE, 0 /*MSH*/, 0 /*MSIAttr*/, 0 /*MSI*/, (void*)0 /*msi_addr*/);
  printf("main(): Finish SMMU setup\n");
  //dsb();
   printf("start to set up translation table\n");
   tt_init();
    printf("Successfully set up translation table\n");
    puts("This is HPS test.\n");
    //uint32_t error = 0;

    //set up exception handler
    //init_bmfw_exception_register(BMFW_EXP_SYNC_ERROR_SPX, &derr_exception_handler, (void*)&D1Cnt);
   // init_bmfw_exception_register(BMFW_EXP_SYS_ERROR_SPX, &derr_exception_handler, (void*)&D1Cnt);

   // give_ctrl_to_sdm(); //hps will wait here until SDM give back control


    return 0;
}






/**************************************************************************************************/


static inline void setup_smmu_firewall(void)
{
	u32 i;

	/* Off the DDR secure transaction for all TBU supported peripherals */
	for (i = SYSMGR_DMA0_SID_ADDR; i < SYSMGR_TSN2_SID_ADDR; i +=
	     SOCFPGA_NEXT_TBU_PERIPHERAL) {
		/* skip this, future use register */
		if (i == SYSMGR_USB3_SID_ADDR)
			continue;

		writel(SECURE_TRANS_RESET, (uintptr_t)i);
	}
}

void socfpga_init_smmu(void)
{
	setup_smmu_firewall();
	//psi_test();
}


