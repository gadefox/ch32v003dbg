#include <tusb.h>
#include <hardware/flash.h>

#include "utils.h"

//------------------------------------------------------------------------------

static const tusb_desc_device_t desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,

  // Use Interface Association Descriptor (IAD) for CDC
  // As required by USB Specs IAD's subclass must be common class (2) and
  // protocol must be IAD (1)
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = 0x2E8A, // Raspberry Pi
  .idProduct          = 0x000A, // Pico SDK CDC
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

const uint8_t* tud_descriptor_device_cb(void) { return (uint8_t const *) &desc_device; }

//------------------------------------------------------------------------------

#define CONFIG_TOTAL_LEN   (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define EPNUM_CDC_0_NOTIF  0x81
#define EPNUM_CDC_0_OUT    0x02
#define EPNUM_CDC_0_IN     0x82

static const uint8_t desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute,
  // power in mA
  TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // 1st CDC: Interface number, string index, EP notification address and size,
  // EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
};

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { return desc_fs_configuration; }

//------------------------------------------------------------------------------

static char desc_serials[FLASH_UNIQUE_ID_SIZE_BYTES * 2 + 1];

static const char* desc_strings[] = {
  "Raspberry Pi",      // 1: Manufacturer
  "CH32V003 Debugger", // 2: Product
  desc_serials,        // 3: Serials
  "Board CDC",         // 4: CDC Interface
};

void tud_init_serials(void) {
  uint8_t id[FLASH_UNIQUE_ID_SIZE_BYTES];
  flash_get_unique_id(id);

  for (int i = 0; i < FLASH_UNIQUE_ID_SIZE_BYTES; i++)
    byte_to_hex(id[i], &desc_serials[i * 2]);

  desc_serials[FLASH_UNIQUE_ID_SIZE_BYTES * 2] = '\0';
}

//------------------------------------------------------------------------------

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  static uint16_t desc_str[32];

  if (index == 0) {
    desc_str[0] = 0x0304;
    desc_str[1] = 0x0409;
    return desc_str;
  }

  if (index > countof(desc_strings))
    return NULL;

  // first byte is length (including header), second byte is string type
  const char* str = desc_strings[index - 1];
  int len = strlen(str);
  desc_str[0] = (len * 2 + 2) | (TUSB_DESC_STRING << 8);

  for (int i = 0; i < len; i++)
    desc_str[i + 1] = str[i];

  return desc_str;
}

//------------------------------------------------------------------------------
