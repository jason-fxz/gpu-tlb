#include "entry.h"
#include "pde3.h"
#include "pde4.h"
enum page_map_type
{
    VID_MEM = 0,
    SYS_MEM = 1,
};
struct vm_area_struct
{
    uint64_t start_addr;
    uint64_t size;
    enum page_map_type type;
};

struct vm_area_struct_head
{
    PDE3 *PDE3_entity;
    PDE4 *PDE4_entity;
    struct vm_area_struct *head;
    struct vm_area_struct *tail;
    std::map<uint64_t, struct vm_area_struct *> vm_area_map;
    bool has_phys_range = false;
    uint64_t phys_start_addr = 0;
    uint64_t phys_end_addr = 0; // exclusive
};

uint64_t pagetype_to_size(pagetype type)
{
    switch (type)
    {
    case PAGE_512M:
        return 0x20000000;
    case PAGE_2M:
        return 0x200000;
    case PAGE_64K:
        return 0x10000;
    case PAGE_4K:
        return 0x1000;
    }
    return 0;
}

void add_pte_entry_to_vm_space(struct vm_area_struct_head *head, ENTRY *entry)
{
    uint64_t new_size = pagetype_to_size(entry->small);
    struct vm_area_struct *new_vm_area = new struct vm_area_struct;

    for (auto it = head->vm_area_map.begin(); it != head->vm_area_map.end();)
    {
        vm_area_struct *vm_area = it->second;
        uint64_t start_now = it->first;
        uint64_t end_now = it->first + vm_area->size;

        uint64_t new_start = entry->virt_addr;
        uint64_t new_end = new_start + new_size;

        auto old_it = it++;

        if ((new_start == end_now || new_end == start_now) && vm_area->type == (entry->A == 0 ? VID_MEM : SYS_MEM))
        {
            if (new_start == end_now)
            {
                vm_area->size += new_size;
                return;
            }
            else if (new_end == start_now)
            {
                new_vm_area->start_addr = new_start;
                new_vm_area->size += new_size;
                new_vm_area->type = entry->A == 0 ? VID_MEM : SYS_MEM;
                head->vm_area_map.erase(old_it);
                head->vm_area_map[new_start] = new_vm_area;
                return;
            }
        }
    }

    new_vm_area->start_addr = entry->virt_addr;
    new_vm_area->size = pagetype_to_size(entry->small);
    ;
    new_vm_area->type = entry->A == 0 ? VID_MEM : SYS_MEM;
    head->vm_area_map[entry->virt_addr] = new_vm_area;
}

static void extend_range(struct vm_area_struct_head *head, uint64_t start, uint64_t size)
{
    if (size == 0)
        return;

    uint64_t end = start + size;
    if (!head->has_phys_range) {
        head->has_phys_range = true;
        head->phys_start_addr = start;
        head->phys_end_addr = end;
    }
    else {
        if (start < head->phys_start_addr)
            head->phys_start_addr = start;
        if (end > head->phys_end_addr)
            head->phys_end_addr = end;
    }
}

static void collect_page_table_range_pde3(PDE3 *pde3, struct vm_area_struct_head *head)
{
    extend_range(head, pde3->phy_addr, 4096);

    for (auto itpde2 = pde3->PDE2s.begin(); itpde2 != pde3->PDE2s.end(); ++itpde2)
    {
        PDE2 *pde2 = itpde2->second;
        extend_range(head, pde2->phy_addr, 4096);

        for (auto itpde1 = pde2->PDE1s.begin(); itpde1 != pde2->PDE1s.end(); ++itpde1)
        {
            PDE1 *pde1 = itpde1->second;
            extend_range(head, pde1->phy_addr, 4096);

            for (auto itpde0 = pde1->PDE0s.begin(); itpde0 != pde1->PDE0s.end(); ++itpde0)
            {
                PDE0 *pde0 = itpde0->second;
                extend_range(head, pde0->phy_addr, 4096);

                for (auto itpte_big = pde0->PTEs_big.begin(); itpte_big != pde0->PTEs_big.end(); ++itpte_big)
                {
                    PTE *pte = itpte_big->second;
                    extend_range(head, pte->phy_addr, pte->pte_page_size);
                }
                for (auto itpte_small = pde0->PTEs_small.begin(); itpte_small != pde0->PTEs_small.end(); ++itpte_small)
                {
                    PTE *pte = itpte_small->second;
                    extend_range(head, pte->phy_addr, pte->pte_page_size);
                }
            }
        }
    }
}

