// gui_win32.cpp — interface graphique native Windows de Calc (API Win32 pure, aucune dépendance).
//
// Même interpréteur que la version console : la fenêtre ne fait qu'afficher.
//   - historique des calculs (double-clic : reprendre l'expression)
//   - pavé numérique et fonctions, saisie clavier, ↑ / ↓ pour rappeler les expressions
//   - panneau des variables et fonctions définies (double-clic : insérer)
//   - en cas d'erreur, le caractère fautif est sélectionné dans la saisie
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <string>
#include <vector>

#include "interpreter.hpp"

namespace {

// ── Palette (identique au portfolio) ─────────────────────────────
const COLORREF BG = RGB(13, 16, 23), PANEL = RGB(21, 26, 35), FIELD = RGB(28, 34, 48), BORDER = RGB(42, 50, 66);
const COLORREF TEXT = RGB(230, 232, 238), MUTED = RGB(138, 147, 166), ACCENT = RGB(34, 211, 238);
const COLORREF ACCENT_DARK = RGB(14, 116, 144), DANGER = RGB(248, 113, 113), KEY_NUM = RGB(36, 43, 58);

enum { ID_HISTORY = 100, ID_INPUT, ID_VARS, ID_FUNCS, ID_RESET, ID_HELP, ID_KEY0 = 200 };
enum class KeyKind { Number, Operator, Function, Action, Equal };

struct Key { const wchar_t* label; const char* insert; KeyKind kind; };

// Pavé : 5 rangées × 7 colonnes. insert == nullptr → action spéciale (effacer, retour, égal).
const Key KEYS[] = {
    {L"sin", "sin(", KeyKind::Function}, {L"cos", "cos(", KeyKind::Function}, {L"tan", "tan(", KeyKind::Function},
    {L"7", "7", KeyKind::Number}, {L"8", "8", KeyKind::Number}, {L"9", "9", KeyKind::Number}, {L"÷", "/", KeyKind::Operator},
    {L"ln", "ln(", KeyKind::Function}, {L"log", "log(", KeyKind::Function}, {L"√", "sqrt(", KeyKind::Function},
    {L"4", "4", KeyKind::Number}, {L"5", "5", KeyKind::Number}, {L"6", "6", KeyKind::Number}, {L"×", "*", KeyKind::Operator},
    {L"x²", "^2", KeyKind::Function}, {L"xʸ", "^", KeyKind::Function}, {L"|x|", "abs(", KeyKind::Function},
    {L"1", "1", KeyKind::Number}, {L"2", "2", KeyKind::Number}, {L"3", "3", KeyKind::Number}, {L"−", "-", KeyKind::Operator},
    {L"π", "pi", KeyKind::Function}, {L"e", "e", KeyKind::Function}, {L"ans", "ans", KeyKind::Function},
    {L"0", "0", KeyKind::Number}, {L".", ".", KeyKind::Number}, {L"%", "%", KeyKind::Operator}, {L"+", "+", KeyKind::Operator},
    {L"(", "(", KeyKind::Function}, {L")", ")", KeyKind::Function}, {L",", ",", KeyKind::Function},
    {L"C", nullptr, KeyKind::Action}, {L"←", nullptr, KeyKind::Action}, {L"=", nullptr, KeyKind::Equal},
};
constexpr int KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);
constexpr int COLS = 7, ROWS = 5;

struct HistoryItem { std::wstring expr, result; bool info; };   // info : définition de fonction

struct App {
    HWND wnd{}, history{}, input{}, vars{}, funcs{}, reset{}, help{};
    HWND keys[KEY_COUNT]{};
    HFONT ui{}, uiBold{}, big{}, mono{}, small{}, keyFont{};
    HBRUSH bgBrush{}, panelBrush{}, fieldBrush{};
    calc::Interpreter interp;
    std::vector<HistoryItem> items;
    std::vector<std::wstring> recall;
    size_t recallPos = 0;
    std::wstring status;
    bool statusError = false;
    double scale = 1.0;
    RECT inputBox{}, historyBox{}, sideBox{}, statusBox{}, varsLabel{}, funcsLabel{};
    WNDPROC editProc{};
} g;

int S(double v) { return static_cast<int>(v * g.scale + 0.5); }

// ── Conversions UTF-8 ⇄ UTF-16 ───────────────────────────────────
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring inputText() {
    int n = GetWindowTextLengthW(g.input);
    std::wstring w(n, L'\0');
    if (n) GetWindowTextW(g.input, w.data(), n + 1);
    return w;
}

