#include "gui.hpp"

#include "ai.hpp"
#include "board.hpp"
#include "move.hpp"
#include "movegen.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <string>

namespace {
constexpr int kBoardPixels = 896;
constexpr int kCellPixels = kBoardPixels / 8;
constexpr int kPadding = 32;
constexpr int kPanelWidth = 300;
constexpr int kPanelHeight = 640;
constexpr int kStatusHeight = 92;
constexpr int kWindowWidth = kBoardPixels + kPanelWidth + (kPadding * 3);
constexpr int kWindowHeight = kBoardPixels + kStatusHeight + (kPadding * 2);
constexpr int kBoardBorder = 10;
constexpr int kButtonHeight = 48;
constexpr int kButtonGap = 16;
constexpr UINT kRobotTurnMessage = WM_APP + 1;

enum class GameMode {
    HumanVsHuman,
    HumanVsRobot
};

struct GuiState {
    Board board;
    MoveGen moveGen;
    ChessAi ai;
    MoveList legalMoves;
    int selectedSquare = -1;
    bool gameOver = false;
    GameMode mode = GameMode::HumanVsRobot;
    RobotDifficulty difficulty = RobotDifficulty::Medium;
    bool humanPlaysBlack = false;
    std::string statusText;
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

const char* DifficultyLabel(RobotDifficulty difficulty) {
    switch (difficulty) {
        case RobotDifficulty::Beginner:
            return "Beginner";
        case RobotDifficulty::Easy:
            return "Easy";
        case RobotDifficulty::Medium:
            return "Medium";
        case RobotDifficulty::Hard:
            return "Hard";
    }
    return "Easy";
}

const char* HumanSideLabel(bool humanPlaysBlack) {
    return humanPlaysBlack ? "Black" : "White";
}

RECT MakeRect(int left, int top, int width, int height) {
    RECT rect = {left, top, left + width, top + height};
    return rect;
}

RECT BoardRect() {
    return MakeRect(kPadding, kPadding, kBoardPixels, kBoardPixels);
}

RECT OuterBoardRect() {
    return MakeRect(kPadding - kBoardBorder, kPadding - kBoardBorder, kBoardPixels + (kBoardBorder * 2), kBoardPixels + (kBoardBorder * 2));
}

int PanelLeft() {
    return kPadding + kBoardPixels + kPadding;
}

RECT PanelRect() {
    return MakeRect(PanelLeft(), kPadding, kPanelWidth, kPanelHeight);
}

RECT NewGameRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 88, kPanelWidth - 48, kButtonHeight);
}

RECT ModeRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 152, kPanelWidth - 48, kButtonHeight);
}

RECT SideRect() {
    return MakeRect(PanelLeft() + 24, kPadding + 216, kPanelWidth - 48, kButtonHeight);
}

RECT DifficultyRect(int index) {
    return MakeRect(PanelLeft() + 24, kPadding + 346 + (index * (kButtonHeight + kButtonGap)), kPanelWidth - 48, kButtonHeight);
}

RECT StatusPanelRect() {
    return MakeRect(kPadding, kPadding + kBoardPixels + 16, kBoardPixels, kStatusHeight);
}

bool IsRobotTurn(const GuiState& state) {
    return state.mode == GameMode::HumanVsRobot && state.board.sideToMove != state.humanPlaysBlack;
}

bool IsCurrentPlayerPiece(const GuiState& state, int square) {
    return state.board.isSidePiece(square, state.board.sideToMove);
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
    HPEN pen = CreatePen(PS_SOLID, 2, borderColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 22, 22);
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
        if (inCheck) {
            state.statusText = std::string(blackToMove ? "Black" : "White") + " is checkmated";
        } else {
            state.statusText = "Stalemate";
        }
        return;
    }

    state.gameOver = false;
    if (IsRobotTurn(state)) {
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
    RefreshGameState(state);
}

void QueueRobotTurnIfNeeded(const GuiState& state, HWND hwnd) {
    if (!state.gameOver && IsRobotTurn(state)) {
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

    state.selectedSquare = -1;
    RefreshGameState(state);
    return true;
}

void RunRobotTurn(GuiState& state, HWND hwnd) {
    while (!state.gameOver && IsRobotTurn(state)) {
        state.selectedSquare = -1;
        state.statusText = "Robot thinking...";
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);

        Move robotMove;
        if (!state.ai.findMove(state.board, state.moveGen, state.difficulty, robotMove)) {
            RefreshGameState(state);
            return;
        }

        if (!state.board.makeMove(robotMove)) {
            RefreshGameState(state);
            return;
        }

        RefreshGameState(state);
    }
}

