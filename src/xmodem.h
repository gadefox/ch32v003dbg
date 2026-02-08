#pragma once

#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------

extern bool xmodem_mode;

//------------------------------------------------------------------------------

void xmodem_init(void);
bool xmodem_update(bool connected, bool byte_ie, uint8_t byte_in, uint8_t* byte_out);

#if LOGS
uint16_t xmodem_crc_calc(const uint8_t* src, uint32_t size);
#endif // LOGS

//------------------------------------------------------------------------------
