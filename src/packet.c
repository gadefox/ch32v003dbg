#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "packet.h"

//------------------------------------------------------------------------------

void packet_init(packet* p, uint16_t cap) {
  packet_clear(p);
  p->buf = malloc(cap);
  p->cap = cap;
}

//------------------------------------------------------------------------------

void packet_clear(packet* p) {
  p->len = 0;
  p->pos = 0;
  p->error = false;
}

//------------------------------------------------------------------------------
// Advances the cursor by d characters; sets error if it would go past buffer

bool packet_skip(packet* p, int d) {
  if (p->pos + d > p->len) {
    p->error = true;
    return false;
  }
  p->pos += d;
  return true;
}

//------------------------------------------------------------------------------
// Skips all whitespace characters from the current cursor position

void packet_skip_ws(packet* p) {
  while (p->pos < p->len &&
         isspace((unsigned char)p->buf[p->pos]))
    p->pos++;
}

//==============================================================================
// Character manipulation functions for packets

bool packet_match(packet* p, uint8_t b) {
  return p->pos < p->len && p->buf[p->pos] == b;
}

//------------------------------------------------------------------------------
// If the character at the cursor matches c, advances cursor and returns true

bool packet_match_advance(packet* p, uint8_t b) {
  if (packet_match(p, b)) {
    p->pos++;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
// Returns the character at the cursor and advances it; sets error if at end

uint8_t packet_take(packet* p) {
  if (p->pos < p->len)
    return p->buf[p->pos++];

  p->error = true;
  return 0;
}

//------------------------------------------------------------------------------
// Expects character c at the current cursor; sets error if it does not match

bool packet_expect(packet* p, uint8_t b) {
  if (packet_match_advance(p, b))
    return true;

  p->error = true;
  return false;
}

//------------------------------------------------------------------------------

bool packet_put(packet* p, uint8_t b) {
  if (p->len >= p->cap) {
    LOG_R("pkt:put: overflow cap=%u\n", p->cap);
    p->error = true;
    return false;
  }

  p->buf[p->len] = b;
  p->len++;
  return true;
}

//==============================================================================
// String parsing functions for packets

//------------------------------------------------------------------------------
// Expects the exact string s at the current cursor; sets error if mismatch

bool packet_expect_buf(packet* p, const uint8_t* s) {
  while (*s) {
    if (!packet_expect(p, *s))
      return false;
    s++;
  }
  return true;
}

//------------------------------------------------------------------------------

bool packet_put_buf(packet* p, const uint8_t* buf, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    if (!packet_put(p, buf[i]))
      return false;
  }
  return true;
}

//==============================================================================
// Number parsing functions for packets

int32_t packet_take_int_base(packet *p, uint8_t base) {
  CHECK(base <= 10);
  int32_t accum = 0;
  uint32_t pos = p->pos;
  bool any_digits = false, negative = false;

  if (packet_match(p, '-')) {
    negative = true;
    pos++;
  }

  while (pos < p->len) {
    int digit = p->buf[pos] - '0';
    if (digit < 0 || digit >= base)
      break;

    accum = accum * base + digit;
    any_digits = true;
    pos++;
  }

  if (!any_digits) {
    p->error = true;
    return 0;
  }

  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return negative ? -accum : accum;
}

//------------------------------------------------------------------------------

int32_t packet_take_int(packet* p) {
  if (packet_match_advance(p, '0')) {
    // Number begins with zero digit
    if (p->pos >= p->len)
      return 0;  // Standalone '0' is a valid number, not a parse error

    uint8_t b = p->buf[p->pos];
    if (b == 'x' || b == 'X') {
      p->pos++;  // consume 'x'/'X'
      return packet_take_hex(p);
    }

    if (b == 'b' || b == 'B') {
      p->pos++;  // consume 'b'/'B'
      return packet_take_int_base(p, 2);
    }

    // Handle 08 or 09 as decimal instead of octal
    if (b == '8' || b == '9')
      return packet_take_int_base(p, 10);

    // No prefix -> octal, next digit must be parsed by the octal parser
    return packet_take_int_base(p, 8);
  }

  // Fallback to decimal (base 10) when no base prefix is present
  return packet_take_int_base(p, 10);
}

//==============================================================================
// Hexadecimal parsing functions for packets

//------------------------------------------------------------------------------
// Parses exactly 'digits' hex characters from the current cursor.
// Cursor is advanced only for valid digits.
//
uint32_t packet_take_hex_digits(packet* p, uint8_t digits) {
  CHECK(digits <= sizeof(uint32_t) * 2);
  uint32_t accum = 0, pos = p->pos;
  uint8_t digit;

  while (digits > 0) {
    if (pos >= p->len ||
        !from_hex_check(p->buf[pos], &digit)) {
      p->error = true;
      return 0;
    }

    accum = (accum << 4) | digit;
    pos++;
    digits--;
  }

  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return accum;
}

//------------------------------------------------------------------------------
// Parses up to 8 hex digits (uint32_t) from the current cursor.
// Cursor is advanced only for valid digits. Sets error if no digits found.

uint32_t packet_take_hex(packet* p) {
  uint32_t accum = 0, pos = p->pos;
  bool any_digits = false;
  uint8_t digit;

  for (int i = 0; i < sizeof(uint32_t) * 2; i++) {
    if (pos >= p->len ||
        !from_hex_check(p->buf[pos], &digit))
      break;
 
    accum = (accum << 4) | digit;
    any_digits = true;
    pos++;
  }

  if (!any_digits) {
    p->error = true;
    return 0;
  }
  
  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return accum;
}

//------------------------------------------------------------------------------
// Converts `count` hex-encoded bytes from the packet into binary and stores
// them in `buf`. Each byte in the packet is represented by two hex characters.

bool packet_take_hex_to_buf(packet* p, uint8_t* buf, uint32_t count) {
  uint32_t pos = p->pos;
  uint8_t lo, hi;

  for (int i = 0; i < count; i++) {
    if (pos + 1 >= p->len ||
        !from_hex_check(p->buf[pos++], &hi) ||
        !from_hex_check(p->buf[pos++], &lo)) {
      p->error = true;
      return false;
    }

    *buf++ = (hi << 4) | lo;
  }

  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return true;
}

//==============================================================================
// Matching functions for packets
// These functions check if a string or character matches the content at the current cursor.
// Cursor is only advanced on a successful match; errors are not set automatically.

//------------------------------------------------------------------------------
// Matches string `s` at current cursor position; requires a word boundary.
// Examples:
//   s = "reset" matches packet "reset 0"
//   s = "reset" matches packet "reset\n"
//   s = "reset" does NOT match packet "reset123"
//   s = "reset" does NOT match packet "res"

bool packet_match_word(packet* p, const uint8_t* buf) {
  uint32_t pos = p->pos;

  while (*buf) {
    if (pos >= p->len || p->buf[pos] != *buf)
      return false;
    pos++;
    buf++;
  }

  // Word boundary: end of buffer or non-alphanumeric
  if (pos < p->len && isalnum((unsigned char)p->buf[pos]))
    return false;
  
  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return true;
}

//------------------------------------------------------------------------------
// matches s at cursor; does not care what follows
// e.g. s = "reset" matches packet "reset123"

bool packet_match_prefix(packet* p, const uint8_t* buf) {
  uint32_t pos = p->pos;

  while (*buf) {
    if (pos >= p->len || p->buf[pos] != *buf)
      return false;
    pos++;
    buf++;
  }

  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return true;
}

//------------------------------------------------------------------------------
// s = ASCII bytes (e.g. "reset") matched against hex-encoded
// packet data (e.g. "7265736574")

bool packet_match_prefix_hex(packet* p, const uint8_t* buf) {
  uint32_t pos = p->pos;

  while (*buf) {
    if (pos + 1 >= p->len ||
       from_hex(p->buf[pos++]) != (*buf >> 4) ||
       from_hex(p->buf[pos++]) != (*buf & 0xF))
      return false;
    buf++;
  }

  // Commit cursor position only after successfully parsing all digit
  p->pos = pos;
  return true;
}

//==============================================================================
// PUT – append / write to packet

//------------------------------------------------------------------------------

bool packet_put_hex_u8(packet* p, uint8_t b) {
  // must be high nibble first
  return packet_put(p, to_hex(b >> 4)) &&
         packet_put(p, to_hex(b & 0xF));
}

//------------------------------------------------------------------------------

bool packet_put_hex_u16(packet* p, uint16_t w) {
  // must be little-endian
  return packet_put_hex_u8(p, w) &&
         packet_put_hex_u8(p, w >> 8);
}

//------------------------------------------------------------------------------

bool packet_put_hex_u32(packet* p, uint32_t x) {
  // must be little-endian
  return packet_put_hex_u8(p, x) &&
         packet_put_hex_u8(p, x >> 8) &&
         packet_put_hex_u8(p, x >> 16) &&
         packet_put_hex_u8(p, x >> 24);
}

//------------------------------------------------------------------------------

bool packet_put_hex_buf(packet* p, uint8_t* buf, uint32_t count) {
  for (int i = 0; i < count; i++) {
    if (!packet_put_hex_u8(p, buf[i]))
      return false;
  }
  return true;
}

//==============================================================================
// Console command arguments

int32_t packet_take_arg(packet* p, int32_t optional) {
  packet_skip_ws(p);
  if (packet_is_empty(p) && optional != -1)
    return optional;  // default value

  int32_t value = packet_take_int(p);
  packet_skip_ws(p);
  if (packet_is_empty(p))
    return value;

  p->error = true;
  return -1;
}

//------------------------------------------------------------------------------
