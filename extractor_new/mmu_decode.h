#ifndef _MMU_DECODE_H_
#define _MMU_DECODE_H_

#include <cstdint>
#include <ostream>
#include <stdexcept>

enum class MmuFormat
{
    VER2 = 2,
    VER3 = 3, // hopper and later
};

struct MmuFlags {
    std::uint8_t value;
    MmuFormat format;
};


static inline std::uint64_t make_mask(int high, int low) {
    return ((1ULL << (high - low + 1)) - 1) << low;
}

static inline std::uint64_t get_bits(std::uint64_t raw, int high, int low) {
    return (raw & make_mask(high, low)) >> low;
}

static inline std::uint64_t mmu_read_u64_le(const std::uint8_t *ptr) {
    std::uint64_t value = 0;

    for (int i = 0; i < 8; ++i)
        value |= ((std::uint64_t)ptr[i]) << (i * 8);

    return value;
}

static inline std::uint8_t mmu_entry_valid(std::uint64_t raw) {
    return get_bits(raw, 0, 0); // [0:0] -> V
}

static inline std::uint8_t mmu_entry_aperture(std::uint64_t raw) {
    return get_bits(raw, 2, 1); // [2:1] -> A
}

static inline MmuFlags mmu_entry_flags(std::uint64_t raw, MmuFormat format) {
    return MmuFlags{ static_cast<std::uint8_t>(get_bits(raw, 7, 3)), format }; // [7:3] -> PTE Flags
}


// PTE entry flags
static inline std::uint8_t mmu_entry_peer(std::uint64_t raw, MmuFormat format) {
    switch (format) {
        case MmuFormat::VER3:
            return get_bits(raw, 63, 61); // [63:61] -> Peer ID
        case MmuFormat::VER2:
            return get_bits(raw, 35, 33); // [35:33] -> Peer ID
        default:
            throw std::runtime_error("Invalid MMU format");
    }
}


static inline std::ostream& operator<<(std::ostream& os, const MmuFlags& f) {
    switch (f.format) {
        case MmuFormat::VER3:
            os << "|VOL:" << ((f.value >> 0) & 0x1) << "|P:" << ((f.value >> 1) & 0x1)
               << "|RO:" << ((f.value >> 2) & 0x1) << "|AD:" << ((f.value >> 3) & 0x1)
               << "|ACD:" << ((f.value >> 4) & 0x1) << "|";
            break;
        case MmuFormat::VER2:
            os << "|VOL:" << ((f.value >> 0) & 0x1) << "|E:" << ((f.value >> 1) & 0x1)
               << "|P:" << ((f.value >> 2) & 0x1) << "|RO:" << ((f.value >> 3) & 0x1)
               << "|AD:" << ((f.value >> 4) & 0x1) << "|";
            break;
        default:
            throw std::runtime_error("Invalid MMU format");
    }
    return os;
}



// For PDE1 and above
static inline std::uint64_t mmu_decode_single_pde_address(std::uint64_t raw, MmuFormat format) {
    switch (format) {
        case MmuFormat::VER3:
            return get_bits(raw, 51, 12) << 12; // [51:12] -> PA[51:12]
        case MmuFormat::VER2:
            return get_bits(raw, 53, 8) << 12; // [53:8] -> PA[57:12]
        default:
            throw std::runtime_error("Invalid MMU format");
    }
}

// For PDE0 Dual Big
static inline std::uint64_t mmu_decode_dual_big_address(std::uint64_t raw, MmuFormat format) {
    switch (format) {
        case MmuFormat::VER3:
            return get_bits(raw, 51, 8) << 8; // [51:8] -> PA[51:8]
        case MmuFormat::VER2:
            return get_bits(raw, 53, 4) << 8; // [53:4] -> PA[57:8]
        default:
            throw std::runtime_error("Invalid MMU format");
    }
}

// For PDE0 Dual Small
static inline std::uint64_t mmu_decode_dual_small_address(std::uint64_t raw, MmuFormat format) {
   switch (format) {
        case MmuFormat::VER3:
            return get_bits(raw, 51, 12) << 12; // [51:12] -> PA[51:12]
        case MmuFormat::VER2:
            return get_bits(raw, 53, 8) << 12; // [53:8] -> PA[57:12]
        default:
            throw std::runtime_error("Invalid MMU format");
    }
}

// For PTE
static inline std::uint64_t mmu_decode_pte_address(std::uint64_t raw, MmuFormat format) {
    switch (format) {
        case MmuFormat::VER3:
            return get_bits(raw, 51, 12) << 12; // [51:12] -> PA[51:12]
        case MmuFormat::VER2:
            return get_bits(raw, 53, 8) << 12; // [53:8] -> PA[57:12]
        default:
            throw std::runtime_error("Invalid MMU format");
    }
}

#endif
