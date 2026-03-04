#include <stdio.h>
#include <stdlib.h>
#include <pico/time.h>

#include "flash.h"
#include "utils.h"

//==============================================================================
// FPEC

inline int flash_fpec_locked(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  return ctlr.raw & CTLR_LOCK;
}

//------------------------------------------------------------------------------

int flash_fpec_lock(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  if (!flash_set_ctlr(ctlr.raw | CTLR_LOCK))
    return -1;
  return flash_fpec_locked();
}

//------------------------------------------------------------------------------

int flash_fpec_unlock(void) {
  // Unlock FPEC controller
  if (!flash_set_keyr(UNLOCK_KEY1) || !flash_set_keyr(UNLOCK_KEY2))
    return -1;
  return flash_fpec_locked();
}

//==============================================================================
// Fast prog

inline int flash_fastprog_locked(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  return ctlr.raw & CTLR_FLOCK;
}

//------------------------------------------------------------------------------

int flash_fastprog_lock(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  if (!flash_set_ctlr(ctlr.raw | CTLR_FLOCK))
    return -1;
  return flash_fastprog_locked();
}

//------------------------------------------------------------------------------

int flash_fastprog_unlock(void) {
  // Unlock fast programming
  if (!flash_set_mode_keyr(UNLOCK_KEY1) || !flash_set_mode_keyr(UNLOCK_KEY2))
    return -1;
  return flash_fastprog_locked();
}

//==============================================================================
// API

bool flash_status_wait(void) {
  for (int i = 0; i < 200; i++) {  // timeout 100 ms
    flash_statr statr;
    if (!flash_get_statr(&statr))
      return false;

    if (!statr.b.BUSY)
      return true;

    sleep_us(500);
  }

  print_r(2, "flash: status timeout\n");
  return false;  // timeout
}

//------------------------------------------------------------------------------

bool flash_start(uint32_t ctlr) {
  flash_ctlr save;
  if (!flash_get_ctlr(&save))            return false;
  if (!flash_set_ctlr(ctlr))             return false;

  bool ret = false;

  // Start the operation and wait for erase to complete
  if (!flash_set_ctlr(ctlr | CTLR_STRT)) goto cleanup;
  if (!flash_status_wait())              goto cleanup;

  ret = true;

cleanup:
  // Restore control register
  (void)flash_set_ctlr(save.raw);
  return ret;
}

//------------------------------------------------------------------------------
// NOTE:performance comparison (16KB, 256 pages):
// - Debug register erase: 997ms (~3.9ms/page)
// - On-chip code erase:   917ms (~3.6ms/page)
// Speedup: 1.09x (9% faster with on-chip code) → minimal difference
// Erase time dominated by flash HW, not debug overhead

inline bool flash_erase(uint32_t addr, uint32_t ctlr) {
  if (!flash_set_addr(addr))
    return false;
  return flash_start(ctlr);
}

//------------------------------------------------------------------------------
// This is some tricky code that feeds data directly from the debug interface
// to the flash page programming buffer, triggering a page write every time
// the buffer fills. This avoids needing an on-chip buffer at the cost of having
// to do some assembly programming in the debug module.

// FIXME: somehow the first write after debugger restart fails on the first word...

// good - 0x19e0006f
// bad  - 0x00010040

const uint16_t prog_write[] = {
  // Copy word and trigger BUFLOAD
  0x4180,          // lw   s0, 0(a1)            // Load word from DM_DATA0
  0xC200,          // sw   s0, 0(a2)            // Store to destination address
  0xC914,          // sw   a3, 16(a0)           // FLASH_CTLR = FTPG | BUFLOAD

  // waitloop1: Busywait for copy to complete - this seems to be required now?
  0x4540,          // lw   s0, 12(a0)           // Load FLASH_STATR
  0x8805,          // andi s0, s0, 1            // Check BUSY bit
  0xFC75,          // bnez s0, <waitloop1>      // Loop if still busy

  // Advance dest pointer and trigger START if we ended a page
  0x0611,          // addi a2, a2, 4            // dst_addr += 4
  0x7413, 0x03F6,  // andi s0, a2, 63           // Check lower 6 bits
  0xE419,          // bnez s0, <end>            // If not page boundary, done
  0xC918,          // sw   a4, 16(a0)           // FLASH_CTLR = FTPG | STRT

  // waitloop2: Busywait for page write to complete
  0x4540,          // lw   s0, 12(a0)           // Load FLASH_STATR
  0x8805,          // andi s0, s0, 1            // Check BUSY bit
  0xFC75,          // bnez s0, <waitloop2>      // Loop if still busy

  // Reset buffer, don't need busywait as it'll complete before we send the next dword.
  0xC91C,          // sw   a5, 16(a0)           // FLASH_CTLR = FTPG | BUFRST

  // Update page address
  0xC950           // sw   a2, 20(a0)           // FLASH_ADDR = new address
};

//------------------------------------------------------------------------------
// NOTE: Flash write must be page-aligned: word_count has to be a whole number of pages
 
