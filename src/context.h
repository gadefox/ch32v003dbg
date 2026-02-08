// API wrapper around the Risc-V Debug Module. Adds convenient register access,
// (mis)aligned memory read/write, bulk read/write, and caching of GPRs/PROG{N}
// registers to reduce traffic on the DMI bus.

// Should _not_ contain anything platform- or chip-specific.
// Memory access methods assum the target has at least 8 prog registers
// FIXME - currently assuming we have exactly 8 prog registers
// FIXME - should check actual number of prog registers at startup...

#pragma once

#include "utils.h"

//==============================================================================
// API

#define DUMP_WORDS  8*24
#define DUMP_SIZE   (DUMP_WORDS * sizeof(uint32_t))

//------------------------------------------------------------------------------

void ctx_init(void);
void ctx_dump(void);
void ctx_dump_block(uint32_t offset, uint32_t addr_base, uint32_t total_size);

//----------
// CPU control
bool ctx_halt(void);
bool ctx_resume(void);
bool ctx_step(void);

bool ctx_dm_status_wait(uint32_t mask, bool want_set, uint32_t timeout_us);
bool ctx_reset(void);
void ctx_enable_breakpoints(void);

//----------
// Run small (32 byte on CH32V003) programs from the debug program buffer
void ctx_load_prog(const uint32_t* prog, uint32_t size, uint32_t clobbers);
bool ctx_run_prog(uint32_t timeout_us);
bool ctx_abstracts_wait_busy(uint32_t timeout_us);

//----------
// CSR access
uint32_t ctx_get_csr(int index);
void ctx_set_csr(int index, uint32_t csr);

//----------
// CPU register access
uint32_t ctx_get_gpr(int index);
void ctx_set_gpr(int index, uint32_t gpr);

//----------
// Memory access
uint32_t ctx_get_mem_u32(uint32_t addr);
bool ctx_set_mem_u32(uint32_t addr, uint32_t data);

uint16_t ctx_get_mem_u16(uint32_t addr);
bool ctx_set_mem_u16(uint32_t addr, uint16_t data);

uint8_t ctx_get_mem_u8(uint32_t addr);
bool ctx_set_mem_u8(uint32_t addr, uint8_t data);

//----------
// Bulk memory access
bool ctx_get_block_aligned(uint32_t addr, uint32_t* data, uint32_t data_count);
bool ctx_set_block_aligned(uint32_t addr, uint32_t* data, uint32_t data_count);

uint32_t ctx_get_mem_u32_aligned(uint32_t addr);
bool ctx_set_mem_u32_aligned(uint32_t addr, uint32_t data);

void ctx_reload_regs(void);

#if LOGS
void ctx_test(void);
#endif

//==============================================================================
// Debug-specific CSRs

//------------------------------------------------------------------------------
// Debug Control and Status Register

#define CSR_DCSR  0x7B0

#define DCSR_STEP           (1 << 2)
#define DCSR_NMIP           (1 << 3)
#define DCSR_MPRVEN         (1 << 4)

#define DCSR_CAUSE_EBREAK   (0 << 6)
#define DCSR_CAUSE_TRIGGER  (1 << 6)
#define DCSR_CAUSE_HALTREQ  (2 << 6)
#define DCSR_CAUSE_STEP     (3 << 6)
#define DCSR_CAUSE_RESET    (4 << 6)

#define DCSR_STOPTIME       (1 << 9)
#define DCSR_STOPCOUNT      (1 << 10)
#define DCSR_STEPIE         (1 << 11)
#define DCSR_EBREAKU        (1 << 12)
#define DCSR_EBREAKS        (1 << 13)
#define DCSR_EBREAKM        (1 << 15)

#define DCSR_XDEBUGVER(v)   (((v) & 0xF) << 28)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PRV       : 2;  //  2
    uint32_t STEP      : 1;  //  3
    uint32_t NMIP      : 1;  //  4
    uint32_t MPRVEN    : 1;  //  5
    uint32_t PAD0      : 1;  //  6
    uint32_t CAUSE     : 3;  //  9
    uint32_t STOPTIME  : 1;  // 10
    uint32_t STOPCOUNT : 1;  // 11
    uint32_t STEPIE    : 1;  // 12
    uint32_t EBREAKU   : 1;  // 13
    uint32_t EBREAKS   : 1;  // 14
    uint32_t PAD1      : 1;  // 15
    uint32_t EBREAKM   : 1;  // 16
    uint32_t PAD3      : 12; // 28
    uint32_t XDEBUGVER : 4;  // 32
  } b;
} csr_dcsr;

