#include <pico/bootrom.h>

#include "boot.h"
#include "flash.h"

//------------------------------------------------------------------------------

bool boot_is_locked(void) {
  flash_statr statr = flash_get_statr();
  return statr.b.BOOT_LOCK;
}

//------------------------------------------------------------------------------

bool boot_lock(void) {
  // Lock BOOT area
  flash_statr statr = flash_get_statr();
  if (!flash_set_statr(statr.raw | STATR_BOOT_LOCK))
    return false;

  CHECK(flash_is_locked());
  return true;
}

//------------------------------------------------------------------------------

bool boot_unlock(void) {
  // Unlock BOOT area
  if (!boot_set_keyr(UNLOCK_KEY1) || !boot_set_keyr(UNLOCK_KEY2))
    return false;

  CHECK(!flash_is_locked());
  return true;
}

//------------------------------------------------------------------------------
// Dumps flash regs and the first 768 bytes of bootloader area.

void boot_dump(uint32_t addr) {
  ctx_dump_block(addr, BOOT_ADDR, BOOT_SIZE);
}

//------------------------------------------------------------------------------

void boot_pico(void) {
  reset_usb_boot(0, 0);
}

//------------------------------------------------------------------------------
