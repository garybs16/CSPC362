#include "gui.hpp"

#include "ai.hpp"
#include "board.hpp"
#include "move.hpp"
#include "movegen.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr int kBoardPixels = 800;
constexpr int kCellPixels = kBoardPixels / 8;
constexpr int kPadding = 32;
constexpr int kPanelWidth = 336;
constexpr int kStatusHeight = 84;
constexpr int kPanelHeight = kBoardPixels + kStatusHeight + 16;
constexpr int kWindowWidth = kBoardPixels + kPanelWidth + (kPadding * 3);
constexpr int kWindowHeight = kBoardPixels + kStatusHeight + (kPadding * 2);
constexpr int kBoardBorder = 10;
constexpr int kButtonHeight = 38;
constexpr int kTimerButtonHeight = 34;
constexpr UINT kRobotTurnMessage = WM_APP + 1;
constexpr UINT_PTR kClockTimerId = 1;
constexpr UINT kClockTickMs = 100;
constexpr int kReviewSearchDepth = 3;
constexpr int kReviewQuiescenceDepth = 4;
constexpr int kReviewInfinity = 1'000'000;
constexpr int kReviewMateScore = 100'000;
constexpr int kStockfishReviewDepth = 12;

enum class GameMode {
    HumanVsHuman,
    HumanVsRobot
};

enum class TimeControlPreset {
    Bullet1,
    Blitz3,
    Blitz5,
    Rapid10,
    Rapid15
};

struct ReviewMoveInfo {
    int beforeEvalWhite = 0;
    int afterEvalWhite = 0;
    int centipawnLoss = 0;
    int bestScoreForMover = 0;
    int playedScoreForMover = 0;
    Move playedMove;
    Move bestMove;
    std::string label;
    std::string note;
    bool isBestMove = false;
};

struct GuiState {
    Board board;
    Board reviewBoard;
    MoveGen moveGen;
    ChessAi ai;
    MoveList legalMoves;
    int selectedSquare = -1;
    bool gameStarted = false;
    bool gameOver = false;
    bool gameOverMessageShown = false;
    bool hasLastMove = false;
    bool reviewMode = false;
    bool reviewHasLastMove = false;
    Move lastMove;
    Move reviewLastMove;
    GameMode mode = GameMode::HumanVsRobot;
    RobotDifficulty difficulty = RobotDifficulty::Medium;
    TimeControlPreset timeControl = TimeControlPreset::Blitz5;
    bool humanPlaysBlack = false;
    std::int64_t whiteTimeMs = 0;
    std::int64_t blackTimeMs = 0;
    ULONGLONG lastClockTick = 0;
    std::string statusText;
    std::string resultText;
    std::vector<Move> gameMoves;
    std::vector<ReviewMoveInfo> reviewMoves;
    std::vector<std::string> positionHistory;
    int reviewPly = 0;
};

wchar_t PieceGlyph(int pieceId) {
    static const wchar_t kGlyphs[12] = {
        L'\x2659', L'\x2658', L'\x2657', L'\x2656', L'\x2655', L'\x2654',
        L'\x265F', L'\x265E', L'\x265D', L'\x265C', L'\x265B', L'\x265A'
    };
    if (pieceId < 0 || pieceId > 11) {
        return L'?';
    }
    return kGlyphs[pieceId];
}

const char* ModeLabel(GameMode mode) {
    return mode == GameMode::HumanVsRobot ? "Vs Robot" : "Human vs Human";
}

const char* HumanSideLabel(bool humanPlaysBlack) {
    return humanPlaysBlack ? "Black" : "White";
}

std::int64_t InitialTimeMs(TimeControlPreset preset) {
    switch (preset) {
        case TimeControlPreset::Bullet1:
            return 60'000;
        case TimeControlPreset::Blitz3:
            return 180'000;
        case TimeControlPreset::Blitz5:
            return 300'000;
        case TimeControlPreset::Rapid10:
            return 600'000;
        case TimeControlPreset::Rapid15:
            return 900'000;
    }
    return 300'000;
}

std::string FormatClock(std::int64_t timeMs) {
    if (timeMs < 0) {
        timeMs = 0;
    }

    const std::int64_t totalSeconds = timeMs / 1000;
    const std::int64_t minutes = totalSeconds / 60;
    const std::int64_t seconds = totalSeconds % 60;
    const std::int64_t tenths = (timeMs % 1000) / 100;

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%lld:%02lld.%lld",
        static_cast<long long>(minutes),
        static_cast<long long>(seconds),
        static_cast<long long>(tenths));

    return std::string(buffer);
}

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

int StaticEvalWhite(const Board& board) {
    static constexpr std::array<int, 12> kPieceValues = {
        100, 320, 330, 500, 900, 0,
        100, 320, 330, 500, 900, 0
    };

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

    return score;
}

int StaticEvalForSideToMove(const Board& board) {
    const int whiteScore = StaticEvalWhite(board);
    return board.sideToMove ? -whiteScore : whiteScore;
}

int EvalWhiteWithTerminal(const Board& board, const MoveGen& moveGen) {
    MoveList moves;
    moveGen.generateAll(board, moves);
    if (moves.count == 0) {
        if (moveGen.isInCheck(board, board.sideToMove)) {
            return board.sideToMove ? 100000 : -100000;
        }
        return 0;
    }

    return StaticEvalWhite(board);
}

int PieceValue(int pieceId) {
    static constexpr std::array<int, 12> kPieceValues = {
        100, 320, 330, 500, 900, 0,
        100, 320, 330, 500, 900, 0
    };

    if (pieceId < 0 || pieceId >= static_cast<int>(kPieceValues.size())) {
        return 0;
    }
    return kPieceValues[static_cast<std::size_t>(pieceId)];
}

int ReviewMoveOrderingScore(const Board& board, const Move& move) {
    int score = 0;
    const int movingPiece = board.getPieceAt(move.getFrom());
    int capturedPiece = board.getPieceAt(move.getTo());
    if (move.getFlags() == EnPassant) {
        capturedPiece = board.sideToMove ? P : bP;
    }

    if (capturedPiece != -1) {
        score += (PieceValue(capturedPiece) * 10) - PieceValue(movingPiece);
    }
    if (move.isPromotion()) {
        score += 800;
    }
    if (move.isCastle()) {
        score += 50;
    }
    return score;
}

std::vector<Move> OrderedReviewMoves(const Board& board, const MoveGen& moveGen, bool tacticalOnly) {
    MoveList moves;
    moveGen.generateAll(board, moves);

    std::vector<Move> orderedMoves;
    orderedMoves.reserve(moves.count);
    for (int i = 0; i < moves.count; ++i) {
        const Move& move = moves.moves[i];
        if (tacticalOnly && !move.isCapture() && !move.isPromotion()) {
            continue;
        }
        orderedMoves.push_back(move);
    }

    std::sort(orderedMoves.begin(), orderedMoves.end(), [&](const Move& left, const Move& right) {
        return ReviewMoveOrderingScore(board, left) > ReviewMoveOrderingScore(board, right);
    });
    return orderedMoves;
}

int ReviewTerminalScore(const Board& board, const MoveGen& moveGen, int depth) {
    MoveList moves;
    moveGen.generateAll(board, moves);
    if (moves.count > 0) {
        return kReviewInfinity;
    }

    if (moveGen.isInCheck(board, board.sideToMove)) {
        return -kReviewMateScore - depth;
    }
    return 0;
}

int ReviewQuiescence(const Board& board, const MoveGen& moveGen, int depth, int alpha, int beta) {
    const int terminalScore = ReviewTerminalScore(board, moveGen, depth);
    if (terminalScore != kReviewInfinity) {
        return terminalScore;
    }

    const bool inCheck = moveGen.isInCheck(board, board.sideToMove);
    const int standPat = StaticEvalForSideToMove(board);
    if (depth == 0) {
        return standPat;
    }
    if (!inCheck) {
        if (standPat >= beta) {
            return beta;
        }
        alpha = std::max(alpha, standPat);
    }

    std::vector<Move> orderedMoves = OrderedReviewMoves(board, moveGen, !inCheck);
    for (const Move& move : orderedMoves) {
        Board next(board);
        if (!next.makeMove(move)) {
            continue;
        }

        const int score = -ReviewQuiescence(next, moveGen, depth - 1, -beta, -alpha);
        if (score >= beta) {
            return beta;
        }
        alpha = std::max(alpha, score);
    }

    return alpha;
}

int ReviewNegamax(const Board& board, const MoveGen& moveGen, int depth, int alpha, int beta) {
    const int terminalScore = ReviewTerminalScore(board, moveGen, depth);
    if (terminalScore != kReviewInfinity) {
        return terminalScore;
    }

    if (depth == 0) {
        return ReviewQuiescence(board, moveGen, kReviewQuiescenceDepth, alpha, beta);
    }

    std::vector<Move> orderedMoves = OrderedReviewMoves(board, moveGen, false);
    int bestScore = -kReviewInfinity;
    for (const Move& move : orderedMoves) {
        Board next(board);
        if (!next.makeMove(move)) {
            continue;
        }

        const int score = -ReviewNegamax(next, moveGen, depth - 1, -beta, -alpha);
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }

    return bestScore;
}

int ReviewScoreForMoverAfterMove(const Board& boardAfterMove, const MoveGen& moveGen) {
    return -ReviewNegamax(boardAfterMove, moveGen, kReviewSearchDepth, -kReviewInfinity, kReviewInfinity);
}

std::string SquareName(int square) {
    if (square < 0 || square > 63) {
        return "--";
    }

    std::string squareName(1, static_cast<char>('a' + (square % 8)));
    squareName += std::to_string((square / 8) + 1);
    return squareName;
}

char PromotionSuffix(const Move& move) {
    switch (move.getFlags()) {
        case PromotionKnight:
        case CapturePromotionKnight:
            return 'n';
        case PromotionBishop:
        case CapturePromotionBishop:
            return 'b';
        case PromotionRook:
        case CapturePromotionRook:
            return 'r';
        case PromotionQueen:
        case CapturePromotionQueen:
            return 'q';
        default:
            return '\0';
    }
}

std::string MoveText(const Move& move) {
    std::string text = SquareName(move.getFrom()) + "-" + SquareName(move.getTo());
    const char promotion = PromotionSuffix(move);
    if (promotion != '\0') {
        text += "=";
        text += static_cast<char>(promotion - 'a' + 'A');
    }
    return text;
}

int SquareFromUci(const std::string& square) {
    if (square.size() < 2 || square[0] < 'a' || square[0] > 'h' || square[1] < '1' || square[1] > '8') {
        return -1;
    }
    return (square[1] - '1') * 8 + (square[0] - 'a');
}

int PromotionFlagFromUci(char promotion, bool capture) {
    switch (promotion) {
        case 'n':
            return capture ? CapturePromotionKnight : PromotionKnight;
        case 'b':
            return capture ? CapturePromotionBishop : PromotionBishop;
        case 'r':
            return capture ? CapturePromotionRook : PromotionRook;
        case 'q':
            return capture ? CapturePromotionQueen : PromotionQueen;
        default:
            return -1;
    }
}

