#pragma once

#include "context.h"

//==============================================================================
// API

void vendor_dump(void);

//==============================================================================
// Vendor bytes areas

#define VNDB_ADDR  0x1FFFF7C0
#define VNDB_SIZE  64  /* 0x1FFFF800 */

typedef enum {
  TSSOP20,  // 0
  QFN20,    // 1
  SOP16,    // 2
  SOP8,     // 3
} package;

const char* package_to_text(package p);

//------------------------------------------------------------------------------
// NOTE: VAL0: When the bootloader runs, it checks this value; if it is
// non-zero, it uses it to decide whether to call the function that configures
// the interrupt controller thresholds, clears pending interrupts, and enables
// global interrupts.

#define VNDB_CHIPID  (VNDB_ADDR + 0x04)  /* 0x1FFFF7C4 */

typedef union {
  uint32_t raw;
  struct {
    uint32_t VAL0    : 8;  //  8; =10h
    uint32_t VAL1    : 4;  // 12; =5
    uint32_t VAL2    : 4;  // 16; =0
    uint32_t PACKAGE : 4;  // 20; Chip package type
    uint32_t VAL3    : 12; // 32; =003
  } b;
} vndb_chipid;

void vndb_chipid_dump(vndb_chipid r);
inline vndb_chipid vndb_get_chipid(void) {
  vndb_chipid chipid;
  chipid.raw = ctx_get_mem_u32_aligned(VNDB_CHIPID);
  return chipid;
}

//==============================================================================
// Electronic Signature

//------------------------------------------------------------------------------
// Flash capacity register

#define ESIG_FLACAP  (VNDB_ADDR + 0x20)  /* 0x1FFFF7E0 */

inline uint16_t esig_get_flacap(void) {
  return ctx_get_mem_u32_aligned(ESIG_FLACAP); }

//------------------------------------------------------------------------------
// UID registers

#define ESIG_UNIID1      (VNDB_ADDR + 0x28)  /* 0x1FFFF7E8 */
#define ESIG_UNIID2      (VNDB_ADDR + 0x2C)  /* 0x1FFFF7EC */
#define ESIG_UNIID3      (VNDB_ADDR + 0x30)  /* 0x1FFFF7F0 */

inline uint32_t esig_get_uniid1(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID1); }
inline uint32_t esig_get_uniid2(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID2); }
inline uint32_t esig_get_uniid3(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID3); }

//------------------------------------------------------------------------------
