#ifndef _PDE2_H_
#define _PDE2_H_

#include "pde.h"
#include "pde1.h"
#include "pte.h"

class PDE2
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    PDEType type = PD2;

    ENTRY self_entry;
    MmuFormat format = MmuFormat::VER2;
    std::map<int, ENTRY *> pde_entry;
    std::map<int, PDE1 *> PDE1s;

public:
    PDE2(uint64_t phy_addr,
         void *base_addr,
         PDEType type,
         ENTRY self_entry,
         MmuFormat format = MmuFormat::VER2,
         uint64_t dump_size = 0)
        : phy_addr(phy_addr),
          dump_size(dump_size),
          base_addr(base_addr),
          self_entry(self_entry),
          format(format)
    {
        this->entry_addr = (uint8_t *)this->base_addr + this->phy_addr;
        this->type = type;
    }

    ~PDE2()
    {
        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            delete it->second;

        for (auto it = this->PDE1s.begin(); it != this->PDE1s.end(); it++)
            delete it->second;

        this->pde_entry.clear();
        this->PDE1s.clear();
    }

    void print(uint64_t addr)
    {
        const char *indent = this->format == MmuFormat::VER3 ? "\t\t" : "\t";
        uint64_t pd3_index_mask = this->format == MmuFormat::VER2 ? 0x3 : 0x1FF;
        std::cout << indent << std::dec << std::setw(3) << std::setfill(' ')
                  << ((addr >> 47) & pd3_index_mask) << "-->PD2@0x" << std::hex
                  << std::setw(10) << std::setfill('0') << this->phy_addr << std::endl;

        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            PDE1s[it->first]->print(addr | ((uint64_t)it->first << 38));
    }

    bool construct()
    {
        bool ok;

        ok = construct_PDE2_entry();
        if (!ok)
            return false;

        ok = construct_PDE1();
        if (!ok)
            return false;

        return true;
    }

    bool construct_PDE2_entry()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

            if (addr < mmu_get_dump_start())
                return false;

            if (addr > this->dump_size)
                return false;

            return size <= (this->dump_size - addr);
        };

        if (!in_dump_range(this->phy_addr, 4096))
            return false;

        for (int i = 0; i < 512; i++)
        {
            uint8_t *entry_Ptr = (uint8_t *)this->entry_addr + i * 8;

            std::uint64_t entry_bits = mmu_read_u64_le(entry_Ptr);
            std::uint8_t V = mmu_entry_valid(entry_bits);
            std::uint8_t A = mmu_entry_aperture(entry_bits);
            MmuFlags flags = mmu_entry_flags(entry_bits, this->format);
            std::uint64_t addr = mmu_decode_single_pde_address(entry_bits, this->format);

            if (V == 0x00 && A == 0x00 && addr == 0x0)
                continue;

            if (V != 0x00)
                return false;

            if (A == 0)
                return false;

            if (addr == 0x0)
                continue;

            ENTRY *entry_pde = new ENTRY(addr, flags, V, A, i, entry_bits);
            this->pde_entry[i] = entry_pde;
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }

    bool construct_PDE1()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

            if (addr < mmu_get_dump_start())
                return false;

            if (addr > this->dump_size)
                return false;

            return size <= (this->dump_size - addr);
        };

        if (this->pde_entry.size() == 0)
            return false;

        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end();)
        {
            if (!in_dump_range(it->second->addr, 4096))
                return false;

            PDE1 *ptPtr = new PDE1(it->second->addr, this->base_addr, PD1, *(it->second), this->format, this->dump_size);
            bool ok = ptPtr->construct();
            if (!ok) {
                delete ptPtr;
                it = this->pde_entry.erase(it);
            }
            else {
                PDE1s[it->first] = ptPtr;
                it++;
            }
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }
};

#endif
