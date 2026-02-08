#include <stdio.h>
#include <stdlib.h>
#include <pico/time.h>

#include "flash.h"
#include "swio.h"
#include "utils.h"

//==============================================================================
// API

bool flash_is_fpec_locked(void) {
  flash_ctlr ctlr = flash_get_ctlr();
  return ctlr.raw & CTLR_LOCK;
}

//------------------------------------------------------------------------------

bool flash_lock_fpec(void) {
  flash_ctlr ctlr = flash_get_ctlr();
  if (!flash_set_ctlr(ctlr.raw | CTLR_LOCK))
    return false;

  return flash_is_fpec_locked();
}

//------------------------------------------------------------------------------

bool flash_unlock_fpec(void) {
  // Unlock FPEC controller
  if (!flash_set_keyr(UNLOCK_KEY1) || !flash_set_keyr(UNLOCK_KEY2))
    return false;

  return !flash_is_fpec_locked();
}

//------------------------------------------------------------------------------

bool flash_is_fast_prog_locked(void) {
  flash_ctlr ctlr = flash_get_ctlr();
  return ctlr.raw & CTLR_FLOCK;
}

//------------------------------------------------------------------------------

bool flash_lock_fast_prog(void) {
  flash_ctlr ctlr = flash_get_ctlr();
  if (!flash_set_ctlr(ctlr.raw | CTLR_FLOCK))
    return false;

  return flash_is_fast_prog_locked();
}

//------------------------------------------------------------------------------

bool flash_unlock_fast_prog(void) {
  // Unlock fast programming
  if (!flash_set_mode_keyr(UNLOCK_KEY1) || !flash_set_mode_keyr(UNLOCK_KEY2))
    return false;

  return !flash_is_fast_prog_locked();
}

//------------------------------------------------------------------------------
// NOTE: Flash erase timing (measured): 1 page ≈ 3.3 ms
// 1 sector (16 pages) ≈ 50 ms
// chip erase ≈ 3.5 ms (global erase, not cumulative)

static bool flash_wait_busy(uint32_t timeout_us) {
  uint32_t deadline = time_us_32() + timeout_us;
  bool logged = false;

  while (true) {
    flash_statr statr = flash_get_statr();
    if (!statr.b.BUSY)
      return true;

    if ((int32_t)(time_us_32() - deadline) >= 0) {
      LOG_R("flash:STATR.BUSY timeout\n");
      return false;  // timeout
    }

    LOG_ONCE(logged, "flash:STATR.BUSY not cleared yet\n");
  }
}

//------------------------------------------------------------------------------
// NOTE:performance comparison (16KB, 256 pages):
// - Debug register erase: 997ms (~3.9ms/page)
// - On-chip code erase:   917ms (~3.6ms/page)
// Speedup: 1.09x (9% faster with on-chip code) → minimal difference
// Erase time dominated by flash HW, not debug overhead

bool flash_erase(uint32_t addr, uint32_t ctlr_bits, uint32_t timeout_us) {
  // Set target address and erase mode
  if (!flash_set_addr(addr)) return false;

  flash_ctlr ctlr_save = flash_get_ctlr();
  if (!flash_set_ctlr(ctlr_bits)) return false;

  bool ret = false;  // from here on, cleanup is required

  // Start the erase operation
  if (!flash_set_ctlr(ctlr_bits | CTLR_STRT)) goto cleanup;

  // Wait for erase to complete
  if (!flash_wait_busy(timeout_us)) goto cleanup;

  ret = true;

cleanup:
  // Restore control register
  (void)flash_set_ctlr(ctlr_save.raw);
  return ret;
}

//------------------------------------------------------------------------------

