#include <iostream>
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
    }

    {
        Board board;
        board.defaultBoard();
        ok &= Expect(board.makeMove(Move(12, 28, DoublePawnPush)), "e2e4 should apply");
        ok &= Expect(board.getPieceAt(28) == P, "white pawn should land on e4");
        ok &= Expect(board.getPieceAt(12) == -1, "e2 should be empty after e2e4");
        ok &= Expect(board.enPassant == 20, "e2e4 should set en passant square to e3");
        ok &= Expect(board.sideToMove, "side to move should switch to black after white move");

        MoveList blackMoves;
        moveGen.generateAll(board, blackMoves);
        ok &= Expect(blackMoves.count == 20, "black should have 20 legal replies after e2e4");
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
        board.castling = 0;
        board.sideToMove = false;
        board.enPassant = 43;
        PlacePiece(board, K, 4);
        PlacePiece(board, bK, 60);
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

    if (!ok) {
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
