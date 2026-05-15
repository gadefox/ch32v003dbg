// Software breakpoint support for WCH MCUs.
// Patches flash to insert breakpoints on resume, unpatches flash on halt.

// Includes a small optimization to prevent excessive patch/unpatching - if the
// next instruction is a breakpoint when we're about to resume the CPU, we skip
// the patch/unpatch, step to the breakpoint, and just leave the CPU halted.

#pragma once

#include <stdbool.h>
#include <stdint.h>

//------------------------------------------------------------------------------

void break_init(void);
void break_dump(void);

int break_set(uint16_t addr);
int break_clear(uint16_t addr);

bool break_resume(bool step);
bool break_patch(void *except, uint32_t mask);

//------------------------------------------------------------------------------
