#include <cstdint>
#include <cstddef>
#include <iostream>
#include "board.hpp"


int main(){
    Board myboard;
    myboard.setEmpty();
    myboard.piece_bitboard[P] |= (1ULL << 0);
    myboard.piece_bitboard[P] |= (1ULL << 8);
    myboard.printBitBoard(myboard.piece_bitboard[P]);
    
    return 0;
}