#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------
// Misc

#define countof(x)  (sizeof(x) / sizeof(x[0]))

#define SOH  0x01  // Start Of Header (128-byte block)
#define STX  0x02  // Start Of TeXt (1K-byte block)
#define EOT  0x04  // End Of Transmission
#define ACK  0x06  // Acknowledge
#define NAK  0x15  // Negative Acknowledge
#define SYN  0x16  // Synchronous Idle
#define CAN  0x18  // Cancel

//------------------------------------------------------------------------------

int s_cmp(const char* prefix, const char* text);

//------------------------------------------------------------------------------
// Logs

#define LOGS  0

#define PRINT_BLACK    30
#define PRINT_RED      31
#define PRINT_GREEN    32
#define PRINT_YELLOW   33
#define PRINT_BLUE     34
#define PRINT_MAGENTA  35
#define PRINT_CYAN     36
#define PRINT_WHITE    37

#define print_r(...)  print_color(PRINT_RED, __VA_ARGS__)
#define print_g(...)  print_color(PRINT_GREEN, __VA_ARGS__)
#define print_y(...)  print_color(PRINT_YELLOW, __VA_ARGS__)
#define print_b(...)  print_color(PRINT_BLUE, __VA_ARGS__)
#define print_m(...)  print_color(PRINT_MAGENTA, __VA_ARGS__)
#define print_c(...)  print_color(PRINT_CYAN, __VA_ARGS__)
#define print_w(...)  print_color(PRINT_WHITE, __VA_ARGS__)

void print_color(uint8_t color, uint8_t spaces, const char* fmt, ...);
void print_num(uint8_t spaces, const char* name, uint32_t value);
void print_float(uint8_t spaces, const char* name, float value);
void print_hex(uint8_t spaces, const char* name, uint32_t value);
void print_bits(uint8_t spaces, const char *name, uint32_t value, uint8_t bit_count);
void print_str(uint8_t spaces, const char* name, const char* text);

#if LOGS

#define CHECK(A)  if (!(A)) { print_r(0, "ERROR: %s %d\n", __FILE__, __LINE__); }
#define LOG(...)  printf(__VA_ARGS__)
#define LOG_ONCE(flag, ...)  \
  do { if (!(flag)) { LOG_C(__VA_ARGS__); (flag) = true; } } while (0)

#define LOG_R(...) print_r(0, __VA_ARGS__)
#define LOG_G(...) print_g(0, __VA_ARGS__)
#define LOG_Y(...) print_y(0, __VA_ARGS__)
#define LOG_B(...) print_b(0, __VA_ARGS__)
#define LOG_M(...) print_m(0, __VA_ARGS__)
#define LOG_C(...) print_c(0, __VA_ARGS__)
#define LOG_W(...) print_w(0, __VA_ARGS__)

#else

#define LOG(...)
#define LOG_R(...)
#define LOG_G(...)
#define LOG_Y(...)
#define LOG_B(...)
#define LOG_M(...)
#define LOG_C(...)
#define LOG_W(...)

#define LOG_ONCE(flag, ...)

#define CHECK(A, args...)

#endif /* LOGS */

//------------------------------------------------------------------------------
// Hex

inline uint8_t to_hex(uint8_t x) { return x < 10 ? x + '0' : x + 'A' - 10; }
void byte_to_hex(uint8_t b, uint8_t* hex);

int from_hex(uint8_t b);
bool from_hex_check(uint8_t b, uint8_t* out);

//------------------------------------------------------------------------------
// 2*n CPU cycles → 16*n ns (1 cycle = 8 ns @ 125 MHz)

inline void delay_cycles(uint32_t n) {
  asm volatile (
    ".syntax unified      \n"
    "loop: subs %0, %0, #1\n"
    "      bne loop       \n"
    "      nop            \n"
    : "+r" (n)
    :
    : "cc");
}

//------------------------------------------------------------------------------
// Colored status LED
 
#define CLED_RED      0
#define CLED_GREEN    1
#define CLED_BLUE     2
#define CLED_YELLOW   3
#define CLED_CYAN     4
#define CLED_MAGENTA  5
#define CLED_WHITE    6

void cled_set_color(uint8_t idx);

//------------------------------------------------------------------------------
