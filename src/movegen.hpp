#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP
#include <iostream>
#include <vector>

class MoveGen{
    public:
        std::vector<int> genPawn(int location, bool isBlack);
};



#endif