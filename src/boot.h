#pragma once

#include "context.h"

//==============================================================================
// API

bool boot_is_locked(void);
bool boot_lock(void);
bool boot_unlock(void);

// Debug dump
void boot_dump(uint32_t addr);

void boot_pico(void);

//==============================================================================
// Bootloader-reserved memory

#define BOOT_ADDR  0x1FFFF000
#define BOOT_SIZE  1920  /* 0x1FFFF780 */

//==============================================================================
// Bootloader registers (memory-mapped I/O)

//------------------------------------------------------------------------------
// Unlock BOOT key register

#define BOOT_KEYR  0x40022028

inline bool boot_set_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(BOOT_KEYR, reg); }

//------------------------------------------------------------------------------
