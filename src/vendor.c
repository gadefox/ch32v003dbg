#include <stdio.h>

#include "vendor.h"

//==============================================================================
// API

void vendor_dump(void) {
  print_y(0, "vendor:dump\n");

  vnd_chipid chipid = vnd_get_chipid();
  vnd_chipid_dump(chipid);

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

void vnd_chipid_dump(vnd_chipid r) {
  print_hex(0, "CHIPID", r.raw);

  const char* package = package_to_text(r.b.PACKAGE);
  print_str(2, "package", package);

  print_num(2, "sectors", r.b.SECTORS);

  printf("  VAL0:%X  VAL1:%X  VAL2:%03X\n", r.b.VAL0, r.b.VAL1, r.b.VAL2);
}

//------------------------------------------------------------------------------
