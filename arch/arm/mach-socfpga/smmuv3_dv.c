// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation. All rights reserved
 *
 */

#include <common.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smmuv3_dv.h>
#include <asm/arch/system_manager.h>
#include <linux/errno.h>

struct smmuv3_ns_s
{
  const volatile uint32_t SMMU_IDR0;              // +0x0000 - RO - ID Register 0
  const volatile uint32_t SMMU_IDR1;              // +0x0004 - RO - ID Register 1
  const volatile uint32_t SMMU_IDR2;              // +0x0008 - RO - ID Register 2
  const volatile uint32_t SMMU_IDR3;              // +0x000C - RO - ID Register 3
  const volatile uint32_t SMMU_IDR4;              // +0x0010 - RO - ID Register 4
  const volatile uint32_t SMMU_IDR5;              // +0x0014 - RO - ID Register 5

  const volatile uint32_t SMMU_IIDR;              // +0x0018 - RO - Implementation Identification Register
  const volatile uint32_t SMMU_AIDR;              // +0x001C - RO - Architecture Identification Register

        volatile uint32_t SMMU_CR0;               // +0x0020 - RW - Global Control Register 0
  const volatile uint32_t SMMU_CR0ACK;            // +0x0024 - RW - Global Control 0 Update Acknowledge Register
        volatile uint32_t SMMU_CR1;               // +0x0028 - RW - Global Control Register 1
        volatile uint32_t SMMU_CR2;               // +0x002C - RW - Global Control Register 2

  const volatile uint32_t padding0[4];            // +0x0030-0x003C - RESERVED

  const volatile uint32_t SMMU_STATUSR;           // +0x0040 - RW - Global Status Register

        volatile uint32_t SMMU_GBPA;              // +0x0044 - RW - Global Bypass Attribute register
        volatile uint32_t SMMU_AGBPA;             // +0x0048 - RW - Alternative Global Bypass Attribute register

  const volatile uint32_t padding1;               // +0x004C - RESERVED

        volatile uint32_t SMMU_IRQ_CTRL;          // +0x0050 - RW - Interrupt Control register
  const volatile uint32_t SMMU_IRQ_CTRLACK;       // +0x0054 - RW - Interrupt Control register update Acknowledge

  const volatile uint32_t padding2[2];            // +0x0058-0x005C - RESERVED

  const volatile uint32_t SMMU_GERROR;            // +0x0060 - RW - Global Error status
        volatile uint32_t SMMU_GERRORN;           // +0x0064 - RW - Global Error status acknowledgement

        volatile uint64_t SMMU_GERROR_IRQ_CFG0;   // +0x0068 - RW - Global Error IRQ Configuration register 0   ***64-bit***
        volatile uint32_t SMMU_GERROR_IRQ_CFG1;   // +0x0070 - RW - Global Error IRQ Configuration register 1
        volatile uint32_t SMMU_GERROR_IRQ_CFG2;   // +0x0074 - RW - Global Error IRQ Configuration register 2

  const volatile uint32_t padding3[2];            // +0x0078-0x007C - RESERVED

        volatile uint64_t SMMU_STRTAB_BASE;       // +0x0080 - RW - Stream Table Base address register   ***64-bit***
        volatile uint32_t SMMU_STRTAB_BASE_CFG;   // +0x0088 - RW - Stream Table Base Configuration register

  const volatile uint32_t padding4;               // +0x008C - RESERVED

        volatile uint64_t SMMU_CMDQ_BASE;         // +0x0090 - RW - Command Queue Base address register   ***64-bit***
        volatile uint32_t SMMU_CMDQ_PROD;         // +0x0098 - RW - Command Queue Consumer index register
        volatile uint32_t SMMU_CMDQ_CONS;         // +0x009C - RW - Command Queue Producer index register

        volatile uint64_t SMMU_EVENTQ_BASE;       // +0x00A0 - RW - Command Queue Producer index register  ***64-bit***

        volatile uint32_t ASMMU_EVENTQ_PROD;      // +0x00A8 - RW - Aliases SMMU_EVENTQ_PROD or is RAZ/WI
        volatile uint32_t ASMMU_EVENTQ_CONS;      // +0x00AC - RW - Aliases SMMU_EVENTQ_CONS or is RAZ/WI

        volatile uint64_t SMMU_EVENTQ_IRQ_CFG0;   // +0x00B0 - RW - Event Queue IRQ Configuration register 0  ***64-bit***
        volatile uint32_t SMMU_EVENTQ_IRQ_CFG1;   // +0x00B8 - RW - Event Queue IRQ Configuration register 1
        volatile uint32_t SMMU_EVENTQ_IRQ_CFG2;   // +0x00BC - RW - Event Queue IRQ Configuration register 2

        volatile uint64_t SMMU_PRIQ_BASE;         // +0x00C0 - RW - PRI Queue Base address register  ***64-bit***
        volatile uint32_t ASMMU_PRIQ_PROD;        // +0x00C8 - RW - Aliases PRIQ_PROD or is RAZ/WI
        volatile uint32_t ASMMU_PRIQ_CONS;        // +0x00CC - RW - Aliases PRIQ_CONS or is RAZ/WI

        volatile uint64_t SMMU_PRIQ_IRQ_CFG;      // +0x00D0 - RW - PRI Queue IRQ Configuration register 0  ***64-bit***
        volatile uint32_t SMMU_PRIQ_IRQ_CFG1;     // +0x00D8 - RW - PRI Queue IRQ Configuration register 1
        volatile uint32_t SMMU_PRIQ_IRQ_CFG2;     // +0x00DC - RW - PRI Queue IRQ Configuration register 2

  const volatile uint32_t padding5[8];            // +0x00E0-0x00FC - RESERVED

        volatile uint32_t SMMU_GATOS_CTRL;        // +0x0100 - RW - Global Address Translation Operations Control register

  const volatile uint32_t padding6;               // +0x0104 - RESERVED

        volatile uint64_t SMMU_GATOS_SID;         // +0x0108 - RW - Global Address Translation Operations StreamID configuration register
        volatile uint64_t SMMU_GATOS_ADDR;        // +0x0110 - RW - Global Address Translation Operations Address input register
  const volatile uint64_t SMMU_GATOS_PAR;         // +0x0118 - RW - Global Address Translation Operations Physical Address Result register

  const volatile uint32_t padding7[59];           // +0x011C-0x01FF - RESERVED

  const volatile uint32_t padding8[64];           // +0x0200-0x02FF - RESERVED
  const volatile uint32_t padding9[64];           // +0x0300-0x03FF - RESERVED
  const volatile uint32_t padding10[64];          // +0x0400-0x04FF - RESERVED
  const volatile uint32_t padding11[64];          // +0x0500-0x05FF - RESERVED
  const volatile uint32_t padding12[64];          // +0x0600-0x06FF - RESERVED
  const volatile uint32_t padding13[64];          // +0x0700-0x07FF - RESERVED
  const volatile uint32_t padding14[64];          // +0x0800-0x08FF - RESERVED
  const volatile uint32_t padding15[64];          // +0x0900-0x09FF - RESERVED
  const volatile uint32_t padding16[64];          // +0x0A00-0x0AFF - RESERVED
  const volatile uint32_t padding17[64];          // +0x0B00-0x0BFF - RESERVED
  const volatile uint32_t padding18[64];          // +0x0C00-0x0CFF - RESERVED
  const volatile uint32_t padding19[64];          // +0x0D00-0x0DFF - RESERVED
  const volatile uint32_t padding20[64];          // +0x0E00-0x0EFF - RESERVED
  const volatile uint32_t padding21[64];          // +0x0F00-0x0FFF - RESERVED

  const volatile uint32_t padding22[15360];       // +0x1000-0xFFFF - RESERVED

  const volatile uint32_t padding23[39];          // +0x10000-0x100A7 - RESERVED
  
        volatile uint32_t SMMU_EVENTQ_PROD;       // +0x100A8 - RW - SMMU_EVENTQ_PROD
        volatile uint32_t SMMU_EVENTQ_CONS;       // +0x100AC - RW - SMMU_EVENTQ_CONS


  // 0x0E00 - 0x0EFF - IMPLEMENTATION DEFINED registers
  // 0x0FD0 - 0x0FFC - IMPLEMENTATION DEFINED ID registers
};

struct smmuv3_s_s
{
  const volatile uint32_t SMMU_S_IDR0;            // +0x8000 - RO - Secure ID Register 0
  const volatile uint32_t SMMU_S_IDR1;            // +0x8004 - RO - Secure ID Register 1
  const volatile uint32_t SMMU_S_IDR2;            // +0x8008 - RO - Secure ID Register 2
  const volatile uint32_t SMMU_S_IDR3;            // +0x800C - RO - Secure ID Register 3
  const volatile uint32_t SMMU_S_IDR4;            // +0x8010 - RO - Secure ID Register 4

  const volatile uint32_t padding0[3];            // +0x8014 - RESERVED

        volatile uint32_t SMMU_S_CR0;             // +0x8020 - RW - Secure Global Control Register 0
  const volatile uint32_t SMMU_S_CR0ACK;          // +0x8024 - RW - Secure Global Control 0 Update Acknowledge Register
        volatile uint32_t SMMU_S_CR1;             // +0x8028 - RW - Secure Global Control Register 1
        volatile uint32_t SMMU_S_CR2;             // +0x802C - RW - Secure Global Control Register 2

  const volatile uint32_t padding1[3];            // +0x8030-0x8038 - RESERVED

        volatile uint32_t SMMU_S_INIT;            // +0x803C - RW - Secure Initialization control register

  const volatile uint32_t padding2;               // +0x8040 - RESERVED

