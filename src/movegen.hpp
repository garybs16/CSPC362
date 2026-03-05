#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "board.hpp"
#include "movelist.hpp"

class MoveGen{
    public:
        void includeMagic();
        void generateAll(const Board& board, MoveList& ml) const;
        bool isInCheck(const Board& board, bool blackSide) const;

    private:
        void generatePseudoLegal(const Board& board, MoveList& ml) const;
        void addPromotionMoves(MoveList& movelist, int from, int to, bool capture) const;
        void genPawn(const Board& board, MoveList& movelist) const;
        void genKnight(const Board& board, MoveList& movelist) const;
        void genSlide(const Board& board, MoveList& movelist) const;
        void genKing(const Board& board, MoveList& movelist) const;
        void genCastling(const Board& board, MoveList& movelist) const;
        bool isSquareAttacked(const Board& board, int square, bool byBlack) const;
};

#endif
