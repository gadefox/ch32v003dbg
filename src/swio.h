// Standalone Implementation of WCH's SWIO protocol for their CH32V003 chip.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "def.h"

//------------------------------------------------------------------------------

bool swio_init(void);
void swio_dump(void);

uint32_t swio_get(uint8_t addr);
void swio_put(uint8_t addr, uint32_t data);

bool swio_reset(void);
bool swio_halt(void);
bool swio_resume(void);
bool swio_step(void);

//==============================================================================
// Debug interface registers

#define PIO_ADDR(x)  LOBYTE(~(x) << 1)

typedef enum {
  DMPM_USER       = 0,
  DMPM_SUPERVISOR = 1,
  DMPM_MACHINE    = 3
} dm_privmode;

typedef enum {
  DMTDIV1,
  DMTDIV2
} dm_tdiv;

typedef enum {
  DMSOPN8,
  DMSOPN16,
  DMSOPN32,
  DMSOPN64
} dm_sopn;

typedef enum {
  DMIO_FREE,
  DMIO_FREE_SINGLE,
  DMIO_FREE_SINGLE_DUAL,
  DMIO_FREE_SINGLE_DUAL_QUAD
} dm_iomode;

//------------------------------------------------------------------------------

const char *dm_str(uint8_t addr);
void dm_print(uint8_t addr, uint32_t raw);

//------------------------------------------------------------------------------
// Data registers, can be used for temporary storage of data

extern uint16_t dm_data_addr;

#define DM_DATA_BASE  0xE0000000
#define DM_DATA_ADDR  (DM_DATA_BASE + dm_data_addr)

#define DM_DATA0  PIO_ADDR(0x04)             // 0xF6
#define DM_DATA1  PIO_ADDR(0x05)             // 0xF4

#define DM_DATA_MAX  12
#define DM_DATA(n)   (DM_DATA0 - (n) * 2)

void dm_data_dump(uint8_t count);

inline void dm_set_data0(uint32_t value) { swio_put(DM_DATA0, value); }
inline void dm_set_data1(uint32_t value) { swio_put(DM_DATA1, value); }

inline void dm_clear_data0(void) { dm_set_data0(0); }
inline void dm_clear_data1(void) { dm_set_data1(0); }

inline uint32_t dm_get_data(uint8_t i) {
  return swio_get(DM_DATA(i)); }
inline uint32_t dm_get_data0(void) { return dm_get_data(0); }
inline uint32_t dm_get_data1(void) { return dm_get_data(1); }

//------------------------------------------------------------------------------
// Debug module control register

#define DM_CONTROL  PIO_ADDR(0x10)  // 0xDE

#define DMC_ACTIVE        (1u << 0)
#define DMC_NDMRESET      (1u << 1)
#define DMC_ACKHAVERESET  (1u << 28)
#define DMC_RESUMEREQ     (1u << 30)
#define DMC_HALTREQ       (1u << 31)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DMACTIVE        : 1;  // [0]
    uint32_t NDMRESET        : 1;  // [1]
    uint32_t PAD9            : 26; // [27:2]
    uint32_t ACKHAVERESET    : 1;  // [28]
    uint32_t PAD1            : 1;  // [20]
    uint32_t RESUMEREQ       : 1;  // [30]
    uint32_t HALTREQ         : 1;  // [31]
  } b;
} dm_control;

_Static_assert(sizeof(dm_control) == 4, "dm_control");

void dm_control_dump(dm_control r);

inline void dm_set_control(uint32_t value) { swio_put(DM_CONTROL, value); }

inline dm_control dm_get_control(void) {
  dm_control control;
  control.raw = swio_get(DM_CONTROL);
  return control; }

//------------------------------------------------------------------------------
// Debug module status register

#define DM_STATUS  PIO_ADDR(0x11)  // 0xDC

#define DMS_VERSION(n)     ((n) & 0xF)
#define DMS_AUTHENTICATED  (1u << 7)
#define DMS_ANYHALTED      (1u << 8)
#define DMS_ALLHALTED      (1u << 9)
#define DMS_ANYRUNNING     (1u << 10)
#define DMS_ALLRUNNING     (1u << 11)
#define DMS_ANYAVAIL       (1u << 12)
#define DMS_ALLAVAIL       (1u << 13)
#define DMS_ANYRESUMEACK   (1u << 16)
#define DMS_ALLRESUMEACK   (1u << 17)
#define DMS_ANYHAVERESET   (1u << 18)
#define DMS_ALLHAVERESET   (1u << 19)

