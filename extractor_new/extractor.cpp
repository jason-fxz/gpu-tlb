#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "pde.h"
#include "pde3.h"
#include "pde4.h"
#include "vm_area_struct.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3)
    {
        std::cout << argv[0] << " <dump> [ver2|ver3]\n";
        return -1;
    }

    MmuFormat format = MmuFormat::VER2;
    if (argc == 3)
    {
        std::string mmu_format = argv[2];
        if (mmu_format == "ver2")
            format = MmuFormat::VER2;
        else if (mmu_format == "ver3" || mmu_format == "hopper")
            format = MmuFormat::VER3;
        else
        {
            std::cout << "unsupported format: " << argv[2] << std::endl;
            std::cout << "supported values: ver2, ver3(hopper)\n";
            return -1;
        }
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        std::cout << "cannot open " << argv[1] << std::endl;
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    std::cout << "size: 0x" << std::hex << st.st_size << std::endl;
    void *basePtr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (basePtr == MAP_FAILED)
    {
        std::cout << "cannot mmap " << argv[1] << std::endl;
        return -1;
    }

    if (format == MmuFormat::VER3)
    {
        std::vector<PDE4 *> PDE4s;
        for (std::uint64_t offset = 0; offset + 4096 <= (uint64_t)st.st_size; offset += 4096)
        {
            PDE4 *topPtr = new PDE4(offset, basePtr, PD4, ENTRY(), format, (uint64_t)st.st_size, 2);
            bool ok = topPtr->construct();
            if (!ok)
                delete topPtr;
            else
                PDE4s.push_back(topPtr);
        }

        for (auto topPtr : PDE4s)
        {
            topPtr->print(0);
            struct vm_area_struct_head *head = visualize_virtual_address_space(topPtr);
            std::cout << "\n";
            print_area(head);
            std::cout << "\n\n\n";
        }
    }
    else
    {
        std::vector<PDE3 *> PDE3s;
        for (std::uint64_t offset = 0; offset + 4096 <= (uint64_t)st.st_size;
             offset += 4096)
        {
            PDE3 *topPtr = new PDE3(offset, basePtr, PD3, ENTRY(), format, (uint64_t)st.st_size, 4);

            bool ok = topPtr->construct();
            if (!ok)
                delete topPtr;
            else
                PDE3s.push_back(topPtr);
        }

        for (auto topPtr : PDE3s)
        {
            uint64_t pd3_addr = topPtr->phy_addr;
            bool is_pd3 = true;
            for (auto itpd3 = PDE3s.begin(); itpd3 != PDE3s.end(); itpd3++)
            {
                for (auto itpd2 = (*itpd3)->PDE2s.begin(); itpd2 != (*itpd3)->PDE2s.end();
                     itpd2++)
                {
                    if (itpd2->second->phy_addr == pd3_addr)
                    {
                        is_pd3 = false;
                        break;
                    }
                }
            }
            if (is_pd3)
            {
                topPtr->print(0);
                struct vm_area_struct_head *head = visualize_virtual_address_space(topPtr);
                std::cout << "\n";
                print_area(head);
                std::cout << "\n\n\n";
            }
        }
    }
}
