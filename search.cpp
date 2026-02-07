#include "search.h"

#include <chrono>
#include <limits>

namespace {
constexpr int kInf = 1000000000;
}

int minimax(Board& board, int depth, int alpha, int beta, bool maximizing, const EvalFn& eval_fn) {
  if (depth == 0 || board.is_game_over()) {
    return eval_fn(board);
  }

  if (maximizing) {
    int max_eval = -kInf;
    for (const Move& move : board.legal_moves()) {
      board.push(move);
      int val = minimax(board, depth - 1, alpha, beta, false, eval_fn);
      board.pop();

      if (val > max_eval) {
        max_eval = val;
      }
      if (val > alpha) {
        alpha = val;
      }
      if (beta <= alpha) {
        break;
      }
    }
    return max_eval;
  }

  int min_eval = kInf;
  for (const Move& move : board.legal_moves()) {
    board.push(move);
    int val = minimax(board, depth - 1, alpha, beta, true, eval_fn);
    board.pop();

    if (val < min_eval) {
      min_eval = val;
    }
    if (val < beta) {
      beta = val;
    }
    if (beta <= alpha) {
      break;
    }
  }
  return min_eval;
}

std::optional<Move> select_move(Board& board, int depth, const EvalFn& eval_fn,
                                std::optional<double> time_limit_seconds) {
  std::optional<Move> best_move;
  int best_value = (board.turn() == Color::White) ? -kInf : kInf;

  const auto start = std::chrono::steady_clock::now();

  for (const Move& move : board.legal_moves()) {
    if (time_limit_seconds.has_value()) {
      const auto now = std::chrono::steady_clock::now();
      const std::chrono::duration<double> elapsed = now - start;
      if (elapsed.count() > *time_limit_seconds) {
        break;
      }
    }

    board.push(move);
    bool maximizing = (board.turn() == Color::Black);  // not board.turn() after push.
    int val = minimax(board, depth - 1, -kInf, kInf, maximizing, eval_fn);
    board.pop();

    if (board.turn() == Color::White) {
      if (val > best_value) {
        best_value = val;
        best_move = move;
      }
    } else {
      if (val < best_value) {
        best_value = val;
        best_move = move;
      }
    }
  }

  return best_move;
}

