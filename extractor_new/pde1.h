#ifndef _PDE1_H_
#define _PDE1_H_

#include "pde.h"
#include "pde0.h"
#include "pte.h"

class PDE1
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    PDEType type = PD1;

    ENTRY self_entry;
    MmuFormat format = MmuFormat::VER2;
    std::map<int, ENTRY *> pde_entry;
    std::map<int, PDE0 *> PDE0s;

    std::map<int, ENTRY *> pte_entry;

public:
    PDE1(uint64_t phy_addr,
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

    ~PDE1()
    {
        for (auto it = this->pde_entry.begin(); it != this->pde_entry.end(); it++)
            delete it->second;

        for (auto it = this->PDE0s.begin(); it != this->PDE0s.end(); it++)
            delete it->second;

        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
            delete it->second;

        this->pde_entry.clear();
        this->PDE0s.clear();
        this->pte_entry.clear();
    }

    void print(uint64_t addr)
    {
        for (int i = 0; i < this->type; i++)
            std::cout << "\t";

        std::cout << "PDE1: 0x" << std::hex << this->phy_addr << "  type:" << this->type
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

            PDE0s[it->first]->print(addr | ((uint64_t)it->first << 29));
        }

        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
        {
            for (int i = 0; i < this->type; i++)
                std::cout << "\t";

            std::cout << "\t PDE1 is PTE:" << std::dec << it->first << " "
                      << "A: " << (uint64_t)it->second->A
                      << " V: " << (uint64_t)it->second->V << " "
                      << "flags: 0b" << std::bitset<8>(it->second->flags) << " "
                      << "Page_addr: 0x" << std::hex << it->second->addr << " "
                      << "Page_size: PAGE_512M"
                      << " virt_addr: 0x" << std::hex << (addr | ((uint64_t)it->first << 29))
                      << " " << std::endl;

            it->second->virt_addr = (addr | ((uint64_t)it->first << 29));
            it->second->small = PAGE_512M;
        }

        std::cout << std::endl;
    }

    bool construct()
    {
        bool ok;

        ok = construct_PDE1_entry();
        if (!ok)
            return false;

        ok = construct_PDE0();
        if (!ok) {
            if (this->pte_entry.size() != 0)
                return true;

            return false;
        }

        return true;
    }

    bool construct_PDE1_entry()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

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
            std::uint8_t flags = entry_Ptr[0] & 0xff;
            std::uint64_t addr = mmu_decode_single_pde_address(entry_bits, this->format);

            if (V == 0x00) {
                if (addr == 0x0)
                    continue;

                if (A == 0)
                    return false;

                ENTRY *entry_pde = new ENTRY(addr, flags, V, A, i, entry_bits);
                this->pde_entry[i] = entry_pde;
            }
            else if (V == 0x01) {
                if (A > 0x03)
                    return false;

                std::uint64_t pte_addr = mmu_decode_pte_address(entry_bits, this->format);
                ENTRY *entry_pte = new ENTRY(pte_addr, flags, V, A, i, entry_bits);
                this->pte_entry[i] = entry_pte;
            }
            else {
                return false;
            }
        }

        if (this->pde_entry.size() != 0 || this->pte_entry.size() != 0)
            return true;

        return false;
    }

    bool construct_PDE0()
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

            PDE0 *ptPtr = new PDE0(it->second->addr, this->base_addr, PD0, *(it->second), this->format, this->dump_size);
            bool ok = ptPtr->construct();
            if (!ok) {
                delete ptPtr;
                it = this->pde_entry.erase(it);
            }
            else {
                PDE0s[it->first] = ptPtr;
                it++;
            }
        }

        if (this->pde_entry.size() != 0)
            return true;

        return false;
    }
};

#endif
