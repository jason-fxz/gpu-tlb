#ifndef _PDE4_H_
#define _PDE4_H_

#include "pde.h"
#include "pde3.h"

class PDE4
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    PDEType type = PD4;
    ENTRY self_entry;
    MmuFormat format = MmuFormat::VER3;
    uint32_t max_entries = 2;

    std::map<int, ENTRY *> pde_entry;
    std::map<int, PDE3 *> PDE3s;

public:
    PDE4(uint64_t phy_addr,
         void *base_addr,
         PDEType type,
         ENTRY self_entry,
         MmuFormat format = MmuFormat::VER3,
         uint64_t dump_size = 0,
         uint32_t max_entries = 2)
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

    ~PDE4()
    {
        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            delete it->second;

        for (auto it = this->PDE3s.begin(); it != this->PDE3s.end(); it++)
            delete it->second;

        this->pde_entry.clear();
        this->PDE3s.clear();
    }

    void print(uint64_t addr)
    {
        for (int i = 0; i < this->type; i++)
            std::cout << "\t";

        std::cout << "PDE4: 0x" << std::hex << this->phy_addr << "  type:" << this->type
                  << "  entry:" << this->self_entry.entry_bits << std::endl;

        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
        {
            for (int i = 0; i < this->type; i++)
                std::cout << "\t";

            std::cout << "\t" << std::dec << it->first << " "
                      << "A: " << (uint64_t)it->second->A
                      << " V: " << (uint64_t)it->second->V << " "
                      << "flags: 0b" << std::bitset<8>(it->second->flags) << " "
                      << "next_phy_addr: 0x" << std::hex << it->second->addr
                      << std::endl;

            PDE3s[it->first]->print(addr | ((uint64_t)it->first << 56));
        }

        std::cout << std::endl;
    }

    bool construct()
    {
        bool ok = construct_PDE4_entry();
        if (!ok)
            return false;

        ok = construct_PDE3();
        if (!ok)
            return false;

        return true;
    }

    bool construct_PDE4_entry()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

            if (addr > this->dump_size)
                return false;

            return size <= (this->dump_size - addr);
        };

        if (this->format != MmuFormat::VER3)
            return false;

        if (!in_dump_range(this->phy_addr, 4096))
            return false;

        if (this->max_entries == 0 || this->max_entries > 512)
            return false;

        for (uint32_t i = this->max_entries; i < 512; i++)
        {
            uint8_t *entry_ptr = (uint8_t *)this->entry_addr + i * 8;
            if (mmu_read_u64_le(entry_ptr) != 0)
                return false;
        }

        for (uint32_t i = 0; i < this->max_entries; i++)
        {
            uint8_t *entry_ptr = (uint8_t *)this->entry_addr + i * 8;

            std::uint64_t entry_bits = mmu_read_u64_le(entry_ptr);
            std::uint8_t V = mmu_entry_valid(entry_bits);
            std::uint8_t A = mmu_entry_aperture(entry_bits);
            std::uint8_t flags = entry_ptr[0] & 0xff;
            std::uint64_t addr = mmu_decode_single_pde_address(entry_bits, this->format);

            if (V == 0x00 && A == 0x00 && addr == 0x0)
                continue;

            if (V != 0x00)
                return false;

            if (A == 0)
                return false;

            if (addr == 0)
                continue;

            ENTRY *entry = new ENTRY(addr, flags, V, A, i, entry_bits);
            this->pde_entry[i] = entry;
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }

    bool construct_PDE3()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

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

            PDE3 *pde3_ptr = new PDE3(it->second->addr,
                                      this->base_addr,
                                      PD3,
                                      *(it->second),
                                      this->format,
                                      this->dump_size,
                                      512);
            bool ok = pde3_ptr->construct();
            if (!ok) {
                delete pde3_ptr;
                it = this->pde_entry.erase(it);
            }
            else {
                this->PDE3s[it->first] = pde3_ptr;
                it++;
            }
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }
};

#endif
