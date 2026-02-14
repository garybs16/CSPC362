#include "movegen.hpp"
#include "move.hpp"
#include "movelist.hpp"
#include "magic.hpp"
#include <bit>
#include "def.hpp"
using namespace Masks;
// Pawn
inline int popLSB(uint64_t& bitboard){
  int index = std::countr_zero(bitboard);
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
void MoveGen::genKnight(Board& board, MoveList& movelist)
{
    uint64_t empty = ~board.occupancy[dualOccupancy];
    int knightsColor = board.sideToMove ? bN : N;
    uint64_t knights = board.piece_bitboard[knightsColor];
    uint64_t allies = board.occupancy[board.sideToMove ? blackOccupancy : whiteOccupancy];
    uint64_t enemy = board.occupancy[board.sideToMove ? whiteOccupancy : blackOccupancy];
    bool sideToMove = board.sideToMove;

    while (knights) {
        int from = popLSB(knights);
        uint64_t position = 1ULL << from;
        uint64_t attacks = 0ULL;

        attacks |= (position << 17) & ~File_H & ~allies;
        attacks |= (position << 10) & ~(File_G | File_H) & ~allies;
        attacks |= (position >> 15) & ~File_A & ~allies;
        attacks |= (position >> 6) & ~(File_G | File_H) & ~allies;
        attacks |= (position << 15) & ~File_A & allies;
        attacks |= (position << 6) & (File_A | File_B) & ~allies;
        attacks |= (position >> 17) & ~File_H & ~allies;
        attacks |= (position >> 10) & ~(File_A | File_B) & ~allies;

        uint64_t capture = attacks & enemy;
        uint64_t notCapture = attacks & ~enemy;
        if (capture) {
            addMoves(capture, from, 4, movelist);
        }
        if (notCapture) {
            addMoves(notCapture, from, 0, movelist);
        }
    }
}

uint64_t MoveGen::maskRookAttacks(int square) {
    uint64_t attacks = 0ULL;
    int r = square / 8;
    int f = square % 8;

    for (int r2 = r + 1; r2 < 7; r2++) attacks |= (1ULL << (r2 * 8 + f));
    for (int r2 = r - 1; r2 > 0; r2--) attacks |= (1ULL << (r2 * 8 + f));
    for (int f2 = f + 1; f2 < 7; f2++) attacks |= (1ULL << (r * 8 + f2));
    for (int f2 = f - 1; f2 > 0; f2--) attacks |= (1ULL << (r * 8 + f2));
    return attacks;
}
// Generating sliding pieces attacks map
// Runs only onece when starts up to fill the attack tables for magic bitboards
uint64_t MoveGen::generateRookAttacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
	int r = square / 8;
	int f = square % 8;

    for (int r2 = r + 1; r2 <= 7; r2++)
    {
        attacks |= (1ULL << (r2 * 8 + f));
		if (occupancy & (1ULL << (r2 * 8 + f))) break;
    }
    for (int r2 = r - 1; r2 >= 0; r2--)
    {
		attacks |= (1ULL << (r2 * 8 + f));
        if (occupancy & (1ULL << (r2 * 8 + f))) break;
    }
    for (int f2 = f + 1; f2 <= 7; f2++)
	{
        attacks |= (1ULL << (r * 8 + f2));
        if (occupancy & (1ULL << (r * 8 + f2))) break;
    }
    for (int f2 = f - 1; f2 >= 0; f2--)
    {
        attacks |= (1ULL << (r * 8 + f2));
        if (occupancy & (1ULL << (r * 8 + f2))) break;
    }
	return attacks;
}
uint64_t MoveGen::generateBishopAttacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int r = square / 8;
    int f = square % 8;
    for (int r2 = r + 1, f2 = f + 1; r2 <= 7 && f2 <= 7; r2++, f2++)
    {
        attacks |= (1ULL << (r2 * 8 + f2));
        if (occupancy & (1ULL << (r2 * 8 + f2))) break;
    }
    for (int r2 = r + 1, f2 = f - 1; r2 <= 7 && f2 >= 0; r2++, f2--)
    {
        attacks |= (1ULL << (r2 * 8 + f2));
        if (occupancy & (1ULL << (r2 * 8 + f2))) break;
    }
    for (int r2 = r - 1, f2 = f + 1; r2 >= 0 && f2 <= 7; r2--, f2++)
    {
        attacks |= (1ULL << (r2 * 8 + f2));
        if (occupancy & (1ULL << (r2 * 8 + f2))) break;
    }
    for (int r2 = r - 1, f2 = f - 1; r2 >= 0 && f2 >= 0; r2--, f2--)
    {
        attacks |= (1ULL << (r2 * 8 + f2));
        if (occupancy & (1ULL << (r2 * 8 + f2))) break;
    }
    return attacks;
}

