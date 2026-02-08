// Standalone Implementation of WCH's SWIO protocol for their CH32V003 chip.

#pragma once

#include <stdint.h>

//------------------------------------------------------------------------------

bool swio_init(void);
bool swio_reset(void);
void swio_dump(void);

uint32_t swio_get(uint32_t addr);
void swio_put(uint32_t addr, uint32_t data);

//==============================================================================
// Debug interface registers

void dm_print(uint8_t reg, uint32_t raw);
const char* dm_to_name(uint8_t reg);

//------------------------------------------------------------------------------
// Data registers, can be used for temporary storage of data

#define DM_DATA_ADDR   0xE0000000
#define DM_DATA0_ADDR  (DM_DATA_ADDR + 0xF4)
#define DM_DATA1_ADDR  (DM_DATA_ADDR + 0xF8)

#define DM_DATA0  0x04
#define DM_DATA1  0x05

inline uint32_t dm_get_data0(void) { return swio_get(DM_DATA0); }
inline void dm_set_data0(uint32_t d) { swio_put(DM_DATA0, d); }

inline uint32_t dm_get_data1(void) { return swio_get(DM_DATA1); }
inline void dm_set_data1(uint32_t d) { swio_put(DM_DATA1, d); }

//------------------------------------------------------------------------------
// Debug module control register

#define DM_CONTROL  0x10

#define DMCN_DMACTIVE         (1 << 0)
#define DMCN_NDMRESET         (1 << 1)
#define DMCN_CLRRESETHALTREQ  (1 << 2)
#define DMCN_SETRESETHALTREQ  (1 << 3)
#define DMCN_CLRKEEPALIVE     (1 << 4)
#define DMCN_SETKEEPALIVE     (1 << 5)
#define DMCN_HASEL            (1 << 26)
#define DMCN_ACKUNAVAIL       (1 << 27)
#define DMCN_ACKHAVERESET     (1 << 28)
#define DMCN_HARTRESET        (1 << 29)
#define DMCN_RESUMEREQ        (1 << 30)
#define DMCN_HALTREQ          (1 << 31)

typedef union {
  uint32_t raw;
  struct {
    uint32_t DMACTIVE        : 1;  //  1
    uint32_t NDMRESET        : 1;  //  2
    uint32_t CLRRESETHALTREQ : 1;  //  3
    uint32_t SETRESETHALTREQ : 1;  //  4
    uint32_t CLRKEEPALIVE    : 1;  //  5
    uint32_t SETKEEPALIVE    : 1;  //  6
    uint32_t HARTSELHI       : 10; // 16
    uint32_t HARTSELLO       : 10; // 26
    uint32_t HASEL           : 1;  // 27
    uint32_t ACKUNAVAIL      : 1;  // 28
    uint32_t ACKHAVERESET    : 1;  // 29
    uint32_t HARTRESET       : 1;  // 30
    uint32_t RESUMEREQ       : 1;  // 31
    uint32_t HALTREQ         : 1;  // 32
  } b;
} dm_control;

void dm_control_dump(dm_control r);
inline void dm_set_control(uint32_t reg) { swio_put(DM_CONTROL, reg); }

inline dm_control dm_get_control(void) {
  dm_control control;
  control.raw = swio_get(DM_CONTROL);
  return control;
}

//------------------------------------------------------------------------------
// Debug module status register

#define DM_STATUS  0x11

#define DMST_VERSION(n)     ((n) & 0xF)
#define DMST_AUTHENTICATED  (1 << 7)
#define DMST_ANYHALTED      (1 << 8)
#define DMST_ALLHALTED      (1 << 9)
#define DMST_ANYRUNNING     (1 << 10)
#define DMST_ALLRUNNING     (1 << 11)
#define DMST_ANYAVAIL       (1 << 12)
#define DMST_ALLAVAIL       (1 << 13)
#define DMST_ANYRESUMEACK   (1 << 16)
#define DMST_ALLRESUMEACK   (1 << 17)
#define DMST_ANYHAVERESET   (1 << 18)
#define DMST_ALLHAVERESET   (1 << 19)