void DrawButton(HDC hdc, const RECT& rect, const char* text, bool active) {
    const COLORREF fillColor = active ? RGB(182, 122, 51) : RGB(57, 62, 70);
    const COLORREF borderColor = active ? RGB(232, 190, 116) : RGB(103, 110, 120);
    const COLORREF shadowColor = RGB(17, 19, 22);
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 0, 5);
    DrawRoundPanel(hdc, shadowRect, shadowColor, shadowColor);
    DrawRoundPanel(hdc, rect, fillColor, borderColor);
    DrawTextCenterA(hdc, rect, text, RGB(245, 242, 235));
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
        DrawTextLeftA(hdc, bottomRect, label, RGB(245, 236, 220));
    }

    for (int rank = 0; rank < 8; ++rank) {
        RECT leftRect = {
            boardRect.left + 8,
            boardRect.top + (rank * kCellPixels) + 6,
            boardRect.left + 30,
            boardRect.top + ((rank + 1) * kCellPixels)
        };
        char label[2] = {static_cast<char>('8' - rank), '\0'};
        DrawTextLeftA(hdc, leftRect, label, RGB(245, 236, 220));
    }

    SelectObject(hdc, oldFont);
}

void DrawControls(HDC hdc, const GuiState& state, HFONT titleFont, HFONT labelFont, HFONT buttonFont) {
    RECT panel = PanelRect();
    DrawRoundPanel(hdc, panel, RGB(35, 39, 46), RGB(76, 84, 96));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    SetBkMode(hdc, TRANSPARENT);

    RECT header = {panel.left + 24, panel.top + 24, panel.right - 24, panel.top + 60};
    DrawTextLeftA(hdc, header, "CPSC362 Chess", RGB(242, 236, 225));

    SelectObject(hdc, labelFont);
    RECT subHeader = {panel.left + 24, panel.top + 58, panel.right - 24, panel.top + 84};
    DrawTextLeftA(hdc, subHeader, "Sharper board, larger layout, robot play", RGB(172, 178, 189));

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, NewGameRect(), "New Game", false);

    std::string modeText = std::string("Mode: ") + ModeLabel(state.mode);
    DrawButton(hdc, ModeRect(), modeText.c_str(), state.mode == GameMode::HumanVsRobot);

    std::string sideText = std::string("Human: ") + HumanSideLabel(state.humanPlaysBlack);
    DrawButton(hdc, SideRect(), sideText.c_str(), state.humanPlaysBlack);

    SelectObject(hdc, labelFont);
    RECT difficultyHeader = {panel.left + 24, panel.top + 288, panel.right - 24, panel.top + 320};
    DrawTextLeftA(hdc, difficultyHeader, "Robot Difficulty", RGB(220, 226, 233));

    SelectObject(hdc, buttonFont);
    DrawButton(hdc, DifficultyRect(0), "Beginner", state.difficulty == RobotDifficulty::Beginner);
    DrawButton(hdc, DifficultyRect(1), "Easy", state.difficulty == RobotDifficulty::Easy);
    DrawButton(hdc, DifficultyRect(2), "Medium", state.difficulty == RobotDifficulty::Medium);
    DrawButton(hdc, DifficultyRect(3), "Hard", state.difficulty == RobotDifficulty::Hard);

    SelectObject(hdc, labelFont);
    RECT footer = {panel.left + 24, panel.top + 596, panel.right - 24, panel.bottom - 20};
    std::string footerText = std::string("Current difficulty: ") + DifficultyLabel(state.difficulty) + "\n";
    footerText += state.mode == GameMode::HumanVsRobot
        ? "Robot always takes the opposite color."
        : "Both sides stay manual in this mode.";
    DrawTextLeftA(hdc, footer, footerText.c_str(), RGB(176, 183, 192), DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
}

