#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP
#include <iostream>
#include <vector>
#include "board.hpp"
#include "movelist.hpp"

class MoveGen{
    public:
        void generateAll();
    private:
        void addMoves(uint64_t targets, int from, int flags, MoveList& ml);
        void addPawnMoves(uint64_t target, int shift_, int flags_, MoveList& movelist);
        void genPawn(Board& board, MoveList& movelist);
        
};



#endif
