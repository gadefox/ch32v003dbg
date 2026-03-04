#include <stdio.h>
#include <stdlib.h>
#include <hardware/timer.h>

#include "break.h"
#include "flash.h"
#include "packet.h"
#include "server.h"

//------------------------------------------------------------------------------

typedef enum {
  DISCONNECTED,        // 0
  RUNNING,             // 1
  KILLED,              // 2
  IDLE,                // 3
  RECV_PACKET,         // 4
  RECV_PACKET_ESCAPE,  // 5
  RECV_SUFFIX1,        // 6
  RECV_SUFFIX2,        // 7
  SEND_PREFIX,         // 8
  SEND_PACKET,         // 9
  SEND_PACKET_ESCAPE,  // 10
  SEND_SUFFIX1,        // 11
  SEND_SUFFIX2,        // 12
  SEND_SUFFIX3,        // 13
  RECV_ACK             // 14
} srv_state;

//------------------------------------------------------------------------------

static srv_state state;
static packet send, recv;
static uint8_t send_valid;


static uint8_t *page_cache;
static int page_base;
static uint64_t page_bitmap;

static uint8_t expected_checksum;
static uint8_t checksum;

static uint32_t last_halt;

//------------------------------------------------------------------------------

const char* memory_map = "<?xml version=\"1.0\"?>\
<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">\
<memory-map>\
<memory type=\"flash\" start=\"0x00000000\" length=\"0x4000\">\
<property name=\"blocksize\">64</property>\
</memory>\
<memory type=\"ram\" start=\"0x20000000\" length=\"0x800\"/>\
</memory-map>";

//------------------------------------------------------------------------------

static void server_set_resp(const uint8_t *buf, uint32_t size) {
  packet_put_buf(&send, buf, size);
  send_valid = true;
}

//------------------------------------------------------------------------------

void server_init(void) {
  server_clear();
  packet_init(&send, 256);
  packet_init(&recv, 256);
  page_cache = malloc(CH32_FLASH_PAGE_SIZE);
}

//------------------------------------------------------------------------------

void server_clear(void) {
  page_bitmap = 0;
  page_base = -1;

  for (int i = 0; i < CH32_FLASH_PAGE_SIZE; i++)
    page_cache[i] = 0xFF;
}

//------------------------------------------------------------------------------
/*
At a minimum, a stub is required to support the ‘?’ command to tell GDB the
reason for halting, ‘g’ and ‘G’ commands for register access, and the ‘m’ and
‘M’ commands for memory access. Stubs that only control single-threaded targets
can implement run control with the ‘c’ (continue) command, and if the target
architecture supports hardware-assisted single-stepping, the ‘s’ (step) command.
Stubs that support multi-threading targets should support the ‘vCont’ command.
All other commands are optional.

// The binary data representation uses 7d (ASCII ‘}’) as an escape character.
// Any escaped byte is transmitted as the escape character followed by the
// original character XORed with 0x20. For example, the byte 0x7d would be
// transmitted as the two bytes 0x7d 0x5d. The bytes 0x23 (ASCII ‘#’), 0x24
// (ASCII ‘$’), and 0x7d (ASCII ‘}’) must always be escaped. Responses sent by
// the stub must also escape 0x2a (ASCII ‘*’), so that it is not interpreted
// as the start of a run-length encoded sequence (described next).
*/


//------------------------------------------------------------------------------

typedef void (*handler_fn)(void);

typedef struct {
  const char* name;
  handler_fn func;
} handler;

const handler server_handlers[] = {
  { "?",  server_handle_questionmark },
  { "!",  server_handle_bang },
  { "c",  server_handle_c },
  { "D",  server_handle_D },
  { "g",  server_handle_g },
  { "G",  server_handle_G },
  { "H",  server_handle_H },
  { "k",  server_handle_k },
  { "m",  server_handle_m },
  { "M",  server_handle_M },
  { "p",  server_handle_p },
  { "P",  server_handle_P },
  { "q",  server_handle_q },
  { "s",  server_handle_s },
  { "R",  server_handle_R },
  { "v",  server_handle_v },
  { "z0", server_handle_z0 },
  { "Z0", server_handle_Z0 },
  { "z1", server_handle_z1 },
  { "Z1", server_handle_Z1 }
};

