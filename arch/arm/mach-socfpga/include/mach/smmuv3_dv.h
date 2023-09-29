// ----------------------------------------------------------
// SMMUv3 - Helper Functions
// Header
//
// ARM Support - Jul 2016
// ----------------------------------------------------------

#ifndef __smmuv3_h
#define __smmuv3_h

#ifndef __ASSEMBLY__
// ----------------------------------------------------------

struct EventRecord_s
{
  struct
  {
    uint32_t CODE :8;
    uint64_t      :56;
  } d0;

  uint64_t d1;
  uint64_t d2;
  uint64_t d3;
};

// ----------------------------------------------------------

// Sets the base (virtual) address of the SMMU.
//
// THIS FUNCTION MUST BE CALLED FIRST!
void smmuSetBaseAddr(void* addr);

// ----------------------------------------------------------


// Returns whether mulit-level tables are supported (SMMU_IDR0.ST_LEVEL)
#define SMMU_IDR0_LINEAR_TABLES                     (0x0)
#define SMMU_IDR0_TWO_LEVEL_TABLES                  (0x1)
uint32_t smmuGetMultiLevelSupport(void);


// Returns termination model behaviour (SMMU_IDR0.TERM_MODEL)
#define SMMU_IDR0_RAZ_TERMINATION_SUPPORTED         (0x0)
#define SMMU_IDR0_RAZ_TERMINATION_NOT_SUPPORTED     (0x1)
uint32_t smmuGetTerminationModel(void);


// Returns the supported stall models (SMMU_IDR0.STALL_MODEL)
#define SMMU_IDR0_TERMINATE_AND_STALL               (0x0)
#define SMMU_IDR0_STALL_ONLY                        (0x1)
#define SMMU_IDR0_TERMINATE_ONLY                    (0x2)
uint32_t smmuGetStallModel(void);


// Returns the supported table walk endianness (SMMU_IDR0.TTENDIAN)
#define SMMU_IDR0_LE_AND_BE                         (0x0)
#define SMMU_IDR0_LE_ONLY                           (0x2)
#define SMMU_IDR0_BE_ONLY                           (0x3)
uint32_t smmuGetEndianness(void);


// Returns the supported VMID sizes (SMMU_IDR0.VMID16)
#define SMMU_IDR0_VMID_8BIT                          (0x0)
#define SMMU_IDR0_VMID_16BIT                         (0x1)
uint32_t smmuGetVMIDSize(void);


// Returns the support for VMID wild card matching (SMMU_IDR0.VMW)
uint32_t smmuGetVMW(void);


// Returns the supported ASID sizes (SMMU_IDR0.ASID16)
#define SMMU_IDR0_ASID_8BIT                          (0x0)
#define SMMU_IDR0_ASID_16BIT                         (0x1)
uint32_t smmuGetASIDSize(void);


// Returns whether hardware update of translation tables is supported (SMMU_IDR0.HTTU)
#define SMMU_IDR0_NO_HW_UPDATE                       (0x0)
#define SMMU_IDR0_AF_ONLY                            (0x1)
#define SMMU_IDR0_AF_AND_DF                          (0x2)
uint32_t smmuGetHTTU(void);


// Returns the value of SMMU_IDR0.S1P
uint32_t smmuGetStage1Support(void);


// Returns the value of SMMU_IDR0.S2P
uint32_t smmuGetStage2Support(void);


// Returns the value of SMMU_IDR1.TABLES_PRESET
uint32_t smmuGetTablesPreset(void);


// Returns the value of SMMU_IDR1.QUEUES_PRESET
uint32_t smmuGetQueuesPreset(void);


// Returns the value of SMMU_IDR1.ATTR_TYPES_OVR
bool smmuIsAttributeOverrideInBypassSupported(void);


// Returns the value of SMMU_IDR1.SSIDSIZE
uint32_t smmuGetSubStreamIDBits(void);


// Returns the value of SMMU_IDR1.SIDSIZE
uint32_t smmuGetStreamIDBits(void);