bool flash_write_pages(uint32_t addr, const uint32_t *data, size_t count) {
  CHECK(!(count % CH32_FLASH_PAGE_WORDS));
  CHECK(!(addr & 3));

  if (!flash_set_addr(addr))                                 return false;

  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))                                return false;
  if (!flash_set_ctlr(CTLR_OBWRE | CTLR_FTPG | CTLR_BUFRST)) return false;

  bool ret = false;
  ctx_load_prog((uint32_t *)prog_write, sizeof(prog_write) / 4);
  if (!gpr_cache_save(GPRB(S0) | GPRB(A0) | GPRB(A1) | GPRB(A2) | GPRB(A3) |
        GPRB(A4) | GPRB(A5)))                                goto cleanup;

  if (!gpr_set_a(0, FLASH_ACTLR))                            goto cleanup;
  if (!gpr_set_a(1, DM_DATA_ADDR))                           goto cleanup;
  if (!gpr_set_a(2, addr))                                   goto cleanup;
  if (!gpr_set_a(3, CTLR_OBWRE | CTLR_FTPG | CTLR_BUFLOAD))  goto cleanup;
  if (!gpr_set_a(4, CTLR_OBWRE | CTLR_FTPG | CTLR_STRT))     goto cleanup;
  if (!gpr_set_a(5, CTLR_OBWRE | CTLR_FTPG | CTLR_BUFRST))   goto cleanup;

  // Flash words using auto-execution
  dm_set_abstractauto(DM_AUTOEXECDATA(1));

  for (size_t i = 0; i < count; i++) {
    dm_set_data0(data[i]);
    if (!dm_abstractcs_wait())
      goto cleanup;
  }

  // Success
  ret = true;

cleanup:
  // Disable auto-execution
  dm_set_abstractauto(0);

  // Restor FLASH control register
  (void)flash_set_ctlr(ctlr.raw);

  return ret;
}

//------------------------------------------------------------------------------

bool flash_verify_pages(uint32_t addr, const uint32_t *data, size_t count) {
  CHECK(!(addr & 3));

  uint32_t bytes = count * 4;
  uint32_t *readback = malloc(bytes);
  bool ret = false;

  if (!ctx_get_block_aligned(addr, readback, bytes)) goto cleanup;
  for (size_t i = 0; i < count; i++)
    if (data[i] != readback[i])                      goto cleanup;

  // Success
  ret = true;

cleanup:
  free(readback);
  return ret;
}

//------------------------------------------------------------------------------
// Dumps flash regs and the first 768 bytes of flash.

void flash_dump(uint32_t addr) {
  ctx_dump_block(addr, CH32_FLASH_ADDR, CH32_FLASH_SIZE);

  flash_actlr actlr;
  if (flash_get_actlr(&actlr))
    flash_actlr_dump(actlr);

  flash_ctlr ctlr;
  if (flash_get_ctlr(&ctlr))
    flash_ctlr_dump(ctlr);

  flash_obr obr;
  if (flash_get_obr(&obr))
    flash_obr_dump(obr);

  flash_statr statr;
  if (flash_get_statr(&statr))
    flash_statr_dump(statr);

  uint32_t wpr;
  if (flash_get_wpr(&wpr))
    print_hex(0, "FLASH_WPR", wpr);
}

//==============================================================================
// Flash registers (memory-mapped I/O)

void flash_actlr_dump(flash_actlr r) {
  print_hex(0, "ACTLR", r.raw);
  printf("  LATENCY:%d\n", r.b.LATENCY);
}

//------------------------------------------------------------------------------

void flash_statr_dump(flash_statr r) {
  print_hex(0, "STATR", r.raw);
  printf("  BOOT_LOCK:%d  BUSY:%d  EOP:%d  MODE:%d  WRPRTERR:%d\n",
         r.b.BOOT_LOCK, r.b.BUSY, r.b.EOP, r.b.MODE, r.b.WRPRTERR);
}

//------------------------------------------------------------------------------

void flash_ctlr_dump(flash_ctlr r) {
  print_hex(0, "CTLR", r.raw);
  printf("  BUFLOAD:%d  BUFRST:%d  ERRIE:%d  EOPIE:%d  FLOCK:%d  FTER:%d  FTPG:%d\n",
         r.b.BUFLOAD, r.b.BUFRST, r.b.ERRIE, r.b.EOPIE, r.b.FLOCK, r.b.FTER, r.b.FTPG);
  printf("  LOCK:%d  MER:%d  OBER:%d  OBPG:%d  OBWRE:%d  PER:%d  PG:%d  STRT:%d\n",
         r.b.LOCK, r.b.MER, r.b.OBER, r.b.OBPG, r.b.OBWRE, r.b.PER, r.b.PG, r.b.STRT);
}

//------------------------------------------------------------------------------

void flash_obr_dump(flash_obr r) {
  print_hex(0, "OBR", r.raw);
  printf("  DATA0:%d  DATA1:%d  IWDG_SW:%d  OBERR:%d  RDPRT:%d  RST_MODE  STANDBY_RST:%d  START_MODE:%d\n",
         r.b.DATA0, r.b.DATA1, r.b.IWDG_SW, r.b.OBERR, r.b.RDPRT, r.b.RST_MODE, r.b.STANDBY_RST, r.b.START_MODE);
}

//------------------------------------------------------------------------------