Move MoveFromUci(const Board& board, const MoveGen& moveGen, const std::string& uciMove) {
    if (uciMove.size() < 4) {
        return Move();
    }

    const int from = SquareFromUci(uciMove.substr(0, 2));
    const int to = SquareFromUci(uciMove.substr(2, 2));
    if (from == -1 || to == -1) {
        return Move();
    }

    MoveList legalMoves;
    moveGen.generateAll(board, legalMoves);
    if (uciMove.size() >= 5) {
        const bool capture = board.isSidePiece(to, !board.sideToMove);
        const int promotionFlag = PromotionFlagFromUci(uciMove[4], capture);
        if (promotionFlag != -1) {
            const Move* promotionMove = legalMoves.find(from, to, promotionFlag);
            if (promotionMove) {
                return *promotionMove;
            }
        }
    }

    const Move* move = legalMoves.find(from, to);
    return move ? *move : Move();
}

char PieceFenChar(int pieceId) {
    static constexpr char kPieceChars[12] = {
        'P', 'N', 'B', 'R', 'Q', 'K',
        'p', 'n', 'b', 'r', 'q', 'k'
    };
    if (pieceId < 0 || pieceId >= 12) {
        return '\0';
    }
    return kPieceChars[pieceId];
}

std::string BoardToFen(const Board& board) {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            const int piece = board.getPieceAt((rank * 8) + file);
            if (piece == -1) {
                ++emptyCount;
                continue;
            }
            if (emptyCount > 0) {
                fen += static_cast<char>('0' + emptyCount);
                emptyCount = 0;
            }
            fen += PieceFenChar(piece);
        }
        if (emptyCount > 0) {
            fen += static_cast<char>('0' + emptyCount);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    fen += board.sideToMove ? " b " : " w ";
    std::string castling;
    if (board.castling & WhiteKingSide) {
        castling += 'K';
    }
    if (board.castling & WhiteQueenSide) {
        castling += 'Q';
    }
    if (board.castling & BlackKingSide) {
        castling += 'k';
    }
    if (board.castling & BlackQueenSide) {
        castling += 'q';
    }
    fen += castling.empty() ? "-" : castling;
    fen += ' ';
    fen += board.enPassant == -1 ? "-" : SquareName(board.enPassant);
    fen += " 0 1";
    return fen;
}

bool HasLegalEnPassantCapture(const Board& board) {
    if (board.enPassant < 0 || board.enPassant > 63) {
        return false;
    }

    const bool movingBlack = board.sideToMove;
    const int pawnPiece = movingBlack ? bP : P;
    const int pawnRank = movingBlack ? 3 : 4;
    const int epRank = board.enPassant / 8;
    const int epFile = board.enPassant % 8;
    if (epRank != (movingBlack ? 2 : 5)) {
        return false;
    }

    if (epFile > 0 && board.getPieceAt((pawnRank * 8) + epFile - 1) == pawnPiece) {
        return true;
    }
    if (epFile < 7 && board.getPieceAt((pawnRank * 8) + epFile + 1) == pawnPiece) {
        return true;
    }
    return false;
}

std::string BoardPositionKey(const Board& board) {
    std::string key;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            const int piece = board.getPieceAt((rank * 8) + file);
            if (piece == -1) {
                ++emptyCount;
                continue;
            }
            if (emptyCount > 0) {
                key += static_cast<char>('0' + emptyCount);
                emptyCount = 0;
            }
            key += PieceFenChar(piece);
        }
        if (emptyCount > 0) {
            key += static_cast<char>('0' + emptyCount);
        }
        if (rank > 0) {
            key += '/';
        }
    }

    key += board.sideToMove ? " b " : " w ";
    std::string castling;
    if (board.castling & WhiteKingSide) {
        castling += 'K';
    }
    if (board.castling & WhiteQueenSide) {
        castling += 'Q';
    }
    if (board.castling & BlackKingSide) {
        castling += 'k';
    }
    if (board.castling & BlackQueenSide) {
        castling += 'q';
    }
    key += castling.empty() ? "-" : castling;
    key += ' ';
    key += HasLegalEnPassantCapture(board) ? SquareName(board.enPassant) : "-";
    return key;
}

int CountBits(uint64_t value) {
    int count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
}

bool IsLightSquare(int square) {
    const int rank = square / 8;
    const int file = square % 8;
    return ((rank + file) % 2) == 0;
}

bool AllBishopsOnSameColor(uint64_t bishops) {
    bool hasColor = false;
    bool lightSquare = false;
    for (int square = 0; square < 64; ++square) {
        if (((bishops >> square) & 1ULL) == 0) {
            continue;
        }

        const bool currentLightSquare = IsLightSquare(square);
        if (!hasColor) {
            hasColor = true;
            lightSquare = currentLightSquare;
        } else if (lightSquare != currentLightSquare) {
            return false;
        }
    }
    return hasColor;
}

bool HasInsufficientMaterial(const Board& board) {
    if (board.piece_bitboard[P] || board.piece_bitboard[bP]
        || board.piece_bitboard[R] || board.piece_bitboard[bR]
        || board.piece_bitboard[Q] || board.piece_bitboard[bQ]) {
        return false;
    }

    const uint64_t bishops = board.piece_bitboard[B] | board.piece_bitboard[bB];
    const uint64_t knights = board.piece_bitboard[N] | board.piece_bitboard[bN];
    const int bishopCount = CountBits(bishops);
    const int knightCount = CountBits(knights);
    const int minorCount = bishopCount + knightCount;

    if (minorCount == 0) {
        return true;
    }
    if (minorCount == 1) {
        return true;
    }
    if (knightCount == 0 && AllBishopsOnSameColor(bishops)) {
        return true;
    }
    return false;
}

void RecordCurrentPosition(GuiState& state) {
    state.positionHistory.push_back(BoardPositionKey(state.board));
}

int CurrentPositionRepetitions(const GuiState& state) {
    if (state.positionHistory.empty()) {
        return 0;
    }

    const std::string& currentPosition = state.positionHistory.back();
    int count = 0;
    for (const std::string& position : state.positionHistory) {
        if (position == currentPosition) {
            ++count;
        }
    }
    return count;
}

bool SameMove(const Move& left, const Move& right) {
    return left.getFrom() == right.getFrom()
        && left.getTo() == right.getTo()
        && left.getFlags() == right.getFlags();
}

std::string FormatEval(int centipawns) {
    if (centipawns >= 90000) {
        return "White mate";
    }
    if (centipawns <= -90000) {
        return "Black mate";
    }

    char buffer[32];
    const double pawns = static_cast<double>(centipawns) / 100.0;
    std::snprintf(buffer, sizeof(buffer), "%+.1f", pawns);
    return std::string(buffer);
}

std::string FormatMoverScore(int centipawns) {
    if (centipawns >= 90000) {
        return "mate";
    }
    if (centipawns <= -90000) {
        return "getting mated";
    }

    char buffer[32];
    const double pawns = static_cast<double>(centipawns) / 100.0;
    std::snprintf(buffer, sizeof(buffer), "%+.1f", pawns);
    return std::string(buffer);
}

std::string ClassificationForLoss(int loss, bool bestMove, int bestScore, int playedScore) {
    if (bestScore >= 90000 && playedScore < 90000) {
        return "Missed Mate";
    }
    if (bestScore >= 300 && playedScore < 80 && loss >= 260 && loss < 520) {
        return "Missed Win";
    }
    if (bestMove || loss <= 12) {
        return "Best";
    }
    if (loss <= 30) {
        return "Excellent";
    }
    if (loss <= 70) {
        return "Good";
    }
    if (loss <= 120) {
        return "Neutral";
    }
    if (loss <= 220) {
        return "Inaccuracy";
    }
    if (loss <= 450) {
        return "Mistake";
    }
    return "Blunder";
}

COLORREF ClassificationColor(const std::string& label) {
    if (label == "Best") {
        return RGB(118, 188, 139);
    }
    if (label == "Excellent") {
        return RGB(118, 174, 208);
    }
    if (label == "Good") {
        return RGB(166, 195, 116);
    }
    if (label == "Neutral") {
        return RGB(156, 170, 180);
    }
    if (label == "Inaccuracy") {
        return RGB(220, 180, 104);
    }
    if (label == "Mistake") {
        return RGB(205, 135, 92);
    }
    if (label == "Missed Win") {
        return RGB(224, 150, 82);
    }
    if (label == "Missed Mate") {
        return RGB(226, 116, 94);
    }
    return RGB(205, 104, 100);
}

int QualityScore(const std::string& label) {
    if (label == "Best") {
        return 100;
    }
    if (label == "Excellent") {
        return 96;
    }
    if (label == "Good") {
        return 90;
    }
    if (label == "Neutral") {
        return 82;
    }
    if (label == "Inaccuracy") {
        return 68;
    }
    if (label == "Missed Win") {
        return 45;
    }
    if (label == "Missed Mate") {
        return 25;
    }
    if (label == "Mistake") {
        return 42;
    }
    return 15;
}

std::string ReviewNoteForLabel(const std::string& label) {
    if (label == "Best") {
        return "Top engine choice.";
    }
    if (label == "Excellent") {
        return "Nearly best.";
    }
    if (label == "Good") {
        return "Keeps control.";
    }
    if (label == "Neutral") {
        return "Small eval drop.";
    }
    if (label == "Inaccuracy") {
        return "Noticeable drop.";
    }
    if (label == "Missed Win") {
        return "Winning chance missed.";
    }
    if (label == "Missed Mate") {
        return "Forced mate missed.";
    }
    if (label == "Mistake") {
        return "Large eval drop.";
    }
    return "Major swing; check tactics.";
}

bool FileExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string QuoteCommandPath(const std::string& path) {
    return "\"" + path + "\"";
}

std::string FindStockfishExecutable() {
    const std::array<std::string, 4> candidates = {
        "engines\\stockfish.exe",
        "stockfish.exe",
        "engines\\stockfish-windows-x86-64-avx2.exe",
        "engines\\stockfish-windows-x86-64.exe"
    };

    for (const std::string& candidate : candidates) {
        if (FileExists(candidate)) {
            return candidate;
        }
    }
    return "stockfish.exe";
}

struct UciAnalysis {
    bool ok = false;
    int scoreForSideToMove = 0;
    std::string bestMoveUci;
};

class UciEngine {
    public:
        UciEngine() = default;
        ~UciEngine() {
            close();
        }

