#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP
#include <iostream>
#include <vector>
#include "board.hpp"

class MoveGen{
    public:
        std::vector<int> genPawn(Board& board, int location, bool isBlack);
};



#endif
