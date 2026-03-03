#include <stdio.h>

#include "vendor.h"
#include "utils.h"

//==============================================================================
// API

void vendor_dump(void) {
  print_y(0, "vendor:info\n");
  if (!ctx_halted_err("display vendor bytes"))
    return;

  ctx_dump_block(0, VNDB_ADDR, VNDB_SIZE);

  vndb_chipid chipid;
  if (vndb_get_chipid(&chipid))
    vndb_chipid_dump("CHIPID", chipid);

  print_b(0, "UNIID\n");
  uint32_t uniid1;
  if (esig_get_uniid1(&uniid1))
    print_hex(2, "1", uniid1);

  uint32_t uniid2;
  if (esig_get_uniid2(&uniid2))
    print_hex(2, "2", uniid2);

  uint32_t uniid3;
  if (esig_get_uniid3(&uniid3))
    print_hex(2, "3", uniid3);

  uint16_t flacap;
  if (esig_get_flacap(&flacap))
    print_num(0, "FLACAP", flacap);

  uint16_t plltrim;
  if (vndb_get_plltrim(&plltrim))
    print_num(0, "PLLTRIM", plltrim);
}

//==============================================================================
// Vendor bytes areas

static const char* vndb_package_dump(package p) {
  const char *name;

  switch (p) {
    case TSSOP20: name = "TSSOP20"; break;
    case QFN20:   name = "QFN20";   break;
    case SOP16:   name = "SOP16";   break;
    case SOP8:    name = "SOP8";    break;
    default:      name = "?";
  }

  print_str(2, "package", name);
}

//------------------------------------------------------------------------------

void vndb_chipid_dump(const char *name, vndb_chipid r) {
  print_hex(0, name, r.raw);

  vndb_package_dump(r.b.PACKAGE);  
  print_hex(2, "DEVID", r.b.DEVID);
  print_hex(2, "REVID", r.b.REVID);
}

//------------------------------------------------------------------------------
