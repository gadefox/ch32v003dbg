// WCH-specific debug interface config registers

#pragma once

#include <stdint.h>

#include "context.h"
#include "swio.h"

//==============================================================================
// Electronic Signature

#define UNIID1  (1 << 0)
#define UNIID2  (1 << 1)
#define UNIID3  (1 << 2)

void esig_dump(uint32_t mask);

//------------------------------------------------------------------------------
// Flash capacity register

#define ESIG_FLACAP  0x1FFFF7E0

inline uint16_t esig_get_flacap(void) {
  return ctx_get_mem_u32_aligned(ESIG_FLACAP); }

//------------------------------------------------------------------------------
// UID registers

#define ESIG_UNIID1  0x1FFFF7E8
#define ESIG_UNIID2  0x1FFFF7EC
#define ESIG_UNIID3  0x1FFFF7F0

inline uint32_t esig_get_uniid1(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID1); }
inline uint32_t esig_get_uniid2(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID2); }
inline uint32_t esig_get_uniid3(void) {
  return ctx_get_mem_u32_aligned(ESIG_UNIID3); }

//==============================================================================
// Flash registers (memory-mapped I/O)

//------------------------------------------------------------------------------
// Option bytes

#define FLASH_OPTB  0x1FFFF800

typedef union {
  uint32_t raw;
  struct {
    uint32_t RDPR        : 8;  //  8; Read protection control bit
    uint32_t nRDPR       : 8;  // 16
    uint32_t IWDGSW      : 1;  // 17; Independent Watchdog configuration
    uint32_t PAD0        : 1;  // 18
    uint32_t STANDYRST   : 1;  // 19; System reset control in Standby mode
    uint32_t RST_MODE    : 2;  // 21; PD7 alternate as external pin reset
    uint32_t START_MODE  : 1;  // 22; Power-on startup mode
    uint32_t PAD1        : 2;  // 24
    uint32_t nIWDGSW     : 1;  // 25
    uint32_t PAD2        : 1;  // 26
    uint32_t nSTANDYRST  : 1;  // 27
    uint32_t nRST_MODE   : 2;  // 29
    uint32_t nSTART_MODE : 1;  // 30
    uint32_t PAD3        : 2;  // 32
  } b;
} flash_optb;

void flash_optb_dump(flash_optb r);
inline bool flash_set_optb(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_OPTB, reg); }

inline flash_optb flash_get_optb(void) {
  flash_optb optb;
  optb.raw = ctx_get_mem_u32_aligned(FLASH_OPTB);
  return optb;
}

//------------------------------------------------------------------------------

#define FLASH_UDATA  0x1FFFF804

typedef union {
  uint32_t raw;
  struct {
    uint32_t DATA0       : 8;  //  8
    uint32_t nDATA0      : 8;  // 16
    uint32_t DATA1       : 8;  // 24
    uint32_t nDATA1      : 8;  // 32
  } b;
} flash_udata;

void flash_udata_dump(flash_udata r);
inline bool flash_set_udata(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_UDATA, reg); }

inline flash_udata flash_get_udata(void) {
  flash_udata udata;
  udata.raw = ctx_get_mem_u32_aligned(FLASH_UDATA);
  return udata;
}

//------------------------------------------------------------------------------

#define FLASH_WRPR  0x1FFFF808

typedef union {
  uint32_t raw;
  struct {
    uint32_t WRPR0       : 8;  //  8
    uint32_t nWRPR0      : 8;  // 16
    uint32_t WRPR1       : 8;  // 24
    uint32_t nWRPR1      : 8;  // 32
  } b;
} flash_wrpr;

void flash_wrpr_dump(flash_wrpr r);
inline bool flash_set_wrpr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_WRPR, reg); }

inline flash_wrpr flash_get_wrpr(void) {
  flash_wrpr wrpr;
  wrpr.raw = ctx_get_mem_u32_aligned(FLASH_WRPR);
  return wrpr;
}

//------------------------------------------------------------------------------
// Control register

#define FLASH_ACTLR  0x40022000

typedef union {
  uint32_t raw;
  struct {
    uint32_t LATENCY : 2;  //  2; SYSCLK<=24 MHz: 0x00; SYSCLK<-48 MHz: 0x01
    uint32_t PAD0    : 30; // 32
  } b;
} flash_actlr;

