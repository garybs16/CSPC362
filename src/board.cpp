#include "board.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include "move.hpp"

Board::Board(){
    Board::sideToMove = 0;
    setEmpty();
}; 
Board::Board(bool isBlack_){
    Board::sideToMove = isBlack_;
}
//copy con
Board::Board(const Board& other){
    for(size_t i{0}; i < 12; i++ ){
        this->piece_bitboard[i] = other.piece_bitboard[i];
    }
    for(size_t i{0}; i < 3; i++){
        this->occupancy[i] = other.occupancy[i];
    }
    this->castling = other.castling;
    this->sideToMove = other.sideToMove;
    
}
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
        occupancy[0] |= piece_bitboard[i];
    }
    
    ///black occupancy
    for(int i{6}; i < 12; i++){
        occupancy[1] |= piece_bitboard[i];
    }

    ///dual occupancy
    occupancy[2] = occupancy[0] ^ occupancy[1];
}
int Board::getPieceAt(int square) {
    uint64_t bit = (1ULL << square);
    
    if (!(occupancy[dualOccupancy] & bit)) return -1;
    
    int side = (occupancy[0] & bit) ? 0 : 1;
    int start = side ? bP : P;
    int end = side ? bK : K;

    for (int i = start; i <= end; i++) {
        if (piece_bitboard[i] & bit) return i;
    }
    
    return -1; 
}
void Board::makeMove(Move m){
    
    int dest = m.getTo();
    int from = m.getFrom();
    
    int piece = getPieceAt(from);
    int flags = m.getFlags();
    bool present = (piece_bitboard[piece] & 1ULL << from );
    bool occupied = (occupancy[2] & 1ULL << dest);
    if(occupied){
        std::cout << "invalid square" ;
    }
    else if(present){
        piece_bitboard[piece] ^= (1ULL << from);
        piece_bitboard[piece] |= (1ULL << dest);
    }
    
    
}
        
std::string Board::toSquare(int space){
    
    int file = space % 8;
    int row = (space / 8) + 1;
    
    std::string square = Board::files[file];
    square += std::to_string(row);
    return square;
}