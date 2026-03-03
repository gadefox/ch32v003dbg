#include <stdio.h>
#include <hardware/clocks.h>
#include <hardware/sync.h>
#include <pico/time.h>

#include "out/swio.pio.h"
#include "context.h"
#include "utils.h"
#include "vendor.h"

//------------------------------------------------------------------------------

#define PICO_SWIO_PIN  27

//------------------------------------------------------------------------------

static PIO pio;
static uint sm, offset;

//------------------------------------------------------------------------------

bool swio_init(void) {
  // Find next free PIO block
  if (!pio_claim_free_sm_and_add_program(&singlewire_program, &pio, &sm, &offset)) {
    print_r(0, "swio: failed to claim a free PIO state machine\n");
    return false;
  }

  // Get program-specific default config
  pio_sm_config c = singlewire_program_get_default_config(offset);
  sm_config_set_set_pins(&c, PICO_SWIO_PIN, 1);
  sm_config_set_out_pins(&c, PICO_SWIO_PIN, 1);
  sm_config_set_in_pins(&c, PICO_SWIO_PIN);
  sm_config_set_sideset_pins(&c, PICO_SWIO_PIN);
  sm_config_set_sideset(&c, 1, false, true);

  // Configure pin behaviors
  sm_config_set_out_shift(&c, false, false, 32);  // MSB first
  sm_config_set_in_shift(&c, false, true, 32);    // MSB first 

  // Set clock divider (100ns period = 10MHz)
  float div = clock_get_hz(clk_sys) / 10000000.0f;
  sm_config_set_clkdiv(&c, div);

  // Initialize state machine
  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_pins(pio, sm, 0);

  // Set GPIO drive characteristics
  gpio_set_drive_strength(PICO_SWIO_PIN, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate(PICO_SWIO_PIN, GPIO_SLEW_RATE_SLOW);

  // Start state machine
  pio_sm_set_enabled(pio, sm, true);

  return true;
}

//------------------------------------------------------------------------------

static inline void swio_pulse(void) {
  // Reinitialize the GPIO pin: set the pin to SIO function, input direction,
  // and explicitly clears the output latch to 0 (LOW).
  gpio_init(PICO_SWIO_PIN);

  // Save current interrupt state and disable all interrupts.
  // This guarantees that the timing below is not disturbed by IRQs.
  uint32_t state = save_and_disable_interrupts();

  // Switch the pin to output mode. Because the output latch is already 0,
  // the pin immediately drives LOW with no glitch or jitter.
  gpio_set_dir(PICO_SWIO_PIN, GPIO_OUT);

  // Hold the pin LOW for a precise number of CPU cycles.
  // 500 cycles correspond to ~8 microseconds at 125 MHz.
  delay_cycles(500);

  // Set the GPIO pin to input mode. This disables the output driver.
  gpio_set_dir(PICO_SWIO_PIN, GPIO_IN);

  // Restore interrupt state and enable all interrupts.
  restore_interrupts(state);

  // Reconfigure the given pin for use by the specified PIO instance.
  pio_gpio_init(pio, PICO_SWIO_PIN);
}

//------------------------------------------------------------------------------

static inline void swio_config(void) {
  // Enable the shadow configuration register output to position 1
  dm_set_shdwcfgr(DMCF_OUTEN | DMCF_KEY(0x5AA5));

  // Update the shadow configuration register output enable bit to the
  // CFGR, and leave the other bits of the configuration register unchanged
  dm_set_cfgr(DMCF_OUTEN | DMCF_KEY(0x5AA5));
}

//------------------------------------------------------------------------------

static inline bool swio_verify(void) {
  // Verify SWIO link by reading CPBR
  dm_cpbr cpbr = dm_get_cpbr();
  if (cpbr.raw != (DMCP_TDIV | DMCP_OUTSTA | DMCP_VERSION(1))) {
    LOG_R("swio: failed (%08X)\n", cpbr.raw);
    return false;
  }

  // Verify number of prog registers
  dm_abstractcs abstractcs = dm_get_abstractcs();
  if (abstractcs.b.PROGBUFSIZE < 8) {
    LOG_R("swio: target has only %d prog regs\n", abstractcs.b.PROGBUFSIZE);
    return false;
  }

  // Verify number of data registers
  if (abstractcs.b.DATACOUNT < 2) {
    LOG_R("swio: target status bityhas only %d data regs\n", abstractcs.b.DATACOUNT);
    return false;
  }

  // Verify data0 offset address
  dm_hartinfo hartinfo = dm_get_hartinfo();
  if (!hartinfo.b.DATAACCESS || hartinfo.b.DATAADDR != DM_DATA_OFFSET) {
    LOG_R("swio: wrong chip (%08X)\n", hartinfo.raw);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------

bool swio_reset(void) {
  swio_pulse();
  swio_config();

  if (!swio_verify())
    return false;

  // Reset debug module on target
  dm_set_control(DMC_ACTIVE);

  // Initiate a core reset request
  dm_set_control(DMC_ACTIVE | DMC_NDMRESET);
  if (!dm_status_wait(DMS_ANYHAVERESET | DMS_ALLHAVERESET, DMS_ANYHAVERESET | DMS_ALLHAVERESET))
    return false;

  // Clear the reset signal
  dm_set_control(DMC_ACTIVE);

  // A 260+ µs delay is required here to ensure the HAVERESET status is cleared
  sleep_us(400);

  // Clear the reset status signal
  dm_set_control(DMC_ACTIVE | DMC_ACKHAVERESET);
  return dm_status_wait(DMS_ANYHAVERESET | DMS_ALLHAVERESET, 0);
}

//------------------------------------------------------------------------------

inline bool swio_halt(void) {
  dm_set_control(DMC_ACTIVE | DMC_HALTREQ);
  if (!dm_status_wait(DMS_ANYHALTED | DMS_ALLHALTED, DMS_ANYHALTED | DMS_ALLHALTED))
    return false;

  dm_set_control(DMC_ACTIVE);
  return true;
}

//------------------------------------------------------------------------------
// NOTE: DMC_RESUMEREQ is cleared automatically once the MCU resumes execution

inline bool swio_resume(void) {
  dm_set_control(DMC_ACTIVE | DMC_RESUMEREQ);
  return dm_status_wait(DMS_ANYRUNNING | DMS_ALLRUNNING, DMS_ANYRUNNING | DMS_ALLRUNNING);
}

//------------------------------------------------------------------------------

inline bool swio_step(void) {
  dm_set_control(DMC_ACTIVE | DMC_RESUMEREQ);
  return dm_status_wait(DMS_ANYRESUMEACK | DMS_ALLRESUMEACK, DMS_ANYRESUMEACK | DMS_ALLRESUMEACK);
}

//==============================================================================
// Transfer data

uint32_t swio_get(uint8_t addr) {
  pio_sm_put_blocking(pio, sm, addr | 1);
  uint32_t data = pio_sm_get_blocking(pio, sm);

#if SWIO_DUMP
  const char *name = dm_str(addr);
  print_c(0, "%s -> %08X\n", name, data);
#endif
  return data;
}

//------------------------------------------------------------------------------

void swio_put(uint8_t addr, uint32_t data) {
#if SWIO_DUMP
  const char *name = dm_str(addr);
  print_c(0, "%s <- %08X\n", name, data);
#endif

  pio_sm_put_blocking(pio, sm, addr);
  pio_sm_put_blocking(pio, sm, ~data);
}

//==============================================================================
// Dump info

const char *dm_str(uint8_t addr) {
  if (addr > DM_DATA0)
    goto unknown;

  // DM_DATA
  if (addr > DM_CONTROL) {
    static char data[] = "DM_DATA  ";
    uint8_t num = (DM_DATA0 - addr) >> 1;
    decxx_str(data + 7, num);
    return data;
  }

  switch (addr) {
    case DM_CONTROL:      return "DM_CONTROL";
    case DM_STATUS:       return "DM_STATUS";
    case DM_HARTINFO:     return "DM_HARTINFO";
    case DM_ABSTRACTCS:   return "DM_ABSTRACTCS";
    case DM_COMMAND:      return "DM_COMMAND";
    case DM_ABSTRACTAUTO: return "DM_ABSTRACTAUTO";
  }

  // DM_PROGBUF
  if (addr > DM_HALTSUM0) {
    int8_t num = DM_PROGBUF0 - addr;
    if (num < 0)
      goto unknown;

    static char progbuf[] = "DM_PROGBUF  ";
    decxx_str(progbuf + 10, num >> 1);
    return progbuf;
  }

  switch (addr) {
    case DM_HALTSUM0: return "DM_HALTSUM";
    case DM_CPBR:     return "DM_CPBR";
    case DM_CFGR:     return "DM_CFGR";
    case DM_SHDWCFGR: return "DM_SHDWCFGR";
    case DM_CHIPID:   return "DM_CHIPID";
  }

unknown:
  // Unknown
  return "DM?";
}

//------------------------------------------------------------------------------

static inline void swio_tick_dump(void) {
  uint32_t clkdiv = pio->sm[sm].clkdiv;

  uint16_t div_int = HIWORD(clkdiv);
  uint8_t div_frac = HIBYTE(LOWORD(clkdiv));
  float div = div_int + div_frac / 256.0f;

  float tick = 1e9 * div / clock_get_hz(clk_sys);
  print_num(2, "tick (ns)", tick);
}

//------------------------------------------------------------------------------

static void swio_pio_dump(void) {
  print_b(0, "PIO\n");

  uint32_t block = pio_get_index(pio);
  print_num(2, "block", block);
  print_num(2, "sm", sm);
  print_num(2, "offset", offset);

  swio_tick_dump();
}

//------------------------------------------------------------------------------

void swio_dump(void) {
  print_y(0, "swio:info\n");
  swio_pio_dump();

  dm_cpbr cpbr = dm_get_cpbr();
  dm_cpbr_dump(cpbr);

  dm_cfgr cfgr = dm_get_cfgr();
  dm_cfgr_dump(DM_CFGR, cfgr);

  dm_cfgr shdwcfgr = dm_get_shdwcfgr();
  dm_cfgr_dump(DM_SHDWCFGR, cfgr);

  uint32_t chipid = dm_get_chipid();
  vndb_chipid_dump("DM_CHIPID", (vndb_chipid)chipid);
}

//==============================================================================
// Debug interface registers

bool dm_status_wait(uint32_t mask, uint32_t value) {
  for (int i = 0; i < 200; i++) {  // timeout 10 ms
    dm_status status = dm_get_status();
    if ((status.raw & mask) == value)
      return true;

    sleep_us(50);
  }

  print_r(2, "DM_STATUS: timeout (mask=%08X expected=%08X)\n", mask, value);
  return false;
}

//------------------------------------------------------------------------------

inline void dm_print(uint8_t reg, uint32_t raw) {
  const char *name = dm_str(reg);
  print_hex(0, name, raw);
}

//------------------------------------------------------------------------------

static inline void dm_tdiv_dump(dm_tdiv tdiv) {
  print_num(2, "clock division factor", tdiv == DMTDIV1 ? 1 : 2);
}

//------------------------------------------------------------------------------

static uint8_t dm_sopn_to_timebase(dm_sopn sopn) {
  switch (sopn) {
    case DMSOPN8:  return 8;
    case DMSOPN16: return 16;
    case DMSOPN32: return 32;
    case DMSOPN64: return 64;
  }
  return 0;
}

//------------------------------------------------------------------------------

static void dm_sopn_dump(dm_sopn sopn) {
  char buf[32];
  uint8_t timebase = dm_sopn_to_timebase(sopn);
  snprintf(buf, sizeof(buf), "at least %d times the time base", timebase);
  print_str(2, "stop sign factor", buf);
}

//------------------------------------------------------------------------------

static const char *dm_iomode_str(dm_iomode iomode) {
  switch (iomode) {
    case DMIO_FREE:                  return "free";
    case DMIO_FREE_SINGLE:           return "free, single";
    case DMIO_FREE_SINGLE_DUAL:      return "free, single, dual";
    case DMIO_FREE_SINGLE_DUAL_QUAD: return "free, single, dual, quad";
  }
  return "?";
}

//------------------------------------------------------------------------------

static inline void dm_iomode_dump(dm_iomode iomode) {
  const char *desc = dm_iomode_str(iomode);
  print_str(2, "debug I/O port support mode", desc);
}

//------------------------------------------------------------------------------

void dm_data_dump(uint8_t count) {
  print_b(0, "DM_DATA");

  for (uint8_t i = 0; i < count; i++) {
    if (!(i % 6))
      putchar('\n');

    uint32_t data = dm_get_data(i);
    printf("  %d: %08X", i, data);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------

void dm_control_dump(dm_control r) {
  dm_print(DM_CONTROL, r.raw);
  printf("  ACKHAVERESET:%d  DMACTIVE:%d  HALTREQ:%d  NDMRESET:%d  RESUMEREQ:%d\n",
         r.b.ACKHAVERESET, r.b.DMACTIVE, r.b.HALTREQ, r.b.NDMRESET, r.b.RESUMEREQ);
}

//------------------------------------------------------------------------------

void dm_status_dump(dm_status r) {
  dm_print(DM_STATUS, r.raw);
  printf("  ANYAVAIL:%d  ANYHALTED:%d  ANYHAVERESET:%d  ANYRESUMEACK:%d  ANYRUNNING:%d\n",
         r.b.ANYAVAIL, r.b.ANYHALTED, r.b.ANYHAVERESET, r.b.ANYRESUMEACK, r.b.ANYRUNNING);
  printf("  ALLAVAIL:%d  ALLHALTED:%d  ALLHAVERESET:%d  ALLRESUMEACK:%d  ALLRUNNING:%d\n",
         r.b.ALLAVAIL, r.b.ALLHALTED, r.b.ALLHAVERESET, r.b.ALLRESUMEACK, r.b.ALLRUNNING);
  printf("  AUTHENTICATED:%d  VERSION:%d\n", r.b.AUTHENTICATED, r.b.VERSION);
}

//==============================================================================

static void dm_dataaccess_dump(bool data_access) {
  print_b(2, "data reg");
  printf(": mapped to ");
  printf(data_access ? "memory" : "CSR");
  putchar('\n');
}

//------------------------------------------------------------------------------

void dm_hartinfo_dump(dm_hartinfo r) {
  dm_print(DM_HARTINFO, r.raw);
  dm_dataaccess_dump(r.b.DATAACCESS);
  printf("  DATAADDR:%03X  DATASIZE:%d  NSCRATCH:%d\n",
         r.b.DATAADDR, r.b.DATASIZE, r.b.NSCRATCH);
}

//==============================================================================

bool dm_abstractcs_wait(void) {
  for (int i = 0; i < 40; i++) {  // timeout 2 ms
    dm_abstractcs abstractcs = dm_get_abstractcs();
    if (abstractcs.b.BUSY) {
      sleep_us(50);
      continue;
    }

    // If cmderr is 0, the abstract command executed successfully
    if (abstractcs.b.CMDER) {
      print_r(2, "abstract command failed");
      dm_cmder_dump(abstractcs.b.CMDER, false);
      return false;
    }

    return true;
  }

  print_r(2, "abstract command timeout\n");
  return false;  // timeout
}

//------------------------------------------------------------------------------

void dm_cmder_dump(dm_cmder_t cmder, bool print_name) {
  const char *desc;

  if (print_name)
    print_b(2, "cmder");

  putchar(':');
  if (cmder == CMDER_SUCCESS) {
    printf(" no error\n");
    return;
  }

  switch (cmder) {
    case CMDER_ILLEGAL:     desc = "accessing unsupported registers";       break;
    case CMDER_UNSUPPORTED: desc = "does not support command";              break;
    case CMDER_EXEC:        desc = "execution with exception";              break;
    case CMDER_UNAVAILABLE: desc = "not halted or unavailable bus error";   break;
    case CMDER_BUS:         desc = "bus error";                             break;
    case CMDER_PARITY:      desc = "parity bit error during communication"; break;
    case CMDER_OTHER:       desc = "other";                                 break;
    default:                desc = "?";
  }
  print_r(1, "%s\n", desc);
}

//------------------------------------------------------------------------------

void dm_abstractcs_dump(dm_abstractcs r) {
  dm_print(DM_ABSTRACTCS, r.raw);
  dm_cmder_dump(r.b.CMDER, true);
  printf("  BUSY:%d  DATACOUNT:%d  PROGBUFSIZE:%d\n",
         r.b.BUSY, r.b.DATACOUNT, r.b.PROGBUFSIZE);
}

//==============================================================================

static void dm_command_type_dump(dm_cmdtype cmdtype) {
  const char *desc;

  switch (cmdtype) {
    case DM_ACCESS_REG:   desc = "access register";  break;
    case DM_ACCESS_QUICK: desc = "quick access";     break;
    case DM_ACCESS_MEM:   desc = "access to memory"; break;
    default:                 desc = "?";
  }

  print_str(2, "abstract command type", desc);
}

//------------------------------------------------------------------------------

static void dm_command_aarsize_dump(dm_aarsize aarsize) {
  const char *desc;

  switch (aarsize) {
    case AARSIZE8:   desc = "8-bit";   break;
    case AARSIZE16:  desc = "16-bit";  break;
    case AARSIZE32:  desc = "32-bit";  break;
    case AARSIZE64:  desc = "64-bit";  break;
    case AARSIZE128: desc = "128-bit"; break;
    default:             desc = "?";
  }

  print_str(2, "access register data bit width", desc);
}

//------------------------------------------------------------------------------

static void dm_command_regno_dump(uint16_t regno) {
  const char *reg, *name;

  if (regno & DMCM_GPR) {
    if (regno & DMCM_FPR) {
      print_hex(2, "FPR", regno & ~(DMCM_FPR | DMCM_GPR));
      return;
    }

    reg = "GPR";
    regno &= ~DMCM_GPR;
    name = regno < 32 ? gpr_names[regno] : "?";
  } else {
    reg = "CSR";
    name = csr_name(regno);
  }

  print_str(2, reg, name);
}

//------------------------------------------------------------------------------

void dm_command_dump(dm_command r) {
  dm_print(DM_COMMAND, r.raw);
  dm_command_type_dump(r.b.CMDTYPE);
  dm_command_aarsize_dump(r.b.AARSIZE);
  dm_command_regno_dump(r.b.REGNO);
  printf("  AARPOSTINC:%d  POSTEXEC:%d  TRANSFER:%d  WRITE:%d\n",
         r.b.AARPOSTINC, r.b.POSTEXEC, r.b.TRANSFER, r.b.WRITE);
}

//==============================================================================

inline void dm_abstractcs_clear_err(void) {
  dm_abstractcs abstractcs = dm_get_abstractcs();
  if (abstractcs.b.CMDER) {
    abstractcs.b.CMDER = 0b111;  // W1C!
    dm_set_abstractcs(abstractcs.raw);
  }
}

//------------------------------------------------------------------------------

inline void dm_abstractauto_dump(dm_abstractauto r) {
  dm_print(DM_ABSTRACTAUTO, r.raw);
  printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n", r.b.AUTOEXECDATA, r.b.AUTOEXECPROG);
}

//------------------------------------------------------------------------------

void dm_progbuf_dump(size_t count) {
  print_b(0, "DM_PROGBUF");

  for (size_t i = 0; i < count; i++) {
    if (!(i % 6))
      putchar('\n');

    uint32_t progbuf = dm_get_progbuf(i);
    printf("  %d: %08X", i, progbuf);
  }
  putchar('\n');
}

//------------------------------------------------------------------------------

void dm_cpbr_dump(dm_cpbr r) {
  dm_print(DM_CPBR, r.raw);
  dm_tdiv_dump(r.b.TDIV);
  dm_sopn_dump(r.b.SOPN);
  dm_iomode_dump(r.b.IOMODE);
  printf("  CRC8:%d  CMD_EXTENSION:%d  OUTPUT_FUNC:%d  VERSION:%d\n",
         r.b.CHECKSTA, r.b.CMDEXTENSTA, r.b.OUTSTA, r.b.VERSION);
}

//------------------------------------------------------------------------------

void dm_cfgr_dump(uint8_t addr, dm_cfgr r) {
  dm_print(addr, r.raw);
  dm_tdiv_dump(r.b.TDIVCFG);
  dm_sopn_dump(r.b.SOPNCFG);
  dm_iomode_dump(r.b.IOMODECFG);
  printf("  CRC8:%d  CMD_EXTENSION:%d  OUTPUT_FUNC:%d  KEY:%04X\n",
         r.b.CHECKEN, r.b.CMDEXTEN, r.b.OUTEN, r.b.KEY);
}

//------------------------------------------------------------------------------