//------------------------------------------------------------------------------

handler_fn server_find_handler(const char *name) {
  for (int i = 0; i < count_of(server_handlers); i++) {
    const handler *h = &server_handlers[i];

    if (!s_cmp(h->name, name))
      return h->func;
  }
  return NULL;
}

//------------------------------------------------------------------------------
// Report why the CPU halted

void server_handle_questionmark(void) {
  //  SIGINT = 2
  packet_expect(&recv, '?');
  server_set_resp("T05", 3);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Enable extended mode

void server_handle_bang(void) {
  packet_expect(&recv, '!');
  server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Continue - "c<addr>"

void server_handle_c(void) {
  uint32_t addr;

  packet_expect(&recv, 'c');

  // Set PC if requested
  addr = packet_take_hex(&recv);
  if (!recv.error)
    csr_set_dpc(addr);

  // If we did not actually resume because we immediately hit a breakpoint,
  // respond with a "hit breakpoint" message. Otherwise we do not reply until
  // the hart stops.

  if (!break_resume(false)) {
    LOG("break: resume: returned false\n");
    server_set_resp("T05", 3);
    state = SEND_PREFIX;
  } else {
    LOG("break: resume: returned true\n");
    state = RUNNING;
  }
}

//------------------------------------------------------------------------------

void server_handle_D(void) {
  LOG("svr:handle:D: need to double check how to implement this");
  CHECK(false);

  packet_expect(&recv, 'D');
  LOG("svr: detaching\n");
  server_set_resp("OK", 2);

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Read general registers

void server_handle_g(void) {
  packet_expect(&recv, 'g');

  if (recv.error)
    server_set_resp("E01", 3);
  else {
    packet_clear(&send);

    for (size_t i = 0; i < gpr_max; i++) {
      uint32_t gpr;
      if (!gpr_get(i, &gpr))
        return;
      packet_put_hex_u32(&send, gpr);
    }

    uint32_t dpc;
    if (!csr_get_dpc(&dpc))
      return;
    packet_put_hex_u32(&send, dpc);
    send_valid = true;
  }

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Write general registers

void server_handle_G(void) {
  packet_expect(&recv, 'G');

  for (size_t i = 0; i < gpr_max; i++) {
    uint32_t gpr = packet_take_hex_digits(&recv, 8);
    gpr_set(i, gpr);
  }

  uint32_t dpc = packet_take_hex_digits(&recv, 8);
  csr_set_dpc(dpc);

  if (recv.error)
    server_set_resp("E01", 3);
  else
    server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Set thread for subsequent operations

void server_handle_H(void) {
  packet_expect(&recv, 'H');
  packet_skip(&recv, 1);

  int thread_id;
  if (packet_match_advance(&recv, '-')) {
    if (packet_match_advance(&recv, '1'))
      thread_id = -1;
    else
      recv.error = true;
  } else
    thread_id = packet_take_hex(&recv);

  if (recv.error)
    server_set_resp("E01", 3);
  else
    server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Kill

void server_handle_k(void) {
  packet_expect(&recv, 'k');
  // 'k' always kills the target and explicitly does _not_ have a reply.
  state = KILLED;
}

//------------------------------------------------------------------------------
// Read memory

void server_handle_m(void) {
  uint32_t buf[256];

  packet_expect(&recv, 'm');
  int src = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  size_t len = packet_take_hex(&recv);

  if (recv.error) {
    LOG_R("handle:m: %x %x - recv.error '%s'\n", src, len, recv.buf);
    server_set_resp(NULL, 0);
    return;
  }

  packet_clear(&send);

  while (len > 0) {
    if (len == 2) {
      uint16_t data;
      if (!ctx_get_mem16(src, &data))
        return;
      packet_put_hex_u16(&send, data);
      src += 2;
      len -= 2;
    } else if (len == 4) {
      uint32_t data;
      if (!ctx_get_mem32(src, &data))
        return;
      packet_put_hex_u32(&send, data);
      src += 4;
      len -= 4;
    } else if (!(src & 3) && len >= 4) {
      int chunk = len & ~3;
      if (chunk > sizeof(buf))
        chunk = sizeof(buf);

      if (!ctx_get_block_aligned(src, buf, count_of(buf)))
        return;
      packet_put_hex_buf(&send, (uint8_t*)buf, chunk);
      src += chunk;
      len -= chunk;
    } else {
      uint8_t data;
      if (!ctx_get_mem8(src, &data))
        return;

      packet_put_hex_u8(&send, data);
      src++;
      len--;
    }
  }

  send_valid = true;
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Write memory

// Does GDB also uses this for flash write? No, I don't think so.

void server_handle_M(void) {
  uint32_t buf[256];

  packet_expect(&recv, 'M');
  uint32_t dst = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  uint32_t len = packet_take_hex(&recv);
  packet_expect(&recv, ':');

  if (recv.error) {
    LOG_R("handle:M: %x %x - recv.error '%s'\n", dst, len, recv.buf);
    server_set_resp(NULL, 0);
    return;
  }

  while (len) {
    if (!(dst & 3) && len >= 4) {
      int chunk = len & ~3;
      if (chunk > sizeof(buf))
        chunk = sizeof(buf);

      packet_take_hex_to_buf(&recv, (uint8_t*)buf, chunk);
      ctx_set_block_aligned(dst, buf, count_of(buf));
      dst += chunk;
      len -= chunk;
    } else {
      uint8_t x = (uint8_t)packet_take_hex_digits(&recv, 8);
      ctx_set_mem8(dst, x);
      dst += 1;
      len -= 1;
    }
  }

  if (recv.error)
    server_set_resp("E01", 3);
  else
    server_set_resp("OK", 2);

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Read the value of register N

void server_handle_p(void) {
  packet_expect(&recv, 'p');
  size_t gpr = packet_take_hex(&recv);

  if (!recv.error) {
    packet_clear(&send);

    uint32_t reg;
    if (gpr < gpr_max) {
      if (!gpr_get(gpr, &reg))
        return;
    } else {
      if (!csr_get_dpc(&reg))
        return;
    }

    packet_put_hex_u32(&send, reg);
    send_valid = true;
  }

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Write the value of register N

void server_handle_P(void) {
  packet_expect(&recv, 'P');
  uint32_t regnum = packet_take_hex(&recv);
  packet_expect(&recv, '=');
  uint32_t value = packet_take_hex(&recv);

  if (!recv.error) {
    if (regnum < gpr_max)
      gpr_set(regnum, value);
    else
      csr_set_dpc(value);

    server_set_resp("OK", 2);
  }

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// > $qRcmd,72657365742030#fc+

void server_handle_q(void) {
  LOG("svr:handle:q: %s\n", packet_ptr(&recv));

  if (packet_match_prefix(&recv, "qAttached")) {
    // Query what GDB is attached to
    // Reply:
    // ‘1’ The remote server attached to an existing process.
    // ‘0’ The remote server created a new process.
    // ‘E NN’ A badly formed request or an error was encountered.
    server_set_resp("1", 1);
  } else if (packet_match_prefix(&recv, "qC")) {
    // -> qC
    // Return current thread ID
    // Reply: ‘QC<thread-id>’
    // can't be -1 or 0, those are special
    server_set_resp("QC1", 3);
  } else if (packet_match_prefix(&recv, "qfThreadInfo")) {
    // Query all active thread IDs
    // can't be -1 or 0, those are special
    server_set_resp("m1", 2);
  } else if (packet_match_prefix(&recv, "qsThreadInfo")) {
    // Query all active thread IDs, continued
    server_set_resp("l", 1);
  } else if (packet_match_prefix(&recv, "qSupported")) {
    // FIXME: we're ignoring the contents of qSupported
    recv.pos = recv.len;
    server_set_resp("PacketSize=32768;qXfer:memory-map:read+", 39);
  } else if (packet_match_prefix(&recv, "qXfer:")) {
    if (packet_match_prefix(&recv, "memory-map:read::")) {
      int offset = packet_take_hex(&recv);
      packet_expect(&recv, ',');
      int length = packet_take_hex(&recv);

      if (recv.error)
        server_set_resp("E00", 3);
      else {
        packet_clear(&send);
        packet_put(&send, 'l');
        packet_put_buf(&send, memory_map, sizeof(memory_map));
        send_valid = true;
      }
    }

    // FIXME: handle other xfer packets
  } else if (packet_match_prefix(&recv, "qRcmd,")) {
    // Monitor command

    // reset in hex
    if (packet_match_prefix_hex(&recv, "reset")) {
      ctx_reset();
      server_set_resp("OK", 2);
    }
  }

  // If we didn't send anything, ignore the command.
  if (!send.len) {
    recv.pos = recv.len;
    server_set_resp(NULL, 0);
  }

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Restart

void server_handle_R(void) {
  packet_expect(&recv, 'R');
  server_set_resp(NULL, 0);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// Step

void server_handle_s(void) {
  packet_expect(&recv, 's');
  ctx_resume(true);
  server_set_resp("T05", 3);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------

void server_handle_v(void) {
  if (packet_match_prefix(&recv, "vFlash")) {
    if (packet_match_prefix(&recv, "Write")) {
      packet_expect(&recv, ':');
      int addr = packet_take_hex(&recv);
      packet_expect(&recv, ':');
      while (recv.pos < recv.len) {
        uint8_t b = packet_take(&recv);
        server_put_cache(addr++, b);
      }
      server_set_resp("OK", 2);
    } else if (packet_match_prefix(&recv, "Done")) {
      server_flush_cache();
      server_set_resp("OK", 2);
    } else if (packet_match_prefix(&recv, "Erase")) {
      packet_expect(&recv, ':');
      int addr = packet_take_hex(&recv);
      packet_expect(&recv, ',');
      int size = packet_take_hex(&recv);
      if (recv.error) {
        LOG_R("bad vFlashErase packet!\n");
        server_set_resp("E00", 3);
      } else {
        server_flash_erase(addr, size);
        server_set_resp("OK", 2);
      }
    }
  } else if (packet_match_prefix(&recv, "vKill")) {
    // FIXME: should reset cpu or something?
    recv.pos = recv.len;
    ctx_reset();
    server_set_resp("OK", 2);
  } else if (packet_match_prefix(&recv, "vMustReplyEmpty"))
    server_set_resp(NULL, 0);
  else {
    recv.pos = recv.len;
    server_set_resp(NULL, 0);
  }

  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------

void server_handle_z0(void) {
  packet_expect_buf(&recv, "z0,");
  uint32_t addr = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  uint32_t kind = packet_take_hex(&recv);

  LOG("svr:handle:z0: %08X %08X\n", addr, kind);
  break_clear(addr);

  server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

void server_handle_Z0(void) {
  packet_expect_buf(&recv, "Z0,");
  uint32_t addr = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  uint32_t kind = packet_take_hex(&recv);

  LOG("svr:handle:Z0: %08X %08X\n", addr, kind);
  break_set(addr);

  server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------
// FIXME: one of these was software and one was hardware. do we care if both are implemented?

void server_handle_z1(void) {
  packet_expect_buf(&recv, "z1,");
  uint32_t addr = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  uint32_t kind = packet_take_hex(&recv);

  LOG("svr:handle:z1: %08X %08X\n", addr, kind);
  break_clear(addr);

  server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

void server_handle_Z1() {
  packet_expect_buf(&recv, "Z1,");
  uint32_t addr = packet_take_hex(&recv);
  packet_expect(&recv, ',');
  uint32_t kind = packet_take_hex(&recv);

  LOG("svr:handle:Z1: %08X %08X\n", addr, kind);
  break_set(addr);

  server_set_resp("OK", 2);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------

void server_flash_erase(uint32_t addr, uint32_t size) {
  // Erases must be page-aligned
  if ((addr % CH32_FLASH_PAGE_SIZE) || (size % CH32_FLASH_PAGE_SIZE)) {
    LOG_R("\nbad vFlashErase: addr %x, size %x\n", addr, size);
    server_set_resp("E00", 3);
    return;
  }

  if (flash_fpec_unlock() || flash_fastprog_unlock())
    return;
  addr += CH32_FLASH_ADDR;

  while (size) {
    if (addr == CH32_FLASH_ADDR && size == CH32_FLASH_SIZE) {
      LOG("erase chip %08X\n", addr);
      flash_erase_chip();
      server_set_resp("OK", 2);
      addr += size;
      size -= size;
    } else if (!(addr % CH32_FLASH_SECTOR_SIZE) && size >= CH32_FLASH_SECTOR_SIZE) {
      LOG("erase sector %08X\n", addr);
      flash_erase_sector(addr);
      addr += CH32_FLASH_SECTOR_SIZE;
      size -= CH32_FLASH_SECTOR_SIZE;
    } else if (!(addr % CH32_FLASH_PAGE_SIZE) && size >= CH32_FLASH_PAGE_SIZE) {
      LOG("erase page %08X\n", addr);
      flash_erase_page(addr);
      addr += CH32_FLASH_PAGE_SIZE;
      size -= CH32_FLASH_PAGE_SIZE;
    } else
      CHECK(false);
  }

  server_set_resp("OK", 2);
}

//------------------------------------------------------------------------------

void server_put_cache(uint32_t addr, uint8_t data) {
  int page_offset = addr % CH32_FLASH_PAGE_SIZE;
  int page_base_n = addr - page_offset;

  if (page_base != page_base_n) {
    if (page_bitmap)
      server_flush_cache();
    page_base = page_base_n;
  }

  if (page_bitmap & (1u << page_offset))
    LOG_R("byte in flash page written multiple times\n");
  else {
    page_cache[page_offset] = data;
    page_bitmap |= (1ull << page_offset);
  }
}

//------------------------------------------------------------------------------

void server_flush_cache(void) {
  if (page_base == -1)
    return;

  if (!page_bitmap) {
    // empty page cache, nothing to do - why did this happen?
    LOG_R("empty page write @%08X\n", page_base);
  } else {
    if (page_bitmap == (uint64_t)~0) {
      // full page write
      LOG("full page write @%08X\n", page_base);
    } else
      LOG("partial page write @%08X, mask %016llx\n", page_base, page_bitmap);

    flash_write_pages(page_base, (uint32_t *)page_cache, CH32_FLASH_PAGE_WORDS);
  }
 
  server_clear();
}

//------------------------------------------------------------------------------

void server_handle_packet(void) {
  handler_fn handler = server_find_handler(recv.buf);
  if (handler) {
    recv.pos = 0;
    packet_clear(&send);
    handler();

#if SERVER_PARANOID
    dm_abstractcs_clear_err();
#endif

    if (recv.error) {
      LOG_R("parse failure for packet!\n");
      server_set_resp("E00", 3);
    } else if (recv.pos != recv.len)
      LOG_R("leftover text in packet - \"%s\"\n", packet_ptr(&recv));
  } else {
    LOG_R("no handler for command %s\n", recv.buf);
    server_set_resp(NULL, 0);
  }

  if (!send_valid)
    LOG_R("not responding to command '%s'\n", recv.buf);
}

//------------------------------------------------------------------------------

void server_on_hit_breakpoint(void) {
  LOG("breaking\n");
  server_set_resp("T05", 3);
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------

bool server_update(bool connected, bool byte_ie, uint8_t byte_in, uint8_t *byte_out) {
  //----------------------------------------
  // Disconnection

  if (!connected) {
    if (state != DISCONNECTED)
      state = DISCONNECTED;
    return false;
  }

  //----------------------------------------
  // Main state machine

  switch (state) {
    case RUNNING:
      if (byte_in == '\x003') {
        // Got a break character from GDB while running.
        LOG("breaking\n");
        ctx_halt();
        server_set_resp("T05", 3);
        state = SEND_PREFIX;
      } else {
        uint32_t now = time_us_32();
        if (now - last_halt > 100000) {  // 100 ms
          last_halt = now;
          if (dm_get_haltsum0()) {
            uint32_t dpc;
            if (!csr_get_dpc(&dpc))
              return false;

            printf("core halted due to breakpoint @%08X\n", dpc);
            ctx_halt();
            server_set_resp("T05", 3);
            state = SEND_PREFIX;
          }
        }
      }
      break;

    case DISCONNECTED:
      state = IDLE;
      break;

    case IDLE:
      if (!byte_ie)
        break;

      // Wait for start char
      if (byte_in == '$') {
        state = RECV_PACKET;
        packet_clear(&recv);
        checksum = 0;
      }
      break;

    case RECV_PACKET:
      if (!byte_ie)
        break;

      // Add bytes to packet until we see the end char
      // Checksum is for the _escaped_ data.
      if (byte_in == '#') {
        expected_checksum = 0;
        state = RECV_SUFFIX1;
      } else if (byte_in == '}') {
        checksum += byte_in;
        state = RECV_PACKET_ESCAPE;
      } else {
        checksum += byte_in;
        packet_put(&recv, byte_in);
      }
      break;

    case RECV_PACKET_ESCAPE:
      if (!byte_ie)
        break;

      checksum += byte_in;
      packet_put(&recv, byte_in ^ 0x20);
      state = RECV_PACKET;
      break;

    case RECV_SUFFIX1:
      if (!byte_ie)
        break;

      expected_checksum = (expected_checksum << 4) | from_hex(byte_in);
      state = RECV_SUFFIX2;
      break;

    case RECV_SUFFIX2:
      if (!byte_ie)
        break;

      expected_checksum = (expected_checksum << 4) | from_hex(byte_in);
      if (expected_checksum != checksum) {
        LOG_R("\npacket transmission error\n");
        LOG_R("expected checksum %02X\n", expected_checksum);
        LOG_R("actual checksum %02X\n", checksum);
        *byte_out = '-';
        state = IDLE;
        return true;
      }
     
      // Packet checksum OK, handle it.
      *byte_out = '+';
#if REMOTE_DUMP
      printf(">> %s\n", recv.buf);
#endif
      server_handle_packet();

      // If handle_packet() changed state, don't change it again.
      if (state == RECV_SUFFIX2)
        state = send_valid ? SEND_PREFIX : IDLE;

      return true;

    case SEND_PREFIX:
#if REMOTE_DUMP
      printf("<< %s\n", send.buf);
#endif
      *byte_out = '$';
      checksum = 0;
      state = send.len ? SEND_PACKET : SEND_SUFFIX1;
      send.pos = 0;
      return true;

    case SEND_PACKET: {
      uint8_t b = send.buf[send.pos];
      if (b == '#' || b == '$' || b == '}' || b == '*') {
        checksum += '}';
        *byte_out = '}';
        state = SEND_PACKET_ESCAPE;
        return true;
      }
      checksum += b;
      *byte_out = b;
      send.pos++;
      if (send.pos == send.len)
        state = SEND_SUFFIX1;
      return true;
    }

    case SEND_PACKET_ESCAPE: {
      uint8_t b = send.buf[send.pos];
      checksum += b ^ 0x20;
      *byte_out = b ^ 0x20;
      state = SEND_PACKET;
      return true;
    }

    case SEND_SUFFIX1:
      *byte_out = '#';
      state = SEND_SUFFIX2;
      return true;

    case SEND_SUFFIX2:
      *byte_out = to_hex((checksum >> 4) & 0xF);
      state = SEND_SUFFIX3;
      return true;

    case SEND_SUFFIX3:
      *byte_out = to_hex((checksum >> 0) & 0xF);
      state = RECV_ACK;
      return true;

    case RECV_ACK:
      if (!byte_ie)
        break;

      if (byte_in == '+')
        state = IDLE;
      else if (byte_in == '-') {
        LOG_R("========================\n");
        LOG_R("========  NACK  ========\n");
        LOG_R("========================\n");
        state = SEND_PACKET;
      } else
        LOG_R("garbage ack char %d '%c'\n", byte_in, byte_in);
      break;
  }

  return false;
}

//------------------------------------------------------------------------------
