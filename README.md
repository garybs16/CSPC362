# CSPC362

A bitboard-based C++ chess application with a Win32/GDI graphical interface and a custom chess engine.

## Feature Alignment

This project matches the feature set described in the presentation:

* Human vs Human and Human vs Robot game modes.
* Human color selection from the GUI.
* Four robot difficulty levels: `Easy`, `Medium`, `Hard`, and `Grandmaster`.
* Start, New Game, Surrender, and Time Control controls.
* Select-and-move GUI input with legal destination highlighting.
* Chess clocks with 1, 3, 5, 10, and 15 minute presets.
* Bitboard-based board representation.
* Legal move generation for pawns, knights, bishops, rooks, queens, and kings.
* Special rules for castling, en passant, and pawn promotion.
* Check, checkmate, stalemate, timeout, and surrender game-ending states.
* Robot move selection using search and evaluation.
* Post-game review with move classification, accuracy, best move comparison, and an evaluation bar.

## Build and Run

From `src`, build the GUI application with:

```sh
mingw32-make
```

Run the application:

```sh
./engine.exe
```

## Tests

Build the test executable:

```sh
mingw32-make test
```

Run the tests:

```sh
./engine_tests.exe
```

The tests cover core engine behavior including initial legal move count, pinned-piece legality, castling, en passant, promotion, and AI move selection.

## Future Work

These items are listed as next steps in the presentation and are not required for the current delivered feature set:

* Add draw detection for the fifty-move rule and threefold repetition.
* Add animations, sounds, and a move history panel.
* Implement save and load using PGN notation.
* Explore porting the engine to the web.
