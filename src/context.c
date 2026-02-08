#include <stdio.h>
#include <string.h>
#include <pico/time.h>

#include "context.h"
#include "swio.h"

//==============================================================================
// API

static uint32_t prog_clobber; // Bits are 1 if running the current program will clober the reg
static uint32_t prog_size;
static uint32_t prog_cache[DM_PROGBUF_MAX];

static uint32_t reg_cache[GPR_MAX];

static uint32_t dirty_regs;  // bits are 1 if we modified the reg on device
static uint32_t cached_regs; // bits are 1 if reg_cache[i] is valid

//------------------------------------------------------------------------------

void ctx_init(void) {
  prog_clobber = 0;
  prog_size = 0;

  for (int i = 0; i < DM_PROGBUF_MAX; i++)
    prog_cache[i] = 0xDEADBEEF;
  for (int i = 0; i < GPR_MAX; i++)
    reg_cache[i] = 0xDEADBEEF;

  dirty_regs = 0;
  cached_regs = 0;
}

//------------------------------------------------------------------------------

#if LOGS

static void ctx_clear_err(void) {
  dm_abstractcs abstractcs = dm_get_abstractcs();
  abstractcs.b.CMDER = DMAB_CMDER_OTH_ERR;
  dm_set_abstractcs(abstractcs.raw);
}

//------------------------------------------------------------------------------

static void test_block(uint32_t addr, uint32_t cmder) {
  uint32_t block[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
  bool result;

  printf("  addr: %08X, expected: %d", addr, cmder);

  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);
  CHECK(ctx_set_block_aligned(addr, block, countof(block)));
  CHECK(dm_get_abstractcs().b.CMDER == cmder);

  if (cmder != DMAB_CMDER_SUCCESS)
    ctx_clear_err();

  print_g(2, "on\n");
}

//------------------------------------------------------------------------------

static const uint8_t crc_data[1024] = {
  0x74, 0x72, 0x75, 0x6e, 0x63, 0x61, 0x74, 0x65, 0x20, 0x2d, 0x2d, 0x73, 0x69,
  0x7a, 0x65, 0x3d, 0x30, 0x20, 0x7e, 0x2f, 0x2e, 0x78, 0x73, 0x65, 0x73, 0x73,
  0x69, 0x6f, 0x6e, 0x2d, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x73, 0xa, 0x66, 0x69,
  0x6e, 0x64, 0x20, 0x2f, 0x75, 0x73, 0x72, 0x20, 0x2d, 0x75, 0x73, 0x65, 0x72,
  0x20, 0x66, 0x6f, 0x78, 0xa
};

static const uint8_t crc_data2[] = {
  0x6f, 0x0, 0x40, 0xa, 0x0, 0x0, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0,
  0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36,
  0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0,
  0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0,
  0x0, 0x36, 0x1, 0x0, 0x0, 0x36, 0x1, 0x0, 0x0
};

//------------------------------------------------------------------------------

static void test_crc(const uint8_t* data, uint32_t size, uint16_t expected) {
  uint16_t crc = xmodem_crc_calc(data, size);

  printf("  size: %d  crc: %04X", size, crc);
  if (crc == expected)
    print_g(2, "ok\n");
  else
    print_r(2, "wrong (expected %04X)\n", expected);
}

//------------------------------------------------------------------------------

