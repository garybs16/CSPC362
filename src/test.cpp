#include <cstdint>
#include <iostream>
#include "ai.hpp"
#include "board.hpp"
#include "movegen.hpp"

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

void PlacePiece(Board& board, int pieceId, int square) {
    board.piece_bitboard[pieceId] |= (1ULL << square);
}

std::uint64_t Perft(const Board& board, const MoveGen& moveGen, int depth) {
    if (depth == 0) {
        return 1;
    }

    MoveList moves;
    moveGen.generateAll(board, moves);
    std::uint64_t nodes = 0;
    for (int i = 0; i < moves.count; ++i) {
        Board next(board);
        if (next.makeMove(moves.moves[i])) {
            nodes += Perft(next, moveGen, depth - 1);
        }
    }
    return nodes;
}
}

int main() {
    bool ok = true;
    MoveGen moveGen;

    {
        Board board;
        board.defaultBoard();
        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(moves.count == 20, "initial position should have 20 legal moves");
        ok &= Expect(Perft(board, moveGen, 2) == 400, "initial position perft depth 2 should be 400");
    }

    {
        Board board;
        board.defaultBoard();
        ok &= Expect(board.makeMove(Move(12, 28, DoublePawnPush)), "e2e4 should apply");
        ok &= Expect(board.getPieceAt(28) == P, "white pawn should land on e4");
        ok &= Expect(board.getPieceAt(12) == -1, "e2 should be empty after e2e4");
        ok &= Expect(board.enPassant == 20, "e2e4 should set en passant square to e3");
        ok &= Expect(board.halfmoveClock == 0, "pawn moves should reset the halfmove clock");
        ok &= Expect(board.sideToMove, "side to move should switch to black after white move");

        ok &= Expect(board.makeMove(Move(62, 45)), "g8f6 should apply");
        ok &= Expect(board.halfmoveClock == 1, "quiet non-pawn moves should increment the halfmove clock");
        ok &= Expect(board.makeMove(Move(6, 21)), "g1f3 should apply");
        ok &= Expect(board.halfmoveClock == 2, "consecutive quiet non-pawn moves should keep incrementing the halfmove clock");

        MoveList blackMoves;
        moveGen.generateAll(board, blackMoves);
        ok &= Expect(blackMoves.count > 0, "black should still have legal replies after quiet development");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, R, 12);
        PlacePiece(board, bR, 60);
        PlacePiece(board, bK, 56);
        board.updateOccupancy();

        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(!moves.contains(12, 13), "pinned rook should not move sideways");
        ok &= Expect(moves.contains(12, 20), "pinned rook should still be able to move along the pin");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = WhiteKingSide | WhiteQueenSide;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, R, 0);
        PlacePiece(board, R, 7);
        PlacePiece(board, bK, 60);
        board.updateOccupancy();

        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(moves.contains(4, 6, KingSideCastle), "white kingside castling should be legal on an empty back rank");
        ok &= Expect(moves.contains(4, 2, QueenSideCastle), "white queenside castling should be legal on an empty back rank");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = WhiteKingSide;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, R, 7);
        PlacePiece(board, bK, 60);
        PlacePiece(board, bR, 61);
        board.updateOccupancy();

        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(!moves.contains(4, 6, KingSideCastle), "castling through an attacked square should be illegal");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = 43;
        PlacePiece(board, K, 4);
        PlacePiece(board, bK, 62);
        PlacePiece(board, P, 36);
        PlacePiece(board, bP, 35);
        board.updateOccupancy();

        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(moves.contains(36, 43, EnPassant), "en passant capture should be generated");
        ok &= Expect(board.makeMove(Move(36, 43, EnPassant)), "en passant capture should apply");
        ok &= Expect(board.getPieceAt(43) == P, "white pawn should land on d6 after en passant");
        ok &= Expect(board.getPieceAt(35) == -1, "captured black pawn should be removed by en passant");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, bK, 60);
        PlacePiece(board, P, 48);
        board.updateOccupancy();

        MoveList moves;
        moveGen.generateAll(board, moves);
        ok &= Expect(moves.contains(48, 56, PromotionQueen), "promotion move should be generated");
        ok &= Expect(board.makeMove(Move(48, 56, PromotionQueen)), "promotion move should apply");
        ok &= Expect(board.getPieceAt(56) == Q, "white pawn should promote to a queen");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, bK, 12);
        board.updateOccupancy();

        ok &= Expect(!board.makeMove(Move(4, 12, Capture)), "king captures should be rejected");
    }

    {
        Board board;
        board.setEmpty();
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = -1;
        PlacePiece(board, K, 4);
        PlacePiece(board, Q, 3);
        PlacePiece(board, bQ, 59);
        PlacePiece(board, bK, 62);
        board.updateOccupancy();

        ChessAi ai;
        Move chosenMove;
        ok &= Expect(ai.findMove(board, moveGen, RobotDifficulty::Hard, chosenMove), "AI should find a move in a legal position");
        ok &= Expect(chosenMove.getFrom() == 3 && chosenMove.getTo() == 59, "AI should capture the hanging queen");
    }

    if (!ok) {
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