HFONT makeFont(int px, int weight, const wchar_t* face) {
    return CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

void setStatus(const std::wstring& text, bool error) {
    g.status = text;
    g.statusError = error;
    InvalidateRect(g.wnd, &g.statusBox, TRUE);
}

void refreshSide() {
    SendMessageW(g.vars, LB_RESETCONTENT, 0, 0);
    for (const auto& v : g.interp.variables()) SendMessageW(g.vars, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(v).c_str()));
    SendMessageW(g.funcs, LB_RESETCONTENT, 0, 0);
    const auto fs = g.interp.functions();
    if (fs.empty()) SendMessageW(g.funcs, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(aucune — ex. f(x) = x^2)"));
    for (const auto& f : fs) SendMessageW(g.funcs, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(f).c_str()));
}

void addHistory(HistoryItem item) {
    g.items.push_back(std::move(item));
    if (g.items.size() > 500) {   // historique borné
        g.items.erase(g.items.begin());
        SendMessageW(g.history, LB_DELETESTRING, 0, 0);
    }
    int idx = static_cast<int>(SendMessageW(g.history, LB_ADDSTRING, 0, 0));
    SendMessageW(g.history, LB_SETTOPINDEX, idx, 0);
}

void insertText(const std::wstring& text) {
    SendMessageW(g.input, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    SetFocus(g.input);
}

void evaluate() {
    std::wstring raw = inputText();
    size_t a = raw.find_first_not_of(L" \t"), b = raw.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return;
    std::wstring expr = raw.substr(a, b - a + 1);
    if (g.recall.empty() || g.recall.back() != expr) g.recall.push_back(expr);
    g.recallPos = g.recall.size();
    try {
        const calc::Result r = g.interp.execute(narrow(expr));
        std::wstring shown;
        switch (r.kind) {
            case calc::Result::Kind::Value: shown = L"= " + widen(calc::formatNumber(r.value)); break;
            case calc::Result::Kind::Assigned: shown = widen(r.name) + L" = " + widen(calc::formatNumber(r.value)); break;
            case calc::Result::Kind::Defined: shown = L"fonction " + widen(r.name) + L" définie"; break;
        }
        addHistory({expr, shown, r.kind == calc::Result::Kind::Defined});
        SetWindowTextW(g.input, L"");
        setStatus(L"", false);
        refreshSide();
    } catch (const calc::CalcError& e) {
        setStatus(L"Erreur : " + widen(e.what()), true);
        // Sélectionne le caractère fautif (positions en octets UTF-8 → caractères)
        std::string bytes = narrow(raw);
        size_t pos = std::min(e.position() + narrow(raw.substr(0, a)).size(), bytes.size());
        int wpos = static_cast<int>(widen(bytes.substr(0, pos)).size());
        SendMessageW(g.input, EM_SETSEL, wpos, wpos + 1);
    }
    SetFocus(g.input);
}

void showHelp() {
    MessageBoxW(g.wnd,
        L"Opérateurs :  + − × ÷ % ^ ( )\n"
        L"Variables :  rayon = 3     puis   2 * pi * rayon\n"
        L"                 ans = dernier résultat,  pi et e prédéfinis\n"
        L"Fonctions :  f(x) = x^2 + 1     puis   f(4)\n"
        L"Intégrées :  sin cos tan asin acos atan sqrt exp ln log log2\n"
        L"                 abs floor ceil round pow hypot min max\n\n"
        L"Clavier :  Entrée = calculer,  Échap = effacer,  ↑ ↓ = expressions précédentes\n"
        L"Double-clic sur l'historique ou une variable pour la réutiliser.",
        L"Aide — Calc", MB_OK | MB_ICONINFORMATION);
}

// La saisie intercepte Entrée, Échap et les flèches pour rappeler l'historique.
LRESULT CALLBACK EditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { evaluate(); return 0; }
        if (wp == VK_ESCAPE) { SetWindowTextW(h, L""); setStatus(L"", false); return 0; }
        if ((wp == VK_UP || wp == VK_DOWN) && !g.recall.empty()) {
            if (wp == VK_UP && g.recallPos > 0) --g.recallPos;
            if (wp == VK_DOWN && g.recallPos < g.recall.size()) ++g.recallPos;
            SetWindowTextW(h, g.recallPos < g.recall.size() ? g.recall[g.recallPos].c_str() : L"");
            int n = GetWindowTextLengthW(h);
            SendMessageW(h, EM_SETSEL, n, n);
            return 0;
        }
    }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;   // pas de bip
    return CallWindowProcW(g.editProc, h, msg, wp, lp);
}