        volatile uint32_t SMMU_S_GBPA;            // +0x8044 - RW - Secure Global Bypass Attribute register
        volatile uint32_t SMMU_S_AGBPA;           // +0x8048 - RW - Secure Alternative Global Bypass Attribute register

  const volatile uint32_t padding3;               // +0x804C - RESERVED

        volatile uint32_t SMMU_S_IRQ_CTRL;        // +0x8050 - RW - Secure Interrupt Control register
  const volatile uint32_t SMMU_S_IRQ_CTRLACK;     // +0x8054 - RW - Secure Interrupt Control register update Acknowledge

  const volatile uint32_t padding4[2];            // +0x8058-0x005C - RESERVED

  const volatile uint32_t SMMU_S_GERROR;          // +0x8060 - RW - Secure Global Error status
        volatile uint32_t SMMU_S_GERRORN;         // +0x8064 - RW - Secure Global Error status acknowledgement

        volatile uint64_t SMMU_S_GERROR_IRQ_CFG0; // +0x8068 - RW - Secure Global Error IRQ Configuration register 0   ***64-bit***
        volatile uint32_t SMMU_S_GERROR_IRQ_CFG1; // +0x8070 - RW - Secure Global Error IRQ Configuration register 1
        volatile uint32_t SMMU_S_GERROR_IRQ_CFG2; // +0x8074 - RW - Secure Global Error IRQ Configuration register 2

  const volatile uint32_t padding5[2];            // +0x8078-0x807C - RESERVED

        volatile uint64_t SMMU_S_STRTAB_BASE;     // +0x8080 - RW - Secure Stream Table Base address register   ***64-bit***
        volatile uint32_t SMMU_S_STRTAB_BASE_CFG; // +0x8088 - RW - Secure Stream Table Base Configuration register

  const volatile uint32_t padding6;               // +0x808C - RESERVED

        volatile uint64_t SMMU_S_CMDQ_BASE;       // +0x8090 - RW - Secure Command Queue Base address register   ***64-bit***
        volatile uint32_t SMMU_S_CMDQ_PROD;       // +0x8098 - RW - Secure Command Queue Consumer index register
        volatile uint32_t SMMU_S_CMDQ_CONS;       // +0x809C - RW - Secure Command Queue Producer index register

        volatile uint64_t SMMU_S_EVENTQ_BASE;     // +0x80A0 - RW - Secure Command Queue Producer index register  ***64-bit***
        volatile uint32_t SMMU_S_EVENTQ_PROD;     // +0x80A8 - RW - Secure Event Queue Producer index register
        volatile uint32_t SMMU_S_EVENTQ_CONS;     // +0x80AC - RW - Secure vent Queue Consumer index register

        volatile uint64_t SMMU_S_EVENTQ_IRQ_CFG0; // +0x80B0 - RW - Secure Event Queue IRQ Configuration register 0  ***64-bit***
        volatile uint32_t SMMU_S_EVENTQ_IRQ_CFG1; // +0x80B8 - RW - Secure Event Queue IRQ Configuration register 1
        volatile uint32_t SMMU_S_EVENTQ_IRQ_CFG2; // +0x80BC - RW - Secure Event Queue IRQ Configuration register 2

  const volatile uint32_t padding7[16];           // +0x80C0-0x80FC - RESERVED

        volatile uint32_t SMMU_S_GATOS_CTRL;      // +0x8100 - RW - Global Address Translation Operations Control register

  const volatile uint32_t padding8;               // +0x8104 - RESERVED

        volatile uint64_t SMMU_S_GATOS_SID;       // +0x8108 - RW - Global Address Translation Operations StreamID configuration register
        volatile uint64_t SMMU_S_GATOS_ADDR;      // +0x8110 - RW - Global Address Translation Operations Address input register
  const volatile uint64_t SMMU_S_GATOS_PAR;       // +0x8118 - RW - Global Address Translation Operations Physical Address Result register

// 0x0E00 - 0x0EFF - Secure IMPLEMENTATION DEFINED registers
// 0x0FD0 - 0x0FFC - Secure IMPLEMENTATION DEFINED ID registers
};

//
// Stream Table Entry
//
struct ste_s
{
  struct
  {
    uint32_t valid        :1;
    uint32_t Config       :3;
    uint32_t S1Fmt        :2;
    uint64_t S1ContextPtr :42;
    uint32_t              :11;
    uint32_t S1CDMax      :5;
  } d0;

  struct
  {
    uint32_t S1DSS        :2;
    uint32_t S1CIR        :2;
    uint32_t S1COR        :2;
    uint32_t S1CSH        :2;
    uint32_t              :4;
    uint32_t DRE          :1;
    uint32_t CONT         :4;
    uint32_t DCP          :1;
    uint32_t PPAR         :1;
    uint32_t MEV          :1;
    uint32_t              :7;
    uint32_t S1STALLD     :1;
    uint32_t EATS         :2;
    uint32_t STRW         :2;
    uint32_t MemAttr      :4;
    uint32_t MTCFG        :1;
    uint32_t ALLOCCFG     :4;
    uint32_t              :3;
    uint32_t SHCFG        :2;
    uint32_t NSCFG        :2;
    uint32_t PRIVCFG      :2;
    uint32_t INSTCFG      :2;
    uint32_t              :12;
  } d1;

  struct
  {
    uint32_t S2VMID       :16;
    uint32_t IMPDEF       :16;
    uint32_t S2T0SZ       :6;
    uint32_t S2SL0        :2;
    uint32_t S2IR0        :2;
    uint32_t S2OR0        :2;
    uint32_t S2SH0        :2;
    uint32_t S2TG         :2;
    uint32_t S2PS         :3;
    uint32_t S2AA64       :1;
    uint32_t S2ENDI       :1;
    uint32_t S2AFFD       :1;
    uint32_t S2PTW        :1;
    uint32_t S2HD         :1;
    uint32_t S2HA         :1;
    uint32_t S2S          :1;
    uint32_t S2R          :1;
    uint32_t              :5;
  } d2;
  
  uint64_t d3;
  uint64_t d4;
  uint64_t d5;
  uint64_t d6;
  uint64_t d7;
};


//
// Context descriptor
//
struct cd_s
{
  struct
  {
    uint32_t T0SZ         :6;
    uint32_t TG0          :2;
    uint32_t IR0          :2;
    uint32_t OR0          :2;
    uint32_t SH0          :2;
    uint32_t EPD0         :1;
    uint32_t ENDI         :1;
    uint32_t T1SZ         :6;
    uint32_t TG1          :2;
    uint32_t IR1          :2;
    uint32_t OR1          :2;
    uint32_t SH1          :2;
    uint32_t EPD1         :1;
    uint32_t valid        :1;
    uint32_t IPS          :3;
    uint32_t AFFD         :1;
    uint32_t WXN          :1;
    uint32_t UWXN         :1;
    uint32_t TBI          :2;
    uint32_t PAN          :1;
    uint32_t AA64         :1;
    uint32_t HD           :1;
    uint32_t HA           :1;
    uint32_t ARS          :3;
    uint32_t ASET         :1;
    uint32_t ASID         :16;
  } d0;

  struct
  {
    uint32_t NSCFG0       :1;
    uint32_t HAD0         :1;
    uint32_t              :2;
    uint64_t TTB0         :48;
    uint32_t              :12;
  } d1;

  struct
  {
    uint32_t NSCFG1       :1;
    uint32_t HAD1         :1;
    uint32_t              :2;
    uint64_t TTB1         :48;
    uint32_t              :12;
  } d2;

  struct
  {
    uint32_t MAIR0        :8;
    uint32_t MAIR1        :8;
    uint32_t MAIR2        :8;
    uint32_t MAIR3        :8;
    uint32_t MAIR4        :8;
    uint32_t MAIR5        :8;
    uint32_t MAIR6        :8;
    uint32_t MAIR7        :8;
  }d3;

  struct
  {
    uint32_t AMAIR0       :8;
    uint32_t AMAIR1       :8;
    uint32_t AMAIR2       :8;
    uint32_t AMAIR3       :8;
    uint32_t AMAIR4       :8;
    uint32_t AMAIR5       :8;
    uint32_t AMAIR6       :8;
    uint32_t AMAIR7       :8;
  }d4;

  struct
  {
    uint32_t IMPDEF       :32;
    uint32_t              :32;
  }d5;

  uint64_t d6;
  uint64_t d7;
};


//
// Pointers
//

static struct smmuv3_ns_s*   smmu0;
static struct smmuv3_s_s*    smmu1;

static struct ste_s*         st;

static uint64_t*             pCMDQ;
static uint64_t*             pEVENTQ;

// ------------------------------------------------------------
// Address and ID information
// ------------------------------------------------------------

void smmuSetBaseAddr(void* addr)
{
  smmu0 = (struct smmuv3_ns_s*)addr;

  // The Secure registers are at an offset of 0x8000
  smmu1 = (struct smmuv3_s_s*)((uint64_t) addr + 0x8000);
	printf("smmu1_S_IDR1.secure : 0x%lx\n", smmu1->SMMU_S_IDR1 & BIT(31));
  return;
}


// Returns the value of SMMU_IDR0.ST_LEVEL
uint32_t smmuGetMultiLevelSupport(void)
{
  uint32_t ST_LEVEL;

  ST_LEVEL = 0x3 & (smmu0->SMMU_IDR0 >> 27);

  return ST_LEVEL;
}


// Returns the value of SMMU_IDR0.TERM_MODEL
uint32_t smmuGetTerminationModel(void)
{
  uint32_t TERM_MODEL;

  TERM_MODEL = 0x1 & (smmu0->SMMU_IDR0 >> 26);

  return TERM_MODEL;
}


