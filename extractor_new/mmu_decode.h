#ifndef _MMU_DECODE_H_
#define _MMU_DECODE_H_

#include <cstdint>

enum class MmuFormat
{
    VER2 = 2,
    VER3 = 3,
};

static inline std::uint64_t mmu_read_u64_le(const std::uint8_t *ptr)
{
    std::uint64_t value = 0;

    for (int i = 0; i < 8; ++i)
        value |= ((std::uint64_t)ptr[i]) << (i * 8);

    return value;
}

static inline std::uint8_t mmu_entry_valid(std::uint64_t raw)
{
    return raw & 0x1;
}

static inline std::uint8_t mmu_entry_aperture(std::uint64_t raw)
{
    return (raw >> 1) & 0x3;
}

static inline std::uint64_t mmu_decode_single_pde_address(std::uint64_t raw, MmuFormat format)
{
    if (format == MmuFormat::VER3)
        return raw & 0x000ffffffffff000ULL; // [51:12] -> PA[51:12]

    return ((raw >> 8) & 0x3fffffffffffULL) << 12; // [53:8] -> PA[53:12]
}

static inline std::uint64_t mmu_decode_dual_big_address(std::uint64_t raw, MmuFormat format)
{
    if (format == MmuFormat::VER3)
        return ((raw >> 8) & 0xfffffffffffULL) << 8; // [51:8] -> PA[51:8]

    return ((raw >> 4) & 0x3ffffffffffffULL) << 8; // [53:4] -> PA[53:8]
}

static inline std::uint64_t mmu_decode_dual_small_address(std::uint64_t raw, MmuFormat format)
{
    if (format == MmuFormat::VER3)
        return raw & 0x000ffffffffff000ULL; // [51:12] in upper qword

    return ((raw >> 8) & 0x3fffffffffffULL) << 12; // [53:8] in upper qword
}

static inline std::uint64_t mmu_decode_pte_address(std::uint64_t raw, MmuFormat format)
{
    if (format == MmuFormat::VER3)
        return raw & 0x000ffffffffff000ULL; // [51:12]

    return ((raw >> 8) & 0x3fffffffffffULL) << 12; // [53:8]
}

#endif