typedef union {
  uint32_t raw;
  struct {
    uint32_t VERSION       : 4;  // [3:0]
    uint32_t PAD0          : 3;  // [6:4]
    uint32_t AUTHENTICATED : 1;  // [7]
    uint32_t ANYHALTED     : 1;  // [8]
    uint32_t ALLHALTED     : 1;  // [9]
    uint32_t ANYRUNNING    : 1;  // [10]
    uint32_t ALLRUNNING    : 1;  // [11]
    uint32_t ANYAVAIL      : 1;  // [12]
    uint32_t ALLAVAIL      : 1;  // [13]
    uint32_t PAD1          : 2;  // [15:14]
    uint32_t ANYRESUMEACK  : 1;  // [16]
    uint32_t ALLRESUMEACK  : 1;  // [17]
    uint32_t ANYHAVERESET  : 1;  // [18]
    uint32_t ALLHAVERESET  : 1;  // [19]
    uint32_t PAD2          : 12; // [31:20]
  } b;
} dm_status;

_Static_assert(sizeof(dm_status) == 4, "dm_status");

void dm_status_dump(dm_status r);

inline dm_status dm_get_status(void) {
  dm_status status;
  status.raw = swio_get(DM_STATUS);
  return status; }

bool dm_status_wait(uint32_t mask, uint32_t value);

//------------------------------------------------------------------------------
// Microprocessor status register

#define DM_HARTINFO  PIO_ADDR(0x12)  // 0xDA

#define DMH_DATAADDR(n)  ((n) & 0xFFF)
#define DMH_DATASIZE(n)  (((n) & 0xF) << 12)
#define DMH_DATAACCESS   (1u << 16)
#define DMH_NSCRATCH(n)  (((n) & 0xF) << 20)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATAADDR   : 12; // [11:0]
    uint32_t DATASIZE   : 4;  // [15:12]
    uint32_t DATAACCESS : 1;  // [16]
    uint32_t PAD0       : 3;  // [19:17]
    uint32_t NSCRATCH   : 4;  // [23:20]
    uint32_t PAD1       : 8;  // [31:24]
  } b;
} dm_hartinfo;

_Static_assert(sizeof(dm_hartinfo) == 4, "dm_hartinfo");

void dm_hartinfo_dump(dm_hartinfo r);

inline dm_hartinfo dm_get_hartinfo(void) {
  dm_hartinfo hartinfo;
  hartinfo.raw = swio_get(DM_HARTINFO);
  return hartinfo; }

//------------------------------------------------------------------------------
// Abstract command status register

#define DM_ABSTRACTCS  PIO_ADDR(0x16)  // 0xD2

typedef enum {
  CMDER_SUCCESS,
  CMDER_ILLEGAL,
  CMDER_UNSUPPORTED,
  CMDER_EXEC,
  CMDER_UNAVAILABLE,
  CMDER_BUS,
  CMDER_PARITY,
  CMDER_OTHER
} dm_cmder_t;

void dm_cmder_dump(dm_cmder_t cmder, bool print_name);

#define DMA_DATACOUNT(c)    ((c) & 0xF)
#define DMA_CMDER(e)        (((e) & 0b111) << 8)
#define DMA_BUSY            (1u << 12)
#define DMA_PROGBUFSIZE(s)  (((s) & 0x1F) << 24)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATACOUNT   : 4;  // [3:0]
    uint32_t PAD0        : 4;  // [7:4]
    uint32_t CMDER       : 3;  // [10:8]
    uint32_t PAD1        : 1;  // [11]
    uint32_t BUSY        : 1;  // [12]
    uint32_t PAD2        : 11; // [23:13]
    uint32_t PROGBUFSIZE : 5;  // [28:24]
    uint32_t PAD3        : 3;  // [31:29]
  } b;
} dm_abstractcs;

_Static_assert(sizeof(dm_abstractcs) == 4, "dm_abstractcs");

void dm_abstractcs_dump(dm_abstractcs r);

inline void dm_set_abstractcs(uint32_t value) { swio_put(DM_ABSTRACTCS, value); }

inline dm_abstractcs dm_get_abstractcs(void) {
  dm_abstractcs abstractcs;
  abstractcs.raw = swio_get(DM_ABSTRACTCS);
  return abstractcs; }

void dm_abstractcs_clear_err(void);
bool dm_abstractcs_wait(void);

