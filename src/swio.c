#include <stdio.h>
#include <hardware/clocks.h>
#include <hardware/sync.h>

#include "out/singlewire.pio.h"
#include "break.h"
#include "reg.h"
#include "utils.h"

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

  uint32_t part = dm_get_part();
  dm_print(DM_PART, part);
}

//------------------------------------------------------------------------------