/*
const uint16_t prog_command[] = {
  0xc94c,          // sw   a1, 20(a0)
  0xc910,          // sw   a2, 16(a0)
  0xc914,          // sw   a3, 16(a0)

  // waitloop:
  0x455c,          // lw   a5, 12(a0)
  0x8b85,          // andi a5, a5, 1
  0xfff5,          // bnez a5, <waitloop>

  0x2823, 0x0005,  // sw   zero, 16(a0)
  0x9002,          // ebreak
  0x0001           // nop
};

bool flash_erase_command(uint32_t addr, uint32_t ctlr, uint32_t timeout_us) {
  ctx_load_prog((uint32_t*)prog_command, sizeof(prog_command) / sizeof(uint32_t),
      GPRB_A0 | GPRB_A1 | GPRB_A2 | GPRB_A3 | GPRB_A5);
  gpr_set_a0(FLASH_ACTLR);
  gpr_set_a1(addr);
  gpr_set_a2(ctlr);
  gpr_set_a3(ctlr | CTLR_STRT);
  return ctx_run_prog(timeout_us);
}
*/

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
  0xc200,          // sw   s0, 0(a2)            // Store to destination address
  0xc914,          // sw   a3, 16(a0)           // FLASH_CTLR = FTPG | BUFLOAD

  // waitloop1: Busywait for copy to complete - this seems to be required now?
  0x4540,          // lw   s0, 12(a0)           // Load FLASH_STATR
  0x8805,          // andi s0, s0, 1            // Check BUSY bit
  0xfc75,          // bnez s0, <waitloop1>      // Loop if still busy

  // Advance dest pointer and trigger START if we ended a page
  0x0611,          // addi a2, a2, 4            // dst_addr += 4
  0x7413, 0x03f6,  // andi s0, a2, 63           // Check lower 6 bits
  0xe419,          // bnez s0, <end>            // If not page boundary, done
  0xc918,          // sw   a4, 16(a0)           // FLASH_CTLR = FTPG | STRT

  // waitloop2: Busywait for page write to complete
  0x4540,          // lw   s0, 12(a0)           // Load FLASH_STATR
  0x8805,          // andi s0, s0, 1            // Check BUSY bit
  0xfc75,          // bnez s0, <waitloop2>      // Loop if still busy

  // Reset buffer, don't need busywait as it'll complete before we send the next dword.
  0xc91c,          // sw   a5, 16(a0)           // FLASH_CTLR = FTPG | BUFRST

  // Update page address
  0xc950           // sw   a2, 20(a0)           // FLASH_ADDR = new address
};

//------------------------------------------------------------------------------
// NOTE: Flash write must be page-aligned: word_count has to be a whole number of pages
 
bool flash_write_pages(uint32_t dst_addr, const uint32_t* src_data, uint32_t word_count) {
  CHECK((word_count % CH32_FLASH_PAGE_WORDS) == 0);
  CHECK((dst_addr & 3) == 0);

  if (!flash_set_addr(dst_addr))                return false;

  flash_ctlr ctlr_save = flash_get_ctlr();
  if (!flash_set_ctlr(CTLR_FTPG | CTLR_BUFRST)) return false;

  ctx_load_prog((uint32_t*)prog_write, sizeof(prog_write) / sizeof(uint32_t),
      GPRB_S0 | GPRB_A0 | GPRB_A1 | GPRB_A2 | GPRB_A3 | GPRB_A4 | GPRB_A5);

  gpr_set_a0(FLASH_ACTLR);
  gpr_set_a1(DM_DATA0_ADDR);
  gpr_set_a2(dst_addr);
  gpr_set_a3(CTLR_FTPG | CTLR_BUFLOAD);
  gpr_set_a4(CTLR_FTPG | CTLR_STRT);
  gpr_set_a5(CTLR_FTPG | CTLR_BUFRST);

  dm_set_data0(src_data[0]);

  bool ret = false;  // from here on, cleanup is required
  if (!ctx_run_prog(1000)) goto cleanup;               // 1 ms
  dm_set_abstractauto(DMAA_AUTOEXECDATA(1));

  for (uint32_t i = 1; i < word_count; i++) {
    dm_set_data0(src_data[i]);
    if (!ctx_abstracts_wait_busy(3000)) goto cleanup;  // 3 ms
  }

  ret = true;

cleanup:
  // Disable automatic debug/abstract commands
  dm_set_abstractauto(0);

  // Restor FLASH control register
  (void)flash_set_ctlr(ctlr_save.raw);

  return ret;
}

