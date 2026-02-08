#include <stdio.h>

#include "vendor.h"

//==============================================================================
// API

void vendor_dump(void) {
  print_y(0, "vendor:dump\n");

  ctx_dump_block(0, VNDB_ADDR, VNDB_SIZE);
  
  vndb_chipid chipid = vndb_get_chipid();
  vndb_chipid_dump(chipid);

  print_b(0, "UNIID\n");
  uint32_t uniid1 = esig_get_uniid1();
  print_hex(2, "1", uniid1);

  uint32_t uniid2 = esig_get_uniid2();
  print_hex(2, "2", uniid2);

  uint32_t uniid3 = esig_get_uniid3();
  print_hex(2, "3", uniid3);

  uint16_t flacap = esig_get_flacap();
  print_num(0, "FLACAP", flacap);
}

//==============================================================================
// Vendor bytes areas

const char* package_to_text(package p) {
  switch (p) {
    case TSSOP20:
      return "TSSOP20";
    case QFN20:
      return "QFN20";
    case SOP16:
      return "SOP16";
    case SOP8:
      return "SOP8";
    default:
      return "?";
  }
}

//------------------------------------------------------------------------------

void vndb_chipid_dump(vndb_chipid r) {
  print_hex(0, "CHIPID", r.raw);
  printf("  0:%02X  1:%X  2:%X  3:%03X\n", r.b.VAL0, r.b.VAL1, r.b.VAL2, r.b.VAL3);
  
  const char* package = package_to_text(r.b.PACKAGE);
  print_str(2, "package", package);
}

//------------------------------------------------------------------------------
