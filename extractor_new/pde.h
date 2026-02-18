#ifndef _PDE_H_
#define _PDE_H_

#include <cstdint>
#include <iostream>
#include <map>
#include <bitset>
#include <iomanip>
#include <vector>
#include "entry.h"
#include "mmu_decode.h"

enum PDEType { PD4, PD3, PD2, PD1, PD0, PTE0, PAGE512M, PAGE2M, PAGE64K, PAGE4K};

static inline const char *mmu_aperture_name(std::uint8_t aperture)
{
  switch (aperture & 0x3)
  {
  case 0:
    return "VL";
  case 1:
    return "VP";
  case 2:
    return "SC";
  default:
    return "SN";
  }
}

class PDE {
protected:
  uint64_t phy_addr;
  void *base_addr;
  void *entry_addr;
  std::map<int, ENTRY* > pde_entry;

  PDEType type;

  ENTRY self_entry;

public:
  PDE(uint64_t, void*, PDEType, ENTRY);

  ~PDE();
};

#endif
