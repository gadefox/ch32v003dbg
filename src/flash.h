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
    uint32_t LATENCY : 2;  //  2; SYSCLK<=24 MHz: 0x00; SYSCLK<-48 MHz: 0x01
    uint32_t PAD0    : 30; // 32
  } b;
} flash_actlr;

void flash_actlr_dump(flash_actlr r);
inline bool flash_set_actlr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_ACTLR, reg); }

  inline flash_actlr flash_get_actlr(void) {
  flash_actlr actlr;
  actlr.raw = ctx_get_mem_u32_aligned(FLASH_ACTLR);
  return actlr;
}

//------------------------------------------------------------------------------
// FPEC key register

#define UNLOCK_KEY1  0x45670123
#define UNLOCK_KEY2  0xCDEF89AB

#define FLASH_KEYR  0x40022004

inline bool flash_set_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_KEYR, reg); }

//------------------------------------------------------------------------------
// Status register

#define FLASH_STATR  0x4002200C

#define STATR_BUSY       (1 << 0)
#define STATR_WRPRTERR   (1 << 4)
#define STATR_EOP        (1 << 5)
#define STATR_MODE       (1 << 14)
#define STATR_BOOT_LOCK  (1 << 15)

typedef union {
  uint32_t raw;
  struct {
    uint32_t BUSY      : 1;  //  1; True if flash busy
    uint32_t PAD0      : 3;  //  4
    uint32_t WRPRTERR  : 1;  //  5; True if the flash was written while locked
    uint32_t EOP       : 1;  //  6; True if flash finished programming
    uint32_t PAD1      : 8;  // 14
    uint32_t MODE      : 1;  // 15; Something to do with boot area flash?
    uint32_t BOOT_LOCK : 1;  // 16; True if boot flash locked
    uint32_t PAD2      : 16; // 32
  } b;
} flash_statr;

void flash_statr_dump(flash_statr r);
inline bool flash_set_statr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_STATR, reg); }

inline flash_statr flash_get_statr(void) {
  flash_statr statr;
  statr.raw = ctx_get_mem_u32_aligned(FLASH_STATR);
  return statr;
}

//------------------------------------------------------------------------------
// Configuration register

#define FLASH_CTLR  0x40022010

#define CTLR_PG       (1 << 0)
#define CTLR_PER      (1 << 1)
#define CTLR_MER      (1 << 2)
#define CTLR_OBG      (1 << 4)
#define CTLR_OBER     (1 << 5)
#define CTLR_STRT     (1 << 6)
#define CTLR_LOCK     (1 << 7)
#define CTLR_OBWRE    (1 << 9)
#define CTLR_ERRIE    (1 << 10)
#define CTLR_EOPIE    (1 << 12)
#define CTLR_FLOCK    (1 << 15)
#define CTLR_FTPG     (1 << 16)
#define CTLR_FTER     (1 << 17)
#define CTLR_BUFLOAD  (1 << 18)
#define CTLR_BUFRST   (1 << 19)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PG      : 1;  //  1; Program enable
    uint32_t PER     : 1;  //  2; Perform sector erase
    uint32_t MER     : 1;  //  3; Perform full erase
    uint32_t PAD0    : 1;  //  4;
    uint32_t OBG     : 1;  //  5; Perform user-selected word programming
    uint32_t OBER    : 1;  //  6; Perform user-selected word erasure
    uint32_t STRT    : 1;  //  7; Start
    uint32_t LOCK    : 1;  //  8; Flash lock status

    uint32_t PAD1    : 1;  //  9
    uint32_t OBWRE   : 1;  // 10; User-selected word write enable
    uint32_t ERRIE   : 1;  // 11; Error status interrupt control
    uint32_t PAD2    : 1;  // 12
    uint32_t EOPIE   : 1;  // 13; EOP interrupt control
    uint32_t PAD3    : 2;  // 15
    uint32_t FLOCK   : 1;  // 16; Fast programming mode lock

    uint32_t FTPG    : 1;  // 17; Fast page programming?
    uint32_t FTER    : 1;  // 18; Fast erase
    uint32_t BUFLOAD : 1;  // 19; Cache data into BUF
    uint32_t BUFRST  : 1;  // 20; BUF reset operation
    uint32_t PAD4    : 12; // 32;
  } b;
} flash_ctlr;

