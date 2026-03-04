#pragma once

#include "context.h"
#include "flash.h"

//==============================================================================
// API

int optb_is_unlocked(void);
int optb_lock(void);
int optb_unlock(void);

// Debug dump
void optb_dump(void);

bool optb_write(uint32_t addr, uint8_t *data, size_t count);
inline bool optb_erase(void) { return flash_start(CTLR_OBWRE | CTLR_OBER); }

//==============================================================================
// Option bytes area

#define OPTB_ADDR  0x1FFFF800
#define OPTB_SIZE  CH32_FLASH_PAGE_SIZE  /* 0x1FFFF840 */

//------------------------------------------------------------------------------

#define OPTB_USER  OPTB_ADDR

typedef union {
  uint32_t raw;
  struct {
    uint32_t RDPR        : 8;  // [7:0]    Read protection control bit
    uint32_t nRDPR       : 8;  // [15:8]
    uint32_t IWDGSW      : 1;  // [16]     Independent Watchdog configuration
    uint32_t PAD0        : 1;  // [17]
    uint32_t STANDYRST   : 1;  // [18]     System reset control in Standby mode
    uint32_t RST_MODE    : 2;  // [20:19]  PD7 alternate as external pin reset
    uint32_t START_MODE  : 1;  // [21]     Power-on startup mode
    uint32_t PAD1        : 2;  // [23:22]
    uint32_t nIWDGSW     : 1;  // [24]
    uint32_t PAD2        : 1;  // [25]
    uint32_t nSTANDYRST  : 1;  // [26]
    uint32_t nRST_MODE   : 2;  // [28:27]
    uint32_t nSTART_MODE : 1;  // [29]
    uint32_t PAD3        : 2;  // [31:30]
  } b;
} optb_user;

_Static_assert(sizeof(optb_user) == 4, "optb_user");

void optb_user_dump(optb_user r);
inline bool optb_set_user(uint32_t value) { return ctx_set_mem32_aligned(OPTB_USER, value); }
inline bool optb_get_user(optb_user *user) { return ctx_get_mem32_aligned(OPTB_USER, &user->raw); }

//------------------------------------------------------------------------------

#define OPTB_DATA  (OPTB_ADDR + 4)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATA0       : 8;  // [7:0]
    uint32_t nDATA0      : 8;  // [15:8]
    uint32_t DATA1       : 8;  // [23:16]
    uint32_t nDATA1      : 8;  // [31:24]
  } b;
} optb_data;

_Static_assert(sizeof(optb_data) == 4, "optb_data");

void optb_data_dump(optb_data r);
inline bool optb_set_data(uint32_t value) { return ctx_set_mem32_aligned(OPTB_DATA, value); }
inline bool optb_get_data(optb_data *data) { return ctx_get_mem32_aligned(OPTB_DATA, &data->raw); }

//------------------------------------------------------------------------------

#define OPTB_WRPR  (OPTB_ADDR + 8)

typedef union {
  uint32_t raw;
  struct {
    uint32_t WRPR0       : 8;  // [7:0]
    uint32_t nWRPR0      : 8;  // [15:8]
    uint32_t WRPR1       : 8;  // [23:16]
    uint32_t nWRPR1      : 8;  // [31:24]
  } b;
} optb_wrpr;

_Static_assert(sizeof(optb_wrpr) == 4, "optb_wrpr");

void optb_wrpr_dump(uint8_t, optb_wrpr r);
inline bool optb_set_wrpr(uint8_t i, uint32_t value) { return ctx_set_mem32_aligned(OPTB_WRPR + i * 4, value); }
inline bool optb_get_wrpr(uint8_t i, optb_wrpr *wrpr) { return ctx_get_mem32_aligned(OPTB_WRPR + i * 4, &wrpr->raw); }

//==============================================================================
// Option bytes area registers (memory-mapped I/O)

//------------------------------------------------------------------------------
// OBKEY register

#define OPTB_OBKEYR  0x40022008

inline bool optb_set_obkeyr(uint32_t value) { return ctx_set_mem32_aligned(OPTB_OBKEYR, value); }

//------------------------------------------------------------------------------
