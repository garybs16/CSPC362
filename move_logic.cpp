#include "move_logic.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>


bool isPawnMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	if (whiteTurn)
	{
		// Single square move
		if (dest == from + 8 && emptySquares(board, dest))
			return true;
		// Double square move from starting position
		if (from / 8 == 1 && dest == from + 16 && emptySquares(board, dest) && emptySquares(board, from + 8))
			return true;
		// Capture moves
		if ((dest == from + 7 || dest == from + 9) && (blackOccupied(board) & (1ull << dest)))
		{
			if (pawnLogic::blackOccupied(board) & (1ull << dest))
				return true;
		}
	}
	else
	{
		// Single square move
		if (dest == from - 8 && emptySquares(board, dest))
			return true;
		// Double square move from starting position
		if (from / 8 == 6 && dest == from - 16 && emptySquares(board, dest) && emptySquares(board, from - 8))
			return true;
		// Capture moves
		if ((dest == from - 7 || dest == from - 9) && (whiteOccupied(board) & (1ull << dest)))
		{
			if (pawnLogic::whiteOccupied(board) & (1ull << dest))
				return true;
		}
	}

	return false;

}

bool isKnightMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	int rowFrom = from / 8;
	int colFrom = from % 8;
	int rowDest = dest / 8;
	int colDest = dest % 8;
	int rowDiff = abs(rowFrom - rowDest);
	int colDiff = abs(colFrom - colDest);

	// Check for L-shaped move
	if (!((rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2)))
		return false;

	// Check if destination square is occupied by own piece
	if (whiteTurn)
	{
		if (whiteOccupied(board) & (1ull << dest))
			return false;
	}
	else
	{
		if (blackOccupied(board) & (1ull << dest))
			return false;
	}
	return true;
}

bool isBishopMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	int rowFrom = from / 8;
	int colFrom = from % 8;
	int rowDest = dest / 8;
	int colDest = dest % 8;
	int rowDiff = abs(rowFrom - rowDest);
	int colDiff = abs(colFrom - colDest);

	// Check for diagonal move - equal row and column differences
	if (rowDiff != colDiff)
		// Not a diagonal move - should be equal to be valid
		return false;

	// Check if destination square is occupied by own piece
	if (whiteTurn)
	{
		if (whiteOccupied(board) & (1ull << dest))
			return false;
	}
	else
	{
		if (blackOccupied(board) & (1ull << dest))
			return false;
	}
	// Check for obstructions
	int rowStep = (rowDest > rowFrom) ? 1 : -1;
	int colStep = (colDest > colFrom) ? 1 : -1;
	int r = rowFrom + rowStep;
	int c = colFrom + colStep;
	while (r != rowDest && c != colDest)
	{
		int square = r * 8 + c;
		if (!emptySquares(board, square))
			return false;
		r += rowStep;
		c += colStep;
	}
	return true;
}

bool isRookMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	int rowFrom = from / 8;
	int colFrom = from % 8;
	int rowDest = dest / 8;
	int colDest = dest % 8;
	// Check for straight move
	if (rowFrom != rowDest && colFrom != colDest)
		return false;
	// Check if destination square is occupied by own piece
	if (whiteTurn)
	{
		if (whiteOccupied(board) & (1ull << dest))
			return false;
	}
	else
	{
		if (blackOccupied(board) & (1ull << dest))
			return false;
	}
	// Check for obstructions
	if (rowFrom == rowDest)
	{
		int step = (colDest > colFrom) ? 1 : -1;
		for (int c = colFrom + step; c != colDest; c += step)
		{
			int square = rowFrom * 8 + c;
			if (!emptySquares(board, square))
				return false;
		}
	}
	else
	{
		int step = (rowDest > rowFrom) ? 1 : -1;
		for (int r = rowFrom + step; r != rowDest; r += step)
		{
			int square = r * 8 + colFrom;
			if (!emptySquares(board, square))
				return false;
		}
	}
	return true;
}

bool isQueenMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	// Queen's move is a combination of Rook and Bishop
	return isRookMoveLegal(board, piece_, from, dest, whiteTurn) || 
		isBishopMoveLegal(board, piece_, from, dest, whiteTurn);
}

bool isKingMoveLegal(const Board& board, int piece_, int from, int dest, bool whiteTurn)
{
	int rowFrom = from / 8;
	int colFrom = from % 8;
	int rowDest = dest / 8;
	int colDest = dest % 8;
	int rowDiff = abs(rowFrom - rowDest);
	int colDiff = abs(colFrom - colDest);
	// Check for one square move in any direction
	if (rowDiff > 1 || colDiff > 1)
		return false;
	// Check if destination square is occupied by own piece
	if (whiteTurn)
	{
		if (whiteOccupied(board) & (1ull << dest))
			return false;
	}
	else
	{
		if (blackOccupied(board) & (1ull << dest))
			return false;
	}
	return true;
}