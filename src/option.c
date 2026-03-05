#include <stdio.h>

#include "option.h"
#include "utils.h"

//------------------------------------------------------------------------------

static uint16_t stub_write[] = {
  0x0437, 0xE000,  // lui    s0, DM_DATA_BASE[31:12]
  0x0413, 0xFFFF,  // addi   s0, s0, DM_DATA_ADDR[11:0] ; s0 = 0xE00000F4
  0x4008,          // c.lw   a0, 0(s0)                  ; a0 = *s0      (DATA0)
  0x404C,          // c.lw   a1, 4(s0)                  ; a1 = *(s0+4)  (DATA1)
  0x9023, 0x00A5,  // sh     a0, 0(a1)                  ; *(uint16_t)a1 = a0
  0x0589,          // c.addi a1, 2                      ; a1 += 2
  0xC04C,          // c.sw   a1, 4(s0)                  ; *(s0+4) = a1  (DATA1)
  0x2437, 0x4002,  // lui    s0, FLASH_ACTLR[31:12]     ; s0 = 0x40022
  // loop: wait for programming to complete
  0x4448,          // c.lw   a0, 12(s0)                 ; a0 = *(s0+12)  (FLASH_STATR)
  0x8905,          // c.andi a0, 1                      ; a0 &= 1        (BUSY)
  0xFD75,          // c.bnez a0, -4                     ; If a0 == 1 -> goto loop
  0x9002           // c.ebreak
};

_Static_assert(!(sizeof(stub_write) & 3), "stub_write");

//------------------------------------------------------------------------------

inline void optb_set_stub_opcode(uint16_t opcode) {
  stub_write[3] = opcode;
}

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

bool optb_write(uint8_t offset, uint8_t *data, uint8_t count) {
#if PROG_DUMP
  print_c("option write: addr=%d size=%d\n", offset, count);
#endif

  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))                          return false;
  if (!flash_set_ctlr(CTLR_OBWRE | CTLR_OBPG))         return false;

  bool ret = false;

  ctx_load_prog((uint32_t *)stub_write, sizeof(stub_write) / 4);
  if (!gpr_cache_save(GPRB(S0) | GPRB(A0) | GPRB(A1))) goto cleanup;

  dm_set_data1(offset + OPTB_ADDR);
  dm_set_abstractauto(DM_AUTOEXECDATA(1));

  for (size_t i = 0; i < count; i++) {
    dm_set_data0(data[i]);
    if (!dm_abstractcs_wait())                         goto cleanup;
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