void merge_area(struct vm_area_struct_head *head)
{
    for (auto it = head->vm_area_map.begin(); it != head->vm_area_map.end();)
    {
        vm_area_struct *vm_area = it->second;
        // uint64_t start_now = it->first;
        uint64_t end_now = it->first + vm_area->size;

        it++;

        if (it == head->vm_area_map.end())
        {
            return;
        }
        while (end_now == it->first && vm_area->type == it->second->type)
        {
            vm_area->size += it->second->size;
            it = head->vm_area_map.erase(it);
        }
    }
}

static void collect_virtual_address_space(PDE3 *top_pde3, struct vm_area_struct_head *head)
{
    for (auto itpde2 = top_pde3->PDE2s.begin(); itpde2 != top_pde3->PDE2s.end(); itpde2++)
    {
        PDE2 *pde2 = itpde2->second;
        for (auto itpde1 = pde2->PDE1s.begin(); itpde1 != pde2->PDE1s.end(); itpde1++)
        {
            PDE1 *pde1 = itpde1->second;
            for (auto entry_it = pde1->pte_entry.begin(); entry_it != pde1->pte_entry.end(); entry_it++)
            {
                add_pte_entry_to_vm_space(head, entry_it->second);
            }
            for (auto itpde0 = pde1->PDE0s.begin(); itpde0 != pde1->PDE0s.end(); itpde0++)
            {
                PDE0 *pde0 = itpde0->second;
                for (auto entry_it = pde0->pte_entry.begin(); entry_it != pde0->pte_entry.end(); entry_it++)
                {
                    add_pte_entry_to_vm_space(head, entry_it->second);
                }
                for (auto itpte_big = pde0->PTEs_big.begin(); itpte_big != pde0->PTEs_big.end(); itpte_big++)
                {
                    PTE *pte = itpte_big->second;
                    for (auto entry_it = pte->pte_entry.begin(); entry_it != pte->pte_entry.end(); entry_it++)
                    {
                        add_pte_entry_to_vm_space(head, entry_it->second);
                    }
                }
                for (auto itpte_small = pde0->PTEs_small.begin(); itpte_small != pde0->PTEs_small.end(); itpte_small++)
                {
                    PTE *pte = itpte_small->second;
                    for (auto entry_it = pte->pte_entry.begin(); entry_it != pte->pte_entry.end(); entry_it++)
                    {
                        add_pte_entry_to_vm_space(head, entry_it->second);
                    }
                }
            }
        }
    }
}

vm_area_struct_head *visualize_virtual_address_space(PDE3 *top_pde3)
{
    struct vm_area_struct_head *head = new struct vm_area_struct_head;
    head->PDE3_entity = top_pde3;
    head->PDE4_entity = NULL;
    head->head = NULL;
    head->tail = NULL;

    collect_virtual_address_space(top_pde3, head);
    collect_page_table_range_pde3(top_pde3, head);
    merge_area(head);
    return head;
}

vm_area_struct_head *visualize_virtual_address_space(PDE4 *top_pde4)
{
    struct vm_area_struct_head *head = new struct vm_area_struct_head;
    head->PDE3_entity = NULL;
    head->PDE4_entity = top_pde4;
    head->head = NULL;
    head->tail = NULL;

    for (auto it = top_pde4->PDE3s.begin(); it != top_pde4->PDE3s.end(); it++)
    {
        collect_virtual_address_space(it->second, head);
        collect_page_table_range_pde3(it->second, head);
    }

    extend_range(head, top_pde4->phy_addr, 4096);

    merge_area(head);
    return head;
}

void print_area(struct vm_area_struct_head *head)
{
    for (auto it = head->vm_area_map.begin(); it != head->vm_area_map.end(); it++)
    {
        vm_area_struct *vm_area = it->second;
        std::cout << "0x" << std::setw(10) << std::setfill('0') << std::hex << vm_area->start_addr << "-----0x" 
        << std::setw(10) << std::setfill('0') << vm_area->start_addr + vm_area->size
        << " size: 0x" << vm_area->size << " type: " << (vm_area->type == VID_MEM ? "VID_MEM" : "SYS_MEM") << std::endl;
    }
}

void print_physical_range(struct vm_area_struct_head *head)
{
    if (!head->has_phys_range)
    {
        std::cout << "PageTable Range: [empty]" << std::endl;
        return;
    }

    std::cout << "PageTable Range: 0x" << std::setw(10) << std::setfill('0') << std::hex << head->phys_start_addr
              << "-----0x" << std::setw(10) << std::setfill('0') << head->phys_end_addr
              << " size: 0x" << (head->phys_end_addr - head->phys_start_addr) << std::endl;
}