void flash_actlr_dump(flash_actlr r);
inline bool flash_set_actlr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_ACTLR, reg); }

  inline flash_actlr flash_get_actlr(void) {
  flash_actlr actlr;
  actlr.raw = ctx_get_mem_u32_aligned(FLASH_ACTLR);
  return actlr;
}

//------------------------------------------------------------------------------
// FPEC key register

#define FLASH_KEYR  0x40022004

inline bool flash_set_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_KEYR, reg); }

//------------------------------------------------------------------------------
// OBKEY register

#define FLASH_OBKEYR  0x40022008

inline bool flash_set_obkeyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_OBKEYR, reg); }

//------------------------------------------------------------------------------
// Status register

#define FLASH_STATR  0x4002200C

#define STATR_BUSY       (1 << 0)
#define STATR_WRPRTERR   (1 << 4)
#define STATR_EOP        (1 << 5)
#define STATR_MODE       (1 << 14)
#define STATR_BOOT_LOCK  (1 << 15)

typedef union {
  uint32_t raw;
  struct {
    uint32_t BUSY      : 1;  //  1; True if flash busy
    uint32_t PAD0      : 3;  //  4
    uint32_t WRPRTERR  : 1;  //  5; True if the flash was written while locked
    uint32_t EOP       : 1;  //  6; True if flash finished programming
    uint32_t PAD1      : 8;  // 14
    uint32_t MODE      : 1;  // 15; Something to do with boot area flash?
    uint32_t BOOT_LOCK : 1;  // 16; True if boot flash locked
    uint32_t PAD2      : 16; // 32
  } b;
} flash_statr;

void flash_statr_dump(flash_statr r);
inline bool flash_set_statr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_STATR, reg); }

inline flash_statr flash_get_statr(void) {
  flash_statr statr;
  statr.raw = ctx_get_mem_u32_aligned(FLASH_STATR);
  return statr;
}

//------------------------------------------------------------------------------
// Configuration register

#define FLASH_CTLR  0x40022010

#define CTLR_PG       (1 << 0)
#define CTLR_PER      (1 << 1)
#define CTLR_MER      (1 << 2)
#define CTLR_OBG      (1 << 4)
#define CTLR_OBER     (1 << 5)
#define CTLR_STRT     (1 << 6)
#define CTLR_LOCK     (1 << 7)
#define CTLR_OBWRE    (1 << 9)
#define CTLR_ERRIE    (1 << 10)
#define CTLR_EOPIE    (1 << 12)
#define CTLR_FLOCK    (1 << 15)
#define CTLR_FTPG     (1 << 16)
#define CTLR_FTER     (1 << 17)
#define CTLR_BUFLOAD  (1 << 18)
#define CTLR_BUFRST   (1 << 19)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PG      : 1;  //  1; Program enable
    uint32_t PER     : 1;  //  2; Perform sector erase
    uint32_t MER     : 1;  //  3; Perform full erase
    uint32_t PAD0    : 1;  //  4;
    uint32_t OBG     : 1;  //  5; Perform user-selected word programming
    uint32_t OBER    : 1;  //  6; Perform user-selected word erasure
    uint32_t STRT    : 1;  //  7; Start
    uint32_t LOCK    : 1;  //  8; Flash lock status

    uint32_t PAD1    : 1;  //  9
    uint32_t OBWRE   : 1;  // 10; User-selected word write enable
    uint32_t ERRIE   : 1;  // 11; Error status interrupt control
    uint32_t PAD2    : 1;  // 12
    uint32_t EOPIE   : 1;  // 13; EOP interrupt control
    uint32_t PAD3    : 2;  // 15
    uint32_t FLOCK   : 1;  // 16; Fast programming mode lock

    uint32_t FTPG    : 1;  // 17; Fast page programming?
    uint32_t FTER    : 1;  // 18; Fast erase
    uint32_t BUFLOAD : 1;  // 19; Cache data into BUF
    uint32_t BUFRST  : 1;  // 20; BUF reset operation
    uint32_t PAD4    : 12; // 32;
  } b;
} flash_ctlr;

void flash_ctlr_dump(flash_ctlr r);
inline bool flash_set_ctlr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_CTLR, reg); }

inline flash_ctlr flash_get_ctlr(void) {
  flash_ctlr ctlr;
  ctlr.raw = ctx_get_mem_u32_aligned(FLASH_CTLR);
  return ctlr;
}

//------------------------------------------------------------------------------
// Address register

#define FLASH_ADDR  0x40022014

inline bool flash_set_addr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_ADDR, reg); }

