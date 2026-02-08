#pragma once

//------------------------------------------------------------------------------

// Board Specific

#define BOARD_TUD_RHPORT 0

// Target MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040

// Bare-metal (no RTOS)
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

// Enable USB Device stack
#define CFG_TUD_ENABLED 1

// USB speed (RP2040 supports Full Speed only)
#define CFG_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED

// Control endpoint (EP0) packet size
#define CFG_TUD_ENDPOINT0_SIZE 64

//------------------------------------------------------------------------------
// Enabled USB classes

// CDC (USB serial)
#define CFG_TUD_CDC 1

// Disable unused classes
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

//------------------------------------------------------------------------------
// CDC buffers

// Software FIFO sizes
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64

// Endpoint transfer buffer size
#define CFG_TUD_CDC_EP_BUFSIZE 64

//------------------------------------------------------------------------------

void tud_init_serials(void);

//------------------------------------------------------------------------------