// Returns the value of SMMU_IDR0.STALL_MODEL
uint32_t smmuGetStallModel(void)
{
  uint32_t STALL_MODEL;

  STALL_MODEL = 0x3 & (smmu0->SMMU_IDR0 >> 24);

  return STALL_MODEL;
}


// Returns the value of SMMU_IDR0.TTENDIAN
uint32_t smmuGetEndianness(void)
{
  uint32_t TTENDIAN;

  TTENDIAN = 0x3 & (smmu0->SMMU_IDR0 >> 21);

  return TTENDIAN;
}


// Returns the value of SMMU_IDR0.VMID16
uint32_t smmuGetVMIDSize(void)
{
  uint32_t VMID16;

  VMID16 = 0x1 & (smmu0->SMMU_IDR0 >> 18);

  return VMID16;
}


// Returns the value of SMMU_IDR0.VMW
uint32_t smmuGetVMW(void)
{
  uint32_t VMW;

  VMW = 0x1 & (smmu0->SMMU_IDR0 >> 17);

  return VMW;
}


// Returns the value of SMMU_IDR0.ASID16
uint32_t smmuGetASIDSize(void)
{
  uint32_t ASID16;

  ASID16 = 0x1 & (smmu0->SMMU_IDR0 >> 12);

  return ASID16;
}


// Returns the value of SMMU_IDR0.HTTU
uint32_t smmuGetHTTU(void)
{
  uint32_t HTTU;

  HTTU = 0x3 & (smmu0->SMMU_IDR0 >> 6);

  return HTTU;
}


// Returns the value of SMMU_IDR0.S1P
uint32_t smmuGetStage1Support(void)
{
  uint32_t S1P;

  S1P = 0x1 & (smmu0->SMMU_IDR0 >> 1);

  return S1P;
}


// Returns the value of SMMU_IDR0.S2P
uint32_t smmuGetStage2Support(void)
{
  uint32_t S2P;

  S2P = 0x1 & smmu0->SMMU_IDR0;

  return S2P;
}


// Returns the value of SMMU_IDR1.TABLES_PRESET
uint32_t smmuGetTablesPreset(void)
{
  uint32_t TABLES_PRESET;

  TABLES_PRESET = 0x1 & (smmu0->SMMU_IDR1 >> 30);

  return TABLES_PRESET;
}


// Returns the value of SMMU_IDR1.QUEUES_PRESET
uint32_t smmuGetQueuesPreset(void)
{
  uint32_t QUEUES_PRESET;

  QUEUES_PRESET = 0x1 & (smmu0->SMMU_IDR1 >> 29);

  return QUEUES_PRESET;
}


// Returns the value of SMMU_IDR1.ATTR_TYPES_OVR
bool smmuIsAttributeOverrideInBypassSupported(void)
{
  return (((smmu0->SMMU_IDR1 >> 27) & 0x1) == 1);
}


// Returns the value of SMMU_IDR1.SSIDSIZE
uint32_t smmuGetSubStreamIDBits(void)
{
  return ((smmu0->SMMU_IDR1 >> 6) & 0x1F);
}


// Returns the value of SMMU_IDR1.SIDSIZE
uint32_t smmuGetStreamIDBits(void)
{
  return (smmu0->SMMU_IDR1 & 0x3F);
}


// Returns the address offset of the SMMU)IDR2_VATOS, or 0 if SMMU_IDR0.VATOS==0
uint64_t smmuGetVATOSAddrOffset(void)
{
  if (((smmu0->SMMU_IDR0 >> 20) & 0x1) == 1)
    return (((uint64_t)(smmu0->SMMU_IDR2 & 0x1FF) * (uint64_t)0x10000) + (uint64_t)0x20000);
  else
    return 0x0;
}


// Returns the value of SMMU_IDR3.HAD
bool smmuIsHierarchicalAttributeDisableSupported(void)
{
  return (((smmu0->SMMU_IDR3 >> 2) & 0x1) == 1);
}


// Returns the value of SMMU_IDR4
uint32_t smmuGetIDR4(void)
{
  return smmu0->SMMU_IDR4;
}


// Returns the value of SMMU_IDR5.STALL_MAX
uint32_t smmuGetMaxStalls(void)
{
  uint32_t STALL_MAX;

  STALL_MAX = 0xFF & (smmu0->SMMU_IDR5 >> 16);

  return STALL_MAX;
}



// Returns the value of SMMU_IDR5.GRAN64K
bool smmuGet64KGranuleSupport(void)
{
  uint32_t GRAN64K;

  GRAN64K = 0x1 & (smmu0->SMMU_IDR5 >> 6);

  return (GRAN64K == 1);
}


// Returns the value of SMMU_IDR5.GRAN16K
bool smmuGet16KGranuleSupport(void)
{
  uint32_t GRAN16K;

  GRAN16K = 0x1 & (smmu0->SMMU_IDR5 >> 6);

  return (GRAN16K == 1);
}


// Returns the value of SMMU_IDR5.GRAN4K
bool smmuGet4KGranuleSupport(void)
{
  uint32_t GRAN4K;

  GRAN4K = 0x1 & (smmu0->SMMU_IDR5 >> 6);

  return (GRAN4K == 1);
}


// Returns the value of SMMU_IDR5.OAS
uint32_t smmuGetOutputAddrSize(void)
{
  uint32_t OAS;

  OAS = 0x7 & smmu0->SMMU_IDR5;

  return OAS;
}


// Returns the value of SMMU_IIDR
uint32_t smmuGetIIDR(void)
{
  return smmu0->SMMU_IIDR;
}


// Returns the value of SMMU_AIDR.[ArchMajorRev:ArchMinorRev]
uint32_t smmuGetArchVersion(void)
{
  uint32_t arch;

  arch = 0xF & smmu0->SMMU_AIDR;

  return arch;
}

void smmu_enable_IRQ(void){
 smmu0->SMMU_IRQ_CTRL = 0x7;
 smmu1->SMMU_S_IRQ_CTRL = 0x7;
}

// ------------------------------------------------------------
// Top level MMU Settings (NS)
// ------------------------------------------------------------

// Sets the base address and size of the Stream Table
// addr             = PA of stream table, address must be size aligned/
// ra_hint          = Read allocate hint
// stream_id_width  = Width of stream ID, from which the table size if derived as 2^stream_id_width
//
// NOTE: This driver only supports linear tables
void smmuInitStreamTable(void* addr, uint32_t ra_hint, uint32_t stream_id_width)
{
  ra_hint         = ra_hint & 0x1;
  stream_id_width = stream_id_width & 0x1F;

  smmu0->SMMU_STRTAB_BASE = ((uint64_t)addr | ((uint64_t)ra_hint << 62));
  smmu0->SMMU_STRTAB_BASE_CFG = (SMMU_ST_LINEAR_TABLE | stream_id_width);

  // Record the location of the stream table for later uses
  st = (struct ste_s*)addr;

  return;
}

void smmuInitStreamTable_S(void* addr, uint32_t ra_hint, uint32_t stream_id_width)
{
  ra_hint         = ra_hint & 0x1;
  stream_id_width = stream_id_width & 0x1F;

  smmu1->SMMU_S_STRTAB_BASE = ((uint64_t)addr | ((uint64_t)ra_hint << 62));
  smmu1->SMMU_S_STRTAB_BASE_CFG = (SMMU_ST_LINEAR_TABLE | stream_id_width);

  // Record the location of the stream table for later uses
  st = (struct ste_s*)addr;

  return;
  
}

// Sets the base address and size of the Command Queue
// addr             = PA of stream table, address must be size aligned/
// ra_hint          = read allocate hint
// queue_size       = Log2 of the queue size, such that num_entries = 2^queue_size
void smmuInitCommandQueue(void* addr, uint32_t ra_hint, uint32_t queue_size)
{
  ra_hint    = ra_hint & 0x1;
  queue_size = queue_size & 0x1F;

  smmu0->SMMU_CMDQ_BASE = ((uint64_t)addr | ((uint64_t)ra_hint << 62)| queue_size);

  smmu0->SMMU_CMDQ_CONS = 0;
  smmu0->SMMU_CMDQ_PROD = 0;

  // Record an internal copy of the Command Queue base address
  pCMDQ = (uint64_t*)addr;

  return;
}

void smmuInitCommandQueue_S(void* addr, uint32_t ra_hint, uint32_t queue_size)
{
  ra_hint    = ra_hint & 0x1;
  queue_size = queue_size & 0x1F;

  smmu1->SMMU_S_CMDQ_BASE = ((uint64_t)addr | ((uint64_t)ra_hint << 62)| queue_size);

  smmu1->SMMU_S_CMDQ_CONS = 0;
  smmu1->SMMU_S_CMDQ_PROD = 0;

  // Record an internal copy of the Command Queue base address
  pCMDQ = (uint64_t*)addr;

  return;
}



// Sets the base address and size of the Event Queue
// addr             = PA of stream table
// wa_hint          = Write allocate hint
// queue_size       = Log2 of the queue size, such that num_entries = 2^queue_size
void smmuInitEventQueue(void* addr, uint32_t wa_hint, uint32_t queue_size)
{
  wa_hint    = wa_hint & 0x1;
  queue_size = queue_size & 0x1F;

  smmu0->SMMU_EVENTQ_BASE = ((uint64_t)addr | ((uint64_t)wa_hint << 62) | queue_size);

  smmu0->ASMMU_EVENTQ_CONS = 0;
  smmu0->ASMMU_EVENTQ_PROD = 0;

  pEVENTQ = (uint64_t*)addr;

  return;
}

