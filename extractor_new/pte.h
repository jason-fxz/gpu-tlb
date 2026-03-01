#ifndef _PTE_H_
#define _PTE_H_

#include "pde.h"
#include <string>

class PTE
{
public:
    uint64_t phy_addr;
    uint64_t dump_size = 0;
    void *base_addr;
    void *entry_addr;
    ENTRY self_entry;
    std::map<int, ENTRY *> pte_entry;
    PDEType type;
    MmuFormat format = MmuFormat::VER2;

    uint64_t pte_page_size = 4096;
    uint64_t pte_entry_size = 8;

public:
    PTE(uint64_t phy_addr,
        void *base_addr,
        PDEType type,
        ENTRY self_entry,
        MmuFormat format = MmuFormat::VER2,
        uint64_t dump_size = 0)
        : phy_addr(phy_addr), dump_size(dump_size), base_addr(base_addr), self_entry(self_entry), format(format)
    {
        this->type = type;
        this->entry_addr = (uint8_t *)this->base_addr + this->phy_addr;
    }

    PTE(uint64_t phy_addr, void *base_addr, PDEType type, ENTRY self_entry,
        uint64_t pte_page_size, uint64_t pte_entry_size,
        MmuFormat format = MmuFormat::VER2, uint64_t dump_size = 0)
        : phy_addr(phy_addr), dump_size(dump_size), base_addr(base_addr), self_entry(self_entry),
          format(format), pte_page_size(pte_page_size), pte_entry_size(pte_entry_size)
    {
        this->type = type;
        this->entry_addr = (uint8_t *)this->base_addr + this->phy_addr;
    }
    ~PTE()
    {
        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
        {
            delete it->second;
        }
        this->pte_entry.clear();
    }

    void print(uint64_t addr)
    {
        const char *indent_pt = this->format == MmuFormat::VER3 ? "\t\t\t\t\t" : "\t\t\t\t";
        const char *indent_page = this->format == MmuFormat::VER3 ? "\t\t\t\t\t\t" : "\t\t\t\t\t";
        std::string page_name;
        int shift = 12;
        uint64_t mask = 0x1FF;
        if (this->type == PAGE64K)
        {
            page_name = "64KB-Page";
            shift = 16;
            mask = 0x01F;
        }
        else if (this->type == PAGE4K)
        {
            page_name = "4KB-Page";
            shift = 12;
            mask = 0x1FF;
        }
        else
        {
            page_name = "4KB-Page";
        }

        std::cout << indent_pt << std::dec << std::setw(3) << std::setfill(' ')
                  << ((addr >> 21) & 0x0FF) << "-->PT@0x" << std::hex << std::setw(10)
                  << std::setfill('0') << this->phy_addr << (this->type == PAGE64K ? " [big]" : " [small]") << std::endl;

        for (auto it = this->pte_entry.begin(); it != this->pte_entry.end(); it++)
        {
            uint64_t addr_tmp = addr | ((uint64_t)it->first << shift);
            const char *page_sep = (this->type == PAGE4K) ? "--> " : "-->";
            std::cout << indent_page << std::dec << std::setw(3) << std::setfill(' ')
                      << (it->first & mask) << page_sep << page_name << "@0x" << std::hex
                      << std::setw(10) << std::setfill('0') << it->second->addr
                      << "\tVA: 0x" << std::setw(10) << addr_tmp
                      << "\t|V:" << (int)it->second->V
                      << "|AP:" << mmu_aperture_name(it->second->A)
                      << it->second->flags;
            if ((it->second->A & 0x3) == 1)  // VP
                std::cout << "|PEER:" << (int)it->second->peer;
            std::cout << std::endl;

            it->second->virt_addr = addr_tmp;
        }
    }

    bool construct()
    {
        bool ok;
        ok = construct_PTE_entry();
        if (!ok)
            return false;
        else
            return true;
    }

    bool construct_PTE_entry()
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

        if (!in_dump_range(this->phy_addr, this->pte_page_size))
            return false;

        uint64_t entry_nums = this->pte_page_size / this->pte_entry_size;
        for (int i = 0; (uint64_t)i < entry_nums; i++)
        {
            uint8_t *entry_Ptr = (uint8_t *)this->entry_addr + i * this->pte_entry_size;

            std::uint64_t entry_bits = mmu_read_u64_le(entry_Ptr);
            std::uint8_t V = mmu_entry_valid(entry_bits);
            std::uint8_t A = mmu_entry_aperture(entry_bits);
            MmuFlags flags = mmu_entry_flags(entry_bits, this->format);
            std::uint8_t peer = mmu_entry_peer(entry_bits, this->format);
            std::uint64_t addr = mmu_decode_pte_address(entry_bits, this->format);
            pagetype entry_type;
            if (this->type == PAGE512M)
                entry_type = PAGE_512M;
            else if (this->type == PAGE2M)
                entry_type = PAGE_2M;
            else if (this->type == PAGE64K)
                entry_type = PAGE_64K;
            else if (this->type == PAGE4K)
                entry_type = PAGE_4K;

            if (addr == 0x0 && V == 0x0)
                continue;

            ENTRY *entry = new ENTRY(addr, flags, V, A, i, entry_bits, entry_type, peer);
            if (V == 0x01 && A <= 0x03)
                this->pte_entry[i] = entry;
            else
                return false;
        }
        if (this->pte_entry.size() != 0)
            return true;
        return false;
    }
};

#endif