void ctx_test(void) {
  uint32_t base = 0x20000400;

  print_y(0, "ctx:test\n");
  CHECK(ctx_reset());
  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);

  // Test misaligned reads
  print_b(0, "misaligned reads\n");
  for (int offset = 0; offset < 4; offset++) {
    printf("  offset: %d", offset);

    for (int i = 0; i < 8; i++)
      CHECK(ctx_set_mem_u8(base + i + offset, i + 1));
    for (int i = 0; i < 8; i += 4)
      CHECK(ctx_get_mem_u32(base + offset + i) == 0x04030201 + 0x04040404 * (i >> 2));
    for (int i = 0; i < 8; i += 2)
      CHECK(ctx_get_mem_u16(base + offset + i) == 0x0201 + 0x0202 * (i >> 1));
    for (int i = 0; i < 8; i++)
      CHECK(ctx_get_mem_u8(base + offset + i) == i + 1);

    print_g(2, "ok\n");
  }
  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);

  // Test misaligned writes
  print_b(0, "misaligned writes\n");
  for (int offset = 0; offset < 4; offset++) {
    printf("  offset: %d", offset);

    for (int i = 0; i < 8; i += 4)
      CHECK(ctx_set_mem_u32(base + offset + i, 0x04030201 + 0x04040404 * (i >> 2)));
    for (int i = 0; i < 8; i++)
      CHECK(ctx_get_mem_u8(base + offset + i) == i + 1);
    for (int i = 0; i < 8; i += 2)
      CHECK(ctx_set_mem_u8(base + offset + i, i + 1));
    for (int i = 0; i < 8; i += 2)
      CHECK(ctx_set_mem_u16(base + offset + i, 0x0201 + 0x0202 * (i >> 1)));
    for (int i = 0; i < 8; i++)
      CHECK(ctx_get_mem_u8(base + offset + i) == i + 1);
    for (int i = 0; i < 8; i++)
      CHECK(ctx_set_mem_u8(base + offset + i, i + 1));
    for (int i = 0; i < 8; i++)
      CHECK(ctx_get_mem_u8(base + offset + i) == i + 1);

    print_g(2, "ok\n");
  }
  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);

  // Test aligned block reads
  print_b(0, "aligned block reads\n");
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      printf("  size: %d, offset: %d", size, offset);

      for (int i = 0; i < offset; i++)
        CHECK(ctx_set_mem_u8(base + i, 0xFF));
      for (int i = offset; i < offset + size; i++)
        CHECK(ctx_set_mem_u8(base + i, i + 1));
      for (int i = offset + size; i < 16; i++)
        CHECK(ctx_set_mem_u8(base + i, 0xFF));

      uint8_t buf[16];
      memset(buf, 0xFF, sizeof(buf));
      CHECK(ctx_get_block_aligned(base + offset,
            (uint32_t*)(buf + 4), size / sizeof(uint32_t)));

      for (int i = 0; i < 4; i++)
        CHECK(buf[i] == 0xFF);
      for (int i = 4; i < size + 4; i++)
        CHECK(buf[i] == i + offset - 3);
      for (int i = size + 4; i < 16; i++)
        CHECK(buf[i] == 0xFF);

      print_g(2, "ok\n");
    }
  }
  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);

  // Test aligned block writes
  print_b(0, "aligned block writes\n");
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      printf("  size: %d, offset: %d", size, offset);

      for (int i = 0; i < 4; i++)
        CHECK(ctx_set_mem_u32(base + (i * sizeof(uint32_t)), 0xFFFFFFFF));

      uint8_t buf[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
      CHECK(ctx_set_block_aligned(base + offset, (uint32_t*)buf, size / sizeof(uint32_t)));

      for (int i = 0; i < offset; i++)
        CHECK(ctx_get_mem_u8(base + i) == 0xFF);
      for (int i = 0; i < size; i++)
        CHECK(ctx_get_mem_u8(base + offset + i) == i + 1);
      for (int i = offset + size; i < 16; i++)
        CHECK(ctx_get_mem_u8(base + i) == 0xFF);

      print_g(2, "ok\n");
    }
  }
  CHECK(dm_get_abstractcs().b.CMDER == DMAB_CMDER_SUCCESS);

  // Test block writes at both ends of memory
  print_b(0, "block writes at both ends of memory\n");
  test_block(0x20000000, DMAB_CMDER_SUCCESS);
  test_block(0x20000000 - 4, DMAB_CMDER_EXC_ERR);   // Off the low end by 4
  test_block(0x20000800 - 16, DMAB_CMDER_SUCCESS);
  test_block(0x20000800 - 12, DMAB_CMDER_EXC_ERR);  // Off the high end by 4

  // Test CRC-16 calculation using DMA sniffer
  print_b(0, "testing CRC-16 via DMA\n");
  for (int i = 0; i < 4; i++)
    test_crc(crc_data2, countof(crc_data2), 0x546C);

  test_crc(crc_data, 57, 0x2545);
  test_crc(crc_data, 128, 0x95EB);
  for (int i = 0; i < 4; i++)
    test_crc(crc_data, countof(crc_data), 0x978C);
}