//------------------------------------------------------------------------------

bool flash_verify_pages(uint32_t addr, const uint32_t* data, uint32_t word_count) {
  CHECK((addr & 3) == 0);

  uint32_t bytes = word_count * sizeof(uint32_t);
  uint32_t* readback = malloc(bytes);
  ctx_get_block_aligned(addr, readback, bytes);
  
  bool ret = false;  // from here on, cleanup is required

  for (uint32_t i = 0; i < word_count; i++) {
    if (data[i] != readback[i])
      goto cleanup;
  }

  ret = true;

cleanup:
  free(readback);
  return ret;
}

//------------------------------------------------------------------------------
// Dumps flash regs and the first 768 bytes of flash.

void flash_dump(uint32_t addr) {
  ctx_dump_block(addr, CH32_FLASH_ADDR, CH32_FLASH_SIZE);

  flash_actlr actlr = flash_get_actlr();
  flash_actlr_dump(actlr);

  flash_ctlr ctlr = flash_get_ctlr();
  flash_ctlr_dump(ctlr);

  flash_obr obr = flash_get_obr();
  flash_obr_dump(obr);

  flash_statr statr = flash_get_statr();
  flash_statr_dump(statr);

  uint32_t wpr = flash_get_wpr();
  print_hex(0, "FLASH_WPR", wpr);
}

//==============================================================================
// Flash registers (memory-mapped I/O)

void flash_actlr_dump(flash_actlr r) {
  print_b(0, "ACTLR\n");
  printf("  %08X\n", r.raw);
  printf("  LATENCY:%d\n", r.b.LATENCY);
}

//------------------------------------------------------------------------------

void flash_statr_dump(flash_statr r) {
  print_b(0, "STATR\n");
  printf("  %08X\n", r.raw);
  printf("  BOOT_LOCK:%d  BUSY:%d  EOP:%d  MODE:%d  WRPRTERR:%d\n",
         r.b.BOOT_LOCK, r.b.BUSY, r.b.EOP, r.b.MODE, r.b.WRPRTERR);
}

//------------------------------------------------------------------------------

void flash_ctlr_dump(flash_ctlr r) {
  print_b(0, "CTLR\n");
  printf("  %08X\n", r.raw);
  printf("  BUFLOAD:%d  BUFRST:%d  ERRIE:%d  EOPIE:%d  FLOCK:%d  FTER:%d  FTPG:%d\n",
         r.b.BUFLOAD, r.b.BUFRST, r.b.ERRIE, r.b.EOPIE, r.b.FLOCK, r.b.FTER, r.b.FTPG);
  printf("  LOCK:%d  MER:%d  OBER:%d  OBG:%d  OBWRE:%d  PER:%d  PG:%d  STRT:%d\n",
         r.b.LOCK, r.b.MER, r.b.OBER, r.b.OBG, r.b.OBWRE, r.b.PER, r.b.PG, r.b.STRT);
}

//------------------------------------------------------------------------------

void flash_obr_dump(flash_obr r) {
  print_b(0, "OBR\n");
  printf("  %08X\n", r.raw);
  printf("  CFGRSTT:%d  DATA0:%d  DATA1:%d  IWDG_SW:%d  OBERR:%d  RDPRT:%d  STANDBY_RST:%d\n",
         r.b.CFGRSTT, r.b.DATA0, r.b.DATA1, r.b.IWDG_SW, r.b.OBERR, r.b.RDPRT, r.b.STANDBY_RST);
}

//------------------------------------------------------------------------------
