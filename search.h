#pragma once

#include <functional>
#include <optional>
#include <vector>

// Placeholder interfaces. Replace with your engine's types.
struct Move {
  // Add fields or constructors as needed.
};

enum class Color {
  White,
  Black
};

class Board {
public:
  // Returns true if the game is over (checkmate, stalemate, etc.).
  bool is_game_over() const;

  // Returns the side to move.
  Color turn() const;

  // Returns all legal moves in the current position.
  std::vector<Move> legal_moves() const;

  // Make/unmake a move. Must be reversible for search.
  void push(const Move& move);
  void pop();
};

// Evaluation function signature.
using EvalFn = std::function<int(const Board&)>;

// Alpha-beta minimax search.
int minimax(Board& board, int depth, int alpha, int beta, bool maximizing, const EvalFn& eval_fn);

// Select the best move using depth-limited minimax with alpha-beta.
// Returns std::nullopt if no move is available or time_limit elapsed before any move selected.
std::optional<Move> select_move(Board& board, int depth, const EvalFn& eval_fn,
                                std::optional<double> time_limit_seconds = std::nullopt);

