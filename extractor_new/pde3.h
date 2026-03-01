#ifndef _PDE3_H_
#define _PDE3_H_

#include "pde.h"
#include "pde2.h"

class PDE3
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    PDEType type = PD3;

    ENTRY self_entry;
    MmuFormat format = MmuFormat::VER2;
    uint32_t max_entries = 4;
    std::map<int, ENTRY *> pde_entry;
    std::map<int, PDE2 *> PDE2s;

public:
    PDE3(uint64_t phy_addr,
         void *base_addr,
         PDEType type,
         ENTRY self_entry,
         MmuFormat format = MmuFormat::VER2,
         uint64_t dump_size = 0,
         uint32_t max_entries = 4)
        : phy_addr(phy_addr),
          dump_size(dump_size),
          base_addr(base_addr),
          self_entry(self_entry),
          format(format),
          max_entries(max_entries)
    {
        this->entry_addr = (uint8_t *)this->base_addr + this->phy_addr;
        this->type = type;
    }

    ~PDE3()
    {
        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            delete it->second;

        for (auto it = this->PDE2s.begin(); it != this->PDE2s.end(); it++)
            delete it->second;

        this->pde_entry.clear();
        this->PDE2s.clear();
    }

    void print(uint64_t addr)
    {
        if (this->self_entry.entry_bits == 0)
        {
            std::cout << "PD3@0x" << std::hex << std::setw(10) << std::setfill('0')
                      << this->phy_addr << std::endl;
        }
        else
        {
            std::cout << "\t" << std::dec << std::setw(3) << std::setfill(' ')
                      << ((addr >> 56) & 0x1FF) << "-->PD3@0x" << std::hex
                      << std::setw(10) << std::setfill('0') << this->phy_addr << std::endl;
        }

        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            PDE2s[it->first]->print(addr | ((uint64_t)it->first << 47));
    }

    bool construct()
    {
        bool ok;

        ok = construct_PDE3_entry();
        if (!ok)
            return false;

        ok = construct_PDE2();
        if (!ok)
            return false;

        return true;
    }

    bool construct_PDE3_entry()
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

        if (this->max_entries == 0 || this->max_entries > 512)
            return false;

        for (uint32_t i = this->max_entries; i < 512; i++)
        {
            uint8_t *entry_Ptr = (uint8_t *)this->entry_addr + i * 8;
            if (mmu_read_u64_le(entry_Ptr) != 0)
                return false;
        }

        for (uint32_t i = 0; i < this->max_entries; i++)
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

            ENTRY *entry = new ENTRY(addr, flags, V, A, i, entry_bits);
            this->pde_entry[i] = entry;
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }

    bool construct_PDE2()
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

            PDE2 *pde2Ptr = new PDE2(it->second->addr, this->base_addr, PD2, *(it->second), this->format, this->dump_size);
            bool ok = pde2Ptr->construct();
            if (!ok) {
                delete pde2Ptr;
                it = this->pde_entry.erase(it);
            }
            else {
                PDE2s[it->first] = pde2Ptr;
                it++;
            }
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }
};

#endif