// Attacks
uint64_t MoveGen::getRookAttacks(int square, uint64_t occupancy) {
    uint64_t mask = rookMasks[square];
    uint64_t occupancyBits = occupancy & mask;
    int magicIndex = (occupancyBits * RookMagics[square]) >> (64 - RookBits[square]);
    return rookAttackTable[square][magicIndex];
}
uint64_t MoveGen::getBishopAttacks(int square, uint64_t occupancy) {
    uint64_t mask = bishopMasks[square];
    uint64_t occupancyBits = occupancy & mask;
    int magicIndex = (occupancyBits * BishopMagics[square]) >> (64 - BishopBits[square]);
    return bishopAttackTable[square][magicIndex];
uint64_t MoveGen::getQueenAttacks(int square, uint64_t occupancy) {
    return getRookAttacks(square, occupancy) | getBishopAttacks(square, occupancy);
}
}

void MoveGen::includeMagic()
{
    for (int bd = 0; bd < 64; bd++)
    {
        rookMasks[bd] = maskRookAttacks(bd);
		bishopMasks[bd] = maskBishopAttacks(bd);
		int rookBits = RookBits[bd];
		int bishopBits = BishopBits[bd];
		int rookOccupancyVariations = 1 << rookBits;
		int bishopOccupancyVariations = 1 << bishopBits;

        for (int i = 0; i < bishopOccupancyVariations; i++)
        {
            uint64_t occupancy = 0ULL;
            uint64_t mask = bishopMasks[bd];
            int boardIndex = i;
            while (mask) {
                int bitIndex = popLSB(mask);
                mask &= ~(1ULL << bitIndex);
                if (boardIndex & 1) {
                    occupancy |= (1ULL << bitIndex);
                }
				boardIndex >>= 1;
            }
			uint64_t magicIndex = (occupancy * BishopMagics[bd]) >> (64 - bishopBits);
			uint64_t attacks = generateBishopAttacks(bd, occupancy);
			bishopAttackTable[bd][magicIndex] = attacks;
        }
        for (int i = 0; i < rookOccupancyVariations; i++) {
            uint64_t occupancy = 0ULL;
            uint64_t mask = rookMasks[bd];
            int boardIndex = i;

            while (mask) {
				int bitIndex = popLSB(mask);
				mask &= ~(1ULL << bitIndex);
                if (boardIndex & 1) {
                    occupancy |= (1ULL << bitIndex);
                }
				boardIndex >>= 1;
            }
			uint64_t magicIndex = (occupancy * RookMagics[bd]) >> (64 - rookBits);
			uint64_t attacks = generateRookAttacks(bd, occupancy);
			rookAttackTable[bd][magicIndex] = attacks;
        }
    }
}
// Sliding Pieces
void MoveGen::genSlide(Board& board, MoveList& movelist)
{
	uint64_t empty = ~board.occupancy[dualOccupancy];
	int bishopsColor = board.sideToMove ? bB : B;
	int rooksColor = board.sideToMove ? bR : R;
	int queensColor = board.sideToMove ? bQ : Q;
    uint64_t bishops = board.piece_bitboard[bishopsColor];
    uint64_t rooks = board.piece_bitboard[rooksColor];
	uint64_t queens = board.piece_bitboard[queensColor];
    
	uint64_t allies = board.occupancy[board.sideToMove ? blackOccupancy : whiteOccupancy];
	uint64_t enemy = board.occupancy[board.sideToMove ? whiteOccupancy : blackOccupancy];
	uint64_t occupancy = board.occupancy[dualOccupancy];

    while (bishops)
    {
        int from = popLSB(bishops);
        uint64_t attacks = getBishopAttacks(from, occupancy) & ~allies;
        uint64_t capture = attacks & enemy;
        uint64_t notCapture = attacks & ~enemy;
        if (capture) {
            addMoves(capture, from, 4, movelist);
        }
        if (notCapture) {
            addMoves(notCapture, from, 0, movelist);
		}
    }
    while (rooks)
    {
        int from = popLSB(rooks);
        uint64_t attacks = getRookAttacks(from, occupancy) & ~allies;
        uint64_t capture = attacks & enemy;
        uint64_t notCapture = attacks & ~enemy;
        if (capture) {
            addMoves(capture, from, 4, movelist);
        }
        if (notCapture) {
            addMoves(notCapture, from, 0, movelist);
        }
    }
    while (queens)
    {
        int from = popLSB(queens);
        uint64_t attacks = (getBishopAttacks(from, occupancy) | getRookAttacks(from, occupancy)) & ~allies;
        uint64_t capture = attacks & enemy;
        uint64_t notCapture = attacks & ~enemy;
        if (capture) {
            addMoves(capture, from, 4, movelist);
        }
        if (notCapture) {
            addMoves(notCapture, from, 0, movelist);
        }
    }
    
}