        bool start(const std::string& executablePath) {
            close();

            SECURITY_ATTRIBUTES securityAttributes = {};
            securityAttributes.nLength = sizeof(securityAttributes);
            securityAttributes.bInheritHandle = TRUE;

            HANDLE childStdoutRead = nullptr;
            HANDLE childStdoutWrite = nullptr;
            HANDLE childStdinRead = nullptr;
            HANDLE childStdinWrite = nullptr;

            if (!CreatePipe(&childStdoutRead, &childStdoutWrite, &securityAttributes, 0)) {
                return false;
            }
            if (!SetHandleInformation(childStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
                CloseHandle(childStdoutRead);
                CloseHandle(childStdoutWrite);
                return false;
            }
            if (!CreatePipe(&childStdinRead, &childStdinWrite, &securityAttributes, 0)) {
                CloseHandle(childStdoutRead);
                CloseHandle(childStdoutWrite);
                return false;
            }
            if (!SetHandleInformation(childStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
                CloseHandle(childStdoutRead);
                CloseHandle(childStdoutWrite);
                CloseHandle(childStdinRead);
                CloseHandle(childStdinWrite);
                return false;
            }

            STARTUPINFOA startupInfo = {};
            startupInfo.cb = sizeof(startupInfo);
            startupInfo.dwFlags = STARTF_USESTDHANDLES;
            startupInfo.hStdInput = childStdinRead;
            startupInfo.hStdOutput = childStdoutWrite;
            startupInfo.hStdError = childStdoutWrite;

            PROCESS_INFORMATION processInfo = {};
            std::string commandLine = QuoteCommandPath(executablePath);
            const BOOL created = CreateProcessA(
                nullptr,
                commandLine.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startupInfo,
                &processInfo);

            CloseHandle(childStdinRead);
            CloseHandle(childStdoutWrite);

            if (!created) {
                CloseHandle(childStdoutRead);
                CloseHandle(childStdinWrite);
                return false;
            }

            process_ = processInfo.hProcess;
            thread_ = processInfo.hThread;
            stdoutRead_ = childStdoutRead;
            stdinWrite_ = childStdinWrite;

            if (!sendCommand("uci") || !waitFor("uciok", 5000)) {
                close();
                return false;
            }
            sendCommand("setoption name Threads value 2");
            sendCommand("setoption name Hash value 64");
            if (!sendCommand("isready") || !waitFor("readyok", 5000)) {
                close();
                return false;
            }

            ready_ = true;
            return true;
        }

        UciAnalysis analyzeFen(const std::string& fen) {
            UciAnalysis analysis;
            if (!ready_) {
                return analysis;
            }

            if (!sendCommand("position fen " + fen)) {
                return analysis;
            }
            if (!sendCommand("go depth " + std::to_string(kStockfishReviewDepth))) {
                return analysis;
            }

            std::string line;
            int latestScore = 0;
            bool hasScore = false;
            const ULONGLONG deadline = GetTickCount64() + 15000;
            while (GetTickCount64() < deadline && readLine(line, 1000)) {
                int parsedScore = 0;
                if (parseScore(line, parsedScore)) {
                    latestScore = parsedScore;
                    hasScore = true;
                }

                if (line.rfind("bestmove ", 0) == 0) {
                    std::istringstream stream(line);
                    std::string token;
                    stream >> token >> analysis.bestMoveUci;
                    analysis.scoreForSideToMove = hasScore ? latestScore : 0;
                    analysis.ok = !analysis.bestMoveUci.empty() && analysis.bestMoveUci != "(none)";
                    return analysis;
                }
            }

            sendCommand("stop");
            return analysis;
        }

    private:
        bool sendCommand(const std::string& command) {
            if (!stdinWrite_) {
                return false;
            }

            const std::string line = command + "\n";
            DWORD written = 0;
            return WriteFile(stdinWrite_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr)
                && written == line.size();
        }

        bool readLine(std::string& line, DWORD timeoutMs) {
            line.clear();
            const ULONGLONG deadline = GetTickCount64() + timeoutMs;
            while (GetTickCount64() < deadline) {
                DWORD available = 0;
                if (!PeekNamedPipe(stdoutRead_, nullptr, 0, nullptr, &available, nullptr)) {
                    return false;
                }
                if (available == 0) {
                    Sleep(2);
                    continue;
                }

                char ch = '\0';
                DWORD read = 0;
                if (!ReadFile(stdoutRead_, &ch, 1, &read, nullptr) || read == 0) {
                    return false;
                }
                if (ch == '\r') {
                    continue;
                }
                if (ch == '\n') {
                    return true;
                }
                line += ch;
            }
            return false;
        }

        bool waitFor(const std::string& expectedText, DWORD timeoutMs) {
            const ULONGLONG deadline = GetTickCount64() + timeoutMs;
            std::string line;
            while (GetTickCount64() < deadline) {
                if (readLine(line, 250) && line.find(expectedText) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

        bool parseScore(const std::string& line, int& score) const {
            std::istringstream stream(line);
            std::string token;
            while (stream >> token) {
                if (token != "score") {
                    continue;
                }

                std::string scoreType;
                int value = 0;
                if (!(stream >> scoreType >> value)) {
                    return false;
                }
                if (scoreType == "cp") {
                    score = value;
                    return true;
                }
                if (scoreType == "mate") {
                    const int sign = value >= 0 ? 1 : -1;
                    score = sign * (kReviewMateScore - std::min(std::abs(value), 100));
                    return true;
                }
                return false;
            }
            return false;
        }

        void close() {
            if (stdinWrite_) {
                sendCommand("quit");
            }
            if (process_) {
                WaitForSingleObject(process_, 200);
            }
            if (stdinWrite_) {
                CloseHandle(stdinWrite_);
                stdinWrite_ = nullptr;
            }
            if (stdoutRead_) {
                CloseHandle(stdoutRead_);
                stdoutRead_ = nullptr;
            }
            if (thread_) {
                CloseHandle(thread_);
                thread_ = nullptr;
            }
            if (process_) {
                CloseHandle(process_);
                process_ = nullptr;
            }
            ready_ = false;
        }

        HANDLE process_ = nullptr;
        HANDLE thread_ = nullptr;
        HANDLE stdoutRead_ = nullptr;
        HANDLE stdinWrite_ = nullptr;
        bool ready_ = false;
};

RECT MakeRect(int left, int top, int width, int height) {
    RECT rect = {left, top, left + width, top + height};
    return rect;
}

RECT BoardRect() {
    return MakeRect(kPadding, kPadding, kBoardPixels, kBoardPixels);
}

RECT OuterBoardRect() {
    return MakeRect(
        kPadding - kBoardBorder,
        kPadding - kBoardBorder,
        kBoardPixels + (kBoardBorder * 2),
        kBoardPixels + (kBoardBorder * 2));
}

int PanelLeft() {
    return kPadding + kBoardPixels + kPadding;
}

RECT PanelRect() {
    return MakeRect(PanelLeft(), kPadding, kPanelWidth, kPanelHeight);
}

RECT NewGameRect() {
    const int availableWidth = kPanelWidth - 48;
    const int spacing = 12;
    const int buttonWidth = (availableWidth - spacing) / 2;
    return MakeRect(PanelLeft() + 24, kPadding + 252, buttonWidth, kButtonHeight);
}

RECT StartRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 206, kPanelWidth - 48, kButtonHeight);
}

RECT SurrenderRect() {
    const int availableWidth = kPanelWidth - 48;
    const int spacing = 12;
    const int buttonWidth = (availableWidth - spacing) / 2;
    return MakeRect(PanelLeft() + 24 + buttonWidth + spacing, kPadding + 252, buttonWidth, kButtonHeight);
}

RECT ModeRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 298, kPanelWidth - 48, kButtonHeight);
}

RECT SideRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 344, kPanelWidth - 48, kButtonHeight);
}

RECT ReviewPreviousRect() {
    const int availableWidth = kPanelWidth - 60;
    const int buttonWidth = availableWidth / 2;
    return MakeRect(PanelLeft() + 24, kPadding + 298, buttonWidth, kButtonHeight);
}

RECT ReviewNextRect() {
    const int availableWidth = kPanelWidth - 60;
    const int spacing = 12;
    const int buttonWidth = availableWidth / 2;
    return MakeRect(PanelLeft() + 24 + buttonWidth + spacing, kPadding + 298, buttonWidth, kButtonHeight);
}

RECT ReviewJumpStartRect() {
    const int availableWidth = kPanelWidth - 60;
    const int buttonWidth = availableWidth / 2;
    return MakeRect(PanelLeft() + 24, kPadding + 650, buttonWidth, kButtonHeight);
}

RECT ReviewJumpEndRect() {
    const int availableWidth = kPanelWidth - 60;
    const int spacing = 12;
    const int buttonWidth = availableWidth / 2;
    return MakeRect(PanelLeft() + 24 + buttonWidth + spacing, kPadding + 650, buttonWidth, kButtonHeight);
}

RECT DifficultyRect(int index) {
    const int top = kPadding + 428 + (index * (kButtonHeight + 7));
    return MakeRect(PanelLeft() + 24, top, kPanelWidth - 48, kButtonHeight);
}

RECT TimeControlRect(int index) {
    const int top = kPadding + 634 + (index * (kTimerButtonHeight + 6));
    return MakeRect(PanelLeft() + 24, top, kPanelWidth - 48, kTimerButtonHeight);
}

RECT StatusPanelRect() {
    return MakeRect(kPadding, kPadding + kBoardPixels + 16, kBoardPixels, kStatusHeight);
}

bool IsRobotTurn(const GuiState& state) {
    return !state.reviewMode && state.mode == GameMode::HumanVsRobot && state.board.sideToMove != state.humanPlaysBlack;
}

enum class ChessSound {
    Move,
    Capture,
    Check,
    GameEnd,
    Draw
};

