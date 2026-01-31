#include <stdio.h>
#include <string.h>

#include "break.h"
#include "flash.h"
#include "utils.h"

//------------------------------------------------------------------------------

#define MAX_BREAKPOINT  32

//------------------------------------------------------------------------------

static bool halted;

static int breakpoint_count;
static uint32_t breakpoints[MAX_BREAKPOINT];

// FIXME: Yes, we're creating two buffers the size of the entire target
// flash memory. For small MCUs like the CH32V003 this is OK, larger MCUs
// will need to do something different.
// FIXME: Do we want to grab a full copy of target flash here? It would save
// us some bookkeeping later, at the cost of a slightly slower startup.

static uint8_t flash_dirty[CH32_FLASH_SIZE];  // Buffer for cached flash contents w/ breakpoints inserted
static uint8_t flash_clean[CH32_FLASH_SIZE];  // Buffer for cached flash contents.

static uint8_t break_map[CH32_FLASH_PAGE_COUNT];  // Number of breakpoints set, per page
static uint8_t flash_map[CH32_FLASH_PAGE_COUNT];  // Number of breakpoints written to device flash, per page.
static uint8_t dirty_map[CH32_FLASH_PAGE_COUNT];  // Nonzero if the flash page does not match our flash_dirty copy.

//------------------------------------------------------------------------------

void break_init(void) {
  breakpoint_count = 0;
  for (int i = 0; i < MAX_BREAKPOINT; i++)
    breakpoints[i] = 0xDEADBEEF;
}

//------------------------------------------------------------------------------

