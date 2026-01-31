#include <stdio.h>

#include "reg.h"
#include "utils.h"

//------------------------------------------------------------------------------
// Electronic Signature

void esig_dump(uint32_t mask) {
  if (mask & UNIID1) {
    uint32_t uniid1 = esig_get_uniid1();
    print_hex(2, "1", uniid1);
  }
  if (mask & UNIID2) {
    uint32_t uniid2 = esig_get_uniid2();
    print_hex(2, "2", uniid2);
  }
  if (mask & UNIID3) {
    uint32_t uniid3 = esig_get_uniid3();
    print_hex(2, "3", uniid3);
  }
}

//------------------------------------------------------------------------------
// Flash registers (memory-mapped I/O)

void flash_optb_dump(flash_optb r) {
  print_b(0, "OPTB\n");
  printf("  %08X\n", r.raw);
  printf("  IWDGSW:%d   RDPR:%02X   RST_MODE:%d   STANDYRST:%d   START_MODE:%d\n",
         r.b.IWDGSW, r.b.RDPR, r.b.RST_MODE, r.b.STANDYRST, r.b.START_MODE);
  printf(" nIWDGSW:%d  nRDPR:%02X  nRST_MODE:%d  nSTANDYRST:%d  nSTART_MODE:%d\n",
         r.b.nIWDGSW, r.b.nRDPR, r.b.nRST_MODE, r.b.nSTANDYRST, r.b.nSTART_MODE);
}

void flash_udata_dump(flash_udata r) {
  print_b(0, "DATA\n");
  printf("  %08X\n", r.raw);
  printf("  0:%02X  n0:%02X  1:%02X  n1:%02X\n",
      r.b.DATA0, r.b.nDATA0, r.b.DATA1, r.b.nDATA1);
}

void flash_wrpr_dump(flash_wrpr r) {
  print_bits(1, "WRPR0", r.b.WRPR0);
  print_bits(0, "nWRPR0", r.b.nWRPR0);
  print_bits(1, "WRPR1", r.b.WRPR1);
  print_bits(0, "nWRPR1", r.b.nWRPR1);
}

void flash_actlr_dump(flash_actlr r) {
  print_b(0, "ACTLR\n");
  printf("  %08X\n", r.raw);
  printf("  LATENCY:%d\n", r.b.LATENCY);
}

void flash_statr_dump(flash_statr r) {
  print_b(0, "STATR\n");
  printf("  %08X\n", r.raw);
  printf("  BOOT_LOCK:%d  BUSY:%d  EOP:%d  MODE:%d  WRPRTERR:%d\n",
         r.b.BOOT_LOCK, r.b.BUSY, r.b.EOP, r.b.MODE, r.b.WRPRTERR);
}

void flash_ctlr_dump(flash_ctlr r) {
  print_b(0, "CTLR\n");
  printf("  %08X\n", r.raw);
  printf("  BUFLOAD:%d  BUFRST:%d  ERRIE:%d  EOPIE:%d  FLOCK:%d  FTER:%d  FTPG:%d\n",
         r.b.BUFLOAD, r.b.BUFRST, r.b.ERRIE, r.b.EOPIE, r.b.FLOCK, r.b.FTER, r.b.FTPG);
  printf("  LOCK:%d  MER:%d  OBER:%d  OBG:%d  OBWRE:%d  PER:%d  PG:%d  STRT:%d\n",
         r.b.LOCK, r.b.MER, r.b.OBER, r.b.OBG, r.b.OBWRE, r.b.PER, r.b.PG, r.b.STRT);
}

void flash_obr_dump(flash_obr r) {
  print_b(0, "OBR\n");
  printf("  %08X\n", r.raw);
  printf("  CFGRSTT:%d  DATA0:%d  DATA1:%d  IWDG_SW:%d  OBERR:%d  RDPRT:%d  STANDBY_RST:%d\n",
         r.b.CFGRSTT, r.b.DATA0, r.b.DATA1, r.b.IWDG_SW, r.b.OBERR, r.b.RDPRT, r.b.STANDBY_RST);
}