//
// =========================
// KING MOVE GENERATION ADDED
// =========================
//
void MoveGen::genKing(Board& board, MoveList& movelist)
{
    int kingPiece = board.sideToMove ? bK : K;
    uint64_t king = board.piece_bitboard[kingPiece];

    if (!king) return;

    int from = popLSB(king);
    uint64_t position = 1ULL << from;

    uint64_t allies = board.occupancy[board.sideToMove ? blackOccupancy : whiteOccupancy];
    uint64_t enemy = board.occupancy[board.sideToMove ? whiteOccupancy : blackOccupancy];

    uint64_t attacks = 0ULL;

    // Vertical
    attacks |= (position << 8);
    attacks |= (position >> 8);

    // Horizontal
    attacks |= (position << 1) & ~File_A;
    attacks |= (position >> 1) & ~File_H;

    // Diagonals
    attacks |= (position << 9) & ~File_A;
    attacks |= (position << 7) & ~File_H;
    attacks |= (position >> 7) & ~File_A;
    attacks |= (position >> 9) & ~File_H;

    // Remove friendly squares
    attacks &= ~allies;

    uint64_t capture = attacks & enemy;
    uint64_t notCapture = attacks & ~enemy;

    if (capture) {
        addMoves(capture, from, 4, movelist);
    }
    if (notCapture) {
        addMoves(notCapture, from, 0, movelist);
    }
}