typedef union {
  uint32_t raw;
  struct {
    uint32_t VERSION       : 4;  //  4
    uint32_t PAD0          : 3;  //  7
    uint32_t AUTHENTICATED : 1;  //  8
    uint32_t ANYHALTED     : 1;  //  9
    uint32_t ALLHALTED     : 1;  // 10
    uint32_t ANYRUNNING    : 1;  // 11
    uint32_t ALLRUNNING    : 1;  // 12
    uint32_t ANYAVAIL      : 1;  // 13
    uint32_t ALLAVAIL      : 1;  // 14
    uint32_t PAD1          : 2;  // 16
    uint32_t ANYRESUMEACK  : 1;  // 17
    uint32_t ALLRESUMEACK  : 1;  // 18
    uint32_t ANYHAVERESET  : 1;  // 19
    uint32_t ALLHAVERESET  : 1;  // 20
    uint32_t PAD2          : 12; // 32
  } b;
} dm_status;

void dm_status_dump(dm_status r);

inline dm_status dm_get_status(void) {
  dm_status status;
  status.raw = swio_get(DM_STATUS);
  return status;
}

//------------------------------------------------------------------------------
// Microprocessor status register

#define DM_HARTINFO  0x12

#define DMHI_DATAADDR(n)    ((n) & 0xFFF)
#define DMHI_DATASIZE(n)    (((n) & 0xF) << 12)
#define DMHI_DATAACCESS     (1 << 16)
#define DMHI_NSCRATCH(n)    (((n) & 0xF) << 20)


typedef union {
  uint32_t raw;
  struct {
    uint32_t DATAADDR   : 12; // 12
    uint32_t DATASIZE   : 4;  // 16
    uint32_t DATAACCESS : 1;  // 17
    uint32_t PAD0       : 3;  // 20
    uint32_t NSCRATCH   : 4;  // 24
    uint32_t PAD1       : 8;  // 32
  } b;
} dm_hartinfo;

void dm_hartinfo_dump(dm_hartinfo r);

inline dm_hartinfo dm_get_hartinfo(void) {
  dm_hartinfo hartinfo;
  hartinfo.raw = swio_get(DM_HARTINFO);
  return hartinfo;
}

//------------------------------------------------------------------------------
// Abstract command status register

#define DM_ABSTRACTCS  0x16

#define DMAB_CMDER_SUCCESS  0
#define DMAB_CMDER_ILC_ERR  1
#define DMAB_CMDER_CNS_ERR  2
#define DMAB_CMDER_EXC_ERR  3
#define DMAB_CMDER_HLT_ERR  4
#define DMAB_CMDER_BUS_ERR  5
#define DMAB_CMDER_PAR_ERR  6
#define DMAB_CMDER_OTH_ERR  7

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATACOUNT   : 4;  //  4
    uint32_t PAD0        : 4;  //  8
    uint32_t CMDER       : 3;  // 11
    uint32_t PAD1        : 1;  // 12
    uint32_t BUSY        : 1;  // 13
    uint32_t PAD2        : 11; // 24
    uint32_t PROGBUFSIZE : 5;  // 29
    uint32_t PAD3        : 3;  // 32
  } b;
} dm_abstractcs;

void dm_abstractcs_dump(dm_abstractcs r);
inline void dm_set_abstractcs(uint32_t reg) { swio_put(DM_ABSTRACTCS, reg); }

inline dm_abstractcs dm_get_abstractcs(void) {
  dm_abstractcs abstractcs;
  abstractcs.raw = swio_get(DM_ABSTRACTCS);
  return abstractcs;
}

//------------------------------------------------------------------------------
// Abstract command register

#define DM_COMMAND  0x17

#define DMCM_REGNO_CSR(n)   ((n) & 0xFFF)
#define DMCM_REGNO_GPR(n)   (((n) & 0x1F) | 0x1000)

#define DMCM_WRITE          (1 << 16)
#define DMCM_TRANSFER       (1 << 17)
#define DMCM_POSTEXEC       (1 << 18)
#define DMCM_AARPOSTINC     (1 << 19)

