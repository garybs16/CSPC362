#include "ai.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>

namespace {
constexpr int kInfinity = 1'000'000;
constexpr int kMateScore = 100'000;
constexpr std::array<int, 12> kPieceValues = {
    100, 320, 330, 500, 900, 0,
    100, 320, 330, 500, 900, 0
};

int MirrorRank(int rank) {
    return 7 - rank;
}

int WhitePositionalBonus(int pieceId, int square) {
    const int rank = square / 8;
    const int file = square % 8;
    const int centerDistance = std::abs(file - 3) + std::abs(rank - 3);

    switch (pieceId) {
        case P:
            return (rank * 6) + (3 - std::abs(file - 3));
        case N:
            return 18 - (centerDistance * 4);
        case B:
            return 16 - (centerDistance * 3);
        case R:
            return rank * 2;
        case Q:
            return 10 - (centerDistance * 2);
        case K:
            if (square == 6 || square == 2) {
                return 35;
            }
            return -centerDistance * 2;
        default:
            return 0;
    }
}
}

ChessAi::ChessAi()
    : random_(static_cast<unsigned int>(
          std::chrono::steady_clock::now().time_since_epoch().count())) {
}

bool ChessAi::findMove(const Board& board, const MoveGen& moveGen, RobotDifficulty difficulty, Move& outMove) const {
    MoveList moves;
    moveGen.generateAll(board, moves);
    if (moves.count == 0) {
        return false;
    }

    const int depth = depthForDifficulty(difficulty);
    struct RootMove {
        Move move;
        int score = std::numeric_limits<int>::min();
    };

    std::vector<RootMove> scoredMoves;
    scoredMoves.reserve(moves.count);

    for (int i = 0; i < moves.count; ++i) {
        Board next(board);
        if (!next.makeMove(moves.moves[i])) {
            continue;
        }

        const int score = -negamax(next, moveGen, depth - 1, -kInfinity, kInfinity).score;
        scoredMoves.push_back({moves.moves[i], score});
    }

    if (scoredMoves.empty()) {
        return false;
    }

    std::sort(scoredMoves.begin(), scoredMoves.end(), [](const RootMove& left, const RootMove& right) {
        return left.score > right.score;
    });

    const int requestedCandidates = candidateCountForDifficulty(difficulty);
    const int bestScore = scoredMoves.front().score;
    std::vector<Move> shortlist;
    shortlist.reserve(requestedCandidates);

    for (const RootMove& candidate : scoredMoves) {
        if (static_cast<int>(shortlist.size()) >= requestedCandidates) {
            break;
        }
        if (!shortlist.empty() && (bestScore - candidate.score) > 140) {
            break;
        }
        shortlist.push_back(candidate.move);
    }

    if (shortlist.empty()) {
        outMove = scoredMoves.front().move;
        return true;
    }

    std::uniform_int_distribution<int> distribution(0, static_cast<int>(shortlist.size()) - 1);
    outMove = shortlist[distribution(random_)];
    return true;
}

int ChessAi::evaluate(const Board& board) const {
    int score = 0;
    for (int square = 0; square < 64; ++square) {
        const int pieceId = board.getPieceAt(square);
        if (pieceId == -1) {
            continue;
        }

        const bool blackPiece = pieceId >= bP;
        const int normalizedPiece = blackPiece ? pieceId - 6 : pieceId;
        const int adjustedSquare = blackPiece ? (MirrorRank(square / 8) * 8) + (square % 8) : square;
        const int pieceScore = kPieceValues[pieceId] + WhitePositionalBonus(normalizedPiece, adjustedSquare);

        score += blackPiece ? -pieceScore : pieceScore;
    }

    return board.sideToMove ? -score : score;
}

int ChessAi::scoreMove(const Board& board, const Move& move) const {
    int score = 0;
    const int fromPiece = board.getPieceAt(move.getFrom());
    int capturedPiece = board.getPieceAt(move.getTo());

    if (move.getFlags() == EnPassant) {
        capturedPiece = board.sideToMove ? P : bP;
    }

    if (capturedPiece != -1) {
        score += (kPieceValues[capturedPiece] * 10) - kPieceValues[fromPiece];
    }
    if (move.isPromotion()) {
        score += 800;
    }
    if (move.isCastle()) {
        score += 50;
    }
    return score;
}

int ChessAi::depthForDifficulty(RobotDifficulty difficulty) const {
    switch (difficulty) {
        case RobotDifficulty::Easy:
            return 2;
        case RobotDifficulty::Medium:
            return 3;
        case RobotDifficulty::Hard:
            return 4;
        case RobotDifficulty::Grandmaster:
            return 5;
    }
    return 2;
}

int ChessAi::candidateCountForDifficulty(RobotDifficulty difficulty) const {
    switch (difficulty) {
        case RobotDifficulty::Easy:
            return 2;
        case RobotDifficulty::Medium:
        case RobotDifficulty::Hard:
        case RobotDifficulty::Grandmaster:
            return 1;
    }
    return 1;
}

ChessAi::SearchResult ChessAi::negamax(const Board& board, const MoveGen& moveGen, int depth, int alpha, int beta) const {
    MoveList moves;
    moveGen.generateAll(board, moves);

    if (moves.count == 0) {
        if (moveGen.isInCheck(board, board.sideToMove)) {
            return {Move(), -kMateScore - depth, false};
        }
        return {Move(), 0, false};
    }

    if (depth == 0) {
        return {moves.moves[0], evaluate(board), true};
    }

    std::vector<Move> orderedMoves(moves.moves, moves.moves + moves.count);
    std::sort(orderedMoves.begin(), orderedMoves.end(), [&](const Move& left, const Move& right) {
        return scoreMove(board, left) > scoreMove(board, right);
    });

    SearchResult bestResult = {orderedMoves.front(), -kInfinity, true};

    for (const Move& move : orderedMoves) {
        Board next(board);
        if (!next.makeMove(move)) {
            continue;
        }

        const int score = -negamax(next, moveGen, depth - 1, -beta, -alpha).score;
        if (score > bestResult.score) {
            bestResult = {move, score, true};
        }

        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }

    return bestResult;
}
