#pragma once

//------------------------------------------------------------------------------

#define LOWORD(x)  ((x) & 0xFFFF)
#define HIWORD(x)  ((x) >> 16)

#define LOBYTE(x)  ((x) & 0xFF)
#define HIBYTE(x)  ((x) >> 8)

#define SOH  0x01  // Start Of Header (128-byte block)
#define STX  0x02  // Start Of TeXt (1K-byte block)
#define EOT  0x04  // End Of Transmission
#define ACK  0x06  // Acknowledge
#define NAK  0x15  // Negative Acknowledge
#define SYN  0x16  // Synchronous Idle
#define CAN  0x18  // Cancel

//------------------------------------------------------------------------------

#define LOGS  0

#define BREAK_DUMP   1
#define GPR_DUMP     0
#define PROG_DUMP    0
#define SWIO_DUMP    0
#define REMOTE_DUMP  0

#define DUMP_WORDS  (8*24)

//------------------------------------------------------------------------------
