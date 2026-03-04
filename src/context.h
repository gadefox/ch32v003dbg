// API wrapper around the Risc-V Debug Module. Adds convenient register access,
// (mis)aligned memory read/write, bulk read/write, and caching of GPRs/PROG{N}
// registers to reduce traffic on the DMI bus.

#pragma once

#include "swio.h"

//==============================================================================
// API

void ctx_init(void);
void ctx_dump(void);
void ctx_dump_block(uint32_t offset, uint32_t addr_base, uint32_t total_size);

//----------
// CPU control
bool ctx_halted_err(const char *msg);
bool ctx_abstracts_wait_busy(void);

bool ctx_halt(void);
bool ctx_resume(bool step);
bool ctx_reset(void);

//----------
// Run small (32 byte on CH32V003) programs from the debug program buffer
void ctx_load_prog(const uint32_t *prog, uint8_t size);
bool ctx_run_prog(const char *name);

bool ctx_read_reg(uint16_t regno, uint32_t* value);
bool ctx_write_reg(uint16_t regno, uint32_t value);

//----------
// Memory access
bool ctx_get_mem32_aligned(uint32_t addr, uint32_t *data);
bool ctx_set_mem32_aligned(uint32_t addr, uint32_t data);

bool ctx_get_mem32(uint32_t addr, uint32_t *data);
bool ctx_set_mem32(uint32_t addr, uint32_t data);

bool ctx_get_mem16(uint32_t addr, uint16_t* data);
bool ctx_set_mem16(uint32_t addr, uint16_t data);

bool ctx_get_mem8(uint32_t addr, uint8_t *data);
bool ctx_set_mem8(uint32_t addr, uint8_t data);

//----------
// Bulk memory access (aligned)
bool ctx_get_block_aligned(uint32_t addr, uint32_t *data, size_t count);
bool ctx_set_block_aligned(uint32_t addr, uint32_t *data, size_t count);

void ctx_test(void);

//==============================================================================
// RISC-V-specific CSRs

void csr_dump(void);

//----------
// Register access

static inline bool csr_get(uint16_t regno, uint32_t *value) { return ctx_read_reg(regno, value); }
static inline bool csr_set(uint16_t regno, uint32_t value) { return ctx_write_reg(regno, value); }

//------------------------------------------------------------------------------
// Status Register

#define CSR_MSTATUS  0x300

#define MSTATUS_MIE    (1u << 3)                   // Machine Interrupt Enable
#define MSTATUS_MPIE   (1u << 7)                   // Machine Previous Interrupt Enable
#define MSTATUS_MPOP   (1u << 23)                  // Whether the current active interrupt needs to come out of the stack
#define MSTATUS_MPPOP  (1u << 24)                  // Whether the current subactive interrupt needs to come out of the stack

typedef union {
  uint32_t raw;
  struct {
    uint32_t PAD0    : 3;  // [2:0]
    uint32_t MIE     : 1;  // [3]
    uint32_t PAD1    : 3;  // [6:4]
    uint32_t MPIE    : 1;  // [7]
    uint32_t PAD2    : 3;  // [10:8]
    uint32_t MPP     : 2;  // [12:11]
    uint32_t PAD3    : 10; // [22:13]
    uint32_t MPOP    : 1;  // [23]
    uint32_t MPPOP   : 1;  // [24]
    uint32_t PAD4    : 7;  // [31:25]
  } b;
} csr_mstatus;

_Static_assert(sizeof(csr_mstatus) == 4, "csr_mstatus");

void csr_mstatus_dump(csr_mstatus r);
static inline bool csr_get_mstatus(csr_mstatus *mstatus) { return csr_get(CSR_MSTATUS, &mstatus->raw); }
static inline bool csr_set_mstatus(uint32_t value) { return csr_set(CSR_MSTATUS, value); }

//------------------------------------------------------------------------------
// Hardware Instruction Set Register

typedef enum {
  MXL32  = 1,
  MXL64  = 2,
  MXL128 = 3
} misa_xl;

#define CSR_MISA  0x301

