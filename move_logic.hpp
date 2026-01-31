#ifndef MOVE_LOGIC_HPP
#define MOVE_LOGIC_HPP
#include <cstdint>
#include <cstddef>
#include "board.hpp"

namespace moveLogic 
{
	uint64_t allOccupied(const Board& board) 
	{
		return board.occupancy[0] | board.occupancy[1];
	}
	uint64_t teamOccupied(const Board& board)
	{
		return whiteTurn ? board.occupancy[0] : board.occupancy[1];
	}
	uint64_t enemyOccupied(const Board& board)
	{
		return whiteTurn ?  board.occupancy[1]: board.occupancy[0];
	}
	bool emptySquares(const Board& board, int squareIndex)
	{
		return (alloccupied(board) & (1ull << squareIndex)) == 0ull;
	}
}

bool isPawnMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);
bool isKnightMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);
bool isBishopMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);
bool isRookMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);
bool isQueenMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);
bool isKingMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn);

#endif
