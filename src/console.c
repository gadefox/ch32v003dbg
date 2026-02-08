#include <stdio.h>
#include <string.h>
#include <pico/time.h>

#include "boot.h"
#include "break.h"
#include "console.h"
#include "context.h"
#include "flash.h"
#include "options.h"
#include "packet.h"
#include "swio.h"
#include "vendor.h"

//------------------------------------------------------------------------------

static packet pkt;

//------------------------------------------------------------------------------

void console_init(void) {
  packet_init(&pkt, 64);
}

//------------------------------------------------------------------------------

void console_update(uint8_t b) {
  if (b == '\n' || b == '\r') {
    if (packet_terminate(&pkt))
      console_dispatch();

    packet_clear(&pkt);
    return;
 }

  if (!pkt.error && !packet_put(&pkt, b))
    print_r(0, "console: full buffer\n");
}

//------------------------------------------------------------------------------

void console_print_status(bool status) {
  if (status)
    print_g(1, "ok\n");
  else
    print_r(1, "failed\n");
}

//------------------------------------------------------------------------------

void console_print_dpc(bool status) {
  if (status) {
    uint32_t dpc = csr_get_dpc();
    print_g(1, "@ %08X\n", dpc);
  } else
    console_print_status(false);
}

//------------------------------------------------------------------------------

int32_t packet_take_addr(int32_t optional, int32_t total_size) {
  uint8_t* arg = packet_ptr(&pkt);
  int32_t addr = packet_take_arg(&pkt, optional);

  if (pkt.error || (addr  & 3) || addr < 0 || addr > total_size) {
    print_r(1, "bad addr:%s, expected", arg);
    if (addr  & 3)
      print_r(1, "aligned addr\n");
    else if (addr < 0)
      print_r(1, "addr>=0\n");
    else if (addr > total_size)
      print_r(1, "addr<=%u\n", total_size);
    else
      print_r(1, "numeric addr\n");

    return -1;
  }

  return addr;
}

//==============================================================================
// Handlers

#define handler_jump(f)  ((void (*)(void))(f))()

typedef struct {
  const char* name;
  const char* alias;
  const char* args;
  void* func;
} handler;

//------------------------------------------------------------------------------

static void* handler_find(const handler* handlers, uint32_t count) {
  packet_skip_ws(&pkt);

  for (uint32_t i = 0; i < count; i++) {
    const handler* h = &handlers[i];

    if (packet_match_word(&pkt, h->name) ||
        (h->alias && packet_match_word(&pkt, h->alias)))
      return h->func;
  }
  return NULL;
}

//------------------------------------------------------------------------------

static uint32_t handlers_get_maxlens(const handler* handlers, uint32_t count) {
  size_t name = 0, alias = 0;

  for (uint32_t i = 0; i < count; i++) {
    const handler* h = &handlers[i];

    size_t len = strlen(h->name);
    if (name < len)
      name = len;

    if (h->alias) {
      len = strlen(h->alias);
      if (alias < len)
        alias = len;
    }
  }

  return (name << 16) | alias;
}

//------------------------------------------------------------------------------

static void console_dump_handlers(const handler* handlers, uint32_t count, const char* name) {
  print_y(0, name);

  uint32_t maxlens = handlers_get_maxlens(handlers, count);
  size_t max_name = (maxlens >> 16) + 1;
  size_t max_alias = (maxlens & 0xFFFF) + 3;

  for (uint32_t i = 0; i < count; i++) {
    const handler* h = &handlers[i];

    // name
    int padding = max_name - strlen(h->name);
    print_b(2, "%s", h->name);
    printf("%*s", padding, "");

    // alias
    padding = max_alias;
    if (h->alias) {
      putchar('|');
      print_c(1, "%s", h->alias);
      padding -= strlen(h->alias) + 2;
    }
    printf("%*s", padding, "");

    // args
    if (h->args)
      printf("<%s>", h->args);

    putchar('\n');
  }
}

//==============================================================================
// Bootloader handlers