#endif /* LOGS */

//------------------------------------------------------------------------------

#define DM_STATUS_WAIT_TIMEOUT  500000

bool ctx_dm_status_wait(uint32_t mask, bool want_set, uint32_t timeout_us) {
  uint32_t deadline = time_us_32() + timeout_us;  // 500 ms
  bool logged = false;

  while (true) {
    dm_status status = dm_get_status();
    bool is_set = (status.raw & mask) != 0;
    if (is_set == want_set)
      return true;
    
    if ((int32_t)(time_us_32() - deadline) >= 0) {
      LOG_R("ctx:DM_STATUS(%08X) %s timeout\n", mask, want_set ? "set" : "clear");
      return false;  // timeout
    }

    LOG_ONCE(logged, "ctx:DM_STATUS(%08X) not %s yet\n", mask, want_set ? "set" : "cleared");
  }
}

//------------------------------------------------------------------------------

bool ctx_halt(void) {
  dm_set_control(DMCN_DMACTIVE | DMCN_HALTREQ);
  if (!ctx_dm_status_wait(DMST_ALLHALTED, true, DM_STATUS_WAIT_TIMEOUT))
    return false;

  dm_set_control(DMCN_DMACTIVE);
  return true;
}

//------------------------------------------------------------------------------

bool ctx_resume(void) {
  if (dm_get_status().b.ALLHAVERESET) {
    LOG_R(0, "ctx: can't resume while in reset!\n");
    return false;
  }

  ctx_reload_regs();
  dm_set_control(DMCN_DMACTIVE | DMCN_RESUMEREQ);
  dm_set_control(DMCN_DMACTIVE);
  cached_regs = 0;
  return true;
}

//------------------------------------------------------------------------------

bool ctx_step(void) {
  if (dm_get_status().b.ALLHAVERESET) {
    LOG_R(0, "ctx: can't step while in reset!\n");
    return false;
  }

  csr_dcsr dcsr = csr_get_dcsr();
  dcsr.b.STEP = 1;
  csr_set_dcsr(dcsr.raw);

  ctx_resume();

  dcsr.b.STEP = 0;
  csr_set_dcsr(dcsr.raw);

  return true;
}

//------------------------------------------------------------------------------

bool ctx_reset(void) {
  // Halt and leave halt request set
  dm_set_control(DMCN_DMACTIVE | DMCN_HALTREQ);
  if (!ctx_dm_status_wait(DMST_ALLHALTED, true, DM_STATUS_WAIT_TIMEOUT)) return false;

  // Set reset request
  dm_set_control(DMCN_DMACTIVE | DMCN_NDMRESET | DMCN_HALTREQ);
  if (!ctx_dm_status_wait(DMST_ALLHAVERESET, true, DM_STATUS_WAIT_TIMEOUT)) return false;

  // Clear reset request and hold halt request
  dm_set_control(DMCN_DMACTIVE | DMCN_HALTREQ);
  if (!ctx_dm_status_wait(DMST_ALLHALTED, true, DM_STATUS_WAIT_TIMEOUT)) return false;

  // Clear HAVERESET
  dm_set_control(DMCN_DMACTIVE | DMCN_ACKHAVERESET | DMCN_HALTREQ);
  if (!ctx_dm_status_wait(DMST_ALLHAVERESET, false, DM_STATUS_WAIT_TIMEOUT)) return false;

  // Clear halt request
  dm_set_control(DMCN_DMACTIVE);

  // Resetting the CPU also resets DCSR, redo it
  ctx_enable_breakpoints();

  // Reset cached state
  ctx_init();
  return true;
}

//------------------------------------------------------------------------------

void ctx_enable_breakpoints(void) {
  CHECK(dm_get_status().b.ALLHALTED);

  // Turn on debug breakpoints & stop counters/timers during debug
  csr_dcsr dcsr = csr_get_dcsr();
  dcsr.raw |= DCSR_STOPTIME | DCSR_STOPCOUNT | DCSR_EBREAKU | DCSR_EBREAKS | DCSR_EBREAKM;
  dcsr.raw &= ~DCSR_STEPIE;
  csr_set_dcsr(dcsr.raw);
}

