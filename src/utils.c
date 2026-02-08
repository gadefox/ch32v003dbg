#include <ctype.h>
#include <stdio.h>
#include <pico/status_led.h>

#include "utils.h"

//==============================================================================
// Misc

int s_cmp(const char* prefix, const char* text) {
  while (true) {
    // NOTE: we return when we run out of matching prefix, not when both
    // strings match.
    if (*prefix == 0)    return 0;
    if (*prefix > *text) return -1;
    if (*prefix < *text) return 1;
    prefix++;
    text++;
  }
}

//==============================================================================
// Logs

void print_color(uint8_t color, uint8_t spaces, const char* fmt, ...) {
  va_list args;

  va_start(args, fmt);
  printf("%*s\x1b[1;%dm", spaces, "", color);
  vprintf(fmt, args);
  printf("\x1b[0m");
}

//------------------------------------------------------------------------------

void print_num(uint8_t spaces, const char* name, uint32_t value) {
  print_b(spaces, name);
  printf(": %d\n", value);
}

//------------------------------------------------------------------------------

void print_float(uint8_t spaces, const char* name, float value) {
  print_b(spaces, name);
  printf(": %f\n", value);
}

//------------------------------------------------------------------------------

void print_hex(uint8_t spaces, const char* name, uint32_t value) {
  print_b(spaces, name);
  printf(": %08X\n", value);
}

//------------------------------------------------------------------------------

void byte_to_hex(uint8_t b, uint8_t* hex) {
  *hex++ = to_hex(b >> 4);
  *hex = to_hex(b & 0xF);
}

//------------------------------------------------------------------------------

void print_bits(uint8_t spaces, const char* name, uint32_t value, uint8_t bit_count) {
  print_b(spaces, name);
  printf(": ");

  for (int i = bit_count - 1; i >= 0; i--)
    putchar(value & (1 << i) ? '1' : '0');
  putchar('\n');
}

//------------------------------------------------------------------------------

void print_str(uint8_t spaces, const char* name, const char* text) {
  print_b(spaces, name);
  printf(": %s\n", text);
}

//==============================================================================
// Hexadecimal

int from_hex(uint8_t b) {
  if (b >= '0' && b <= '9')
    return b - '0';
  if (b >= 'a' && b <= 'f')
    return b - 'a' + 10;
  if (b >= 'A' && b <= 'F')
    return b - 'A' + 10;
  return -1;
}

//------------------------------------------------------------------------------

bool from_hex_check(uint8_t b, uint8_t* out) {
  if (b >= '0' && b <= '9') {
    *out = b - '0';
    return true;
  }
  if (b >= 'a' && b <= 'f') {
    *out = b - 'a' + 10;
    return true;
  }
  if (b >= 'A' && b <= 'F') {
    *out = b - 'A' + 10;
    return true;
  }
  return false;
}

//==============================================================================
// Colored status LED

// NOTE:This function needs to be added to the Pico SDK
/*
bool colored_status_led_set_color(uint32_t color) {
    colored_status_led_on_color = color;
    bool success = false;
    if (colored_status_led_supported()) {
#if COLORED_STATUS_LED_USING_WS2812_PIO
        success = set_ws2812(colored_status_led_on_color);
#endif
    }
    if (success) colored_status_led_on = (color != 0);
    return success;
}
*/
//------------------------------------------------------------------------------

#define LED_DIM  7

const uint32_t cled_colors[] = {
  LED_DIM << 16,                               // red
                     LED_DIM << 8,             // green
                                     LED_DIM,  // blue
  (LED_DIM << 16) | (LED_DIM << 8),            // yellow
                    (LED_DIM << 8) | LED_DIM,  // cyan
  (LED_DIM << 16) |                  LED_DIM,  // magenta
  (LED_DIM << 16) | (LED_DIM << 8) | LED_DIM   // white
};

//------------------------------------------------------------------------------

void cled_set_color(uint8_t idx) {
  CHECK(idx < countof(cled_colors))
  colored_status_led_set_color(cled_colors[idx]);
 }

//------------------------------------------------------------------------------