//------------------------------------------------------------------------------
// Abstract command register

typedef enum {
  DM_ACCESS_REG,
  DM_ACCESS_QUICK,
  DM_ACCESS_MEM
} dm_cmdtype;

typedef enum {
  AARSIZE8,
  AARSIZE16,
  AARSIZE32,
  AARSIZE64,
  AARSIZE128
} dm_aarsize;

#define DM_COMMAND  PIO_ADDR(0x17)  // 0xD0

#define DMCM_FPR         (1u << 5)
#define DMCM_GPR         (1u << 12)
#define DMCM_WRITE       (1u << 16)
#define DMCM_TRANSFER    (1u << 17)
#define DMCM_POSTEXEC    (1u << 18)
#define DMCM_AARPOSTINC  (1u << 19)
#define DMCM_AARSIZE(s)  (AARSIZE##s << 20)

typedef union {
  uint32_t raw;
  struct {
    uint32_t REGNO      : 16; // [15:0]
    uint32_t WRITE      : 1;  // [16]
    uint32_t TRANSFER   : 1;  // [17]
    uint32_t POSTEXEC   : 1;  // [18]
    uint32_t AARPOSTINC : 1;  // [19]
    uint32_t AARSIZE    : 3;  // [22:20]
    uint32_t PAD0       : 1;  // [23]
    uint32_t CMDTYPE    : 8;  // [31:24]
  } b;
} dm_command;

_Static_assert(sizeof(dm_command) == 4, "dm_command");

void dm_command_dump(dm_command r);

inline void dm_set_command(uint32_t value) { swio_put(DM_COMMAND, value); }

inline dm_command dm_get_command(void) {
  dm_command command;
  command.raw = swio_get(DM_COMMAND);
  return command;
}

//------------------------------------------------------------------------------
// Abstract command auto-execution

#define DM_ABSTRACTAUTO  PIO_ADDR(0x18)  // 0xCE

#define DMAA_DATA(n)  (1 << (n))
#define DMAA_DATA0    DMAA_DATA(0)
#define DMAA_DATA1    DMAA_DATA(1)

#define DMAA_PROGBUF(n)  (1 << ((n) + 16))
#define DMAA_PROGBUF0    DMAA_PROGBUF(0)
#define DMAA_PROGBUF1    DMAA_PROGBUF(1)
#define DMAA_PROGBUF2    DMAA_PROGBUF(2)
#define DMAA_PROGBUF3    DMAA_PROGBUF(3)
#define DMAA_PROGBUF4    DMAA_PROGBUF(4)
#define DMAA_PROGBUF5    DMAA_PROGBUF(5)
#define DMAA_PROGBUF6    DMAA_PROGBUF(6)
#define DMAA_PROGBUF7    DMAA_PROGBUF(7)

typedef union {
  uint32_t raw;
  struct {
    uint32_t AUTOEXECDATA    : 12; // [11:0]
    uint32_t PAD             : 4;  // [15:12]
    uint32_t AUTOEXECPROGBUF : 16; // [31:16]
  } b;
} dm_abstractauto;

_Static_assert(sizeof(dm_abstractauto) == 4, "dm_abstractauto");

void dm_abstractauto_dump(dm_abstractauto r, uint8_t dcount, uint8_t pbcount);

inline void dm_set_abstractauto(uint32_t value) {
  swio_put(DM_ABSTRACTAUTO, value); }

inline dm_abstractauto dm_get_abstractauto(void) {
  dm_abstractauto abstractauto;
  abstractauto.raw = swio_get(DM_ABSTRACTAUTO);
  return abstractauto; }

//------------------------------------------------------------------------------
// Instruction cache registers 0-7

#define DM_PROGBUFMAX  16  // RISCV-debug spec
#define DM_PROGBUF(n)  (PIO_ADDR(0x20) - (n) * 2)

#define DM_PROGBUF0  DM_PROGBUF(0)  // 0xBE
#define DM_PROGBUF1  DM_PROGBUF(1)  // 0xBC
#define DM_PROGBUF2  DM_PROGBUF(2)  // 0xBA
#define DM_PROGBUF3  DM_PROGBUF(3)  // 0xB8
#define DM_PROGBUF4  DM_PROGBUF(4)  // 0xB6
#define DM_PROGBUF5  DM_PROGBUF(5)  // 0xB4
#define DM_PROGBUF6  DM_PROGBUF(6)  // 0xB2
#define DM_PROGBUF7  DM_PROGBUF(7)  // 0xB0