//------------------------------------------------------------------------------

void ctx_load_prog(const uint32_t *prog, uint32_t size, uint32_t clobber) {
  // Upload any DM_PROGBUF{N} word that changed
  for (int i = 0; i < size; i++) {
    if (prog_cache[i] != prog[i]) {
      dm_set_progbuf(i, prog[i]);
      prog_cache[i] = prog[i];
    }
  }

  // Save any registers this program is going to clobber
  for (int i = 0; i < GPR_MAX; i++) {
    uint32_t mask = 1 << i;

    if (!(clobber & mask) || cached_regs & mask)
      continue;

    if (dirty_regs & mask) {
      print_r(0, "ctx:load: reg %d is about to be clobbered, but we can't get a clean copy because it's already dirty\n");
      continue;
    }

    reg_cache[i] = ctx_get_gpr(i);
    cached_regs |= mask;
  }

  prog_size = size;
  prog_clobber = clobber;
}

//------------------------------------------------------------------------------

bool ctx_abstracts_wait_busy(uint32_t timeout_us) {
  uint32_t deadline = time_us_32() + timeout_us;
//  bool logged = false;

  while (true) {
    dm_abstractcs abstractcs = dm_get_abstractcs();
    if (!abstractcs.b.BUSY)
      return true;
    
    if ((int32_t)(time_us_32() - deadline) >= 0) {
      LOG_R("ctx:DM_ABSTRACTCS.BUSY timeout\n");
      return false;  // timeout
    }

//    LOG_ONCE(logged, "ctx:DM_ABSTRACTCS.BUSY not cleared yet\n");
  }
}

//------------------------------------------------------------------------------

bool ctx_run_prog(uint32_t timeout_us) {
  // We can NOT save registers here, as doing so would clobber DATA0 which may
  // be loaded with something the program needs.
  dm_set_command(DMCM_POSTEXEC);

  // Wait for device to become ready
  if (!ctx_abstracts_wait_busy(timeout_us))
    return false;

  dirty_regs |= prog_clobber;
  return true;
}

//------------------------------------------------------------------------------
// Getting multiple GPRs via autoexec is not supported on CH32V003

uint32_t ctx_get_gpr(int index) {
  if (index == GPR_MAX)
    return csr_get_dpc();

  dm_set_command(DMCM_REGNO_GPR(index) | DMCM_TRANSFER | DMCM_AARSIZE_32BIT);
  return dm_get_data0();
}

//------------------------------------------------------------------------------

void ctx_set_gpr(int index, uint32_t gpr) {
  if (index == GPR_MAX)
    csr_set_dpc(gpr);
  else {
    dm_set_data0(gpr);
    dm_set_command(DMCM_REGNO_GPR(index) | DMCM_WRITE | DMCM_TRANSFER | DMCM_AARSIZE_32BIT);
  }
}

//------------------------------------------------------------------------------

void ctx_reload_regs(void) {
  for (int i = 0; i < GPR_MAX; i++) {
    uint32_t mask = 1 << i;

    if (!(dirty_regs & mask))
      continue;

    if (!(cached_regs & mask)) {
      print_r(0, "ctx:regs:reload: GPR %d is dirty and we dont' have a saved copy!\n", i);
      continue;
    }

    ctx_set_gpr(i, reg_cache[i]);
  }

  dirty_regs = 0;
}

//------------------------------------------------------------------------------

uint32_t ctx_get_csr(int index) {
  dm_set_command(DMCM_REGNO_CSR(index) | DMCM_TRANSFER | DMCM_AARSIZE_32BIT);
  return dm_get_data0();
}

//------------------------------------------------------------------------------

void ctx_set_csr(int index, uint32_t data) {
  dm_set_data0(data);
  dm_set_command(DMCM_REGNO_CSR(index) | DMCM_WRITE | DMCM_TRANSFER | DMCM_AARSIZE_32BIT);
}

//------------------------------------------------------------------------------

uint32_t ctx_get_mem_u32(uint32_t addr) {
  int offset = addr & 3;
  int addr_lo = addr & ~3;
  int addr_hi = (addr + 3) & ~3;

  int data_lo = ctx_get_mem_u32_aligned(addr_lo);
  if (offset == 0)
    return data_lo;

  int data_hi = ctx_get_mem_u32_aligned(addr_hi);
  return (data_lo >> (offset * 8)) | (data_hi << (32 - offset * 8));
}

