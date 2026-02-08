#include <hardware/dma.h>
#include <pico/status_led.h>
#include <pico/time.h>

#include "context.h"
#include "flash.h"
#include "utils.h"
#include "xmodem.h"

//------------------------------------------------------------------------------

#define MAX_RETRIES  10

//------------------------------------------------------------------------------

typedef enum {
  DISCONNECTED,  // 0
  SEND_CRC,      // 1
  RECV_HEADER,   // 2
  RECV_BLK,      // 3
  RECV_BLK_INV,  // 4
  RECV_DATA,     // 5
  RECV_CRC1,     // 6
  RECV_CRC2,     // 7
  VALIDATE,      // 8
  CANCEL         // 9
} xm_state;

//------------------------------------------------------------------------------

bool xmodem_mode;

// XMODEM protocol state
static xm_state state;         // Current XMODEM FSM state

// Packet tracking
static uint8_t blk_idx;        // Block number from sender
static uint8_t blk_inv;        // Bitwise inverted block number (header integrity check)
static uint8_t blk_cur;        // Current block number

// Data handling
static uint8_t data[1024];     // Data payload buffer (XMODEM block)
static uint16_t data_idx;      // Current index into data buffer
static uint16_t data_size;     // XMODEM block size

// CRC handling
static uint16_t crc;           // CRC received from the stream

// Control / flow
static uint8_t retry_cnt;      // Incremented on NAK/timeout, reset on valid block

static int dma_chan;           // DMA channel used for CRC calculation
static uint32_t dst_addr;      // Current flash write address

//------------------------------------------------------------------------------

void xmodem_init(void) {
  static uint8_t dummy;

  // Claim a DMA channel
  dma_chan = dma_claim_unused_channel(true);

  // Configure for memory-to-memory (or memory-to-peripheral)
  dma_channel_config c = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);

  // Enable the sniffer on this channel
  channel_config_set_sniff_enable(&c, true);

  // Set default read address (will be updated per transfer)
  dma_channel_configure(dma_chan, &c, &dummy, data, 0, false);

  // Enable DMA sniffer for CRC-16-CCITT and trigger DMA start
  dma_sniffer_enable(dma_chan, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, false);
}

//------------------------------------------------------------------------------

static int xmodem_validate(void) {
  // Is block index valid?
  if (blk_idx + blk_inv != 0xFF)
    return 0;  // Block mismatch

  // check crc
  uint16_t crc_calc = (uint16_t)dma_sniffer_get_data_accumulator();
  if (crc_calc != crc)
    return 0;  // CRC mismatch

  // check blk index
  if (blk_idx == blk_cur)
    return -1;
  if (blk_idx == blk_cur + 1)
    return 1;

  // Unexpected block
  return 0;
}

//------------------------------------------------------------------------------

static bool erase_flash_verify(void) {
  uint32_t word_count;

  if (data_size == 1024) {
    if (!flash_erase_sector(dst_addr))
      return false;

    // XMODEM-1K maps to one flash sector (16 pages)
    word_count = CH32_FLASH_SECTOR_WORDS;
  } else {
    // Erase 2 pages
    if (!flash_erase_page(dst_addr))
      return false;
    if (!flash_erase_page(dst_addr + CH32_FLASH_PAGE_SIZE))
      return false;

    word_count = CH32_FLASH_PAGE_WORDS * 2;
  }

  // Write and verify pages
  if (!flash_write_pages(dst_addr, (uint32_t*)data, word_count) ||
      !flash_verify_pages(dst_addr, (uint32_t*)data, word_count))
    return false;

  dst_addr += word_count * sizeof(uint32_t);
  return true;
}

//------------------------------------------------------------------------------

static int xmodem_handle_block(void) {
  // Validate received XMODEM block:
  int result = xmodem_validate();
  if (result) {
    // result ==  1 → next expected block received: flash data
    // result == -1 → duplicate block (sender retry): ACK again
    if (result != -1) {
      // Flash CH32V003 firmware
      if (!erase_flash_verify()) {
        cled_set_color(CLED_CYAN);
        state = CANCEL;
        return CAN;
      }

      // Accept block, reset FSM and expect the next block number
      blk_cur++;
    }

    state = RECV_HEADER;
    return ACK;
  }

  // Count retry and abort if limit is reached
  retry_cnt++;
  if (retry_cnt >= MAX_RETRIES) {
    // Too many retries: abort
    cled_set_color(CLED_RED);
    state = CANCEL;
    return CAN;
  }

  // Reject block, reset FSM and expect the same block again
  state = RECV_HEADER;
  return NAK;
}

//------------------------------------------------------------------------------