//------------------------------------------------------------------------------
// Debug module registers

void dm_print(uint8_t reg, uint32_t raw) {
  const char* name = dm_to_name(reg);
  print_hex(0, name, raw);
}

const char* dm_to_name(uint8_t reg) {
  switch (reg) {
    case DM_DATA0:
      return "DM_DATA0";
    case DM_DATA1:
      return "DM_DATA1";
    case DM_CONTROL:
      return "DM_CONTROL";
    case DM_STATUS:
      return "DM_STATUS";
    case DM_HARTINFO:
      return "DM_HARTINFO";
    case DM_ABSTRACTCS:
      return "DM_ABSTRACTCS";
    case DM_COMMAND:
      return "DM_COMMAND";
    case DM_ABSTRACTAUTO:
      return "DM_ABSTRACTAUTO";
    case DM_PROGBUF0:
      return "DM_PROGBUF0";
    case DM_PROGBUF1:
      return "DM_PROGBUF1";
    case DM_PROGBUF2:
      return "DM_PROGBUF2";
    case DM_PROGBUF3:
      return "DM_PROGBUF3";
    case DM_PROGBUF4:
      return "DM_PROGBUF4";
    case DM_PROGBUF5:
      return "DM_PROGBUF5";
    case DM_PROGBUF6:
      return "DM_PROGBUF6";
    case DM_PROGBUF7:
      return "DM_PROGBUF7";
    case DM_HALTSUM0:
      return "DM_HALTSUM0";
    case DM_CPBR:
      return "DM_CPBR";
    case DM_CFGR:
      return "DM_CFGR";
    case DM_SHDWCFGR:
      return "DM_SHDWCFGR";
    case DM_PART:
      return "DM_PART";
    default:
      return "DM_?";
  }
}

void dm_control_dump(dm_control r) {
  dm_print(DM_CONTROL, r.raw);
  printf("  ACKHAVERESET:%d  ACKUNAVAIL:%d  CLRKEEPALIVE:%d  CLRRESETHALTREQ:%d  DMACTIVE:%d\n",
         r.b.ACKHAVERESET, r.b.ACKUNAVAIL, r.b.CLRKEEPALIVE, r.b.CLRRESETHALTREQ, r.b.DMACTIVE);
  printf("  HASEL:%d  HALTREQ:%d  HARTRESET:%d  HARTSELHI:%d  HARTSELLO:%d\n",
         r.b.HASEL, r.b.HALTREQ, r.b.HARTRESET, r.b.HARTSELHI, r.b.HARTSELLO);
  printf("  NDMRESET:%d  RESUMEREQ:%d  SETKEEPALIVE:%d  SETRESETHALTREQ:%d\n",
         r.b.NDMRESET, r.b.RESUMEREQ, r.b.SETKEEPALIVE, r.b.SETRESETHALTREQ);
}

void dm_status_dump(dm_status r) {
  dm_print(DM_STATUS, r.raw);
  printf("  ANYAVAIL:%d  ANYHALTED:%d  ANYHAVERESET:%d  ANYRESUMEACK:%d  ANYRUNNING:%d\n",
         r.b.ANYAVAIL, r.b.ANYHALTED, r.b.ANYHAVERESET, r.b.ANYRESUMEACK, r.b.ANYRUNNING);
  printf("  ALLAVAIL:%d  ALLHALTED:%d  ALLHAVERESET:%d  ALLRESUMEACK:%d  ALLRUNNING:%d\n",
         r.b.ALLAVAIL, r.b.ALLHALTED, r.b.ALLHAVERESET, r.b.ALLRESUMEACK, r.b.ALLRUNNING);
  printf("  AUTHENTICATED:%d  VERSION:%d\n", r.b.AUTHENTICATED, r.b.VERSION);
}

void dm_hartinfo_dump(dm_hartinfo r) {
  dm_print(DM_HARTINFO, r.raw);
  printf("  DATAACCESS:%d  DATAADDR:%03X  DATASIZE:%d  NSCRATCH:%d\n",
         r.b.DATAACCESS, r.b.DATAADDR, r.b.DATASIZE, r.b.NSCRATCH);
}

