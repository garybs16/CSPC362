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
    uint64_t pawns = board.piece_bitboard[P];
    uint64_t enemy = board.occupancy[board.sideToMove ? whiteOccupancy : blackOccupancy];
    bool sideToMove = board.sideToMove;

    if (!board.sideToMove){
      uint64_t pushOne = (pawns << 8) & empty;
      uint64_t pushTwo = ((pushOne & RANK_3) << 8) & empty;
      addPawnMoves(pushOne, 8, 0, movelist);
      addPawnMoves(pushTwo, 16, 1, movelist);

      uint64_t captureLeft = (pawns << 7) & ~File_H & enemy;
      uint64_t captureRight = (pawns << 9) & ~File_A & enemy;
      addPawnMoves(captureLeft, 7, 4, movelist);
      addPawnMoves(captureRight, 9, 4, movelist);
    }
    else{
      uint64_t pushOne = (pawns >> 8) & empty;
      uint64_t pushTwo = ((pushOne & RANK_3) >> 8) & empty;
      addPawnMoves(pushOne, -8, 0, movelist);
      addPawnMoves(pushTwo, -16, 1, movelist);

      uint64_t captureLeft = (pawns << 7) & ~File_H & enemy;
      uint64_t captureRight = (pawns << 9) & ~File_A & enemy;
      addPawnMoves(captureLeft, -7, 4, movelist);
      addPawnMoves(captureRight, -9, 4, movelist);
    }
  }

// Knight
void MoveGen::genKnight(Board& board, MoveList& movelist);
{
  uint64_t empty = ~board.occupancy[dualOccupancy];
  uint64_t knights = board.piece_bitboard[N];
  uint64_t allies = board.occupancy[board.sideToMove ? blackOccupancy : whiteOccupancy]
  uint64_t enemy = board.occupancy[board.sideToMove ? whiteOccuoancy : blackOccupancy];

  if (!board.sideToMove){
    int from = popLSB(knights);
    uint64_t position = 1ULL << from;
    uint64_t attacks = 0ULL;

    attacks |= (pos << 17) & ~File_A & ~ allies;
    attacks |= (pos << 10) & ~(File_A | File_B) & ~allies;
    attacks |= (pos >> 15) & ~File_A & ~allies;
    attacks |= (pos >> 6) & ~(File_A | File_B) & ~allies;
    attacks |= (pos << 15) & ~File_H & allies;
    attacks |= (pos << 6) & (File_H | File_G) & ~allies;
    attacks |= (pos >> 17) & ~File_H & ~allies;
    attacks |= (pos >> 10) & ~(File_H | File_G) & ~allies;

    uint64_t capture = attacks & enemy;
    uint64_t notCapture = attacks & ~enemy;
    if (capture) {
      addMoves(capture, form, 4, movelist);
    if (notCapture){
      addMoves(notCapture, from, 0, movelist);
    }
    }
  }
}


