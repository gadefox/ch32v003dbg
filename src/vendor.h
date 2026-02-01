#pragma once

#include "context.h"

//==============================================================================
// API

#define VC_TSSOP20  0x30
#define VC_QFN20    0x31
#define VC_SOP16    0x32
#define VC_SOP8     0x33

const char* variant_to_text(uint8_t var);

void vendor_dump(void);

//==============================================================================
// Vendor bytes areas

#define VEND_CHIP_ID  0x1FFFF7C4

inline uint32_t vendor_get_chipid(void) {
  return ctx_get_mem_u32_aligned(VEND_CHIP_ID); }

//==============================================================================
// Electronic Signature

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