void layout(int w, int h) {
    const int m = S(16), gap = S(10), side = S(250);
    const int keyH = S(46), keyGap = S(6), padH = ROWS * keyH + (ROWS - 1) * keyGap;
    const int inputH = S(50), statusH = S(24);
    const int left = m, right = w - m - side - gap;
    int y = h - m - padH;
    for (int i = 0; i < KEY_COUNT; ++i) {
        int row = i / COLS, col = i % COLS;
        int kw = (right - left - (COLS - 1) * keyGap) / COLS;
        int x = left + col * (kw + keyGap);
        int width = (i == KEY_COUNT - 1) ? 2 * kw + keyGap : kw;   // « = » occupe deux cases
        MoveWindow(g.keys[i], x, y + row * (keyH + keyGap), width, keyH, TRUE);
    }
    g.statusBox = {left, y - gap - statusH, right, y - gap};
    g.inputBox = {left, g.statusBox.top - inputH, right, g.statusBox.top};
    const int fontH = S(24);
    MoveWindow(g.input, left + S(14), g.inputBox.top + (inputH - fontH) / 2, right - left - S(28), fontH + S(2), TRUE);
    g.historyBox = {left, m, right, g.inputBox.top - gap};
    MoveWindow(g.history, left + S(2), m + S(2), right - left - S(4), g.historyBox.bottom - m - S(4), TRUE);

    g.sideBox = {right + gap, m, w - m, h - m};
    const int sx = g.sideBox.left + S(14), sw = g.sideBox.right - g.sideBox.left - S(28);
    const int btnH = S(36);
    int sy = m + S(14);
    g.varsLabel = {sx, sy, sx + sw, sy + S(18)};
    sy += S(22);
    int listH = (g.sideBox.bottom - sy - S(14) - btnH - S(10) - S(22) - S(10)) / 2;
    MoveWindow(g.vars, sx, sy, sw, listH, TRUE);
    sy += listH + S(10);
    g.funcsLabel = {sx, sy, sx + sw, sy + S(18)};
    sy += S(22);
    MoveWindow(g.funcs, sx, sy, sw, listH, TRUE);
    int by = g.sideBox.bottom - S(14) - btnH;
    MoveWindow(g.reset, sx, by, (sw - S(8)) / 2, btnH, TRUE);
    MoveWindow(g.help, sx + (sw + S(8)) / 2, by, (sw - S(8)) / 2, btnH, TRUE);
    InvalidateRect(g.wnd, nullptr, TRUE);
}

