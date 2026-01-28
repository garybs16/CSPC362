#include "board.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>

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
}