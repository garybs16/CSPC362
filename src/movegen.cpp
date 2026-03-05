#include "movegen.hpp"
#include <bit>
#include "def.hpp"

using namespace Masks;

namespace {
constexpr int kKnightOffsets[8][2] = {
    {2, 1}, {2, -1}, {1, 2}, {1, -2},
    {-1, 2}, {-1, -2}, {-2, 1}, {-2, -1}
};

constexpr int kKingOffsets[8][2] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1},
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
};

constexpr int kBishopDirections[4][2] = {
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
};

constexpr int kRookDirections[4][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}
};

inline int popLSB(uint64_t& bitboard) {
    const int index = std::countr_zero(bitboard);
    bitboard &= (bitboard - 1);
    return index;
}

bool IsOnBoard(int rank, int file) {
    return rank >= 0 && rank < 8 && file >= 0 && file < 8;
}

int ToSquare(int rank, int file) {
    return rank * 8 + file;
}
}

void MoveGen::includeMagic() {
}

void MoveGen::addPromotionMoves(MoveList& movelist, int from, int to, bool capture) const {
    if (capture) {
        movelist.push(Move(from, to, CapturePromotionQueen));
        movelist.push(Move(from, to, CapturePromotionRook));
        movelist.push(Move(from, to, CapturePromotionBishop));
        movelist.push(Move(from, to, CapturePromotionKnight));
        return;
    }

    movelist.push(Move(from, to, PromotionQueen));
    movelist.push(Move(from, to, PromotionRook));
    movelist.push(Move(from, to, PromotionBishop));
    movelist.push(Move(from, to, PromotionKnight));
}

void MoveGen::genPawn(const Board& board, MoveList& movelist) const {
    uint64_t pawns = board.piece_bitboard[board.sideToMove ? bP : P];
    const bool movingBlack = board.sideToMove;
    const int forward = movingBlack ? -8 : 8;
    const int startRank = movingBlack ? 6 : 1;
    const int promotionRank = movingBlack ? 1 : 6;

    while (pawns) {
        const int from = popLSB(pawns);
        const int rank = from / 8;
        const int file = from % 8;
        const int oneStep = from + forward;

        if (oneStep >= 0 && oneStep < 64 && !board.isSquareOccupied(oneStep)) {
            if (rank == promotionRank) {
                addPromotionMoves(movelist, from, oneStep, false);
            } else {
                movelist.push(Move(from, oneStep, Quiet));
                if (rank == startRank) {
                    const int twoStep = from + (forward * 2);
                    if (!board.isSquareOccupied(twoStep)) {
                        movelist.push(Move(from, twoStep, DoublePawnPush));
                    }
                }
            }
        }

        if (file > 0) {
            const int captureLeft = from + (movingBlack ? -9 : 7);
            if (captureLeft >= 0 && captureLeft < 64) {
                if (board.isSidePiece(captureLeft, !movingBlack)) {
                    if (rank == promotionRank) {
                        addPromotionMoves(movelist, from, captureLeft, true);
                    } else {
                        movelist.push(Move(from, captureLeft, Capture));
                    }
                } else if (captureLeft == board.enPassant) {
                    movelist.push(Move(from, captureLeft, EnPassant));
                }
            }
        }

        if (file < 7) {
            const int captureRight = from + (movingBlack ? -7 : 9);
            if (captureRight >= 0 && captureRight < 64) {
                if (board.isSidePiece(captureRight, !movingBlack)) {
                    if (rank == promotionRank) {
                        addPromotionMoves(movelist, from, captureRight, true);
                    } else {
                        movelist.push(Move(from, captureRight, Capture));
                    }
                } else if (captureRight == board.enPassant) {
                    movelist.push(Move(from, captureRight, EnPassant));
                }
            }
        }
    }
}

