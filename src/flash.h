// Small driver to read/write flash in the CH32V003 through the RVD interface

#pragma once

#include "reg.h"

//------------------------------------------------------------------------------

#define CH32_FLASH_BASE_ADDR     0x08000000

#define CH32_FLASH_PAGE_WORDS    16
#define CH32_FLASH_PAGE_SIZE     (CH32_FLASH_PAGE_WORDS * sizeof(uint32_t))  // 64 bytes
#define CH32_FLASH_SECTOR_WORDS  (CH32_FLASH_PAGE_WORDS * 16)                // 256 bytes
#define CH32_FLASH_SECTOR_SIZE   (CH32_FLASH_PAGE_SIZE * 16)                 // 1K byte
#define CH32_FLASH_SIZE          (CH32_FLASH_SECTOR_SIZE * 16)               // 16K bytes
#define CH32_FLASH_PAGE_COUNT    (CH32_FLASH_SIZE / CH32_FLASH_PAGE_SIZE)    // 256

//------------------------------------------------------------------------------

// Lock/unlock flash. Assume flash always starts locked.
bool flash_is_locked(uint32_t set, uint32_t clear);

bool flash_lock_fpec(void);
bool flash_unlock_fpec(void);

bool flash_lock_fast_prog(void);
bool flash_unlock_fast_prog(void);

bool flash_lock_option_bytes(void);
bool flash_unlock_option_bytes(void);

// Flash erase, addresses must be aligned
bool flash_erase(uint32_t addr, uint32_t ctlr, uint32_t timeout_us);

inline bool flash_erase_page(uint32_t addr) {
  return flash_erase(addr, CTLR_FTER, 4000); }  // 4 ms
inline bool flash_erase_sector(uint32_t addr) {
  return flash_erase(addr, CTLR_PER, 51000); }  // 51 ms
inline bool flash_erase_chip(void) {
  return flash_erase(CH32_FLASH_BASE_ADDR, CTLR_MER, 4000); }  // 4 ms

// Flash write, dest address must be aligned & size must be a multiple of 4
bool flash_write_pages(uint32_t dst_addr, const uint32_t* src_data, uint32_t word_count);
bool flash_verify_pages(uint32_t dst_addr, const uint32_t* src_data, uint32_t word_count);

// Debug dump
void flash_dump(uint32_t addr);

//------------------------------------------------------------------------------