void PlayChessSound(ChessSound sound) {
    struct Tone {
        double frequency;
        double durationSeconds;
        double volume;
    };

    auto appendLe16 = [](std::vector<char>& data, int value) {
        data.push_back(static_cast<char>(value & 0xFF));
        data.push_back(static_cast<char>((value >> 8) & 0xFF));
    };

    auto appendLe32 = [](std::vector<char>& data, int value) {
        data.push_back(static_cast<char>(value & 0xFF));
        data.push_back(static_cast<char>((value >> 8) & 0xFF));
        data.push_back(static_cast<char>((value >> 16) & 0xFF));
        data.push_back(static_cast<char>((value >> 24) & 0xFF));
    };

    auto buildWave = [&](const std::vector<Tone>& tones) {
        constexpr int sampleRate = 44100;
        constexpr int bitsPerSample = 16;
        constexpr int channels = 1;
        constexpr double pi = 3.14159265358979323846;

        int sampleCount = 0;
        for (const Tone& tone : tones) {
            sampleCount += static_cast<int>(tone.durationSeconds * sampleRate);
        }

        const int dataBytes = sampleCount * channels * (bitsPerSample / 8);
        std::vector<char> wave;
        wave.reserve(44 + dataBytes);

        wave.insert(wave.end(), {'R', 'I', 'F', 'F'});
        appendLe32(wave, 36 + dataBytes);
        wave.insert(wave.end(), {'W', 'A', 'V', 'E'});
        wave.insert(wave.end(), {'f', 'm', 't', ' '});
        appendLe32(wave, 16);
        appendLe16(wave, 1);
        appendLe16(wave, channels);
        appendLe32(wave, sampleRate);
        appendLe32(wave, sampleRate * channels * (bitsPerSample / 8));
        appendLe16(wave, channels * (bitsPerSample / 8));
        appendLe16(wave, bitsPerSample);
        wave.insert(wave.end(), {'d', 'a', 't', 'a'});
        appendLe32(wave, dataBytes);

        for (const Tone& tone : tones) {
            const int toneSamples = static_cast<int>(tone.durationSeconds * sampleRate);
            for (int i = 0; i < toneSamples; ++i) {
                const double t = static_cast<double>(i) / sampleRate;
                const double attack = std::min(1.0, static_cast<double>(i) / (sampleRate * 0.006));
                const double release = std::min(1.0, static_cast<double>(toneSamples - i) / (sampleRate * 0.035));
                const double envelope = attack * release;
                const double body = std::sin(2.0 * pi * tone.frequency * t);
                const double click = std::sin(2.0 * pi * (tone.frequency * 1.5) * t) * 0.18;
                const int sample = static_cast<int>((body + click) * envelope * tone.volume * 32767.0);
                appendLe16(wave, std::max(-32768, std::min(32767, sample)));
            }
        }

        return wave;
    };

    static const std::vector<char> moveSound = buildWave({{620.0, 0.075, 0.28}, {465.0, 0.045, 0.18}});
    static const std::vector<char> captureSound = buildWave({{360.0, 0.055, 0.34}, {250.0, 0.075, 0.26}});
    static const std::vector<char> checkSound = buildWave({{720.0, 0.055, 0.24}, {940.0, 0.075, 0.28}});
    static const std::vector<char> gameEndSound = buildWave({{560.0, 0.070, 0.26}, {420.0, 0.090, 0.28}, {315.0, 0.130, 0.30}});
    static const std::vector<char> drawSound = buildWave({{410.0, 0.080, 0.22}, {410.0, 0.080, 0.18}});

    const std::vector<char>* wave = &moveSound;
    switch (sound) {
        case ChessSound::Move:
            wave = &moveSound;
            break;
        case ChessSound::Capture:
            wave = &captureSound;
            break;
        case ChessSound::Check:
            wave = &checkSound;
            break;
        case ChessSound::GameEnd:
            wave = &gameEndSound;
            break;
        case ChessSound::Draw:
            wave = &drawSound;
            break;
    }

    PlaySoundA(wave->data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

bool IsDrawResultText(const std::string& statusText) {
    return statusText == "Stalemate" || statusText.rfind("Draw", 0) == 0;
}

void PlaySoundAfterMove(const GuiState& state, const Move& move) {
    if (state.gameOver) {
        PlayChessSound(IsDrawResultText(state.statusText) ? ChessSound::Draw : ChessSound::GameEnd);
        return;
    }

    if (state.moveGen.isInCheck(state.board, state.board.sideToMove)) {
        PlayChessSound(ChessSound::Check);
        return;
    }

    PlayChessSound(move.isCapture() ? ChessSound::Capture : ChessSound::Move);
}

bool IsCurrentPlayerPiece(const GuiState& state, int square) {
    return state.board.isSidePiece(square, state.board.sideToMove);
}

bool IsLastMoveSquare(const GuiState& state, int square) {
    if (state.reviewMode) {
        return state.reviewHasLastMove && (state.reviewLastMove.getFrom() == square || state.reviewLastMove.getTo() == square);
    }
    return state.hasLastMove && (state.lastMove.getFrom() == square || state.lastMove.getTo() == square);
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FillVerticalGradient(HDC hdc, const RECT& rect, COLORREF topColor, COLORREF bottomColor) {
    const int height = rect.bottom - rect.top;
    if (height <= 0) {
        return;
    }

    for (int i = 0; i < height; ++i) {
        const int red = ((GetRValue(topColor) * (height - i)) + (GetRValue(bottomColor) * i)) / height;
        const int green = ((GetGValue(topColor) * (height - i)) + (GetGValue(bottomColor) * i)) / height;
        const int blue = ((GetBValue(topColor) * (height - i)) + (GetBValue(bottomColor) * i)) / height;

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(red, green, blue));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        MoveToEx(hdc, rect.left, rect.top + i, nullptr);
        LineTo(hdc, rect.right, rect.top + i);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

void DrawRectOutline(HDC hdc, const RECT& rect, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawRoundPanel(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 16, 16);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawTextCenterA(HDC hdc, const RECT& rect, const char* text, COLORREF color) {
    RECT textRect = rect;
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawTextLeftA(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    RECT textRect = rect;
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &textRect, format);
}

void ResetClocks(GuiState& state) {
    const std::int64_t initialTime = InitialTimeMs(state.timeControl);
    state.whiteTimeMs = initialTime;
    state.blackTimeMs = initialTime;
    state.lastClockTick = GetTickCount64();
}

void BeginNextTurn(GuiState& state) {
    state.lastClockTick = GetTickCount64();
}

void StartMatch(GuiState& state) {
    if (state.gameOver) {
        return;
    }
    state.gameStarted = true;
    state.selectedSquare = -1;
    BeginNextTurn(state);
}

void MarkFlagLoss(GuiState& state, bool blackFlagged) {
    state.gameOver = true;
    state.gameOverMessageShown = false;
    state.statusText = std::string(blackFlagged ? "Black" : "White") + " ran out of time";
    state.resultText = state.statusText;
    PlayChessSound(ChessSound::GameEnd);
}

bool DeductActiveClock(GuiState& state, std::int64_t elapsedMs) {
    if (!state.gameStarted || state.gameOver || elapsedMs <= 0) {
        return false;
    }

    std::int64_t& activeClock = state.board.sideToMove ? state.blackTimeMs : state.whiteTimeMs;
    activeClock -= elapsedMs;
    if (activeClock > 0) {
        return false;
    }

    activeClock = 0;
    MarkFlagLoss(state, state.board.sideToMove);
    return true;
}

void SyncClock(GuiState& state) {
    if (!state.gameStarted || state.gameOver) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (state.lastClockTick == 0) {
        state.lastClockTick = now;
        return;
    }

    DeductActiveClock(state, static_cast<std::int64_t>(now - state.lastClockTick));
    state.lastClockTick = now;
}

int ClampReviewPly(const GuiState& state, int ply) {
    const int maxPly = static_cast<int>(state.gameMoves.size());
    return std::max(0, std::min(ply, maxPly));
}

void UpdateReviewBoard(GuiState& state) {
    state.reviewPly = ClampReviewPly(state, state.reviewPly);
    state.reviewBoard.defaultBoard();

    for (int i = 0; i < state.reviewPly; ++i) {
        state.reviewBoard.makeMove(state.gameMoves[static_cast<std::size_t>(i)]);
    }

    state.reviewHasLastMove = state.reviewPly > 0;
    if (state.reviewHasLastMove && state.reviewPly <= static_cast<int>(state.reviewMoves.size())) {
        state.reviewLastMove = state.gameMoves[static_cast<std::size_t>(state.reviewPly - 1)];
        const ReviewMoveInfo& info = state.reviewMoves[static_cast<std::size_t>(state.reviewPly - 1)];
        char buffer[96];
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Review %d/%d: %s - %s",
            state.reviewPly,
            static_cast<int>(state.gameMoves.size()),
            MoveText(info.playedMove).c_str(),
            info.label.c_str());
        state.statusText = buffer;
    } else {
        state.statusText = "Review: start position";
    }

    state.selectedSquare = -1;
}

ReviewMoveInfo AnalyzeReviewMove(const Board& before, const Move& playedMove, const MoveGen& moveGen, UciEngine* engine) {
    ReviewMoveInfo info;
    info.beforeEvalWhite = EvalWhiteWithTerminal(before, moveGen);
    info.playedMove = playedMove;
    info.bestMove = playedMove;

    Board after(before);
    if (!after.makeMove(playedMove)) {
        info.afterEvalWhite = info.beforeEvalWhite;
        info.centipawnLoss = 0;
        info.label = "Review";
        info.note = "Move could not be replayed.";
        return info;
    }

    if (engine) {
        const UciAnalysis beforeAnalysis = engine->analyzeFen(BoardToFen(before));
        const UciAnalysis afterAnalysis = engine->analyzeFen(BoardToFen(after));
        if (beforeAnalysis.ok && afterAnalysis.ok) {
            const Move engineBestMove = MoveFromUci(before, moveGen, beforeAnalysis.bestMoveUci);
            if (engineBestMove.getFrom() != 0 || engineBestMove.getTo() != 0 || engineBestMove.getFlags() != Quiet) {
                info.bestMove = engineBestMove;
            }

            info.bestScoreForMover = beforeAnalysis.scoreForSideToMove;
            info.playedScoreForMover = -afterAnalysis.scoreForSideToMove;
            info.afterEvalWhite = after.sideToMove ? -afterAnalysis.scoreForSideToMove : afterAnalysis.scoreForSideToMove;
            info.isBestMove = SameMove(playedMove, info.bestMove);
            info.centipawnLoss = std::max(0, info.bestScoreForMover - info.playedScoreForMover);
            info.label = ClassificationForLoss(
                info.centipawnLoss,
                info.isBestMove,
                info.bestScoreForMover,
                info.playedScoreForMover);
            info.note = "Stockfish depth " + std::to_string(kStockfishReviewDepth);
            return info;
        }
    }

    MoveList moves;
    moveGen.generateAll(before, moves);
    int bestScore = std::numeric_limits<int>::min();
    bool foundBest = false;

    for (int i = 0; i < moves.count; ++i) {
        Board candidate(before);
        if (!candidate.makeMove(moves.moves[i])) {
            continue;
        }

        const int score = ReviewScoreForMoverAfterMove(candidate, moveGen);
        if (!foundBest || score > bestScore) {
            foundBest = true;
            bestScore = score;
            info.bestMove = moves.moves[i];
        }
    }

    info.afterEvalWhite = EvalWhiteWithTerminal(after, moveGen);
    const int actualScore = ReviewScoreForMoverAfterMove(after, moveGen);
    info.bestScoreForMover = foundBest ? bestScore : actualScore;
    info.playedScoreForMover = actualScore;
    info.isBestMove = foundBest && SameMove(playedMove, info.bestMove);
    info.centipawnLoss = foundBest ? std::max(0, bestScore - actualScore) : 0;
    info.label = ClassificationForLoss(
        info.centipawnLoss,
        info.isBestMove,
        info.bestScoreForMover,
        info.playedScoreForMover);
    info.note = ReviewNoteForLabel(info.label);
    return info;
}

void BuildReview(GuiState& state) {
    state.reviewMoves.clear();
    state.reviewMoves.reserve(state.gameMoves.size());

    UciEngine engine;
    UciEngine* reviewEngine = nullptr;
    if (engine.start(FindStockfishExecutable())) {
        reviewEngine = &engine;
    }

    Board replayBoard;
    replayBoard.defaultBoard();
    for (const Move& move : state.gameMoves) {
        ReviewMoveInfo info = AnalyzeReviewMove(replayBoard, move, state.moveGen, reviewEngine);
        state.reviewMoves.push_back(info);
        replayBoard.makeMove(move);
    }
}

void EnterReviewMode(GuiState& state, HWND hwnd = nullptr) {
    if (state.gameMoves.empty()) {
        return;
    }

    state.reviewMode = true;
    state.reviewPly = static_cast<int>(state.gameMoves.size());
    state.reviewBoard = state.board;
    state.reviewHasLastMove = state.hasLastMove;
    state.reviewLastMove = state.lastMove;
    state.selectedSquare = -1;

    if (state.reviewMoves.size() != state.gameMoves.size()) {
        state.statusText = "Analyzing game review...";
        if (hwnd) {
            InvalidateRect(hwnd, nullptr, FALSE);
            UpdateWindow(hwnd);
        }

        HCURSOR oldCursor = nullptr;
        if (hwnd) {
            oldCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        }
        BuildReview(state);
        if (oldCursor) {
            SetCursor(oldCursor);
        }
    }

    UpdateReviewBoard(state);
}

void ExitReviewMode(GuiState& state) {
    state.reviewMode = false;
    state.reviewHasLastMove = false;
    state.selectedSquare = -1;
    state.statusText = state.resultText.empty() ? "Game over" : state.resultText;
}

void StepReview(GuiState& state, int delta) {
    if (!state.reviewMode) {
        return;
    }

    state.reviewPly = ClampReviewPly(state, state.reviewPly + delta);
    UpdateReviewBoard(state);
}

int PixelToSquare(int x, int y) {
    const RECT boardRect = BoardRect();
    const int boardX = x - boardRect.left;
    const int boardY = y - boardRect.top;
    if (boardX < 0 || boardY < 0 || boardX >= kBoardPixels || boardY >= kBoardPixels) {
        return -1;
    }

    const int file = boardX / kCellPixels;
    const int rowFromTop = boardY / kCellPixels;
    const int rank = 7 - rowFromTop;
    return (rank * 8) + file;
}

bool GetMoveForSquare(const GuiState& state, int square, bool* isCapture) {
    if (state.reviewMode) {
        return false;
    }
    if (state.selectedSquare == -1) {
        return false;
    }

    for (int i = 0; i < state.legalMoves.count; ++i) {
        const Move& move = state.legalMoves.moves[i];
        if (move.getFrom() != state.selectedSquare || move.getTo() != square) {
            continue;
        }

        if (isCapture) {
            *isCapture = move.isCapture();
        }
        return true;
    }

    return false;
}

void RefreshGameState(GuiState& state) {
    state.legalMoves.clear();
    state.moveGen.generateAll(state.board, state.legalMoves);

    const bool blackToMove = state.board.sideToMove;
    const bool inCheck = state.moveGen.isInCheck(state.board, blackToMove);
    if (state.legalMoves.count == 0) {
        state.gameOver = true;
        state.gameOverMessageShown = false;
        if (inCheck) {
            state.statusText = std::string(blackToMove ? "Black" : "White") + " is checkmated";
        } else {
            state.statusText = "Stalemate";
        }
        state.resultText = state.statusText;
        return;
    }

    if (state.board.halfmoveClock >= 100) {
        state.gameOver = true;
        state.gameOverMessageShown = false;
        state.statusText = "Draw by fifty-move rule";
        state.resultText = state.statusText;
        return;
    }

    if (CurrentPositionRepetitions(state) >= 3) {
        state.gameOver = true;
        state.gameOverMessageShown = false;
        state.statusText = "Draw by threefold repetition";
        state.resultText = state.statusText;
        return;
    }

    if (HasInsufficientMaterial(state.board)) {
        state.gameOver = true;
        state.gameOverMessageShown = false;
        state.statusText = "Draw by insufficient material";
        state.resultText = state.statusText;
        return;
    }

    state.gameOver = false;
    state.gameOverMessageShown = false;
    if (!state.gameStarted) {
        state.statusText = "Ready - press Start to begin";
    } else if (IsRobotTurn(state)) {
        state.statusText = "Robot to move";
    } else {
        state.statusText = std::string(blackToMove ? "Black" : "White") + " to move";
    }
    if (inCheck) {
        state.statusText += " - check";
    }
}

void StartNewGame(GuiState& state) {
    state.board.defaultBoard();
    state.selectedSquare = -1;
    state.gameStarted = false;
    state.reviewMode = false;
    state.reviewPly = 0;
    state.reviewHasLastMove = false;
    state.hasLastMove = false;
    state.gameOverMessageShown = false;
    state.resultText.clear();
    state.gameMoves.clear();
    state.reviewMoves.clear();
    state.positionHistory.clear();
    RecordCurrentPosition(state);
    ResetClocks(state);
    RefreshGameState(state);
}

void QueueRobotTurnIfNeeded(const GuiState& state, HWND hwnd) {
    if (state.gameStarted && !state.gameOver && IsRobotTurn(state)) {
        PostMessage(hwnd, kRobotTurnMessage, 0, 0);
    }
}

int PromptPromotionFlag(HWND hwnd, const POINT& screenPoint, bool capture) {
    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "Queen");
    AppendMenuA(menu, MF_STRING, 2, "Rook");
    AppendMenuA(menu, MF_STRING, 3, "Bishop");
    AppendMenuA(menu, MF_STRING, 4, "Knight");

    SetForegroundWindow(hwnd);
    const int command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        screenPoint.x,
        screenPoint.y,
        0,
        hwnd,
        nullptr);
    DestroyMenu(menu);

    switch (command) {
        case 1:
            return capture ? CapturePromotionQueen : PromotionQueen;
        case 2:
            return capture ? CapturePromotionRook : PromotionRook;
        case 3:
            return capture ? CapturePromotionBishop : PromotionBishop;
        case 4:
            return capture ? CapturePromotionKnight : PromotionKnight;
        default:
            return -1;
    }
}

bool TryApplySelectedMove(GuiState& state, HWND hwnd, int x, int y, int destination) {
    const Move* move = state.legalMoves.find(state.selectedSquare, destination);
    if (!move) {
        return false;
    }

    Move chosenMove = *move;
    if (move->isPromotion()) {
        POINT screenPoint = {x, y};
        ClientToScreen(hwnd, &screenPoint);
        const int chosenFlag = PromptPromotionFlag(hwnd, screenPoint, move->isCapture());
        if (chosenFlag == -1) {
            return false;
        }

        const Move* promotionMove = state.legalMoves.find(state.selectedSquare, destination, chosenFlag);
        if (!promotionMove) {
            return false;
        }
        chosenMove = *promotionMove;
    }

    if (!state.board.makeMove(chosenMove)) {
        return false;
    }

    state.gameMoves.push_back(chosenMove);
    state.reviewMoves.clear();
    RecordCurrentPosition(state);
    state.lastMove = chosenMove;
    state.hasLastMove = true;
    state.selectedSquare = -1;
    RefreshGameState(state);
    PlaySoundAfterMove(state, chosenMove);
    BeginNextTurn(state);
    return true;
}

void MaybeShowGameOverDialog(GuiState& state, HWND hwnd) {
    if (!state.gameOver || state.gameOverMessageShown) {
        return;
    }

    state.gameOverMessageShown = true;
    std::string message = state.statusText + "\n\nPress Review Game or tap R to review. Press New Game or tap N to play again.";
    MessageBoxA(hwnd, message.c_str(), "Game Over", MB_OK | MB_ICONINFORMATION);
}

void SurrenderCurrentSide(GuiState& state) {
    if (!state.gameStarted || state.gameOver) {
        return;
    }

    state.gameOver = true;
    state.gameOverMessageShown = false;
    state.statusText = std::string(state.board.sideToMove ? "Black" : "White") + " surrendered";
    state.resultText = state.statusText;
    PlayChessSound(ChessSound::GameEnd);
}

void RunRobotTurn(GuiState& state, HWND hwnd) {
    while (!state.gameOver && IsRobotTurn(state)) {
        state.selectedSquare = -1;
        state.statusText = "Robot thinking...";
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);

        const ULONGLONG searchStart = GetTickCount64();
        Move robotMove;
        if (!state.ai.findMove(state.board, state.moveGen, state.difficulty, robotMove)) {
            RefreshGameState(state);
            BeginNextTurn(state);
            return;
        }
        const ULONGLONG searchEnd = GetTickCount64();

        if (DeductActiveClock(state, static_cast<std::int64_t>(searchEnd - searchStart))) {
            state.lastClockTick = searchEnd;
            MaybeShowGameOverDialog(state, hwnd);
            return;
        }

        state.lastClockTick = searchEnd;
        if (!state.board.makeMove(robotMove)) {
            RefreshGameState(state);
            BeginNextTurn(state);
            return;
        }

        state.gameMoves.push_back(robotMove);
        state.reviewMoves.clear();
        RecordCurrentPosition(state);
        state.lastMove = robotMove;
        state.hasLastMove = true;
        RefreshGameState(state);
        PlaySoundAfterMove(state, robotMove);
        BeginNextTurn(state);
    }

    MaybeShowGameOverDialog(state, hwnd);
}

void DrawButton(HDC hdc, const RECT& rect, const char* text, bool active) {
    const COLORREF fillColor = active ? RGB(86, 126, 123) : RGB(54, 60, 69);
    const COLORREF borderColor = active ? RGB(142, 183, 176) : RGB(85, 94, 106);
    const COLORREF shadowColor = RGB(23, 27, 32);
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 0, 2);
    DrawRoundPanel(hdc, shadowRect, shadowColor, shadowColor);
    DrawRoundPanel(hdc, rect, fillColor, borderColor);
    DrawTextCenterA(hdc, rect, text, RGB(236, 239, 240));
}

