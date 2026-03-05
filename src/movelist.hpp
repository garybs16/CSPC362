#ifndef MOVELIST_HPP
#define MOVELIST_HPP
#include "move.hpp"
#include <iostream>

struct MoveList{
    Move moves[256];
    int count = 0;

    void clear(){
        count = 0;
    }

    void push(const Move& m){
        if (count < 256) {
            moves[count++] = m;
        }
    }

    const Move* find(int from, int to, int flags = -1) const {
        for (int i = 0; i < count; ++i) {
            if (moves[i].getFrom() != from || moves[i].getTo() != to) {
                continue;
            }
            if (flags != -1 && moves[i].getFlags() != flags) {
                continue;
            }
            return &moves[i];
        }
        return nullptr;
    }

    bool contains(int from, int to, int flags = -1) const {
        return find(from, to, flags) != nullptr;
    }

    void printList(){
        for(int i{0}; i < count; i++){
            std::cout << "f: " << moves[i].getFrom() <<" "<<"to: " <<moves[i].getTo() << std::endl;
        }
    }
};
#endif
