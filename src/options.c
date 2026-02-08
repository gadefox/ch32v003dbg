#include <stdio.h>

#include "flash.h"
#include "options.h"
#include "utils.h"

//------------------------------------------------------------------------------

bool optb_is_locked(void) {
  flash_ctlr ctlr = flash_get_ctlr();
  return ctlr.b.OBWRE;
}

//------------------------------------------------------------------------------

bool optb_lock(void) {
  flash_ctlr ctlr = flash_get_ctlr();
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
  print_y(0, "options:dump\n");

  ctx_dump_block(0, OPTB_ADDR, OPTB_SIZE);

  optb_wrpr1 wrpr1 = optb_get_wrpr1();
  optb_wrpr1_dump(wrpr1);

  optb_wrpr2 wrpr2 = optb_get_wrpr2();
  optb_wrpr2_dump(wrpr2);

  optb_user user = optb_get_user();
  optb_user_dump(user);

  optb_data data = optb_get_data();
  optb_data_dump(data);
}

//==============================================================================
// Option bytes registers (memory-mapped I/O)

void optb_user_dump(optb_user r) {
  print_b(0, "USER\n");
  printf("  %08X\n", r.raw);
  printf("  IWDGSW:%d   RDPR:%02X   RST_MODE:%d   STANDYRST:%d   START_MODE:%d\n",
         r.b.IWDGSW, r.b.RDPR, r.b.RST_MODE, r.b.STANDYRST, r.b.START_MODE);
  printf(" nIWDGSW:%d  nRDPR:%02X  nRST_MODE:%d  nSTANDYRST:%d  nSTART_MODE:%d\n",
         r.b.nIWDGSW, r.b.nRDPR, r.b.nRST_MODE, r.b.nSTANDYRST, r.b.nSTART_MODE);
}

//------------------------------------------------------------------------------

void optb_data_dump(optb_data r) {
  print_b(0, "DATA\n");
  printf("  %08X\n", r.raw);
  printf("  0:%02X  n0:%02X  1:%02X  n1:%02X\n",
      r.b.DATA0, r.b.nDATA0, r.b.DATA1, r.b.nDATA1);
}

//------------------------------------------------------------------------------

void optb_wrpr1_dump(optb_wrpr1 r) {
  print_bits(1, "WRPR0", r.b.WRPR0, 8);
  print_bits(0, "nWRPR0", r.b.nWRPR0, 8);
  print_bits(1, "WRPR1", r.b.WRPR1, 8);
  print_bits(0, "nWRPR1", r.b.nWRPR1, 8);
}

//------------------------------------------------------------------------------

void optb_wrpr2_dump(optb_wrpr2 r) {
  print_bits(1, "WRPR2", r.b.WRPR2, 8);
  print_bits(0, "nWRPR2", r.b.nWRPR2, 8);
  print_bits(1, "WRPR3", r.b.WRPR3, 8);
  print_bits(0, "nWRPR3", r.b.nWRPR3, 8);
}

//------------------------------------------------------------------------------