// Returns the address offset of the SMMU)IDR2_VATOS, or 0 if SMMU_IDR0.VATOS==0
uint64_t smmuGetVATOSAddrOffset(void);


// Returns the value of SMMU_IDR3.HAD
bool smmuIsHierarchicalAttributeDisableSupported(void);


// Returns the value of SMMU_IDR4
uint32_t smmuGetIDR4(void);


// Returns the value of SMMU_IDR5.STALL_MAX
uint32_t smmuGetMaxStalls(void);


// Returns the value of SMMU_IDR5.GRAN64K
bool smmuGet64KGranuleSupport(void);


// Returns the value of SMMU_IDR5.GRAN16K
bool smmuGet16KGranuleSupport(void);


// Returns the value of SMMU_IDR5.GRAN4
bool smmuGet4KGranuleSupport(void);


// Returns the supported output address size (SMMU_IDR5.OAS)
#define SMMU_IDR5_32BIT_OUTPUT_ADDR                 (0x0)
#define SMMU_IDR5_36BIT_OUTPUT_ADDR                 (0x1)
#define SMMU_IDR5_40BIT_OUTPUT_ADDR                 (0x2)
#define SMMU_IDR5_42BIT_OUTPUT_ADDR                 (0x3)
#define SMMU_IDR5_44BIT_OUTPUT_ADDR                 (0x4)
#define SMMU_IDR5_48BIT_OUTPUT_ADDR                 (0x5)
#define SMMU_IDR5_52BIT_OUTPUT_ADDR                 (0x6)
uint32_t smmuGetOutputAddrSize(void);


// Returns the value of SMMU_IIDR
uint32_t smmuGetIIDR(void);


// Returns the value of SMMU_AIDR.[ArchMajorRev:ArchMinorRev]
#define SMMU_AIDR_SMMUv3                            (0x0)
#define SMMU_AIDR_SMMUv3_1                          (0x1)
uint32_t smmuGetArchVersion(void);

// ------------------------------------------------------------
// Configure SMMU
// ------------------------------------------------------------

// Sets the base address and size of the Stream Table
// addr             = PA of stream table, address must be size aligned/
// ra_hint          = Read allocate hint
// stream_id_width  = Width of stream ID, from which the table size if derived as 2^stream_id_width
//
// NOTE: This driver only supports linear tables
#define SMMU_ST_NO_READ_ALLOCATE                    (0x0)
#define SMMU_ST_READ_ALLOCATE                       (0x1)
#define SMMU_ST_LINEAR_TABLE                        (0x0)
void smmuInitStreamTable_S(void* addr, uint32_t ra_hint, uint32_t stream_id_width);
void smmuInitStreamTable(void* addr, uint32_t ra_hint, uint32_t stream_id_width);


// Sets the base address and size of the Command Queue
// addr             = PA of stream table
// ra_hint          = read allocate hint
// queue_size       = Log2 of the queue size, such that num_entries = 2^queue_size
#define SMMU_CQ_NO_READ_ALLOCATE                    (0x0)
#define SMMU_CQ_READ_ALLOCATE                       (0x1)
void smmuInitCommandQueue_S(void* addr, uint32_t ra_hint, uint32_t queue_size);
void smmuInitCommandQueue(void* addr, uint32_t ra_hint, uint32_t queue_size);


// Sets the base address and size of the Event Queue
// addr             = PA of stream table
// wa_hint          = Write allocate hint
// queue_size       = Log2 of the queue size, such that num_entries = 2^queue_size
#define SMMU_EQ_NO_WRITR_ALLOCATE                   (0x0)
#define SMMU_EQ_WRITE_ALLOCATE                      (0x1)
void smmuInitEventQueue_S(void* addr, uint32_t wa_hint, uint32_t queue_size);
void smmuInitEventQueue(void* addr, uint32_t wa_hint, uint32_t queue_size);