void DrawCoordinates(HDC hdc, HFONT coordFont) {
    const RECT boardRect = BoardRect();
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, coordFont));
    SetBkMode(hdc, TRANSPARENT);

    for (int file = 0; file < 8; ++file) {
        RECT bottomRect = {
            boardRect.left + (file * kCellPixels) + 6,
            boardRect.bottom - 28,
            boardRect.left + ((file + 1) * kCellPixels) - 6,
            boardRect.bottom - 4
        };
        char label[2] = {static_cast<char>('a' + file), '\0'};
        DrawTextLeftA(hdc, bottomRect, label, RGB(232, 234, 226));
    }

    for (int rank = 0; rank < 8; ++rank) {
        RECT leftRect = {
            boardRect.left + 8,
            boardRect.top + (rank * kCellPixels) + 6,
            boardRect.left + 30,
            boardRect.top + ((rank + 1) * kCellPixels)
        };
        char label[2] = {static_cast<char>('8' - rank), '\0'};
        DrawTextLeftA(hdc, leftRect, label, RGB(232, 234, 226));
    }

    SelectObject(hdc, oldFont);
}

void DrawClockCard(HDC hdc, const RECT& rect, const char* sideLabel, const std::string& timeText, bool active, HFONT labelFont, HFONT valueFont) {
    const COLORREF fillColor = active ? RGB(58, 78, 87) : RGB(30, 35, 42);
    const COLORREF borderColor = active ? RGB(118, 158, 164) : RGB(72, 82, 94);
    DrawRoundPanel(hdc, rect, fillColor, borderColor);

    RECT sideRect = {rect.left + 14, rect.top + 8, rect.right - 14, rect.top + 24};
    RECT timeRect = {rect.left + 14, rect.top + 21, rect.right - 14, rect.bottom - 3};
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    DrawTextLeftA(hdc, sideRect, sideLabel, RGB(188, 199, 207));
    SelectObject(hdc, valueFont);
    DrawTextCenterA(hdc, timeRect, timeText.c_str(), RGB(236, 239, 238));
    SelectObject(hdc, oldFont);
}

int ReviewAccuracy(const GuiState& state) {
    if (state.reviewMoves.empty()) {
        return 100;
    }

    int totalScore = 0;
    for (const ReviewMoveInfo& info : state.reviewMoves) {
        totalScore += QualityScore(info.label);
    }

    return std::max(0, std::min(100, totalScore / static_cast<int>(state.reviewMoves.size())));
}

