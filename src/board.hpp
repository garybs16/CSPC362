#ifndef BOARD_HPP
#define BOARD_HPP
#include <cstdint>
#include <cstddef>
#include <iostream>
enum piece{
    P, N, B, R, Q, K,
    bP, bN, bB, bR, bQ, bK
};

struct Move{
    int from,
        dest,
        type;
};


class Board{
    public:
        uint64_t piece_bitboard[12];
        uint64_t occupancy[3];
        void setEmpty();
        void printBitBoard(uint64_t bitboard);
        void defaultBoard();
        void updateOccupancy();
        void makeMove(int piece_, int from, int dest, bool turn);
};

#endif