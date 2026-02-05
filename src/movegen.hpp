#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP
#include <iostream>
#include <vector>
#include "board.hpp"
#include "movelist.hpp"

class MoveGen{
    public:
        void toMove(uint64_t target, int shift_, int flags_, MoveList& movelist);
        MoveList genPawn(Board& board, MoveList& movelist);
        
};



#endif
