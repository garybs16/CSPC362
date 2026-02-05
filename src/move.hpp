#ifndef MOVE_HPP
#define MOVE_HPP
#include <cstdint>
struct Move{
    //bits 0-5: squares from (0-63)
    //6-11: desination square (0-63)
    //12-15: flag
    uint16_t moveData_;
    Move(int from, int to, int flags = 0){
        moveData_ = (from) | (to << 6) | (flags << 12);
    };
    Move() : moveData_(0) {};
    
    /// 0x3F is 63 in hex 0x3F = 0011 1111
    //example data = 0111 1011 1101 0011 
    // actually seperated like this | 0111  |[101111] | [010011]
    //                              | flags |to square| from square
    // data >> 6 =   0000 0000 0111 1011 
    // data & 0x3F (111111) = 0000 0000 0111 1011  
    //                       &            11 1111 = 0000000000[111011] = 59
    int getFrom() const {
        return moveData_ & 0x3F;
    };
    int getTo() const {
        return (moveData_ >> 6) & 0x3F;
    }
    int getFlags() const {
        return (moveData_ >> 12); 
    } // current flags up for change
    // 0000 (0) = default move (same as constructor)
    // 0001 (1) Double pawn push
    // 0010 (2) King side castle
    // 0011 (3) Queen side castle
    // 0100 (4) Capture 
    

};
#endif