void DrawStatusPanel(HDC hdc, const GuiState& state, HFONT titleFont, HFONT bodyFont) {
    RECT panel = StatusPanelRect();
    DrawRoundPanel(hdc, panel, RGB(33, 38, 44), RGB(79, 87, 98));

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    SetBkMode(hdc, TRANSPARENT);

    RECT statusTitle = {panel.left + 26, panel.top + 14, panel.right - 26, panel.top + 40};
    DrawTextLeftA(hdc, statusTitle, state.statusText.c_str(), RGB(243, 236, 222));

    SelectObject(hdc, bodyFont);
    RECT info = {panel.left + 26, panel.top + 44, panel.right - 26, panel.bottom - 18};
    std::string infoText;
    if (state.gameOver) {
        infoText = "Game over. Use New Game to reset the board.";
    } else if (IsRobotTurn(state)) {
        infoText = "Robot is active for this turn. Difficulty can be changed from the right panel.";
    } else {
        infoText = "Click a piece, then click one of its highlighted legal destinations.";
    }
    DrawTextLeftA(hdc, info, infoText.c_str(), RGB(176, 182, 190), DT_LEFT | DT_TOP | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
}

void DrawBoard(HDC hdc, const GuiState& state) {
    const COLORREF light = RGB(236, 222, 196);
    const COLORREF dark = RGB(123, 88, 57);
    const COLORREF selectedColor = RGB(231, 190, 76);
    const COLORREF moveColor = RGB(94, 142, 110);
    const COLORREF captureColor = RGB(168, 75, 62);
    const RECT fullRect = {0, 0, kWindowWidth, kWindowHeight};
    const RECT outerBoard = OuterBoardRect();
    const RECT boardRect = BoardRect();

    FillVerticalGradient(hdc, fullRect, RGB(20, 24, 29), RGB(43, 49, 58));
    DrawRoundPanel(hdc, outerBoard, RGB(46, 35, 24), RGB(128, 97, 66));

    HFONT pieceFont = CreateFontW(
        76, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Symbol");
    HFONT titleFont = CreateFontA(
        26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Bahnschrift");
    HFONT labelFont = CreateFontA(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
    HFONT buttonFont = CreateFontA(
        20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Bahnschrift");
    HFONT coordFont = CreateFontA(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");

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

            COLORREF color = ((row + file) % 2 == 0) ? light : dark;
            if (square == state.selectedSquare) {
                color = selectedColor;
            } else if (isLegalTarget) {
                color = isCaptureTarget ? captureColor : moveColor;
            }

            RECT cell = {x, y, x + kCellPixels, y + kCellPixels};
            FillRectColor(hdc, cell, color);

            HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(76, 54, 35));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gridPen));
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
            Rectangle(hdc, cell.left, cell.top, cell.right, cell.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(gridPen);

            const int piece = state.board.getPieceAt(square);
            if (piece != -1) {
                const wchar_t glyph = PieceGlyph(piece);
                wchar_t text[2] = {glyph, L'\0'};
                RECT shadowRect = cell;
                OffsetRect(&shadowRect, 4, 4);
                SetTextColor(hdc, RGB(24, 18, 12));
                DrawTextW(hdc, text, 1, &shadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT pieceRect = cell;
                SetTextColor(hdc, piece <= 5 ? RGB(250, 246, 240) : RGB(38, 26, 18));
                DrawTextW(hdc, text, 1, &pieceRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    DrawRectOutline(hdc, boardRect, RGB(146, 112, 77), 3);
    DrawCoordinates(hdc, coordFont);
    DrawStatusPanel(hdc, state, titleFont, labelFont);
    DrawControls(hdc, state, titleFont, labelFont, buttonFont);

    SelectObject(hdc, oldFont);
    DeleteObject(pieceFont);
    DeleteObject(titleFont);
    DeleteObject(labelFont);
    DeleteObject(buttonFont);
    DeleteObject(coordFont);
}

bool HandleControlClick(GuiState& state, HWND hwnd, const POINT& point) {
    const RECT newGameRect = NewGameRect();
    const RECT modeRect = ModeRect();
    const RECT sideRect = SideRect();
    const RECT beginnerRect = DifficultyRect(0);
    const RECT easyRect = DifficultyRect(1);
    const RECT mediumRect = DifficultyRect(2);
    const RECT hardRect = DifficultyRect(3);

    if (PtInRect(&newGameRect, point)) {
        StartNewGame(state);
    } else if (PtInRect(&modeRect, point)) {
        state.mode = state.mode == GameMode::HumanVsRobot ? GameMode::HumanVsHuman : GameMode::HumanVsRobot;
        StartNewGame(state);
    } else if (PtInRect(&sideRect, point)) {
        state.humanPlaysBlack = !state.humanPlaysBlack;
        StartNewGame(state);
    } else if (PtInRect(&beginnerRect, point)) {
        state.difficulty = RobotDifficulty::Beginner;
        state.selectedSquare = -1;
        RefreshGameState(state);
    } else if (PtInRect(&easyRect, point)) {
        state.difficulty = RobotDifficulty::Easy;
        state.selectedSquare = -1;
        RefreshGameState(state);
    } else if (PtInRect(&mediumRect, point)) {
        state.difficulty = RobotDifficulty::Medium;
        state.selectedSquare = -1;
        RefreshGameState(state);
    } else if (PtInRect(&hardRect, point)) {
        state.difficulty = RobotDifficulty::Hard;
        state.selectedSquare = -1;
        RefreshGameState(state);
    } else {
        return false;
    }

    InvalidateRect(hwnd, nullptr, FALSE);
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
            QueueRobotTurnIfNeeded(*createdState, hwnd);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            if (!state) {
                return 0;
            }

            const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (HandleControlClick(*state, hwnd, point)) {
                return 0;
            }

            if (state->gameOver || IsRobotTurn(*state)) {
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
                QueueRobotTurnIfNeeded(*state, hwnd);
            }

            InvalidateRect(hwnd, nullptr, FALSE);
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
        0, className, "CPSC362 Chess", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, instance, nullptr);
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
