#include "vendor.h"

//==============================================================================
// API

const char* variant_to_text(uint8_t var) {
  switch (var) {
    case VC_TSSOP20:
      return "TSSOP20";
    case VC_QFN20:
      return "QFN20";
    case VC_SOP16:
      return "SOP16";
    case VC_SOP8:
      return "SOP8";
    default:
      return "?";
  }
}

//------------------------------------------------------------------------------

void vendor_dump(void) {
  print_y(0, "vendor:dump\n");

  uint32_t chipid = vendor_get_chipid();
  print_hex(0, "CHIPID", chipid);

  print_b(0, "UNIID\n");
  uint32_t uniid1 = esig_get_uniid1();
  print_hex(2, "1", uniid1);

  uint32_t uniid2 = esig_get_uniid2();
  print_hex(2, "2", uniid2);

  uint32_t uniid3 = esig_get_uniid3();
  print_hex(2, "3", uniid3);
}

//------------------------------------------------------------------------------
