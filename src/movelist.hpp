#ifndef MOVELIST_HPP
#define MOVELIST_HPP
#include "move.hpp"
#include <iostream>
#include <bitset>
struct MoveList{
    Move moves[256];
    int order[256]; /// this will be to sort the order of moves by 
    int count = 0;  /// appeal to maximize efficency
    

    void push(Move m){
        moves[count++] = {m};
    };
    void printList(){
        for(int i{0}; i < count; i++){
            std::cout << "f: " << moves[i].getFrom() <<" "<<"to: " <<moves[i].getTo() << std::endl;
        }
        
    }
    
};


#endif