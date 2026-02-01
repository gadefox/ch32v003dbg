#include <stdio.h>
#include <hardware/clocks.h>
#include <hardware/sync.h>

#include "out/singlewire.pio.h"
#include "break.h"
#include "swio.h"
#include "utils.h"
#include "vendor.h"

//------------------------------------------------------------------------------

#define DUMP_COMMANDS  0
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
  sm_config_set_out_shift(&c, false, false, 32);
  sm_config_set_in_shift(&c, false, true, 32);

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

bool swio_reset(void) {
  // Save current interrupt state and disable all interrupts.
  // This guarantees that the timing below is not disturbed by IRQs.
  uint32_t irq_state = save_and_disable_interrupts();

  // Initialize the GPIO pin in a safe state: set the pin to SIO function,
  // input direction, and explicitly clears the output latch to 0 (LOW).
  gpio_init(PICO_SWIO_PIN);

  // Switch the pin to output mode. Because the output latch is already 0,
  // the pin immediately drives LOW with no glitch or jitter.
  gpio_set_dir(PICO_SWIO_PIN, GPIO_OUT);

  // Hold the pin LOW for a precise number of CPU cycles.
  // 500 cycles correspond to ~8 microseconds at 125 MHz.
  delay_cycles(500);

  // Set the GPIO pin to input mode. This disables the output driver.
  gpio_set_dir(PICO_SWIO_PIN, GPIO_IN);

  // Configure the given pin for use by the specified PIO instance.
  pio_gpio_init(pio, PICO_SWIO_PIN);

  // Restore interrupt state and enable all interrupts.
  restore_interrupts(irq_state);

  // Enable debug output pin on target
  dm_set_shdwcfgr(DMSC_OUTEN | DMSC_KEY(0x5AA5));
  dm_set_cfgr(DMCF_OUTEN | DMCF_KEY(0x5AA5));

  // Reset debug module on target
  dm_set_control(0);
  dm_set_control(DMCN_DMACTIVE);

  // Verify SWIO link by reading CPBR
  dm_cpbr cpbr = dm_get_cpbr();
  if (cpbr.raw != (CPBR_TDIV(3) | CPBR_OUTSTA | CPBR_VERSION(1))) {
    LOG_R("failed to initialize SWIO (CPBR=%08X, expected 00010403)\n", cpbr.raw);
    return false;
  }

  // Verify number of data registers; dscratch registers supported and data0
  // offset address
  dm_hartinfo hartinfo = dm_get_hartinfo();
  if (hartinfo.raw != (DMHI_NSCRATCH(2) | DMHI_DATAACCESS | DMHI_DATASIZE(2) | DMHI_DATAADDR(0xF4))) {
    LOG_R("wrong chip (hartinfo=%08X, expected 002120F4)\n", hartinfo.raw);
    return false;
  }

  dm_status status = dm_get_status();
  break_set_halted(status.b.ALLHALTED);
  return true;
}

//------------------------------------------------------------------------------

uint32_t swio_get(uint32_t addr) {
  pio_sm_put_blocking(pio, sm, ((~addr) << 1) | 1);
  uint32_t data = pio_sm_get_blocking(pio, sm);
#if DUMP_COMMANDS
  printf("get_dbg: ");
  dm_print(addr, data);
#endif
  return data;
}

//------------------------------------------------------------------------------

void swio_put(uint32_t addr, uint32_t data) {
#if DUMP_COMMANDS
  printf("set_dbg: ");
  dm_print(addr, data);
#endif
  pio_sm_put_blocking(pio, sm, ((~addr) << 1) | 0);
  pio_sm_put_blocking(pio, sm, ~data);
}

//------------------------------------------------------------------------------

void swio_dump(void) {
  print_y(0, "swio:dump\n");
  print_b(0, "PIO\n");

  uint blk = pio_get_index(pio);
  print_num(2, "block", blk);
  
  print_num(2, "sm", sm);
  print_num(2, "offset", offset);

  uint32_t clkdiv = pio->sm[sm].clkdiv;
  uint16_t div_int = clkdiv >> 16;
  uint8_t div_frac = (clkdiv >> 8) & 0xFF;
  float div = div_int + div_frac / 256.0f;
  float tick = 1e9 * div / clock_get_hz(clk_sys);
  print_num(2, "tick (ns)", tick);
  
  dm_cpbr cpbr = dm_get_cpbr();
  dm_cpbr_dump(cpbr);
  
  dm_cfgr cfgr = dm_get_cfgr();
  dm_cfgr_dump(cfgr);
  
  dm_shdwcfgr shwdcfgr = dm_get_shdwcfgr();
  dm_shdwcfgr_dump(shwdcfgr);

  uint32_t chipid = dm_get_chipid();
  dm_print(DM_CHIPID, chipid);
}

//==============================================================================
// Debug interface registers

void dm_print(uint8_t reg, uint32_t raw) {
  const char* name = dm_to_name(reg);
  print_hex(0, name, raw);
}

//------------------------------------------------------------------------------

