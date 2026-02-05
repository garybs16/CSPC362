#include "movegen.hpp"
#include "move.hpp"
#include "movelist.hpp"
#include <bit>
#include "def.hpp"
using namespace Masks;
// Pawn
inline int popLSB(uint64_t& bitboard){
  int index = std::__countr_zero(bitboard);
  bitboard &= ~(1ULL << index);
  return index;
}
void MoveGen::addMoves(uint64_t targets, int from, int flags, MoveList& ml) {
    while (targets) {
        int to = popLSB(targets);
        ml.push(Move(from, to, flags));
    }
}
void MoveGen::addPawnMoves(uint64_t target, int shift_, int flags_ , MoveList& movelist){
  
  while(target){
    int to_ = popLSB(target);
    int from_ = to_ - shift_;
    movelist.push(Move(from_, to_, flags_));
  };
}
void MoveGen::genPawn(Board& board, MoveList& movelist)
{
    uint64_t empty = ~board.occupancy[dualOccupancy];
    
    bool sideToMove = board.sideToMove;

   
    uint64_t pushOne = (board.piece_bitboard[P] << 8) & empty;
    uint64_t pushTwo = (((board.piece_bitboard[P] << 16) & RANK_3) << 8) & empty;
    addPawnMoves(pushOne, 8, 0, movelist);
    addPawnMoves(pushTwo, 16, 0, movelist);
  }


