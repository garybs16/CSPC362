#include "board.hpp"
#include <cstddef>

namespace {
bool IsBlackPiece(int pieceId) {
    return pieceId >= bP;
}

void RemovePiece(uint64_t& bitboard, int square) {
    bitboard &= ~(1ULL << square);
}

void AddPiece(uint64_t& bitboard, int square) {
    bitboard |= (1ULL << square);
}

int PromotionPieceForFlag(int flags, bool movingBlack) {
    switch (flags) {
        case PromotionKnight:
        case CapturePromotionKnight:
            return movingBlack ? bN : N;
        case PromotionBishop:
        case CapturePromotionBishop:
            return movingBlack ? bB : B;
        case PromotionRook:
        case CapturePromotionRook:
            return movingBlack ? bR : R;
        case PromotionQueen:
        case CapturePromotionQueen:
            return movingBlack ? bQ : Q;
        default:
            return movingBlack ? bP : P;
    }
}
}

Board::Board() {
    setEmpty();
}

Board::Board(bool isBlack_) : sideToMove(isBlack_) {
    setEmpty();
}

Board::Board(const Board& other) {
    *this = other;
}

Board& Board::operator=(const Board& other) {
    if (this == &other) {
        return *this;
    }

    for (size_t i = 0; i < 12; ++i) {
        piece_bitboard[i] = other.piece_bitboard[i];
    }
    for (size_t i = 0; i < 3; ++i) {
        occupancy[i] = other.occupancy[i];
    }
    castling = other.castling;
    sideToMove = other.sideToMove;
    enPassant = other.enPassant;
    halfmoveClock = other.halfmoveClock;
    return *this;
}

void Board::setEmpty() {
    for (size_t i = 0; i < 12; ++i) {
        piece_bitboard[i] = 0ULL;
    }
    for (size_t i = 0; i < 3; ++i) {
        occupancy[i] = 0ULL;
    }
    halfmoveClock = 0;
}

