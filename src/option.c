#include <stdio.h>

#include "option.h"
#include "utils.h"

#include <pico/time.h>
//------------------------------------------------------------------------------

inline int optb_is_unlocked(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  return ctlr.raw & CTLR_OBWRE;
}

//------------------------------------------------------------------------------

int optb_lock(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return -1;
  if (!flash_set_ctlr(ctlr.raw & ~CTLR_OBWRE))
    return -1;
  return optb_is_unlocked();
}

//------------------------------------------------------------------------------

int optb_unlock(void) {
  // Unlock option bytes
  if (!optb_set_obkeyr(UNLOCK_KEY1) || !optb_set_obkeyr(UNLOCK_KEY2))
    return -1;
  return optb_is_unlocked();
}

//------------------------------------------------------------------------------

bool optb_write(uint8_t offset, uint32_t data) {
//  if (!flash_set_addr(OPTB_ADDR + offset))                  return false;


  uint32_t addr = OPTB_ADDR + offset;
  printf("%08X |", addr);
//  if (!flash_set_addr(addr))                  return false;

  printf("-");
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))                        return false;
  printf("%08X |", ctlr.raw);
  if (!flash_set_ctlr(ctlr.raw | CTLR_OBPG))                         return false;

  flash_statr x;
  flash_get_statr(&x);
  printf("%d |", x.b.BUSY);

  bool ret = false;

  if (!ctx_set_mem32_aligned(addr, data))   goto cleanup;
  flash_get_statr(&x);
  printf("%d |", x.b.BUSY);

  if (!flash_status_wait())                               goto cleanup;
  printf("\n");

  ret = true;

cleanup:
  // Restore control register
  (void)flash_set_ctlr(ctlr.raw);
  return ret;
}

//------------------------------------------------------------------------------

void optb_dump(void) {
  print_y(0, "option:info\n");
  if (!ctx_halted_err("display option bytes"))
    return;

  ctx_dump_block(0, OPTB_ADDR, OPTB_SIZE);

  for (int i = 0; i < 2; i++) {
    optb_wrpr wrpr;
    if (optb_get_wrpr(i, &wrpr))
      optb_wrpr_dump(i + 1, wrpr);
  }

  optb_user user;
  if (optb_get_user(&user))
    optb_user_dump(user);

  optb_data data;
  if (optb_get_data(&data))
    optb_data_dump(data);
}

//==============================================================================
// Option bytes registers (memory-mapped I/O)

void optb_user_dump(optb_user r) {
  print_hex(0, "USER", r.raw);
  printf("  IWDGSW:%d   RDPR:%02X   RST_MODE:%d   STANDYRST:%d   START_MODE:%d\n",
         r.b.IWDGSW, r.b.RDPR, r.b.RST_MODE, r.b.STANDYRST, r.b.START_MODE);
  printf(" nIWDGSW:%d  nRDPR:%02X  nRST_MODE:%d  nSTANDYRST:%d  nSTART_MODE:%d\n",
         r.b.nIWDGSW, r.b.nRDPR, r.b.nRST_MODE, r.b.nSTANDYRST, r.b.nSTART_MODE);
}

//------------------------------------------------------------------------------

void optb_data_dump(optb_data r) {
  print_hex(0, "DATA", r.raw);
  printf("  0:%02X  n0:%02X  1:%02X  n1:%02X\n",
      r.b.DATA0, r.b.nDATA0, r.b.DATA1, r.b.nDATA1);
}

//------------------------------------------------------------------------------

void optb_wrpr_dump(uint8_t i, optb_wrpr r) {
  char wrpr[] = "WRPR ";
  wrpr[4] = i + '0';
  print_hex(0, wrpr, r.raw);

  print_bits(2, "0", r.b.WRPR0, 8);
  print_bits(1, "n0", r.b.nWRPR0, 8);
  print_bits(2, "1", r.b.WRPR1, 8);
  print_bits(1, "n1", r.b.nWRPR1, 8);
}

//------------------------------------------------------------------------------