#define MISA_EXT_A  (1u << 0)   // Atomic extension
#define MISA_EXT_C  (1u << 2)   // Compressed instructions
#define MISA_EXT_D  (1u << 3)   // Double-precision FPU
#define MISA_EXT_E  (1u << 4)   // RV32E base ISA
#define MISA_EXT_F  (1u << 5)   // Single-precision FPU
#define MISA_EXT_G  (1u << 6)   // Additional standard extensions
#define MISA_EXT_H  (1u << 7)   // Hypervisor extension
#define MISA_EXT_I  (1u << 8)   // Base integer ISA
#define MISA_EXT_M  (1u << 12)  // Multiply/Divide extension
#define MISA_EXT_N  (1u << 13)  // User-level interrupts
#define MISA_EXT_Q  (1u << 16)  // Quad-precision FPU
#define MISA_EXT_S  (1u << 18)  // Supervisor mode implemented
#define MISA_EXT_U  (1u << 20)  // User mode implemented
#define MISA_EXT_X  (1u << 23)  // Non-standard extensions present

typedef union {
  uint32_t raw;
  struct {
    uint32_t A    : 1;  // [0]
    uint32_t PAD0 : 1;  // [1]
    uint32_t C    : 1;  // [2]
    uint32_t D    : 1;  // [3]
    uint32_t E    : 1;  // [4]
    uint32_t F    : 1;  // [5]
    uint32_t G    : 1;  // [6]
    uint32_t H    : 1;  // [7]
    uint32_t I    : 1;  // [8]
    uint32_t PAD1 : 3;  // [11:9]
    uint32_t M    : 1;  // [12]
    uint32_t N    : 1;  // [13]
    uint32_t PAD2 : 2;  // [15:14]
    uint32_t Q    : 1;  // [16]
    uint32_t PAD3 : 1;  // [17]
    uint32_t S    : 1;  // [18]
    uint32_t PAD4 : 1;  // [19]
    uint32_t U    : 1;  // [20]
    uint32_t PAD5 : 2;  // [22:21]
    uint32_t X    : 1;  // [23]
    uint32_t PAD6 : 6;  // [29:24]
    uint32_t MXL  : 2;  // [31:30]
  } b;
} csr_misa;

_Static_assert(sizeof(csr_misa) == 4, "csr_misa");

void csr_misa_dump(csr_misa r);
static inline bool csr_get_misa(csr_misa *misa) { return csr_get(CSR_MISA, &misa->raw); }
static inline bool csr_set_misa(uint32_t value) { return csr_set(CSR_MISA, value); }

static inline uint8_t csr_misa_rv(csr_misa r) { return r.b.I ? 32 : 16; }

//------------------------------------------------------------------------------
// Exception Base Address Register

#define CSR_MTVEC  0x305

#define MTVEC_IVT  (1u << 0)       // 1 = offset = IVT[interrupt number]
#define MTVEC_ABS  (1u << 1)       // 1 = identification by absolute address

typedef union {
  uint32_t raw;
  struct {
    uint32_t IVT   : 1;  // [0]
    uint32_t ABS   : 1;  // [1]
    uint32_t BASE  : 30; // [31:2]
  } b;
} csr_mtvec;

_Static_assert(sizeof(csr_mtvec) == 4, "csr_mtvec");

void csr_mtvec_dump(csr_mtvec r);
static inline bool csr_get_mtvec(csr_mtvec *mtvec) { return csr_get(CSR_MTVEC, &mtvec->raw); }
static inline bool csr_set_mtvec(uint32_t value) { return csr_set(CSR_MTVEC, value); }

//------------------------------------------------------------------------------
// Temporary data storage register
  
#define CSR_MSCRATCH  0x340

static inline bool csr_get_mscratch(uint32_t *value) { return csr_get(CSR_MSCRATCH, value); }
static inline bool csr_set_mscratch(uint32_t value) { return csr_set(CSR_MSCRATCH, value); }

//------------------------------------------------------------------------------
// Exception program pointer register

#define CSR_MEPC  0x341

static inline bool csr_get_mepc(uint32_t *value) { return csr_get(CSR_MEPC, value); }
static inline bool csr_set_mepc(uint32_t value) { return csr_set(CSR_MEPC, value); }

//------------------------------------------------------------------------------
// Exception cause register

#define CSR_MCAUSE  0x342

#define MCAUSE_IRQ  (1u << 31)

typedef union {
  uint32_t raw;
  struct {
    uint32_t CODE : 31; // [30:0]
    uint32_t IRQ  : 1;  // [31]
  } b;
} csr_mcause;

_Static_assert(sizeof(csr_mcause) == 4, "csr_mcause");

