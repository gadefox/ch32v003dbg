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

bool break_halt(void);
void break_set_halted(bool value);
bool break_is_halted(void);
bool break_resume(void);

int break_find(uint32_t addr);
int break_set(uint32_t addr, int size);
int break_clear(uint32_t addr, int size);

void break_patch_flash(void);
void break_unpatch_flash(void);

#if LOGS
void break_print_logs(void);
#endif /* LOGS */

//------------------------------------------------------------------------------