void flash_ctlr_dump(flash_ctlr r);
inline bool flash_set_ctlr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_CTLR, reg); }

inline flash_ctlr flash_get_ctlr(void) {
  flash_ctlr ctlr;
  ctlr.raw = ctx_get_mem_u32_aligned(FLASH_CTLR);
  return ctlr;
}

//------------------------------------------------------------------------------
// Address register

#define FLASH_ADDR  0x40022014

inline bool flash_set_addr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_ADDR, reg); }

//------------------------------------------------------------------------------
// Option byte register

#define FLASH_OBR  0x4002201C

typedef union {
  uint32_t raw;
  struct {
    uint32_t OBERR       : 1; //  1; Unlock OB error
    uint32_t RDPRT       : 1; //  2; Flash read protect flag
    uint32_t IWDG_SW     : 1; //  3; Independent watchdog enable
    uint32_t PAD0        : 1; //  4
    uint32_t STANDBY_RST : 1; //  5; System reset control in Standby mode
    uint32_t CFGRSTT     : 2; //  7; Configuration word reset delay time
    uint32_t PAD1        : 3; // 10
    uint32_t DATA0       : 8; // 18; Data byte 0
    uint32_t DATA1       : 8; // 26; Data byte 1
    uint32_t PAD2        : 6; // 32
  } b;
} flash_obr;

void flash_obr_dump(flash_obr r);

inline flash_obr flash_get_obr(void) {
  flash_obr obr;
  obr.raw = ctx_get_mem_u32_aligned(FLASH_OBR);
  return obr;
}

//------------------------------------------------------------------------------
// Write protection register

#define FLASH_WPR  0x40022020

inline uint32_t flash_get_wpr(void) {
  return ctx_get_mem_u32_aligned(FLASH_WPR); }

//------------------------------------------------------------------------------
// Extended key register

#define FLASH_MODEKEYR  0x40022024

inline bool flash_set_mode_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_MODEKEYR, reg); }

//==============================================================================
// API

#define CH32_FLASH_ADDR  0x08000000

#define CH32_FLASH_PAGE_WORDS    16
#define CH32_FLASH_PAGE_SIZE     (CH32_FLASH_PAGE_WORDS * sizeof(uint32_t))  // 64 bytes
#define CH32_FLASH_SECTOR_WORDS  (CH32_FLASH_PAGE_WORDS * 16)                // 256 bytes
#define CH32_FLASH_SECTOR_SIZE   (CH32_FLASH_PAGE_SIZE * 16)                 // 1K byte
#define CH32_FLASH_SIZE          (CH32_FLASH_SECTOR_SIZE * 16)               // 16K bytes
#define CH32_FLASH_PAGE_COUNT    (CH32_FLASH_SIZE / CH32_FLASH_PAGE_SIZE)    // 256

//------------------------------------------------------------------------------

// Lock/unlock flash. Assume flash always starts locked.
bool flash_is_fpec_locked(void);
bool flash_lock_fpec(void);
bool flash_unlock_fpec(void);

bool flash_is_fast_prog_locked(void);
bool flash_lock_fast_prog(void);
bool flash_unlock_fast_prog(void);

// Flash erase, addresses must be aligned
bool flash_erase(uint32_t addr, uint32_t ctlr, uint32_t timeout_us);

inline bool flash_erase_page(uint32_t addr) {
  return flash_erase(addr, CTLR_FTER, 4000); }  // 4 ms
inline bool flash_erase_sector(uint32_t addr) {
  return flash_erase(addr, CTLR_PER, 51000); }  // 51 ms
inline bool flash_erase_chip(void) {
  return flash_erase(CH32_FLASH_ADDR, CTLR_MER, 4000); }  // 4 ms

// Flash write, dest address must be aligned & size must be a multiple of 4
bool flash_write_pages(uint32_t dst_addr, const uint32_t* src_data, uint32_t word_count);
bool flash_verify_pages(uint32_t dst_addr, const uint32_t* src_data, uint32_t word_count);

// Debug dump
void flash_dump(uint32_t addr);

//------------------------------------------------------------------------------
