#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pico/time.h>

#include "context.h"
#include "option.h"
#include "utils.h"
#include "xmodem.h"

//==============================================================================
// GPR cache

static uint32_t gpr_cache[32];
static uint32_t gpr_saved;  // bits are 1 if we modified the reg on device
uint8_t  gpr_max;

//------------------------------------------------------------------------------
// Save registers that are about to be clobbered. Skip registers already saved
// in the cache.

bool gpr_cache_save(uint32_t clobber) {
#if GPR_DUMP
  print_c(0, "GPR: save %08X\n", clobber);
#endif

  uint32_t to_cache = clobber & ~gpr_saved;
  for (size_t i = 0; i < gpr_max; i++) {
    if (!(to_cache & (1u << i)))
      continue;

    if (!gpr_get(i, &gpr_cache[i]))
      return false;

#if GPR_DUMP
    print_c(2, "%s -> %08X\n", gpr_names[i], gpr_cache[i]);
#endif
    gpr_saved |= 1u << i;
  }

  return true;
}

//------------------------------------------------------------------------------

bool gpr_cache_restore(void) {
#if GPR_DUMP
  print_c(0, "GPR: restore %08X\n", gpr_saved);
#endif

  for (size_t i = 0; i < gpr_max; i++) {
    if (!(gpr_saved & (1u << i)))
      continue;

    if (!gpr_set(i, gpr_cache[i]))
      return false;

#if GPR_DUMP
    print_c(2, "%s <- %08X\n", gpr_names[i], gpr_cache[i]);
#endif
    gpr_saved &= ~(1u << i);
  }

  return true;
}

//------------------------------------------------------------------------------

static void gpr_cache_dump(void) {
  print_b(0, "reg cache");

  uint8_t count = 0;
  for (int i = 0; i < gpr_max; i++) {
    if (!(gpr_saved & (1u << i)))
      continue;

    if (!(count % 6))
      putchar('\n');

    printf("  %s: %08X", gpr_names[i], gpr_cache[i]);
    count++;
  }

  if (!count)
    printf(": %d", count);
  putchar('\n');
}

//==============================================================================
// Progbuf cache

static uint32_t prog_cache[DM_PROGBUFMAX];
static uint8_t prog_size;

//------------------------------------------------------------------------------

static inline void prog_cache_init(uint8_t new_size) {
  CHECK(new_size <= DM_PROGBUFMAX)
  prog_size = new_size;

  for (size_t i = 0; i < prog_size; i++)
    prog_cache[i] = 0xDEADBEEF;
}

//------------------------------------------------------------------------------