void Board::printBitBoard(uint64_t bitboard) const {
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << " ";
        for (size_t file = 0; file < 8; ++file) {
            const int square = rank * 8 + static_cast<int>(file);
            std::cout << (((bitboard >> square) & 1ULL) ? "1" : "0");
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Board::defaultBoard() {
    setEmpty();
    sideToMove = false;
    castling = WhiteKingSide | WhiteQueenSide | BlackKingSide | BlackQueenSide;
    enPassant = -1;
    halfmoveClock = 0;

    for (size_t i = 8; i < 16; ++i) {
        piece_bitboard[P] |= (1ULL << i);
    }
    for (size_t i = 48; i < 56; ++i) {
        piece_bitboard[bP] |= (1ULL << i);
    }

    piece_bitboard[R] |= (1ULL << 0);
    piece_bitboard[R] |= (1ULL << 7);
    piece_bitboard[bR] |= (1ULL << 56);
    piece_bitboard[bR] |= (1ULL << 63);

    piece_bitboard[N] |= (1ULL << 1);
    piece_bitboard[N] |= (1ULL << 6);
    piece_bitboard[bN] |= (1ULL << 57);
    piece_bitboard[bN] |= (1ULL << 62);

    piece_bitboard[B] |= (1ULL << 2);
    piece_bitboard[B] |= (1ULL << 5);
    piece_bitboard[bB] |= (1ULL << 58);
    piece_bitboard[bB] |= (1ULL << 61);

    piece_bitboard[Q] |= (1ULL << 3);
    piece_bitboard[K] |= (1ULL << 4);
    piece_bitboard[bQ] |= (1ULL << 59);
    piece_bitboard[bK] |= (1ULL << 60);

    updateOccupancy();
}

void Board::updateOccupancy() {
    occupancy[whiteOccupancy] = 0ULL;
    occupancy[blackOccupancy] = 0ULL;

    for (int i = 0; i < 6; ++i) {
        occupancy[whiteOccupancy] |= piece_bitboard[i];
    }
    for (int i = 6; i < 12; ++i) {
        occupancy[blackOccupancy] |= piece_bitboard[i];
    }

    occupancy[dualOccupancy] = occupancy[whiteOccupancy] | occupancy[blackOccupancy];
}

int Board::getPieceAt(int square) const {
    if (square < 0 || square > 63) {
        return -1;
    }

    const uint64_t bit = 1ULL << square;
    for (int pieceId = 0; pieceId < 12; ++pieceId) {
        if (piece_bitboard[pieceId] & bit) {
            return pieceId;
        }
    }
    return -1;
}

bool Board::isSquareOccupied(int square) const {
    return square >= 0 && square < 64 && ((occupancy[dualOccupancy] >> square) & 1ULL);
}

bool Board::isSidePiece(int square, bool blackSide) const {
    const int pieceId = getPieceAt(square);
    if (pieceId == -1) {
        return false;
    }
    return IsBlackPiece(pieceId) == blackSide;
}

int Board::findKing(bool blackSide) const {
    const uint64_t kingBoard = piece_bitboard[blackSide ? bK : K];
    for (int square = 0; square < 64; ++square) {
        if ((kingBoard >> square) & 1ULL) {
            return square;
        }
    }
    return -1;
}

bool Board::makeMove(const Move& m) {
    const int from = m.getFrom();
    const int dest = m.getTo();
    const int flags = m.getFlags();
    const int movingPiece = getPieceAt(from);
    const bool movingBlack = sideToMove;

    if (movingPiece == -1 || IsBlackPiece(movingPiece) != movingBlack) {
        return false;
    }

    const int capturedPiece = getPieceAt(dest);
    if (capturedPiece != -1 && IsBlackPiece(capturedPiece) == movingBlack) {
        return false;
    }
    if (capturedPiece == K || capturedPiece == bK) {
        return false;
    }

    const bool resetsHalfmoveClock = movingPiece == P || movingPiece == bP || capturedPiece != -1 || flags == EnPassant;
    enPassant = -1;

    RemovePiece(piece_bitboard[movingPiece], from);
    if (capturedPiece != -1) {
        RemovePiece(piece_bitboard[capturedPiece], dest);
    }

    if (flags == EnPassant) {
        const int capturedSquare = movingBlack ? dest + 8 : dest - 8;
        RemovePiece(piece_bitboard[movingBlack ? P : bP], capturedSquare);
    }

    AddPiece(piece_bitboard[movingPiece], dest);

    if (movingPiece == K) {
        castling &= ~(WhiteKingSide | WhiteQueenSide);
    } else if (movingPiece == bK) {
        castling &= ~(BlackKingSide | BlackQueenSide);
    } else if (movingPiece == R) {
        if (from == 0) {
            castling &= ~WhiteQueenSide;
        } else if (from == 7) {
            castling &= ~WhiteKingSide;
        }
    } else if (movingPiece == bR) {
        if (from == 56) {
            castling &= ~BlackQueenSide;
        } else if (from == 63) {
            castling &= ~BlackKingSide;
        }
    }

    if (capturedPiece == R) {
        if (dest == 0) {
            castling &= ~WhiteQueenSide;
        } else if (dest == 7) {
            castling &= ~WhiteKingSide;
        }
    } else if (capturedPiece == bR) {
        if (dest == 56) {
            castling &= ~BlackQueenSide;
        } else if (dest == 63) {
            castling &= ~BlackKingSide;
        }
    }

    if (flags == KingSideCastle) {
        if (movingBlack) {
            RemovePiece(piece_bitboard[bR], 63);
            AddPiece(piece_bitboard[bR], 61);
        } else {
            RemovePiece(piece_bitboard[R], 7);
            AddPiece(piece_bitboard[R], 5);
        }
    } else if (flags == QueenSideCastle) {
        if (movingBlack) {
            RemovePiece(piece_bitboard[bR], 56);
            AddPiece(piece_bitboard[bR], 59);
        } else {
            RemovePiece(piece_bitboard[R], 0);
            AddPiece(piece_bitboard[R], 3);
        }
    } else if (flags == DoublePawnPush) {
        enPassant = movingBlack ? dest + 8 : dest - 8;
    }

    if (m.isPromotion()) {
        RemovePiece(piece_bitboard[movingPiece], dest);
        AddPiece(piece_bitboard[PromotionPieceForFlag(flags, movingBlack)], dest);
    }

    halfmoveClock = resetsHalfmoveClock ? 0 : halfmoveClock + 1;
    sideToMove = !movingBlack;
    updateOccupancy();
    return true;
}

std::string Board::toSquare(int space) const {
    const int file = space % 8;
    const int row = (space / 8) + 1;

    std::string square(1, static_cast<char>('a' + file));
    square += std::to_string(row);
    return square;
}
