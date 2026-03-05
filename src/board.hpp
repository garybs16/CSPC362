#ifndef BOARD_HPP
#define BOARD_HPP
#include <cstdint>
#include <iostream>
#include <string>
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

enum CastlingRight{
    WhiteKingSide = 1,
    WhiteQueenSide = 2,
    BlackKingSide = 4,
    BlackQueenSide = 8
};

class Board{
    public:
        uint64_t piece_bitboard[12]{};
        uint64_t occupancy[3]{};
        unsigned char castling = WhiteKingSide | WhiteQueenSide | BlackKingSide | BlackQueenSide;
        bool sideToMove = false; /// 0 = white 1 = black
        int enPassant = -1; // Square index for en passant target, -1 if not available

        Board();
        explicit Board(bool isBlack);
        Board(const Board& other);

        void setEmpty();
        void printBitBoard(uint64_t bitboard) const;
        void defaultBoard();
        void updateOccupancy();
        bool makeMove(const Move& m);
        int getPieceAt(int square) const;
        bool isSquareOccupied(int square) const;
        bool isSidePiece(int square, bool blackSide) const;
        int findKing(bool blackSide) const;
        std::string toSquare(int space) const;
};

#endif