int CountReviewLabel(const GuiState& state, const char* label) {
    int count = 0;
    for (const ReviewMoveInfo& info : state.reviewMoves) {
        if (info.label == label) {
            ++count;
        }
    }
    return count;
}

int ReviewAccuracyForSide(const GuiState& state, bool blackSide) {
    int totalScore = 0;
    int moveCount = 0;
    for (std::size_t i = 0; i < state.reviewMoves.size(); ++i) {
        const bool moveByBlack = (i % 2) == 1;
        if (moveByBlack != blackSide) {
            continue;
        }

        totalScore += QualityScore(state.reviewMoves[i].label);
        ++moveCount;
    }

    if (moveCount == 0) {
        return 100;
    }
    return std::max(0, std::min(100, totalScore / moveCount));
}

void DrawReviewAccuracyTile(HDC hdc, const RECT& rect, const char* sideLabel, int accuracy, bool currentSide, HFONT labelFont, HFONT valueFont) {
    const COLORREF fillColor = currentSide ? RGB(50, 67, 75) : RGB(31, 37, 44);
    const COLORREF borderColor = currentSide ? RGB(116, 157, 164) : RGB(68, 80, 91);
    DrawRoundPanel(hdc, rect, fillColor, borderColor);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    RECT sideRect = {rect.left + 12, rect.top + 8, rect.right - 12, rect.top + 28};
    DrawTextLeftA(hdc, sideRect, sideLabel, RGB(185, 198, 206));

    SelectObject(hdc, valueFont);
    char accuracyText[16];
    std::snprintf(accuracyText, sizeof(accuracyText), "%d%%", accuracy);
    RECT valueRect = {rect.left + 12, rect.top + 27, rect.right - 12, rect.bottom - 6};
    DrawTextCenterA(hdc, valueRect, accuracyText, RGB(239, 242, 240));

    SelectObject(hdc, oldFont);
}

void DrawReviewProgressBar(HDC hdc, const RECT& rect, int currentPly, int totalPly) {
    RECT track = {rect.left, rect.top, rect.right, rect.bottom};
    DrawRoundPanel(hdc, track, RGB(24, 29, 35), RGB(55, 67, 77));

    if (totalPly <= 0 || currentPly <= 0) {
        return;
    }

    RECT fill = {track.left + 3, track.top + 3, track.right - 3, track.bottom - 3};
    const int fillWidth = ((fill.right - fill.left) * std::min(currentPly, totalPly)) / totalPly;
    fill.right = fill.left + std::max(2, fillWidth);
    FillRectColor(hdc, fill, RGB(96, 151, 139));
}

void DrawReviewTopSummary(HDC hdc, const GuiState& state, HFONT labelFont, HFONT valueFont) {
    const RECT panel = PanelRect();
    const int currentPly = state.reviewPly;
    const int totalPly = static_cast<int>(state.gameMoves.size());
    const int availableWidth = kPanelWidth - 48;
    const int spacing = 12;
    const int cardWidth = (availableWidth - spacing) / 2;
    const bool whiteCurrent = currentPly > 0 && (currentPly % 2) == 1;
    const bool blackCurrent = currentPly > 0 && (currentPly % 2) == 0;

    RECT whiteCard = MakeRect(panel.left + 24, panel.top + 86, cardWidth, 62);
    RECT blackCard = MakeRect(panel.left + 24 + cardWidth + spacing, panel.top + 86, cardWidth, 62);
    DrawReviewAccuracyTile(hdc, whiteCard, "White", ReviewAccuracyForSide(state, false), whiteCurrent, labelFont, valueFont);
    DrawReviewAccuracyTile(hdc, blackCard, "Black", ReviewAccuracyForSide(state, true), blackCurrent, labelFont, valueFont);

    RECT progressCard = {panel.left + 24, panel.top + 156, panel.right - 24, panel.top + 194};
    DrawRoundPanel(hdc, progressCard, RGB(31, 37, 44), RGB(68, 80, 91));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    char progressText[64];
    std::snprintf(progressText, sizeof(progressText), "%d/%d  Avg %d%%", currentPly, totalPly, ReviewAccuracy(state));
    RECT progressLabel = {progressCard.left + 12, progressCard.top + 8, progressCard.left + 166, progressCard.bottom - 8};
    DrawTextLeftA(hdc, progressLabel, progressText, RGB(188, 201, 209));

    RECT progressBar = {progressCard.left + 174, progressCard.top + 13, progressCard.right - 12, progressCard.bottom - 13};
    DrawReviewProgressBar(hdc, progressBar, currentPly, totalPly);
    SelectObject(hdc, oldFont);
}

void DrawReviewCountPill(HDC hdc, const RECT& rect, const char* label, int count, COLORREF accentColor, HFONT font) {
    DrawRoundPanel(hdc, rect, RGB(26, 31, 38), accentColor);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    RECT labelRect = {rect.left + 10, rect.top + 2, rect.right - 42, rect.bottom - 2};
    DrawTextLeftA(hdc, labelRect, label, RGB(203, 212, 218));

    char countText[12];
    std::snprintf(countText, sizeof(countText), "%d", count);
    RECT countRect = {rect.right - 38, rect.top + 2, rect.right - 8, rect.bottom - 2};
    DrawTextCenterA(hdc, countRect, countText, RGB(239, 242, 240));

    SelectObject(hdc, oldFont);
}