// Sets the cacheability and shareability attributes used for ST, CQ and EQ accesses
// inner_cache  = Inner cacheability attribute
// outer_cache  = Outer cacheability attribute
// shareability = Shareability attribute
//
// Note: This driver sets the same attributes for queues and tables
#define SMMU_CR1_NON_CACHEABLE                      (0x0)
#define SMMU_CR1_WB_CACHEABLE                       (0x1)
#define SMMU_CR1_WT_CACHEABLE                       (0x2)
#define SMMU_CR1_NON_SHARED                         (0x0)
#define SMMU_CR1_INNER_SHARED                       (0x2)
#define SMMU_CR1_OUTER_SHARED                       (0x3)
#define SMMU_CR1_system                       (0x3)
void smmuInitTableAttributes_S(uint32_t inner_cache, uint32_t outer_cache, uint32_t shareability);
void smmuInitTableAttributes(uint32_t inner_cache, uint32_t outer_cache, uint32_t shareability);

// Enables command queue processing
void smmuEnableCommandProcessing_S(void);
void smmuEnableCommandProcessing(void);


// Enables Event Queue writes
void smmuEnableEventQueue_S(void);
void smmuEnableEventQueue(void);


// Enable translation of Non-secure streams
// all non-secure transactions are checked against configuration structures
// and may (if the relevant STE enables it) undergo translation.
void smmuEnableTranslation_S(void);
void smmuEnableTranslation(void);


// Sets behaviour of SMMU when in by-pass mode (SMMU_CR0.SMMUEN==0)
// abort     = Whether incoming accesses should abort, or be passed through
// INSTCFG   = Instruction/data override
// PRIVCFG   = User/privileged override
// SHCFG     = Shareability override
// ALLOCCFG  = Allocation/transient hints override
// MTCFG     = Memory type override
// MemAttr   = MemAttr to be used when MTCFG==SMMU_GBPA_OVERRIDE
//
// NOTE: when abort==SMMU_GBPA_ABORT, all other parameters are ignored
#define SMMU_GBPA_ABORT                             (1 << 20)
#define SMMU_GBPA_BYPASS                            (0)


#define SMMU_GBPA_INSTCFG_INCOMING                  (0)
#define SMMU_GBPA_INSTCFG_DATA                      (2 << 18)
#define SMMU_GBPA_INSTCFG_INSTRUCTION               (3 << 18)

#define SMMU_GBPA_PRIVCFG_INCOMING                  (0)
#define SMMU_GBPA_PRIVCFG_UNPRIV                    (2 << 16)
#define SMMU_GBPA_PRIVCFG_PRIV                      (3 << 16)

#define SMMU_GBPA_SHCFG_INCOMING                    (1 << 12)
#define SMMU_GBPA_SHCFG_NSH                         (0)
#define SMMU_GBPA_SHCFG_OSH                         (2 << 12)
#define SMMU_GBPA_SHCFG_ISH                         (3 << 12)

#define SMMU_GBPA_ALLOCCFG_INCOMING                 (0)
#define SMMU_GBPA_ALLOCCFG_RA                       (1 << 10)
#define SMMU_GBPA_ALLOCCFG_WA                       (1 << 9)
#define SMMU_GBPA_ALLOCCFG_T                        (1 << 8)

#define SMMU_GBPA_MTCFG_INCOMING                    (0)
#define SMMU_GBPA_MTCFG_OVERRIDE                    (1 << 10)

#define SMMU_GBPA_MEMTYPE_OVERRIDE                  (1 << 4)
void smmuSetByPassBehaviour(uint32_t abort, uint32_t INSTCFG, uint32_t PRIVCFG, uint32_t SHCFG, uint32_t ALLOCCFG, uint32_t MTCFG, uint32_t MemAttr);



// Invalidate all internal caches
// This function causes a SMMU-global invalidation to be performed for all configuration
// and translation caches for all translation regimes and security worlds.
//
// Calling this function when the SMMU is enabled (SMMU_CR0.SMMUEN==1) is CONSTRAINED UNPREDICTABLE
void smmuInvalidateAll(void);


// ------------------------------------------------------------
// Stream table related functions
// ------------------------------------------------------------

