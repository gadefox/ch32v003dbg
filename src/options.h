#pragma once

#include "context.h"

//==============================================================================
// API

bool optb_is_locked(void);
bool optb_lock(void);
bool optb_unlock(void);

// Debug dump
void optb_dump(void);

//==============================================================================
// Option bytes area

#define OPTB_ADDR  0x1FFFF800
#define OPTB_SIZE  64  /* 0x1FFFF840 */

//------------------------------------------------------------------------------

#define OPTB_USER  OPTB_ADDR

typedef union {
  uint32_t raw;
  struct {
    uint32_t RDPR        : 8;  //  8; Read protection control bit
    uint32_t nRDPR       : 8;  // 16
    uint32_t IWDGSW      : 1;  // 17; Independent Watchdog configuration
    uint32_t PAD0        : 1;  // 18
    uint32_t STANDYRST   : 1;  // 19; System reset control in Standby mode
    uint32_t RST_MODE    : 2;  // 21; PD7 alternate as external pin reset
    uint32_t START_MODE  : 1;  // 22; Power-on startup mode
    uint32_t PAD1        : 2;  // 24
    uint32_t nIWDGSW     : 1;  // 25
    uint32_t PAD2        : 1;  // 26
    uint32_t nSTANDYRST  : 1;  // 27
    uint32_t nRST_MODE   : 2;  // 29
    uint32_t nSTART_MODE : 1;  // 30
    uint32_t PAD3        : 2;  // 32
  } b;
} optb_user;

void optb_user_dump(optb_user r);
inline bool optb_set_user(uint32_t reg) {
  return ctx_set_mem_u32_aligned(OPTB_USER, reg); }

inline optb_user optb_get_user(void) {
  optb_user user;
  user.raw = ctx_get_mem_u32_aligned(OPTB_USER);
  return user;
}

//------------------------------------------------------------------------------

#define OPTB_DATA  (OPTB_ADDR + 0x04)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATA0       : 8;  //  8
    uint32_t nDATA0      : 8;  // 16
    uint32_t DATA1       : 8;  // 24
    uint32_t nDATA1      : 8;  // 32
  } b;
} optb_data;

void optb_data_dump(optb_data r);
inline bool optb_set_data(uint32_t reg) {
  return ctx_set_mem_u32_aligned(OPTB_DATA, reg); }

inline optb_data optb_get_data(void) {
  optb_data data;
  data.raw = ctx_get_mem_u32_aligned(OPTB_DATA);
  return data;
}

//------------------------------------------------------------------------------

#define OPTB_WRPR1  (OPTB_ADDR + 0x08)

typedef union {
  uint32_t raw;
  struct {
    uint32_t WRPR0       : 8;  //  8
    uint32_t nWRPR0      : 8;  // 16
    uint32_t WRPR1       : 8;  // 24
    uint32_t nWRPR1      : 8;  // 32
  } b;
} optb_wrpr1;

void optb_wrpr1_dump(optb_wrpr1 r);
inline bool optb_set_wrpr1(uint32_t reg) {
  return ctx_set_mem_u32_aligned(OPTB_WRPR1, reg); }

inline optb_wrpr1 optb_get_wrpr1(void) {
  optb_wrpr1 wrpr1;
  wrpr1.raw = ctx_get_mem_u32_aligned(OPTB_WRPR1);
  return wrpr1;
}

//------------------------------------------------------------------------------

#define OPTB_WRPR2  (OPTB_ADDR + 0x0C)

typedef union {
  uint32_t raw;
  struct {
    uint32_t WRPR2       : 8;  //  8
    uint32_t nWRPR2      : 8;  // 16
    uint32_t WRPR3       : 8;  // 24
    uint32_t nWRPR3      : 8;  // 32
  } b;
} optb_wrpr2;

void optb_wrpr2_dump(optb_wrpr2 r);
inline bool optb_set_wrpr2(uint32_t reg) {
  return ctx_set_mem_u32_aligned(OPTB_WRPR2, reg); }

inline optb_wrpr2 optb_get_wrpr2(void) {
  optb_wrpr2 wrpr2;
  wrpr2.raw = ctx_get_mem_u32_aligned(OPTB_WRPR2);
  return wrpr2;
}

//==============================================================================
// Option bytes area registers (memory-mapped I/O)

//------------------------------------------------------------------------------
// OBKEY register

#define OPTB_OBKEYR  0x40022008

inline bool optb_set_obkeyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(OPTB_OBKEYR, reg); }

//------------------------------------------------------------------------------
