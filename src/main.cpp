#include <cstdint>
#include <cstddef>
#include <iostream>
#include <bitset>
#include "board.hpp"
#include "move.hpp"
#include "movelist.hpp"
#include "movegen.hpp"
int main(){

    
    Board test;
    
    test.defaultBoard();
    test.updateOccupancy();
    test.printBitBoard(test.occupancy[2]);

    Move m(9, 17, 0);
    test.makeMove(m);
    test.printBitBoard(test.piece_bitboard[P]);
    /*MoveList ml;
    MoveGen mg;
    mg.genPawn(test,ml);
    ml.printList();

    
    test.updateOccupancy();
    test.printBitBoard(test.occupancy[2]);*/
    return 0;
}