// Generates a Stream Table Entry (STE), and inserts into the Stream Table (ST)
//
// NOTE: This function does NOT perform any invalidation commands
//

#define SMMU_ST_BYPASS               (0x4)
#define SMMU_ST_STAGE1_ONLY          (0x5)
#define SMMU_ST_STAGE2_ONLY          (0x6)
#define SMMU_ST_STAGE1_AND_STAGE2    (0x7)

#define SMMU_ST_ALLOW_STALLS         (0)
#define SMMU_ST_NO_STALLS            (1)

#define SMMU_ST_STRW_EL1             (0x0)
#define SMMU_ST_STRW_EL2             (0x2)
#define SMMU_ST_STRW_EL3             (0x1)

#define SMMU_ST_S2SL0_4K_L0          (0x2)
#define SMMU_ST_S2SL0_4K_L1          (0x1)
#define SMMU_ST_S2SL0_4K_L2          (0x0)

#define SMMU_ST_S2SL0_16K_L1         (0x2)
#define SMMU_ST_S2SL0_16K_L2         (0x1)
#define SMMU_ST_S2SL0_16K_L3         (0x0)

#define SMMU_ST_S2SL0_64K_L1         (0x2)
#define SMMU_ST_S2SL0_64K_L2         (0x1)
#define SMMU_ST_S2SL0_64K_L3         (0x0)

#define SMMU_ST_S2TG_4K              (0x0)
#define SMMU_ST_S2TG_16K             (0x2)
#define SMMU_ST_S2TG_64K             (0x1)

#define SMMU_ST_S2PS_32              (0x0)
#define SMMU_ST_S2PS_36              (0x1)
#define SMMU_ST_S2PS_40              (0x2)
#define SMMU_ST_S2PS_42              (0x3)
#define SMMU_ST_S2PS_44              (0x4)
#define SMMU_ST_S2PS_48              (0x5)

// NOTE: As a simplification, this function uses fixed values for the following STE fields:
// S1DSS    = b00   (terminate)
// S1CIR    = b01   (Normal, Write-Back cacheable, read-allocate)
// S1COR    = b01   (Normal, Write-Back cacheable, read-allocate)
// S1CSH    = b11   (Inner shareable)
// CONT     = b0
// PPAR     = b0
// MEV      = b0    (Fault records are not merged)
// EATS     = b00   (ATS Translation Requests are returned unsuccessful/aborted)
// MTCFG    = b0    (Use incoming Type)
// MemAttr  = b0    (A "don't care", because MTCFG==0)
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
void smmuInitBasicSTE_S(const uint32_t index, const uint32_t config, void* S1ContextPtr, const uint32_t S1CDMax, const uint32_t S1STALLD, const uint32_t STRW, const uint32_t S2VMID, const uint32_t S2T0SZ, void* S2TTB, uint32_t SL0, const uint32_t S2TG, const uint32_t S2PS);
void smmuInitBasicSTE(const uint32_t index, const uint32_t config, void* S1ContextPtr, const uint32_t S1CDMax, const uint32_t S1STALLD, const uint32_t STRW, const uint32_t S2VMID, const uint32_t S2T0SZ, void* S2TTB, uint32_t SL0, const uint32_t S2TG, const uint32_t S2PS);

// Clears the valid bit in a Stream Table Entry (STE),
//
// NOTE: This function does NOT perform any invalidation commands
//

void smmuClearSTE(uint32_t index);


// ------------------------------------------------------------
// Context descriptor related functions
// ------------------------------------------------------------

#define SMMU_CD_SIZEOF               (0x40)

// Generates a Context Descriptor(CD), which can be referenced by a Stream Table Entry (STE)

//
// NOTE: This function does NOT perform any invalidation commands
//

#define SMMU_CD_MAIR_DEVICE_NGNRNE   (0x0)
#define SMMU_CD_MAIR_NORMAL_WB_WA    (0xFF)
#define SMMU_CD_MAIR_NORMAL_NOCACHE  (0x44)

