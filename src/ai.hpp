#ifndef AI_HPP
#define AI_HPP

#include <random>
#include "board.hpp"
#include "movegen.hpp"

enum class RobotDifficulty {
    Easy,
    Medium,
    Hard,
    Grandmaster
};

class ChessAi {
    public:
        ChessAi();

        bool findMove(const Board& board, const MoveGen& moveGen, RobotDifficulty difficulty, Move& outMove) const;

    private:
        struct SearchResult {
            Move move;
            int score = 0;
            bool hasMove = false;
        };

        int evaluate(const Board& board) const;
        int scoreMove(const Board& board, const Move& move) const;
        int depthForDifficulty(RobotDifficulty difficulty) const;
        int candidateCountForDifficulty(RobotDifficulty difficulty) const;
        SearchResult negamax(const Board& board, const MoveGen& moveGen, int depth, int alpha, int beta) const;

        mutable std::mt19937 random_;
};

#endif