void fillRound(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

void paint(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    FillRect(dc, &rc, g.bgBrush);
    fillRound(dc, g.historyBox, PANEL, PANEL, S(12));
    fillRound(dc, g.sideBox, PANEL, PANEL, S(12));
    fillRound(dc, g.inputBox, FIELD, GetFocus() == g.input ? ACCENT : BORDER, S(10));
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g.small);
    SetTextColor(dc, MUTED);
    DrawTextW(dc, L"VARIABLES", -1, &g.varsLabel, DT_LEFT | DT_SINGLELINE);
    DrawTextW(dc, L"FONCTIONS", -1, &g.funcsLabel, DT_LEFT | DT_SINGLELINE);
    SelectObject(dc, g.ui);
    SetTextColor(dc, g.statusError ? DANGER : MUTED);
    RECT sr = g.statusBox; sr.left += S(4);
    std::wstring st = g.status.empty() ? L"Entrée = calculer · ↑ ↓ = rappeler · double-clic = réutiliser" : g.status;
    DrawTextW(dc, st.c_str(), -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (g.items.empty()) {
        RECT hr = g.historyBox;
        SetTextColor(dc, MUTED);
        DrawTextW(dc, L"Tapez un calcul, par exemple   2 ^ 10 / (3 + 1)", -1, &hr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    EndPaint(h, &ps);
}

void drawKey(const DRAWITEMSTRUCT* d) {
    const Key& k = KEYS[d->CtlID - ID_KEY0];
    bool pressed = d->itemState & ODS_SELECTED;
    COLORREF fill = KEY_NUM, fg = TEXT;
    switch (k.kind) {
        case KeyKind::Number: fill = KEY_NUM; break;
        case KeyKind::Operator: fill = FIELD; fg = ACCENT; break;
        case KeyKind::Function: fill = FIELD; fg = RGB(190, 197, 210); break;
        case KeyKind::Action: fill = FIELD; fg = DANGER; break;
        case KeyKind::Equal: fill = ACCENT_DARK; fg = RGB(255, 255, 255); break;
    }
    if (pressed) fill = k.kind == KeyKind::Equal ? ACCENT : BORDER;
    FillRect(d->hDC, &d->rcItem, g.bgBrush);
    fillRound(d->hDC, d->rcItem, fill, fill, S(10));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, fg);
    SelectObject(d->hDC, k.kind == KeyKind::Function ? g.ui : g.keyFont);
    RECT r = d->rcItem;
    DrawTextW(d->hDC, k.label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawButton(const DRAWITEMSTRUCT* d, const wchar_t* label) {
    bool pressed = d->itemState & ODS_SELECTED;
    FillRect(d->hDC, &d->rcItem, g.panelBrush);
    fillRound(d->hDC, d->rcItem, pressed ? BORDER : FIELD, BORDER, S(8));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, TEXT);
    SelectObject(d->hDC, g.ui);
    RECT r = d->rcItem;
    DrawTextW(d->hDC, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawHistory(const DRAWITEMSTRUCT* d) {
    if (d->itemID == static_cast<UINT>(-1)) return;
    if (d->itemID >= g.items.size()) return;
    const HistoryItem& it = g.items[d->itemID];
    RECT r = d->rcItem;
    HBRUSH b = CreateSolidBrush((d->itemState & ODS_SELECTED) ? FIELD : PANEL);
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    SetBkMode(d->hDC, TRANSPARENT);
    RECT top = {r.left + S(14), r.top + S(6), r.right - S(14), r.top + S(26)};
    SelectObject(d->hDC, g.mono);
    SetTextColor(d->hDC, MUTED);
    DrawTextW(d->hDC, it.expr.c_str(), -1, &top, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    RECT bottom = {r.left + S(14), r.top + S(22), r.right - S(20), r.bottom - S(4)};
    SelectObject(d->hDC, it.info ? g.ui : g.big);
    SetTextColor(d->hDC, it.info ? ACCENT : TEXT);
    DrawTextW(d->hDC, it.result.c_str(), -1, &bottom, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void onKey(int index) {
    const Key& k = KEYS[index];
    if (k.insert) { insertText(widen(k.insert)); return; }
    if (k.kind == KeyKind::Equal) { evaluate(); return; }
    if (k.label[0] == L'C') { SetWindowTextW(g.input, L""); setStatus(L"", false); SetFocus(g.input); return; }
    // ← : efface la sélection ou le caractère avant le curseur
    DWORD s = 0, e = 0;
    SendMessageW(g.input, EM_GETSEL, reinterpret_cast<WPARAM>(&s), reinterpret_cast<LPARAM>(&e));
    if (s == e && s > 0) SendMessageW(g.input, EM_SETSEL, s - 1, e);
    insertText(L"");
}

std::wstring listText(HWND list, int idx) {
    int n = static_cast<int>(SendMessageW(list, LB_GETTEXTLEN, idx, 0));
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    SendMessageW(list, LB_GETTEXT, idx, reinterpret_cast<LPARAM>(w.data()));
    return w;
}

void createControls(HWND h) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    g.history = CreateWindowExW(0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_HISTORY), inst, nullptr);
    g.input = CreateWindowExW(0, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_INPUT), inst, nullptr);
    SendMessageW(g.input, EM_SETLIMITTEXT, 1000, 0);
    g.editProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g.input, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc)));
    const DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
    g.vars = CreateWindowExW(0, L"LISTBOX", nullptr, listStyle, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_VARS), inst, nullptr);
    g.funcs = CreateWindowExW(0, L"LISTBOX", nullptr, listStyle, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FUNCS), inst, nullptr);
    g.reset = CreateWindowExW(0, L"BUTTON", L"Tout effacer", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_RESET), inst, nullptr);
    g.help = CreateWindowExW(0, L"BUTTON", L"Aide", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_HELP), inst, nullptr);
    for (int i = 0; i < KEY_COUNT; ++i)
        g.keys[i] = CreateWindowExW(0, L"BUTTON", KEYS[i].label, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_KEY0 + i)), inst, nullptr);

    SendMessageW(g.input, WM_SETFONT, reinterpret_cast<WPARAM>(g.big), TRUE);
    SendMessageW(g.vars, WM_SETFONT, reinterpret_cast<WPARAM>(g.mono), TRUE);
    SendMessageW(g.funcs, WM_SETFONT, reinterpret_cast<WPARAM>(g.mono), TRUE);
    SendMessageW(g.history, LB_SETITEMHEIGHT, 0, S(58));
    for (HWND list : {g.history, g.vars, g.funcs}) SetWindowTheme(list, L"DarkMode_Explorer", nullptr);  // barres de défilement sombres
    refreshSide();
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: g.wnd = h; createControls(h); return 0;
        case WM_SIZE: layout(LOWORD(lp), HIWORD(lp)); return 0;
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize = {S(760), S(560)};
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: paint(h); return 0;
        case WM_SETFOCUS: SetFocus(g.input); return 0;
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wp), TEXT);
            SetBkColor(reinterpret_cast<HDC>(wp), FIELD);
            return reinterpret_cast<LRESULT>(g.fieldBrush);
        case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(wp), TEXT);
            SetBkColor(reinterpret_cast<HDC>(wp), PANEL);
            return reinterpret_cast<LRESULT>(g.panelBrush);
        case WM_DRAWITEM: {
            auto* d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
            if (d->CtlID == ID_HISTORY) drawHistory(d);
            else if (d->CtlID == ID_RESET) drawButton(d, L"Tout effacer");
            else if (d->CtlID == ID_HELP) drawButton(d, L"Aide");
            else if (d->CtlID >= ID_KEY0) drawKey(d);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            if (id == ID_INPUT && (code == EN_SETFOCUS || code == EN_KILLFOCUS)) InvalidateRect(h, &g.inputBox, TRUE);
            else if (id >= ID_KEY0 && id < ID_KEY0 + KEY_COUNT) onKey(id - ID_KEY0);
            else if (id == ID_HELP) showHelp();
            else if (id == ID_RESET) {
                if (MessageBoxW(h, L"Effacer l'historique, les variables et les fonctions ?", L"Calc",
                                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    g.interp.reset();
                    g.items.clear();
                    SendMessageW(g.history, LB_RESETCONTENT, 0, 0);
                    refreshSide();
                    setStatus(L"Tout est effacé.", false);
                    InvalidateRect(h, nullptr, TRUE);
                }
                SetFocus(g.input);
            } else if (code == LBN_DBLCLK) {
                HWND list = reinterpret_cast<HWND>(lp);
                int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
                if (sel < 0) return 0;
                if (id == ID_HISTORY) {
                    SetWindowTextW(g.input, g.items[sel].expr.c_str());
                    int n = GetWindowTextLengthW(g.input);
                    SendMessageW(g.input, EM_SETSEL, n, n);
                    SetFocus(g.input);
                } else if (id == ID_VARS) {
                    std::wstring t = listText(list, sel);
                    insertText(t.substr(0, t.find(L' ')));
                } else if (id == ID_FUNCS && !g.interp.functions().empty()) {
                    std::wstring t = listText(list, sel);
                    insertText(t.substr(0, t.find(L'(') + 1));
                }
            }
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    g.scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0;
    ReleaseDC(nullptr, screen);

    g.ui = makeFont(15, FW_NORMAL, L"Segoe UI");
    g.small = makeFont(12, FW_SEMIBOLD, L"Segoe UI");
    g.keyFont = makeFont(20, FW_SEMIBOLD, L"Segoe UI");
    g.big = makeFont(24, FW_SEMIBOLD, L"Segoe UI");
    g.mono = makeFont(15, FW_NORMAL, L"Consolas");
    g.bgBrush = CreateSolidBrush(BG);
    g.panelBrush = CreateSolidBrush(PANEL);
    g.fieldBrush = CreateSolidBrush(FIELD);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = g.bgBrush;
    wc.lpszClassName = L"CalcFenetre";
    RegisterClassExW(&wc);

    HWND w = CreateWindowExW(0, wc.lpszClassName, L"Calc", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(940), S(640), nullptr, nullptr, inst, nullptr);
    BOOL dark = TRUE;   // barre de titre sombre (Windows 10 20H1+ / Windows 11, ignoré ailleurs)
    DwmSetWindowAttribute(w, 20, &dark, sizeof dark);
    ShowWindow(w, show);
    UpdateWindow(w);
    SetFocus(g.input);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
