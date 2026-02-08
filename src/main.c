#include <tusb.h>
#include <pico/status_led.h>
#include <pico/stdlib.h>

#include "console.h"
#include "context.h"
#include "break.h"
#include "flash.h"
#include "server.h"
#include "swio.h"
#include "tusb_config.h"
#include "xmodem.h"

#if LOGS
#warning "LOGS are enabled; XMODEM feature will be disabled!"
#endif

//------------------------------------------------------------------------------

#define PICO_KEY_PIN  24

//------------------------------------------------------------------------------

static bool key_prev = true;

//------------------------------------------------------------------------------

#define OK_DELAY  150
#define OK_COUNT  3

#define ERR_DELAY  75
#define ERR_COUNT  8

void blink(int delay, int count) {
  while (count > 0) {
    status_led_set_state(true);
    sleep_ms(delay);
    
    status_led_set_state(false);
    sleep_ms(delay);

    count--;
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
  ctx_init();
  server_init();
  swio_init();
  xmodem_init();

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
      if (!gpio_get(PICO_KEY_PIN)) {
        if (key_prev) {                   // falling edge
          if (swio_reset())
            blink(OK_DELAY, OK_COUNT);    // success status (slow pattern)
          else
            blink(ERR_DELAY, ERR_COUNT);  // error status (fast pattern)

          sleep_ms(50);                   // debounce
          key_prev = false;
        }
      } else
        key_prev = true;

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
