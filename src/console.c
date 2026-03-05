#include <stdio.h>
#include <string.h>
#include <pico/time.h>

#include "boot.h"
#include "break.h"
#include "console.h"
#include "flash.h"
#include "option.h"
#include "packet.h"
#include "vendor.h"

//------------------------------------------------------------------------------

static packet pkt;

//------------------------------------------------------------------------------

inline void console_init(void) {
  packet_init(&pkt, 64);
}

//------------------------------------------------------------------------------

static int console_take_value(int optional, size_t size) {
  int value = packet_take_arg(&pkt, optional);

  if (pkt.error || value < 0 || value > size) {
    print_r(2, "bad value:");
    if (pkt.error)
      print_r(1, "expected numeric value\n");
    else
      print_r(1, "out of range (%04X)\n", size);
    return -1;
  }
  return value;
}

//------------------------------------------------------------------------------

static int console_take_addr(int optional, size_t size) {
  int addr = packet_take_arg(&pkt, optional);

  if (pkt.error || (addr & 3) || addr < 0 || addr > size) {
    print_r(2, "bad addr:");
    if (pkt.error)
      print_r(1, "expected numeric value\n");
    else if (addr & 3)
      print_r(1, "unaligned\n");
    else
      print_r(1, "out of range (%04X)\n", size);
    return -1;
  }
  return addr;
}

//------------------------------------------------------------------------------

static void console_get_u32(uint32_t base, size_t size) {
  int addr = console_take_addr(-1, size);
  if (addr == -1)
    return;

  addr += base;

  uint32_t value;
  if (ctx_get_mem32_aligned(addr, &value))
    printf("  %08X: %08X\n", addr, value);
}

//==============================================================================
// Handlers

#define handler_jump(f)  ((void (*)(void))(f))()

typedef struct {
  const char* name;
  const char* alias;
  const char* args;
  void *func;
} handler;

//------------------------------------------------------------------------------

static void *handler_find(const handler *handlers, size_t count) {
  packet_skip_ws(&pkt);

  for (size_t i = 0; i < count; i++) {
    const handler* h = &handlers[i];

    if (packet_match_word(&pkt, h->name) ||
        (h->alias && packet_match_word(&pkt, h->alias)))
      return h->func;
  }
  return NULL;
}

//------------------------------------------------------------------------------

