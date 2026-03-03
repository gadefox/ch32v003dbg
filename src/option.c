#include <stdio.h>

#include "flash.h"
#include "option.h"
#include "utils.h"

//------------------------------------------------------------------------------

bool optb_is_locked(void) {
  flash_ctlr ctlr;
  if ( !flash_get_ctlr(&ctlr))
    return true;
  return ctlr.b.OBWRE;
}

//------------------------------------------------------------------------------

bool optb_lock(void) {
  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))
    return false;

  if (!flash_set_ctlr(ctlr.raw & ~CTLR_OBWRE))
    return false;

  return optb_is_locked();
}

//------------------------------------------------------------------------------

bool optb_unlock(void) {
  // Unlock option bytes
  if (!optb_set_obkeyr(UNLOCK_KEY1) || !optb_set_obkeyr(UNLOCK_KEY2))
    return false;

  return !optb_is_locked();
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
  print_b(0, "WRPR%d\n", i);
  print_bits(2, "0", r.b.WRPR0, 8);
  print_bits(1, "n0", r.b.nWRPR0, 8);
  print_bits(2, "1", r.b.WRPR1, 8);
  print_bits(1, "n1", r.b.nWRPR1, 8);
}

//------------------------------------------------------------------------------
