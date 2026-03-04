#include <stdio.h>
#include <string.h>

#include "break.h"
#include "flash.h"
#include "utils.h"

//------------------------------------------------------------------------------

#define BP_MAX   64
#define BP_FREE  0xFFFF

//------------------------------------------------------------------------------

typedef struct {
  uint32_t break_map;
  uint32_t dirty_map;
  uint16_t cache[CH32_FLASH_PAGE_WORDS * 2];
  uint16_t index;
} cache_page;

static cache_page pages[BP_MAX];

//------------------------------------------------------------------------------

typedef struct {
  cache_page *page;
  uint16_t    addr;
  uint8_t     offset;
} break_point;

static break_point points[BP_MAX];

//------------------------------------------------------------------------------

static void cache_page_dump(void) {
  print_b(0, "cache pages");

  size_t count = 0;
  for (size_t i = 0; i < BP_MAX; i++) {
    if (pages[i].index == BP_FREE)
      continue;

    if (!(count % 6))
      putchar('\n');

    printf("  %2d: %d", count, pages[i].index);
    print_bits(4, "break map", pages[i].break_map, 32);
    print_bits(4, "dirty map", pages[i].dirty_map, 32);
    count++;
  }
  printf(": %d\n", count);
}

//------------------------------------------------------------------------------

static inline void cache_page_init_all(void) {
  for (size_t i = 0; i < BP_MAX; i++)
    pages[i].index = BP_FREE;
}

//------------------------------------------------------------------------------

static cache_page *cache_page_find(uint16_t index) {
  for (size_t i = 0; i < BP_MAX; i++) {
    cache_page *page = &pages[i];
    if (page->index == index)
      return page;
  }
  return NULL;
}

//------------------------------------------------------------------------------

static cache_page *cache_page_alloc(uint16_t index) {
  // Find empty slot
  cache_page *page = cache_page_find(BP_FREE);
  if (!page)
    return NULL;

  // Load flash page bytes
  if (!ctx_get_block(index * CH32_FLASH_PAGE_SIZE + CH32_FLASH_ADDR,
                     (uint32_t *)page->cache, CH32_FLASH_PAGE_WORDS))
    return NULL;

  // Initialize page slot
  page->index = index;
  page->break_map = 0;
  page->dirty_map = 0;
  return page;
}

//------------------------------------------------------------------------------

static void break_point_patch(cache_page *page, uint32_t mask, uint16_t *patched) {
  if (!mask)
    return;

  for (size_t i = 0; i < BP_MAX; i++) {
    if (points[i].addr == BP_FREE || points[i].page != page)
      continue;

    CHECK(page->break_map & (1u << points[i].offset));
    if (mask & (1u << points[i].offset))
      patched[points[i].offset] = 0x9002;  // c.ebreak
  }
}

//------------------------------------------------------------------------------

static bool cache_page_patch(cache_page *page, uint32_t mask) {
#if BREAK_DUMP
  print_c(2, "break:patch: mask=%08X, breakmap=%08X, dirty=%08X\n", mask, page->break_map, page->dirty_map);
#endif

  uint32_t dirty_new = page->break_map & mask;
  if (page->dirty_map == dirty_new)
    goto quit;

  // Patch flash page
  uint32_t addr = page->index * CH32_FLASH_PAGE_SIZE + CH32_FLASH_ADDR;
  if (!flash_erase_page(addr))
    return false;

  uint16_t patched[CH32_FLASH_PAGE_WORDS * 2];
  memcpy(patched, page->cache, CH32_FLASH_PAGE_SIZE);
  break_point_patch(page, dirty_new, patched);

  if (!flash_write_pages(addr, (uint32_t *)patched, CH32_FLASH_PAGE_WORDS))
    return false;

  page->dirty_map = dirty_new;

quit:
  if (!page->break_map) {
    // Free page slot
#if BREAK_DUMP
    print_c(2, "break:cache: free page slot: %04X", page->index);
#endif
    page->index = BP_FREE;
  }

  return true;
}

//------------------------------------------------------------------------------

bool break_patch(void *except, uint32_t mask) {
  for (size_t i = 0; i < BP_MAX; i++) {
    cache_page *page = &pages[i];
    if (page == except || page->index == BP_FREE)
      continue;

    if (!cache_page_patch(page, mask))
      return false;
  }
  return true;
}

//------------------------------------------------------------------------------

static void break_point_dump(void) {
  print_b(0, "breakpoints");

  size_t count = 0;
  for (size_t i = 0; i < BP_MAX; i++) {
    if (points[i].addr == BP_FREE)
      continue;

    if (!(count % 6))
      putchar('\n');

    printf("  %2d: %08X", count, points[i].addr);
    count++;
  }
  printf(": %d\n", count);
}

//------------------------------------------------------------------------------

static inline void break_point_init_all(void) {
  for (size_t i = 0; i < BP_MAX; i++)
    points[i].addr = BP_FREE;
}

//------------------------------------------------------------------------------

