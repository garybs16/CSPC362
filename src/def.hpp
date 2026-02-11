#include <cstdint>

#ifndef DEF_HPP
#define DEF_HPP

namespace Masks {
    constexpr uint64_t RANK_1 = 0xFFULL;
    constexpr uint64_t RANK_2 = RANK_1 << 8;
    constexpr uint64_t RANK_3 = RANK_1 << 16;
    constexpr uint64_t RANK_4 = RANK_1 << 24;
    constexpr uint64_t RANK_5 = RANK_1 << 32;
    constexpr uint64_t RANK_6 = RANK_1 << 40;
    constexpr uint64_t RANK_7 = RANK_1 << 48;
    constexpr uint64_t RANK_8 = RANK_1 << 56;

    // File Mask
    constexpr uint64_t File_A = 0x0101010101010101ULL;;
    constexpr uint64_t File_B = File_A << 1;
    constexpr uint64_t File_C = File_A << 2;
    constexpr uint64_t File_D = File_A << 3;
    constexpr uint64_t File_E = File_A << 4;
    constexpr uint64_t File_F = File_A << 5;
    constexpr uint64_t File_G = File_A << 6;
    constexpr uint64_t File_H = File_A << 7;
}
#endif
