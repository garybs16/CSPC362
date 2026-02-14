// ui.cpp  (SFML 2.6.1)
// Connects SFML UI to your engine Board + MoveGen bitboards.

#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>

#include "board.hpp"
#include "movegen.hpp"
#include "movelist.hpp"
#include "move.hpp"

static const char* PieceName(int p) {
    switch (p) {
        case P:  return "P";  case N:  return "N";  case B:  return "B";
        case R:  return "R";  case Q:  return "Q";  case K:  return "K";
        case bP: return "bP"; case bN: return "bN"; case bB: return "bB";
        case bR: return "bR"; case bQ: return "bQ"; case bK: return "bK";
        default: return ".";
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 800), "Chess (SFML 2.6.1 + Engine)");
    window.setFramerateLimit(60);

    const int boardSize = 8;
    const float squareSize = 100.0f;

    // --- Engine objects ---
    Board board;

    // IMPORTANT: MoveGen is huge; allocate it on the heap to avoid stack overflow.
    auto movegen = std::make_unique<MoveGen>();

    MoveList movelist;

    board.setEmpty();
    board.defaultBoard();
    board.updateOccupancy();

    // Only needed if you rely on rook/bishop/queen magic attack lookups
    movegen->includeMagic();

    // Convert UI (row,col) to engine square (0..63 where 0=a1)
    auto rcToSquare = [](int row, int col) {
        int rank = 7 - row;        // UI row 0 (top) -> rank 7
        int file = col;            // col 0 -> file a
        return rank * 8 + file;    // a1 = 0
    };

    int selectedSquare = -1;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left) {

                int mouseX = event.mouseButton.x;
                int mouseY = event.mouseButton.y;

                int col = static_cast<int>(mouseX / squareSize);
                int row = static_cast<int>(mouseY / squareSize);

                if (col >= 0 && col < boardSize && row >= 0 && row < boardSize) {
                    int sq = rcToSquare(row, col);
                    int p  = board.getPieceAt(sq);

                    std::cout << "Clicked row=" << row
                              << " col=" << col
                              << " square=" << sq
                              << " piece=" << PieceName(p)
                              << " sideToMove=" << (board.sideToMove ? "black" : "white")
                              << "\n";

                    // Simple two-click move test (no legality checking here yet)
                    if (selectedSquare == -1) {
                        if (p != -1) {
                            selectedSquare = sq;
                            std::cout << "Selected square " << selectedSquare << "\n";
                        }
                    } else {
                        if (sq != selectedSquare) {
                            Move m(selectedSquare, sq, 0);
                            board.makeMove(m);
                            board.updateOccupancy();
                        }
                        selectedSquare = -1;
                    }
                }
            }
        }

        // --- Draw ---
        window.clear();

        for (int row = 0; row < boardSize; ++row) {
            for (int col = 0; col < boardSize; ++col) {
                sf::RectangleShape square(sf::Vector2f(squareSize, squareSize));
                square.setPosition(col * squareSize, row * squareSize);

                bool isDark = (row + col) % 2 == 1;
                square.setFillColor(isDark ? sf::Color(118, 150, 86)
                                           : sf::Color(238, 238, 210));

                int sq = rcToSquare(row, col);
                if (sq == selectedSquare) {
                    square.setFillColor(sf::Color(240, 200, 60)); // highlight selected
                }

                window.draw(square);

                // Draw piece from engine board
                int p = board.getPieceAt(sq);
                if (p != -1) {
                    bool isWhite = (p >= P && p <= K);

                    float radius = squareSize * 0.4f;
                    sf::CircleShape pieceShape(radius);
                    pieceShape.setOrigin(radius, radius);

                    float centerX = col * squareSize + squareSize / 2.0f;
                    float centerY = row * squareSize + squareSize / 2.0f;
                    pieceShape.setPosition(centerX, centerY);

                    if (isWhite) {
                        pieceShape.setFillColor(sf::Color::White);
                        pieceShape.setOutlineColor(sf::Color::Black);
                    } else {
                        pieceShape.setFillColor(sf::Color::Black);
                        pieceShape.setOutlineColor(sf::Color::White);
                    }
                    pieceShape.setOutlineThickness(3.0f);

                    window.draw(pieceShape);
                }
            }
        }

        window.display();
    }

    return 0;
}