void MoveGen::genKnight(const Board& board, MoveList& movelist) const {
    uint64_t knights = board.piece_bitboard[board.sideToMove ? bN : N];
    const bool movingBlack = board.sideToMove;

    while (knights) {
        const int from = popLSB(knights);
        const int rank = from / 8;
        const int file = from % 8;

        for (const auto& offset : kKnightOffsets) {
            const int toRank = rank + offset[0];
            const int toFile = file + offset[1];
            if (!IsOnBoard(toRank, toFile)) {
                continue;
            }

            const int to = ToSquare(toRank, toFile);
            if (board.isSidePiece(to, movingBlack)) {
                continue;
            }

            movelist.push(Move(from, to, board.isSidePiece(to, !movingBlack) ? Capture : Quiet));
        }
    }
}

void MoveGen::genSlide(const Board& board, MoveList& movelist) const {
    const bool movingBlack = board.sideToMove;
    uint64_t bishops = board.piece_bitboard[movingBlack ? bB : B];
    uint64_t rooks = board.piece_bitboard[movingBlack ? bR : R];
    uint64_t queens = board.piece_bitboard[movingBlack ? bQ : Q];

    auto addSlidingMoves = [&](uint64_t pieces, const int directions[][2], int directionCount) {
        while (pieces) {
            const int from = popLSB(pieces);
            const int rank = from / 8;
            const int file = from % 8;

            for (int i = 0; i < directionCount; ++i) {
                int toRank = rank + directions[i][0];
                int toFile = file + directions[i][1];

                while (IsOnBoard(toRank, toFile)) {
                    const int to = ToSquare(toRank, toFile);
                    if (board.isSidePiece(to, movingBlack)) {
                        break;
                    }
                    if (board.isSidePiece(to, !movingBlack)) {
                        movelist.push(Move(from, to, Capture));
                        break;
                    }

                    movelist.push(Move(from, to, Quiet));
                    toRank += directions[i][0];
                    toFile += directions[i][1];
                }
            }
        }
    };

    addSlidingMoves(bishops, kBishopDirections, 4);
    addSlidingMoves(rooks, kRookDirections, 4);

    constexpr int kQueenDirections[8][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    addSlidingMoves(queens, kQueenDirections, 8);
}

void MoveGen::genCastling(const Board& board, MoveList& movelist) const {
    if (board.sideToMove) {
        if ((board.castling & BlackKingSide) &&
            board.getPieceAt(60) == bK &&
            board.getPieceAt(63) == bR &&
            !board.isSquareOccupied(61) &&
            !board.isSquareOccupied(62) &&
            !isSquareAttacked(board, 60, false) &&
            !isSquareAttacked(board, 61, false) &&
            !isSquareAttacked(board, 62, false)) {
            movelist.push(Move(60, 62, KingSideCastle));
        }

        if ((board.castling & BlackQueenSide) &&
            board.getPieceAt(60) == bK &&
            board.getPieceAt(56) == bR &&
            !board.isSquareOccupied(57) &&
            !board.isSquareOccupied(58) &&
            !board.isSquareOccupied(59) &&
            !isSquareAttacked(board, 60, false) &&
            !isSquareAttacked(board, 59, false) &&
            !isSquareAttacked(board, 58, false)) {
            movelist.push(Move(60, 58, QueenSideCastle));
        }
        return;
    }

    if ((board.castling & WhiteKingSide) &&
        board.getPieceAt(4) == K &&
        board.getPieceAt(7) == R &&
        !board.isSquareOccupied(5) &&
        !board.isSquareOccupied(6) &&
        !isSquareAttacked(board, 4, true) &&
        !isSquareAttacked(board, 5, true) &&
        !isSquareAttacked(board, 6, true)) {
        movelist.push(Move(4, 6, KingSideCastle));
    }

    if ((board.castling & WhiteQueenSide) &&
        board.getPieceAt(4) == K &&
        board.getPieceAt(0) == R &&
        !board.isSquareOccupied(1) &&
        !board.isSquareOccupied(2) &&
        !board.isSquareOccupied(3) &&
        !isSquareAttacked(board, 4, true) &&
        !isSquareAttacked(board, 3, true) &&
        !isSquareAttacked(board, 2, true)) {
        movelist.push(Move(4, 2, QueenSideCastle));
    }
}

void MoveGen::genKing(const Board& board, MoveList& movelist) const {
    const bool movingBlack = board.sideToMove;
    uint64_t kingBoard = board.piece_bitboard[movingBlack ? bK : K];
    if (!kingBoard) {
        return;
    }

    const int from = popLSB(kingBoard);
    const int rank = from / 8;
    const int file = from % 8;

    for (const auto& offset : kKingOffsets) {
        const int toRank = rank + offset[0];
        const int toFile = file + offset[1];
        if (!IsOnBoard(toRank, toFile)) {
            continue;
        }

        const int to = ToSquare(toRank, toFile);
        if (board.isSidePiece(to, movingBlack)) {
            continue;
        }

        movelist.push(Move(from, to, board.isSidePiece(to, !movingBlack) ? Capture : Quiet));
    }

    genCastling(board, movelist);
}

bool MoveGen::isSquareAttacked(const Board& board, int square, bool byBlack) const {
    if (square < 0 || square > 63) {
        return false;
    }

    const uint64_t squareBit = 1ULL << square;
    if (byBlack) {
        const uint64_t pawnAttacks =
            ((board.piece_bitboard[bP] & ~File_H) >> 7) |
            ((board.piece_bitboard[bP] & ~File_A) >> 9);
        if (pawnAttacks & squareBit) {
            return true;
        }
    } else {
        const uint64_t pawnAttacks =
            ((board.piece_bitboard[P] & ~File_A) << 7) |
            ((board.piece_bitboard[P] & ~File_H) << 9);
        if (pawnAttacks & squareBit) {
            return true;
        }
    }

    const int rank = square / 8;
    const int file = square % 8;
    const int knightPiece = byBlack ? bN : N;
    const int kingPiece = byBlack ? bK : K;

    for (const auto& offset : kKnightOffsets) {
        const int fromRank = rank - offset[0];
        const int fromFile = file - offset[1];
        if (!IsOnBoard(fromRank, fromFile)) {
            continue;
        }
        if (board.getPieceAt(ToSquare(fromRank, fromFile)) == knightPiece) {
            return true;
        }
    }

    for (const auto& offset : kKingOffsets) {
        const int fromRank = rank + offset[0];
        const int fromFile = file + offset[1];
        if (!IsOnBoard(fromRank, fromFile)) {
            continue;
        }
        if (board.getPieceAt(ToSquare(fromRank, fromFile)) == kingPiece) {
            return true;
        }
    }

    auto rayHitsAttacker = [&](const int directions[][2], int directionCount, int pieceA, int pieceB) {
        for (int i = 0; i < directionCount; ++i) {
            int toRank = rank + directions[i][0];
            int toFile = file + directions[i][1];

            while (IsOnBoard(toRank, toFile)) {
                const int piece = board.getPieceAt(ToSquare(toRank, toFile));
                if (piece == -1) {
                    toRank += directions[i][0];
                    toFile += directions[i][1];
                    continue;
                }

                if (piece == pieceA || piece == pieceB) {
                    return true;
                }
                break;
            }
        }
        return false;
    };

    if (rayHitsAttacker(kBishopDirections, 4, byBlack ? bB : B, byBlack ? bQ : Q)) {
        return true;
    }
    if (rayHitsAttacker(kRookDirections, 4, byBlack ? bR : R, byBlack ? bQ : Q)) {
        return true;
    }

    return false;
}

void MoveGen::generatePseudoLegal(const Board& board, MoveList& ml) const {
    ml.clear();
    genPawn(board, ml);
    genKnight(board, ml);
    genSlide(board, ml);
    genKing(board, ml);
}

bool MoveGen::isInCheck(const Board& board, bool blackSide) const {
    const int kingSquare = board.findKing(blackSide);
    return kingSquare != -1 && isSquareAttacked(board, kingSquare, !blackSide);
}

void MoveGen::generateAll(const Board& board, MoveList& ml) const {
    MoveList pseudoLegal;
    generatePseudoLegal(board, pseudoLegal);

    ml.clear();
    const bool movingBlack = board.sideToMove;
    for (int i = 0; i < pseudoLegal.count; ++i) {
        Board next(board);
        if (!next.makeMove(pseudoLegal.moves[i])) {
            continue;
        }
        if (!isInCheck(next, movingBlack)) {
            ml.push(pseudoLegal.moves[i]);
        }
    }
}
