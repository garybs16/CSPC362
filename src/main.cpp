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
    
    MoveList ml;
    MoveGen mg;
    mg.genPawn(test,ml);
    ml.printList();
    test.makeMove(ml.moves[1]);
    test.updateOccupancy();
    test.printBitBoard(test.occupancy[2]);
    return 0;
}