//------------------------------------------------------------------------------

uint16_t ctx_get_mem_u16(uint32_t addr) {
  int offset = addr & 3;
  int addr_lo = (addr + 0) & ~3;
  int addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = ctx_get_mem_u32_aligned(addr_lo);
  if (offset < 3)
    return data_lo >> (offset * 8);

  uint32_t data_hi = ctx_get_mem_u32_aligned(addr_hi);
  return (data_lo >> 24) | (data_hi << 8);
}

//------------------------------------------------------------------------------

uint8_t ctx_get_mem_u8(uint32_t addr) {
  int offset = addr & 3;
  int addr_lo = addr & ~3;
  uint32_t data_lo = ctx_get_mem_u32_aligned(addr_lo);
  return data_lo >> (offset * 8);
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u32(uint32_t addr, uint32_t data) {
  int offset = addr & 3;
  int addr_lo = addr & ~3;
  int addr_hi = (addr + 4) & ~3;

  if (offset == 0)
    return ctx_set_mem_u32_aligned(addr_lo, data);

  uint32_t data_lo = ctx_get_mem_u32_aligned(addr_lo);
  uint32_t data_hi = ctx_get_mem_u32_aligned(addr_hi);

  if (offset == 1) {
    data_lo &= 0x000000FF;
    data_lo |= data << 8;
    data_hi &= 0xFFFFFF00;
    data_hi |= data >> 24;
  } else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_lo |= data << 16;
    data_hi &= 0xFFFF0000;
    data_hi |= data >> 16;
  } else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_lo |= data << 24;
    data_hi &= 0xFF000000;
    data_hi |= data >> 8;
  }

  return ctx_set_mem_u32_aligned(addr_lo, data_lo) &&
         ctx_set_mem_u32_aligned(addr_hi, data_hi);
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u16(uint32_t addr, uint16_t data) {
  int offset = addr & 3;
  int addr_lo = addr & ~3;
  int addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = ctx_get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo &= 0xFFFF0000;
    data_lo |= data;
  } else if (offset == 1) {
    data_lo &= 0xFF0000FF;
    data_lo |= data << 8;
  } else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_lo |= data << 16;
  } else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_lo |= data << 24;
  }

  if (!ctx_set_mem_u32_aligned(addr_lo, data_lo))
    return false;

  if (offset == 3) {
    uint32_t data_hi = ctx_get_mem_u32_aligned(addr_hi);
    data_hi &= 0xFFFFFF00;
    data_hi |= data >> 8;
    return ctx_set_mem_u32_aligned(addr_hi, data_hi);
  }

  return true;
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u8(uint32_t addr, uint8_t data) {
  int offset = addr & 3;
  int addr_lo = addr & ~3;

  uint32_t data_lo = ctx_get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo &= 0xFFFFFF00;
    data_lo |= data;
  } else if (offset == 1) {
    data_lo &= 0xFFFF00FF;
    data_lo |= data << 8;
  } else if (offset == 2) {
    data_lo &= 0xFF00FFFF;
    data_lo |= data << 16;
  } else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_lo |= data << 24;
  }

  return ctx_set_mem_u32_aligned(addr_lo, data_lo);
}

//------------------------------------------------------------------------------
// data0 = data to write
// data1 = address. set low bit if this is a write
// only clobbers A0/A1

static uint16_t prog_get_set_u32[] = {
  0x0537, 0xe000,  // lui  a0, DM_DATA0_ADDR[31:12]
  0x0513, 0x0f45,  // addi a0, a0, DM_DATA0_ADDR[11:0]
  0x414c,          // lw   a1, 4(a0)                  // Load addr from DATA1
  0x8985,          // andi a1, a1, 1                  // Check LSB (set/get flag)
  0xc591,          // beqz a1, get_u32                // If 0 -> read
                // set_u32:
  0x414c,          // lw   a1, 4(a0)                  // Reload addr from DATA1
  0x15fd,          // addi a1, a1, -1                 // Clear LSB flag
  0x4108,          // lw   a0, 0(a0)                  // Load data from DATA0
  0xc188,          // sw   a0, 0(a1)                  // Write to memory
  0x9002,          // ebreak
                // get_u32:
  0x414c,          // lw   a1, 4(a0)                  // Load addr from DATA1
  0x418c,          // lw   a1, 0(a1)                  // Read from memory
  0xc10c,          // sw   a1, 0(a0)                  // Write to DATA0
  0x9002           // ebreak
};

