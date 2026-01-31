// API wrapper around the Risc-V Debug Module. Adds convenient register access,
// (mis)aligned memory read/write, bulk read/write, and caching of GPRs/PROG{N}
// registers to reduce traffic on the DMI bus.

// Should _not_ contain anything platform- or chip-specific.
// Memory access methods assum the target has at least 8 prog registers
// FIXME - currently assuming we have exactly 8 prog registers
// FIXME - should check actual number of prog registers at startup...

#pragma once

#include "utils.h"

//------------------------------------------------------------------------------

extern bool xmodem_mode;

//------------------------------------------------------------------------------

void ctx_init(void);
void ctx_dump(void);

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

//------------------------------------------------------------------------------