// Attack check
bool MoveGen::isSquareAttacked(Board& board, int square) 
{
    uint64_t occupancy = board.occupancy[dualOccupancy];
    uint64_t enemyPawns = board.piece_bitboard[byWhite ? P : bP];
    uint64_t enemyKnights = board.piece_bitboard[byWhite ? N : bN];
    uint64_t enemyBishops = board.piece_bitboard[byWhite ? B : bB];
    uint64_t enemyRooks = board.piece_bitboard[byWhite ? R : bR];
    uint64_t enemyQueens = board.piece_bitboard[byWhite ? Q : bQ];
    uint64_t enemyKing = board.piece_bitboard[byWhite ? K : bK];

    // Pawn attacks
    if (byWhite) {
        if ((enemyPawns << 7) & ~File_H & (1ULL << square)) return true;
        if ((enemyPawns << 9) & ~File_A & (1ULL << square)) return true;
    } else {
        if ((enemyPawns >> 7) & ~File_A & (1ULL << square)) return true;
        if ((enemyPawns >> 9) & ~File_H & (1ULL << square)) return true;
    }
    // Knight attacks
    uint64_t position = 1ULL << squre;
	uint64_t knightAttacks = 0ULL;
	knightAttacks |= (position << 17) & ~File_H & enemyKnights;
	knightAttacks |= (position << 10) & ~(File_G | File_H) & enemyKnights;
	knightAttacks |= (position >> 15) & ~File_A & enemyKnights;
	knightAttacks |= (position >> 6) & ~(File_G | File_H) & enemyKnights;
	knightAttacks |= (position << 15) & ~File_A & enemyKnights;
	knightAttacks |= (position << 6) & ~(File_A | File_B) & enemyKnights;
	knightAttacks |= (position >> 17) & ~File_H & enemyKnights;
	knightAttacks |= (position >> 10) & ~(File_A | File_B) & enemyKnights;
	if (knightAttacks) return true;

    // Bishop attacks
    if ((getBishopAttacks(square, occupancy) & (enemyBishops | enemyQueens)) != 0) return true;

    // Rook attacks
    if ((getRookAttacks(square, occupancy) & (enemyRooks | enemyQueens)) != 0) return true;

	// Queen attacks
	if ((getQueenAttacks(square, occupancy) & enemyQueens) != 0) return true;

    // King attacks
	uint64_t kingAttacks = 0ULL;
	kingAttacks |= (position << 8) & enemyKing;
	kingAttacks |= (position << 8) & enemyKing;
	kingAttacks |= (position >> 1) & ~File_A & enemyKing;
	kingAttacks |= (position >> 1) & ~File_H & enemyKing;
	kingAttacks |= (position << 9) & ~File_A & enemyKing;
	kingAttacks |= (position << 7) & ~File_A & enemyKing;
	kingAttacks |= (position >> 9) & ~File_H & enemyKing;
	kingAttacks |= (position >> 7) & ~File_H & enemyKing;
	if (kingAttacks) return true;

    return false;
}

// Castling
void MoveGen::genCastling(Board& board, MoveList& movelist) 
{
    if (board.sideToMove == white) 
    {
        // White kingside
        if ((board.castlingRights & 1) && !(board.occupancy[dualOccupancy] & (1ULL << 5 | 1ULL << 6)) && !isSquareAttacked(board, 4) && !isSquareAttacked(board, 5) && !isSquareAttacked(board, 6)) {
            movelist.push(Move(4, 6, 2)); 
        }
        // White queenside
        if ((board.castlingRights & 2) && !(board.occupancy[dualOccupancy] & (1ULL << 1 | 1ULL << 2 | 1ULL << 3)) && !isSquareAttacked(board, 4) && !isSquareAttacked(board, 3) && !isSquareAttacked(board, 2)) {
            movelist.push(Move(4, 2, 2)); 
        }
    } 
    else 
    {
        // Black kingside
        if ((board.castlingRights & 4) && !(board.occupancy[dualOccupancy] & (1ULL << 61 | 1ULL << 62)) && !isSquareAttacked(board, 60) && !isSquareAttacked(board, 61) && !isSquareAttacked(board, 62)) {
            movelist.push(Move(60, 62, 2)); 
        }
        // Black queenside
        if ((board.castlingRights & 8) && !(board.occupancy[dualOccupancy] & (1ULL << 57 | 1ULL << 58 | 1ULL << 59)) && !isSquareAttacked(board, 60) && !isSquareAttacked(board, 59) && !isSquareAttacked(board, 58)) {
            movelist.push(Move(60, 58, 2)); 
        }
    }
}


// gen all

void MoveGen::generateAll(Board& board, MoveList& ml){
  nullptr;
}
