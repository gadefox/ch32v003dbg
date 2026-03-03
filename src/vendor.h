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

//------------------------------------------------------------------------------

#define VNDB_CHIPID  (VNDB_ADDR + 4)  /* 0x1FFFF7C4 */

typedef union {
  uint32_t raw;
  struct {
    uint32_t REVID   : 16; // [15:0]   =0510h
    uint32_t PACKAGE : 4;  // [19:16]  Chip package type
    uint32_t DEVID   : 12; // [31:20]  =003
  } b;
} vndb_chipid;

void vndb_chipid_dump(const char *name, vndb_chipid r);
inline bool vndb_get_chipid(vndb_chipid *chipid) { return ctx_get_mem_u32_aligned(VNDB_CHIPID, &chipid->raw); }

_Static_assert(sizeof(vndb_chipid) == 4, "vndb_chipid");

//------------------------------------------------------------------------------

#define VNDB_PLLTRIM  (VNDB_ADDR + 0x14)  /* 0x1FFFF7D4 */

inline bool vndb_get_plltrim(uint16_t *value) { return ctx_get_mem_u16(VNDB_PLLTRIM, value); }

//==============================================================================
// Electronic Signature

//------------------------------------------------------------------------------
// Flash capacity register

#define ESIG_FLACAP  (VNDB_ADDR + 0x20)  /* 0x1FFFF7E0 */

inline bool esig_get_flacap(uint16_t *value) { return ctx_get_mem_u16(ESIG_FLACAP, value); }

//------------------------------------------------------------------------------
// UID registers

#define ESIG_UNIID1  (VNDB_ADDR + 0x28)  /* 0x1FFFF7E8 */
#define ESIG_UNIID2  (VNDB_ADDR + 0x2C)  /* 0x1FFFF7EC */
#define ESIG_UNIID3  (VNDB_ADDR + 0x30)  /* 0x1FFFF7F0 */

inline bool esig_get_uniid1(uint32_t *value) { return ctx_get_mem_u32_aligned(ESIG_UNIID1, value); }
inline bool esig_get_uniid2(uint32_t *value) { return ctx_get_mem_u32_aligned(ESIG_UNIID2, value); }
inline bool esig_get_uniid3(uint32_t *value) { return ctx_get_mem_u32_aligned(ESIG_UNIID3, value); }

//------------------------------------------------------------------------------