uint32_t ctx_get_mem_u32_aligned(uint32_t addr) {
  ctx_load_prog((uint32_t*)prog_get_set_u32,
      sizeof(prog_get_set_u32) / sizeof(uint32_t), GPRB_A0 | GPRB_A1);

  dm_set_data1(addr);
  if (!ctx_run_prog(10000))  // 10 ms
    return 0;
  return dm_get_data0();
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  ctx_load_prog((uint32_t*)prog_get_set_u32,
      sizeof(prog_get_set_u32) / sizeof(uint32_t), GPRB_A0 | GPRB_A1);

  dm_set_data0(data);
  dm_set_data1(addr | 1);
  return ctx_run_prog(100000);  // 100 ms
}

//------------------------------------------------------------------------------

static uint16_t prog_get_block_aligned[] = {
  0x0537, 0xe000,  // lui  a0, DM_DATA1_ADDR[31:12]
  0x2583, 0x0f85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Load src addr
  0x418c,          // lw   a1, 0(a1)                        // Read from memory
  0x2a23, 0x0eb5,  // sw   a1, DM_DATA0_ADDR[11:0](a0)      // Write to DATA0
  0x2583, 0x0f85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Reload addr
  0x0591,          // addi a1, a1, 4                        // Increment addr
  0x2c23, 0x0eb5,  // sw   a1, DM_DATA1_ADDR[11:0](a0)      // Update addr
  0x9002,          // ebreak
  0x0001           // nop
};

bool ctx_get_block_aligned(uint32_t addr, uint32_t* data, uint32_t data_count) {
  CHECK((addr & 3) == 0);

  ctx_load_prog((uint32_t*)prog_get_block_aligned,
      sizeof(prog_get_block_aligned) / sizeof(uint32_t), GPRB_A0 | GPRB_A1);

  dm_set_data1(addr);
  if (!ctx_run_prog(10000))  // 10 ms
    return false;
  
  bool ret = false;  // from here on, cleanup is required
  dm_set_abstractauto(DMAA_AUTOEXECDATA(1));
  data[0] = dm_get_data0();

  // Read words using auto-execution
  for (int i = 1; i < data_count; i++) {
    if (!ctx_abstracts_wait_busy(10000)) goto cleanup;  // 10 ms per word
    data[i] = dm_get_data0();
  }

  ret = true;

cleanup:
  // Disable auto-exec after all words are written
  dm_set_abstractauto(0);
  return true;
}

//------------------------------------------------------------------------------

static uint16_t prog_set_block_aligned[] = {
  0x0537, 0xe000,  // lui  a0, DM_DATA1_ADDR[31:12]
  0x2583, 0x0f85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Load dest addr
  0x2503, 0x0f45,  // lw   a0, DM_DATA0_ADDR[11:0](a0)      // Load data
  0xc188,          // sw   a0, 0(a1)                        // Write to memory
  0x0591,          // addi a1, a1, 4                        // Increment addr
  0x0537, 0xe000,  // lui  a0, DM_DATA1_ADDR[31:12]
  0x2c23, 0x0eb5,  // sw   a1, DM_DATA1_ADDR[11:0](a0)      // Update addr
  0x9002,          // ebreak
  0x0001           // nop
};

bool ctx_set_block_aligned(uint32_t addr, uint32_t *data, uint32_t data_count) {
  CHECK((addr & 3) == 0);

  ctx_load_prog((uint32_t*)prog_set_block_aligned,
      sizeof(prog_set_block_aligned) / sizeof(uint32_t), GPRB_A0 | GPRB_A1);

  dm_set_data0(data[0]);
  dm_set_data1(addr);
  if (!ctx_run_prog(10000))  // 10 ms
    return false;

  dm_set_abstractauto(DMAA_AUTOEXECDATA(1));
  bool ret = false;  // from here on, cleanup is required

  // Write remaining words using auto-execution
  for (int i = 1; i < data_count; i++) {
    dm_set_data0(data[i]);
    if (!ctx_abstracts_wait_busy(10000)) goto cleanup;  // 10 ms
  }

  ret = true;

cleanup:
  // Disable auto-exec after all words are written
  dm_set_abstractauto(0);
  return ret;
}