void DrawReviewControls(HDC hdc, const GuiState& state, HFONT labelFont, HFONT buttonFont) {
    const RECT panel = PanelRect();
    const int currentPly = state.reviewPly;
    const int totalPly = static_cast<int>(state.gameMoves.size());

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, ReviewPreviousRect(), "Previous", currentPly > 0);
    DrawButton(hdc, ReviewNextRect(), "Next", currentPly < totalPly);

    SelectObject(hdc, labelFont);
    RECT reviewTitle = {panel.left + 24, panel.top + 354, panel.right - 24, panel.top + 380};
    DrawTextLeftA(hdc, reviewTitle, "Move Quality", RGB(218, 225, 228));

    RECT scoreCard = {panel.left + 24, panel.top + 386, panel.right - 24, panel.top + 506};
    DrawRoundPanel(hdc, scoreCard, RGB(31, 37, 44), RGB(68, 80, 91));

    const int missedCount = CountReviewLabel(state, "Missed Win") + CountReviewLabel(state, "Missed Mate");
    const int pillWidth = ((scoreCard.right - scoreCard.left) - 42) / 2;
    const int pillHeight = 22;
    const int pillGap = 6;
    const int leftX = scoreCard.left + 12;
    const int rightX = leftX + pillWidth + 18;
    const int topY = scoreCard.top + 12;
    const char* labels[8] = {"Best", "Excel", "Good", "Neutral", "Inacc", "Mistake", "Blunder", "Missed"};
    const int counts[8] = {
        CountReviewLabel(state, "Best"),
        CountReviewLabel(state, "Excellent"),
        CountReviewLabel(state, "Good"),
        CountReviewLabel(state, "Neutral"),
        CountReviewLabel(state, "Inaccuracy"),
        CountReviewLabel(state, "Mistake"),
        CountReviewLabel(state, "Blunder"),
        missedCount
    };
    const COLORREF colors[8] = {
        ClassificationColor("Best"),
        ClassificationColor("Excellent"),
        ClassificationColor("Good"),
        ClassificationColor("Neutral"),
        ClassificationColor("Inaccuracy"),
        ClassificationColor("Mistake"),
        ClassificationColor("Blunder"),
        RGB(224, 150, 82)
    };

    for (int i = 0; i < 8; ++i) {
        const int columnX = (i % 2 == 0) ? leftX : rightX;
        const int rowY = topY + ((i / 2) * (pillHeight + pillGap));
        RECT pillRect = MakeRect(columnX, rowY, pillWidth, pillHeight);
        DrawReviewCountPill(hdc, pillRect, labels[i], counts[i], colors[i], labelFont);
    }

    RECT moveCard = {panel.left + 24, panel.top + 512, panel.right - 24, panel.top + 644};
    DrawRoundPanel(hdc, moveCard, RGB(31, 37, 44), RGB(68, 80, 91));

    SelectObject(hdc, labelFont);
    if (state.reviewMoves.size() != state.gameMoves.size()) {
        RECT moveInfo = {moveCard.left + 16, moveCard.top + 16, moveCard.right - 16, moveCard.bottom - 16};
        DrawTextLeftA(hdc, moveInfo, "Analyzing game review...\nThis can take a moment after longer games.", RGB(190, 201, 208), DT_LEFT | DT_TOP | DT_WORDBREAK);
    } else if (currentPly == 0 || state.reviewMoves.empty()) {
        RECT moveInfo = {moveCard.left + 16, moveCard.top + 16, moveCard.right - 16, moveCard.bottom - 16};
        DrawTextLeftA(hdc, moveInfo, "Start position\nUse Next to step through the analysis.", RGB(190, 201, 208), DT_LEFT | DT_TOP | DT_WORDBREAK);
    } else {
        const ReviewMoveInfo& info = state.reviewMoves[static_cast<std::size_t>(currentPly - 1)];
        RECT labelPill = {moveCard.left + 14, moveCard.top + 10, moveCard.left + 152, moveCard.top + 36};
        DrawRoundPanel(hdc, labelPill, RGB(25, 30, 36), ClassificationColor(info.label));
        DrawTextCenterA(hdc, labelPill, info.label.c_str(), ClassificationColor(info.label));

        char moveNumberText[32];
        std::snprintf(moveNumberText, sizeof(moveNumberText), "%d / %d", currentPly, totalPly);
        RECT moveNumberRect = {moveCard.right - 76, moveCard.top + 10, moveCard.right - 14, moveCard.top + 36};
        DrawTextCenterA(hdc, moveNumberRect, moveNumberText, RGB(180, 193, 202));

        RECT playedBox = {moveCard.left + 14, moveCard.top + 42, moveCard.right - 14, moveCard.top + 70};
        RECT bestBox = {moveCard.left + 14, moveCard.top + 76, moveCard.right - 14, moveCard.top + 104};
        DrawRoundPanel(hdc, playedBox, RGB(26, 31, 38), RGB(62, 74, 86));
        DrawRoundPanel(hdc, bestBox, RGB(26, 31, 38), ClassificationColor("Best"));

        char playedText[96];
        std::snprintf(playedText, sizeof(playedText), "Played  %s  (%s)", MoveText(info.playedMove).c_str(), FormatMoverScore(info.playedScoreForMover).c_str());
        RECT playedTextRect = {playedBox.left + 10, playedBox.top + 3, playedBox.right - 10, playedBox.bottom - 3};
        DrawTextLeftA(hdc, playedTextRect, playedText, RGB(204, 213, 219));

        char bestText[96];
        std::snprintf(bestText, sizeof(bestText), "Best    %s  (%s)", MoveText(info.bestMove).c_str(), FormatMoverScore(info.bestScoreForMover).c_str());
        RECT bestTextRect = {bestBox.left + 10, bestBox.top + 3, bestBox.right - 10, bestBox.bottom - 3};
        DrawTextLeftA(hdc, bestTextRect, bestText, RGB(226, 236, 232));

        char swingText[96];
        std::snprintf(swingText, sizeof(swingText), "Loss %d cp     Eval %s", info.centipawnLoss, FormatEval(info.afterEvalWhite).c_str());
        RECT swingRect = {moveCard.left + 16, moveCard.top + 106, moveCard.right - 16, moveCard.bottom - 4};
        DrawTextLeftA(hdc, swingRect, swingText, RGB(170, 182, 190));
    }

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, ReviewJumpStartRect(), "Start", currentPly > 0);
    DrawButton(hdc, ReviewJumpEndRect(), "End", currentPly < totalPly);

    SelectObject(hdc, labelFont);
    RECT footer = {panel.left + 24, panel.top + 704, panel.right - 24, panel.bottom - 18};
    std::string footerText = "Left/right arrows step moves\nHome/End jump  R exits review";
    DrawTextLeftA(hdc, footer, footerText.c_str(), RGB(160, 174, 184), DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void DrawControls(HDC hdc, const GuiState& state, HFONT titleFont, HFONT labelFont, HFONT buttonFont, HFONT clockFont) {
    RECT panel = PanelRect();
    DrawRoundPanel(hdc, panel, RGB(32, 38, 46), RGB(62, 74, 86));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    SetBkMode(hdc, TRANSPARENT);

    RECT header = {panel.left + 24, panel.top + 20, panel.right - 24, panel.top + 50};
    DrawTextLeftA(hdc, header, "CPSC362 Chess", RGB(232, 237, 237));

    SelectObject(hdc, labelFont);
    RECT subHeader = {panel.left + 24, panel.top + 52, panel.right - 24, panel.top + 76};
    DrawTextLeftA(hdc, subHeader, state.reviewMode ? "Post-game analysis board" : "Robot mode with chess clocks", RGB(156, 170, 180));

    if (state.reviewMode) {
        DrawReviewTopSummary(hdc, state, labelFont, clockFont);
    } else {
        RECT whitePanelClock = {panel.left + 24, panel.top + 86, panel.right - 24, panel.top + 136};
        RECT blackPanelClock = {panel.left + 24, panel.top + 144, panel.right - 24, panel.top + 194};
        DrawClockCard(hdc, whitePanelClock, "White", FormatClock(state.whiteTimeMs), !state.gameOver && !state.board.sideToMove, labelFont, clockFont);
        DrawClockCard(hdc, blackPanelClock, "Black", FormatClock(state.blackTimeMs), !state.gameOver && state.board.sideToMove, labelFont, clockFont);
    }

    SelectObject(hdc, buttonFont);
    const bool canReview = state.gameOver && !state.gameMoves.empty();
    const char* startText = "Start";
    if (state.reviewMode) {
        startText = "Exit Review";
    } else if (canReview) {
        startText = "Review Game";
    } else if (state.gameStarted) {
        startText = "Started";
    }
    DrawButton(hdc, StartRect(), startText, state.reviewMode || (state.gameStarted && !state.gameOver) || canReview);
    DrawButton(hdc, NewGameRect(), "New Game", false);
    DrawButton(hdc, SurrenderRect(), state.reviewMode ? "Exit" : "Surrender", false);

    if (state.reviewMode) {
        DrawReviewControls(hdc, state, labelFont, buttonFont);
        SelectObject(hdc, oldFont);
        return;
    }

    std::string modeText = std::string("Mode: ") + ModeLabel(state.mode);
    DrawButton(hdc, ModeRect(), modeText.c_str(), state.mode == GameMode::HumanVsRobot);

    std::string sideText = std::string("Human: ") + HumanSideLabel(state.humanPlaysBlack);
    DrawButton(hdc, SideRect(), sideText.c_str(), state.humanPlaysBlack);

    SelectObject(hdc, labelFont);
    RECT difficultyHeader = {panel.left + 24, panel.top + 398, panel.right - 24, panel.top + 424};
    DrawTextLeftA(hdc, difficultyHeader, "Robot Difficulty", RGB(218, 225, 228));

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, DifficultyRect(0), "Easy 400-700", state.difficulty == RobotDifficulty::Easy);
    DrawButton(hdc, DifficultyRect(1), "Medium 800-1200", state.difficulty == RobotDifficulty::Medium);
    DrawButton(hdc, DifficultyRect(2), "Hard 1300-1800", state.difficulty == RobotDifficulty::Hard);
    DrawButton(hdc, DifficultyRect(3), "Master 2000+", state.difficulty == RobotDifficulty::Grandmaster);

    SelectObject(hdc, labelFont);
    RECT timerHeader = {panel.left + 24, panel.top + 610, panel.right - 24, panel.top + 632};
    DrawTextLeftA(hdc, timerHeader, "Time Control", RGB(218, 225, 228));

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, TimeControlRect(0), "1 minute", state.timeControl == TimeControlPreset::Bullet1);
    DrawButton(hdc, TimeControlRect(1), "3 minutes", state.timeControl == TimeControlPreset::Blitz3);
    DrawButton(hdc, TimeControlRect(2), "5 minutes", state.timeControl == TimeControlPreset::Blitz5);
    DrawButton(hdc, TimeControlRect(3), "10 minutes", state.timeControl == TimeControlPreset::Rapid10);
    DrawButton(hdc, TimeControlRect(4), "15 minutes", state.timeControl == TimeControlPreset::Rapid15);

    SelectObject(hdc, labelFont);
    RECT footer = {panel.left + 24, panel.top + 846, panel.right - 24, panel.bottom - 18};
    std::string footerText = "Enter start  X surrender  N new game\nR review  M mode  C color  1-4 bot  Esc clear";
    DrawTextLeftA(hdc, footer, footerText.c_str(), RGB(160, 174, 184), DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
}