static void prog_cache_dump(void) {
  print_b(0, "prog cache");

  for (size_t i = 0; i < prog_size; i++) {
    if (!(i % 6))
      putchar('\n');
    printf("  %d: %08X", i, prog_cache[i]);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------

void ctx_load_prog(const uint32_t *prog, uint8_t size) {
  CHECK(size <= prog_size);
  dm_abstractcs_clear_err();

  // Upload any DM_PROGBUF(N) word that changed
  for (size_t i = 0; i < size; i++) {
    if (prog_cache[i] != prog[i]) {
      dm_set_progbuf(i, prog[i]);
      prog_cache[i] = prog[i];
    }
  }
}

//==============================================================================
// Test

static bool test_write_seq(uint32_t addr) {
  // Fill memory with incrementing byte pattern 0x00..0x0F
  for (int i = 0; i < 16; i++) {
    if (!ctx_set_mem_u8(addr + i, i))
      return false;
  }
  return true;
}

//------------------------------------------------------------------------------

static bool test_read_seq(uint32_t addr) {
  // Read memory with incrementing byte pattern 0x00..0x0F
  for (int i = 0; i < 16; i++) {
    uint8_t byte;
    if (!ctx_get_mem_u8(addr + i, &byte))
      return false;
    if (byte != i)
      return false;
  }
  return true;
}

//------------------------------------------------------------------------------


static void test_misaligned_reads(uint32_t base) {
  print_b(0, "misaligned reads\n");

  for (int offset = 0; offset < 4; offset++) {
    bool result = false;
    printf("  offset: %d", offset);

    // Fill memory
    uint32_t addr = base + offset;
    if (!test_write_seq(addr))                    goto result;
    if (!test_read_seq(addr))                     goto result;

    // Read shorts
    for (int i = 0; i < 8; i++) {
      uint16_t word;
      if (!ctx_get_mem_u16(addr + i * 2, &word))  goto result;
      if (word != 0x0100 + i * 0x0202)            goto result;
    }

    // Read words
    for (int i = 0; i < 4; i++) {
      uint32_t dword;
      if (!ctx_get_mem_u32(addr + i * 4, &dword)) goto result;
      if (dword != 0x03020100 + i * 0x04040404)   goto result;
    }

    result = true;

result:
    print_status(result);
  }
}

//------------------------------------------------------------------------------

static void test_misaligned_writes(uint32_t base) {
  print_b(0, "misaligned writes\n");

  for (int offset = 0; offset < 4; offset++) {
    bool result = false;
    printf("  offset: %d", offset);

    // Write byte
    uint32_t addr = base + offset;
    if (!test_write_seq(addr))                                 goto result;
    if (!test_read_seq(addr))                                  goto result;

    // Write shorts
    for (int i = 0; i < 8; i++)
      if (!ctx_set_mem_u16(addr + i * 2, 0x0100 + i * 0x0202)) goto result;
    if (!test_read_seq(addr))                                  goto result;

    // Read words
    for (int i = 0; i < 4; i++)
      if (!ctx_set_mem_u32(addr + i * 4, 0x03020100 + i * 0x04040404)) goto result;
    if (!test_read_seq(addr))                                  goto result;

    result = true;

result:
    print_status(result);
  }
}

//------------------------------------------------------------------------------

static inline bool test_write_const(uint32_t addr, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (!ctx_set_mem_u8(addr + i, 0xAB))
      return false;
  }
  return true;
}

//------------------------------------------------------------------------------

static inline bool test_read_const(uint32_t addr, size_t count) {
  for (size_t i = 0; i < count; i++) {
    uint8_t byte;
    if (!ctx_get_mem_u8(addr + i, &byte))
      return false;
    if (byte != 0xAB)
      return false;
  }
  return true;
}

//------------------------------------------------------------------------------

static void test_aligned_block_reads(uint32_t base) {
  print_b(0, "aligned block reads\n");

  for (int offset = 0; offset < 4; offset++) {
    bool result = false;
    printf("  offset: %d:", offset);

    // Fill memory
    uint32_t addr = base;
    if (!test_write_const(addr, offset))                  goto result;
    addr += offset;
    if (!test_write_seq(addr))                            goto result;
    addr += 16;
    if (!test_write_const(addr, 4 - offset))              goto result;

    // Read blocks
    uint8_t buf[20];
    memset(buf, 0xFF, sizeof(buf));
    if (!ctx_get_block_aligned(base, (uint32_t *)buf, 5)) goto result;

    // Test result
    uint8_t *p = buf;
    for (int i = offset; i > 0; i--)
      if (*p++ != 0xAB)                                   goto result;
    for (int i = 0; i < 16; i++)
      if (*p++ != i)                                      goto result;
    for (int i = offset; i < 4; i++)
      if (*p++ != 0xAB)                                   goto result;

    result = true;

result:
    print_status(result);
  }
}

//------------------------------------------------------------------------------

static void test_aligned_block_writes(uint32_t base) {
  print_b(0, "aligned block writes\n");

  for (int offset = 0; offset < 4; offset++) {
    bool result = false;
    printf("  offset: %d", offset);

    // Fill memory
    uint8_t buf[20];
    uint8_t *p = buf;
    for (int i = offset; i > 0; i--)
      *p++ = 0xAB;
    for (int i = 0; i < 16; i++)
      *p++ = i;
    for (int i = offset; i < 4; i++)
      *p++ = 0xAB;

    if (!ctx_set_block_aligned(base, (uint32_t *)buf, 5))   goto result;

    // Test result
    uint32_t addr = base;
    if (!test_read_const(addr, offset))                     goto result;
    addr += offset;
    if (!test_read_seq(addr))                               goto result;
    addr += 16;
    if (!test_read_const(addr, 4 - offset))                 goto result;

    result = true;

result:
    print_status(result);
  }
}

//------------------------------------------------------------------------------

static void test_block(uint32_t addr, bool expected) {
  printf("  block: %08X, expected: %d", addr, expected);

  uint32_t block[4] = { 0x88AACCEE, 0x88AACCEE, 0x88AACCEE, 0x88AACCEE };
  if (ctx_set_block_aligned(addr, block, count_of(block)))
    print_status(true);
}

//------------------------------------------------------------------------------

static void test_addr(uint32_t addr, bool expected) {
  printf("  addr: %08X, expected: %d", addr, expected);

  uint32_t data;
  if (ctx_get_mem_u32_aligned(addr, &data))
    print_status(true);
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

static void test_calc_crc(const uint8_t *data, uint32_t size, uint16_t expected) {
  uint16_t crc = xmodem_crc_calc(data, size);

  printf("  size: %d  crc: %04X", size, crc);
  if (crc == expected)
    print_g(1, "ok\n");
  else
    print_r(1, "wrong (expected %04X)\n", expected);
}

//------------------------------------------------------------------------------

static void test_crc(void) {
  print_b(0, "testing CRC-16 via DMA\n");

  for (int i = 0; i < 4; i++)
    test_calc_crc(crc_data2, count_of(crc_data2), 0x546C);

  test_calc_crc(crc_data, 57, 0x2545);
  test_calc_crc(crc_data, 128, 0x95EB);

  for (int i = 0; i < 4; i++)
    test_calc_crc(crc_data, count_of(crc_data), 0x978C);
}

//------------------------------------------------------------------------------

void ctx_test(void) {
  print_y(0, "debug:test\n");

  if (!ctx_halted_err("test"))
    return;

  uint32_t addr = 0x20000400;
  test_misaligned_reads(addr);
  test_misaligned_writes(addr);
  test_aligned_block_reads(addr);
  test_aligned_block_writes(addr);

  // Test block writes at both ends of memory
  print_b(0, "block writes at both ends of memory\n");
  test_block(0x20000000, true);
  test_block(0x20000800 - 16, true);
  test_block(0x20000000 - 4, false);   // Off the low end by 4
  test_block(0x20000800 - 12, false);  // Off the high end by 4

  print_b(0, "address reads at both ends of memory\n");
  test_addr(OPTB_ADDR, true);
  test_addr(OPTB_ADDR + OPTB_SIZE - 4, true);
  test_addr(OPTB_ADDR + OPTB_SIZE, false);

  // Test CRC-16 calculation using DMA sniffer
  test_crc();
}

//==============================================================================
// API

void ctx_init(void) {
  // Prog buf(N)
  dm_abstractcs abstractcs = dm_get_abstractcs();
  prog_cache_init(abstractcs.b.PROGBUFSIZE);

  // GPRs
  gpr_max = 16;
  gpr_saved = 0;
}

//------------------------------------------------------------------------------

void ctx_dump_block(uint32_t offset, uint32_t base, uint32_t size) {
  uint32_t data[DUMP_WORDS];

  // Calculate words
  uint32_t remaining = size - offset;
  uint32_t words = remaining < DUMP_WORDS * 4 ? remaining / 4 : DUMP_WORDS;

  uint32_t addr = base + offset;
  if (!ctx_get_block_aligned(addr, data, words))
    return;

  print_hex(0, "addr", addr);
  printf("%*s", 2, "");
  uint32_t col = addr & 0xF;
  for (size_t i = 0; i < 8; i++) {
    print_m(7, "+%02X", col);
    col += 4;
  }

  uint32_t row = (addr & 0xFFF0) >> 4;
  for (size_t i = 0; i < words; i++) {
    if (!(i & 7)) {  // 8 columns
      putchar('\n');
      print_m(1, "%03Xx", row);
      row += 2;
      row &= 0xFFFF;
    }
    printf("  %08X", data[i]);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------

void ctx_dump(void) {
  print_y(0, "debug:info\n");

  gpr_cache_dump();
  prog_cache_dump();

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

  dm_status status = dm_get_status();
  dm_status_dump(status);

  if (!haltsum0) {
    ctx_halted_err("display debug registers");
    return;
  }
 
  dm_progbuf_dump(prog_size);
  dm_data_dump(hartinfo.b.DATASIZE);

  csr_dmcu_cr dmcu_cr;
  if (csr_get_dmcu_cr(&dmcu_cr))
    csr_dmcu_cr_dump(dmcu_cr);

  csr_dcsr dcsr;
  if (csr_get_dcsr(&dcsr))
    csr_dcsr_dump(dcsr);

  uint32_t dpc;
  if (csr_get_dpc(&dpc))
    print_hex(0, "DPC", dpc);

  csr_dscratch_dump(hartinfo.b.NSCRATCH);
}

//------------------------------------------------------------------------------

inline bool ctx_halted_err(const char *msg) {
  if (!dm_get_haltsum0()) {
    print_r(2, "can't %s while target is running\n", msg);
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

bool ctx_halt(void) {
  if (!swio_halt())
    return false;

  csr_misa misa;
  if (!csr_get_misa(&misa))
    return false;

  gpr_max = csr_misa_rv(misa);

  // Turn on debug breakpoints & stop counters/timers during debug
  csr_dcsr dcsr;
  if (!csr_get_dcsr(&dcsr))
    return false;

  return csr_set_dcsr(dcsr.raw | DCSR_STOPTIME | DCSR_EBREAKM);
}

//------------------------------------------------------------------------------

bool ctx_resume(bool step) {
  csr_dcsr dcsr;
  if (!csr_get_dcsr(&dcsr))
    return false;

  dcsr.b.STEP = step;
  if (!csr_set_dcsr(dcsr.raw))
    return false;

  if (!gpr_cache_restore())
    return false;

  if (step)
    return swio_step();
  return swio_resume();
}

//------------------------------------------------------------------------------

inline bool ctx_reset(void) {
  if (!swio_reset())
    return false;

  // Reset cached state
  ctx_init();
  return true;
}

//==============================================================================
// Progbuf

//------------------------------------------------------------------------------
// NOTE: We can NOT save registers here, as doing so would clobber DATA0 which
// may be loaded with something the program needs.

inline bool ctx_prog_exec(const char *name) {
#if PROG_DUMP
  print_c(0,"PROG: exec %s\n", name);
#endif

  // Execute the command in progbuf
  dm_set_command(DMCM_POSTEXEC);

  // Wait for device to become ready
  return dm_abstractcs_wait();
}

//==============================================================================
// Register access

bool ctx_read_reg(uint16_t regno, uint32_t *value) {
  dm_abstractcs_clear_err();

  // Set abstract command to copy register data to Data0 register
  dm_set_command(regno | DMCM_TRANSFER | DMCM_AARSIZE32);

  // Wait for device to become ready
  if (!dm_abstractcs_wait())
    return false;

  *value = dm_get_data0();
  return true;
}

//------------------------------------------------------------------------------

bool ctx_write_reg(uint16_t regno, uint32_t value) {
  dm_abstractcs_clear_err();

  // Write the data to be written, wdata, to Data0
  dm_set_data0(value);

  // Set abstract command to copy Data0 data to x6 register
  dm_set_command(regno | DMCM_WRITE | DMCM_TRANSFER | DMCM_AARSIZE32);

  // Wait for device to become ready
  return dm_abstractcs_wait();
}

//==============================================================================
// Memory access - GET

bool ctx_get_mem_u32(uint32_t addr, uint32_t* data) {
  uint32_t data_lo;
  uint32_t addr_lo = addr & ~3;
  if (!ctx_get_mem_u32_aligned(addr_lo, &data_lo))
    return false;

  uint8_t offset = addr & 3;
  if (offset == 0) {
    *data = data_lo;
    return true;
  }

  uint32_t data_hi;
  if (!ctx_get_mem_u32_aligned(addr_lo + 4, &data_hi))
    return false;

  offset *= 8;
  data_lo >>= offset;
  data_hi <<= 32 - offset;

  *data = data_lo | data_hi;
  return true;
}

//------------------------------------------------------------------------------

bool ctx_get_mem_u16(uint32_t addr, uint16_t *data) {
  uint32_t data_lo;
  uint32_t addr_lo = addr & ~3;
  if (!ctx_get_mem_u32_aligned(addr_lo, &data_lo))
    return false;

  uint8_t offset = addr & 3;
  if (offset < 3) {
    offset *= 8;
    data_lo >>= offset;

    *data = data_lo;
    return true;
  }

  data_lo >>= 24;

  uint32_t data_hi;
  if (!ctx_get_mem_u32_aligned(addr_lo + 4, &data_hi))
    return false;

  data_hi <<= 8;

  *data = data_lo | data_hi;
  return true;
}

//------------------------------------------------------------------------------

bool ctx_get_mem_u8(uint32_t addr, uint8_t *data) {
  uint32_t data_lo;
  if (!ctx_get_mem_u32_aligned(addr & ~3, &data_lo))
    return false;

  uint8_t offset = (addr & 3) * 8;
  *data = data_lo >> offset;
  return true;
}

//==============================================================================
// Memory access - SET

bool ctx_set_mem_u32(uint32_t addr, uint32_t data) {
  uint8_t offset = addr & 3;
  if (offset == 0)
    return ctx_set_mem_u32_aligned(addr, data);

  uint32_t data_lo;
  uint32_t addr_lo = addr & ~3;
  if (!ctx_get_mem_u32_aligned(addr_lo, &data_lo))
    return false;

  uint32_t data_hi;
  uint32_t addr_hi = addr_lo + 4;
  if (!ctx_get_mem_u32_aligned(addr_hi, &data_hi))
    return false;

  switch (offset) {
    case 1:
      data_lo &= 0x000000FF;
      data_lo |= data << 8;
      data_hi &= 0xFFFFFF00;
      data_hi |= data >> 24;  // 8+24=32
      break;
    case 2:
      data_lo &= 0x0000FFFF;
      data_lo |= data << 16;
      data_hi &= 0xFFFF0000;
      data_hi |= data >> 16;  // 16+16=32
      break;
    default:
      data_lo &= 0x00FFFFFF;
      data_lo |= data << 24;
      data_hi &= 0xFF000000;
      data_hi |= data >> 8;   // 24+8=32
  }

  return ctx_set_mem_u32_aligned(addr_lo, data_lo) &&
         ctx_set_mem_u32_aligned(addr_hi, data_hi);
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u16(uint32_t addr, uint16_t data) {
  uint32_t data_lo;
  uint32_t addr_lo = addr & ~3;
  if (!ctx_get_mem_u32_aligned(addr_lo, &data_lo))
    return false;

  uint32_t offset = addr & 3;
  switch (offset) {
    case 0:
      data_lo &= 0xFFFF0000;
      data_lo |= data;
      break;
    case 1:
      data_lo &= 0xFF0000FF;
      data_lo |= (uint32_t)data << 8;
      break;
    case 2:
      data_lo &= 0x0000FFFF;
      data_lo |= (uint32_t)data << 16;
      break;
    default:
      data_lo &= 0x00FFFFFF;
      data_lo |= (uint32_t)data << 24;
  }

  if (!ctx_set_mem_u32_aligned(addr_lo, data_lo))
    return false;

  if (offset != 3)
    return true;

  uint32_t data_hi;
  uint32_t addr_hi = addr_lo + 4;
  if (!ctx_get_mem_u32_aligned(addr_hi, &data_hi))
    return false;

  data_hi &= 0xFFFFFF00;
  data_hi |= HIBYTE(data);
  return ctx_set_mem_u32_aligned(addr_hi, data_hi);
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u8(uint32_t addr, uint8_t data) {
  uint32_t data_lo;
  uint32_t addr_lo = addr & ~3;
  if (!ctx_get_mem_u32_aligned(addr_lo, &data_lo))
    return false;

  switch (addr & 3) {
    case 0:
      data_lo &= 0xFFFFFF00;
      data_lo |= data;
      break;
    case 1:
      data_lo &= 0xFFFF00FF;
      data_lo |= (uint32_t)data << 8;
      break;
    case 2:
      data_lo &= 0xFF00FFFF;
      data_lo |= (uint32_t)data << 16;
      break;
    default:
      data_lo &= 0x00FFFFFF;
      data_lo |= (uint32_t)data << 24;
  }

  return ctx_set_mem_u32_aligned(addr_lo, data_lo);
}

//------------------------------------------------------------------------------

static uint16_t prog_get_set_u32[] = {
  0x0537, 0xE000,  // lui  a0, DM_DATA_BASE[31:12]
  0x0513, 0x0F45,  // addi a0, a0, DM_DATA_ADDR[11:0]
  0x414C,          // lw   a1, 4(a0)                  // Load addr from DATA1
  0x8985,          // andi a1, a1, 1                  // Check LSB (set/get flag)
  0xC591,          // beqz a1, get_u32                // If 0 -> read
                // set_u32:
  0x414C,          // lw   a1, 4(a0)                  // Reload addr from DATA1
  0x15FD,          // addi a1, a1, -1                 // Clear LSB flag
  0x4108,          // lw   a0, 0(a0)                  // Load data from DATA0
  0xC188,          // sw   a0, 0(a1)                  // Write to memory
  0x9002,          // ebreak
                // get_u32:
  0x414C,          // lw   a1, 4(a0)                  // Load addr from DATA1
  0x418C,          // lw   a1, 0(a1)                  // Read from memory
  0xC10C,          // sw   a1, 0(a0)                  // Write to DATA0
  0x9002           // ebreak
};

//------------------------------------------------------------------------------

bool ctx_get_mem_u32_aligned(uint32_t addr, uint32_t *data) {
#if PROG_DUMP
  print_c("get32: %08X\n", addr);
#endif

  ctx_load_prog((uint32_t *)prog_get_set_u32, sizeof(prog_get_set_u32) / 4);
  if (!gpr_cache_save(GPRB(A0) | GPRB(A1)))
    return false;

  // Set LSB to 0 to indicate read operation
  dm_set_data1(addr);
  if (!ctx_prog_exec("getw"))
    return false;

  *data = dm_get_data0();
  return true;
}

//------------------------------------------------------------------------------

bool ctx_set_mem_u32_aligned(uint32_t addr, uint32_t data) {
#if PROG_DUMP
  print_c("set32: %08X\n", addr);
#endif

  ctx_load_prog((uint32_t *)prog_get_set_u32, sizeof(prog_get_set_u32) / 4);
  if (!gpr_cache_save(GPRB(A0) | GPRB(A1)))
    return false;

  // Set LSB to 1 to indicate write operation
  dm_set_data1(addr | 1);
  dm_set_data0(data);

  return ctx_prog_exec("setw");
}

//==============================================================================
// Memory access - block

static uint16_t prog_get_block_aligned[] = {
  0x0537, 0xE000,  // lui  a0, DM_DATA_BASE[31:12]
  0x2583, 0x0F85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Load src addr
  0x418C,          // lw   a1, 0(a1)                        // Read from memory
  0x2A23, 0x0EB5,  // sw   a1, DM_DATA0_ADDR[11:0](a0)      // Write to DATA0
  0x2583, 0x0F85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Reload addr
  0x0591,          // addi a1, a1, 4                        // Increment addr
  0x2C23, 0x0EB5,  // sw   a1, DM_DATA1_ADDR[11:0](a0)      // Update addr
  0x9002,          // ebreak
  0x0001           // nop
};

bool ctx_get_block_aligned(uint32_t addr, uint32_t *data, size_t count) {
  CHECK(!(addr & 3));

  ctx_load_prog((uint32_t *)prog_get_block_aligned, sizeof(prog_get_block_aligned) / 4);
  if (!gpr_cache_save(GPRB(A0) | GPRB(A1)))
    return false;

  dm_set_data1(addr);
  if (!ctx_prog_exec("getblk"))  // 10 ms
    return false;

  // Read words using auto-execution
  dm_set_abstractauto(DM_AUTOEXECDATA(1));
  bool ret = false;

  while (count > 1) {
    *data++ = dm_get_data0();
    count--;

    if (!dm_abstractcs_wait())
      goto cleanup;
  }

  // Success
  ret = true;

cleanup:
  // Disable auto-execution before reading the last word
  dm_set_abstractauto(0);

  if (!ret)
    return false;

  *data = dm_get_data0();
  return true;
}

//------------------------------------------------------------------------------

static uint16_t prog_set_block_aligned[] = {
  0x0537, 0xE000,  // lui  a0, DM_DATA_BASE[31:12]
  0x2583, 0x0F85,  // lw   a1, DM_DATA1_ADDR[11:0](a0)      // Load dest addr
  0x2503, 0x0F45,  // lw   a0, DM_DATA0_ADDR[11:0](a0)      // Load data
  0xC188,          // sw   a0, 0(a1)                        // Write to memory
  0x0591,          // addi a1, a1, 4                        // Increment addr
  0x0537, 0xE000,  // lui  a0, DM_DATA1_ADDR[31:12]
  0x2C23, 0x0EB5,  // sw   a1, DM_DATA1_ADDR[11:0](a0)      // Update addr
  0x9002,          // ebreak
  0x0001           // nop
};

bool ctx_set_block_aligned(uint32_t addr, uint32_t *data, size_t count) {
  CHECK(!(addr & 3));

  ctx_load_prog((uint32_t *)prog_set_block_aligned, sizeof(prog_set_block_aligned) / 4);
  if (!gpr_cache_save(GPRB(A0) | GPRB(A1)))
    return false;

  dm_set_data1(addr);

  // Write words using auto-execution
  dm_set_abstractauto(DM_AUTOEXECDATA(1));
  bool ret = false;

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
  return ret;
}

//==============================================================================
// RISC-V-specific CSRs

static void csr_prv_dump(uint8_t prv) {
  const char *desc;

  switch (prv) {
    case DMPM_USER:       desc = "user";       break;
    case DMPM_SUPERVISOR: desc = "supervisor"; break;
    case DMPM_MACHINE:    desc = "machine";    break;
    default:              desc = "?";
  }

  print_str(2, "privilege mode", desc);
}

//==============================================================================

void csr_mstatus_dump(csr_mstatus r) {
  print_hex(0, "MSTATUS", r.raw);
  csr_prv_dump(r.b.MPP);
  printf("  MIE:%d  MPIE:%d  MPOP:%d  MPPOP:%d\n",
         r.b.MIE, r.b.MPIE, r.b.MPOP, r.b.MPPOP);
}

//==============================================================================

static uint8_t misa_xl_to_bits(misa_xl mxl) {
  switch (mxl) {
    case MXL32:  return 32;
    case MXL64:  return 64;
    case MXL128: return 128;
  }
  return 0;
}

//------------------------------------------------------------------------------

static void csr_misa_mxl_dump(misa_xl mxl) {
  print_b(2, "machine word length");
  uint8_t bits = misa_xl_to_bits(mxl);
  printf(": %d-bit\n", bits);
}

//------------------------------------------------------------------------------

void csr_misa_dump(csr_misa r) {
  print_hex(0, "MISA", r.raw);
  csr_misa_mxl_dump(r.b.MXL);

  if (r.b.A) print_str(2, "A", "atomic extension");
  if (r.b.C) print_str(2, "C", "compressed extension");
  if (r.b.D) print_str(2, "D", "double-precision floating-point extension");
  if (r.b.E) print_str(2, "E", "base instruction set with 16 registers");
  if (r.b.F) print_str(2, "F", "single-precision floating-point extension");
  if (r.b.G) print_str(2, "G", "additional standard extensions present");
  if (r.b.H) print_str(2, "H", "hypervisor extension");
  if (r.b.I) print_str(2, "I", "standard instruction set with 32 registers");
  if (r.b.M) print_str(2, "M", "integer multiply/divide extension");
  if (r.b.N) print_str(2, "N", "user-level interrupts supported");
  if (r.b.S) print_str(2, "S", "supervisor mode implemented");
  if (r.b.U) print_str(2, "U", "user mode implemented");
  if (r.b.X) print_str(2, "X", "non-standard extensions present");
}

//==============================================================================

void csr_mtvec_dump(csr_mtvec r) {
  print_hex(0, "MTVEC", r.raw);
  printf("  ADDR:%08X  ABS:%d  IVT:%d\n", r.b.BASE, r.b.ABS, r.b.IVT);
}

//==============================================================================

static void csr_mcause_irq_dump(int code) {
  const char *desc;

  switch (code) {
    case 2:  desc = "non maskable";         break;
    case 3:  desc = "exception";            break;
    case 12: desc = "system timer";         break;
    case 13: desc = "debug trap";           break;
    case 14: desc = "software";             break;
    case 16: desc = "window watchdog";      break;
    case 17: desc = "exti line detection";  break;
    case 18: desc = "flash";                break;
    case 19: desc = "rcc";                  break;
    case 20: desc = "external line";        break;
    case 21: desc = "awu";                  break;
    case 22: desc = "dma channel 1";        break;
    case 23: desc = "dma channel 2";        break;
    case 24: desc = "dma channel 3";        break;
    case 25: desc = "dma channel 4";        break;
    case 26: desc = "dma channel 5";        break;
    case 27: desc = "dma channel 6";        break;
    case 28: desc = "dma channel 7";        break;
    case 29: desc = "adc";                  break;
    case 30: desc = "i2c event";            break;
    case 31: desc = "i2c error";            break;
    case 32: desc = "uart";                 break;
    case 33: desc = "spi";                  break;
    case 34: desc = "tim1 break";           break;
    case 35: desc = "tim1 update";          break;
    case 36: desc = "tim1 trigger";         break;
    case 37: desc = "tim1 cc";              break;
    case 38: desc = "tim2";                 break;
    default: desc = "?";
  }

  print_b(2, "interrupt");
  putchar(':');
  print_g(1, "%s\n", desc);
}

//------------------------------------------------------------------------------

static void csr_mcause_except_dump(int code) {
  const char *desc;

  switch (code) {
    case 0:  desc = "instruction address misalignment"; break;
    case 1:  desc = "instruction access fault";         break;
    case 2:  desc = "illegal instruction";              break;
    case 3:  desc = "breakpoint";                       break;
    case 4:  desc = "load address misalignment";        break;
    case 5:  desc = "load access fault";                break;
    case 6:  desc = "store address misalignment";       break;
    case 7:  desc = "store access fault";               break;
    case 8:  desc = "ecall in User mode";               break;
    case 11: desc = "ecall in Machine mode";            break;
    default: desc = "?";
  }

  print_b(2, "exception");
  putchar(':');
  print_r(1, "%s\n", desc);
}

//------------------------------------------------------------------------------

void csr_mcause_dump(csr_mcause r) {
  print_hex(0, "MCAUSE", r.raw);
  if (r.b.IRQ)
    csr_mcause_irq_dump(r.b.CODE);
  else
    csr_mcause_except_dump(r.b.CODE);
}

//==============================================================================

void csr_mvendorid_dump(csr_mvendorid r) {
  print_hex(0, "MVENDORID", r.raw);
  printf("  ID:%X  BANK:%X\n", r.b.JEDEC_ID, r.b.JEDEC_BANK);
}

//==============================================================================

static void csr_vendor_dump(uint8_t vnd0, uint8_t vnd1, uint8_t vnd2) {
  printf("  VENDOR0:%c  VENDOR1:%c  VENDOR2:%c\n", vnd0 + '@', vnd1 + '@', vnd2 + '@');
}

//------------------------------------------------------------------------------

void csr_marchid_dump(csr_marchid r) {
  print_hex(0, "MARCHID", r.raw);
  printf("  ARCH:%d  SERIAL:%d  VERSION:%c\n", r.b.ARCH, r.b.SERIAL, r.b.VERSION + '@');
  csr_vendor_dump(r.b.VENDOR0, r.b.VENDOR1, r.b.VENDOR2);
}

//==============================================================================

void csr_mimpid_dump(csr_mimpid r) {
  print_hex(0, "MIMPID", r.raw);
  csr_vendor_dump(r.b.VENDOR0, r.b.VENDOR1, r.b.VENDOR2);
}

//------------------------------------------------------------------------------

void csr_intsyscr_dump(csr_intsyscr r) {
  print_hex(0, "INTSYSCR", r.raw);
  printf("  HWSTK:%d  EABI:%d  INEST:%d\n", r.b.HWSTK, r.b.EABI, r.b.INEST);
}

//==============================================================================
// Debug-specific CSRs

const char *csr_name(uint16_t csr) {
  switch (csr) {
    case CSR_MSTATUS:   return "MSTATUS";
    case CSR_MISA:      return "MISA";
    case CSR_MTVEC:     return "MTVEC";
    case CSR_MSCRATCH:  return "MSCRATCH";
    case CSR_MEPC:      return "MEPC";
    case CSR_MCAUSE:    return "MCAUSE";
    case CSR_DCSR:      return "DCSR";
    case CSR_DPC:       return "DPC";
    case CSR_DMCU_CR:   return "DMCU_CR";
    case CSR_INTSYSCR:  return "INTSYSCR";
    case CSR_MVENDORID: return "MVENDORID";
    case CSR_MARCHID:   return "MARCHID";
    case CSR_MIMPID:    return "MIMPID";
  }

  // DSCRATCH
  if (csr >= CSR_DSCRATCH0 && csr < CSR_DMCU_CR) {
    static char dscratch[] = "DSCRATCH  ";
    decxx_str(dscratch + 8, csr - CSR_DSCRATCH0);
    return dscratch;
  }

  // Unknown
  return "?";
}

//------------------------------------------------------------------------------n

static void csr_dcsr_cause_dump(dcsr_cause_t cause) {
  const char *desc;
 
  switch (cause) {
    case CAUSE_EBREAK:  desc = "ebreak";       break;
    case CAUSE_TRIGGER: desc = "trigger";      break;
    case CAUSE_HALTREQ: desc = "halt request"; break;
    case CAUSE_STEP:    desc = "single step";  break;
    case CAUSE_RESET:   desc = "reset";        break;
    default: desc = "?";
  }

  print_b(2, "cause");
  putchar(':');
  print_g(1, "%s\n", desc);
}

//------------------------------------------------------------------------------n

void csr_dcsr_dump(csr_dcsr r) {
  print_hex(0, "DCSR", r.raw);
  csr_dcsr_cause_dump(r.b.CAUSE);
  csr_prv_dump(r.b.PRV);
  printf("  EBREAKM:%d  EBREAKU:%d  STEP:%d  STEPIE:%d  STOPTIME:%d  XDEBUGVER:%d\n",
         r.b.EBREAKM, r.b.EBREAKU, r.b.STEP, r.b.STEPIE, r.b.STOPTIME, r.b.XDEBUGVER);
}

//==============================================================================

void csr_dscratch_dump(size_t count) {
  print_b(0, "DSCRATCH");

  for (size_t i = 0; i < count; i++) {
    if (!(i % 6))
      putchar('\n');

    uint32_t dscratch;
    if (csr_get_dscratch(i, &dscratch))
      printf("  %d: %08X", i, dscratch);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------n

void csr_dmcu_cr_dump(csr_dmcu_cr r) {
  print_hex(0, "DMCU_CR", r.raw);
  printf("  SLEEP:%d  STANDBY:%d  IWDG_STOP:%d  TIM1_STOP:%d  TIM2_STOP:%d  WWDG_STOP:%d\n",
         r.b.SLEEP, r.b.STANDBY, r.b.IWDG_STOP, r.b.TIM1_STOP, r.b.TIM2_STOP, r.b.WWDG_STOP);
}

//------------------------------------------------------------------------------n

void csr_dump(void) {
  print_y(0, "csr:dump\n");

  if (!ctx_halted_err("display CSRs"))
    return;

  csr_mvendorid mvendorid;
  if (csr_get_mvendorid(&mvendorid))
    csr_mvendorid_dump(mvendorid);

  csr_marchid marchid;
  if (csr_get_marchid(&marchid))
    csr_marchid_dump(marchid);

  csr_mimpid mimpid;
  if (csr_get_mimpid(&mimpid))
    csr_mimpid_dump(mimpid);

  csr_misa misa;
  if (csr_get_misa(&misa))
    csr_misa_dump(misa);

  csr_mstatus mstatus;
  if (csr_get_mstatus(&mstatus))
    csr_mstatus_dump(mstatus);

  csr_mtvec mtvec;
  if (csr_get_mtvec(&mtvec))
    csr_mtvec_dump(mtvec);

  uint32_t mepc;
  if (csr_get_mepc(&mepc))
    print_hex(0, "MEPC", mepc);

  csr_mcause mcause;
  if (csr_get_mcause(&mcause))
    csr_mcause_dump(mcause);

  uint32_t mscratch;
  if (csr_get_mscratch(&mscratch))
    print_hex(0, "MSCRATCH", mscratch);

  csr_intsyscr intsyscr;
  if (csr_get_intsyscr(&intsyscr))
    csr_intsyscr_dump(intsyscr);
}

//==============================================================================
// General-Purpose Register

const char *gpr_names[32] = { "ze", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4",
  "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

//------------------------------------------------------------------------------n

static void gpr_dump_regs(const char *desc, uint32_t mask, uint8_t mod) {
  print_b(0, desc);
  print_b(0, " regs");

  size_t count = 0;
  for (size_t i = 0; i < gpr_max; i++) {
    if (!(mask & (1u << i)))
      continue;

    if (!(count % mod))
      putchar('\n');

    printf("  %c%d:", desc[0], count);
    count++;

    uint32_t gpr;
    if (!gpr_get(i, &gpr))
      continue;

    if (gpr_saved & (1u << i))
      print_m(1, "%08X", gpr);
    else
      printf(" %08X", gpr);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------n

void gpr_dump(void) {
  print_y(0, "gpr:dump\n");

  if (!ctx_halted_err("display GPRs"))
    return;

  uint32_t dpc;
  if (csr_get_dpc(&dpc))
    print_hex(0, "pc", dpc);

  uint32_t ra, sp, gp, tp;
  if (gpr_get_ra(&ra) && gpr_get_sp(&sp) && gpr_get_gp(&gp) && gpr_get_tp(&tp)) {
    print_b(0, "pointers\n");
    printf("  ret:%08X  stack:%08X  global:%08X  thread:%08X\n", ra, sp, gp, tp);
  }

  gpr_dump_regs("temp", GPRB_T, 4);
  gpr_dump_regs("saved", GPRB_S, 6);
  gpr_dump_regs("arg",  GPRB_A, gpr_max == 32 ? 4 : 6);
}

//------------------------------------------------------------------------------n
