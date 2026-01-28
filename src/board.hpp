#ifndef BOARD_HPP
#define BOARD_HPP
#include <cstdint>
#include <cstddef>
#include <iostream>
enum{
    P, N, B, R, Q, K,
    bP, bN, bB, bR, bQ, bK
};

class Board{
    public:
        uint64_t piece_bitboard[12];
        uint64_t occupancy[3];
        void setEmpty();
        void printBitBoard(uint64_t bitboard);
};

#endif