#if LOGS

uint16_t xmodem_crc_calc(const uint8_t* src, uint32_t size) {
  dma_channel_set_read_addr(dma_chan, src, false);
  dma_sniffer_set_data_accumulator(0);
  dma_channel_set_trans_count(dma_chan, size, true);
  dma_channel_wait_for_finish_blocking(dma_chan);
  return dma_sniffer_get_data_accumulator();
}

#endif // LOGS

//------------------------------------------------------------------------------

static uint8_t xmodem_send_request(void) {
  static uint32_t last;
  uint32_t now = time_us_32();

  // Request transfer start. Sent immediately, then every 3 seconds, up to
  // a maximum of 10 attempts before aborting to avoid indefinite waiting
  if (last != 0 && now - last < 3000000)
    return 0;

  retry_cnt++;
  if (retry_cnt > MAX_RETRIES) {
    cled_set_color(CLED_BLUE);
    state = CANCEL;
    return CAN;
  }

  last = now;
  return 'C';
}

//------------------------------------------------------------------------------

static int xmodem_start(bool byte_ie, uint8_t byte_in) {
  colored_status_led_set_state(false);

  if (!byte_ie)
    return 1;  // Ignore unexpected bytes before SOH/STX; blocks are not active yet.

  if (byte_in == SOH)
    data_size = 128;
  else if (byte_in == STX)
    data_size = 1024;
  else
    return 1;

  if (flash_is_fast_prog_locked()) {
    cled_set_color(CLED_MAGENTA);
    state = CANCEL;
    return -1;
  }

  // Configure DMA transfer length
  dma_channel_set_trans_count(dma_chan, data_size, false);

  dst_addr = CH32_FLASH_ADDR;
  blk_cur = 0;
  return 0;
}

//------------------------------------------------------------------------------

bool xmodem_update(bool connected, bool byte_ie, uint8_t byte_in, uint8_t* byte_out) {
  //----------------------------------------
  // Disconnection

  if (!connected) {
    state = DISCONNECTED;
    return false;
  }

  //----------------------------------------
  // Main state machine

  switch (state) {
    case RECV_DATA:
      if (!byte_ie)
        break;
     
      data[data_idx] = byte_in;
      data_idx++;
      if (data_idx < data_size)
        break;

      // Reset CRC accumulator and trigger DMA
      dma_sniffer_set_data_accumulator(0);
      dma_channel_set_read_addr(dma_chan, data, true);
      state = RECV_CRC1;
      break;

    case RECV_BLK:
      if (byte_ie) {
        blk_idx = byte_in;
        state = RECV_BLK_INV;
      }
      break;

    case RECV_BLK_INV:
      if (byte_ie) {
        blk_inv = byte_in;  // validate later
        state = RECV_DATA;
      }
      break;

    case RECV_CRC1:
      if (byte_ie) {
        crc = byte_in << 8;
        state = RECV_CRC2;
      }
      break;

    case RECV_CRC2:
      if (byte_ie) {
        crc |= byte_in;  // validate later
        state = VALIDATE;
      }
      break;

    case VALIDATE:
      // Wait for a DMA channel calculation to complete
      if (dma_channel_is_busy(dma_chan))
        break;

      *byte_out = xmodem_handle_block();
      return true;

    case DISCONNECTED:
      retry_cnt = 0;
      state = SEND_CRC;
      break;

    case SEND_CRC:
      int ret = xmodem_start(byte_ie, byte_in);
      if (!ret)
        goto header;

      if (ret == -1 ) {  // flash is locked
        *byte_out = CAN;
        return true;
      }

      uint8_t signal = xmodem_send_request();
      if (signal) {
        *byte_out = signal;
        return true;
      }
      break;

    case RECV_HEADER:
      if (!byte_ie)
        break;

      if (byte_in == SOH || byte_in == STX) {
header:
        blk_idx = 0;
        blk_inv = 0;
        data_idx = 0;
        crc = 0;
        state = RECV_BLK;
        break;
      }

      if (byte_in == EOT) {
        cled_set_color(CLED_GREEN);
        *byte_out = ACK;
        goto cancel;
      }
      break;

    case CANCEL:
      *byte_out = CAN;
cancel:
      xmodem_mode = false;
      state = DISCONNECTED;
      return true;
  }

  return false;
}

//------------------------------------------------------------------------------
/*
uint16_t crc16_calc(const uint8_t *data, size_t length) {
  uint16_t crc = 0;
    
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;

    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;  // XMODEM polynomial
      else
        crc <<= 1;
    }
  }
  
  return crc;
}
*/
//------------------------------------------------------------------------------