void csr_mcause_dump(csr_mcause r);
static inline bool csr_get_mcause(csr_mcause *mcause) { return csr_get(CSR_MCAUSE, &mcause->raw); }
static inline bool csr_set_mcause(uint32_t value) { return csr_set(CSR_MCAUSE, value); }

//------------------------------------------------------------------------------
// INTSYSCR register

#define CSR_INTSYSCR  0x804

#define ISCR_HWSTK  (1u << 0)      // Hardware Stack Enable
#define ISCR_INEST  (1u << 1)      // Interrupt Nesting Enable
#define ISCR_EABI   (1u << 2)       // EABI Enable

typedef union {
  uint32_t raw;
  struct {
    uint32_t HWSTK : 1;  // [0]
    uint32_t INEST : 1;  // [1]
    uint32_t EABI  : 1;  // [2]
    uint32_t PAD   : 29; // [31:3]
  } b;
} csr_intsyscr;

_Static_assert(sizeof(csr_intsyscr) == 4, "csr_intsyscr");

void csr_intsyscr_dump(csr_intsyscr r);
static inline bool csr_get_intsyscr(csr_intsyscr *intsyscr) { return csr_get(CSR_INTSYSCR, &intsyscr->raw); }
static inline bool csr_set_intsyscr(uint32_t value) { return csr_set(CSR_INTSYSCR, value); }

//------------------------------------------------------------------------------
// Vendor id register (0xF11)

#define CSR_MVENDORID  0xF11

typedef union {
  uint32_t raw;
  struct {
    uint32_t JEDEC_ID   : 7;  // [6:0]
    uint32_t JEDEC_BANK : 25; // [31:7]
  } b;
} csr_mvendorid;

_Static_assert(sizeof(csr_mvendorid) == 4, "csr_mvendorid");

void csr_mvendorid_dump(csr_mvendorid r);
static inline bool csr_get_mvendorid(csr_mvendorid *mvendorid) { return csr_get(CSR_MVENDORID, &mvendorid->raw); }

//------------------------------------------------------------------------------
// Architecture number register (0xF12)

#define CSR_MARCHID  0xF12

typedef union {
  uint32_t raw;
  struct {
    uint32_t VERSION  : 5;  // [4:0]
    uint32_t SERIAL   : 5;  // [9:5]
    uint32_t ARCH     : 5;  // [14:10]
    uint32_t PAD0     : 1;  // [15]
    uint32_t VENDOR2  : 5;  // [20:16]
    uint32_t VENDOR1  : 5;  // [25:21]
    uint32_t VENDOR0  : 5;  // [30:26]
    uint32_t PAD1     : 1;  // [31]
  } b;
} csr_marchid;

_Static_assert(sizeof(csr_marchid) == 4, "csr_marchid");

void csr_marchid_dump(csr_marchid r);
static inline bool csr_get_marchid(csr_marchid *marchid) { return csr_get(CSR_MARCHID, &marchid->raw); }

//------------------------------------------------------------------------------
// Hardware implementation numbering register

#define CSR_MIMPID  0xF13

typedef union {
  uint32_t raw;
  struct {
    uint32_t PAD0     : 16; // [15:0]
    uint32_t VENDOR2  : 5;  // [20:16]
    uint32_t VENDOR1  : 5;  // [25:21]
    uint32_t VENDOR0  : 5;  // [30:26]
    uint32_t PAD1     : 1;  // [31]
  } b;
} csr_mimpid;

_Static_assert(sizeof(csr_mimpid) == 4, "csr_mimpid");

void csr_mimpid_dump(csr_mimpid r);
static inline bool csr_get_mimpid(csr_mimpid *mimpid) { return csr_get(CSR_MIMPID, &mimpid->raw); }

//==============================================================================
// Debug-specific CSRs

const char *csr_name(uint16_t reg);

//------------------------------------------------------------------------------
// Debug Control and Status Register

typedef enum {
  CAUSE_EBREAK  = 1,
  CAUSE_TRIGGER = 2,
  CAUSE_HALTREQ = 3,
  CAUSE_STEP    = 4,
  CAUSE_RESET   = 5
} dcsr_cause_t;


#define CSR_DCSR  0x7B0

#define DCSR_CAUSE_POS      6
#define DCSR_XDEBUGVER_POS  28

