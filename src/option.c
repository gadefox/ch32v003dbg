#include <stdio.h>

#include "option.h"
#include "utils.h"

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

static uint16_t prog_write[] = {
  0x0537, 0xE000,  // lui  a0, DM_DATA_BASE[31:12]
  0x2583, 0x0F85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)  // Load dest addr from DATA1
  0x2603, 0x0F45,  // lw   a2, DM_DATA0_ADDR[11:0](a0)  // Load data from DATA0
  0x9023, 0x00C5,  // sh   a2, 0(a1)                    // Write data to memory
  0x0589,          // addi a1, a1, 2                    // Increment dest addr
  0x2C23, 0x0EB5,  // sw   a1, DM_DATA1_ADDR[11:0](a0)  // Update addr in DATA1

  // waitloop: Busywait for programming to complete
  0x2537, 0x4002,  // lui  a0, FLASH_ACTLR[31:12]       // Load flash base addr
  0x454C,          // lw   a1, 12(a0)                   // Load FLASH_STATR
  0x8985,          // andi a1, a1, 1                    // Check BUSY bit
  0xFDF5           // bnez a1, <waitloop>               // Loop if still busy                  //
};

_Static_assert(!(sizeof(prog_write) & 3), "prog_write");

//------------------------------------------------------------------------------

bool optb_write(uint32_t addr, uint8_t *data, size_t count) {
#if PROG_DUMP
  print_c("option write: addr=%08X size=%d\n", addr, count);
#endif

  flash_ctlr ctlr;
  if (!flash_get_ctlr(&ctlr))                  return false;
  if (!flash_set_ctlr(CTLR_OBWRE | CTLR_OBPG)) return false;

  ctx_load_prog((uint32_t *)prog_write, sizeof(prog_write) / 4);
  if (!gpr_cache_save(GPRB(A0) | GPRB(A1) | GPRB(A2))) return false;

  dm_set_data1(addr);

  // Write words using auto-execution
  dm_set_abstractauto(DM_AUTOEXECDATA(1));
  bool ret = false;

  for (size_t i = 0; i < count; i++) {
    dm_set_data0(data[i]);
    if (!dm_abstractcs_wait())                 goto cleanup;
  }

  // Success
  ret = true;

cleanup:
  // Disable auto-execution
  dm_set_abstractauto(0);
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