#define SMMU_CD_TG0_4K               (0x0)
#define SMMU_CD_TG0_16K              (0x2)
#define SMMU_CD_TG0_64K              (0x1)

#define SMMU_CD_ALLOW_AF_FAULTS      (0x0)
#define SMMU_CD_NO_AF_FAULTS         (0x1)

#define SMMU_CD_IPS_32               (0x0)
#define SMMU_CD_IPS_36               (0x1)
#define SMMU_CD_IPS_40               (0x2)
#define SMMU_CD_IPS_42               (0x3)
#define SMMU_CD_IPS_44               (0x4)
#define SMMU_CD_IPS_48               (0x5)

#define SMMU_CD_AA64_AARCH32         (0x0)
#define SMMU_CD_AA64_AARCH64         (0x1)

#define SMMU_CD_STALL                (0x3)
#define SMMU_CD_TERMINATE            (0x6)


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
// TBI    = b0    (Top-byte ignore disabled)
// PAN    = b0
// HA:HD  = b11   (HA:HD => Update of Access and Dirty flags enabled)
// ASET   = b1
// NSCFG0 = b1    (Secure CDs only: First level TTB0 descriptor fetches are Secure)
// HAD0   = b1    (TTB0: Hierarchical attributes are disabled)
// NSCFG1 = b1    (Secure CDs only: First level TTB1 descriptor fetches are Secure)
// HAD1   = b1    (TTB1: Hierarchical attributes are disabled)
// TTB1   = b0
void smmuInitBasicCD_S(void* CD, void* pTT0, const uint32_t T0SZ, const uint32_t TG0, const uint32_t AFFD, const uint32_t IPS, const uint32_t AA64, const uint32_t ASID, const uint32_t ARS);
void smmuInitBasicCD(void* CD, void* pTT0, const uint32_t T0SZ, const uint32_t TG0, const uint32_t AFFD, const uint32_t IPS, const uint32_t AA64, const uint32_t ASID, const uint32_t ARS);

// ------------------------------------------------------------
// Secure Configuration
// ------------------------------------------------------------

// Populates a Context Descriptor(CD)

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
void smmuSetByPassBehaviour_S(uint32_t abort, uint32_t INSTCFG, uint32_t PRIVCFG, uint32_t SHCFG, uint32_t ALLOCCFG, uint32_t MTCFG, uint32_t MemAttr);
void smmuSetByPassBehaviour(uint32_t abort, uint32_t INSTCFG, uint32_t PRIVCFG, uint32_t SHCFG, uint32_t ALLOCCFG, uint32_t MTCFG, uint32_t MemAttr);


// ------------------------------------------------------------
// Commands
// ------------------------------------------------------------

#define SMMU_CMD_LEAF               (1)
#define SMMU_CMD_BRANCH             (0)

#define SMMU_CMD_SSEC_S             (1)
#define SMMU_CMD_SSEC_NS            (0)

#define SMM_CMD_NS_QUEUE            (true)
#define SMM_CMD_S_QUEUE             (false)

// Issues a CMD_CFGI_STE command to the specified queue
//
// NOTE: SSec is ignored, and treated as 0, when the ns_queue is selected
bool smmuInvalidateSTE(const bool ns_queue, const uint32_t StreamID, const uint32_t SSec, const uint32_t Leaf);


// Issues a CMD_CFGI_CD command to the specified queue
//
// NOTE: SSec is ignored, and treated as 0, when the ns_queue is selected
bool smmuInvalidateCD(const bool ns_queue, const uint32_t StreamID, const uint32_t SSec, const uint32_t Leaf);


// Issues a CMD_TLBI_NH_ALL command to the specified queue
bool smmuTLBIS1All(const bool ns_queue, const uint32_t VMID);


// Issues a CMD_TLBI_S12_VMALL command to the specified queue
bool smmuTLBIS12All(const bool ns_queue, const uint32_t VMID);


// Issues a CMD_TLBI_EL3_ALL command to the specified queue
bool smmuTLBIAllE3(const bool ns_queue);


