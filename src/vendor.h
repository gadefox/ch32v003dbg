#pragma once

#include "context.h"

//==============================================================================
// API

void vendor_dump(void);

//==============================================================================
// Vendor bytes areas

typedef enum {
  TSSOP20,  // 0
  QFN20,    // 1
  SOP16,    // 2
  SOP8,     // 3
} package;

const char* package_to_text(package p);

//------------------------------------------------------------------------------

#define VND_CHIPID  0x1FFFF7C4

typedef union {
  uint32_t raw;
  struct {
    uint32_t SECTORS : 8;  //  8;  Flash capacity?
    uint32_t VAL0    : 4;  // 12; =5
    uint32_t VAL1    : 4;  // 16; =0
    uint32_t PACKAGE : 4;  // 20; Chip package type
    uint32_t VAL2    : 12; // 32; =003
  } b;
} vnd_chipid;

void vnd_chipid_dump(vnd_chipid r);
inline vnd_chipid vnd_get_chipid(void) {
  vnd_chipid chipid;
  chipid.raw = ctx_get_mem_u32_aligned(VND_CHIPID);
  return chipid;
}

//==============================================================================
// Electronic Signature

//------------------------------------------------------------------------------
// Flash capacity register

#define ESIG_FLACAP  0x1FFFF7E0

inline uint16_t esig_get_flacap(void) {
  return ctx_get_mem_u32_aligned(ESIG_FLACAP); }

//------------------------------------------------------------------------------
// UID registers

#define ESIG_UNIID_ADDR  0x1FFFF7E8
#define ESIG_UNIID1      (ESIG_UNIID_ADDR + 0x00)
#define ESIG_UNIID2      (ESIG_UNIID_ADDR + 0x04)
#define ESIG_UNIID3      (ESIG_UNIID_ADDR + 0x08)

inline uint32_t esig_get_uniid1(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID1); }
inline uint32_t esig_get_uniid2(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID2); }
inline uint32_t esig_get_uniid3(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID3); }

//------------------------------------------------------------------------------