static inline uint8_t break_point_page_offset(uint32_t addr) {;
  uint8_t index = addr % CH32_FLASH_PAGE_SIZE;
  return index / 2;  // word index
}

//------------------------------------------------------------------------------

static int break_point_find(uint16_t addr) {
  for (size_t i = 0; i < BP_MAX; i++) {
    if (points[i].addr == addr)
      return i;
  }
  return -1;
}

//------------------------------------------------------------------------------

static int break_check_addr(uint16_t addr) {
  if (addr & 1) {
    print_r(2, "addr not aligned\n");
    return false;
  }
  if (addr >= CH32_FLASH_SIZE - 2) {  // 2 bytes for c.ebreak
    print_r(2, "invalid addr\n");
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

static inline void break_print_status(bool status, const char *text, int index,
                                uint16_t addr) {
  printf("BP #%d @%08X", index, addr);
  if (status) {
    printf(" [");
    print_g(0, text);
    printf("]\n");
    return;
  }

error:
  print_status(false);
}

//------------------------------------------------------------------------------

int break_set(uint16_t addr) {
  if (!break_check_addr(addr))
    return -1;

  // Prevent duplicate breakpoints at the same address
  if (break_point_find(addr) != -1) {
    print_r(2, "breakpoint @%08X already set\n", addr);
    return -1;
  }

  // Find free slot
  int point_index = break_point_find(BP_FREE);
  if (point_index == -1) {
    print_r(2, "no empty slots left\n");
    return -1;
  }

  // Find page slot
  uint16_t page_index = addr / CH32_FLASH_PAGE_SIZE;
  cache_page *page = cache_page_find(page_index);

  if (!page) {
    // Allocate new slot
    page = cache_page_alloc(page_index);
    if (!page) {
      break_print_status(false, NULL, page_index, addr);
      return -1;
    }
  }

  // Initialize page slot
  uint8_t offset = break_point_page_offset(addr);
  page->break_map |= 1u << offset;

  // Initialize breakpoint slot
  points[point_index].addr = addr;
  points[point_index].page = page;
  points[point_index].offset = offset;

  break_print_status(true, "active", point_index, addr);
  return point_index;
}

//------------------------------------------------------------------------------

int break_clear(uint16_t addr) {
  if (!break_check_addr(addr))
    return -1;

  // Find breakpoint slot
  int index = break_point_find(addr);
  if (index == -1) {
    print_r(0, "no breakpoint found @%08X\n", addr);
    return -1;
  }

  // Update page slot
  cache_page *page = points[index].page;
  page->break_map &= ~(1u << points[index].offset);

  // Free breakpoint slot
  points[index].addr = BP_FREE;

  break_print_status(true, "clear", index, addr);
  return index;
}

//------------------------------------------------------------------------------

void break_init(void) {
  cache_page_init_all();
  break_point_init_all();
}

//------------------------------------------------------------------------------

void break_dump(void) {
  print_y(0, "break:info\n");
  cache_page_dump();
  break_point_dump();
}

//------------------------------------------------------------------------------

bool break_resume(bool step) {
  // Find page slot
  uint32_t dpc;
  if (!csr_get_dpc(&dpc))
    return false;

  uint16_t index = dpc / CH32_FLASH_PAGE_SIZE;
  cache_page *page = cache_page_find(index);

  if (page) {
    uint8_t offset = break_point_page_offset(dpc);

    if (!step || page->dirty_map & (1u << offset)) {
      uint32_t mask = step ? 0: ~(1u << offset);

      if (!cache_page_patch(page, mask))
        return false;
    }
  }

  if (!step) {
    if (!break_patch(page, ~0))
      return false;
  }

  return ctx_resume(step);
}

//------------------------------------------------------------------------------
/*
static int break_scan_page(uint32_t addr) {
  uint16_t page[CH32_FLASH_PAGE_WORDS * 2];

  if (!ctx_get_block(addr, (uint32_t *)page, CH32_FLASH_PAGE_WORDS))
    return -1;

  for (size_t i = 0; i < CH32_FLASH_PAGE_WORDS * 2; i++) {
    uint16_t op = page[i];

    // 0x0000 = illegal → end of firmware
    if (op == 0xFFFF)
      return 0;  // Stop scanning completely

    // c.ebreak
    if (op == 0x9002) {
      if (!break_set(addr + i * 2))
        return -1;
      continue;
    }

    // RISC-V rule: if lowest two bits are 11 → 32-bit instruction
    if ((op & 3) == 3)
      i++;
  }

  return 1;  // Continue with next page
}

//------------------------------------------------------------------------------

bool break_scan(void) {
  for (size_t offset = 0; offset < CH32_FLASH_SIZE; offset += CH32_FLASH_PAGE_SIZE) {
    int result = break_scan_page(CH32_FLASH_ADDR + offset);
    if (result == -1)
      return false;
    if (!result)
      break;
  }
  return true;
}
*/
//------------------------------------------------------------------------------
