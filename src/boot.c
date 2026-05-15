#include <pico/bootrom.h>

#include "boot.h"
#include "flash.h"
#include "utils.h"

//------------------------------------------------------------------------------

int boot_locked(void) {
  flash_statr statr;
  if (!flash_get_statr(&statr))
    return -1;
  return statr.raw & STATR_BOOT_LOCK;
}

//------------------------------------------------------------------------------

int boot_lock(void) {
  // Lock BOOT area
  flash_statr statr;
  if (!flash_get_statr(&statr))
    return -1;
  if (!flash_set_statr(statr.raw | STATR_BOOT_LOCK))
    return -1;
  return boot_locked();
}

//------------------------------------------------------------------------------

int boot_unlock(void) {
  // Unlock BOOT area
  if (!boot_set_keyr(UNLOCK_KEY1) || !boot_set_keyr(UNLOCK_KEY2))
    return -1;
  return boot_locked();
}

//------------------------------------------------------------------------------
// Dumps flash regs and the first 768 bytes of bootloader area.

void boot_dump(uint32_t addr) {
  flash_statr statr;
  if (flash_get_statr(&statr))
    print_lock(statr.b.BOOT_LOCK);

  ctx_dump_block(addr, BOOT_ADDR, BOOT_SIZE);
}

//------------------------------------------------------------------------------

void boot_pico(void) {
  reset_usb_boot(0, 0);
}

//------------------------------------------------------------------------------
