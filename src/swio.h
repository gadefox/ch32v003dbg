// Standalone Implementation of WCH's SWIO protocol for their CH32V003 chip.

#pragma once

#include <stdint.h>

//------------------------------------------------------------------------------

bool swio_init(void);
bool swio_reset(void);
void swio_dump(void);

uint32_t swio_get(uint32_t addr);
void swio_put(uint32_t addr, uint32_t data);

//------------------------------------------------------------------------------