void console_boot_lock(void) {
  print_y(0, "boot: lock\n");

  bool status = boot_lock();
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_boot_unlock(void) {
  print_y(0, "boot: unlock\n");

  bool status = boot_unlock();
  console_print_status(status);
}

//------------------------------------------------------------------------------

const handler boot_handlers[] = {
  { "pico",   "pi", NULL, boot_pico },
  { "lock",   "lo", NULL, console_boot_lock },
  { "unlock", "un", NULL, console_boot_unlock }
};

//------------------------------------------------------------------------------

void console_boot_help(void) {
  console_dump_handlers(boot_handlers, countof(boot_handlers), "boot:\n");
}

//------------------------------------------------------------------------------

void console_boot_parse(void) {
  void* handler = handler_find(boot_handlers, countof(boot_handlers));
  if (!handler)
    console_boot_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Breakpoint handlers

void console_break_halt(void) {
  bool status = break_halt();
  printf("break:halt:");
  console_print_dpc(status);
}

//------------------------------------------------------------------------------

void console_break_resume(void) {
  bool status = break_resume();
  printf("break:resume:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_break_set(void) {
  printf("break:set:");

  int32_t addr = packet_take_addr(-1, INT32_MAX);
  if (addr != -1) {
    bool status = break_set(addr, 2) >= 0;
    console_print_dpc(status);
  }
}

//------------------------------------------------------------------------------

void console_break_clear(void) {
  printf("break:clear:");

  int32_t addr = packet_take_addr(-1, INT32_MAX);
  if (addr != -1) {
    bool status = break_clear(addr, 2) >= 0;
    console_print_status(status);
  }
}

//------------------------------------------------------------------------------

const handler break_handlers[] = {
  { "halt",     "ht", NULL,   console_break_halt },
  { "continue", "co", NULL,   console_break_resume },
  { "set",      "se", "addr", console_break_set },
  { "clear",    "cl", "addr", console_break_clear }
};

//------------------------------------------------------------------------------

void console_break_help(void) {
  console_dump_handlers(break_handlers, countof(break_handlers), "break:\n");
}

//------------------------------------------------------------------------------

void console_break_parse(void) {
  void* handler = handler_find(break_handlers, countof(break_handlers));
  if (!handler)
    console_break_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Flash handlers

//------------------------------------------------------------------------------

void console_flash_erase(void) {
  bool status = flash_erase_chip();
  printf("flash:erase:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_flash_lock(void) {
  print_y(0, "flash:lock\n");

  bool status = flash_lock_fast_prog();
  printf("  fast prog:");
  console_print_status(status);

  status = flash_lock_fpec();
  printf("  controller:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_flash_unlock(void) {
  print_y(0, "flash:unlock\n");

  bool status = flash_unlock_fpec();
  printf("  controller:");
  console_print_status(status);

  status = flash_unlock_fast_prog();
  printf("  fast prog:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

const handler flash_handlers[] = {
  { "erase",   "er", NULL, console_flash_erase },
  { "lock",    "lo", NULL, console_flash_lock },
  { "unlock",  "un", NULL, console_flash_unlock },
  { "patch",   NULL, NULL, break_patch_flash },
  { "unpatch", NULL, NULL, break_unpatch_flash }
};

//------------------------------------------------------------------------------

void console_flash_help(void) {
  console_dump_handlers(flash_handlers, countof(flash_handlers), "flash:\n");
}

//------------------------------------------------------------------------------

void console_flash_parse(void) {
  void* handler = handler_find(flash_handlers, countof(flash_handlers));
  if (!handler)
    console_flash_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Context handlers

void console_ctx_reset(void) {
  bool status = ctx_reset();
  printf("console:reset:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_ctx_halt(void) {
  bool status = ctx_halt();
  printf("console:halt:");
  console_print_dpc(status);
}

//------------------------------------------------------------------------------

void console_ctx_resume(void) {
  bool status = ctx_resume();
  printf("console: resume:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

void console_ctx_step(void) {
  bool status = ctx_step();
  printf("console: step:");
  console_print_status(status);
}

//------------------------------------------------------------------------------

const handler ctx_handlers[] = {
#if LOGS
  { "test",     NULL, NULL, ctx_test },
#endif
  { "reset",    NULL, NULL, console_ctx_reset },
  { "halt",     "ht", NULL, console_ctx_halt },
  { "continue", "co", NULL, console_ctx_resume },
  { "step",     "st", NULL, console_ctx_step }
};

//------------------------------------------------------------------------------

void console_ctx_help(void) {
  console_dump_handlers(ctx_handlers, countof(ctx_handlers), "core:\n");
}

//------------------------------------------------------------------------------

void console_ctx_parse(void) {
  void* handler = handler_find(ctx_handlers, countof(ctx_handlers));
  if (!handler)
    console_ctx_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Help handlers

const handler help_handlers[] = {
  { "boot",    "bo", NULL, console_boot_help },
  { "break",   "br", NULL, console_break_help },
  { "core",    "co", NULL, console_ctx_help },
  { "flash",   "fl", NULL, console_flash_help },
  { "info",    "i",  NULL, console_info_help },
  { "options", "op", NULL, console_optb_help }
};

//------------------------------------------------------------------------------

void console_help_parse(void) {
  void* handler = handler_find(help_handlers, countof(help_handlers));
  if (!handler)
    console_help();
  else
    handler_jump(handler);
}

//------------------------------------------------------------------------------

void console_help(void) {
  console_dump_handlers(help_handlers, countof(help_handlers), "help:\n");
}

//==============================================================================
// Info handlers

const handler info_handlers[] = {
  { "boot",    "bo", NULL,   console_info_boot },
  { "break",   "br", NULL,   break_dump },
  { "core",    "co", NULL,   ctx_dump },
  { "flash",   "fl", "addr", console_info_flash },
  { "options", "op", NULL,   optb_dump },
  { "swio",    "sw", NULL,   swio_dump },
  { "vendor",  "ve", NULL,   vendor_dump }
};

//------------------------------------------------------------------------------

void console_info_boot(void) {
  print_y(0, "boot:dump\n");

  int32_t addr = packet_take_addr(0, BOOT_SIZE);
  if (addr != -1)
    boot_dump(addr);
}

//------------------------------------------------------------------------------

void console_info_flash(void) {
  print_y(0, "flash:dump\n");

  int32_t addr = packet_take_addr(0, CH32_FLASH_SIZE);
  if (addr != -1)
    flash_dump(addr);
}

//------------------------------------------------------------------------------

void console_info_parse(void) {
  void* handler = handler_find(info_handlers, countof(info_handlers));
  if (!handler)
    console_info_help();
  else
    handler_jump(handler);
}

//------------------------------------------------------------------------------

void console_info_help(void) {
  console_dump_handlers(info_handlers, countof(info_handlers), "info:\n");
}

//==============================================================================
// Options handlers

void console_optb_lock(void) {
  print_y(0, "options: lock\n");

  bool status = optb_lock();
  console_print_status(status);
}

void console_optb_unlock(void) {
  print_y(0, "options: unlock\n");

  bool status = optb_unlock();
  console_print_status(status);
}

//------------------------------------------------------------------------------

const handler optb_handlers[] = {
  { "lock",   "lo", NULL, console_optb_lock },
  { "unlock", "un", NULL, console_optb_unlock }
};

//------------------------------------------------------------------------------

void console_optb_help(void) {
  console_dump_handlers(optb_handlers, countof(optb_handlers), "options:\n");
}

//------------------------------------------------------------------------------

void console_optb_parse(void) {
  void* handler = handler_find(optb_handlers, countof(optb_handlers));
  if (!handler)
    console_optb_help();
  else
    handler_jump(handler);
}


//==============================================================================
// Console handlers

const handler console_handlers[] = {
  { "help",    "h",  NULL, console_help_parse },
  { "boot",    "bo", NULL, console_boot_parse },
  { "break",   "br", NULL, console_break_parse },
  { "core",    "co", NULL, console_ctx_parse },
  { "flash",   "fl", NULL, console_flash_parse },
  { "info",    "i",  NULL, console_info_parse },
  { "options", "op", NULL, console_optb_parse }
};

//------------------------------------------------------------------------------

void console_dispatch(void) {
  // Find handler for the entered console command
  void* handler = handler_find(console_handlers, countof(console_handlers));
  if (handler == NULL) {
    // Unknown command → show help
    console_help();
    return;
  }

  // Start execution time measurement used for performance diagnostics and
  // detecting slow or blocking commands
  uint32_t time_a = time_us_32();
  handler_jump(handler);

  // End time measurement, convert to milliseconds;
  uint32_t time_b = time_us_32();
  time_b -= time_a;
  time_b /= 1000;

  // Diagnostic output helps identify slow commands, or performance regressions
  printf("console: command took %d ms\n", time_b);
}

//------------------------------------------------------------------------------