void break_dump(void) {
  print_y(0, "break:dump\n");

  print_b(0, "status\n");
  print_num(2, "count", breakpoint_count);
  print_num(2, "halted", halted);

  uint32_t dpc = csr_get_dpc();
  print_hex(2, "DPC", dpc);

  print_b(0, "breakpoints");
  for (int i = 0; i < MAX_BREAKPOINT; i++) {
    if (i % 6 == 0)
      putchar('\n');
    printf("  %2d: %08X", i, breakpoints[i]);
  }

  print_b(0, "\nbreak_map");
  for (int i = 0; i < CH32_FLASH_PAGE_COUNT; i++) {
    if (i % 26 == 0)
      printf("\n ");
    printf(" %02d", break_map[i]);
  }

  print_b(0, "\nflash_map");
  for (int i = 0; i < CH32_FLASH_PAGE_COUNT; i++) {
    if (i % 26 == 0)
      printf("\n ");
    printf(" %02d", flash_map[i]);
  }

  print_b(0, "\ndirty_map");
  for (int i = 0; i < CH32_FLASH_PAGE_COUNT; i++) {
    if (i % 26 == 0)
      printf("\n ");
    printf(" %02d", dirty_map[i]);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------

bool break_halt(void) {
  if (halted)
    return true;

  if (!ctx_halt())
    return false;

  halted = true;
  break_unpatch_flash();
  return true;
}

bool break_is_halted() { return halted; }
void break_set_halted(bool value) { halted = value; }

//------------------------------------------------------------------------------

bool break_resume(void) {
  if (!halted)
    return true;

  // When resuming, we always step by one instruction first.
  ctx_step();

  // If we land on a breakpoint after stepping, we do _not_ need to patch
  // flash and unhalt - we can just report that we're halted again.
  uint32_t dpc = csr_get_dpc();
  if (break_find(dpc) != -1) {
    print_y(0, "break:resume: not resuming because we immediately hit a breakpoint @ %08X\n", dpc);
    return false;
  }

  break_patch_flash();
  ctx_resume();

  halted = false;
  return true;
}

//------------------------------------------------------------------------------

int break_set(uint32_t addr, int size) {
  CHECK(halted);

  if (size != 2 && size != 4) {
    print_r(0, "break:set: bad size %d\n", size);
    return -1;
  }

  if (addr + size >= CH32_FLASH_SIZE || addr & 1) {
    print_r(0, "break:set: address %08X invalid\n", addr);
    return -1;
  }

  if (break_find(addr) != -1) {
    print_r(0, "break: breakpoint @ %08X already set\n", addr);
    return -1;
  }

  // Find free slot
  int bp_index = break_find(0xDEADBEEF);
  if (bp_index == -1) {
    print_r(0, "break:set: no valid slots left\n");
    return -1;
  }

  // Store the breakpoint
  breakpoints[bp_index] = addr;
  breakpoint_count++;

  int page = addr / CH32_FLASH_PAGE_SIZE;
  break_map[page]++;
  dirty_map[page]++;

  // If this is the first breakpoint in a page, save a clean copy of it.
  if (break_map[page] == 1) {
    int page_base = page * CH32_FLASH_PAGE_SIZE;

    // TODO: check return value
    ctx_get_block_aligned(page_base, (uint32_t*)(flash_clean + page_base), CH32_FLASH_PAGE_WORDS);
    memcpy(flash_dirty + page_base, (uint32_t*)(flash_clean + page_base), CH32_FLASH_PAGE_WORDS);
  }

  // Replace breakpoint address in flash_dirty with c.ebreak
  if (size == 2) {
    uint16_t* dst = (uint16_t*)(flash_dirty + addr);
    // GDB is setting a size 2 breakpoint on a 32-bit instruction. Just ignore the checks for now.
    CHECK((*dst & 3) != 3);
    *dst = 0x9002;      // c.ebreak
  } else if (size == 4) {
    uint32_t* dst = (uint32_t*)(flash_dirty + addr);
    CHECK((*dst & 3) == 3);
    *dst = 0x00100073;  // ebreak
  }

  return bp_index;
}

//------------------------------------------------------------------------------

int break_clear(uint32_t addr, int size) {
  CHECK(halted);

  if (size != 2 && size != 4) {
    print_r(0, "break:clear: bad size %d\n", size);
    return -1;
  }

  if (addr + size >= CH32_FLASH_SIZE || addr & 1) {
    print_r(0, "break:clear: address %08X invalid\n", addr);
    return -1;
  }

  int bp_index = break_find(addr);
  if (bp_index == -1) {
    print_r(0, "break:clear: no breakpoint found @ %08X\n", addr);
    return -1;
  }

  // Clear the breakpoint
  breakpoints[bp_index] = 0xDEADBEEF;
  breakpoint_count--;

  int page = addr / CH32_FLASH_PAGE_SIZE;
  CHECK(break_map[page]);
  break_map[page]--;
  dirty_map[page]++;

  // Restore breakpoint address in flash_dirty with original instruction
  if (size == 2) {
    uint16_t* src = (uint16_t*)(flash_clean + addr);
    CHECK((*src & 3) != 3);
    uint16_t* dst = (uint16_t*)(flash_dirty + addr);
    CHECK(*dst == 0x9002);
    *dst = *src;
  } else if (size == 4) {
    uint32_t* src = (uint32_t*)(flash_clean + addr);
    CHECK((*src & 3) == 3);
    uint32_t* dst = (uint32_t*)(flash_dirty + addr);
    CHECK(*dst == 0x00100073);
    *dst = *src;
  }

  return bp_index;
}

//------------------------------------------------------------------------------

int break_find(uint32_t addr) {
  for (int i = 0; i < MAX_BREAKPOINT; i++) {
    if (breakpoints[i] == addr)
      return i;
  }
  return -1;
}

//------------------------------------------------------------------------------
// Update all pages whose breakpoint counts have changed

void break_patch_flash(void) {
  CHECK(halted);

  for (int page = 0; page < CH32_FLASH_PAGE_COUNT; page++) {
    if (!dirty_map[page])
      continue;

    LOG("patching page %d to have %d breakpoints\n", page, break_map[page]);
    int page_base = page * CH32_FLASH_PAGE_SIZE;
    flash_erase_page(page_base);
    flash_write_pages(page_base, (uint32_t*)(flash_dirty + page_base), CH32_FLASH_PAGE_WORDS);

    flash_map[page] = break_map[page];
    dirty_map[page] = 0;
  }
}

//------------------------------------------------------------------------------
// Replace all marked pages with a clean copy

void break_unpatch_flash(void) {
  CHECK(halted);

  for (int page = 0; page < CH32_FLASH_PAGE_COUNT; page++) {
    if (!flash_map[page])
      continue;

    LOG("unpatching page %d\n", page);
    int page_base = page * CH32_FLASH_PAGE_SIZE;
    flash_erase_page(page_base);
    flash_write_pages(page_base, (uint32_t*)(flash_clean + page_base), CH32_FLASH_PAGE_WORDS);

    flash_map[page] = 0;
    dirty_map[page] = 1;
  }
}

//------------------------------------------------------------------------------
