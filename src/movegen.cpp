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
void MoveGen::toMove(uint64_t target, int shift_, int flags_ , MoveList& movelist){
  
  while(target){
    int to_ = popLSB(target);
    int from_ = to_ - shift_;
    movelist.push(Move(from_, to_, flags_));
  };
}
MoveList MoveGen::genPawn(Board& board, MoveList& movelist)
{
    uint64_t empty = ~board.occupancy[dualOccupancy];
    
    bool sideToMove = board.sideToMove;

   
    uint64_t pushOne = (board.piece_bitboard[P] << 8) & empty;
    uint64_t pushTwo = (((board.piece_bitboard[P] << 16) & RANK_3) << 8) & empty;
    
    toMove(pushTwo, 8, 0, movelist);
    return movelist;
  }
