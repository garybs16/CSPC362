#include <cstdint>
#include <cstddef>
#include <iostream>
#include "board.hpp"


int main(){

    
    Board test;
    test.setEmpty();
    test.defaultBoard();
    /*for(int i =0 ; i<12; i++){
        std::cout << i << std::endl;
        test.printBitBoard(test.piece_bitboard[i]);
        
    } */
   test.updateOccupancy();
   test.printBitBoard(test.occupancy[0]);
   test.makeMove(0, 9, 24, 0);
   test.updateOccupancy();
   test.printBitBoard(test.piece_bitboard[0]);
   for(int i{0}; i < 64; i++){
       std::cout << test.toSquare(i) << " ";

   }
    return 0;
}