#define DMCM_AARSIZE_8IT    (0 << 20)
#define DMCM_AARSIZE_16BIT  (1 << 20)
#define DMCM_AARSIZE_32BIT  (2 << 20)

typedef union {
  uint32_t raw;
  struct {
    uint32_t REGNO      : 16; // 16
    uint32_t WRITE      : 1;  // 17
    uint32_t TRANSFER   : 1;  // 18
    uint32_t POSTEXEC   : 1;  // 19
    uint32_t AARPOSTINC : 1;  // 20
    uint32_t AARSIZE    : 3;  // 23
    uint32_t PAD0       : 9;  // 32
  } b;
} dm_command;

void dm_command_dump(dm_command r);

inline dm_command dm_get_command(void) {
  dm_command command;
  command.raw = swio_get(DM_COMMAND);
  return command;
}

inline void dm_set_command(uint32_t reg) { swio_put(DM_COMMAND, reg); }

//------------------------------------------------------------------------------
// Abstract command auto-execution

#define DM_ABSTRACTAUTO  0x18

#define DMAA_AUTOEXECDATA(b)  ((b) & 0xFFF)
#define DMAA_AUTOEXECPROG(b)  (((b) & 0xFF) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t AUTOEXECDATA : 12; // 12
    uint32_t PAD0         : 4;  // 16
    uint32_t AUTOEXECPROG : 8;  // 24;  NOTE: V2 series design 8 progbuf, corresponding to bits [23:16]
    uint32_t PAD1         : 8;  // 32
  } b;
} dm_abstractauto;

void dm_abstractauto_dump(dm_abstractauto r);
inline void dm_set_abstractauto(uint32_t reg) { swio_put(DM_ABSTRACTAUTO, reg); }

inline dm_abstractauto dm_get_abstractauto(void) {
  dm_abstractauto abstractauto;
  abstractauto.raw = swio_get(DM_ABSTRACTAUTO);
  return abstractauto;
}

//------------------------------------------------------------------------------
// Instruction cache registers 0-7

#define DM_PROGBUF_MAX  8

#define DM_PROGBUF0  0x20
#define DM_PROGBUF1  0x21
#define DM_PROGBUF2  0x22
#define DM_PROGBUF3  0x23
#define DM_PROGBUF4  0x24
#define DM_PROGBUF5  0x25
#define DM_PROGBUF6  0x26
#define DM_PROGBUF7  0x27

void dm_progbuf_dump(void);
inline uint32_t dm_get_progbuf(uint8_t i) { return swio_get(DM_PROGBUF0 + i); }
inline void dm_set_progbuf(uint8_t i, uint32_t reg) { swio_put(DM_PROGBUF0 + i, reg); }

//------------------------------------------------------------------------------
// Halt status register

#define DM_HALTSUM0  0x40

inline uint32_t dm_get_haltsum0(void) { return swio_get(DM_HALTSUM0); }

//------------------------------------------------------------------------------
// Debug MCU Configuration Register
// FIXME: what was this for?

#define DM_DBGMCU_CR  0x7C0

typedef union {
  uint32_t raw;
  struct {
    uint32_t IWDG_STOP : 1;  //  1
    uint32_t WWDG_STOP : 1;  //  2
    uint32_t PAD0      : 2;  //  4
    uint32_t TIM1_STOP : 1;  //  5
    uint32_t TIM2_STOP : 1;  //  6
    uint32_t PAD1      : 26; // 32
  } b;
} dm_dbgmcu_cr;

//------------------------------------------------------------------------------
// Capability Register

#define DM_CPBR  0x7C

