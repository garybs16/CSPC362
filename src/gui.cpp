#include "gui.hpp"
#include "board.hpp"
#include "move.hpp"
#include "movegen.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <string>

namespace {
constexpr int kBoardPixels = 640;
constexpr int kCellPixels = kBoardPixels / 8;
constexpr int kPadding = 24;
constexpr int kWindowWidth = kBoardPixels + (kPadding * 2);
constexpr int kWindowHeight = kBoardPixels + (kPadding * 2) + 64;

struct GuiState {
    Board board;
    MoveGen moveGen;
    MoveList legalMoves;
    int selectedSquare = -1;
    bool gameOver = false;
    std::string statusText;
    int totalScore = 0;
};

char PieceLabel(int pieceId) {
    static const char kLabels[12] = {'P', 'N', 'B', 'R', 'Q', 'K', 'p', 'n', 'b', 'r', 'q', 'k'};
    if (pieceId < 0 || pieceId > 11) {
        return '?';
    }
    return kLabels[pieceId];
}

int PixelToSquare(int x, int y) {
    const int boardX = x - kPadding;
    const int boardY = y - kPadding;
    if (boardX < 0 || boardY < 0 || boardX >= kBoardPixels || boardY >= kBoardPixels) {
        return -1;
    }

    const int file = boardX / kCellPixels;
    const int rowFromTop = boardY / kCellPixels;
    const int rank = 7 - rowFromTop;
    return (rank * 8) + file;
}

bool IsCurrentPlayerPiece(const GuiState& state, int square) {
    return state.board.isSidePiece(square, state.board.sideToMove);
}

bool GetMoveForSquare(const GuiState& state, int square, bool* isCapture) {
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

    Evaluate evaluation(state.board)
    evaluation.evaluateTotal();
    state.totalScore = evaluation.totalScore;

    const bool blackToMove = state.board.sideToMove;
    const bool inCheck = state.moveGen.isInCheck(state.board, blackToMove);
    if (state.legalMoves.count == 0) {
        state.gameOver = true;
        if (inCheck) {
            state.statusText = std::string(blackToMove ? "Black" : "White") + " is checkmated";
            if(balckToMove){
                    state.totalScore += 1000;
                }
            else{
                state.totalScore -= 1000;
            }
        } else {
            state.statusText = "Stalemate";
        }
        return;
    }

    state.gameOver = false;
    state.statusText = std::string(blackToMove ? "Black" : "White") + " to move";
    if (inCheck) {
        state.statusText += " - check";
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

    state.selectedSquare = -1;
    RefreshGameState(state);
    return true;
}

void DrawBoard(HDC hdc, GuiState& state) {
    const COLORREF light = RGB(240, 217, 181);
    const COLORREF dark = RGB(181, 136, 99);
    const COLORREF highlight = RGB(246, 246, 105);
    const COLORREF moveHighlight = RGB(170, 212, 140);
    const COLORREF captureHighlight = RGB(220, 128, 128);

    RECT bg = {0, 0, kWindowWidth, kWindowHeight};
    HBRUSH bgBrush = CreateSolidBrush(RGB(32, 34, 37));
    FillRect(hdc, &bg, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT pieceFont = CreateFontA(
        42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
    HFONT infoFont = CreateFontA(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, pieceFont));

    for (int row = 0; row < 8; ++row) {
        for (int file = 0; file < 8; ++file) {
            const int x = kPadding + (file * kCellPixels);
            const int y = kPadding + (row * kCellPixels);
            const int rank = 7 - row;
            const int square = (rank * 8) + file;

            bool isCaptureTarget = false;
            const bool isLegalTarget = GetMoveForSquare(state, square, &isCaptureTarget);

            COLORREF color = ((row + file) % 2 == 0) ? light : dark;
            if (square == state.selectedSquare) {
                color = highlight;
            } else if (isLegalTarget) {
                color = isCaptureTarget ? captureHighlight : moveHighlight;
            }

            RECT cell = {x, y, x + kCellPixels, y + kCellPixels};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &cell, brush);
            DeleteObject(brush);

            const int piece = state.board.getPieceAt(square);
            if (piece != -1) {
                const char label = PieceLabel(piece);
                char text[2] = {label, '\0'};
                SetTextColor(hdc, (piece <= 5) ? RGB(250, 250, 250) : RGB(15, 15, 15));
                DrawTextA(hdc, text, 1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    SelectObject(hdc, infoFont);
    SetTextColor(hdc, RGB(222, 222, 222));

    RECT status = {kPadding, kPadding + kBoardPixels + 4, kPadding + kBoardPixels, kPadding + kBoardPixels + 28};
    DrawTextA(hdc, state.statusText.c_str(), -1, &status, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT info = {kPadding, kPadding + kBoardPixels + 26, kPadding + kBoardPixels, kPadding + kBoardPixels + 50};
    std::string infoText = state.gameOver ? "Game over" : "Select a piece, then a legal destination";
    DrawTextA(hdc, infoText.c_str(), -1, &info, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(pieceFont);
    DeleteObject(infoFont);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiState* state = reinterpret_cast<GuiState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* createdState = new GuiState();
            createdState->board.defaultBoard();
            RefreshGameState(*createdState);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createdState));
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!state) {
                return 0;
            }
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int square = PixelToSquare(x, y);
            if (square < 0) {
                state->selectedSquare = -1;
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }

            if (state->gameOver) {
                return 0;
            }

            if (state->selectedSquare == square) {
                state->selectedSquare = -1;
            } else if (IsCurrentPlayerPiece(*state, square)) {
                state->selectedSquare = square;
            } else if (state->selectedSquare != -1) {
                TryApplySelectedMove(*state, hwnd, x, y, square);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case WM_PAINT: {
            if (!state) {
                return 0;
            }
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawBoard(hdc, *state);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
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
    HINSTANCE instance = GetModuleHandle(nullptr);
    const char* className = "ChessWindowClass";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassA(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0, className, "CPSC362 Chess GUI", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight, nullptr, nullptr, instance, nullptr);
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