void smmuInitEventQueue_S(void* addr, uint32_t wa_hint, uint32_t queue_size)
{
  wa_hint    = wa_hint & 0x1;
  queue_size = queue_size & 0x1F;

  smmu1->SMMU_S_EVENTQ_BASE = ((uint64_t)addr | ((uint64_t)wa_hint << 62) | queue_size);

  smmu1->SMMU_S_EVENTQ_CONS = 0;
  smmu1->SMMU_S_EVENTQ_PROD = 0;

  pEVENTQ = (uint64_t*)addr;

  return;
}

// Sets the cacheability and shareability attributes used for ST, CQ and EQ accesses
// inner_cache  = Inner cacheability attribute
// outer_cache  = Outer cacheability attribute
// shareability = Shareability attribute
//
// Note: This driver sets the same attributes for queues and tables
void smmuInitTableAttributes(uint32_t inner_cache, uint32_t outer_cache, uint32_t shareability)
{
  inner_cache  = inner_cache  & 0x3;
  outer_cache  = outer_cache  & 0x3;
  shareability = shareability & 0x3;

  smmu0->SMMU_CR1 = ((shareability << 10) | (outer_cache << 8) | (inner_cache << 6) |   /*table*/
                     (shareability << 4)  | (outer_cache << 2) | (inner_cache));        /*queue*/
  return;
}

void smmuInitTableAttributes_S(uint32_t inner_cache, uint32_t outer_cache, uint32_t shareability)
{
  inner_cache  = inner_cache  & 0x3;
  outer_cache  = outer_cache  & 0x3;
  shareability = shareability & 0x3;

  smmu1->SMMU_S_CR1 = ((shareability << 10) | (outer_cache << 8) | (inner_cache << 6) |   /*table*/
                     (shareability << 4)  | (outer_cache << 2) | (inner_cache));        /*queue*/
  return;
}



// Enables command queue processing
void smmuEnableCommandProcessing(void)
{
  smmu0->SMMU_CR0 = (smmu0->SMMU_CR0 | (1 << 3));  // Sets CMDQEN

  // Poll SMMU_CR0ACK until the CMDQEN shows as being set
  while ((smmu0->SMMU_CR0ACK & (1 << 3)) == 0)
  {}

  return;
}
// Enables command queue processing
void smmuEnableCommandProcessing_S(void)
{
  smmu1->SMMU_S_CR0 = (smmu1->SMMU_S_CR0 | (1 << 3));  // Sets CMDQEN

  // Poll SMMU_CR0ACK until the CMDQEN shows as being set
  while ((smmu1->SMMU_S_CR0ACK & (1 << 3)) == 0)
  {}

  return;
}


//TBD - a disable counterpart.


// Enables Event Queue writes
void smmuEnableEventQueue(void)
{
  smmu0->SMMU_CR0 = (smmu0->SMMU_CR0 | (1 << 2));  // Sets EVENTQEN

  // Poll SMMU_CR0ACK until the EVENTQEN shows as being set
  while ((smmu0->SMMU_CR0ACK & (1 << 2)) == 0)
  {}

  return;
}

void smmuEnableEventQueue_S(void)
{
  smmu1->SMMU_S_CR0 = (smmu1->SMMU_S_CR0 | (1 << 2));  // Sets EVENTQEN

  // Poll SMMU_CR0ACK until the EVENTQEN shows as being set
  while ((smmu1->SMMU_S_CR0ACK & (1 << 2)) == 0)
  {}

  return;
}
//TBD - a disable counterpart.


// Enable translation of Non-secure streams
// all non-secure transactions are checked against configuration structures
// and may (if the relevant STE enables it) undergo translation.
void smmuEnableTranslation(void)
{
  smmu0->SMMU_CR0 = (smmu0->SMMU_CR0 | 1);         // Set SMMUEN

  // Poll SMMU_CR0ACK until the SMMUEN shows as being set
  while ((smmu0->SMMU_CR0ACK & 1) == 0)
  {}

  return;
}

void smmuEnableTranslation_S(void)
{
  smmu1->SMMU_S_CR0 = (smmu1->SMMU_S_CR0 | 1);         // Set SMMUEN

  // Poll SMMU_CR0ACK until the SMMUEN shows as being set
  while ((smmu1->SMMU_S_CR0ACK & 1) == 0)
  {}

  return;
}


//TBD - a disable counterpart.


// Sets behaviour of SMMU when in by-pass mode (SMMU_CR0.SMMUEN==0)
// abort     = Whether incoming accesses should abort, or be passed through
// INSTCFG   = Instruction/data override
// PRIVCFG   = User/privileged override
// SHCFG     = Shareability override
// ALLOCCFG  = Allocation/transient hints override
// MTCFG     = Memory type override
// MemAttr   = MemAttr to be used when MTCFG==SMMU_GBPA_MEMTYPE_OVERRIDE
//
// NOTE: when abort==SMMU_GBPA_ABORT, all other parameters are ignored
void smmuSetByPassBehaviour(uint32_t abort, uint32_t INSTCFG, uint32_t PRIVCFG, uint32_t SHCFG, uint32_t ALLOCCFG, uint32_t MTCFG, uint32_t MemAttr)
{
  if (abort == SMMU_GBPA_ABORT)
    smmu0->SMMU_GBPA = SMMU_GBPA_ABORT;  // If abort model selected, other parameters are "don't care"
  else
    smmu0->SMMU_GBPA = (INSTCFG | PRIVCFG | SHCFG | ALLOCCFG | MTCFG | MemAttr);

  return;
}


// Invalidate all internal caches
// This function causes a SMMU-global invalidation to be performed for all configuration
// and translation caches for all translation regimes and security worlds.
//
// Calling this function when the SMMU is enabled (SMMU_CR0.SMMUEN==1) is CONSTRAINED UNPREDICTABLE
void smmuInvalidateAll(void)
{
  if ((smmu0->SMMU_CR0 & 0x1) == 1)
    printf("smmuInvalidateAll: ERROR - Write to SMMU_C_INIT when SMMU_CR0.SMMUEN==1, this is CONSTRAINED UNPREDICTABLE\n");

  if ((smmu1->SMMU_S_CR0 & 0x1) == 1)
    printf("smmuInvalidateAll: ERROR - Write to SMMU_C_INIT when SMMU_S_CR0.SMMUEN==1, this is CONSTRAINED UNPREDICTABLE\n");

  smmu1->SMMU_S_INIT = 1;

  // Poll until complete
  while ((smmu1->SMMU_S_INIT & 1) == 1)
  {}

  return;
}
void clearall(void)
{
  smmu0->SMMU_CR0 = 0;

  smmu1->SMMU_S_CR0  = 0;

  smmu1->SMMU_S_INIT = 1;

  // Poll until complete
  while ((smmu1->SMMU_S_INIT & 1) == 1)
  {}

  return;
}

// ------------------------------------------------------------
// Secure Configuration
// ------------------------------------------------------------

// Sets behaviour of SMMU when in by-pass mode (SMMU_S_CR0.SMMUEN==0)
// abort     = Whether incoming accesses should abort, or be passed through
// INSTCFG   = Instruction/data override
// PRIVCFG   = User/privileged override
// SHCFG     = Shareability override
// ALLOCCFG  = Allocation/transient hints override
// MTCFG     = Memory type override
// MemAttr   = MemAttr to be used when MTCFG==SMMU_GBPA_MEMTYPE_OVERRIDE
//
// NOTE: when abort==SMMU_GBPA_ABORT, all other parameters are ignored
void smmuSetByPassBehaviour_S(uint32_t abort, uint32_t INSTCFG, uint32_t PRIVCFG, uint32_t SHCFG, uint32_t ALLOCCFG, uint32_t MTCFG, uint32_t MemAttr)
{
  if (abort == SMMU_GBPA_ABORT)
    smmu1->SMMU_S_GBPA = SMMU_GBPA_ABORT;  // If abort model selected, other parameters are "don't care"
  else
    smmu1->SMMU_S_GBPA = (INSTCFG | PRIVCFG | SHCFG | ALLOCCFG | MTCFG | MemAttr);

  return;
}


// ------------------------------------------------------------
// Stream table related functions
// ------------------------------------------------------------

// Generates a Stream Table Entry (STE), and inserts into the Stream Table (ST)

