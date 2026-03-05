#ifndef MOVE_HPP
#define MOVE_HPP
#include <cstdint>

enum MoveFlag : int {
    Quiet = 0,
    DoublePawnPush = 1,
    KingSideCastle = 2,
    QueenSideCastle = 3,
    Capture = 4,
    EnPassant = 5,
    PromotionKnight = 8,
    PromotionBishop = 9,
    PromotionRook = 10,
    PromotionQueen = 11,
    CapturePromotionKnight = 12,
    CapturePromotionBishop = 13,
    CapturePromotionRook = 14,
    CapturePromotionQueen = 15
};

struct Move{
    uint16_t moveData_;

    Move(int from, int to, int flags = Quiet) {
        moveData_ = (from) | (to << 6) | (flags << 12);
    }

    Move() : moveData_(0) {}

    int getFrom() const {
        return moveData_ & 0x3F;
    }

    int getTo() const {
        return (moveData_ >> 6) & 0x3F;
    }

    int getFlags() const {
        return moveData_ >> 12;
    }

    bool isCapture() const {
        const int flags = getFlags();
        return flags == Capture || flags == EnPassant || flags >= CapturePromotionKnight;
    }

    bool isPromotion() const {
        return getFlags() >= PromotionKnight;
    }

    bool isCastle() const {
        const int flags = getFlags();
        return flags == KingSideCastle || flags == QueenSideCastle;
    }
};

#endif
