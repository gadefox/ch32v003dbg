#pragma once

#include <stdbool.h>

//------------------------------------------------------------------------------

void console_init(void);
void console_update(uint8_t b);

void console_print_dpc(bool result);
void console_print_result(bool result);
int32_t packet_take_addr(int32_t optional, int32_t max);

//------------------------------------------------------------------------------
// Bootloader handlers

void console_boot_help(void);
void console_boot_parse(void);

void console_boot_lock(void);
void console_boot_unlock(void);

//------------------------------------------------------------------------------
// Breakpoint handlers

void console_break_help(void);
void console_break_parse(void);

void console_break_halt(void);
void console_break_resume(void);
void console_break_set(void);
void console_break_clear(void);

//------------------------------------------------------------------------------
// Console handlers

void console_help(void);
void console_dispatch(void);

//------------------------------------------------------------------------------
// Context handlers

void console_ctx_help(void);
void console_ctx_parse(void);

void console_ctx_reset(void);
void console_ctx_halt(void);
void console_ctx_resume(void);
void console_ctx_step(void);

//------------------------------------------------------------------------------
// Flash handlers

void console_flash_help(void);
void console_flash_parse(void);

void console_flash_erase(void);
void console_flash_lock(void);
void console_flash_unlock(void);

//------------------------------------------------------------------------------
// Help handlers
//
void console_help_parse(void);

//------------------------------------------------------------------------------
// Info handlers

void console_info_help(void);
void console_info_parse(void);

void console_info_boot(void);
void console_info_flash(void);
void console_info_optb(void);

//------------------------------------------------------------------------------
// Options handlers

void console_optb_help(void);
void console_optb_parse(void);

void console_optb_lock(void);
void console_optb_unlock(void);

//------------------------------------------------------------------------------
