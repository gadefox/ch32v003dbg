#include <tusb.h>
#include <pico/status_led.h>
#include <pico/stdlib.h>

#include "console.h"
#include "context.h"
#include "break.h"
#include "flash.h"
#include "server.h"
#include "tusb_config.h"
#include "xmodem.h"

#if LOGS
#warning "LOGS are enabled; XMODEM feature will be disabled!"
#endif

//------------------------------------------------------------------------------

static void blink(uint delay, size_t count) {
  for (size_t i = 0; i < count; i++) {
    status_led_set_state(true);
    sleep_ms(delay);
    status_led_set_state(false);
    sleep_ms(delay);
  }
}

//------------------------------------------------------------------------------

static inline void blink_ok(void) { blink(150, 3); }
static inline void blink_err(void) { blink(75, 5); }

//------------------------------------------------------------------------------

static inline void reset(void) {
  if (ctx_reset())
    blink_ok();                   // success status (slow pattern)
  else
    blink_err();                  // error status (fast pattern)
}

//------------------------------------------------------------------------------

#define PICO_KEY_PIN  24

static inline void handle_key(void) {
  if (gpio_get(PICO_KEY_PIN))
    return;

  static uint32_t last;
  uint32_t now = time_us_32();
  if (now - last > 50000) {
    reset();
    last = now;
  }
}

//------------------------------------------------------------------------------

int main(void) {
  uint8_t usb_in, usb_out;
  bool usb_oe;

  // Configure key GPIO
  gpio_init(PICO_KEY_PIN);
  gpio_set_dir(PICO_KEY_PIN, GPIO_IN);
  gpio_pull_up(PICO_KEY_PIN);

  // Configure builtin LED
  status_led_init();
  colored_status_led_set_state(false);

  stdio_init_all();

  tud_init_serials();
  tud_init(BOARD_TUD_RHPORT);

  break_init();
  console_init();
  server_init();
  swio_init();
  xmodem_init();

  reset();

  while (true) {
    // Update TinyUSB
    tud_task();

    bool connected = tud_cdc_n_connected(0);
    bool usb_ie = tud_cdc_n_available(0);  // NOTE:this "available" check is required for some reason
    if (usb_ie) {
      tud_cdc_n_read(0, &usb_in, 1);
#if !LOGS
      if (usb_in == SYN)
        xmodem_mode = true;
#endif
    }

    if (xmodem_mode) {
      // Handle xmodem
      usb_oe = xmodem_update(connected, usb_ie, usb_in, &usb_out);
    } else {
      // Handle console and GDB server
      handle_key();

      // Update console
      if (usb_ie)
        console_update(usb_in);

      // Update GDB stub
      usb_oe = server_update(connected, usb_ie, usb_in, &usb_out);
    }

    if (usb_oe) {
      tud_cdc_n_write(0, &usb_out, 1);
      tud_cdc_n_write_flush(0);
    }
  }

  return 0;
}

//------------------------------------------------------------------------------
