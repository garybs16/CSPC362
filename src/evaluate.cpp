#include "evaluate.hpp"

namespace
{
	const int pieceValues[12] = 
	{10, 30, 30, 50, 90, 0,
	-10, -30, -30, -50, -90, 0};

    const int pawnTable[64] =
    {
         0,  0,  0,  0,  0,  0,  0,  0,
         1,  1,  1, -2, -2,  1,  1,  1,
         1, -1, -1,  0,  0, -1, -1,  1,
         0,  0,  0,  2,  2,  0,  0,  0,
         1,  1,  2,  3,  3,  2,  1,  1,
         2,  2,  3,  4,  4,  3,  2,  2,
         5,  5,  5,  5,  5,  5,  5,  5,
         0,  0,  0,  0,  0,  0,  0,  0
    };

    const int knightTable[64] =
    {
        -4, -3, -2, -2, -2, -2, -3, -4,
        -3, -1,  0,  1,  1,  0, -1, -3,
        -2,  1,  2,  3,  3,  2,  1, -2,
        -2,  0,  3,  4,  4,  3,  0, -2,
        -2,  1,  3,  4,  4,  3,  1, -2,
        -2,  0,  2,  3,  3,  2,  0, -2,
        -3, -1,  0,  0,  0,  0, -1, -3,
        -4, -3, -2, -2, -2, -2, -3, -4
    };

    const int bishopTable[64] =
    {
        -2, -1, -1, -1, -1, -1, -1, -2,
        -1,  0,  0,  0,  0,  0,  0, -1,
        -1,  0,  1,  2,  2,  1,  0, -1,
        -1,  1,  2,  3,  3,  2,  1, -1,
        -1,  1,  2,  3,  3,  2,  1, -1,
        -1,  0,  1,  2,  2,  1,  0, -1,
        -1,  1,  0,  0,  0,  0,  1, -1,
        -2, -1, -1, -1, -1, -1, -1, -2
    };

    const int rookTable[64] =
    {
         0,  0,  1,  2,  2,  1,  0,  0,
         0,  0,  1,  2,  2,  1,  0,  0,
         0,  0,  1,  2,  2,  1,  0,  0,
         0,  0,  1,  2,  2,  1,  0,  0,
         0,  0,  1,  2,  2,  1,  0,  0,
         0,  0,  1,  2,  2,  1,  0,  0,
         1,  1,  2,  3,  3,  2,  1,  1,
         0,  0,  0,  1,  1,  0,  0,  0
    };

    const int queenTable[64] =
    {
        -2, -1, -1,  0,  0, -1, -1, -2,
        -1,  0,  0,  1,  1,  0,  0, -1,
        -1,  0,  1,  1,  1,  1,  0, -1,
         0,  1,  1,  2,  2,  1,  1,  0,
         0,  1,  1,  2,  2,  1,  1,  0,
        -1,  1,  1,  1,  1,  1,  0, -1,
        -1,  0,  1,  0,  0,  0,  0, -1,
        -2, -1, -1,  0,  0, -1, -1, -2
    };

    const int kingTable[64] =
    {
        -3, -4, -4, -5, -5, -4, -4, -3,
        -3, -4, -4, -5, -5, -4, -4, -3,
        -3, -4, -4, -5, -5, -4, -4, -3,
        -3, -4, -4, -5, -5, -4, -4, -3,
        -2, -3, -3, -4, -4, -3, -3, -2,
        -1, -2, -2, -2, -2, -2, -2, -1,
         2,  2,  0,  0,  0,  0,  2,  2,
         3,  4,  1,  0,  0,  1,  4,  3
    };

    int mirrorSquare(int square) {
        return (square ^ 56);
	}

    int popLeastSingleBit(uint64_t& bitboard)
    {
        unsigned long square;
        _BitScanForward64(&square, bitboard);
        bitboard &= (bitboard - 1);
        return static_cast<int>(square);
    }
}

Evaluate::Evaluate(const Board& currentBoard) : board(currentBoard), materialScore(0), positionScore(0),totalScore(0)
{}

void Evaluate::materialBalance() 
{
	materialScore = 0;

	for (int pieceId = 0; pieceId < 12; ++pieceId) {
		uint64_t pieces = board.piece_bitboard[pieceId];
		while (pieces) {
			popLeastSingleBit(pieces);
			materialScore += pieceValues[pieceId];
		}
	}
	return;
}

void Evaluate::positionBalance()
{
	positionScore = 0;

    // White Pieces

	uint64_t whitePawns = board.piece_bitboard[P];
	while (whitePawns)
	{
		int squareId = popLeastSingleBit(whitePawns);
		positionScore += pawnTable[squareId];
	}

	uint64_t whiteKnights = board.piece_bitboard[N];
    while (whiteKnights)
    {
		int squareId = popLeastSingleBit(whiteKnights);
		positionScore += knightTable[squareId];
    }

	uint64_t whiteBishops = board.piece_bitboard[B];
    while (whiteBishops)
    {
        int squareId = popLeastSingleBit(whiteBishops);
        positionScore += bishopTable[squareId];
	}

	uint64_t whiteRooks = board.piece_bitboard[R];
    while (whiteRooks)
    {
        int squareId = popLeastSingleBit(whiteRooks);
        positionScore += rookTable[squareId];
	}

    uint64_t whiteQueens = board.piece_bitboard[Q];
    while (whiteQueens)
    {
        int squareId = popLeastSingleBit(whiteQueens);
        positionScore += queenTable[squareId];
	}

    uint64_t whiteKing = board.piece_bitboard[K];
    while (whiteKing)
    {
        int squareId = popLeastSingleBit(whiteKing);
        positionScore += kingTable[squareId];
    }
	// Black Pieces

	uint64_t blackPawns = board.piece_bitboard[bP];
    while (blackPawns)
    {
        int squareId = popLeastSingleBit(blackPawns);
		positionScore -= pawnTable[mirrorSquare(squareId)];
    }

	uint64_t blackKnights = board.piece_bitboard[bN];
    while (blackKnights)
    {
        int squareId = popLeastSingleBit(blackKnights);
        positionScore -= knightTable[mirrorSquare(squareId)];
	}

    uint64_t blackBishops = board.piece_bitboard[bB];
    while (blackBishops)
    {
        int squareId = popLeastSingleBit(blackBishops);
        positionScore -= bishopTable[mirrorSquare(squareId)];
	}

    uint64_t blackRooks = board.piece_bitboard[bR];
    while (blackRooks)
    {
        int squareId = popLeastSingleBit(blackRooks);
        positionScore -= rookTable[mirrorSquare(squareId)];
    }
    uint64_t blackQueens = board.piece_bitboard[bQ];
    while (blackQueens)
    {
        int squareId = popLeastSingleBit(blackQueens);
        positionScore -= queenTable[mirrorSquare(squareId)];
    }
    uint64_t blackKing = board.piece_bitboard[bK];
    while (blackKing)
    {
        int squareId = popLeastSingleBit(blackKing);
        positionScore -= kingTable[mirrorSquare(squareId)];
	}
}

void Evaluate::evaluateTotal()
{
	materialBalance();
	positionBalance();
	totalScore = materialScore + positionScore;
}