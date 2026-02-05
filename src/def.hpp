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
}
#endif