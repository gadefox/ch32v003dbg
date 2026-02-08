#pragma once

#include "utils.h"

//------------------------------------------------------------------------------

#define packet_ptr(p)        ((p)->buf + (p)->pos)
#define packet_terminate(p)  packet_put(p, '\0')
#define packet_is_empty(p)   packet_match(p, '\0')

//------------------------------------------------------------------------------

typedef struct {
  uint8_t* buf;
  uint16_t cap;
  uint16_t len;
  uint16_t pos;
  bool error;
} packet;

//------------------------------------------------------------------------------

void packet_init(packet* p, uint16_t cap);
void packet_clear(packet* p);
bool packet_skip(packet* p, int d);
void packet_skip_ws(packet* p);

//------------------------------------------------------------------------------
// Character manipulation functions for packets

bool packet_match(packet* p, uint8_t b);
bool packet_match_advance(packet* p, uint8_t b);
uint8_t packet_take(packet* p);
bool packet_expect(packet* p, uint8_t b);
bool packet_put(packet* p, uint8_t b);

//------------------------------------------------------------------------------
// String parsing functions for packets

bool packet_expect_buf(packet* p, const uint8_t* buf);
bool packet_put_buf(packet* p, const uint8_t* buf, uint32_t count);

//------------------------------------------------------------------------------
// Number parsing functions for packets
 
int32_t packet_take_int_base(packet *p, uint8_t base);
int32_t packet_take_int(packet* p);

//------------------------------------------------------------------------------
// Hexadecimal parsing functions for packets

uint32_t packet_take_hex_digits(packet* p, uint8_t digits);
uint32_t packet_take_hex(packet* p);
bool packet_take_hex_to_buf(packet* p, uint8_t* buf, uint32_t count);

//------------------------------------------------------------------------------
// Matching functions for packets

bool packet_match_word(packet* p, const uint8_t* buf);
bool packet_match_prefix(packet* p, const uint8_t* buf);
bool packet_match_prefix_hex(packet* p, const uint8_t* buf);

//------------------------------------------------------------------------------
// PUT – append / write to packet

bool packet_put_hex_u8(packet* p, uint8_t b);
bool packet_put_hex_u16(packet* p, uint16_t w);
bool packet_put_hex_u32(packet* p, uint32_t x);
bool packet_put_hex_buf(packet* p, uint8_t* buf, uint32_t count);

//------------------------------------------------------------------------------
// Console command arguments

int32_t packet_take_arg(packet* p, int32_t optional);
 
//------------------------------------------------------------------------------