void csr_dcsr_dump(csr_dcsr r);

inline csr_dcsr csr_get_dcsr(void) {
  csr_dcsr r;
  r.raw = ctx_get_csr(CSR_DCSR);
  return r;
}

inline void csr_set_dcsr(uint32_t reg) { ctx_set_csr(CSR_DCSR, reg); }

//------------------------------------------------------------------------------
// Debug Program Counter

#define CSR_DPC  0x7B1

inline uint32_t csr_get_dpc(void) { return ctx_get_csr(CSR_DPC); }
inline void csr_set_dpc(uint32_t r) { ctx_set_csr(CSR_DPC, r); }

//------------------------------------------------------------------------------
// Debug Scratch Registers
//
#define CSR_DSCRATCH0  0x7B2
#define CSR_DSCRATCH1  0x7B3

inline uint32_t csr_get_dscratch0(void) { return ctx_get_csr(CSR_DSCRATCH0); }
inline void csr_set_dscratch0(uint32_t r) { ctx_set_csr(CSR_DSCRATCH0, r); }

inline uint32_t csr_get_dscratch1(void) { return ctx_get_csr(CSR_DSCRATCH1); }
inline void csr_set_dscratch1(uint32_t r) { ctx_set_csr(CSR_DSCRATCH1, r); }

//==============================================================================
// General-Purpose Register

#define GPR_ZERO  0   //  x0; hardwired zero
#define GPR_RA    1   //  x1; return address
#define GPR_SP    2   //  x2; stack pointer
#define GPR_GP    3   //  x3; global pointer
#define GPR_TP    4   //  x4; thread pointer
#define GPR_T0    5   //  x5; thread pointer
#define GPR_T1    6   //  x6; thread pointer
#define GPR_T2    7   //  x7; thread pointer
#define GPR_S0    8   //  x8; saved / frame pointer
#define GPR_S1    9   //  x9; saved / frame pointer
#define GPR_A0    10  // x10; function parameter / return
#define GPR_A1    11  // x11; function parameter / return
#define GPR_A2    12  // x12; function parameter
#define GPR_A3    13  // x13; function parameter
#define GPR_A4    14  // x14; function parameter
#define GPR_A5    15  // x15; function parameter
#define GPR_MAX   16

// GPR bitmasks
#define GPRB_ZERO  (1 << GPR_ZERO)
#define GPRB_RA    (1 << GPR_RA)
#define GPRB_SP    (1 << GPR_SP)
#define GPRB_GP    (1 << GPR_GP)
#define GPRB_TP    (1 << GPR_TP)
#define GPRB_T0    (1 << GPR_T0)
#define GPRB_T1    (1 << GPR_T1)
#define GPRB_T2    (1 << GPR_T2)
#define GPRB_S0    (1 << GPR_S0)
#define GPRB_S1    (1 << GPR_S1)
#define GPRB_A0    (1 << GPR_A0)
#define GPRB_A1    (1 << GPR_A1)
#define GPRB_A2    (1 << GPR_A2)
#define GPRB_A3    (1 << GPR_A3)
#define GPRB_A4    (1 << GPR_A4)
#define GPRB_A5    (1 << GPR_A5)

inline uint32_t gpr_get_a0(void) { return ctx_get_gpr(GPR_A0); }
inline void gpr_set_a0(uint32_t reg) { ctx_set_gpr(GPR_A0, reg); }

inline uint32_t gpr_get_a1(void) { return ctx_get_gpr(GPR_A1); }
inline void gpr_set_a1(uint32_t reg) { ctx_set_gpr(GPR_A1, reg); }

inline uint32_t gpr_get_a2(void) { return ctx_get_gpr(GPR_A2); }
inline void gpr_set_a2(uint32_t reg) { ctx_set_gpr(GPR_A2, reg); }

inline uint32_t gpr_get_a3(void) { return ctx_get_gpr(GPR_A3); }
inline void gpr_set_a3(uint32_t reg) { ctx_set_gpr(GPR_A3, reg); }

inline uint32_t gpr_get_a4(void) { return ctx_get_gpr(GPR_A4); }
inline void gpr_set_a4(uint32_t reg) { ctx_set_gpr(GPR_A4, reg); }

inline uint32_t gpr_get_a5(void) { return ctx_get_gpr(GPR_A5); }
inline void gpr_set_a5(uint32_t reg) { ctx_set_gpr(GPR_A5, reg); }

//------------------------------------------------------------------------------
