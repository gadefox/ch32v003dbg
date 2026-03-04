#pragma once

#include "context.h"

//==============================================================================
// Flash registers (memory-mapped I/O)

//------------------------------------------------------------------------------
// Control register

#define FLASH_ACTLR  0x40022000

typedef union {
  uint32_t raw;
  struct {
    uint32_t LATENCY : 2;  // [1:0]  SYSCLK<=24 MHz: 0x00; SYSCLK<-48 MHz: 0x01
    uint32_t PAD0    : 30; // [31:2]
  } b;
} flash_actlr;

_Static_assert(sizeof(flash_actlr) == 4, "flash_actlr");

void flash_actlr_dump(flash_actlr r);
inline bool flash_set_actlr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_ACTLR, value); }
inline bool flash_get_actlr(flash_actlr *actlr) { return ctx_get_mem32_aligned(FLASH_ACTLR, &actlr->raw); }

//------------------------------------------------------------------------------
// FPEC key register

#define UNLOCK_KEY1  0x45670123
#define UNLOCK_KEY2  0xCDEF89AB

#define FLASH_KEYR  0x40022004

inline bool flash_set_keyr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_KEYR, value); }

//------------------------------------------------------------------------------
// Status register

#define FLASH_STATR  0x4002200C

#define STATR_BUSY       (1u << 0)
#define STATR_WRPRTERR   (1u << 4)
#define STATR_EOP        (1u << 5)
#define STATR_MODE       (1u << 14)
#define STATR_BOOT_LOCK  (1u << 15)

typedef union {
  uint32_t raw;
  struct {
    uint32_t BUSY      : 1;  // [0]      True if flash busy
    uint32_t PAD0      : 3;  // [3:1]
    uint32_t WRPRTERR  : 1;  // [4]      True if the flash was written while locked
    uint32_t EOP       : 1;  // [5]      True if flash finished programming
    uint32_t PAD1      : 8;  // [13:6]
    uint32_t MODE      : 1;  // [14]     Something to do with boot area flash?
    uint32_t BOOT_LOCK : 1;  // [15]     True if boot flash locked
    uint32_t PAD2      : 16; // [31:16]
  } b;
} flash_statr;

_Static_assert(sizeof(flash_statr) == 4, "flash_statr");

void flash_statr_dump(flash_statr r);
inline bool flash_set_statr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_STATR, value); }
inline bool flash_get_statr(flash_statr *statr) { return ctx_get_mem32_aligned(FLASH_STATR, &statr->raw); }

//------------------------------------------------------------------------------
// Configuration register

#define FLASH_CTLR  0x40022010

#define CTLR_PG       (1u << 0)
#define CTLR_PER      (1u << 1)
#define CTLR_MER      (1u << 2)
#define CTLR_OBPG     (1u << 4)
#define CTLR_OBER     (1u << 5)
#define CTLR_STRT     (1u << 6)
#define CTLR_LOCK     (1u << 7)
#define CTLR_OBWRE    (1u << 9)
#define CTLR_ERRIE    (1u << 10)
#define CTLR_EOPIE    (1u << 12)
#define CTLR_FLOCK    (1u << 15)
#define CTLR_FTPG     (1u << 16)
#define CTLR_FTER     (1u << 17)
#define CTLR_BUFLOAD  (1u << 18)
#define CTLR_BUFRST   (1u << 19)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PG      : 1;  // [0]      Program enable
    uint32_t PER     : 1;  // [1]      Perform sector erase
    uint32_t MER     : 1;  // [2]      Perform full erase
    uint32_t PAD0    : 1;  // [3]
    uint32_t OBPG    : 1;  // [4]      Perform user-selected word programming
    uint32_t OBER    : 1;  // [5]      Perform user-selected word erasure
    uint32_t STRT    : 1;  // [6]      Start
    uint32_t LOCK    : 1;  // [7]      Flash lock status
    uint32_t PAD1    : 1;  // [8]
    uint32_t OBWRE   : 1;  // [9]      User-selected word write enable
    uint32_t ERRIE   : 1;  // [10]     Error status interrupt control
    uint32_t PAD2    : 1;  // [11]
    uint32_t EOPIE   : 1;  // [12]     EOP interrupt control
    uint32_t PAD3    : 2;  // [14:13]
    uint32_t FLOCK   : 1;  // [15]     Fast programming mode lock
    uint32_t FTPG    : 1;  // [16]     Fast page programming?
    uint32_t FTER    : 1;  // [17]     Fast erase
    uint32_t BUFLOAD : 1;  // [18]     Cache data into BUF
    uint32_t BUFRST  : 1;  // [19]     BUF reset operation
    uint32_t PAD4    : 12; // [31:20]
  } b;
} flash_ctlr;