// NOTE: As a simplification, this function uses fixed values for the following STE fields:
// S1Format = b00   (Two or more Context Descriptors in a linear table indexed by SubstreamID, IGNORED when S1CDMax==0)
// S1DSS    = b00   (terminate)
// S1CIR    = b01   (Normal, Write-Back cacheable, read-allocate)
// S1COR    = b01   (Normal, Write-Back cacheable, read-allocate)
// S1CSH    = b11   (Inner shareable)
// DRE      = b0    (3.1: A device request to consume data using a ‘read and invalidate’ transaction is transformed to a read without a data-destructive side-effect.)
// CONT     = b0
// DCP      = b0    (3.1: Directed cache prefetch operations are inhibited)
// PPAR     = b0
// MEV      = b0    (Fault records are not merged)
// EATS     = b00   (ATS Translation Requests are returned unsuccessful/aborted)
// MTCFG    = b0    (Use incoming type/cacheability)
// MemAttr  = b0    (A "don't care, because MTCFG==0)
// ALLOCCFG = b1110 (read/write allocate)
// SHCFG    = b01   (Use incoming shareability attribute)
// NSCFG    = b00   (Use incoming NS attribute)
// PRIVCFG  = b00   (Use incoming NS attribute)
// INSTCFG  = b00   (Use incoming NS attribute)
// S2IR0    = b01   (Write-back cacheable, read-allocate, write-allocate)
// S2OR0    = b01   (Write-back cacheable, read-allocate, write-allocate)
// S2SH0    = b11   (Inner shareable)
// S2AA64   = b1    (AArch64)
// S2ENDI   = b0    (LE)
// S2AFFD   = b1    (An AccessFlag fault never occurs)
// S2PTW    = b0    (Stage 1 translation table walks allowed to any valid Stage 2 address)
void smmuInitBasicSTE(const uint32_t index, const uint32_t config, void* S1ContextPtr, const uint32_t S1CDMax, const uint32_t S1STALLD, const uint32_t STRW, const uint32_t S2VMID, const uint32_t S2T0SZ, void* S2TTB, uint32_t SL0, const uint32_t S2TG, const uint32_t S2PS)
{
  //
  // TBD: implement limit checking on index, and NULL pointer check
  //
  

  // Ensure valid bit is clear while we program the STE
  st[index].d0.valid         = 0;
  // dsb();

  // Populate STE
  st[index].d0.Config        = config;
  st[index].d0.S1ContextPtr  = ((uint64_t)S1ContextPtr >> 6);  // Using a bit field means this shift is necessary
  st[index].d0.S1CDMax       = S1CDMax;
  st[index].d0.S1Fmt         = 0;                              // IGNORED when S1CDMax=0

  st[index].d1.S1DSS         = 0;
  st[index].d1.S1CIR         = 0x3;
  st[index].d1.S1COR         = 0x0;
  st[index].d1.S1CSH         = 0x3;
  st[index].d1.DRE           = 0x0;
  st[index].d1.CONT          = 0x0;
  st[index].d1.DCP           = 0x0;
  st[index].d1.PPAR          = 0x0;
  st[index].d1.MEV           = 0x0;
  st[index].d1.S1STALLD      = S1STALLD;
  st[index].d1.EATS          = 0x1;
  st[index].d1.STRW          = 0x2;
  st[index].d1.MTCFG         = 0x0;
  st[index].d1.MemAttr       = 0x0;
  st[index].d1.ALLOCCFG      = 0xE;
  st[index].d1.SHCFG         = 0x3;
  st[index].d1.NSCFG         = 0x2;//vraval change 1 to 2
  st[index].d1.PRIVCFG       = 0x1;//vraval changed 1 to 3
  st[index].d1.INSTCFG       = 0x0;

  st[index].d2.S2VMID        = S2VMID;
  st[index].d2.S2T0SZ        = S2T0SZ;

  st[index].d2.S2SL0         = SL0;
  st[index].d2.S2IR0         = 0x1;
  st[index].d2.S2OR0         = 0x1;
  st[index].d2.S2SH0         = 0x3;
  st[index].d2.S2TG          = S2TG;
  st[index].d2.S2PS          = S2PS;
  st[index].d2.S2AA64        = 0x1;
  st[index].d2.S2ENDI        = 0x0;
  st[index].d2.S2AFFD        = 0x1;
  st[index].d2.S2PTW         = 0x0;
  st[index].d2.IMPDEF        = 0xf;

  st[index].d3               = (uint64_t)S2TTB;

  // dsb();

  // Only set Valid to true once other fields are programmed
  st[index].d0.valid         = 1;

  // dsb();

  //#ifdef DEBUG
  printf("smmuInitBasicSTE:: Created STE: \n");
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[1],  ((uint32_t*)&st[index])[0]);   // d0
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[3],  ((uint32_t*)&st[index])[2]);   // d1
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[5],  ((uint32_t*)&st[index])[4]);   // d2
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[7],  ((uint32_t*)&st[index])[6]);   // d3
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[9],  ((uint32_t*)&st[index])[8]);   // d4
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[11], ((uint32_t*)&st[index])[10]);  // d5
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[13], ((uint32_t*)&st[index])[12]);  // d6
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[15], ((uint32_t*)&st[index])[14]);  // d7
 // #endif

  return;
}
void setpnu(const uint32_t index){
 st[index].d1.PRIVCFG       = 0x3;
}

void smmuInitBasicSTE_S(const uint32_t index, const uint32_t config, void* S1ContextPtr, const uint32_t S1CDMax, const uint32_t S1STALLD, const uint32_t STRW, const uint32_t S2VMID, const uint32_t S2T0SZ, void* S2TTB, uint32_t SL0, const uint32_t S2TG, const uint32_t S2PS)
{
  //
  // TBD: implement limit checking on index, and NULL pointer check
  //
  

  // Ensure valid bit is clear while we program the STE
  st[index].d0.valid         = 0;
  // dsb();

  // Populate STE
  st[index].d0.Config        = config;
  st[index].d0.S1ContextPtr  = ((uint64_t)S1ContextPtr >> 6);  // Using a bit field means this shift is necessary
  st[index].d0.S1CDMax       = S1CDMax;
  st[index].d0.S1Fmt         = 0;                              // IGNORED when S1CDMax=0

  st[index].d1.S1DSS         = 0;
  st[index].d1.S1CIR         = 0x3;
  st[index].d1.S1COR         = 0x0;
  st[index].d1.S1CSH         = 0x3;
  st[index].d1.DRE           = 0x0;
  st[index].d1.CONT          = 0x0;
  st[index].d1.DCP           = 0x1;
  st[index].d1.PPAR          = 0x0;
  st[index].d1.MEV           = 0x0;
  st[index].d1.S1STALLD      = S1STALLD;
  st[index].d1.EATS          = 0x1;
  st[index].d1.STRW          = 0x1;
  st[index].d1.MTCFG         = 0x1;
  st[index].d1.MemAttr       = 0x2;
  st[index].d1.ALLOCCFG      = 0xE;
  st[index].d1.SHCFG         = 0x3;
  st[index].d1.NSCFG         = 0x2;//vraval change 1 to 2
  st[index].d1.PRIVCFG       = 0x1;//vraval changed 1 to 3
  st[index].d1.INSTCFG       = 0x0;

  st[index].d2.S2VMID        = S2VMID;
  st[index].d2.S2T0SZ        = S2T0SZ;

  st[index].d2.S2SL0         = SL0;
  st[index].d2.S2IR0         = 0x1;
  st[index].d2.S2OR0         = 0x1;
  st[index].d2.S2SH0         = 0x3;
  st[index].d2.S2TG          = S2TG;
  st[index].d2.S2PS          = S2PS;
  st[index].d2.S2AA64        = 0x1;
  st[index].d2.S2ENDI        = 0x0;
  st[index].d2.S2AFFD        = 0x1;
  st[index].d2.S2PTW         = 0x0;

  st[index].d3               = (uint64_t)S2TTB;

  // dsb();

  // Only set Valid to true once other fields are programmed
  st[index].d0.valid         = 1;

  // dsb();

  #ifdef DEBUG
  printf("smmuInitBasicSTE:: Created STE: \n");
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[1],  ((uint32_t*)&st[index])[0]);   // d0
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[3],  ((uint32_t*)&st[index])[2]);   // d1
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[5],  ((uint32_t*)&st[index])[4]);   // d2
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[7],  ((uint32_t*)&st[index])[6]);   // d3
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[9],  ((uint32_t*)&st[index])[8]);   // d4
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[11], ((uint32_t*)&st[index])[10]);  // d5
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[13], ((uint32_t*)&st[index])[12]);  // d6
  printf("  0x%08x %08x\n", ((uint32_t*)&st[index])[15], ((uint32_t*)&st[index])[14]);  // d7
  #endif

  return;
}

// Clears the valid bit in a Stream Table Entry (STE)
void smmuClearSTE(uint32_t index)
{
  st[index].d0.valid         = 0;
  // dsb();

  return;
}


// ------------------------------------------------------------
// Context descriptor related functions
// ------------------------------------------------------------


// Generates a Context Descriptor(CD), which can be referenced by a Stream Table Entry (STE)