void dm_progbuf_dump(size_t count);

inline void dm_set_progbuf(uint8_t i, uint32_t value) { swio_put(DM_PROGBUF(i), value); }
inline uint32_t dm_get_progbuf(uint8_t i) { return swio_get(DM_PROGBUF(i)); }

//------------------------------------------------------------------------------
// Halt status register

#define DM_HALTSUM0  PIO_ADDR(0x40)  // 0x7E

inline bool dm_get_haltsum0(void) { return swio_get(DM_HALTSUM0); }

//------------------------------------------------------------------------------
// Capability Register

#define DM_CPBR  PIO_ADDR(0x7C)  // 6

#define DMCP_TDIV         (3u << 0)
#define DMCP_SOPN(n)      (((n) & 3) << 4)
#define DMCP_CHECKSTA     (1u << 8)
#define DMCP_CMDEXTENSTA  (1u << 9)
#define DMCP_OUTSTA       (1u << 10)
#define DMCP_IOMODE(n)    (((n) & 3) << 11)
#define DMCP_VERSION(n)   ((n) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t TDIV        : 2;  // [1:0]    Clock division factor
    uint32_t PAD0        : 2;  // [3:2]
    uint32_t SOPN        : 2;  // [5:4]    Stop sign factor
    uint32_t PAD1        : 2;  // [7:6]
    uint32_t CHECKSTA    : 1;  // [8]      CRC8 checksum function
    uint32_t CMDEXTENSTA : 1;  // [9]      Command code extension function
    uint32_t OUTSTA      : 1;  // [10]     Output function status
    uint32_t IOMODE      : 2;  // [12:11]  Debug I/O port support mode
    uint32_t PAD2        : 3;  // [15:13]
    uint32_t VERSION     : 16; // [31:16]  Version number
  } b;
} dm_cpbr;

_Static_assert(sizeof(dm_cpbr) == 4, "dm_cpbr");

void dm_cpbr_dump(dm_cpbr r);

inline dm_cpbr dm_get_cpbr(void) {
  dm_cpbr cpbr;
  cpbr.raw = swio_get(DM_CPBR);
  return cpbr; }

//------------------------------------------------------------------------------
// Configuration Register
 
#define DM_CFGR  PIO_ADDR(0x7D)  // 4

#define DMCF_CHECKEN   (1u << 8)
#define DMCF_CMDEXTEN  (1u << 9)
#define DMCF_OUTEN     (1u << 10)
#define DMCF_KEY(b)    (LOWORD(b) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t TDIVCFG   : 2;  // [1:0]    Clock division factor
    uint32_t PAD0      : 2;  // [3:2]
    uint32_t SOPNCFG   : 2;  // [5:4]    Stop sign factor
    uint32_t PAD1      : 2;  // [7:6]
    uint32_t CHECKEN   : 1;  // [8]      CRC8 checksum function
    uint32_t CMDEXTEN  : 1;  // [9]      Command code extension function
    uint32_t OUTEN     : 1;  // [10]     Output function status
    uint32_t IOMODECFG : 2;  // [12:11]  Debug I/O port support mode
    uint32_t PAD2      : 3;  // [15:13]
    uint32_t KEY       : 16; // [31:16]  Key register
  } b;
} dm_cfgr;

_Static_assert(sizeof(dm_cfgr) == 4, "dm_cfgr");

void dm_cfgr_dump(uint8_t addr, dm_cfgr r);

inline void dm_set_cfgr(uint32_t value) { swio_put(DM_CFGR, value); }

inline dm_cfgr dm_get_cfgr(void) {
  dm_cfgr cfgr;
  cfgr.raw = swio_get(DM_CFGR);
  return cfgr; }

//------------------------------------------------------------------------------
// Shadow Configuration Register

#define DM_SHDWCFGR  PIO_ADDR(0x7E)  // 2

inline void dm_set_shdwcfgr(uint32_t value) {
  swio_put(DM_SHDWCFGR, value); }

inline dm_cfgr dm_get_shdwcfgr(void) {
  dm_cfgr cfgr;
  cfgr.raw = swio_get(DM_SHDWCFGR);
  return cfgr; }

//------------------------------------------------------------------------------
// Chip ID, see vnd_chipid

#define DM_CHIPID  PIO_ADDR(0x7F)

inline uint32_t dm_get_chipid(void) { return swio_get(DM_CHIPID); }

//------------------------------------------------------------------------------