const char* dm_to_name(uint8_t reg) {
  switch (reg) {
    case DM_DATA0:
      return "DM_DATA0";
    case DM_DATA1:
      return "DM_DATA1";
    case DM_CONTROL:
      return "DM_CONTROL";
    case DM_STATUS:
      return "DM_STATUS";
    case DM_HARTINFO:
      return "DM_HARTINFO";
    case DM_ABSTRACTCS:
      return "DM_ABSTRACTCS";
    case DM_COMMAND:
      return "DM_COMMAND";
    case DM_ABSTRACTAUTO:
      return "DM_ABSTRACTAUTO";
    case DM_PROGBUF0:
      return "DM_PROGBUF0";
    case DM_PROGBUF1:
      return "DM_PROGBUF1";
    case DM_PROGBUF2:
      return "DM_PROGBUF2";
    case DM_PROGBUF3:
      return "DM_PROGBUF3";
    case DM_PROGBUF4:
      return "DM_PROGBUF4";
    case DM_PROGBUF5:
      return "DM_PROGBUF5";
    case DM_PROGBUF6:
      return "DM_PROGBUF6";
    case DM_PROGBUF7:
      return "DM_PROGBUF7";
    case DM_HALTSUM0:
      return "DM_HALTSUM0";
    case DM_CPBR:
      return "DM_CPBR";
    case DM_CFGR:
      return "DM_CFGR";
    case DM_SHDWCFGR:
      return "DM_SHDWCFGR";
    case DM_CHIPID:
      return "DM_CHIPID";
    default:
      return "DM_?";
  }
}

//------------------------------------------------------------------------------

void dm_control_dump(dm_control r) {
  dm_print(DM_CONTROL, r.raw);
  printf("  ACKHAVERESET:%d  ACKUNAVAIL:%d  CLRKEEPALIVE:%d  CLRRESETHALTREQ:%d  DMACTIVE:%d\n",
         r.b.ACKHAVERESET, r.b.ACKUNAVAIL, r.b.CLRKEEPALIVE, r.b.CLRRESETHALTREQ, r.b.DMACTIVE);
  printf("  HASEL:%d  HALTREQ:%d  HARTRESET:%d  HARTSELHI:%d  HARTSELLO:%d\n",
         r.b.HASEL, r.b.HALTREQ, r.b.HARTRESET, r.b.HARTSELHI, r.b.HARTSELLO);
  printf("  NDMRESET:%d  RESUMEREQ:%d  SETKEEPALIVE:%d  SETRESETHALTREQ:%d\n",
         r.b.NDMRESET, r.b.RESUMEREQ, r.b.SETKEEPALIVE, r.b.SETRESETHALTREQ);
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

//------------------------------------------------------------------------------

void dm_hartinfo_dump(dm_hartinfo r) {
  dm_print(DM_HARTINFO, r.raw);
  printf("  DATAACCESS:%d  DATAADDR:%03X  DATASIZE:%d  NSCRATCH:%d\n",
         r.b.DATAACCESS, r.b.DATAADDR, r.b.DATASIZE, r.b.NSCRATCH);
}

//------------------------------------------------------------------------------

void dm_abstractcs_dump(dm_abstractcs r) {
  dm_print(DM_ABSTRACTCS, r.raw);
  printf("  BUSY:%d  CMDER:%d  DATACOUNT:%d  PROGBUFSIZE:%d\n",
         r.b.BUSY, r.b.CMDER, r.b.DATACOUNT, r.b.PROGBUFSIZE);
}

//------------------------------------------------------------------------------

void dm_command_dump(dm_command r) {
  dm_print(DM_COMMAND, r.raw);
  printf("  AARPOSTINC:%d  AARSIZE:%d  POSTEXEC:%d  REGNO:%04X  TRANSFER:%d  WRITE:%d\n",
         r.b.AARPOSTINC, r.b.AARSIZE, r.b.POSTEXEC, r.b.REGNO, r.b.TRANSFER, r.b.WRITE);
}

//------------------------------------------------------------------------------

void dm_abstractauto_dump(dm_abstractauto r) {
  dm_print(DM_ABSTRACTAUTO, r.raw);
  printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n",
         r.b.AUTOEXECDATA, r.b.AUTOEXECPROG);
}

//------------------------------------------------------------------------------

void dm_progbuf_dump(void) {
  print_b(0, "DM_PROGBUF\n");
  for (int i = 0; i < DM_PROGBUF_MAX; i++) {
    uint32_t progbuf = dm_get_progbuf(i);
    printf("  %d: %08X", i, progbuf);

    if ((i & 3) == 3)
      putchar('\n');
  }
}

//------------------------------------------------------------------------------

void dm_cpbr_dump(dm_cpbr r) {
  dm_print(DM_CPBR, r.raw);
  printf("  CHECKSTA:%d  CMDEXTENSTA:%d  IOMODE:%d  OUTSTA:%d  SOPN:%d  TDIV:%d  VERSION:%d\n",
         r.b.CHECKSTA, r.b.CMDEXTENSTA, r.b.IOMODE, r.b.OUTSTA, r.b.SOPN, r.b.TDIV, r.b.VERSION);
}

//------------------------------------------------------------------------------

void dm_cfgr_dump(dm_cfgr r) {
  dm_print(DM_CFGR, r.raw);
  printf("  CHECKEN:%d  CMDEXTEN:%d  IOMODECFG:%d  KEY:%04X  OUTEN:%d  SOPNCFG:%d  TDIVCFG:%d\n",
         r.b.CHECKEN, r.b.CMDEXTEN, r.b.IOMODECFG, r.b.KEY, r.b.OUTEN, r.b.SOPNCFG, r.b.TDIVCFG);
}

//------------------------------------------------------------------------------

void dm_shdwcfgr_dump(dm_shdwcfgr r) {
  dm_print(DM_SHDWCFGR, r.raw);
  printf("  CHECKEN:%d  CMDEXTEN:%d  IOMODECFG:%d  KEY:%04X  OUTEN:%d  SOPNCFG:%d  TDIVCFG:%d\n",
         r.b.CHECKEN, r.b.CMDEXTEN, r.b.IOMODECFG, r.b.KEY, r.b.OUTEN, r.b.SOPNCFG, r.b.TDIVCFG);
}

//------------------------------------------------------------------------------
