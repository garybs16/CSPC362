#include "board.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include "move.hpp"

void Board::setEmpty(){
    for(size_t i =0; i< 12; i++){
        piece_bitboard[i] = 0ULL;
    }
}

void Board::printBitBoard(uint64_t bitboard){
    for(int rank = 7; rank >=0; rank--){
        std::cout << rank + 1 << " ";
        for(size_t file = 0; file < 8; file++){
            int square = rank * 8 + file;
            if((bitboard >> square) & 1ULL){
                std::cout << "1";
            }else{
                std::cout << "0";
            }
            
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Board::defaultBoard(){
    ///white pawns
    for(size_t i = 8; i <16; i++){
        piece_bitboard[P] |= (1ULL << i);
    }
    /// black pawns
    for(size_t i = 55; i >= 48; i--){
        piece_bitboard[bP] |= (1ULL << i);
    }
    ///white rools
    piece_bitboard[R] |= (1ULL);
    piece_bitboard[R] |= (1ULL << 7);
    ///black rooks
    piece_bitboard[bR] |= (1ULL << 63);
    piece_bitboard[bR] |= (1ULL << 56);

    ///white knight
    piece_bitboard[N] |= (1ULL << 1);
    piece_bitboard[N] |= (1ULL << 6);
    //black knight
    piece_bitboard[bN] |= (1ULL << 62);
    piece_bitboard[bN] |= (1ULL << 57);

    //white bishop 
    piece_bitboard[B] |= (1ULL << 2);
    piece_bitboard[B] |= (1ULL << 5);
     //black bishop 
    piece_bitboard[bB] |= (1ULL << 61);
    piece_bitboard[bB] |= (1ULL << 58);

    //white queen and king
    piece_bitboard[Q] |= (1ULL << 3);
    piece_bitboard[K] |= (1ULL << 4);
    //black queen and king
    piece_bitboard[bQ] |= (1ULL << 59);
    piece_bitboard[bK] |= (1ULL << 60);
}

void Board::updateOccupancy(){
    occupancy[whiteOccupancy] = 0ULL;
    occupancy[blackOccupancy] = 0ULL;
    occupancy[dualOccupancy] = 0ULL;

    ///white occupancy
    for(int i{0}; i < 6; i++){
        occupancy[whiteOccupancy] |= piece_bitboard[i];
    }
    
    ///black occupancy
    for(int i{6}; i < 12; i++){
        occupancy[blackOccupancy] |= piece_bitboard[i];
    }

    ///dual occupancy
    occupancy[dualOccupancy] = occupancy[0] ^ occupancy[1];
}


///not sure if we want makemove inside of board
void Board::makeMove(int piece_, int from, int dest, bool turn){
    Move move;
    move.dest = dest;
    move.from = from;
    bool present = (piece_bitboard[piece_] & 1ULL << move.from );
    bool occupied = (occupancy[0] & 1ULL << move.dest);
    if(occupied){
        std::cout << "invalid square" ;
    }
    else if(present){
        piece_bitboard[piece_] ^= (1ULL << move.from);
        piece_bitboard[piece_] |= (1ULL << move.dest);
    }
    
    
}
        
std::string Board::toSquare(int space){
    
    int file = space % 8;
    int row = (space / 8) + 1;
    
    std::string square = Board::files[file];
    square += std::to_string(row);
    return square;
}