// NOTE: As a simplification, this function uses fixed values for the following CD fields:
// IR0    = b01   (TTB0: Write-back cacheable, read-allocate, write-allocate)
// OR0    = b01   (TTB0: Write-back cacheable, read-allocate, write-allocate)
// SH0    = b11   (TTB0: Inner shareable)
// EPD0   = b0    (TTB0: Table walks to TTB0 allowed)
// EPD1   = b1    (No table walks to TTB1)
// ENDI   = b0    (Little endian table walks)
// T1SZ   = 32    (VA space covered by TTB1 is 32-bit)
// TG1    = b10   (TTB1 uses 4K granule)
// IR1    = b01   (TTB1: Write-back cacheable, read-allocate, write-allocate)
// OR1    = b01   (TTB1: Write-back cacheable, read-allocate, write-allocate)
// SH1    = b11   (TTB1: Inner shareable)
// WXN    = b0    (Instruction fetches permitted to write-able memory)
// UWXN   = b0    (AArch32 only: instruction fetches permitted to EL0 write-able memory)
// TBI    = b00   (Top-byte ignore disabled)
// PAN    = b0
// HA:HD  = b00   (HA:HD => HTTU disabled)
// ASET   = b1
// NSCFG0 = b1    (Secure CDs only: First level TTB0 descriptor fetches are Secure)
// HAD0   = b1    (TTB0: Hierarchical attributes are disabled)
// NSCFG1 = b1    (Secure CDs only: First level TTB1 descriptor fetches are Secure)
// HAD1   = b1    (TTB1: Hierarchical attributes are disabled)
// TTB1   = b0
void smmuInitBasicCD(void* CD, void* pTT0, const uint32_t T0SZ, const uint32_t TG0, const uint32_t AFFD, const uint32_t IPS, const uint32_t AA64, const uint32_t ASID, const uint32_t ARS)
{
  struct cd_s* pCD;

  // Check that pCD is not a NULL pointer
  if (CD == 0)
    printf("smmuInitBasicCD(): CD is a NULL pointer\n");

  // Cast CD to the cd_s for easier manipulation
  pCD = (struct cd_s*)CD;


  // Ensure valid bit is clear while we program the CD
  pCD->d0.valid = 0;
  // dsb();

  // Populate CD
  pCD->d0.T0SZ    = T0SZ;
  pCD->d0.AA64    = 0x1;
  pCD->d0.TG0     = TG0;
  pCD->d0.IR0     = 0x0;
  pCD->d0.OR0     = 0x0;
  pCD->d0.SH0     = 0x0;
  pCD->d0.EPD0    = 0x0;
  pCD->d0.ENDI    = 0x0;
  pCD->d0.T1SZ    = 32;
  pCD->d0.TG1     = 0x2;
  pCD->d0.IR1     = 0x0;
  pCD->d0.OR1     = 0x0;
  pCD->d0.SH1     = 0x0;
  pCD->d0.EPD1    = 0x1;
  pCD->d0.IPS     = IPS;
  pCD->d0.AFFD    = AFFD;
  pCD->d0.WXN     = 0x0;
  pCD->d0.UWXN    = 0x0;
  pCD->d0.TBI     = 0x0;
  pCD->d0.PAN     = 0x0;
  pCD->d0.HD      = 0x0;
  pCD->d0.HA      = 0x0;
  pCD->d0.ARS     = ARS;
  pCD->d0.ASET    = 0x1;
  pCD->d0.ASID    = ASID;

  pCD->d1.NSCFG0 = 0x0;//vraval 1 to 2 TF
  pCD->d1.HAD0   = 0x1;
  pCD->d1.TTB0   = (((uint64_t)pTT0) >> 4);

  pCD->d2.NSCFG1 = 0x0;//vraval 1 to 2 TF
  pCD->d2.HAD1   = 0x1;
  pCD->d2.TTB1   = 0x0;

  pCD->d3.MAIR0  = SMMU_CD_MAIR_DEVICE_NGNRNE;
  pCD->d3.MAIR1  = SMMU_CD_MAIR_NORMAL_NOCACHE;
  pCD->d3.MAIR2  = SMMU_CD_MAIR_NORMAL_WB_WA; 
  pCD->d3.MAIR3  = 0x0;
  pCD->d3.MAIR4  = 0x0;
  pCD->d3.MAIR5  = 0x0;
  pCD->d3.MAIR6  = 0x0;
  pCD->d3.MAIR7  = 0x0;


  pCD->d4.AMAIR0  = 0x0;
  pCD->d4.AMAIR1  = 0x0;
  pCD->d4.AMAIR2  = 0x0;
  pCD->d4.AMAIR3  = 0x0;
  pCD->d4.AMAIR4  = 0x0;
  pCD->d4.AMAIR5  = 0x0;
  pCD->d4.AMAIR6  = 0x0;
  pCD->d4.AMAIR7  = 0x0;

  pCD->d5.IMPDEF  = 0x0;
  // dsb();

  // Only set Valid to true once other fields are programmed
  pCD->d0.valid = 1;
  // dsb();

  //#ifdef DEBUG
  printf("smmuInitBasicCD:: Created CD: \n");
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[1],  ((uint32_t*)pCD)[0]);   // d0
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[3],  ((uint32_t*)pCD)[2]);   // d1
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[5],  ((uint32_t*)pCD)[4]);   // d2
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[7],  ((uint32_t*)pCD)[6]);   // d3
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[9],  ((uint32_t*)pCD)[8]);   // d4
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[11], ((uint32_t*)pCD)[10]);  // d5
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[13], ((uint32_t*)pCD)[12]);  // d6
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[15], ((uint32_t*)pCD)[14]);  // d7
 // #endif

  return;
}

void smmuInitBasicCD_S(void* CD, void* pTT0, const uint32_t T0SZ, const uint32_t TG0, const uint32_t AFFD, const uint32_t IPS, const uint32_t AA64, const uint32_t ASID, const uint32_t ARS)
{
  struct cd_s* pCD;

  // Check that pCD is not a NULL pointer
  if (CD == 0)
    printf("smmuInitBasicCD(): CD is a NULL pointer\n");

  // Cast CD to the cd_s for easier manipulation
  pCD = (struct cd_s*)CD;


  // Ensure valid bit is clear while we program the CD
  pCD->d0.valid = 0;
  // dsb();

  // Populate CD
  pCD->d0.T0SZ    = T0SZ;
  pCD->d0.AA64    = 0x1;
  pCD->d0.TG0     = TG0;
  pCD->d0.IR0     = 0x0;
  pCD->d0.OR0     = 0x0;
  pCD->d0.SH0     = 0x0;
  pCD->d0.EPD0    = 0x0;
  pCD->d0.ENDI    = 0x0;
  pCD->d0.T1SZ    = 32;
  pCD->d0.TG1     = 0x2;
  pCD->d0.IR1     = 0x0;
  pCD->d0.OR1     = 0x0;
  pCD->d0.SH1     = 0x0;
  pCD->d0.EPD1    = 0x1;
  pCD->d0.IPS     = IPS;
  pCD->d0.AFFD    = AFFD;
  pCD->d0.WXN     = 0x0;
  pCD->d0.UWXN    = 0x0;
  pCD->d0.TBI     = 0x0;
  pCD->d0.PAN     = 0x0;
  pCD->d0.HD      = 0x0;
  pCD->d0.HA      = 0x0;
  pCD->d0.ARS     = ARS;
  pCD->d0.ASET    = 0x1;
  pCD->d0.ASID    = ASID;

  pCD->d1.NSCFG0 = 0x0;//vraval 1 to 2 TF
  pCD->d1.HAD0   = 0x1;
  pCD->d1.TTB0   = (((uint64_t)pTT0) >> 4);

  pCD->d2.NSCFG1 = 0x0;//vraval 1 to 2 TF
  pCD->d2.HAD1   = 0x1;
  pCD->d2.TTB1   = 0x0;

  pCD->d3.MAIR0  = SMMU_CD_MAIR_DEVICE_NGNRNE;
  pCD->d3.MAIR1  = SMMU_CD_MAIR_NORMAL_NOCACHE;
  pCD->d3.MAIR2  = SMMU_CD_MAIR_NORMAL_WB_WA;
  pCD->d3.MAIR3  = 0x0;
  pCD->d3.MAIR4  = 0x0;
  pCD->d3.MAIR5  = 0x0;
  pCD->d3.MAIR6  = 0x0;
  pCD->d3.MAIR7  = 0x0;


  pCD->d4.AMAIR0  = 0x0;
  pCD->d4.AMAIR1  = 0x0;
  pCD->d4.AMAIR2  = 0x0;
  pCD->d4.AMAIR3  = 0x0;
  pCD->d4.AMAIR4  = 0x0;
  pCD->d4.AMAIR5  = 0x0;
  pCD->d4.AMAIR6  = 0x0;
  pCD->d4.AMAIR7  = 0x0;

  pCD->d5.IMPDEF  = 0x0;
  // dsb();

  // Only set Valid to true once other fields are programmed
  pCD->d0.valid = 1;
  // dsb();

  #ifdef DEBUG
  printf("smmuInitBasicCD:: Created CD: \n");
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[1],  ((uint32_t*)pCD)[0]);   // d0
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[3],  ((uint32_t*)pCD)[2]);   // d1
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[5],  ((uint32_t*)pCD)[4]);   // d2
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[7],  ((uint32_t*)pCD)[6]);   // d3
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[9],  ((uint32_t*)pCD)[8]);   // d4
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[11], ((uint32_t*)pCD)[10]);  // d5
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[13], ((uint32_t*)pCD)[12]);  // d6
  printf("  0x%08x %08x\n", ((uint32_t*)pCD)[15], ((uint32_t*)pCD)[14]);  // d7
  #endif

  return;
}
// ------------------------------------------------------------
// Commands
// ------------------------------------------------------------

// TBD:
// Currently this code treats command a byte arrays.  This is inefficient,
// and should be changed to have a typedef for commands.
//

#define COMMAND_SIZE     (16)

#define CMDQ_INDEX_MASK  (0x00001F)
#define CMDQ_WRAP_MASK   (0x1 << 5)


//
static uint32_t smmuQueueIndexInc(uint32_t qprod, uint32_t qidx_mask, uint32_t qwrap_mask)
{
  uint32_t i = (qprod + 1) & qidx_mask;

  // In the generic case for an arbitrary increment,
  // wrap occurred if new index is . old index. Invert wrap bit if so.
  if (i <= (qprod & qidx_mask))
    i = ((qprod & qwrap_mask ) ^ qwrap_mask) | i;
  else
    i = (qprod & qwrap_mask ) | i;

  return i;
}

static bool smmuQueueEmpty(uint32_t cons, uint32_t prod)
{
  return ((cons & CMDQ_INDEX_MASK) == (prod & CMDQ_INDEX_MASK)) &&
          ((cons & CMDQ_WRAP_MASK) == (prod & CMDQ_WRAP_MASK));
}

static bool smmuQueueFull(uint32_t cons, uint32_t prod)
{
  return (((cons & CMDQ_INDEX_MASK) == (prod & CMDQ_INDEX_MASK)) &&
          ((cons & CMDQ_WRAP_MASK) != (prod & CMDQ_WRAP_MASK)));
}

static bool smmuCmdQueueError(void)
{
  return (smmu0->SMMU_CMDQ_CONS & 0x1);
}