#define DCSR_STEP           (1u << 2)
#define DCSR_CAUSE(c)       (((c) & 7) << DCSR_CAUSE_POS)
#define DCSR_STOPTIME       (1u << 9)
#define DCSR_STEPIE         (1u << 11)
#define DCSR_EBREAKU        (1u << 12)
#define DCSR_EBREAKM        (1u << 15)
#define DCSR_XDEBUGVER(v)   (((v) & 0xF) << DCSR_XDEBUGVER_POS)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PRV       : 2;  // [1:0]
    uint32_t STEP      : 1;  // [2]
    uint32_t PAD0      : 3;  // [5:3]
    uint32_t CAUSE     : 3;  // [8:6]
    uint32_t STOPTIME  : 1;  // [9]
    uint32_t PAD1      : 1;  // [10]
    uint32_t STEPIE    : 1;  // [11]
    uint32_t EBREAKU   : 1;  // [12]
    uint32_t PAD2      : 2;  // [14:13]
    uint32_t EBREAKM   : 1;  // [15]
    uint32_t PAD3      : 12; // [27:16]
    uint32_t XDEBUGVER : 4;  // [31:28]
  } b;
} csr_dcsr;

_Static_assert(sizeof(csr_dcsr) == 4, "csr_dcsr");

void csr_dcsr_dump(csr_dcsr r);
static inline bool csr_get_dcsr(csr_dcsr *dcsr) { return csr_get(CSR_DCSR, &dcsr->raw); }
static inline bool csr_set_dcsr(uint32_t value) { return csr_set(CSR_DCSR, value); }

//------------------------------------------------------------------------------
// Debug Program Counter

#define CSR_DPC  0x7B1

static inline bool csr_get_dpc(uint32_t *value) { return csr_get(CSR_DPC, value); }
static inline bool csr_set_dpc(uint32_t value) { return csr_set(CSR_DPC, value); }

//------------------------------------------------------------------------------
// Debug Scratch Registers
//
#define CSR_DSCRATCH0  0x7B2
#define CSR_DSCRATCH1  0x7B3

#define CSR_DSCRATCH_MAX  14  // CSR_DMCU_CR - CSR_DSCRATCH0

void csr_dscratch_dump(size_t count);
static inline bool csr_get_dscratch(uint8_t i, uint32_t *value) { return csr_get(CSR_DSCRATCH0 + i, value); }
static inline bool csr_set_dscratch(uint8_t i, uint32_t value) { return csr_set(CSR_DSCRATCH0 + i, value); }

//------------------------------------------------------------------------------
// Debug MCU Configuration Register

#define CSR_DMCU_CR  0x7C0

#define DMCR_SLEEP      (1u << 0)
#define DMCR_STANDBY    (1u << 2)
#define DMCR_IWDG_STOP  (1u << 8)
#define DMCR_WWDG_STOP  (1u << 9)
#define DMCR_TIM1_STOP  (1u << 12)
#define DMCR_TIM2_STOP  (1u << 13)

typedef union {
  uint32_t raw;
  struct {
    uint32_t SLEEP     : 1;  // [0]
    uint32_t PAD0      : 1;  // [1]
    uint32_t STANDBY   : 1;  // [2]
    uint32_t PAD1      : 5;  // [7:3]
    uint32_t IWDG_STOP : 1;  // [8]
    uint32_t WWDG_STOP : 1;  // [9]
    uint32_t PAD2      : 2;  // [11:10]
    uint32_t TIM1_STOP : 1;  // [12]
    uint32_t TIM2_STOP : 1;  // [13]
    uint32_t PAD3      : 18; // [31:14]
  } b;
} csr_dmcu_cr;

_Static_assert(sizeof(csr_dmcu_cr) == 4, "csr_dmcu_cr");

void csr_dmcu_cr_dump(csr_dmcu_cr r);
static inline bool csr_get_dmcu_cr(csr_dmcu_cr *dmcu_cr) { return csr_get(CSR_DMCU_CR, &dmcu_cr->raw); }
static inline bool csr_set_dmcu_cr(uint32_t value) { return csr_set(CSR_DMCU_CR, value); }

//==============================================================================
// General-Purpose Registers

// GPR bitmasks
#define GPRB(x)  (1u << GPR_##x)

void gpr_dump(void);

bool gpr_cache_save(uint32_t clobber);
bool gpr_cache_restore(void);

