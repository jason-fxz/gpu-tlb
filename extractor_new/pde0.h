#ifndef _PDE0_H_
#define _PDE0_H_

#include "pde.h"
#include "pte.h"

class PDE0
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    PDEType type = PD0;

    ENTRY self_entry;
    MmuFormat format = MmuFormat::VER2;

    std::map<int, ENTRY *> pde_entry_big;
    std::map<int, ENTRY *> pde_entry_small;
    std::map<int, PTE *> PTEs_big;
    std::map<int, PTE *> PTEs_small;
    std::map<int, ENTRY *> pte_entry;

public:
    PDE0(uint64_t phy_addr,
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

    ~PDE0()
    {
        for (auto it = this->PTEs_big.begin(); it != this->PTEs_big.end(); it++)
            delete it->second;

        for (auto it = this->pde_entry_big.begin(); it != this->pde_entry_big.end(); it++)
            delete it->second;

        for (auto it = this->pde_entry_small.begin(); it != this->pde_entry_small.end(); it++)
            delete it->second;

        for (auto it = this->PTEs_small.begin(); it != this->PTEs_small.end(); it++)
            delete it->second;

        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
            delete it->second;

        this->PTEs_big.clear();
        this->pde_entry_big.clear();
        this->pde_entry_small.clear();
        this->PTEs_small.clear();
        this->pte_entry.clear();
    }

    void print(uint64_t addr)
    {
        for (int i = 0; i < this->type; i++)
            std::cout << "\t";

        std::cout << "PDE0: 0x" << std::hex << this->phy_addr << "  type:" << this->type
                  << "  entry:" << this->self_entry.entry_bits << std::endl;

        for (auto it = this->pde_entry_big.begin(); it != this->pde_entry_big.end(); it++)
        {
            for (int i = 0; i < this->type; i++)
                std::cout << "\t";

            std::cout << "\t" << std::dec << it->first << " "
                      << "big_A: " << (uint64_t)it->second->A << " "
                      << "big_V: " << (uint64_t)it->second->V << " "
                      << "big_flags: 0b" << std::bitset<8>(it->second->flags) << " "
                      << "big_next_phy_addr: 0x" << std::hex << it->second->addr
                      << std::endl;

            PTEs_big[it->first]->print(addr | ((uint64_t)it->first << 21));
        }

        for (auto it = this->pde_entry_small.begin(); it != this->pde_entry_small.end(); it++)
        {
            for (int i = 0; i < this->type; i++)
                std::cout << "\t";

            std::cout << "\t" << std::dec << it->first << " "
                      << "small_A: " << (uint64_t)it->second->A << " "
                      << "small_V: " << (uint64_t)it->second->V << " "
                      << "small_flags: 0b" << std::bitset<8>(it->second->flags) << " "
                      << "small_next_phy_addr: 0x" << std::hex << it->second->addr
                      << std::endl;

            PTEs_small[it->first]->print(addr | ((uint64_t)it->first << 21));
        }

        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
        {
            for (int i = 0; i < this->type; i++)
                std::cout << "\t";

            std::cout << "\t PD0 is PTE:" << std::dec << it->first << " "
                      << "A: " << (uint64_t)it->second->A
                      << " V: " << (uint64_t)it->second->V << " "
                      << "flags: 0b" << std::bitset<8>(it->second->flags) << " "
                      << "Page_addr: 0x" << std::hex << it->second->addr << " "
                      << "Page_size: PAGE_2M"
                      << " virt_addr: 0x" << std::hex << (addr | ((uint64_t)it->first << 21))
                      << " " << std::endl;

            it->second->virt_addr = (addr | ((uint64_t)it->first << 21));
            it->second->small = PAGE_2M;
        }

        std::cout << std::endl;
    }

    bool construct()
    {
        bool ok;

        ok = construct_PDE0_entry();
        if (!ok)
            return false;

        ok = construct_PDE();
        if (!ok) {
            if (this->pte_entry.size() != 0)
                return true;

            return false;
        }

        return true;
    }

    bool construct_PDE0_entry()
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

        for (int i = 0; i < 256; i++)
        {
            uint8_t *entry_Ptr_big = (uint8_t *)this->entry_addr + i * 16;
            uint8_t *entry_Ptr_small = entry_Ptr_big + 8;

            std::uint64_t entry_bits_big = mmu_read_u64_le(entry_Ptr_big);
            std::uint64_t entry_bits_small = mmu_read_u64_le(entry_Ptr_small);

            std::uint8_t V = mmu_entry_valid(entry_bits_big);
            std::uint8_t A_big = mmu_entry_aperture(entry_bits_big);
            std::uint8_t A_small = mmu_entry_aperture(entry_bits_small);
            std::uint8_t flags_big = entry_Ptr_big[0] & 0xff;
            std::uint8_t flags_small = entry_Ptr_small[0] & 0xff;

            std::uint64_t addr_big = mmu_decode_dual_big_address(entry_bits_big, this->format);
            std::uint64_t addr_small = mmu_decode_dual_small_address(entry_bits_small, this->format);
            std::uint64_t addr_pte = mmu_decode_pte_address(entry_bits_big, this->format);

            if (V == 0x00) {
                if (addr_big == 0x0 && addr_small == 0x0)
                    continue;

                if (addr_big != 0x0) {
                    if (A_big == 0)
                        return false;

                    ENTRY *entry_big = new ENTRY(addr_big, flags_big, V, A_big, i, entry_bits_big, PAGE_64K);
                    this->pde_entry_big[i] = entry_big;
                }

                if (addr_small != 0x0) {
                    if (A_small == 0)
                        return false;

                    ENTRY *entry_small = new ENTRY(addr_small, flags_small, V, A_small, i, entry_bits_small, PAGE_4K);
                    this->pde_entry_small[i] = entry_small;
                }
            }
            else if (V == 0x01) {
                if (A_big > 0x03)
                    return false;

                ENTRY *entry_pte = new ENTRY(addr_pte, flags_big, V, A_big, i, entry_bits_big, PAGE_2M);
                this->pte_entry[i] = entry_pte;
            }
            else {
                return false;
            }
        }

        if (this->pde_entry_big.size() != 0 || this->pde_entry_small.size() != 0 || this->pte_entry.size() != 0)
            return true;

        return false;
    }

    bool construct_PDE()
    {
        auto in_dump_range = [&](uint64_t addr, uint64_t size) {
            if (this->dump_size == 0)
                return true;

            if (addr > this->dump_size)
                return false;

            return size <= (this->dump_size - addr);
        };

        if (this->pde_entry_big.size() == 0 && this->pde_entry_small.size() == 0)
            return false;

        for (auto it = this->pde_entry_big.begin(); it != this->pde_entry_big.end();)
        {
            if (!in_dump_range(it->second->addr, 256))
                return false;

            PTE *ptPtr = nullptr;
            if (it->second->small == PAGE_64K)
                ptPtr = new PTE(it->second->addr, this->base_addr, PAGE64K, *(it->second), 256, 8, this->format, this->dump_size);
            else
                return false;

            bool ok = ptPtr->construct();
            if (!ok) {
                delete ptPtr;
                it = this->pde_entry_big.erase(it);
            }
            else {
                PTEs_big[it->first] = ptPtr;
                it++;
            }
        }

        for (auto it = this->pde_entry_small.begin(); it != this->pde_entry_small.end();)
        {
            if (!in_dump_range(it->second->addr, 4096))
                return false;

            PTE *ptPtr = nullptr;
            if (it->second->small == PAGE_4K)
                ptPtr = new PTE(it->second->addr, this->base_addr, PAGE4K, *(it->second), 4096, 8, this->format, this->dump_size);
            else
                return false;

            bool ok = ptPtr->construct();
            if (!ok) {
                delete ptPtr;
                it = this->pde_entry_small.erase(it);
            }
            else {
                PTEs_small[it->first] = ptPtr;
                it++;
            }
        }

        if (this->pde_entry_small.size() != 0 || this->pde_entry_big.size() != 0)
            return true;

        return false;
    }
};

#endif