void DrawStatusPanel(HDC hdc, const GuiState& state, HFONT titleFont, HFONT bodyFont) {
    RECT panel = StatusPanelRect();
    DrawRoundPanel(hdc, panel, RGB(32, 38, 46), RGB(62, 74, 86));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    SetBkMode(hdc, TRANSPARENT);

    RECT statusTitle = {panel.left + 24, panel.top + 14, panel.right - 24, panel.top + 40};
    DrawTextLeftA(hdc, statusTitle, state.statusText.c_str(), RGB(232, 237, 237));

    SelectObject(hdc, bodyFont);
    RECT info = {panel.left + 24, panel.top + 44, panel.right - 24, panel.bottom - 16};
    std::string infoText;
    if (state.reviewMode) {
        infoText = "Use Previous and Next or the arrow keys to review every move. The side panel shows eval, best move, and move quality.";
    } else if (state.gameOver) {
        infoText = state.gameMoves.empty()
            ? "Game over. Start a new game to reset the board."
            : "Game over. Press Review Game to replay the moves with analysis.";
    } else if (!state.gameStarted) {
        infoText = "Press Start to begin the match and activate the clocks.";
    } else if (IsRobotTurn(state)) {
        infoText = "Robot is moving. The active clock is highlighted on the right.";
    } else {
        infoText = "Select a piece, then click one of its highlighted legal destinations.";
    }
    DrawTextLeftA(hdc, info, infoText.c_str(), RGB(160, 174, 184), DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
}

void DrawEvalBar(HDC hdc, const GuiState& state) {
    const RECT boardRect = BoardRect();
    RECT bar = {boardRect.right + 12, boardRect.top, boardRect.right + 26, boardRect.bottom};
    DrawRoundPanel(hdc, bar, RGB(29, 34, 40), RGB(66, 78, 88));

    int eval = EvalWhiteWithTerminal(state.reviewBoard, state.moveGen);
    if (state.reviewPly > 0 && state.reviewPly <= static_cast<int>(state.reviewMoves.size())) {
        eval = state.reviewMoves[static_cast<std::size_t>(state.reviewPly - 1)].afterEvalWhite;
    }
    eval = std::max(-1000, std::min(1000, eval));

    const double whiteShare = std::max(0.05, std::min(0.95, 0.5 + (static_cast<double>(eval) / 2000.0)));
    const int whiteHeight = static_cast<int>((bar.bottom - bar.top - 6) * whiteShare);
    RECT whiteRect = {bar.left + 3, bar.bottom - 3 - whiteHeight, bar.right - 3, bar.bottom - 3};
    FillRectColor(hdc, whiteRect, RGB(225, 229, 226));
}

void DrawBoard(HDC hdc, const GuiState& state) {
    const Board& displayBoard = state.reviewMode ? state.reviewBoard : state.board;
    const COLORREF light = RGB(221, 216, 203);
    const COLORREF dark = RGB(116, 136, 128);
    const COLORREF lastMoveLight = RGB(199, 196, 142);
    const COLORREF lastMoveDark = RGB(126, 151, 119);
    const COLORREF selectedColor = RGB(190, 178, 116);
    const COLORREF moveColor = RGB(87, 130, 118);
    const COLORREF captureColor = RGB(176, 100, 95);
    const RECT fullRect = {0, 0, kWindowWidth, kWindowHeight};
    const RECT outerBoard = OuterBoardRect();
    const RECT boardRect = BoardRect();

    FillVerticalGradient(hdc, fullRect, RGB(24, 29, 35), RGB(37, 46, 54));
    DrawRoundPanel(hdc, outerBoard, RGB(36, 42, 46), RGB(79, 92, 98));

    HFONT pieceFont = CreateFontW(
        68, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Symbol");
    HFONT titleFont = CreateFontA(
        24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Bahnschrift");
    HFONT labelFont = CreateFontA(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
    HFONT buttonFont = CreateFontA(
        18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Bahnschrift");
    HFONT coordFont = CreateFontA(
        16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
    HFONT clockFont = CreateFontA(
        22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Bahnschrift");

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, pieceFont));

    for (int row = 0; row < 8; ++row) {
        for (int file = 0; file < 8; ++file) {
            const int x = boardRect.left + (file * kCellPixels);
            const int y = boardRect.top + (row * kCellPixels);
            const int rank = 7 - row;
            const int square = (rank * 8) + file;

            bool isCaptureTarget = false;
            const bool isLegalTarget = GetMoveForSquare(state, square, &isCaptureTarget);
            const bool isSelected = square == state.selectedSquare;
            const bool isLastMove = IsLastMoveSquare(state, square);

            COLORREF color = ((row + file) % 2 == 0) ? light : dark;
            if (isLastMove) {
                color = ((row + file) % 2 == 0) ? lastMoveLight : lastMoveDark;
            }
            if (isSelected) {
                color = selectedColor;
            }

            RECT cell = {x, y, x + kCellPixels, y + kCellPixels};
            FillRectColor(hdc, cell, color);

            HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(93, 101, 96));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gridPen));
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
            Rectangle(hdc, cell.left, cell.top, cell.right, cell.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(gridPen);

            if (isSelected) {
                DrawRectOutline(hdc, cell, RGB(229, 220, 160), 3);
            } else if (isLegalTarget) {
                if (isCaptureTarget) {
                    HPEN capturePen = CreatePen(PS_SOLID, 7, captureColor);
                    HPEN oldCapturePen = static_cast<HPEN>(SelectObject(hdc, capturePen));
                    HBRUSH oldCaptureBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
                    const int inset = 16;
                    Ellipse(hdc, cell.left + inset, cell.top + inset, cell.right - inset, cell.bottom - inset);
                    SelectObject(hdc, oldCaptureBrush);
                    SelectObject(hdc, oldCapturePen);
                    DeleteObject(capturePen);
                } else {
                    HBRUSH moveBrush = CreateSolidBrush(moveColor);
                    HBRUSH oldMoveBrush = static_cast<HBRUSH>(SelectObject(hdc, moveBrush));
                    HPEN oldMovePen = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
                    const int radius = 12;
                    const int centerX = (cell.left + cell.right) / 2;
                    const int centerY = (cell.top + cell.bottom) / 2;
                    Ellipse(hdc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
                    SelectObject(hdc, oldMovePen);
                    SelectObject(hdc, oldMoveBrush);
                    DeleteObject(moveBrush);
                }
            }

            const int piece = displayBoard.getPieceAt(square);
            if (piece != -1) {
                const wchar_t glyph = PieceGlyph(piece);
                wchar_t text[2] = {glyph, L'\0'};
                RECT shadowRect = cell;
                OffsetRect(&shadowRect, 4, 4);
                SetTextColor(hdc, RGB(41, 42, 39));
                DrawTextW(hdc, text, 1, &shadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT pieceRect = cell;
                SetTextColor(hdc, piece <= 5 ? RGB(244, 243, 238) : RGB(36, 39, 38));
                DrawTextW(hdc, text, 1, &pieceRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    DrawRectOutline(hdc, boardRect, RGB(85, 99, 105), 3);
    DrawCoordinates(hdc, coordFont);
    if (state.reviewMode) {
        DrawEvalBar(hdc, state);
    }
    DrawStatusPanel(hdc, state, titleFont, labelFont);
    DrawControls(hdc, state, titleFont, labelFont, buttonFont, clockFont);

    SelectObject(hdc, oldFont);
    DeleteObject(pieceFont);
    DeleteObject(titleFont);
    DeleteObject(labelFont);
    DeleteObject(buttonFont);
    DeleteObject(coordFont);
    DeleteObject(clockFont);
}

bool HandleKeyDown(GuiState& state, HWND hwnd, WPARAM wParam) {
    if (state.reviewMode) {
        switch (wParam) {
            case VK_LEFT:
                StepReview(state, -1);
                break;
            case VK_RIGHT:
                StepReview(state, 1);
                break;
            case VK_HOME:
                state.reviewPly = 0;
                UpdateReviewBoard(state);
                break;
            case VK_END:
                state.reviewPly = static_cast<int>(state.gameMoves.size());
                UpdateReviewBoard(state);
                break;
            case 'R':
            case VK_ESCAPE:
                ExitReviewMode(state);
                break;
            case 'N':
                StartNewGame(state);
                break;
            default:
                return false;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    switch (wParam) {
        case VK_RETURN:
            if (state.gameOver && !state.gameMoves.empty()) {
                EnterReviewMode(state, hwnd);
            } else {
                StartMatch(state);
            }
            break;
        case 'N':
            StartNewGame(state);
            break;
        case 'M':
            state.mode = state.mode == GameMode::HumanVsRobot ? GameMode::HumanVsHuman : GameMode::HumanVsRobot;
            StartNewGame(state);
            break;
        case 'C':
            state.humanPlaysBlack = !state.humanPlaysBlack;
            StartNewGame(state);
            break;
        case '1':
            state.difficulty = RobotDifficulty::Easy;
            RefreshGameState(state);
            BeginNextTurn(state);
            break;
        case '2':
            state.difficulty = RobotDifficulty::Medium;
            RefreshGameState(state);
            BeginNextTurn(state);
            break;
        case '3':
            state.difficulty = RobotDifficulty::Hard;
            RefreshGameState(state);
            BeginNextTurn(state);
            break;
        case '4':
            state.difficulty = RobotDifficulty::Grandmaster;
            RefreshGameState(state);
            BeginNextTurn(state);
            break;
        case VK_ESCAPE:
            state.selectedSquare = -1;
            break;
        case 'R':
            if (state.gameOver && !state.gameMoves.empty()) {
                EnterReviewMode(state, hwnd);
            } else {
                return false;
            }
            break;
        case 'X':
            SurrenderCurrentSide(state);
            break;
        default:
            return false;
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    QueueRobotTurnIfNeeded(state, hwnd);
    return true;
}

bool HandleControlClick(GuiState& state, HWND hwnd, const POINT& point) {
    const RECT startRect = StartRect();
    const RECT surrenderRect = SurrenderRect();
    const RECT newGameRect = NewGameRect();
    const RECT modeRect = ModeRect();
    const RECT sideRect = SideRect();
    const RECT beginnerRect = DifficultyRect(0);
    const RECT easyRect = DifficultyRect(1);
    const RECT mediumRect = DifficultyRect(2);
    const RECT hardRect = DifficultyRect(3);
    const RECT timeRect0 = TimeControlRect(0);
    const RECT timeRect1 = TimeControlRect(1);
    const RECT timeRect2 = TimeControlRect(2);
    const RECT timeRect3 = TimeControlRect(3);
    const RECT timeRect4 = TimeControlRect(4);
    const RECT reviewPreviousRect = ReviewPreviousRect();
    const RECT reviewNextRect = ReviewNextRect();
    const RECT reviewJumpStartRect = ReviewJumpStartRect();
    const RECT reviewJumpEndRect = ReviewJumpEndRect();

    if (state.reviewMode) {
        if (PtInRect(&startRect, point) || PtInRect(&surrenderRect, point)) {
            ExitReviewMode(state);
        } else if (PtInRect(&newGameRect, point)) {
            StartNewGame(state);
        } else if (PtInRect(&reviewPreviousRect, point)) {
            StepReview(state, -1);
        } else if (PtInRect(&reviewNextRect, point)) {
            StepReview(state, 1);
        } else if (PtInRect(&reviewJumpStartRect, point)) {
            state.reviewPly = 0;
            UpdateReviewBoard(state);
        } else if (PtInRect(&reviewJumpEndRect, point)) {
            state.reviewPly = static_cast<int>(state.gameMoves.size());
            UpdateReviewBoard(state);
        } else {
            return false;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    if (PtInRect(&startRect, point)) {
        if (state.gameOver && !state.gameMoves.empty()) {
            EnterReviewMode(state, hwnd);
        } else {
            StartMatch(state);
        }
    } else if (PtInRect(&surrenderRect, point)) {
        SurrenderCurrentSide(state);
    } else if (PtInRect(&newGameRect, point)) {
        StartNewGame(state);
    } else if (PtInRect(&modeRect, point)) {
        state.mode = state.mode == GameMode::HumanVsRobot ? GameMode::HumanVsHuman : GameMode::HumanVsRobot;
        StartNewGame(state);
    } else if (PtInRect(&sideRect, point)) {
        state.humanPlaysBlack = !state.humanPlaysBlack;
        StartNewGame(state);
    } else if (PtInRect(&beginnerRect, point)) {
        state.difficulty = RobotDifficulty::Easy;
        RefreshGameState(state);
        BeginNextTurn(state);
    } else if (PtInRect(&easyRect, point)) {
        state.difficulty = RobotDifficulty::Medium;
        RefreshGameState(state);
        BeginNextTurn(state);
    } else if (PtInRect(&mediumRect, point)) {
        state.difficulty = RobotDifficulty::Hard;
        RefreshGameState(state);
        BeginNextTurn(state);
    } else if (PtInRect(&hardRect, point)) {
        state.difficulty = RobotDifficulty::Grandmaster;
        RefreshGameState(state);
        BeginNextTurn(state);
    } else if (PtInRect(&timeRect0, point)) {
        state.timeControl = TimeControlPreset::Bullet1;
        StartNewGame(state);
    } else if (PtInRect(&timeRect1, point)) {
        state.timeControl = TimeControlPreset::Blitz3;
        StartNewGame(state);
    } else if (PtInRect(&timeRect2, point)) {
        state.timeControl = TimeControlPreset::Blitz5;
        StartNewGame(state);
    } else if (PtInRect(&timeRect3, point)) {
        state.timeControl = TimeControlPreset::Rapid10;
        StartNewGame(state);
    } else if (PtInRect(&timeRect4, point)) {
        state.timeControl = TimeControlPreset::Rapid15;
        StartNewGame(state);
    } else {
        return false;
    }

    state.selectedSquare = -1;
    InvalidateRect(hwnd, nullptr, FALSE);
    MaybeShowGameOverDialog(state, hwnd);
    QueueRobotTurnIfNeeded(state, hwnd);
    return true;
}

void PaintBuffered(HWND hwnd, HDC windowDc, const GuiState& state) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, clientRect.right, clientRect.bottom);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

    DrawBoard(memoryDc, state);
    BitBlt(windowDc, 0, 0, clientRect.right, clientRect.bottom, memoryDc, 0, 0, SRCCOPY);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiState* state = reinterpret_cast<GuiState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* createdState = new GuiState();
            StartNewGame(*createdState);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createdState));
            SetTimer(hwnd, kClockTimerId, kClockTickMs, nullptr);
            QueueRobotTurnIfNeeded(*createdState, hwnd);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER: {
            if (!state || wParam != kClockTimerId) {
                return 0;
            }
            SyncClock(*state);
            MaybeShowGameOverDialog(*state, hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!state) {
                return 0;
            }

            SyncClock(*state);
            const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (HandleControlClick(*state, hwnd, point)) {
                return 0;
            }

            if (state->reviewMode) {
                return 0;
            }

            MaybeShowGameOverDialog(*state, hwnd);
            if (state->gameOver) {
                return 0;
            }

            if (!state->gameStarted || IsRobotTurn(*state)) {
                return 0;
            }

            const int square = PixelToSquare(point.x, point.y);
            if (square < 0) {
                state->selectedSquare = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (state->selectedSquare == square) {
                state->selectedSquare = -1;
            } else if (IsCurrentPlayerPiece(*state, square)) {
                state->selectedSquare = square;
            } else if (state->selectedSquare != -1 && TryApplySelectedMove(*state, hwnd, point.x, point.y, square)) {
                MaybeShowGameOverDialog(*state, hwnd);
                QueueRobotTurnIfNeeded(*state, hwnd);
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            if (!state) {
                return 0;
            }

            SyncClock(*state);
            if (HandleKeyDown(*state, hwnd, wParam)) {
                MaybeShowGameOverDialog(*state, hwnd);
                return 0;
            }
            MaybeShowGameOverDialog(*state, hwnd);
            return 0;
        }
        case kRobotTurnMessage: {
            if (!state) {
                return 0;
            }
            RunRobotTurn(*state, hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            if (!state) {
                return 0;
            }

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintBuffered(hwnd, hdc, *state);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, kClockTimerId);
            delete state;
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
}  // namespace

int RunChessGui() {
    SetProcessDPIAware();

    HINSTANCE instance = GetModuleHandle(nullptr);
    const char* className = "ChessWindowClass";

    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    RECT windowRect = {0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        className,
        "CPSC362 Chess",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