#define CPBR_TDIV(n)           ((n) & 3)
#define CPBR_SOPN(n)           (((n) & 3) << 4)
#define CPBR_CHECKSTA          (1 << 8)
#define CPBR_CMDEXTENSTA       (1 << 9)
#define CPBR_OUTSTA            (1 << 10)
#define CPBR_IOMODE(n)         (((n) & 3) << 11)
#define CPBR_VERSION(n)        (((n) & 0xFFFF) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t TDIV        : 2;  //  2; Clock division factor
    uint32_t PAD0        : 2;  //  4
    uint32_t SOPN        : 2;  //  6; Stop sign factor
    uint32_t PAD1        : 2;  //  8
    uint32_t CHECKSTA    : 1;  //  9; CRC8 checksum function
    uint32_t CMDEXTENSTA : 1;  // 10; Command code extension function
    uint32_t OUTSTA      : 1;  // 11; Output function status
    uint32_t IOMODE      : 2;  // 13; Debug I/O port support mode
    uint32_t PAD2        : 3;  // 16
    uint32_t VERSION     : 16; // 32; Version number
  } b;
} dm_cpbr;

void dm_cpbr_dump(dm_cpbr r);

inline dm_cpbr dm_get_cpbr(void) {
  dm_cpbr cpbr;
  cpbr.raw = swio_get(DM_CPBR);
  return cpbr;
}

//------------------------------------------------------------------------------
// Configuration Register
 
#define DM_CFGR  0x7D

#define DMCF_CHECKEN   (1 << 8)
#define DMCF_CMDEXTEN  (1 << 9)
#define DMCF_OUTEN     (1 << 10)
#define DMCF_KEY(b)    (((b) & 0xFFFF) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t TDIVCFG   : 2;  //  2; Clock division factor
    uint32_t PAD0      : 2;  //  4
    uint32_t SOPNCFG   : 2;  //  6; Stop sign factor
    uint32_t PAD1      : 2;  //  8
    uint32_t CHECKEN   : 1;  //  9; CRC8 checksum function
    uint32_t CMDEXTEN  : 1;  // 10; Command code extension function
    uint32_t OUTEN     : 1;  // 11; Output function status
    uint32_t IOMODECFG : 2;  // 13; Debug I/O port support mode
    uint32_t PAD2      : 3;  // 16
    uint32_t KEY       : 16; // 32; Key register
  } b;
} dm_cfgr;

void dm_cfgr_dump(dm_cfgr r);
inline void dm_set_cfgr(uint32_t reg) { swio_put(DM_CFGR, reg); }

inline dm_cfgr dm_get_cfgr(void) {
  dm_cfgr cfgr;
  cfgr.raw = swio_get(DM_CFGR);
  return cfgr;
}

//------------------------------------------------------------------------------
// Shadow Configuration Register

#define DM_SHDWCFGR  0x7E

#define DMSC_CHECKEN   (1 << 8)
#define DMSC_CMDEXTEN  (1 << 9)
#define DMSC_OUTEN     (1 << 10)
#define DMSC_KEY(b)    (((b) & 0xFFFF) << 16)

typedef union {
  uint32_t raw;
  struct {
    uint32_t TDIVCFG   : 2;  //  2; Clock division factor
    uint32_t PAD0      : 2;  //  4
    uint32_t SOPNCFG   : 2;  //  6; Stop sign factor
    uint32_t PAD1      : 2;  //  8
    uint32_t CHECKEN   : 1;  //  9; CRC8 checksum function
    uint32_t CMDEXTEN  : 1;  // 10; Command code extension function
    uint32_t OUTEN     : 1;  // 11; Output function status
    uint32_t IOMODECFG : 2;  // 13; Debug I/O port support mode
    uint32_t PAD2      : 3;  // 16
    uint32_t KEY       : 16; // 32; Key register
  } b;
} dm_shdwcfgr;

void dm_shdwcfgr_dump(dm_shdwcfgr r);
inline void dm_set_shdwcfgr(uint32_t reg) { swio_put(DM_SHDWCFGR, reg); }

inline dm_shdwcfgr dm_get_shdwcfgr(void) {
  dm_shdwcfgr shwdcfgr;
  shwdcfgr.raw = swio_get(DM_SHDWCFGR);
  return shwdcfgr;
}

//------------------------------------------------------------------------------
// Chip ID, see vnd_chipid

#define DM_CHIPID  0x7F

inline uint32_t dm_get_chipid(void) { return swio_get(DM_CHIPID); }

//------------------------------------------------------------------------------