//------------------------------------------------------------------------------
// Option byte register

#define FLASH_OBR  0x4002201C

typedef union {
  uint32_t raw;
  struct {
    uint32_t OBERR       : 1; //  1; Unlock OB error
    uint32_t RDPRT       : 1; //  2; Flash read protect flag
    uint32_t IWDG_SW     : 1; //  3; Independent watchdog enable
    uint32_t PAD0        : 1; //  4
    uint32_t STANDBY_RST : 1; //  5; System reset control in Standby mode
    uint32_t CFGRSTT     : 2; //  7; Configuration word reset delay time
    uint32_t PAD1        : 3; // 10
    uint32_t DATA0       : 8; // 18; Data byte 0
    uint32_t DATA1       : 8; // 26; Data byte 1
    uint32_t PAD2        : 6; // 32
  } b;
} flash_obr;

void flash_obr_dump(flash_obr r);

inline flash_obr flash_get_obr(void) {
  flash_obr obr;
  obr.raw = ctx_get_mem_u32_aligned(FLASH_OBR);
  return obr;
}

//------------------------------------------------------------------------------
// Write protection register

#define FLASH_WPR  0x40022020

inline uint32_t flash_get_wpr(void) {
  return ctx_get_mem_u32_aligned(FLASH_WPR); }

//------------------------------------------------------------------------------
// Extended key register

#define FLASH_MODEKEYR  0x40022024

inline bool flash_set_mode_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_MODEKEYR, reg); }

//------------------------------------------------------------------------------
// Unlock BOOT key register

#define FLASH_BOOTKEYR  0x40022028

inline bool flash_set_boot_keyr(uint32_t reg) {
  return ctx_set_mem_u32_aligned(FLASH_BOOTKEYR, reg); }

//==============================================================================
// Debug module registers

void dm_print(uint8_t reg, uint32_t raw);
const char* dm_to_name(uint8_t reg);

//------------------------------------------------------------------------------
// Data registers, can be used for temporary storage of data

#define DM_DATA_ADDR   0xE0000000
#define DM_DATA0_ADDR  (DM_DATA_ADDR + 0xF4)
#define DM_DATA1_ADDR  (DM_DATA_ADDR + 0xF8)

#define DM_DATA0      0x04
#define DM_DATA1      0x05

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

//==============================================================================
// Debug-specific CSRs

//------------------------------------------------------------------------------
// Debug Control and Status Register

#define CSR_DCSR  0x7B0

#define DCSR_STEP           (1 << 2)
#define DCSR_NMIP           (1 << 3)
#define DCSR_MPRVEN         (1 << 4)

#define DCSR_CAUSE_EBREAK   (0 << 6)
#define DCSR_CAUSE_TRIGGER  (1 << 6)
#define DCSR_CAUSE_HALTREQ  (2 << 6)
#define DCSR_CAUSE_STEP     (3 << 6)
#define DCSR_CAUSE_RESET    (4 << 6)

#define DCSR_STOPTIME       (1 << 9)
#define DCSR_STOPCOUNT      (1 << 10)
#define DCSR_STEPIE         (1 << 11)
#define DCSR_EBREAKU        (1 << 12)
#define DCSR_EBREAKS        (1 << 13)
#define DCSR_EBREAKM        (1 << 15)

#define DCSR_XDEBUGVER(v)   (((v) & 0xF) << 28)

typedef union {
  uint32_t raw;
  struct {
    uint32_t PRV       : 2;  //  2
    uint32_t STEP      : 1;  //  3
    uint32_t NMIP      : 1;  //  4
    uint32_t MPRVEN    : 1;  //  5
    uint32_t PAD0      : 1;  //  6
    uint32_t CAUSE     : 3;  //  9
    uint32_t STOPTIME  : 1;  // 10
    uint32_t STOPCOUNT : 1;  // 11
    uint32_t STEPIE    : 1;  // 12
    uint32_t EBREAKU   : 1;  // 13
    uint32_t EBREAKS   : 1;  // 14
    uint32_t PAD1      : 1;  // 15
    uint32_t EBREAKM   : 1;  // 16
    uint32_t PAD3      : 12; // 28
    uint32_t XDEBUGVER : 4;  // 32
  } b;
} csr_dcsr;

void csr_dcsr_dump(csr_dcsr r);

inline csr_dcsr csr_get_dcsr(void) {
  csr_dcsr r;
  r.raw = ctx_get_csr(CSR_DCSR);
  return r;
}

