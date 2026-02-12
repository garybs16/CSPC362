#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP
#include <iostream>
#include <vector>
#include "board.hpp"
#include "movelist.hpp"

class MoveGen{
    public:
        void includeMagic();
        void generateAll(Board& board, MoveList& ml);
		uint64_t rookAttackMap(int square, uint64_t occupancy);
		uint64_t bishopAttackMap(int square, uint64_t occupancy);
        uint64_t getRookAttacks(int square, uint64_t occupancy);
        uint64_t getBishopAttacks(int square, uint64_t occupancy);
        uint64_t getQueenAttacks(int square, uint64_t occupancy);
		
    private:
        // Magic bitboard data
        uint64_t rookMasks[64];
        uint64_t bishopMasks[64];
		// Attack tables
		uint64_t rookAttackTable[64][4096]; // 4096 = 2^12 (max occupancy variations for rook)
		uint64_t bishopAttackTable[64][512];  // 512 = 2^9 (max occupancy variations for bishop)
		// Move generation helpers
		uint64_t maskRookAttacks(int square);
		uint64_t maskBishopAttacks(int square);
		uint64_t generateRookAttacks(int square, uint64_t occupancy);
		uint64_t generateBishopAttacks(int square, uint64_t occupancy);

        void addMoves(uint64_t targets, int from, int flags, MoveList& ml);
        void addPawnMoves(uint64_t target, int shift_, int flags_, MoveList& movelist);
        void genPawn(Board& board, MoveList& movelist);
        void genKnight(Board& board, MoveList& movelist);
        void genSlide(Board& board, MoveList& movelist); // Bihsop, Rook, Queen
		void genKing(Board& board, MoveList& movelist);
};



#endif