static uint32_t handlers_get_maxlens(const handler *handlers, size_t count) {
  size_t name = 0, alias = 0;

  for (size_t i = 0; i < count; i++) {
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

static void console_dump_handlers(const handler *handlers, size_t count, const char *name) {
  print_y(0, name);

  uint32_t maxlens = handlers_get_maxlens(handlers, count);
  size_t max_name = HIWORD(maxlens) + 1;
  size_t max_alias = LOWORD(maxlens) + 3;

  for (size_t i = 0; i < count; i++) {
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

static void console_boot_info(void) {
  print_y(0, "boot:info\n");
  if (ctx_halted_err("display bootloader bytes")) {
    int addr = console_take_addr(0, BOOT_SIZE - 4);
    if (addr != -1)
      boot_dump(addr);
  }
}

//------------------------------------------------------------------------------

static void console_boot_get(void) {
  print_y(0, "boot:get\n");
  if (ctx_halted_err("read bootloader"))
    console_get_u32(BOOT_ADDR, BOOT_SIZE - 4);
}

//------------------------------------------------------------------------------

static void console_boot_lock(void) {
  print_y(0, "boot:lock\n");
  if (ctx_halted_err("lock bootloader")) {
    bool status = boot_lock() > 0;
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static void console_boot_unlock(void) {
  print_y(0, "boot:unlock\n");
  if (ctx_halted_err("unlock bootloader")) {
    bool status = !boot_unlock();
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static const handler boot_handlers[] = {
  { "info",   "i", NULL,  console_boot_info },
  { "get",    "g", NULL,  console_boot_get },
  { "pico",   "pi", NULL, boot_pico },
  { "lock",   "lo", NULL, console_boot_lock },
  { "unlock", "un", NULL, console_boot_unlock }
};

//------------------------------------------------------------------------------

static void console_boot_help(void) {
  console_dump_handlers(boot_handlers, count_of(boot_handlers), "boot:\n");
}

//------------------------------------------------------------------------------

static void console_boot_parse(void) {
  void *handler = handler_find(boot_handlers, count_of(boot_handlers));
  if (!handler)
    console_boot_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Breakpoint handlers

static void console_break_set(void) {
  print_y(0, "break:set\n");
  if (ctx_halted_err("set breakpoint")) {
    int addr = console_take_addr(-1, CH32_FLASH_SIZE - 2);
    if (addr != -1)
      break_set(addr);
  }
}

//------------------------------------------------------------------------------

static void console_break_clear(void) {
  print_y(0, "break:clear\n");
  if (ctx_halted_err("clear breakpoint")) {
    int addr = console_take_addr(-1, CH32_FLASH_SIZE - 2);
    if (addr != -1)
      break_clear(addr);
  }
}

//------------------------------------------------------------------------------

static void console_break_unpatch(void) {
  print_y(0, "break:unpatch\n");
  if (ctx_halted_err("unpatch flash")) {
    bool status = break_patch(NULL, 0);
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static const handler break_handlers[] = {
  { "info",    "i",  NULL,   break_dump },
  { "clear",   "c",  "addr", console_break_clear },
  { "set",     "s",  "addr", console_break_set },
  { "unpatch", "un", NULL,   console_break_unpatch }
};

//------------------------------------------------------------------------------

static void console_break_help(void) {
  console_dump_handlers(break_handlers, count_of(break_handlers), "break:\n");
}

//------------------------------------------------------------------------------

static void console_break_parse(void) {
  void *handler = handler_find(break_handlers, count_of(break_handlers));
  if (!handler)
    console_break_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Flash handlers

static void console_flash_info(void) {
  print_y(0, "flash:dump\n");
  if (ctx_halted_err("display flash")) {
    int addr = console_take_addr(0, CH32_FLASH_SIZE - 4);
    if (addr != -1)
      flash_dump(addr);
  }
}

//------------------------------------------------------------------------------

static void console_flash_get(void) {
  print_y(0, "flash:get\n");
  if (ctx_halted_err("read flash"))
    console_get_u32(CH32_FLASH_ADDR, CH32_FLASH_SIZE - 4);
}

//------------------------------------------------------------------------------

static void console_flash_erase(void) {
  print_y(0, "flash:erase\n");
  if (ctx_halted_err("erase flash")) {
    bool status = flash_erase_chip();
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static void console_flash_lock(void) {
  print_y(0, "flash:lock\n");
  if (!ctx_halted_err("lock flash"))
    return;

  bool status = flash_fastprog_lock() > 0;
  printf("  fast prog:");
  print_status(status);

  status = flash_fpec_lock() > 0;
  printf("  controller:");
  print_status(status);
}

//------------------------------------------------------------------------------

static void console_flash_unlock(void) {
  print_y(0, "flash:unlock\n");
  if (!ctx_halted_err("unlock flash"))
    return;

  bool status = !flash_fpec_unlock();
  printf("  controller:");
  print_status(status);

  status = !flash_fastprog_unlock();
  printf("  fast prog:");
  print_status(status);
}

//------------------------------------------------------------------------------

static const handler flash_handlers[] = {
  { "info",   "i", "offset", console_flash_info },
  { "get",    "g", "offset", console_flash_get },
  { "erase",  "er", NULL,    console_flash_erase },
  { "lock",   "lo", NULL,    console_flash_lock },
  { "unlock", "un", NULL,    console_flash_unlock }
};

//------------------------------------------------------------------------------

static void console_flash_help(void) {
  console_dump_handlers(flash_handlers, count_of(flash_handlers), "flash:\n");
}

//------------------------------------------------------------------------------

static void console_flash_parse(void) {
  void *handler = handler_find(flash_handlers, count_of(flash_handlers));
  if (!handler)
    console_flash_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Context handlers

static void console_ctx_reset(void) {
  print_y(0, "debug:reset\n");
  bool status = ctx_reset();
  print_status(status);
}

//------------------------------------------------------------------------------

static void console_halted_dpc(bool status) {
  if (status) {
    uint32_t dpc;
    if (!csr_get_dpc(&dpc))
      goto error;
    printf("  halted @%08X\n", dpc);
    return;
  }

error:
  // Failed
  print_status(false);
}

//------------------------------------------------------------------------------

static void console_ctx_halt(void) {
  print_y(0, "debug:halt\n");
  bool status = ctx_halt();
  console_halted_dpc(status);
}

//------------------------------------------------------------------------------

static void console_ctx_resume(void) {
  print_y(0, "debug:resume\n");
  if (ctx_halted_err("resume")) {
    bool status = break_resume(false);
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static void console_ctx_step(void) {
  print_y(0, "debug:step\n");
  if (ctx_halted_err("step")) {
    bool status = break_resume(true);
    console_halted_dpc(status);
  }
}

//------------------------------------------------------------------------------

static const handler ctx_handlers[] = {
  { "info",   "i",  NULL, ctx_dump },
  { "test",  NULL,  NULL, ctx_test },
  { "halt",   "h",  NULL, console_ctx_halt },
  { "reset",  "rs", NULL, console_ctx_reset },
  { "resume", "r",  NULL, console_ctx_resume },
  { "step",   "s",  NULL, console_ctx_step }
};

//------------------------------------------------------------------------------

static void console_ctx_help(void) {
  console_dump_handlers(ctx_handlers, count_of(ctx_handlers), "debug:\n");
}

//------------------------------------------------------------------------------

static void console_ctx_parse(void) {
  void *handler = handler_find(ctx_handlers, count_of(ctx_handlers));
  if (!handler)
    console_ctx_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Info handlers

static const handler info_handlers[] = {
  { "csr",    "cr", NULL,   csr_dump },
  { "gpr",    "gr", NULL,   gpr_dump },
  { "swio",   "sw", NULL,   swio_dump }
};

//------------------------------------------------------------------------------

static void console_info_help(void) {
  console_dump_handlers(info_handlers, count_of(info_handlers), "info:\n");
}

//------------------------------------------------------------------------------

static void console_info_parse(void) {
  void *handler = handler_find(info_handlers, count_of(info_handlers));
  if (!handler)
    console_info_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Option bytes handlers

static void console_option_get(void) {
  uint8_t data[] = { 0xA5, 0x17 };
  optb_write(32, data, 2);
  return;

  print_y(0, "option:get\n");
  if (ctx_halted_err("read option bytes"))
    console_get_u32(OPTB_ADDR, OPTB_SIZE - 4);
}

//------------------------------------------------------------------------------

static void console_option_erase(void) {
  print_y(0, "option:erase\n");
  if (ctx_halted_err("erase option bytes")) {
    bool status = optb_erase();
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static void console_option_write(const char *name, uint32_t addr) {
  print_y(0, "option:write %s\n", name);
  if (!ctx_halted_err("write option bytes"))
    return;

  int value = console_take_value(-1, 255);
  if (value == -1)
    return;

  print_c(0, "-- %d\n", value);
  uint8_t data = (uint8_t)value;
  bool status = optb_write(addr, &data, 1);
  print_status(status);
}

//------------------------------------------------------------------------------

static void console_option_rdpr(void)  { console_option_write("rdpr",   0); }
static void console_option_user(void)  { console_option_write("user",   2); }
static void console_option_data0(void) { console_option_write("data0",  4); }
static void console_option_data1(void) { console_option_write("data1",  6); }
static void console_option_wrpr0(void) { console_option_write("wrpr0",  8); }
static void console_option_wrpr1(void) { console_option_write("wrpr1", 10); }
static void console_option_wrpr2(void) { console_option_write("wrpr2", 12); }
static void console_option_wrpr3(void) { console_option_write("wrpr3", 14); }

//------------------------------------------------------------------------------

static void console_option_lock(void) {
  print_y(0, "option:lock\n");
  if (ctx_halted_err("lock option bytes")) {
    bool status = !optb_lock();
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static void console_option_unlock(void) {
  print_y(0, "option:unlock\n");
  if (ctx_halted_err("unlock option bytes")) {
    bool status = optb_unlock() > 0;
    print_status(status);
  }
}

//------------------------------------------------------------------------------

static const handler option_handlers[] = {
  { "info",   "i",  NULL,     optb_dump },
  { "get",    "g",  "offset", console_option_get },
  { "lock",   "lo", NULL,     console_option_lock },
  { "unlock", "un", NULL,     console_option_unlock },
  { "erase",  "er", NULL,     console_option_erase },
  { "rdpr",   "r",  "byte",   console_option_rdpr },
  { "user",   "u",  "byte",   console_option_user },
  { "data0",  "d0", "byte",   console_option_data0 },
  { "data1",  "d1", "byte",   console_option_data1 },
  { "wrpr0",  "w0", "byte",   console_option_wrpr0 },
  { "wrpr1",  "w1", "byte",   console_option_wrpr1 },
  { "wrpr2",  "w2", "byte",   console_option_wrpr2 },
  { "wrpr3",  "w3", "byte",   console_option_wrpr3 }
};

//------------------------------------------------------------------------------

static void console_option_help(void) {
  console_dump_handlers(option_handlers, count_of(option_handlers), "option:\n");
}

//------------------------------------------------------------------------------

static void console_option_parse(void) {
  void *handler = handler_find(option_handlers, count_of(option_handlers));
  if (!handler)
    console_option_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Vendor handlers

static void console_vendor_get(void) {
  print_y(0, "vendor:get\n");
  if (ctx_halted_err("read vendor bytes"))
    console_get_u32(VNDB_ADDR, VNDB_SIZE - 4);
}

//------------------------------------------------------------------------------

static const handler vendor_handlers[] = {
  { "info", "i", NULL,     vendor_dump },
  { "get",  "g", "offset", console_vendor_get }
};

//------------------------------------------------------------------------------

static void console_vendor_help(void) {
  console_dump_handlers(vendor_handlers, count_of(vendor_handlers), "vendor:\n");
}

//------------------------------------------------------------------------------

static void console_vendor_parse(void) {
  void *handler = handler_find(vendor_handlers, count_of(vendor_handlers));
  if (!handler)
    console_vendor_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Help handlers

static const handler help_handlers[] = {
  { "boot",   "bo", NULL, console_boot_help },
  { "break",  "b",  NULL, console_break_help },
  { "debug",  "d",  NULL, console_ctx_help },
  { "flash",  "fl", NULL, console_flash_help },
  { "info",   "i",  NULL, console_info_help },
  { "option", "op", NULL, console_option_help },
  { "vendor", "ve", NULL, console_vendor_help }
};

//------------------------------------------------------------------------------

static void console_help(void) {
  console_dump_handlers(help_handlers, count_of(help_handlers), "help:\n");
}

//------------------------------------------------------------------------------

static void console_help_parse(void) {
  void *handler = handler_find(help_handlers, count_of(help_handlers));
  if (!handler)
    console_help();
  else
    handler_jump(handler);
}

//==============================================================================
// Console handlers

static const handler console_handlers[] = {
  { "help",   "h",  NULL, console_help_parse },
  { "boot",   "bo", NULL, console_boot_parse },
  { "break",  "b",  NULL, console_break_parse },
  { "debug",  "d",  NULL, console_ctx_parse },
  { "flash",  "fl", NULL, console_flash_parse },
  { "info",   "i",  NULL, console_info_parse },
  { "option", "op", NULL, console_option_parse },
  { "vendor", "ve", NULL, console_vendor_parse }
};

//------------------------------------------------------------------------------

void console_dispatch(void) {
  // Find handler for the entered console command
  void *handler = handler_find(console_handlers, count_of(console_handlers));
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

void console_update(uint8_t b) {
  if (b == '\n' || b == '\r') {
    packet_end(&pkt);
    console_dispatch();
    packet_clear(&pkt);
    return;
 }

  if (!pkt.error && !packet_put(&pkt, b))
    print_r(0, "console: full buffer\n");
}

//------------------------------------------------------------------------------