//------------------------------------------------------------------------------

void ctx_dump_block(uint32_t offset, uint32_t base_addr, uint32_t total_size) {
  uint32_t data[DUMP_WORDS];

  // Calculate words
  uint32_t remaining = total_size - offset;
  uint32_t words = remaining < DUMP_SIZE ? remaining / sizeof(uint32_t): DUMP_WORDS;

  uint32_t addr = base_addr + offset;
  ctx_get_block_aligned(addr, data, words);
  print_hex(0, "addr", addr);

  for (uint32_t i = 0; i < words; i++) {
    printf("  %08X", data[i]);
    if ((i & 7) == 7)  // 8 columns
      putchar('\n');
  }
}

//------------------------------------------------------------------------------

void ctx_dump(void) {
  print_y(0, "ctx:dump\n");

  print_b(0, "prog_cache");
  for (int i = 0; i < prog_size; i++) {
    if (i % 4 == 0)
      putchar('\n');
    printf("  %d: %08X", i, prog_cache[i]);
  }

  print_b(0, "\nreg_cache\n");
  for (int i = 0; i < GPR_MAX; i++) {
    printf("  %2d: %08X", i, reg_cache[i]);
    if ((i & 3) == 3)
      putchar('\n');
  }

  print_bits(0, "dirty_regs", dirty_regs, 32);
  print_bits(0, "cached_regs", cached_regs, 32);

  uint32_t data0 = dm_get_data0();
  dm_print(DM_DATA0, data0);

  uint32_t data1 = dm_get_data1();
  dm_print(DM_DATA1, data1);

  dm_abstractauto abstractauto = dm_get_abstractauto();
  dm_abstractauto_dump(abstractauto);

  dm_abstractcs abstractcs = dm_get_abstractcs();
  dm_abstractcs_dump(abstractcs);

  dm_command command = dm_get_command();
  dm_command_dump(command);

  dm_control control = dm_get_control();
  dm_control_dump(control);

  uint32_t haltsum0 = dm_get_haltsum0();
  dm_print(DM_HALTSUM0, haltsum0);

  dm_hartinfo hartinfo = dm_get_hartinfo();
  dm_hartinfo_dump(hartinfo);

  dm_progbuf_dump();
  
  dm_status status = dm_get_status();
  dm_status_dump(status);

  if (!status.b.ALLHALTED) {
    print_y(2, "can't display debug CSRs while target is running\n");
    return;
  }

  csr_dcsr r = csr_get_dcsr();
  csr_dcsr_dump(r);

  uint32_t dpc = csr_get_dpc();
  print_hex(0, "CSR_DPC", dpc);

  uint32_t dscratch0 = csr_get_dscratch0();
  print_hex(0, "CSR_DSCRATCH0", dscratch0);

  uint32_t dscratch1 = csr_get_dscratch1();
  print_hex(0, "CSR_DSCRATCH1", dscratch1);
}

//==============================================================================
// Debug-specific CSRs

void csr_dcsr_dump(csr_dcsr r) {
  print_b(0, "DCSR\n");
  printf("  %08X\n", r.raw);
  printf("  CAUSE:%d  EBREAKM:%d  EBREAKS:%d  EBREAKU:%d  MPRVEN:%d  NMIP:%d\n",
         r.b.CAUSE, r.b.EBREAKM, r.b.EBREAKS, r.b.EBREAKU, r.b.MPRVEN, r.b.NMIP);
  printf("  PRV:%d  STEP:%d  STEPIE:%d  STOPCOUNT:%d  STOPTIME:%d  XDEBUGVER:%d\n",
         r.b.PRV, r.b.STEP, r.b.STEPIE, r.b.STOPCOUNT, r.b.STOPTIME, r.b.XDEBUGVER);
}

//------------------------------------------------------------------------------n
