#ifndef BOARD_HPP
#define BOARD_HPP
#include <cstdint>
#include <cstddef>
#include <iostream>
#include "move.hpp"
enum piece{
    P, N, B, R, Q, K,
    bP, bN, bB, bR, bQ, bK
};
enum occupancy{
    whiteOccupancy = 0,
    blackOccupancy = 1,
    dualOccupancy = 2
};
class Board{
    public:
        uint64_t piece_bitboard[12];
        uint64_t occupancy[3];
        short int castling {1111};  
        bool sideToMove; /// 0 = white 1 = black
        int pieceOnSquare[64];


        Board();
        Board(bool isBlack);
        //copy
        Board(const Board& other);


        void setEmpty();
        void printBitBoard(uint64_t bitboard);
        void defaultBoard();
        void updateOccupancy();
        void makeMove(Move m);
        int getPieceAt(int square);
        //remove this
        std::string toSquare(int space);
    private:
        std::string files[8] = {"a", "b","c","d","e","f", "g", "h"};
};

#endif