static bool smmuCmdQueueError_S(void)
{
  return (smmu1->SMMU_S_CMDQ_CONS & 0x1);
}
// Adds a command to NS Command Queue
//
// NOTE: This function is intended for internal use only.
static uint32_t smmuAddCommand(uint8_t* command)
{
  uint8_t* entry;
  uint32_t i;

  // #ifdef DEBUG
  printf("smmuAddCommand:: Command is:\n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x\n", command[3], command[2], command[1], command[0], command[7], command[6], command[5], command[4], command[11], command[10], command[9], command[8], command[15], command[14], command[13], command[12]);
  printf("smmuAddCommand:: START: PROD = 0x%x  COND = 0x%x\n", (smmu0->SMMU_CMDQ_PROD), (smmu0->SMMU_CMDQ_CONS));
  //#endif 

  // Wait until the queue is not full
  // In practice, this should never be the case as
  // this code always for a command to complete before
  // returning.  Hence therse should only ever be
  // one command in flight.
  while (smmuQueueFull(smmu0->SMMU_CMDQ_CONS, smmu0->SMMU_CMDQ_PROD))
  {}
	printf("after smmuQueueFull\n");

  // Get empty entry in queue
  entry = (uint8_t*)((uint64_t)pCMDQ + ((smmu0->SMMU_CMDQ_PROD & CMDQ_INDEX_MASK) * COMMAND_SIZE));
printf("after entry\n");
  // Copy command into queue
	for(i=0; i<(COMMAND_SIZE); i++)
	{
		writeb(command[i], (entry + i));
		//entry[i] = command[i];
	}
printf("after for_command\n");
printf("smmuAddCommand:: Command is:\n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x\n", entry[3], entry[2], entry[1], entry[0], entry[7], entry[6], entry[5], entry[4], entry[11], entry[10], entry[9], entry[8], entry[15], entry[14], entry[13], entry[12]);

	printf("after flushing 0x%lx - 0x%lx\n", (unsigned long)entry, (unsigned long)(entry + COMMAND_SIZE));
printf("smmu1_S_IDR1.secure : 0x%lx\n", smmu1->SMMU_S_IDR1 & BIT(31));

  // Increment queue pointer
  smmu0->SMMU_CMDQ_PROD = smmuQueueIndexInc(smmu0->SMMU_CMDQ_PROD, CMDQ_INDEX_MASK, CMDQ_WRAP_MASK);
printf("after increase queue counter\n");

  // Wait for the command to be processed
  while(!smmuQueueEmpty(smmu0->SMMU_CMDQ_CONS, smmu0->SMMU_CMDQ_PROD) && !smmuCmdQueueError()){
	  printf("smmuAddCommand:: END: PROD = 0x%x  COND = 0x%x\n", (smmu0->SMMU_CMDQ_PROD), (smmu0->SMMU_CMDQ_CONS));
	 printf("smmuAddCommand:: SMMU0_GERROR = 0x%x\n", (smmu0->SMMU_GERROR));
	}
printf("after waiting command to process\n");
 // #ifdef DEBUG
  printf("smmuAddCommand:: END: PROD = 0x%x  COND = 0x%x\n", (smmu0->SMMU_CMDQ_PROD), (smmu0->SMMU_CMDQ_CONS));
  //#endif

  return smmuCmdQueueError();
}


// Adds a command to S Command Queue
//
// NOTE: This function is intended for internal use only.
static uint32_t smmuAddCommand_S(uint8_t* command)
{
  uint8_t* entry;
  uint32_t i;


   #ifdef DEBUG
  printf("smmuAddCommand:: Command is:\n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x \n 0x%02x%02x%02x%02x\n", command[3], command[2], command[1], command[0], command[7], command[6], command[5], command[4], command[11], command[10], command[9], command[8], command[15], command[14], command[13], command[12]);
  printf("smmuAddCommand:: START: PROD = 0x%x  COND = 0x%x\n", (smmu1->SMMU_S_CMDQ_PROD & CMDQ_INDEX_MASK), (smmu1->SMMU_S_CMDQ_CONS & CMDQ_INDEX_MASK));
  #endif 

  // Wait until the queue is not full
  // In practice, this should never be the case as
  // this code always for a command to complete before
  // returning.  Hence therse should only ever be
  // one command in flight.
  while (smmuQueueFull(smmu1->SMMU_S_CMDQ_CONS, smmu1->SMMU_S_CMDQ_PROD))
  {}


  // Get empty entry in queue
  entry = (uint8_t*)((uint64_t)pCMDQ + ((smmu1->SMMU_S_CMDQ_PROD & CMDQ_INDEX_MASK) * COMMAND_SIZE));

  // Copy command into queue
  for(i=0; i<(COMMAND_SIZE); i++)
    entry[i] = command[i];

	flush_dcache_range((unsigned long)entry, (unsigned long)(entry + COMMAND_SIZE));
	printf("after flushing 0x%lx - 0x%lx\n", (unsigned long)entry, (unsigned long)(entry + COMMAND_SIZE));

  // dsb();

  // Increment queue pointer
  smmu1->SMMU_S_CMDQ_PROD = smmuQueueIndexInc(smmu1->SMMU_S_CMDQ_PROD, CMDQ_INDEX_MASK, CMDQ_WRAP_MASK);

  // Wait for the command to be processed
  while(!smmuQueueEmpty(smmu1->SMMU_S_CMDQ_CONS, smmu1->SMMU_S_CMDQ_PROD) && !smmuCmdQueueError_S())
  {}

  #ifdef DEBUG
  printf("smmuAddCommand:: END: PROD = 0x%x  COND = 0x%x\n", (smmu1->SMMU_S_CMDQ_PROD & CMDQ_INDEX_MASK), (smmu1->SMMU_S_CMDQ_CONS & CMDQ_INDEX_MASK));
  #endif

  return smmuCmdQueueError_S();


    //  printf("smmuAddCommand_S:: ERROR - Currently not supported!\n");
//  return 1;
}


// Issues a CMD_CFGI_ALL command to the specified queue
//
// NOTE: World is ignored, and treated as 0, when the ns_queue is selected
bool smmuInvalidateAllSTE(const bool ns_queue, const uint32_t world)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x04;   // CMD_CFGI_ALL

  if (ns_queue)
    command[1]  = 0;             // World must be 0 in the NS command queue, or a CERROR_ILL is generated
  else
    command[1]  = (uint8_t)((world & 0x1) << 2);

  command[2]  = 0;
  command[3]  = 0;

  command[4]  = 0;
  command[5]  = 0;
  command[6]  = 0;
  command[7]  = 0;

  command[8]  = 31;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;

  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_CFGI_STE command to the specified queue