extern const char *gpr_names[32];

//----------
// Register access

static inline bool gpr_get(uint8_t regno, uint32_t *value) { return ctx_read_reg(DMCM_GPR | regno, value); }
static inline bool gpr_set(uint8_t regno, uint32_t value) { return ctx_write_reg(DMCM_GPR | regno, value); }

extern uint8_t gpr_max;

//------------------------------------------------------------------------------

#define GPR_ZERO  0  // hardwired zero
#define GPR_RA    1  // return address

static inline bool gpr_get_zero(uint32_t *value) { return gpr_get(GPR_ZERO, value); }
static inline bool gpr_get_ra(uint32_t *value) { return gpr_get(GPR_RA, value); }
static inline bool gpr_set_ra(uint32_t value) { return gpr_set(GPR_RA, value); }

//------------------------------------------------------------------------------
// Pointers

#define GPR_SP  2  // stack pointer
#define GPR_GP  3  // global pointer
#define GPR_TP  4  // thread pointer

static inline bool gpr_get_sp(uint32_t *value) { return gpr_get(GPR_SP, value); }
static inline bool gpr_set_sp(uint32_t value) { return gpr_set(GPR_SP, value); }

static inline bool gpr_get_gp(uint32_t *value) { return gpr_get(GPR_GP, value); }
static inline bool gpr_set_gp(uint32_t value) { return gpr_set(GPR_GP, value); }

static inline bool gpr_get_tp(uint32_t *value) { return gpr_get(GPR_TP, value); }
static inline bool gpr_set_tp(uint32_t value) { return gpr_set(GPR_TP, value); }

//------------------------------------------------------------------------------n
// Function parameter / return

#define GPR_A0  10
#define GPR_A1  11
#define GPR_A2  12
#define GPR_A3  13
#define GPR_A4  14
#define GPR_A5  15
#define GPR_A6  16
#define GPR_A7  17

#define GPRB_A  (GPRB(A0) | GPRB(A1) | GPRB(A2) | GPRB(A3) | GPRB(A4) | \
    GPRB(A5) | GPRB(A6) | GPRB(A7))  // 0x0003FC00

static inline bool gpr_get_a(uint8_t i, uint32_t *value) { return gpr_get(GPR_A0 + i, value); }
static inline bool gpr_set_a(uint8_t i, uint32_t value) { return gpr_set(GPR_A0 + i, value); }

//------------------------------------------------------------------------------n
// Saved register

#define GPR_S0   8
#define GPR_S1   9
#define GPR_S2   18
#define GPR_S3   19
#define GPR_S4   20
#define GPR_S5   21
#define GPR_S6   22
#define GPR_S7   23
#define GPR_S8   24
#define GPR_S9   25
#define GPR_S10  26
#define GPR_S11  27

#define GPR_S(i)  (i < 2 ? GPR_S0 + i : GPR_S2 - 2 + i)

#define GPRB_S  (GPRB(S0) | GPRB(S1) | GPRB(S2) | GPRB(S3) | GPRB(S4) | \
    GPRB(S5) | GPRB(S6) | GPRB(S7) | GPRB(S8) | GPRB(S9) | GPRB(S10) | \
    GPRB(S11))  // 0x0FFC0300

static inline bool gpr_get_s(uint8_t i, uint32_t *value) { return gpr_get(GPR_S(i), value); }
static inline bool gpr_set_s(uint8_t i, uint32_t value) { return gpr_set(GPR_S(i), value); }

//------------------------------------------------------------------------------n
// Temporary register

#define GPR_T0  5
#define GPR_T1  6
#define GPR_T2  7
#define GPR_T3  28
#define GPR_T4  29
#define GPR_T5  30
#define GPR_T6  31

#define GPR_T(i)  (i < 3 ? GPR_T0 + i : GPR_T3 - 3 + i)

#define GPRB_T  (GPRB(T0) | GPRB(T1) | GPRB(T2) | GPRB(T3) | GPRB(T4) | \
    GPRB(T5) | GPRB(T6))  // 0xF00000E0

static inline bool gpr_get_t(uint8_t i, uint32_t *value) { return gpr_get(GPR_T(i), value); }
static inline bool gpr_set_t(uint8_t i, uint32_t value) { return gpr_set(GPR_T(i), value); }

//------------------------------------------------------------------------------