inline void csr_set_dcsr(uint32_t reg) { ctx_set_csr(CSR_DCSR, reg); }

//------------------------------------------------------------------------------
// Debug Program Counter

#define CSR_DPC  0x7B1

inline uint32_t csr_get_dpc(void) { return ctx_get_csr(CSR_DPC); }
inline void csr_set_dpc(uint32_t r) { ctx_set_csr(CSR_DPC, r); }

//------------------------------------------------------------------------------
// Debug Scratch Registers
//
#define CSR_DSCRATCH0  0x7B2
#define CSR_DSCRATCH1  0x7B3

inline uint32_t csr_get_dscratch0(void) { return ctx_get_csr(CSR_DSCRATCH0); }
inline void csr_set_dscratch0(uint32_t r) { ctx_set_csr(CSR_DSCRATCH0, r); }

inline uint32_t csr_get_dscratch1(void) { return ctx_get_csr(CSR_DSCRATCH1); }
inline void csr_set_dscratch1(uint32_t r) { ctx_set_csr(CSR_DSCRATCH1, r); }

//==============================================================================
// Debug interface registers

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
// NOTE: not in doc but appears to be part info

#define DM_PART  0x7F

inline uint32_t dm_get_part(void) { return swio_get(DM_PART); }

//==============================================================================
// General-Purpose Register

#define GPR_ZERO  0   //  x0; hardwired zero
#define GPR_RA    1   //  x1; return address
#define GPR_SP    2   //  x2; stack pointer
#define GPR_GP    3   //  x3; global pointer
#define GPR_TP    4   //  x4; thread pointer
#define GPR_T0    5   //  x5; thread pointer
#define GPR_T1    6   //  x6; thread pointer
#define GPR_T2    7   //  x7; thread pointer
#define GPR_S0    8   //  x8; saved / frame pointer
#define GPR_S1    9   //  x9; saved / frame pointer
#define GPR_A0    10  // x10; function parameter / return
#define GPR_A1    11  // x11; function parameter / return
#define GPR_A2    12  // x12; function parameter
#define GPR_A3    13  // x13; function parameter
#define GPR_A4    14  // x14; function parameter
#define GPR_A5    15  // x15; function parameter
#define GPR_MAX   16

// GPR bitmasks
#define GPRB_ZERO  (1 << GPR_ZERO)
#define GPRB_RA    (1 << GPR_RA)
#define GPRB_SP    (1 << GPR_SP)
#define GPRB_GP    (1 << GPR_GP)
#define GPRB_TP    (1 << GPR_TP)
#define GPRB_T0    (1 << GPR_T0)
#define GPRB_T1    (1 << GPR_T1)
#define GPRB_T2    (1 << GPR_T2)
#define GPRB_S0    (1 << GPR_S0)
#define GPRB_S1    (1 << GPR_S1)
#define GPRB_A0    (1 << GPR_A0)
#define GPRB_A1    (1 << GPR_A1)
#define GPRB_A2    (1 << GPR_A2)
#define GPRB_A3    (1 << GPR_A3)
#define GPRB_A4    (1 << GPR_A4)
#define GPRB_A5    (1 << GPR_A5)

inline uint32_t gpr_get_a0(void) { return ctx_get_gpr(GPR_A0); }
inline void gpr_set_a0(uint32_t reg) { ctx_set_gpr(GPR_A0, reg); }

inline uint32_t gpr_get_a1(void) { return ctx_get_gpr(GPR_A1); }
inline void gpr_set_a1(uint32_t reg) { ctx_set_gpr(GPR_A1, reg); }

inline uint32_t gpr_get_a2(void) { return ctx_get_gpr(GPR_A2); }
inline void gpr_set_a2(uint32_t reg) { ctx_set_gpr(GPR_A2, reg); }

inline uint32_t gpr_get_a3(void) { return ctx_get_gpr(GPR_A3); }
inline void gpr_set_a3(uint32_t reg) { ctx_set_gpr(GPR_A3, reg); }

inline uint32_t gpr_get_a4(void) { return ctx_get_gpr(GPR_A4); }
inline void gpr_set_a4(uint32_t reg) { ctx_set_gpr(GPR_A4, reg); }

inline uint32_t gpr_get_a5(void) { return ctx_get_gpr(GPR_A5); }
inline void gpr_set_a5(uint32_t reg) { ctx_set_gpr(GPR_A5, reg); }

//------------------------------------------------------------------------------