//
// NOTE: SSec is ignored, and treated as 0, when the ns_queue is selected
bool smmuInvalidateSTE(const bool ns_queue, const uint32_t StreamID, const uint32_t SSec, const uint32_t Leaf)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x03;   // CMD_CFGI_STE

  if (ns_queue)
    command[1]  = 0;             // SSec must be 0 in the NS command queue, or a CERROR_ILL is generated
  else
    command[1]  = (uint8_t)((SSec & 0x1) << 2);

  command[2]  = 0;
  command[3]  = 0;

  command[4]  = (uint8_t)( StreamID        & 0xFF);
  command[5]  = (uint8_t)((StreamID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((StreamID >> 16) & 0xFF);
  command[7]  = (uint8_t)((StreamID >> 24) & 0xFF);

  command[8]  = (uint8_t)(Leaf & 0x1);

  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_CFGI_CD command to the specified queue
//
// NOTE: SSec is ignored, and treated as 0, when the ns_queue is selected
bool smmuInvalidateCD(const bool ns_queue, const uint32_t StreamID, const uint32_t SSec, const uint32_t Leaf)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x05;   // CMD_CFGI_CD

  if (ns_queue)
    command[1]  = 0;             // SSec must be 0 in the NS command queue, or a CERROR_ILL is generated
  else
    command[1]  = (uint8_t)((SSec & 0x1) << 2);

  command[2]  = 0;
  command[3]  = 0;

  command[4]  = (uint8_t)( StreamID        & 0xFF);
  command[5]  = (uint8_t)((StreamID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((StreamID >> 16) & 0xFF);
  command[7]  = (uint8_t)((StreamID >> 24) & 0xFF);

  command[8]  = (uint8_t)(Leaf & 0x1);

  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_TLBI_NH_ALL command to the specified queue
bool smmuTLBIS1All(const bool ns_queue, const uint32_t VMID)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x10;   // CMD_TLBI_NH_ALL

  command[1]  = 0;
  command[2]  = 0;
  command[3]  = 0;

  command[4]  = (uint8_t)( VMID        & 0xFF);
  command[5]  = (uint8_t)((VMID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((VMID >> 16) & 0xFF);
  command[7]  = (uint8_t)((VMID >> 24) & 0xFF);

  command[8]  = 0;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_TLBI_S12_VMALL command to the specified queue
bool smmuTLBIS12All(const bool ns_queue, const uint32_t VMID)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x28;   // CMD_TLBI_S12_VMALL

  command[1]  = 0;
  command[2]  = 0;
  command[3]  = 0;

  command[4]  = (uint8_t)( VMID        & 0xFF);
  command[5]  = (uint8_t)((VMID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((VMID >> 16) & 0xFF);
  command[7]  = (uint8_t)((VMID >> 24) & 0xFF);

  command[8]  = 0;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_TLBI_EL3_ALL command to the specified queue
bool smmuTLBIAllE3(const bool ns_queue)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x18;   // CMD_TLBI_EL3_ALL

  command[1]  = 0;
  command[2]  = 0;
  command[3]  = 0;
  command[4]  = 0;
  command[5]  = 0;
  command[6]  = 0;
  command[7]  = 0;
  command[8]  = 0;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_TLBI_EL2_ALL command to the specified queue
bool smmuTLBIAllE2(const bool ns_queue)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x20;   // CMD_TLBI_EL2_ALL

  command[1]  = 0;
  command[2]  = 0;
  command[3]  = 0;
  command[4]  = 0;
  command[5]  = 0;
  command[6]  = 0;
  command[7]  = 0;
  command[8]  = 0;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}



// Issues a CMD_TLBI_S12_VMALL command to the specified queue
bool smmuTLBINSNH_ALL(const bool ns_queue)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x30;   // CMD_TLBI_S12_VMALL

  command[1]  = 0;
  command[2]  = 0;
  command[3]  = 0;
  command[4]  = 0;
  command[5]  = 0;
  command[6]  = 0;
  command[7]  = 0;
  command[8]  = 0;
  command[9]  = 0;
  command[10] = 0;
  command[11] = 0;
  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;
  
  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}

// Issues a CMD_ATC_INV command to the specified queue
// Assumes SSV == 0 and SubstreamID == 0
bool smmuATCINV(const uint32_t StreamID, const uint8_t size, const uint64_t addr)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x40;   // CMD_RESUME

  command[1]  = 0; // SSV == 0

  command[2]  = 0; //Substream == 0 
  command[3]  = 0; //Substream == 0 

  command[4]  = (uint8_t)( StreamID        & 0xFF);
  command[5]  = (uint8_t)((StreamID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((StreamID >> 16) & 0xFF);
  command[7]  = (uint8_t)((StreamID >> 24) & 0xFF);

  command[8]  = size & 0x3F;
  command[9]  = (uint8_t)((addr >> 8)  & 0xF0);
  command[10] = (uint8_t)((addr >> 16)  & 0xFF);
  command[11] = (uint8_t)((addr >> 24)  & 0xFF);

  command[12] = (uint8_t)((addr >> 32)  & 0xFF);
  command[13] = (uint8_t)((addr >> 40)  & 0xFF);
  command[14] = (uint8_t)((addr >> 48)  & 0xFF);
  command[15] = (uint8_t)((addr >> 56)  & 0xFF);
  
  // Add command to NS queue, and return error code
    return smmuAddCommand(command);
}

// Issues a CMD_SYNC command to the specified queue
bool smmuSyncCmdQueue(const bool ns_queue, const uint32_t CS, const uint32_t MSH, const uint32_t MSIAttr, uint32_t MSI, void* msi_addr)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x46;   // CMD_SYNC

  command[1]  = (uint8_t)((CS & 0x3) << 4);
  command[2]  = (uint8_t)((MSH & 0x3) << 6);
  command[3]  = (uint8_t)(MSIAttr & 0xF);

  command[4]  = (uint8_t)( MSI        & 0xFF);
  command[5]  = (uint8_t)((MSI >> 8)  & 0xFF);
  command[6]  = (uint8_t)((MSI >> 16) & 0xFF);
  command[7]  = (uint8_t)((MSI >> 24) & 0xFF);

  command[8]  = (uint8_t)( (uint64_t)msi_addr        & 0xFF);
  command[9]  = (uint8_t)(((uint64_t)msi_addr >> 8)  & 0xFF);
  command[10] = (uint8_t)(((uint64_t)msi_addr >> 16) & 0xFF);
  command[11] = (uint8_t)(((uint64_t)msi_addr >> 24) & 0xFF);

  command[12] = (uint8_t)(((uint64_t)msi_addr >> 32) & 0xFF);
  command[13] = (uint8_t)(((uint64_t)msi_addr >> 36) & 0xFF);
  command[14] = (uint8_t)(((uint64_t)msi_addr >> 40) & 0xFF);
  command[15] = (uint8_t)(((uint64_t)msi_addr >> 48) & 0xFF);

  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// Issues a CMD_RESUME command to the specified queue
bool smmuResumeCmdQueue(const bool ns_queue, const uint32_t StreamID, const uint32_t STAG, const uint32_t SSec, const uint32_t Action)
{
  uint8_t command[16];

  command[0]  = (uint8_t)0x44;   // CMD_RESUME

  if (ns_queue)
    command[1]  = ((uint8_t)((Action & 0x3) << 4));  // SSec must be 0 in the NS command queue, or a CERROR_ILL is generated
  else
    command[1]  = ((uint8_t)((SSec & 0x1)   << 2)) | ((uint8_t)((Action & 0x3) << 4)) ;

  command[2]  = 0;
  command[3]  = 0;

  command[4]  = (uint8_t)( StreamID        & 0xFF);
  command[5]  = (uint8_t)((StreamID >> 8)  & 0xFF);
  command[6]  = (uint8_t)((StreamID >> 16) & 0xFF);
  command[7]  = (uint8_t)((StreamID >> 24) & 0xFF);

  command[8]  = (uint8_t)( STAG            & 0xFF);
  command[9]  = (uint8_t)((STAG     >> 8)  & 0xFF);
  command[10] = 0;
  command[11] = 0;

  command[12] = 0;
  command[13] = 0;
  command[14] = 0;
  command[15] = 0;
  
  // Add command to queue, and return error code
  if (ns_queue)
    return smmuAddCommand(command);
  else
    return smmuAddCommand_S(command);
}


// ------------------------------------------------------------
// Event Queue
// ------------------------------------------------------------

#define EVENT_SIZE       (0x20)

#define EVENTQ_INDEX_MASK  (0x00000F)
#define EVENTQ_WRAP_MASK   (0x1 << 4)

//
// NOTE: This code relies on the SMMU providing aliases of 
// SMMU_EVENTQ_CONS and SMMU_EVENTQ_PROD at +0x0A8/+0x0Ac
//

bool smmuGetEventRecord(const bool ns_queue, struct EventRecord_s* record)
{
  uint32_t i, new_index;
  uint64_t* entry;

  if (!ns_queue)
  {
    printf("smmuGetFaultRecord:: ERROR - reading Secure event queue not yet supported.\n");
    return 0;
  }


  #ifdef DEBUG
  printf("smmuGetFaultRecord:: PROD = 0x%x  COND = 0x%x\n", (smmu0->SMMU_EVENTQ_PROD & EVENTQ_INDEX_MASK), (smmu0->SMMU_EVENTQ_CONS & EVENTQ_INDEX_MASK));
  #endif


  if (smmuQueueEmpty(smmu0->SMMU_EVENTQ_CONS, smmu0->SMMU_EVENTQ_PROD))
    return 0;


  new_index = smmuQueueIndexInc(smmu0->SMMU_EVENTQ_CONS, EVENTQ_INDEX_MASK, EVENTQ_WRAP_MASK);
  entry = (uint64_t*)((uint64_t)pEVENTQ + ((new_index & EVENTQ_INDEX_MASK) * EVENT_SIZE));

  for(i=0; i<((EVENT_SIZE/8)); i++)
    ((uint64_t*)record)[i] = entry[i];

  #ifdef DEBUG
  printf("smmuGetFaultRecord:: Record is:\n 0x%016lx \n 0x%016lx \n 0x%016lx \n 0x%016lx \n", entry[0], entry[1], entry[2], entry[3]);
  #endif

  // dsb();

  smmu0->SMMU_EVENTQ_CONS = new_index;

  return 1;
}


uint32_t smmuGetEventRecordSTAG(struct EventRecord_s* record)
{
  #ifdef DEBUG
  printf("smmuGetEventRecordSTAG:: Record.STAG = 0x%x\n", (uint32_t)(0xFFFF & record->d1));
  #endif
  return (uint32_t)(0xFFFF & record->d1);
}

// ------------------------------------------------------------
// ATOS
// ------------------------------------------------------------

// Perform address translation operation using GATOS (Global Address Translation Operation Service)
uint64_t smmuTranslateAddr(uint64_t addr, uint32_t SSID_valid, uint32_t substreamid, uint32_t streamid, uint32_t type, uint32_t instr, uint32_t access, uint32_t HTTUI)
{
  // Check that there is no outstanding operation
  while ((smmu0->SMMU_GATOS_CTRL & 0x1) == 1)
  {}

  // Define the translation operation
  smmu0->SMMU_GATOS_SID  = (((uint64_t)SSID_valid << 52) |
                            ((uint64_t)substreamid << 32) | 
                             (uint64_t)streamid);

  smmu0->SMMU_GATOS_ADDR = ((addr & ~(0xFFFLL)) |
                            ((uint64_t)type << 10) |
                            ((uint64_t)access << 8) |
                            ((uint64_t)instr << 7)  |
                            ((uint64_t)HTTUI << 6));

  // Start the operation
  smmu0->SMMU_GATOS_CTRL = 0x1;

  // Poll for completion of the operation
  while ((smmu0->SMMU_GATOS_CTRL & 0x1) == 1)
  {}

  #ifdef DEBUG
  printf("smmuTranslateAddr: PAR = 0x%lx\n", smmu0->SMMU_GATOS_PAR);
  #endif

  return smmu0->SMMU_GATOS_PAR;
}


// Checks whether a record from SMMU_S_GATOS_PAR/SMMU_GATOR_PAR returned a fault
bool smmuIsPARFault(uint64_t par)
{
  return ((par & 0x1) == 1);
}

// Returns the address field from a PAR record
uint64_t smmuPARAddr(uint64_t par)
{
  if ((par & 0x1) == 0)
    return (par & ~0xFFF0000000000FFFLL);
  else
    return 0;
}


// Returns fault code from a PAR record
uint32_t smmPARFaultCode(uint64_t par)
{
  return ((uint32_t)((par >> 12) & 0xFF));
}


// Returns fault type from a PAR record
uint32_t smmPARFaultType(uint64_t par)
{
  return ((uint32_t)((par >> 1) & 0x3));
}

// ------------------------------------------------------------
// End of smmuv3.c
// ------------------------------------------------------------