_Static_assert(sizeof(flash_ctlr) == 4, "flash_ctlr");

void flash_ctlr_dump(flash_ctlr r);
inline bool flash_set_ctlr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_CTLR, value); }
inline bool flash_get_ctlr(flash_ctlr *ctlr) { return ctx_get_mem32_aligned(FLASH_CTLR, &ctlr->raw); }

//------------------------------------------------------------------------------
// Address register

#define FLASH_ADDR  0x40022014

inline bool flash_set_addr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_ADDR, value); }

//------------------------------------------------------------------------------
// Option byte register

#define FLASH_OBR  0x4002201C

typedef union {
  uint32_t raw;
  struct {
    uint32_t OBERR       : 1; // [0]      Unlock OB error
    uint32_t RDPRT       : 1; // [1]      Flash read protect flag
    uint32_t IWDG_SW     : 1; // [2]      Independent watchdog enable
    uint32_t PAD0        : 1; // [3]
    uint32_t STANDBY_RST : 1; // [4]      System reset control in Standby mode
    uint32_t RST_MODE    : 2; // [6:5]    Configuration word reset delay time
    uint32_t START_MODE  : 1; // [7]      Power-on startup mode
    uint32_t PAD1        : 2; // [9:8]
    uint32_t DATA0       : 8; // [17:10]  Data byte 0
    uint32_t DATA1       : 8; // [25:18]  Data byte 1
    uint32_t PAD2        : 6; // [31:26]
  } b;
} flash_obr;

_Static_assert(sizeof(flash_obr) == 4, "flash_obr");

void flash_obr_dump(flash_obr r);

inline bool flash_get_obr(flash_obr *obr) { return ctx_get_mem32_aligned(FLASH_OBR, &obr->raw); }

//------------------------------------------------------------------------------
// Write protection register

#define FLASH_WPR  0x40022020

inline bool flash_get_wpr(uint32_t *value) { return ctx_get_mem32_aligned(FLASH_WPR, value); }

//------------------------------------------------------------------------------
// Extended key register

#define FLASH_MODEKEYR  0x40022024

inline bool flash_set_mode_keyr(uint32_t value) { return ctx_set_mem32_aligned(FLASH_MODEKEYR, value); }

//==============================================================================
// API

#define CH32_FLASH_ADDR  0x08000000

#define CH32_FLASH_PAGE_WORDS    16
#define CH32_FLASH_PAGE_SIZE     (CH32_FLASH_PAGE_WORDS * 4)              // 64 bytes
#define CH32_FLASH_SECTOR_WORDS  (CH32_FLASH_PAGE_WORDS * 16)             // 256 bytes
#define CH32_FLASH_SECTOR_SIZE   (CH32_FLASH_PAGE_SIZE * 16)              // 1K byte
#define CH32_FLASH_SIZE          (CH32_FLASH_SECTOR_SIZE * 16)            // 16K bytes
#define CH32_FLASH_PAGE_COUNT    (CH32_FLASH_SIZE / CH32_FLASH_PAGE_SIZE) // 256

//------------------------------------------------------------------------------

// Lock/unlock flash. Assume flash always starts locked.
int flash_fpec_locked(void);
int flash_fpec_lock(void);
int flash_fpec_unlock(void);

int flash_fastprog_locked(void);
int flash_fastprog_lock(void);
int flash_fastprog_unlock(void);

bool flash_status_wait(void);
bool flash_start(uint32_t ctlr);

// Flash erase, addresses must be aligned
bool flash_erase(uint32_t addr, uint32_t ctlr);

inline bool flash_erase_page(uint32_t addr) {
  return flash_erase(addr, CTLR_OBWRE | CTLR_FTER); }  // Timeout < 4 ms

inline bool flash_erase_sector(uint32_t addr) {
  return flash_erase(addr, CTLR_OBWRE | CTLR_PER); }   // Timeout < 51 ms

inline bool flash_erase_chip(void) {
  return flash_erase(CH32_FLASH_ADDR, CTLR_OBWRE | CTLR_MER); }  // Timeout < 4 ms

// Flash write, dest address must be aligned & size must be a multiple of 4
bool flash_write_pages(uint32_t addr, const uint32_t *data, size_t count);
bool flash_verify_pages(uint32_t addr, const uint32_t *data, size_t count);

// Debug dump
void flash_dump(uint32_t addr);

//------------------------------------------------------------------------------