// Issues a CMD_TLBI_EL2_ALL command to the specified queue
bool smmuTLBIAllE2(const bool ns_queue);


// Issues a CMD_TLBI_S12_VMALL command to the specified queue
bool smmuTLBINSNH_ALL(const bool ns_queue);

// Issues a CMD_ATC_INV command to the specified queue
// Assumes SSV == 0 and SubstreamID == 0
bool smmuATCINV(const uint32_t StreamID, const uint8_t size, const uint64_t addr);

// Issues a CMD_SYNC command to the specified queue
#define SMMU_SYNC_SIG_NONE           (0x0)
#define SMMU_SYNC_SIG_IRQ            (0x1)
#define SMMU_SYNC_SIG_SEV            (0x2)

#define SMMU_SYNC_MSH_NSH            (0x0)
#define SMMU_SYNC_SIG_OSH            (0x2)
#define SMMU_SYNC_SIG_ISH            (0x3)

bool smmuSyncCmdQueue(const bool ns_queue, const uint32_t CS, const uint32_t MSH, const uint32_t MSIAttr, uint32_t MSI, void* msi_addr);


// Issues a CMD_RESUME command to the specified queue
#define SMMU_RESUME_RESUME           (0x1)
#define SMMU_RESUME_ABORT            (0x2)
#define SMMU_RESUME_RAZ_WI           (0x0)

bool smmuResumeCmdQueue(const bool ns_queue, const uint32_t StreamID, const uint32_t STAG, const uint32_t SSec, const uint32_t Action);


// ------------------------------------------------------------
// Event Queue
// ------------------------------------------------------------

#define SMMU_EVENT_F_TRANSLATION     (0x10)

//
// NOTE: This code relies on the SMMU providing aliases of
// SMMU_EVENTQ_CONS and SMMU_EVENTQ_PROD at +0x0A8/+0x0Ac
//

bool smmuGetEventRecord(const bool ns_queue, struct EventRecord_s* record);


uint32_t smmuGetEventRecordSTAG(struct EventRecord_s* record);

// ------------------------------------------------------------
// ATOS
// ------------------------------------------------------------

// Perform address translation operation using GATOS (Global Address Translation Operation Service)
#define SMMU_ATOS_SUBSTREAM_VALID    (1)
#define SMMU_ATOS_SUBSTREAM_INVALID  (0)

#define SMMU_ATOS_TYPE_STAGE1        (0x1)
#define SMMU_ATOS_TYPE_STAGE2        (0x2)
#define SMMU_ATOS_TYPE_STAGE12       (0x3)

#define SMMU_ATOS_ACCESS_UPRIV_WRITE (0x0)
#define SMMU_ATOS_ACCESS_UPRIV_READ  (0x1)
#define SMMU_ATOS_ACCESS_PRIV_WRITE  (0x2)
#define SMMU_ATOS_ACCESS_PRIV_READ   (0x3)

#define SMMU_ATOS_INSTR              (0x1)
#define SMMU_ATOS_DATA               (0x0)

#define SMMU_ATOS_HTTUI_ALLOW        (0x0)
#define SMMU_ATOS_HTTUI_INHIBT       (0x1)

uint64_t smmuTranslateAddr(uint64_t addr, uint32_t SSID_valid, uint32_t substreamid, uint32_t streamid, uint32_t type, uint32_t instr, uint32_t access, uint32_t HTTUI);


// Checks whether a record from SMMU_S_GATOS_PAR/SMMU_GATOR_PAR returned a fault
bool smmuIsPARFault(uint64_t par);


// Returns the address field from a PAR record
uint64_t smmuPARAddr(uint64_t);


// Returns fault code from a PAR record
uint32_t smmPARFaultCode(uint64_t par);


// Returns fault type from a PAR record
uint32_t smmPARFaultType(uint64_t par);

int smmu_sdm_init(void);

#endif /*__ASSEMBLY__ */
#endif

// ----------------------------------------------------------
// End of smmuv3.h
// ----------------------------------------------------------