void dm_abstractcs_dump(dm_abstractcs r) {
  dm_print(DM_ABSTRACTCS, r.raw);
  printf("  BUSY:%d  CMDER:%d  DATACOUNT:%d  PROGBUFSIZE:%d\n",
         r.b.BUSY, r.b.CMDER, r.b.DATACOUNT, r.b.PROGBUFSIZE);
}

void dm_command_dump(dm_command r) {
  dm_print(DM_COMMAND, r.raw);
  printf("  AARPOSTINC:%d  AARSIZE:%d  POSTEXEC:%d  REGNO:%04X  TRANSFER:%d  WRITE:%d\n",
         r.b.AARPOSTINC, r.b.AARSIZE, r.b.POSTEXEC, r.b.REGNO, r.b.TRANSFER, r.b.WRITE);
}

void dm_abstractauto_dump(dm_abstractauto r) {
  dm_print(DM_ABSTRACTAUTO, r.raw);
  printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n",
         r.b.AUTOEXECDATA, r.b.AUTOEXECPROG);
}

void dm_progbuf_dump(void) {
  print_b(0, "DM_PROGBUF\n");
  for (int i = 0; i < DM_PROGBUF_MAX; i++) {
    uint32_t progbuf = dm_get_progbuf(i);
    printf("  %d: %08X", i, progbuf);

    if ((i & 3) == 3)
      putchar('\n');
  }
}

//------------------------------------------------------------------------------
// Debug-specific CSRs

void csr_dcsr_dump(csr_dcsr r) {
  print_b(0, "DCSR\n");
  printf("  %08X\n", r.raw);
  printf("  CAUSE:%d  EBREAKM:%d  EBREAKS:%d  EBREAKU:%d  MPRVEN:%d  NMIP:%d\n",
         r.b.CAUSE, r.b.EBREAKM, r.b.EBREAKS, r.b.EBREAKU, r.b.MPRVEN, r.b.NMIP);
  printf("  PRV:%d  STEP:%d  STEPIE:%d  STOPCOUNT:%d  STOPTIME:%d  XDEBUGVER:%d\n",
         r.b.PRV, r.b.STEP, r.b.STEPIE, r.b.STOPCOUNT, r.b.STOPTIME, r.b.XDEBUGVER);
}

//------------------------------------------------------------------------------
// Debug interface registers

void dm_cpbr_dump(dm_cpbr r) {
  dm_print(DM_CPBR, r.raw);
  printf("  CHECKSTA:%d  CMDEXTENSTA:%d  IOMODE:%d  OUTSTA:%d  SOPN:%d  TDIV:%d  VERSION:%d\n",
         r.b.CHECKSTA, r.b.CMDEXTENSTA, r.b.IOMODE, r.b.OUTSTA, r.b.SOPN, r.b.TDIV, r.b.VERSION);
}

void dm_cfgr_dump(dm_cfgr r) {
  dm_print(DM_CFGR, r.raw);
  printf("  CHECKEN:%d  CMDEXTEN:%d  IOMODECFG:%d  KEY:%04X  OUTEN:%d  SOPNCFG:%d  TDIVCFG:%d\n",
         r.b.CHECKEN, r.b.CMDEXTEN, r.b.IOMODECFG, r.b.KEY, r.b.OUTEN, r.b.SOPNCFG, r.b.TDIVCFG);
}

void dm_shdwcfgr_dump(dm_shdwcfgr r) {
  dm_print(DM_SHDWCFGR, r.raw);
  printf("  CHECKEN:%d  CMDEXTEN:%d  IOMODECFG:%d  KEY:%04X  OUTEN:%d  SOPNCFG:%d  TDIVCFG:%d\n",
         r.b.CHECKEN, r.b.CMDEXTEN, r.b.IOMODECFG, r.b.KEY, r.b.OUTEN, r.b.SOPNCFG, r.b.TDIVCFG);
}

//------------------------------------------------------------------------------
