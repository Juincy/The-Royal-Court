// The Royal Court | CK3 Mod Manager - native Windows GUI (Win32 + common controls)
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "core.hpp"

#include <windows.h>
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <exception>

using namespace rc;

// Steam app id of Crusader Kings III (used to find the install folder in Steam's library files)
static const char* CK3_APPID = "1158310";

static std::wstring W(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::string U(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring wenv(const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(name, buf, (DWORD)(sizeof buf / sizeof buf[0]));
    return (n > 0 && n < sizeof buf / sizeof buf[0]) ? std::wstring(buf, n) : L"";
}

enum {
    ID_COMBO = 100, ID_NEW, ID_DUP, ID_REN, ID_DEL, ID_EXPORT, ID_IMPORT, ID_PLAY, ID_LOG, ID_FILTER, ID_ALLON, ID_ALLOFF,
    ID_UP, ID_DOWN, ID_FOLDER, ID_LIST, ID_DIR, ID_BROWSE, ID_SAVEDIR, ID_STATUS, ID_COUNT, ID_L1, ID_L2, ID_L3, ID_GAMEVER, ID_RESYNC, ID_THEME, ID_CONFLICTS, ID_CF_PAIR, ID_CF_FILE, ID_CF_FILTER, ID_CF_RESCAN, ID_CF_LIST, ID_ADV, ID_ADV_DELSAVES, ID_ADV_OPENSAVES, ID_ADV_OPENLOGS, ID_SORT, ID_UNDOSORT,
    ID_TREE, ID_EXPAND, ID_COLLAPSE
};

static HINSTANCE g_inst;
static HWND hMain, hL1, hL2, hL3, hCombo, hNew, hDup, hRen, hDel, hExport, hImport, hPlay, hAdv, hLog, hFilter, hAllOn, hAllOff,
    hCount, hUp, hDown, hConflicts, hSort, hUndo, hResync, hTheme, hTip, hList, hStatus, hGameVer, hDir, hBrowse, hSaveDir;
static HFONT g_font;
static int g_dpi = 96;
static Settings g_settings;
static std::vector<Playset> g_playsets;
static std::vector<ModInfo> g_mods;
static std::map<std::string, ModInfo> g_info;
static std::string g_gameVer;           // installed CK3 version ("" = unknown)
static std::vector<std::vector<ModIssue>> g_issues;  // per playset position, from analyzePlayset
static std::vector<char> g_noteSev;     // per list row: 0 none, 1 warning, 2 problem
static int g_sortCol = -1;              // -1 = load order (default); otherwise the column the view is sorted by
static bool g_sortAsc = true;
static std::vector<char> g_verState;    // per list row: 0 unknown, 1 matches the game, 2 may be outdated
static std::vector<int> g_shown;  // list row -> index into the active playset's mods
static bool g_populating = false;
static bool g_drag = false;
static int g_dragFrom = -1;
static LVINSERTMARK g_mark;
static std::vector<std::string> g_docRoots;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static void say(const std::wstring& t) { SetWindowTextW(hStatus, t.c_str()); }
static Playset* active() {
    for (auto& p : g_playsets) if (p.name == g_settings.active) return &p;
    return nullptr;
}
static std::string effectiveDir() { return g_settings.ck3Dir.empty() ? autoFindCK3Dir(g_docRoots) : g_settings.ck3Dir; }
static std::wstring getText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s((size_t)n + 1, 0);
    GetWindowTextW(h, &s[0], n + 1);
    s.resize((size_t)n);
    return s;
}
static std::wstring wlower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static void setFont(HWND h) { SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE); }

static int g_cfView = 0;   // conflicts window: 0 by mod pair, 1 by file

// ---------- theme: "Midnight Court" (dark) and "Parchment" (light) ----------
struct Theme { COLORREF bg, banner, list, alt, border, text, muted, accent, accentText, gold, btn, btnHot, btnDown, sel, selText, ok, warn, bad; };
static const Theme DARK_T = {RGB(0x14,0x12,0x1C), RGB(0x1C,0x19,0x28), RGB(0x19,0x17,0x23), RGB(0x1F,0x1C,0x2C), RGB(0x3A,0x35,0x50), RGB(0xEE,0xE9,0xDC), RGB(0x9C,0x96,0xB0),
                             RGB(0xD4,0xAF,0x37), RGB(0x1A,0x14,0x00), RGB(0xD4,0xAF,0x37), RGB(0x2A,0x26,0x40), RGB(0x3A,0x34,0x5C), RGB(0x22,0x1F,0x35), RGB(0x3A,0x33,0x66), RGB(0xFF,0xFF,0xFF),
                             RGB(0x6F,0xCF,0x97), RGB(0xE8,0xA8,0x38), RGB(0xFF,0x6B,0x6B)};
static const Theme LIGHT_T = {RGB(0xF4,0xEF,0xE4), RGB(0xE9,0xE1,0xCF), RGB(0xFC,0xFA,0xF4), RGB(0xF6,0xF1,0xE6), RGB(0xCF,0xC4,0xA8), RGB(0x2A,0x24,0x33), RGB(0x77,0x6E,0x88),
                              RGB(0x4B,0x2E,0x83), RGB(0xFF,0xFF,0xFF), RGB(0xB0,0x84,0x10), RGB(0xE2,0xD9,0xC4), RGB(0xD6,0xCB,0xB0), RGB(0xCB,0xBE,0xA0), RGB(0xDD,0xD1,0xF0), RGB(0x1E,0x16,0x30),
                              RGB(0x2E,0x7D,0x4F), RGB(0xB0,0x62,0x06), RGB(0xB3,0x26,0x1E)};
static bool g_dark = true;
static const Theme& T() { return g_dark ? DARK_T : LIGHT_T; }
static HBRUSH g_brBg, g_brBanner, g_brList;
static HFONT g_fontSym, g_fontCrown, g_fontTitle, g_fontSub, g_fontBold;
static HWND g_hot = nullptr;
static int g_bannerH = 0;
static RECT g_listFrame = {0, 0, 0, 0};
static std::map<int, std::wstring> g_glyph;  // owner-drawn button id -> symbol shown before its label

static COLORREF mix(COLORREF a, COLORREF b, int pctB) {
    return RGB((GetRValue(a) * (100 - pctB) + GetRValue(b) * pctB) / 100, (GetGValue(a) * (100 - pctB) + GetGValue(b) * pctB) / 100, (GetBValue(a) * (100 - pctB) + GetBValue(b) * pctB) / 100);
}
static void rebuildBrushes() {
    for (HBRUSH* b : {&g_brBg, &g_brBanner, &g_brList}) if (*b) { DeleteObject(*b); *b = nullptr; }
    g_brBg = CreateSolidBrush(T().bg);
    g_brBanner = CreateSolidBrush(T().banner);
    g_brList = CreateSolidBrush(T().list);
}
static bool systemPrefersDark() {
    DWORD v = 1, sz = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS) return true;
    return v == 0;
}
static void themeFrame(HWND h) {
    BOOL dark = g_dark;
    if (FAILED(DwmSetWindowAttribute(h, 20, &dark, sizeof dark))) DwmSetWindowAttribute(h, 19, &dark, sizeof dark);  // dark title bar
    COLORREF cap = T().banner, txt = T().text;
    DwmSetWindowAttribute(h, 35, &cap, sizeof cap);  // caption colour (Windows 11)
    DwmSetWindowAttribute(h, 36, &txt, sizeof txt);
}

// Anti-aliased drawing (GDI+) for the rounded buttons and the theme switch, so edges and icons stay crisp.
static ULONG_PTR g_gdip = 0;
static Gdiplus::Color gc(COLORREF c, int a = 255) { return Gdiplus::Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static void roundedPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    if (d > h) d = h;
    if (d > w) d = w;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void drawSun(Gdiplus::Graphics& g, float cx, float cy, float size, COLORREF c) {
    float r0 = size * 0.15f;
    Gdiplus::SolidBrush br(gc(c));
    g.FillEllipse(&br, cx - r0, cy - r0, r0 * 2, r0 * 2);
    Gdiplus::Pen pen(gc(c), size * 0.075f);
    pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 3.14159265f / 4.0f;
        g.DrawLine(&pen, cx + cosf(a) * r0 * 1.9f, cy + sinf(a) * r0 * 1.9f, cx + cosf(a) * r0 * 2.6f, cy + sinf(a) * r0 * 2.6f);
    }
}
static void drawMoon(Gdiplus::Graphics& g, float cx, float cy, float size, COLORREF c, COLORREF behind) {
    float r = size * 0.30f;
    Gdiplus::SolidBrush br(gc(c)), cut(gc(behind));
    g.FillEllipse(&br, cx - r, cy - r, r * 2, r * 2);
    float r2 = r * 0.82f;
    g.FillEllipse(&cut, cx - r2 + r * 0.62f, cy - r2 - r * 0.38f, r2 * 2, r2 * 2);
}

static void drawToggle(const DRAWITEMSTRUCT* d, COLORREF around) {
    const Theme& t = T();
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    HBRUSH bg = CreateSolidBrush(around); FillRect(dc, &r, bg); DeleteObject(bg);
    float w = (float)(r.right - r.left), h = (float)(r.bottom - r.top);
    float x = (float)r.left, y = (float)r.top;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    COLORREF trackC = g_dark ? RGB(0x2A,0x26,0x40) : RGB(0xDC,0xD2,0xBA);
    Gdiplus::GraphicsPath track;
    roundedPath(track, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, h / 2);
    Gdiplus::SolidBrush tb(gc(trackC));
    g.FillPath(&tb, &track);
    Gdiplus::Pen bp(gc(g_hot == d->hwndItem ? t.gold : t.border), 1.0f);
    g.DrawPath(&bp, &track);
    float k = h - 6.0f * g_dpi / 96.0f;
    float pad = (h - k) / 2;
    float kx = g_dark ? x + w - pad - k : x + pad;
    COLORREF knobC = g_dark ? t.gold : t.accent;
    Gdiplus::SolidBrush kb(gc(knobC));
    g.FillEllipse(&kb, kx, y + pad, k, k);
    float sunX = x + pad + k / 2, moonX = x + w - pad - k / 2, cy = y + h / 2;
    if (g_dark) { drawSun(g, sunX, cy, k, t.muted); drawMoon(g, moonX, cy, k, RGB(0x1A,0x14,0x00), knobC); }
    else { drawSun(g, sunX, cy, k, RGB(0xFF,0xFF,0xFF)); drawMoon(g, moonX, cy, k, t.muted, trackC); }
}

static std::wstring ctlText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s((size_t)n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize((size_t)n);
    return s;
}

static void drawButton(const DRAWITEMSTRUCT* d) {
    const Theme& t = T();
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    int id = (int)d->CtlID;
    HWND parent = GetParent(d->hwndItem);
    RECT wr; GetWindowRect(d->hwndItem, &wr);
    POINT p{wr.left, wr.top}; ScreenToClient(parent, &p);
    bool inBanner = parent == hMain && p.y < g_bannerH;
    COLORREF around = inBanner ? t.banner : t.bg;
    if (id == ID_THEME) { drawToggle(d, around); return; }
    HBRUSH ab = CreateSolidBrush(around); FillRect(dc, &r, ab); DeleteObject(ab);
    bool dis = (d->itemState & ODS_DISABLED) != 0, down = (d->itemState & ODS_SELECTED) != 0, hot = g_hot == d->hwndItem;
    bool primary = id == ID_PLAY || (id == ID_CF_PAIR && g_cfView == 0) || (id == ID_CF_FILE && g_cfView == 1);
    COLORREF fill = primary ? (down ? mix(t.accent, RGB(0,0,0), 20) : hot ? mix(t.accent, RGB(255,255,255), 18) : t.accent) : (down ? t.btnDown : hot ? t.btnHot : t.btn);
    COLORREF edge = primary ? mix(t.accent, RGB(0,0,0), 25) : (hot ? t.gold : t.border);
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath path;
        roundedPath(path, (float)r.left + 0.5f, (float)r.top + 0.5f, (float)(r.right - r.left) - 1.0f, (float)(r.bottom - r.top) - 1.0f, (float)S(5));
        Gdiplus::SolidBrush fb(gc(fill));
        g.FillPath(&fb, &path);
        Gdiplus::Pen ep(gc(edge), 1.0f);
        g.DrawPath(&ep, &path);
    }
    std::wstring label = ctlText(d->hwndItem);
    auto g = g_glyph.find(id);
    std::wstring glyph = g == g_glyph.end() ? L"" : g->second;
    COLORREF fg = dis ? mix(t.muted, fill, 40) : primary ? t.accentText : t.text;
    COLORREF gfg = dis ? fg : primary ? t.accentText : t.gold;
    SIZE gs{0, 0}, ls{0, 0};
    HGDIOBJ of = SelectObject(dc, g_fontSym);
    if (!glyph.empty()) GetTextExtentPoint32W(dc, glyph.c_str(), (int)glyph.size(), &gs);
    SelectObject(dc, primary ? g_fontBold : g_font);
    if (!label.empty()) GetTextExtentPoint32W(dc, label.c_str(), (int)label.size(), &ls);
    int gap = (!glyph.empty() && !label.empty()) ? S(7) : 0;
    int x = r.left + ((r.right - r.left) - (gs.cx + gap + ls.cx)) / 2;
    int cy = (r.top + r.bottom) / 2;
    SetBkMode(dc, TRANSPARENT);
    if (!glyph.empty()) {
        SelectObject(dc, g_fontSym); SetTextColor(dc, gfg);
        RECT gr = {x, cy - S(14), x + gs.cx, cy + S(14)};
        DrawTextW(dc, glyph.c_str(), -1, &gr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    }
    if (!label.empty()) {
        SelectObject(dc, primary ? g_fontBold : g_font); SetTextColor(dc, fg);
        RECT lr = {x + gs.cx + gap, cy - S(14), x + gs.cx + gap + ls.cx + S(2), cy + S(14)};
        DrawTextW(dc, label.c_str(), -1, &lr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    }
    SelectObject(dc, of);
    if ((d->itemState & ODS_FOCUS) && !(d->itemState & ODS_NOFOCUSRECT)) {
        RECT fr = r; InflateRect(&fr, -S(4), -S(4));
        HPEN fp = CreatePen(PS_DOT, 1, mix(fg, fill, 35)); HGDIOBJ o2 = SelectObject(dc, fp); HGDIOBJ o3 = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, fr.left, fr.top, fr.right, fr.bottom);
        SelectObject(dc, o2); SelectObject(dc, o3); DeleteObject(fp);
    }
}

static LRESULT CALLBACK BtnSub(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_MOUSEMOVE && g_hot != h) {
        HWND old = g_hot; g_hot = h;
        if (old) InvalidateRect(old, nullptr, FALSE);
        InvalidateRect(h, nullptr, FALSE);
        TRACKMOUSEEVENT te{sizeof te, TME_LEAVE, h, 0};
        TrackMouseEvent(&te);
    } else if (m == WM_MOUSELEAVE) {
        if (g_hot == h) { g_hot = nullptr; InvalidateRect(h, nullptr, FALSE); }
    } else if (m == WM_NCDESTROY) {
        if (g_hot == h) g_hot = nullptr;
        RemoveWindowSubclass(h, BtnSub, 1);
    }
    return DefSubclassProc(h, m, w, l);
}

static HWND mkBtn(HWND parent, const wchar_t* glyph, const wchar_t* label, int id) {
    HWND h = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_inst, nullptr);
    g_glyph[id] = glyph;
    setFont(h);
    SetWindowSubclass(h, BtnSub, 1, 0);
    return h;
}

// Colours for static text and edit boxes, shared by the main window, the changelog window and the name prompt.
static bool themeCtlColor(UINT m, WPARAM w, LPARAM l, LRESULT& out) {
    const Theme& t = T();
    HDC dc = (HDC)w;
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        SetBkColor(dc, t.bg);
        SetTextColor(dc, id == ID_L2 ? t.gold : (id == ID_L1 || id == ID_L3 || id == ID_STATUS || id == ID_GAMEVER || id == ID_COUNT) ? t.muted : t.text);
        out = (LRESULT)g_brBg;
        return true;
    }
    if (m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX) {
        SetBkColor(dc, t.list); SetTextColor(dc, t.text);
        out = (LRESULT)g_brList;
        return true;
    }
    return false;
}

static void paintBackdrop(HDC dc, const RECT& rc, bool banner) {
    const Theme& t = T();
    FillRect(dc, &rc, g_brBg);
    if (!banner) return;
    RECT b = {0, 0, rc.right, g_bannerH};
    FillRect(dc, &b, g_brBanner);
    HBRUSH gold = CreateSolidBrush(t.gold);
    RECT ln = {0, g_bannerH - S(2), rc.right, g_bannerH};
    FillRect(dc, &ln, gold);
    DeleteObject(gold);
    if (g_listFrame.right > g_listFrame.left) {
        HBRUSH fb = CreateSolidBrush(t.border);
        FrameRect(dc, &g_listFrame, fb);
        DeleteObject(fb);
    }
    SetBkMode(dc, TRANSPARENT);
    int cy = (g_bannerH - S(2)) / 2;
    HGDIOBJ of = SelectObject(dc, g_fontTitle);
    {
        using namespace Gdiplus;
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        float w = (float)S(40), h = (float)S(30);
        float x0 = (float)S(10), y0 = (float)cy - h / 2;
        auto P = [&](float u, float v) { return PointF(x0 + u * w, y0 + v * h); };
        PointF pts[] = { P(0.04f, 0.86f), P(0.0f, 0.18f), P(0.26f, 0.50f), P(0.50f, 0.0f), P(0.74f, 0.50f), P(1.0f, 0.18f), P(0.96f, 0.86f) };
        SolidBrush gb(Color(255, GetRValue(t.gold), GetGValue(t.gold), GetBValue(t.gold)));
        g.FillPolygon(&gb, pts, 7);
        g.FillRectangle(&gb, x0 + 0.04f * w, y0 + 0.86f * h, 0.92f * w, 0.14f * h);
        SolidBrush jb(Color(255, 176, 40, 60));
        float r = 0.07f * w;
        g.FillEllipse(&jb, x0 + 0.50f * w - r, y0 + 0.62f * h - r, 2 * r, 2 * r);
        g.FillEllipse(&jb, x0 + 0.25f * w - r * 0.8f, y0 + 0.70f * h - r * 0.8f, 1.6f * r, 1.6f * r);
        g.FillEllipse(&jb, x0 + 0.75f * w - r * 0.8f, y0 + 0.70f * h - r * 0.8f, 1.6f * r, 1.6f * r);
    }
    SelectObject(dc, g_fontTitle);
    SetTextColor(dc, t.text);
    const wchar_t* title = L"The Royal Court";
    SIZE ts{0, 0}; GetTextExtentPoint32W(dc, title, (int)wcslen(title), &ts);
    RECT tr = {S(58), cy - ts.cy / 2 - S(1), S(58) + ts.cx + S(4), cy + ts.cy / 2 + S(2)};
    DrawTextW(dc, title, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, g_fontSub);
    SetTextColor(dc, t.muted);
    SetTextCharacterExtra(dc, S(2) / 1);
    std::wstring sub = L"CK3 MOD MANAGER  ·  v" + W(VERSION);
    RECT sr = {tr.right + S(10), cy - S(6) + S(5), tr.right + S(400), cy + S(16)};
    DrawTextW(dc, sub.c_str(), -1, &sr, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
    SetTextCharacterExtra(dc, 0);
    SelectObject(dc, of);
}

static LRESULT drawHeader(NMCUSTOMDRAW* cd) {
    const Theme& t = T();
    HWND hdr = cd->hdr.hwndFrom;
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        RECT r; GetClientRect(hdr, &r);
        HBRUSH b = CreateSolidBrush(t.banner); FillRect(cd->hdc, &r, b); DeleteObject(b);
        return CDRF_NOTIFYITEMDRAW;
    }
    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        RECT r = cd->rc;
        int i = (int)cd->dwItemSpec;
        wchar_t buf[64] = L"";
        HDITEMW hi{};
        hi.mask = HDI_TEXT; hi.pszText = buf; hi.cchTextMax = 63;
        SendMessageW(hdr, HDM_GETITEMW, (WPARAM)i, (LPARAM)&hi);
        bool sorted = hList && hdr == ListView_GetHeader(hList) && i == g_sortCol;
        HDC dc = cd->hdc;
        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ of = SelectObject(dc, g_fontBold);
        SetTextColor(dc, sorted ? t.gold : t.muted);
        RECT tr = r; tr.left += S(8); tr.right -= sorted ? S(22) : S(6);
        DrawTextW(dc, buf, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (sorted) {
            SelectObject(dc, g_fontSym);
            SetTextColor(dc, t.gold);
            RECT ar = {r.right - S(22), r.top, r.right - S(6), r.bottom};
            DrawTextW(dc, g_sortAsc ? L"▲" : L"▼", -1, &ar, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        }
        HPEN pen = CreatePen(PS_SOLID, 1, t.border);
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, r.right - 1, r.top + S(5), nullptr); LineTo(dc, r.right - 1, r.bottom - S(5));
        MoveToEx(dc, r.left, r.bottom - 1, nullptr); LineTo(dc, r.right, r.bottom - 1);
        SelectObject(dc, op); DeleteObject(pen);
        SelectObject(dc, of);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

// The list view does not pass its header's custom-draw messages on to its parent, so it is subclassed to paint the header itself.
static LRESULT CALLBACK ListSub(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)l;
        if (nh->code == NM_CUSTOMDRAW && nh->hwndFrom == ListView_GetHeader(h)) return drawHeader((NMCUSTOMDRAW*)l);
    } else if (m == WM_NCDESTROY) RemoveWindowSubclass(h, ListSub, 2);
    return DefSubclassProc(h, m, w, l);
}

static void applyTheme() {
    const Theme& t = T();
    rebuildBrushes();
    themeFrame(hMain);
    const wchar_t* ctl = g_dark ? L"DarkMode_CFD" : nullptr;
    const wchar_t* exp = g_dark ? L"DarkMode_Explorer" : L"Explorer";
    for (HWND h : {hCombo, hFilter, hDir}) if (h) SetWindowTheme(h, ctl, nullptr);
    if (hList) {
        SetWindowTheme(hList, exp, nullptr);
        ListView_SetBkColor(hList, t.list);
        ListView_SetTextBkColor(hList, t.list);
        ListView_SetTextColor(hList, t.text);
    }
    RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    if (HWND lg = FindWindowW(L"RCLog", nullptr)) SendMessageW(lg, WM_APP + 1, 0, 0);
    if (HWND cf = FindWindowW(L"RCConf", nullptr)) SendMessageW(cf, WM_APP + 1, 0, 0);
}


// Every change is written straight to the playset's file (the launcher-format .json is the only storage).
static void saveActive() {
    Playset* ps = active();
    if (ps && !savePlayset(*ps, g_info)) say(L"Warning: could not save the playset file.");
}
static void saveSettingsNow() {
    if (!saveSettings(g_settings)) say(L"Warning: could not save settings.");
}
static void sortPlaysets() {
    std::sort(g_playsets.begin(), g_playsets.end(), [](const Playset& a, const Playset& b) { return lower(a.name) < lower(b.name); });
}

// ---------- conflict scan: file lists of the enabled mods, read in the background ----------
static std::map<std::string, ModFiles> g_fileIndex;   // mod id -> files (kept for the session; rebuilt when a mod's folder changes)
static ConflictReport g_conf;
static std::string g_confSig;                         // which playset state g_conf was computed for
static bool g_scanning = false;
static int g_scanDone = 0, g_scanTotal = 0;
static HWND hConf = nullptr;                          // conflicts window, if open
static void confRefresh();

struct ScanItem { std::string id, dir, fp; };
struct ScanJob {
    std::vector<ScanItem> items;
    std::vector<ModFiles> out;
    std::atomic<bool> cancel{false};
    HWND notify = nullptr;
};
static ScanJob* g_job = nullptr;
static HANDLE g_jobThread = nullptr;

static DWORD WINAPI scanThread(LPVOID p) {
    ScanJob* j = (ScanJob*)p;
    j->out.resize(j->items.size());
    for (size_t i = 0; i < j->items.size() && !j->cancel.load(); i++) {
        j->out[i] = indexModFiles(j->items[i].dir, &j->cancel);
        j->out[i].fingerprint = j->items[i].fp;
        PostMessageW(j->notify, WM_APP + 2, (WPARAM)(i + 1), (LPARAM)j->items.size());
    }
    PostMessageW(j->notify, WM_APP + 3, (WPARAM)j, 0);
    return 0;
}

static std::string playsetSignature(const Playset& ps) {
    std::string s = ps.name;
    for (auto& m : ps.mods) if (m.enabled) { s += '\n'; s += m.id; }
    return s;
}

// Rebuilds the conflict report from the cached file lists when the enabled mods or their order changed (no disk access).
static void ensureConflicts() {
    Playset* ps = active();
    if (!ps) return;
    std::string sig = playsetSignature(*ps);
    if (sig == g_confSig && (g_conf.valid || g_scanning)) return;
    bool ready = true;
    for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        auto it = g_info.find(m.id);
        if (it == g_info.end() || it->second.contentState != 1) continue;
        auto f = g_fileIndex.find(m.id);
        if (f == g_fileIndex.end() || !f->second.complete) { ready = false; break; }
    }
    g_confSig = sig;
    g_conf = ready ? findConflicts(*ps, g_info, g_fileIndex) : ConflictReport();
    confRefresh();
}

// Starts a background scan for enabled mods that have no file list yet (or whose folder changed, when verify is set).
static void maybeScan(bool verify) {
    if (g_job) return;
    Playset* ps = active();
    if (!ps) return;
    auto* job = new ScanJob();
    for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        auto it = g_info.find(m.id);
        if (it == g_info.end() || it->second.contentState != 1) continue;
        std::string fp = modFingerprint(it->second);
        auto f = g_fileIndex.find(m.id);
        if (f != g_fileIndex.end() && f->second.complete && (!verify || f->second.fingerprint == fp)) continue;
        job->items.push_back({m.id, it->second.contentDir, fp});
    }
    if (job->items.empty()) { delete job; ensureConflicts(); return; }
    job->notify = hMain;
    g_job = job;
    g_scanning = true; g_scanDone = 0; g_scanTotal = (int)job->items.size();
    g_jobThread = CreateThread(nullptr, 0, scanThread, job, 0, nullptr);
    if (!g_jobThread) { g_scanning = false; g_job = nullptr; delete job; }
    confRefresh();
}

static void stopScan() {
    if (!g_job) return;
    g_job->cancel = true;
    if (g_jobThread) { WaitForSingleObject(g_jobThread, 5000); CloseHandle(g_jobThread); g_jobThread = nullptr; }
}

// ---------- list ----------
static void updateCount() {
    Playset* ps = active();
    if (!ps) return;
    int on = 0, old = 0;
    for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        on++;
        auto it = g_info.find(m.id);
        if (it != g_info.end() && matchGameVersion(it->second.supported, g_gameVer) == VerMatch::Mismatch) old++;
    }
    int bad = 0;
    for (size_t i = 0; i < ps->mods.size() && i < g_issues.size(); i++) if (ps->mods[i].enabled && issueSeverity(g_issues[i]) == 2) bad++;
    std::wstring t = std::to_wstring(on) + L" of " + std::to_wstring(ps->mods.size()) + L" enabled";
    if (bad > 0) t += L"  |  " + std::to_wstring(bad) + L" with problems";
    if (g_scanning) t += L"  |  scanning mod files " + std::to_wstring(g_scanDone) + L"/" + std::to_wstring(g_scanTotal);
    else if (g_conf.valid && !g_conf.files.empty()) t += L"  |  " + std::to_wstring(g_conf.files.size()) + L" file conflicts";
    if (old > 0) t += L"  |  " + std::to_wstring(old) + L" may be outdated";
    SetWindowTextW(hCount, t.c_str());
}

static void resizeCols();
static std::wstring noteText(int i, char& sev) {
    Playset* ps = active();
    const auto& iss = g_issues[(size_t)i];
    sev = (char)issueSeverity(iss);
    std::string sum = issueSummary(iss);
    if (!sum.empty()) return W(sum);
    if (ps && ps->mods[(size_t)i].enabled && g_info.count(ps->mods[(size_t)i].id)) { sev = 3; return L"\u2713"; }  // 3 = checked, all fine
    return L"";
}

// ---------- auto sort state ----------
static std::vector<std::string> g_undoIds;     // load order (mod ids) before the last Auto Sort
static std::string g_undoPlayset;
static int g_ctxIdx = -1;                      // playset position of the row the context menu was opened on
static const int ID_CTX_LOCK = 700, ID_CTX_CAT = 710;   // ID_CTX_CAT + category; ID_CTX_CAT + CAT_COUNT = automatic

static std::set<std::string>& lockedIds() { static std::set<std::string> none; Playset* ps = active(); return ps ? g_settings.locks[ps->name] : none; }
static int catOfMod(const std::string& id, bool* overridden = nullptr) {
    auto ov = g_settings.cats.find(id);
    if (overridden) *overridden = ov != g_settings.cats.end();
    if (ov != g_settings.cats.end()) return ov->second;
    auto it = g_info.find(id);
    return it == g_info.end() ? (int)CAT_CONTENT : guessCategory(it->second).cat;
}
static void updateUndoBtn() {
    Playset* ps = active();
    bool on = ps && !g_undoIds.empty() && g_undoPlayset == ps->name;
    if (hUndo) { EnableWindow(hUndo, on ? TRUE : FALSE); InvalidateRect(hUndo, nullptr, TRUE); }
}

static void populate() {
    Playset* ps = active();
    if (!ps) return;
    g_populating = true;
    SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hList);
    g_shown.clear();
    g_verState.clear();
    ensureConflicts();
    g_issues = analyzePlayset(*ps, g_info, g_gameVer);
    if (g_conf.valid) addConflictIssues(g_issues, g_conf, *ps, g_info);
    g_noteSev.clear();
    std::wstring f = wlower(getText(hFilter));
    auto shownName = [&](int i) {
        const ModRef& m = ps->mods[(size_t)i];
        auto it = g_info.find(m.id);
        return it != g_info.end() ? W(it->second.name) : W(m.name.empty() ? m.id : m.name) + L"  (not installed)";
    };
    std::vector<int> order;
    for (int i = 0; i < (int)ps->mods.size(); i++) {
        if (!f.empty() && wlower(shownName(i)).find(f) == std::wstring::npos) continue;
        order.push_back(i);
    }
    if (g_sortCol >= 1) {
        // View-only sort: the load order in the playset is never touched. Empty cells always go last.
        auto key = [&](int i) -> std::string {
            const ModRef& m = ps->mods[(size_t)i];
            auto it = g_info.find(m.id);
            bool inst = it != g_info.end();
            switch (g_sortCol) {
                case 1: return inst ? it->second.name : (m.name.empty() ? m.id : m.name);
                case 2: return inst ? it->second.version : "";
                case 3: return inst ? it->second.supported : "";
                case 4: return inst ? it->second.source : "";
                case 5: return inst ? std::string(catName(catOfMod(m.id))) : "";
                default: return issueSummary(g_issues[(size_t)i]);
            }
        };
        std::vector<std::pair<std::string, int>> keyed;
        for (int i : order) keyed.push_back({key(i), i});
        bool asc = g_sortAsc;
        int col = g_sortCol;
        std::stable_sort(keyed.begin(), keyed.end(), [&](const std::pair<std::string, int>& x, const std::pair<std::string, int>& y) {
            if (x.first.empty() != y.first.empty()) return y.first.empty();
            if (col == 6) {
                int sx = issueSeverity(g_issues[(size_t)x.second]), sy = issueSeverity(g_issues[(size_t)y.second]);
                if (sx != sy) return asc ? sx > sy : sx < sy;   // worst first when ascending
            }
            int c = naturalCompare(x.first, y.first);
            return asc ? c < 0 : c > 0;
        });
        order.clear();
        for (auto& k : keyed) order.push_back(k.second);
    }
    for (int i : order) {
        const ModRef& m = ps->mods[(size_t)i];
        auto it = g_info.find(m.id);
        bool inst = it != g_info.end();
        std::wstring name = shownName(i);
        int row = (int)g_shown.size();
        std::wstring num = std::to_wstring(i + 1);
        if (g_settings.locks.count(ps->name) && g_settings.locks[ps->name].count(m.id)) num += L" \U0001F512";
        LVITEMW li{};
        li.mask = LVIF_TEXT;
        li.iItem = row;
        li.pszText = &num[0];
        ListView_InsertItem(hList, &li);
        ListView_SetItemText(hList, row, 1, (LPWSTR)name.c_str());
        std::wstring ver = inst ? W(it->second.version) : L"", src = inst ? W(it->second.source) : L"";
        ListView_SetItemText(hList, row, 2, (LPWSTR)ver.c_str());
        std::wstring gv = inst ? W(it->second.supported) : L"";
        VerMatch vm = inst ? matchGameVersion(it->second.supported, g_gameVer) : VerMatch::Unknown;
        g_verState.push_back(vm == VerMatch::Match ? 1 : vm == VerMatch::Mismatch ? 2 : 0);
        ListView_SetItemText(hList, row, 3, (LPWSTR)gv.c_str());
        ListView_SetItemText(hList, row, 4, (LPWSTR)src.c_str());
        char nsev = 0;
        std::wstring note = noteText(i, nsev);
        bool ovr = false;
        std::wstring ty = inst ? W(catName(catOfMod(m.id, &ovr))) + (ovr ? L" *" : L"") : L"";
        ListView_SetItemText(hList, row, 5, (LPWSTR)ty.c_str());
        ListView_SetItemText(hList, row, 6, (LPWSTR)note.c_str());
        g_noteSev.push_back(nsev);
        ListView_SetCheckState(hList, row, m.enabled ? TRUE : FALSE);
        g_shown.push_back(i);
    }
    SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
    g_populating = false;
    resizeCols();
    updateCount();
    updateUndoBtn();
}

static void fillCombo() {
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    int sel = 0, i = 0;
    for (auto& p : g_playsets) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)W(p.name).c_str());
        if (p.name == g_settings.active) sel = i;
        i++;
    }
    SendMessageW(hCombo, CB_SETCURSEL, (WPARAM)sel, 0);
}

static std::string findGameExeInSteam();
static void refreshGameVersion(const std::string& exe) {
    g_gameVer = readGameVersion(exe);
    SetWindowTextW(hGameVer, g_gameVer.empty() ? L"CK3 version: unknown" : (L"CK3 version: " + W(g_gameVer)).c_str());
}

static long long modDirStamp() {
    std::string dir = effectiveDir();
    if (dir.empty()) return 0;
    std::error_code ec;
    auto t = fs::last_write_time(P(dir) / "mod", ec);
    return ec ? 0 : (long long)t.time_since_epoch().count();
}
static long long g_modStamp = 0;

static void reload() {
    std::error_code gec;
    refreshGameVersion((!g_settings.gameExe.empty() && fs::exists(P(g_settings.gameExe), gec)) ? g_settings.gameExe : findGameExeInSteam());
    std::string dir = effectiveDir();
    g_mods = scanMods(dir);
    g_info = infoMap(g_mods);
    int migrated = migrateLegacyStore(g_mods, g_settings);
    g_playsets = loadPlaysets(g_mods);
    if (g_playsets.empty()) {
        Playset d;
        d.name = "Default";
        syncPlayset(d, g_mods);
        savePlayset(d, g_info);
        g_playsets.push_back(d);
    }
    if (!active()) g_settings.active = g_playsets[0].name;
    saveSettingsNow();
    SetWindowTextW(hDir, W(dir).c_str());
    g_modStamp = modDirStamp();
    fillCombo();
    populate();
    PostMessageW(hMain, WM_APP + 4, 1, 0);
    if (migrated > 0) say(L"Your " + std::to_wstring(migrated) + L" playset(s) from the older version were converted to launcher playset files.");
    else if (dir.empty()) say(L"CK3 folder not found. Enter it below (the folder that contains \"mod\"), then press Save folder.");
    else if (g_mods.empty()) say(L"No mods found in " + W(dir) + L"\\mod");
}

// After a single checkbox change: the problems of every mod can change (dependencies), so redo the Notes column.
static void refreshNotes() {
    Playset* ps = active();
    if (!ps) return;
    ensureConflicts();
    g_issues = analyzePlayset(*ps, g_info, g_gameVer);
    if (g_conf.valid) addConflictIssues(g_issues, g_conf, *ps, g_info);
    g_populating = true;
    for (size_t r = 0; r < g_shown.size(); r++) {
        char nsev = 0;
        std::wstring note = noteText(g_shown[r], nsev);
        ListView_SetItemText(hList, (int)r, 6, (LPWSTR)note.c_str());
        if (r < g_noteSev.size()) g_noteSev[r] = nsev;
    }
    g_populating = false;
    InvalidateRect(hList, nullptr, FALSE);
    updateCount();
}

static void updateSortArrows() {
    HWND hdr = ListView_GetHeader(hList);
    for (int i = 0; i < 7; i++) {
        HDITEMW hi{};
        hi.mask = HDI_FORMAT;
        if (!Header_GetItem(hdr, i, &hi)) continue;
        hi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == g_sortCol) hi.fmt |= g_sortAsc ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(hdr, i, &hi);
    }
}

// Load order can only be changed in the playset's own order: no filter, no column sort.
static bool filterActive();
static bool orderLocked() { return filterActive() || g_sortCol >= 1; }

static void showDetails(int row) {
    Playset* ps = active();
    if (!ps || row < 0 || row >= (int)g_shown.size()) return;
    int idx = g_shown[(size_t)row];
    const ModRef& m = ps->mods[(size_t)idx];
    auto it = g_info.find(m.id);
    std::string t;
    if (it == g_info.end()) t = (m.name.empty() ? m.id : m.name) + "\nNot installed (" + m.id + ")\n";
    else {
        const ModInfo& mi = it->second;
        auto join = [](const std::vector<std::string>& v) { std::string o; for (size_t i = 0; i < v.size(); i++) o += (i ? ", " : "") + v[i]; return o.empty() ? std::string("-") : o; };
        t = mi.name + "\n\nPosition in playset: " + std::to_string(idx + 1) + (m.enabled ? " (enabled)" : " (disabled)") +
            "\nDescriptor: " + mi.id + "\nSource: " + mi.source + "\nMod version: " + (mi.version.empty() ? "-" : mi.version) +
            "\nGame version: " + (mi.supported.empty() ? "-" : mi.supported) +
            "\nFiles: " + (mi.path.empty() ? (mi.archive.empty() ? std::string("-") : "archive " + mi.archive) : mi.contentDir + (mi.contentState == 2 ? "  (NOT FOUND)" : "")) +
            "\nType: " + catName(catOfMod(mi.id)) + (g_settings.cats.count(mi.id) ? " (set by you)" : " (" + guessCategory(mi).why + ")") +
            (lockedIds().count(mi.id) ? "\nPosition: locked (Auto Sort will not move it)" : "") +
            "\nDependencies: " + join(mi.deps) + "\nReplaces vanilla folders: " + join(mi.replacePaths) + "\nTags: " + join(mi.tags) + "\n";
    }
    const auto& iss = g_issues[(size_t)idx];
    if (!iss.empty()) {
        t += "\nChecks:\n";
        for (auto& i : iss) t += std::string(i.sev == 2 ? "  [problem] " : i.sev == 1 ? "  [warning] " : "  [info] ") + i.text + "\n";
    } else if (m.enabled) t += "\nChecks: no problems found.\n";
    MessageBoxW(hMain, W(t).c_str(), L"Mod details", MB_ICONINFORMATION);
}

// Rescans the mod folder (e.g. after subscribing to a mod while the app is open). New mods are added to the end of every playset, disabled.
static bool g_resyncing = false;
static void resync(bool automatic) {
    if (g_resyncing) return;
    g_resyncing = true;
    std::set<std::string> before;
    for (auto& m : g_mods) before.insert(m.id);
    reload();
    int added = 0, gone = 0;
    std::set<std::string> after;
    for (auto& m : g_mods) { after.insert(m.id); if (!before.count(m.id)) added++; }
    for (auto& b : before) if (!after.count(b)) gone++;
    std::wstring msg = std::wstring(automatic ? L"The mod folder changed, list refreshed: " : L"Rescanned: ") + std::to_wstring(g_mods.size()) + L" mods installed";
    if (added) msg += L", " + std::to_wstring(added) + L" new (added at the end of your playsets, disabled)";
    if (gone) msg += L", " + std::to_wstring(gone) + L" removed";
    say(msg + L".");
    g_resyncing = false;
}

static void resizeCols() {
    RECT r; GetClientRect(hList, &r);
    int fixed = S(64) + S(100) + S(110) + S(90) + S(90), notes = S(280);
    int w = r.right - fixed - notes - GetSystemMetrics(SM_CXVSCROLL);
    if (w < S(150)) w = S(150);
    ListView_SetColumnWidth(hList, 1, w);
    int last = r.right - fixed - w;                    // the last column takes what is left, so the header has no empty strip
    ListView_SetColumnWidth(hList, 6, last < S(120) ? S(120) : last);
}

static void selectRow(int row) {
    ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
    ListView_SetItemState(hList, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(hList, row, FALSE);
}

static bool filterActive() { return !getText(hFilter).empty(); }

// Move the mod at position `from` so it ends up at position `to` (both indices into the full playset).
static void moveMod(int from, int to) {
    Playset* ps = active();
    if (!ps || from < 0 || to < 0 || from >= (int)ps->mods.size() || to >= (int)ps->mods.size() || from == to) return;
    ModRef m = ps->mods[(size_t)from];
    ps->mods.erase(ps->mods.begin() + from);
    ps->mods.insert(ps->mods.begin() + to, m);
    saveActive();
    populate();
    selectRow(to);
}

// ---------- small modal text prompt ----------
struct AskCtx { HWND edit = nullptr; std::wstring* out = nullptr; bool done = false, ok = false; };
static LRESULT CALLBACK AskProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    AskCtx* c = (AskCtx*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); themeFrame(h); return 0;
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT*)l); return TRUE;
        case WM_COMMAND:
            if (c && LOWORD(w) == IDOK) { *c->out = getText(c->edit); c->ok = true; c->done = true; }
            else if (c && LOWORD(w) == IDCANCEL) c->done = true;
            return 0;
        case WM_CLOSE: if (c) c->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static bool askText(const wchar_t* title, const wchar_t* prompt, std::wstring& value) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = AskProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCAsk";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc); reg = true;
    }
    AskCtx ctx; ctx.out = &value;
    RECT pr; GetWindowRect(hMain, &pr);
    int w = S(380), h = S(140);
    int x = pr.left + ((pr.right - pr.left) - w) / 2, y = pr.top + ((pr.bottom - pr.top) - h) / 2;
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RCAsk", title, WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, h, hMain, nullptr, g_inst, &ctx);
    HWND st = CreateWindowW(L"STATIC", prompt, WS_CHILD | WS_VISIBLE, S(12), S(12), S(340), S(20), d, nullptr, g_inst, nullptr);
    ctx.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", value.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, S(12), S(36), S(340), S(24), d, nullptr, g_inst, nullptr);
    SetWindowTheme(ctx.edit, g_dark ? L"DarkMode_CFD" : nullptr, nullptr);
    HWND ok = mkBtn(d, L"\u2713", L"OK", IDOK);
    HWND ca = mkBtn(d, L"", L"Cancel", IDCANCEL);
    MoveWindow(ok, S(176), S(68), S(86), S(30), TRUE);
    MoveWindow(ca, S(268), S(68), S(86), S(30), TRUE);
    setFont(st); setFont(ctx.edit);
    SendMessageW(ctx.edit, EM_SETSEL, 0, -1);
    EnableWindow(hMain, FALSE);
    ShowWindow(d, SW_SHOW);
    SetFocus(ctx.edit);
    MSG msg;
    while (!ctx.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(d, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(hMain, TRUE);
    DestroyWindow(d);
    SetForegroundWindow(hMain);
    return ctx.ok;
}


// ---------- Auto Sort preview ----------
struct SortCtx {
    const SortPlan* plan = nullptr; const Playset* ps = nullptr;
    HWND list = nullptr, label = nullptr, ok = nullptr, cancel = nullptr;
    bool done = false, apply = false;
};
static void sortDlgLayout(HWND h, SortCtx* c) {
    RECT rc; GetClientRect(h, &rc);
    int m = S(12);
    MoveWindow(c->label, m, m, rc.right - 2 * m, S(44), TRUE);
    MoveWindow(c->list, m, m + S(50), rc.right - 2 * m, rc.bottom - S(50) - S(52) - 2 * m + S(12), TRUE);
    MoveWindow(c->ok, rc.right - m - S(150) - S(8) - S(100), rc.bottom - m - S(30), S(150), S(30), TRUE);
    MoveWindow(c->cancel, rc.right - m - S(100), rc.bottom - m - S(30), S(100), S(30), TRUE);
    RECT lr; GetClientRect(c->list, &lr);
    ListView_SetColumnWidth(c->list, 4, lr.right - S(56) - S(300) - S(100) - S(90) > S(160) ? lr.right - S(56) - S(300) - S(100) - S(90) : S(160));
}
static LRESULT CALLBACK SortProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    SortCtx* c = (SortCtx*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); themeFrame(h); return 0;
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_CTLCOLORSTATIC: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT*)l); return TRUE;
        case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize.x = S(720); mi->ptMinTrackSize.y = S(360); return 0; }
        case WM_SIZE: if (c && c->list && w != SIZE_MINIMIZED) { sortDlgLayout(h, c); RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN); } return 0;
        case WM_NOTIFY: {
            NMHDR* nh = (NMHDR*)l;
            if (c && nh->hwndFrom == c->list && nh->code == NM_CUSTOMDRAW) {
                auto* cd = (NMLVCUSTOMDRAW*)l;
                const Theme& t = T();
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    cd->clrTextBk = (row & 1) ? t.alt : t.list;
                    cd->clrText = t.text;
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    cd->clrTextBk = (row & 1) ? t.alt : t.list;
                    cd->clrText = t.text;
                    if (cd->iSubItem == 3 && c->plan && row < c->plan->order.size()) {
                        int old = c->plan->order[row];
                        cd->clrText = old > (int)row ? t.ok : old < (int)row ? t.warn : t.muted;
                    }
                    return CDRF_DODEFAULT;
                }
            }
            break;
        }
        case WM_COMMAND:
            if (c && LOWORD(w) == IDOK) { c->apply = true; c->done = true; }
            else if (c && LOWORD(w) == IDCANCEL) c->done = true;
            return 0;
        case WM_CLOSE: if (c) c->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static bool showSortPreview(const SortPlan& plan, const Playset& ps) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SortProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCSort";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    SortCtx ctx; ctx.plan = &plan; ctx.ps = &ps;
    RECT pr; GetWindowRect(hMain, &pr);
    int w = S(980), h = S(620);
    int x = pr.left + ((pr.right - pr.left) - w) / 2, y = pr.top + ((pr.bottom - pr.top) - h) / 2;
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RCSort", L"Auto Sort preview - The Royal Court", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN, x, y, w, h, hMain, nullptr, g_inst, &ctx);
    std::wstring sum = L"Auto Sort will move " + std::to_wstring(plan.moves.size()) + L" of " + std::to_wstring(ps.mods.size()) +
        L" mods. Green = moves earlier, amber = moves later. Nothing is changed until you press Apply, and you can undo it afterwards.";
    if (plan.conflictChoices > 0) sum += L" " + std::to_wstring(plan.conflictChoices) + L" file-conflict tie-break(s) applied.";
    for (auto& wn : plan.warnings) sum += L"\n⚠ " + W(wn);
    ctx.label = CreateWindowW(L"STATIC", sum.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, d, nullptr, g_inst, nullptr);
    setFont(ctx.label);
    ctx.list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, 0, 10, 10, d, nullptr, g_inst, nullptr);
    ListView_SetExtendedListViewStyle(ctx.list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    setFont(ctx.list);
    SetWindowSubclass(ctx.list, ListSub, 2, 0);
    SetWindowTheme(ctx.list, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    const Theme& t = T();
    ListView_SetBkColor(ctx.list, t.list); ListView_SetTextBkColor(ctx.list, t.list); ListView_SetTextColor(ctx.list, t.text);
    const wchar_t* heads[5] = {L"New #", L"Mod", L"Type", L"Change", L"Why"};
    int widths[5] = {S(56), S(300), S(100), S(90), S(300)};
    for (int i = 0; i < 5; i++) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = (LPWSTR)heads[i]; col.cx = widths[i]; ListView_InsertColumn(ctx.list, i, &col); }
    std::map<int, const SortMove*> mv;
    for (auto& m : plan.moves) mv[m.from] = &m;
    auto& lk = g_settings.locks[ps.name];
    for (int p = 0; p < (int)plan.order.size(); p++) {
        int old = plan.order[(size_t)p];
        auto it = g_info.find(ps.mods[(size_t)old].id);
        std::wstring num = std::to_wstring(p + 1), name = it != g_info.end() ? W(it->second.name) : W(ps.mods[(size_t)old].name.empty() ? ps.mods[(size_t)old].id : ps.mods[(size_t)old].name);
        LVITEMW li{}; li.mask = LVIF_TEXT; li.iItem = p; li.pszText = &num[0];
        ListView_InsertItem(ctx.list, &li);
        ListView_SetItemText(ctx.list, p, 1, (LPWSTR)name.c_str());
        std::wstring ty = W(catName(plan.cat[(size_t)old]));
        ListView_SetItemText(ctx.list, p, 2, (LPWSTR)ty.c_str());
        std::wstring ch, why;
        if (lk.count(ps.mods[(size_t)old].id)) { ch = L"\U0001F512 locked"; }
        else if (old > p) { ch = L"▲ from " + std::to_wstring(old + 1); }
        else if (old < p) { ch = L"▼ from " + std::to_wstring(old + 1); }
        auto f = mv.find(old);
        if (f != mv.end()) why = W(f->second->reason);
        ListView_SetItemText(ctx.list, p, 3, (LPWSTR)ch.c_str());
        ListView_SetItemText(ctx.list, p, 4, (LPWSTR)why.c_str());
    }
    ctx.ok = mkBtn(d, L"✓", L"Apply sort", IDOK);
    ctx.cancel = mkBtn(d, L"", L"Cancel", IDCANCEL);
    sortDlgLayout(d, &ctx);
    themeFrame(d);
    EnableWindow(hMain, FALSE);
    ShowWindow(d, SW_SHOW);
    SetFocus(ctx.ok);
    MSG msg;
    while (!ctx.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(d, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(hMain, TRUE);
    DestroyWindow(d);
    SetForegroundWindow(hMain);
    return ctx.apply;
}

// ---------- changelog window: one expandable branch per version ----------
static std::vector<std::wstring> wrapText(const std::wstring& t, size_t width) {
    std::vector<std::wstring> lines;
    std::wstring cur;
    size_t i = 0;
    while (i < t.size()) {
        size_t sp = t.find(L' ', i);
        std::wstring word = t.substr(i, sp == std::wstring::npos ? std::wstring::npos : sp - i);
        if (!cur.empty() && cur.size() + 1 + word.size() > width) { lines.push_back(cur); cur.clear(); }
        if (!cur.empty()) cur += L' ';
        cur += word;
        if (sp == std::wstring::npos) break;
        i = sp + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

static HTREEITEM addNode(HWND tree, HTREEITEM parent, const std::wstring& text) {
    TVINSERTSTRUCTW is{};
    is.hParent = parent;
    is.hInsertAfter = TVI_LAST;
    is.item.mask = TVIF_TEXT;
    is.item.pszText = (LPWSTR)text.c_str();
    return (HTREEITEM)SendMessageW(tree, TVM_INSERTITEMW, 0, (LPARAM)&is);
}

static void addBulletLines(HWND tree, HTREEITEM parent, const std::wstring& text, bool bullet) {
    auto lines = wrapText(text, 105);
    for (size_t i = 0; i < lines.size(); i++) addNode(tree, parent, (i == 0 && bullet ? std::wstring(L"\u2022 ") : std::wstring(L"    ")) + lines[i]);
}

static void buildChangelogTree(HWND tree) {
    std::string txt = "Changelog not found.";
    if (HRSRC r = FindResourceW(nullptr, L"CHANGELOG", MAKEINTRESOURCEW(10))) {  // RT_RCDATA
        if (HGLOBAL g = LoadResource(nullptr, r)) txt.assign((const char*)LockResource(g), SizeofResource(nullptr, r));
    }
    // An empty "Unreleased" placeholder is hidden so the newest real version is the one that opens.
    if (size_t u = txt.find("## [Unreleased]\n- Nothing yet."); u != std::string::npos) {
        size_t e = txt.find("## [", u + 5);
        if (e != std::string::npos) txt.erase(u, e - u);
    }
    HTREEITEM version = nullptr, category = nullptr, firstVersion = nullptr;
    std::vector<HTREEITEM> firstCats;
    for (size_t p; (p = txt.find("**")) != std::string::npos;) txt.erase(p, 2);   // plain text: no markdown marks in the tree
    txt.erase(std::remove(txt.begin(), txt.end(), '`'), txt.end());
    std::istringstream in(txt);
    std::string line;
    bool inFirst = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("## ", 0) == 0) {
            std::wstring head = W(line.substr(3));
            std::wstring out;
            for (wchar_t c : head) if (c != L'[' && c != L']') out += c;
            if (!out.empty() && iswdigit(out[0])) out = L"Version " + out;
            version = addNode(tree, TVI_ROOT, out);
            category = nullptr;
            if (!firstVersion) { firstVersion = version; inFirst = true; } else inFirst = false;
        } else if (!version) {
            continue;
        } else if (line.rfind("### ", 0) == 0) {
            category = addNode(tree, version, W(line.substr(4)));
            if (inFirst) firstCats.push_back(category);
        } else if (line.rfind("- ", 0) == 0) {
            addBulletLines(tree, category ? category : version, W(line.substr(2)), true);
        } else if (!line.empty()) {
            addBulletLines(tree, category ? category : version, W(line), false);
        }
    }
    if (firstVersion) {
        SendMessageW(tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)firstVersion);
        for (auto c : firstCats) SendMessageW(tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)c);
        SendMessageW(tree, TVM_ENSUREVISIBLE, 0, (LPARAM)firstVersion);
    }
}

static void expandAll(HWND tree, HTREEITEM item, UINT code) {
    for (; item; item = (HTREEITEM)SendMessageW(tree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)item)) {
        HTREEITEM child = (HTREEITEM)SendMessageW(tree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)item);
        if (child) { expandAll(tree, child, code); SendMessageW(tree, TVM_EXPAND, code, (LPARAM)item); }
    }
}

static void applyLogTheme(HWND t) {
    const Theme& th = T();
    SetWindowTheme(t, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    TreeView_SetBkColor(t, th.list);
    TreeView_SetTextColor(t, th.text);
    TreeView_SetLineColor(t, th.border);
}

static LRESULT CALLBACK LogProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            auto mkc = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex) {
                HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_inst, nullptr);
                setFont(c);
                return c;
            };
            mkBtn(h, L"\u25BE", L"Expand all", ID_EXPAND);
            mkBtn(h, L"\u25B8", L"Collapse all", ID_COLLAPSE);
            HWND t = mkc(WC_TREEVIEWW, L"", TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_TABSTOP, ID_TREE, 0);
            buildChangelogTree(t);
            themeFrame(h);
            applyLogTheme(t);
            return 0;
        }
        case WM_APP + 1: { themeFrame(h); applyLogTheme(GetDlgItem(h, ID_TREE)); RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME); return 0; }
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT*)l); return TRUE;
        case WM_SIZE: {
            RECT r; GetClientRect(h, &r);
            MoveWindow(GetDlgItem(h, ID_EXPAND), S(8), S(8), S(120), S(28), TRUE);
            MoveWindow(GetDlgItem(h, ID_COLLAPSE), S(134), S(8), S(130), S(28), TRUE);
            MoveWindow(GetDlgItem(h, ID_TREE), 0, S(42), r.right, r.bottom - S(42), TRUE);
            return 0;
        }
        case WM_COMMAND: {
            HWND t = GetDlgItem(h, ID_TREE);
            HTREEITEM root = (HTREEITEM)SendMessageW(t, TVM_GETNEXTITEM, TVGN_ROOT, 0);
            if (LOWORD(w) == ID_EXPAND) expandAll(t, root, TVE_EXPAND);
            else if (LOWORD(w) == ID_COLLAPSE) { expandAll(t, root, TVE_COLLAPSE); SendMessageW(t, TVM_ENSUREVISIBLE, 0, (LPARAM)root); }
            return 0;
        }
        case WM_CLOSE: DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static HWND g_logWin = nullptr;
static void showChangelog() {
    if (g_logWin && IsWindow(g_logWin)) { if (IsIconic(g_logWin)) ShowWindow(g_logWin, SW_RESTORE); SetForegroundWindow(g_logWin); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = LogProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCLog";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    g_logWin = CreateWindowExW(0, L"RCLog", L"Changelog - The Royal Court", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, S(780), S(560), hMain, nullptr, g_inst, nullptr);
}

// ---------- conflicts window ----------
static int g_cfPairA = -1, g_cfPairB = -1;           // set when a pair row was opened: the file list shows only their shared files
static std::vector<int> g_cfRows;                    // visible rows: indexes into pairs / files / wipes of g_conf
static HWND hCfList = nullptr, hCfFilter = nullptr, hCfStatus = nullptr;
static std::wstring g_cfText;

static std::wstring modLabel(int pos) {
    Playset* ps = active();
    if (!ps || pos < 0 || pos >= (int)ps->mods.size()) return L"?";
    auto it = g_info.find(ps->mods[(size_t)pos].id);
    return L"#" + std::to_wstring(pos + 1) + L"   " + W(it != g_info.end() ? it->second.name : ps->mods[(size_t)pos].id);
}

static void cfColumns() {
    while (ListView_DeleteColumn(hCfList, 0)) {}
    struct Col { const wchar_t* t; int w; };
    std::vector<Col> cols;
    if (g_cfView == 0) { cols.push_back({L"Loses (loads earlier)", 280}); cols.push_back({L"Wins (loads later)", 280}); cols.push_back({L"Files", 70}); cols.push_back({L"Where", 420}); }
    else { cols.push_back({L"File", 460}); cols.push_back({L"Winner (loads last)", 280}); cols.push_back({L"Also in", 420}); }
    for (size_t i = 0; i < cols.size(); i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPWSTR)cols[i].t;
        c.cx = S(cols[i].w);
        ListView_InsertColumn(hCfList, (int)i, &c);
    }
    ListView_SetColumnWidth(hCfList, (int)cols.size() - 1, LVSCW_AUTOSIZE_USEHEADER);
}

static void cfRebuildRows() {
    if (!hConf) return;
    g_cfRows.clear();
    std::string f = lower(U(getText(hCfFilter)));
    Playset* ps = active();
    std::vector<std::string> names;   // lower-case mod names by playset position, for filtering
    if (ps && !f.empty()) for (size_t i = 0; i < ps->mods.size(); i++) { auto it = g_info.find(ps->mods[i].id); names.push_back(lower(it != g_info.end() ? it->second.name : ps->mods[i].id)); }
    auto nm = [&](int p) -> const std::string& { static const std::string none; return p >= 0 && p < (int)names.size() ? names[(size_t)p] : none; };
    if (g_conf.valid) {
        if (g_cfView == 0) {
            for (size_t i = 0; i < g_conf.pairs.size(); i++) {
                const auto& p = g_conf.pairs[i];
                if (!f.empty()) {
                    bool hit = nm(p.a).find(f) != std::string::npos || nm(p.b).find(f) != std::string::npos;
                    for (auto& a : p.areas) if (!hit && a.first.find(f) != std::string::npos) hit = true;
                    if (!hit) continue;
                }
                g_cfRows.push_back((int)i);
            }
        } else if (g_cfView == 1) {
            for (size_t i = 0; i < g_conf.files.size(); i++) {
                const auto& fc = g_conf.files[i];
                if (g_cfPairA >= 0 && (std::find(fc.mods.begin(), fc.mods.end(), g_cfPairA) == fc.mods.end() || std::find(fc.mods.begin(), fc.mods.end(), g_cfPairB) == fc.mods.end())) continue;
                if (!f.empty() && fc.path.find(f) == std::string::npos && nm(fc.mods.back()).find(f) == std::string::npos) continue;
                g_cfRows.push_back((int)i);
            }
        }
    }
    ListView_SetItemCountEx(hCfList, (int)g_cfRows.size(), LVSICF_NOINVALIDATEALL);
    InvalidateRect(hCfList, nullptr, TRUE);
    std::wstring st;
    if (g_scanning) st = L"Reading mod files... " + std::to_wstring(g_scanDone) + L"/" + std::to_wstring(g_scanTotal);
    else if (!g_conf.valid) st = L"No file data yet. Enable some mods that have a files folder, then press Rescan files.";
    else {
        st = std::to_wstring(g_conf.files.size()) + L" conflicting files  |  " + std::to_wstring(g_conf.pairs.size()) + L" mod pairs  |  " +
             std::to_wstring(g_conf.modsIndexed) + L" mods checked";
        if (g_conf.modsWithoutFiles) st += L" (" + std::to_wstring(g_conf.modsWithoutFiles) + L" skipped: no readable files folder)";
        if (g_cfView == 1 && g_cfPairA >= 0) st = L"Files shared by " + modLabel(g_cfPairA) + L"  and  " + modLabel(g_cfPairB) + L"  (press \"By file\" to see all)  |  " + std::to_wstring(g_cfRows.size()) + L" files";
        else if (g_cfView == 0) st += L".  The later mod wins. Double-click a pair for its files.";
    }
    SetWindowTextW(hCfStatus, st.c_str());
}

static void cfEmptyRows();
static void confRefresh() { if (hConf) { cfEmptyRows(); cfRebuildRows(); } }

static const wchar_t* cfCell(int row, int sub) {
    if (row < 0 || row >= (int)g_cfRows.size()) return L"";
    size_t idx = (size_t)g_cfRows[(size_t)row];
    g_cfText.clear();
    if (g_cfView == 0) {
        if (idx >= g_conf.pairs.size()) return L"";
        const auto& p = g_conf.pairs[idx];
        if (sub == 0) g_cfText = modLabel(p.a);
        else if (sub == 1) g_cfText = modLabel(p.b);
        else if (sub == 2) g_cfText = std::to_wstring(p.count);
        else {
            for (size_t i = 0; i < p.areas.size() && i < 4; i++) g_cfText += (i ? L",  " : L"") + W(p.areas[i].first) + L" \u00D7" + std::to_wstring(p.areas[i].second);
            if (p.areas.size() > 4) g_cfText += L"  ...";
        }
    } else {
        if (idx >= g_conf.files.size()) return L"";
        const auto& fc = g_conf.files[idx];
        if (sub == 0) g_cfText = W(fc.path);
        else if (sub == 1) g_cfText = modLabel(fc.mods.back());
        else {
            for (size_t i = 0; i + 1 < fc.mods.size() && i < 4; i++) g_cfText += (i ? L";  " : L"") + modLabel(fc.mods[i]);
            if (fc.mods.size() > 5) g_cfText += L"  +" + std::to_wstring(fc.mods.size() - 5) + L" more";
        }
    }
    return g_cfText.c_str();
}

// Switching views: the visible row list is emptied BEFORE the columns change, so the list never asks for cells of the old view.
static void cfEmptyRows() {
    g_cfRows.clear();
    ListView_SetItemCountEx(hCfList, 0, 0);
}

static void cfSetView(int v) {
    cfEmptyRows();
    g_cfView = v;
    g_cfPairA = g_cfPairB = -1;
    cfColumns();
    cfRebuildRows();
    for (int id : {ID_CF_PAIR, ID_CF_FILE}) InvalidateRect(GetDlgItem(hConf, id), nullptr, FALSE);
}

static void cfLayout() {
    RECT r; GetClientRect(hConf, &r);
    int m = S(12), y = m, x = m, bh = S(30), gap = S(6);
    MoveWindow(GetDlgItem(hConf, ID_CF_PAIR), x, y, S(140), bh, TRUE); x += S(140) + gap;
    MoveWindow(GetDlgItem(hConf, ID_CF_FILE), x, y, S(110), bh, TRUE); x += S(110) + gap;
    x += gap;
    MoveWindow(GetDlgItem(hConf, ID_CF_RESCAN), r.right - m - S(150), y, S(150), bh, TRUE);
    int fw = r.right - m - S(150) - gap * 2 - x;
    MoveWindow(hCfFilter, x, y + S(3), fw < S(100) ? S(100) : fw, bh - S(6), TRUE);
    y += bh + gap;
    MoveWindow(hCfStatus, m, y + S(2), r.right - 2 * m, S(20), TRUE);
    y += S(26);
    MoveWindow(hCfList, m, y, r.right - 2 * m, r.bottom - y - m, TRUE);
    int nc = Header_GetItemCount(ListView_GetHeader(hCfList));
    if (nc > 0) ListView_SetColumnWidth(hCfList, nc - 1, LVSCW_AUTOSIZE_USEHEADER);   // the last column takes the remaining width
}

static void cfTheme() {
    const Theme& t = T();
    themeFrame(hConf);
    SetWindowTheme(hCfList, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetWindowTheme(hCfFilter, g_dark ? L"DarkMode_CFD" : nullptr, nullptr);
    ListView_SetBkColor(hCfList, t.list);
    ListView_SetTextBkColor(hCfList, t.list);
    ListView_SetTextColor(hCfList, t.text);
    RedrawWindow(hConf, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

static LRESULT CALLBACK ConfProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            hConf = h;
            mkBtn(h, L"↔", L"By mod pair", ID_CF_PAIR);
            mkBtn(h, L"☰", L"By file", ID_CF_FILE);
            mkBtn(h, L"↻", L"Rescan files", ID_CF_RESCAN);
            hCfFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_CF_FILTER, g_inst, nullptr);
            setFont(hCfFilter);
            hCfStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_STATUS, g_inst, nullptr);
            setFont(hCfStatus);
            hCfList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_CF_LIST, g_inst, nullptr);
            setFont(hCfList);
            ListView_SetExtendedListViewStyle(hCfList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowSubclass(hCfList, ListSub, 2, 0);
            cfTheme();
            cfColumns();
            cfLayout();
            cfRebuildRows();
            return 0;
        }
        case WM_SIZE: if (hCfList) cfLayout(); return 0;
        case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize.x = S(760); mi->ptMinTrackSize.y = S(360); return 0; }
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT*)l); return TRUE;
        case WM_APP + 1: cfTheme(); return 0;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_CF_PAIR: g_cfPairA = g_cfPairB = -1; cfSetView(0); break;
                case ID_CF_FILE: g_cfPairA = g_cfPairB = -1; cfSetView(1); break;
                case ID_CF_FILTER: if (HIWORD(w) == EN_CHANGE) cfRebuildRows(); break;
                case ID_CF_RESCAN: g_fileIndex.clear(); g_confSig.clear(); g_conf = ConflictReport(); maybeScan(false); cfRebuildRows(); refreshNotes(); break;
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR* nh = (NMHDR*)l;
            if (nh->hwndFrom != hCfList) return 0;
            if (nh->code == LVN_GETDISPINFOW) {
                auto* di = (NMLVDISPINFOW*)l;
                if (di->item.mask & LVIF_TEXT) di->item.pszText = (LPWSTR)cfCell(di->item.iItem, di->item.iSubItem);
                return 0;
            }
            if (nh->code == NM_DBLCLK) {
                int row = ((NMITEMACTIVATE*)l)->iItem;
                if (g_cfView == 0 && row >= 0 && row < (int)g_cfRows.size()) {
                    const auto& p = g_conf.pairs[(size_t)g_cfRows[(size_t)row]];
                    int a = p.a, b = p.b;
                    cfEmptyRows();
                    g_cfView = 1; g_cfPairA = a; g_cfPairB = b;
                    cfColumns(); cfRebuildRows();
                    for (int id : {ID_CF_PAIR, ID_CF_FILE}) InvalidateRect(GetDlgItem(hConf, id), nullptr, FALSE);
                }
                return 0;
            }
            if (nh->code == NM_CUSTOMDRAW) {
                auto* cd = (NMLVCUSTOMDRAW*)l;
                const Theme& t = T();
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    bool sel = (ListView_GetItemState(hCfList, (int)row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                    cd->clrTextBk = sel ? t.sel : (row & 1) ? t.alt : t.list;
                    cd->clrText = sel ? t.selText : t.text;
                }
            }
            return CDRF_DODEFAULT;
        }
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: hConf = hCfList = hCfFilter = hCfStatus = nullptr; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void showConflicts() {
    if (hConf) { if (IsIconic(hConf)) ShowWindow(hConf, SW_RESTORE); SetForegroundWindow(hConf); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ConfProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCConf";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    g_cfView = 0; g_cfPairA = g_cfPairB = -1;
    CreateWindowExW(0, L"RCConf", L"Conflicts - The Royal Court", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1080), S(600), hMain, nullptr, g_inst, nullptr);
    if (!g_scanning) maybeScan(true);
    ensureConflicts();
    confRefresh();
}

// ---------- Play: start the game directly, without the Paradox Launcher ----------
static std::string regString(HKEY root, const wchar_t* sub, const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD sz = sizeof buf;
    if (RegGetValueW(root, sub, name, RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) return "";
    return U(buf);
}

// Looks for ck3.exe in the Steam library that holds Crusader Kings III.
static std::string findGameExeInSteam() {
    std::string steam = regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (steam.empty()) steam = regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (steam.empty()) return "";
    std::vector<std::string> libs = {steam};
    std::string vdf;
    if (readFile(P(steam) / "steamapps" / "libraryfolders.vdf", vdf)) for (auto& l : steamLibraryPaths(vdf)) libs.push_back(l);
    std::error_code ec;
    for (auto& lib : libs) {
        std::string acf;
        fs::path apps = P(lib) / "steamapps";
        if (!readFile(apps / (std::string("appmanifest_") + CK3_APPID + ".acf"), acf)) continue;
        fs::path exe = apps / "common" / P(acfInstallDir(acf)) / "binaries" / "ck3.exe";
        if (fs::exists(exe, ec)) return exe.u8string();
    }
    return "";
}

static bool processRunning(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = true; break; } } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

static bool pickGameExe(std::wstring& out) {
    wchar_t buf[MAX_PATH * 2] = {0};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hMain;
    o.lpstrTitle = L"Find ck3.exe (in your Crusader Kings III folder, inside \"binaries\")";
    o.lpstrFilter = L"ck3.exe\0ck3.exe\0Programs (*.exe)\0*.exe\0";
    o.lpstrFile = buf;
    o.nMaxFile = (DWORD)(sizeof buf / sizeof buf[0]);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o)) return false;
    out = buf;
    return true;
}

static bool startProgram(const std::string& exeUtf8, std::wstring& err) {
    fs::path exe = P(exeUtf8);
    std::wstring wexe = exe.wstring(), cwd = exe.parent_path().wstring();
    std::wstring cmd = L"\"" + wexe + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(wexe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
        err = L"Windows error " + std::to_wstring(GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// ---------- dialogs ----------
static void browseFolder() {
    BROWSEINFOW bi{};
    bi.hwndOwner = hMain;
    bi.lpszTitle = L"Select your Crusader Kings III folder (the one that contains \"mod\")";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) SetWindowTextW(hDir, path);
        CoTaskMemFree(pidl);
    }
}

static bool pickFile(bool save, const std::wstring& suggested, std::wstring& out) {
    wchar_t buf[MAX_PATH * 2] = {0};
    wcsncpy(buf, suggested.c_str(), MAX_PATH - 1);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hMain;
    o.lpstrFilter = L"Paradox Launcher playset (*.json)\0*.json\0All files (*.*)\0*.*\0";
    o.nFilterIndex = 1;
    o.lpstrFile = buf;
    o.nMaxFile = (DWORD)(sizeof buf / sizeof buf[0]);
    o.lpstrDefExt = L"json";
    o.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    if (!ok) return false;
    out = buf;
    return true;
}

// ---------- layout ----------
static HDWP g_dwp = nullptr;  // all controls are moved in one batch so the window repaints once, cleanly
static void place(HWND h, int x, int y, int w, int hgt) {
    if (w < 0) w = 0;
    if (hgt < 0) hgt = 0;
    if (g_dwp) { HDWP n = DeferWindowPos(g_dwp, h, nullptr, x, y, w, hgt, SWP_NOZORDER | SWP_NOACTIVATE); g_dwp = n; }
    else SetWindowPos(h, nullptr, x, y, w, hgt, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void layout() {
    RECT rc; GetClientRect(hMain, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;  // minimized
    int Wd = rc.right, Ht = rc.bottom;
    g_dwp = BeginDeferWindowPos(40);
    int m = S(12), rowH = S(30), gap = S(6);
    g_bannerH = S(60);
    // banner: icon buttons and the theme switch on the right
    int by = (g_bannerH - S(2) - S(32)) / 2;
    int rx = Wd - m;
    place(hTheme, rx - S(64), by + S(2), S(64), S(28)); rx -= S(64) + S(14);
    place(hLog, rx - S(126), by, S(126), S(32)); rx -= S(126) + gap;
    place(hAdv, rx - S(116), by, S(116), S(32)); rx -= S(116) + gap;
    place(hResync, rx - S(36), by, S(36), S(32));

    int y = g_bannerH + m, x = m;
    place(hL1, x, y + S(7), S(52), S(20)); x += S(56);
    place(hCombo, x, y + S(2), S(220), S(300)); x += S(220) + gap * 2;
    place(hNew, x, y, S(76), rowH); x += S(76) + gap;
    place(hDup, x, y, S(110), rowH); x += S(110) + gap;
    place(hRen, x, y, S(98), rowH); x += S(98) + gap;
    place(hDel, x, y, S(90), rowH); x += S(90) + gap * 2;
    place(hExport, x, y, S(96), rowH); x += S(96) + gap;
    place(hImport, x, y, S(96), rowH);
    place(hPlay, Wd - m - S(140), y, S(140), rowH);

    y += rowH + gap + S(2); x = m;
    place(hL2, x, y + S(4), S(24), S(24)); x += S(30);
    place(hFilter, x, y + S(2), S(230), rowH - S(4)); x += S(230) + gap * 2;
    place(hAllOn, x, y, S(140), rowH); x += S(140) + gap;
    place(hAllOff, x, y, S(146), rowH); x += S(146) + gap * 2;
    place(hConflicts, x, y, S(120), rowH); x += S(120) + gap;
    place(hSort, x, y, S(124), rowH); x += S(124) + gap;
    place(hUndo, x, y, S(112), rowH); x += S(112) + gap * 2;
    rx = Wd - m - S(90);
    place(hDown, rx, y, S(90), rowH);
    rx -= gap + S(80);
    place(hUp, rx, y, S(80), rowH);
    place(hCount, x, y + S(7), rx - gap * 2 - x, S(20));

    y += rowH + gap + S(2);
    int bottom = Ht - m - rowH - gap - S(22) - gap;
    g_listFrame = {m, y, Wd - m, bottom};
    place(hList, m + 1, y + 1, Wd - 2 * m - 2, bottom - y - 2);
    int sy = bottom + gap;
    place(hGameVer, Wd - m - S(220), sy, S(220), S(20));
    place(hStatus, m, sy, Wd - 2 * m - S(230), S(20));
    int dy = sy + S(22) + gap;
    place(hL3, m, dy + S(7), S(80), S(20));
    place(hDir, m + S(84), dy + S(2), Wd - 2 * m - S(84) - S(110) - S(130) - 2 * gap, rowH - S(4));
    place(hBrowse, Wd - m - S(110) - gap - S(120), dy, S(110), rowH);
    place(hSaveDir, Wd - m - S(120), dy, S(120), rowH);
    if (g_dwp) { EndDeferWindowPos(g_dwp); g_dwp = nullptr; }
    resizeCols();
    RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static HWND mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hMain, (HMENU)(INT_PTR)id, g_inst, nullptr);
    setFont(h);
    return h;
}

static void createControls() {
    hL1 = mk(L"STATIC", L"Playset", 0, ID_L1);
    hCombo = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_COMBO);
    hNew = mkBtn(hMain, L"+", L"New", ID_NEW);
    hDup = mkBtn(hMain, L"\u2750", L"Duplicate", ID_DUP);
    hRen = mkBtn(hMain, L"\u270E", L"Rename", ID_REN);
    hDel = mkBtn(hMain, L"\u2715", L"Delete", ID_DEL);
    hExport = mkBtn(hMain, L"\u21E7", L"Export", ID_EXPORT);
    hImport = mkBtn(hMain, L"\u21E9", L"Import", ID_IMPORT);
    hPlay = mkBtn(hMain, L"\u25B6", L"Play", ID_PLAY);
    hResync = mkBtn(hMain, L"\u21BB", L"", ID_RESYNC);
    hAdv = mkBtn(hMain, L"\u2699", L"Advanced", ID_ADV);
    hLog = mkBtn(hMain, L"\u2630", L"Changelog", ID_LOG);
    hTheme = mkBtn(hMain, L"", L"", ID_THEME);
    hL2 = mk(L"STATIC", L"\u2315", SS_CENTER, ID_L2);
    SendMessageW(hL2, WM_SETFONT, (WPARAM)g_fontCrown, TRUE);
    hFilter = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_FILTER, WS_EX_CLIENTEDGE);
    hAllOn = mkBtn(hMain, L"\u2611", L"Enable shown", ID_ALLON);
    hAllOff = mkBtn(hMain, L"\u2610", L"Disable shown", ID_ALLOFF);
    hConflicts = mkBtn(hMain, L"\u2694", L"Conflicts", ID_CONFLICTS);
    hSort = mkBtn(hMain, L"\u21C5", L"Auto Sort", ID_SORT);
    hUndo = mkBtn(hMain, L"\u21B6", L"Undo sort", ID_UNDOSORT);
    EnableWindow(hUndo, FALSE);
    hCount = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_COUNT);
    hUp = mkBtn(hMain, L"\u25B2", L"Up", ID_UP);
    hDown = mkBtn(hMain, L"\u25BC", L"Down", ID_DOWN);
    hList = mk(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, ID_LIST, 0);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(hList, ListSub, 2, 0);
    const wchar_t* heads[7] = {L"#", L"Mod", L"Version", L"Game Version", L"Source", L"Type", L"Notes"};
    int widths[7] = {S(64), S(400), S(100), S(110), S(90), S(90), S(280)};
    for (int i = 0; i < 7; i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPWSTR)heads[i];
        c.cx = widths[i];
        ListView_InsertColumn(hList, i, &c);
    }
    hStatus = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_STATUS);
    hGameVer = mk(L"STATIC", L"", SS_RIGHT | SS_ENDELLIPSIS, ID_GAMEVER);
    hL3 = mk(L"STATIC", L"CK3 folder", 0, ID_L3);
    hDir = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_DIR, WS_EX_CLIENTEDGE);
    hBrowse = mkBtn(hMain, L"\u2026", L"Browse", ID_BROWSE);
    hSaveDir = mkBtn(hMain, L"\u2713", L"Save folder", ID_SAVEDIR);
    // tooltips for the icon-only controls
    hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, hMain, nullptr, g_inst, nullptr);
    auto tip = [&](HWND c, const wchar_t* text) {
        TTTOOLINFOW ti{};
        ti.cbSize = sizeof ti; ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND; ti.hwnd = hMain; ti.uId = (UINT_PTR)c; ti.lpszText = (LPWSTR)text;
        SendMessageW(hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    };
    tip(hResync, L"Rescan the mod folder (use after subscribing to a mod while the app is open)");
    tip(hTheme, L"Switch between dark and light");
    tip(hUp, L"Move the selected mod up in the load order");
    tip(hDown, L"Move the selected mod down in the load order");
}

static void info(const wchar_t* text);
// ---------- advanced ----------
static std::wstring sizeText(std::uintmax_t b) {
    wchar_t buf[48];
    if (b >= (1ull << 30)) swprintf(buf, 48, L"%.1f GB", (double)b / (1ull << 30));
    else swprintf(buf, 48, L"%.1f MB", (double)b / (1ull << 20));
    return buf;
}

static void deleteAllSaves() {
    std::string dir = effectiveDir();
    if (dir.empty()) { info(L"The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder."); return; }
    if (processRunning(L"ck3.exe")) { info(L"Crusader Kings III is running. Close the game first, then try again."); return; }
    SaveScan sc = scanSaves(dir);
    if (sc.entries.empty()) { info((L"No saves found in " + sc.folder.wstring()).c_str()); return; }
    std::wstring q = L"Delete ALL saved games?\n\n" + std::to_wstring(sc.files) + L" save file(s), " + sizeText(sc.bytes) + L"\nin " + sc.folder.wstring() +
                     L"\n\nThey are moved to the Recycle Bin (anything too big for it is deleted permanently). If Steam Cloud sync is on, Steam may bring cloud copies back.";
    if (MessageBoxW(hMain, q.c_str(), L"Delete all saves", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    std::wstring list;
    for (auto& p : sc.entries) { list += p.wstring(); list += L'\0'; }
    list += L'\0';
    SHFILEOPSTRUCTW op{};
    op.hwnd = hMain;
    op.wFunc = FO_DELETE;
    op.pFrom = list.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int rc = SHFileOperationW(&op);
    SaveScan after = scanSaves(dir);
    if (rc != 0 || op.fAnyOperationsAborted || !after.entries.empty())
        say(L"Some saves could not be deleted (" + std::to_wstring(after.entries.size()) + L" item(s) left in the save folder).");
    else
        say(L"Deleted " + std::to_wstring(sc.files) + L" save file(s) (" + sizeText(sc.bytes) + L") to the Recycle Bin.");
}

static void advancedMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_FOLDER, L"Open playsets folder");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_DELSAVES, L"Delete all saves...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_OPENSAVES, L"Open saves folder");
    AppendMenuW(m, MF_STRING, ID_ADV_OPENLOGS, L"Open game logs folder");
    RECT r; GetWindowRect(hAdv, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, hMain, nullptr);
    DestroyMenu(m);
    std::string dir = effectiveDir();
    if (cmd == ID_FOLDER) ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (cmd == ID_ADV_DELSAVES) deleteAllSaves();
    else if (cmd == ID_ADV_OPENSAVES || cmd == ID_ADV_OPENLOGS) {
        std::error_code ec;
        fs::path p = P(dir) / (cmd == ID_ADV_OPENSAVES ? "save games" : "logs");
        if (dir.empty() || !fs::is_directory(p, ec)) info(L"That folder does not exist yet.");
        else ShellExecuteW(hMain, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

static void setShown(bool on) {
    Playset* ps = active();
    if (!ps) return;
    for (int idx : g_shown) ps->mods[(size_t)idx].enabled = on;
    saveActive();
    populate();
}

static void info(const wchar_t* text) { MessageBoxW(hMain, text, L"The Royal Court", MB_ICONINFORMATION); }

// A C++ exception must never escape into Windows' message dispatch (that ends the program without a word).
static void reportError(const char* what) {
    std::wstring m = L"Something went wrong, but your playset files are safe.\n\n" + W(what && *what ? what : "unknown error");
    MessageBoxW(hMain, m.c_str(), L"The Royal Court", MB_ICONWARNING);
}

static void onCommand(int id, int code) {
    Playset* ps = active();
    if (id >= ID_CTX_CAT && id <= ID_CTX_CAT + CAT_COUNT) {          // "Type" choice from the right-click menu
        if (ps && g_ctxIdx >= 0 && g_ctxIdx < (int)ps->mods.size()) {
            const std::string& mid = ps->mods[(size_t)g_ctxIdx].id;
            if (id == ID_CTX_CAT + CAT_COUNT) g_settings.cats.erase(mid); else g_settings.cats[mid] = id - ID_CTX_CAT;
            saveSettingsNow(); populate();
        }
        return;
    }
    switch (id) {
        case ID_COMBO:
            if (code == CBN_SELCHANGE) {
                int i = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                if (i >= 0 && i < (int)g_playsets.size()) { g_settings.active = g_playsets[(size_t)i].name; saveSettingsNow(); populate(); }
            }
            break;
        case ID_FILTER: if (code == EN_CHANGE) populate(); break;
        case ID_ALLON: setShown(true); break;
        case ID_ALLOFF: setShown(false); break;
        case ID_NEW: {
            std::wstring v;
            if (!askText(L"New playset", L"Name for the new playset:", v)) break;
            Playset p;
            p.name = uniqueName(g_playsets, U(v));
            syncPlayset(p, g_mods);
            if (!savePlayset(p, g_info)) { info(L"Could not create the playset file."); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            say(L"Created \"" + W(p.name) + L"\".");
            break;
        }
        case ID_DUP: {
            if (!ps) break;
            std::wstring v = W(ps->name) + L" copy";
            if (!askText(L"Duplicate playset", L"Name for the copy:", v)) break;
            Playset p = *ps;
            p.name = uniqueName(g_playsets, U(v));
            if (!savePlayset(p, g_info)) { info(L"Could not create the playset file."); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_REN: {
            if (!ps) break;
            std::wstring v = W(ps->name);
            if (!askText(L"Rename playset", L"New name:", v)) break;
            std::string n = sanitizeFileName(U(v));
            if (n == ps->name) break;
            if (nameTaken(g_playsets, n, ps->name)) { info(L"A playset with that name already exists."); break; }
            std::string oldName = ps->name;
            if (!renamePlayset(*ps, n, g_info)) { info(L"Could not rename the playset file."); break; }
            if (g_settings.locks.count(oldName)) { g_settings.locks[ps->name] = g_settings.locks[oldName]; g_settings.locks.erase(oldName); }
            if (g_undoPlayset == oldName) g_undoPlayset = ps->name;
            g_settings.active = ps->name;
            sortPlaysets(); saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_DEL: {
            if (!ps) break;
            if (g_playsets.size() < 2) { info(L"You need at least one playset."); break; }
            std::wstring q = L"Delete playset \"" + W(ps->name) + L"\"? Its file will be removed.";
            if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            std::string gone = ps->name;
            deletePlayset(gone);
            g_settings.locks.erase(gone);
            g_playsets.erase(std::remove_if(g_playsets.begin(), g_playsets.end(), [&](const Playset& p) { return p.name == gone; }), g_playsets.end());
            g_settings.active = g_playsets[0].name; saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_EXPORT: {
            if (!ps) break;
            std::wstring path;
            if (!pickFile(true, W(sanitizeFileName(ps->name)) + L".json", path)) break;
            // The name written inside the file is the file name you chose, so both always match.
            Playset outPs = *ps;
            std::string stem = fileStem(U(path));
            if (!stem.empty()) outPs.name = stem;
            if (!writeFile(P(U(path)), exportLauncherPlayset(outPs, g_info))) { info(L"Could not write that file."); break; }
            int n = 0;
            for (auto& m : ps->mods) if (m.enabled) n++;
            say(L"Exported \"" + W(outPs.name) + L"\" (" + std::to_wstring(n) + L" enabled mods) to " + path);
            break;
        }
        case ID_IMPORT: {
            std::wstring path;
            if (!pickFile(false, L"", path)) break;
            std::string text;
            if (!readFile(P(U(path)), text, 5u << 20)) { info(L"Could not read that file (is it larger than 5 MB?)."); break; }
            ImportResult r = parsePlaysetFile(text, g_mods);
            if (!r.ok) { info(W(r.error).c_str()); break; }
            Playset p = r.playset;
            std::string stem = fileStem(U(path));  // the imported playset keeps the file's name
            p.name = uniqueName(g_playsets, stem.empty() ? p.name : stem);
            syncPlayset(p, g_mods);
            int missing = 0;
            for (auto& m : p.mods) if (m.enabled && !g_info.count(m.id)) missing++;
            if (!savePlayset(p, g_info)) { info(L"Could not save the imported playset."); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            std::wstring msg = L"Imported \"" + W(p.name) + L"\"";
            if (missing) msg += L": " + std::to_wstring(missing) + L" mod(s) not installed (shown in the list)";
            if (r.invalid) msg += L"; " + std::to_wstring(r.invalid) + L" invalid entries ignored";
            say(msg + L".");
            break;
        }
        case ID_UP:
        case ID_DOWN: {
            if (orderLocked()) { say(L"Clear the filter and column sorting (click the # header) to change load order."); break; }
            int row = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (row < 0) { say(L"Select a mod first."); break; }
            moveMod(row, id == ID_UP ? row - 1 : row + 1);
            break;
        }
        case ID_PLAY: {
            if (!ps) break;
            std::string dir = effectiveDir();
            if (dir.empty()) { info(L"The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder."); break; }
            std::error_code ec;
            std::string exe = (!g_settings.gameExe.empty() && fs::exists(P(g_settings.gameExe), ec)) ? g_settings.gameExe : findGameExeInSteam();
            if (exe.empty()) {
                std::wstring chosen;
                if (!pickGameExe(chosen)) break;
                if (_wcsicmp(fs::path(chosen).filename().c_str(), L"ck3.exe") != 0) { info(L"Please choose ck3.exe (it is in the \"binaries\" folder of your Crusader Kings III install)."); break; }
                exe = U(chosen);
            }
            if (exe != g_settings.gameExe) { g_settings.gameExe = exe; saveSettingsNow(); }
            if (g_gameVer.empty()) { refreshGameVersion(exe); populate(); }
            if (processRunning(L"ck3.exe")) { info(L"Crusader Kings III is already running. Close the game first, then press Play again."); break; }
            if (!processRunning(L"steam.exe")) { info(L"Steam needs to be running to start the game without the launcher. Start Steam, then press Play again."); break; }
            std::set<std::string> inst;
            for (auto& m : g_mods) inst.insert(m.id);
            ApplyResult r = writeGameModList(dir, *ps, inst);
            if (!r.ok) { info(W(r.message).c_str()); break; }
            std::wstring err;
            if (!startProgram(exe, err)) { info((L"Could not start the game (" + err + L").").c_str()); break; }
            say(W(r.message) + L". Starting Crusader Kings III directly (launcher skipped).");
            break;
        }
        case ID_FOLDER: ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_LOG: showChangelog(); break;
        case ID_ADV: advancedMenu(); break;
        case ID_RESYNC: g_fileIndex.clear(); g_confSig.clear(); resync(false); break;
        case ID_CONFLICTS: showConflicts(); break;
        case ID_CTX_LOCK: {
            if (!ps || g_ctxIdx < 0 || g_ctxIdx >= (int)ps->mods.size()) break;
            auto& lk = g_settings.locks[ps->name];
            const std::string& id = ps->mods[(size_t)g_ctxIdx].id;
            if (lk.count(id)) lk.erase(id); else lk.insert(id);
            saveSettingsNow(); populate();
            break;
        }
        case ID_SORT: {
            if (!ps || ps->mods.size() < 2) { say(L"Nothing to sort."); break; }
            ensureConflicts();
            SortPlan plan = planSort(*ps, g_info, lockedIds(), g_settings.cats, g_conf.valid ? &g_conf : nullptr, &g_fileIndex);
            if (!plan.changed) {
                std::wstring m = L"Already in a good order, nothing to move.";
                for (auto& wn : plan.warnings) m += L"  ⚠ " + W(wn);
                say(m); break;
            }
            if (!showSortPreview(plan, *ps)) { say(L"Auto Sort cancelled, nothing changed."); break; }
            g_undoIds.clear();
            for (auto& m : ps->mods) g_undoIds.push_back(m.id);
            g_undoPlayset = ps->name;
            std::vector<ModRef> nm;
            for (int o : plan.order) nm.push_back(ps->mods[(size_t)o]);
            ps->mods = nm;
            saveActive(); populate();
            say(L"Auto Sort moved " + std::to_wstring(plan.moves.size()) + L" mods. Press Undo sort to go back.");
            break;
        }
        case ID_UNDOSORT: {
            if (!ps || g_undoIds.empty() || g_undoPlayset != ps->name) break;
            std::map<std::string, ModRef> byId;
            for (auto& m : ps->mods) byId.emplace(m.id, m);
            std::vector<ModRef> nm; std::set<std::string> used;
            for (auto& id : g_undoIds) { auto f = byId.find(id); if (f != byId.end() && used.insert(id).second) nm.push_back(f->second); }
            for (auto& m : ps->mods) if (!used.count(m.id)) nm.push_back(m);
            ps->mods = nm;
            g_undoIds.clear();
            saveActive(); populate();
            say(L"Load order restored to how it was before Auto Sort.");
            break;
        }

        case ID_THEME:
            g_dark = !g_dark;
            g_settings.theme = g_dark ? "dark" : "light";
            saveSettingsNow();
            applyTheme();
            break;
        case ID_BROWSE: browseFolder(); break;
        case ID_SAVEDIR: {
            g_settings.ck3Dir = U(getText(hDir));
            reload();
            if (!effectiveDir().empty()) say(L"Folder saved. Found " + std::to_wstring(g_mods.size()) + L" installed mods.");
            break;
        }
    }
}

static LRESULT onNotify(LPARAM l) {
    NMHDR* h = (NMHDR*)l;
    if (h->code == NM_CUSTOMDRAW && hList && h->hwndFrom == ListView_GetHeader(hList)) return drawHeader((NMCUSTOMDRAW*)l);
    if (h->hwndFrom != hList) return 0;
    NMLISTVIEW* nm = (NMLISTVIEW*)l;
    if (h->code == NM_CUSTOMDRAW) {
        // Row colours (zebra, selection, disabled mods muted) plus coloured Notes and Game Version cells.
        auto* cd = (NMLVCUSTOMDRAW*)l;
        const Theme& t = T();
        static COLORREF rowBg = 0, rowFg = 0;
        static bool rowSel = false;
        switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                size_t row = (size_t)cd->nmcd.dwItemSpec;
                Playset* ps = active();
                bool en = ps && row < g_shown.size() && ps->mods[(size_t)g_shown[row]].enabled;
                rowSel = (ListView_GetItemState(hList, (int)row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                rowBg = rowSel ? t.sel : (row & 1) ? t.alt : t.list;
                rowFg = rowSel ? t.selText : en ? t.text : t.muted;
                cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                cd->clrTextBk = rowBg; cd->clrText = rowFg;
                return CDRF_NOTIFYSUBITEMDRAW;
            }
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                size_t row = (size_t)cd->nmcd.dwItemSpec;
                cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                cd->clrTextBk = rowBg; cd->clrText = rowFg;
                if (!rowSel) {
                    if (cd->iSubItem == 6 && row < g_noteSev.size() && g_noteSev[row] > 0)
                        cd->clrText = g_noteSev[row] == 2 ? t.bad : g_noteSev[row] == 1 ? t.warn : t.ok;
                    if (cd->iSubItem == 3 && row < g_verState.size() && g_verState[row] == 2) cd->clrText = t.warn;
                }
                return CDRF_DODEFAULT;
            }
        }
        return CDRF_DODEFAULT;
    }
    if (h->code == LVN_ITEMCHANGED) {
        if (!g_populating && (nm->uChanged & LVIF_STATE) && ((nm->uNewState ^ nm->uOldState) & LVIS_STATEIMAGEMASK)) {
            Playset* ps = active();
            if (ps && nm->iItem >= 0 && nm->iItem < (int)g_shown.size()) {
                ps->mods[(size_t)g_shown[(size_t)nm->iItem]].enabled = ListView_GetCheckState(hList, nm->iItem) != 0;
                saveActive();
                refreshNotes();
                PostMessageW(hMain, WM_APP + 4, 0, 0);
            }
        }
    } else if (h->code == LVN_COLUMNCLICK) {
        // click: ascending, again: descending, again (or "#"): back to load order. Never changes the playset itself.
        int c = nm->iSubItem;
        if (c <= 0) g_sortCol = -1;
        else if (c != g_sortCol) { g_sortCol = c; g_sortAsc = true; }
        else if (g_sortAsc) g_sortAsc = false;
        else g_sortCol = -1;
        updateSortArrows();
        populate();
        say(g_sortCol < 0 ? L"Showing load order." : L"Sorted view (load order is unchanged). Click # to go back to load order.");
    } else if (h->code == NM_DBLCLK) {
        showDetails(((NMITEMACTIVATE*)l)->iItem);
    } else if (h->code == LVN_BEGINDRAG) {
        if (orderLocked()) { say(L"Clear the filter and column sorting (click the # header) to change load order."); return 0; }
        g_drag = true; g_dragFrom = nm->iItem;
        g_mark = LVINSERTMARK{sizeof(LVINSERTMARK), 0, -1, 0};
        SetCapture(hMain);
    }
    return 0;
}

static void endDrag(bool drop) {
    if (!g_drag) return;
    g_drag = false;
    ReleaseCapture();
    LVINSERTMARK none{sizeof(LVINSERTMARK), 0, -1, 0};
    ListView_SetInsertMark(hList, &none);
    if (drop && g_mark.iItem >= 0 && g_dragFrom >= 0) {
        int to = g_mark.iItem + ((g_mark.dwFlags & LVIM_AFTER) ? 1 : 0);
        if (to > g_dragFrom) to--;
        moveMod(g_dragFrom, to);
    }
    g_dragFrom = -1;
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: hMain = h; createControls(); applyTheme(); return 0;
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, true); return 1; }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT*)l); return TRUE;
        case WM_APP + 2: g_scanDone = (int)w; updateCount(); confRefresh(); return 0;
        case WM_APP + 3: {   // background scan finished
            ScanJob* j = (ScanJob*)w;
            if (g_jobThread) { WaitForSingleObject(g_jobThread, 5000); CloseHandle(g_jobThread); g_jobThread = nullptr; }
            for (size_t i = 0; i < j->items.size() && i < j->out.size(); i++) if (j->out[i].complete) g_fileIndex[j->items[i].id] = std::move(j->out[i]);
            bool cancelled = j->cancel.load();
            delete j; g_job = nullptr; g_scanning = false;
            if (!cancelled) { g_confSig.clear(); refreshNotes(); maybeScan(false); }
            return 0;
        }
        case WM_APP + 4: try { maybeScan(w != 0); } catch (...) {} return 0;
        case WM_ACTIVATE:
            if (LOWORD(w) != WA_INACTIVE && g_modStamp && modDirStamp() != g_modStamp) {
                try { resync(true); } catch (...) { g_resyncing = false; }
            }
            return 0;
        case WM_CONTEXTMENU: {
            if ((HWND)w != hList) break;
            Playset* ps = active();
            POINT pt{(short)LOWORD(l), (short)HIWORD(l)};
            int row = -1;
            if (l == -1) { row = ListView_GetNextItem(hList, -1, LVNI_SELECTED); RECT ir; if (row >= 0 && ListView_GetItemRect(hList, row, &ir, LVIR_BOUNDS)) { pt = {ir.left + S(60), ir.bottom}; ClientToScreen(hList, &pt); } }
            else { POINT cp = pt; ScreenToClient(hList, &cp); LVHITTESTINFO hi{}; hi.pt = cp; row = ListView_HitTest(hList, &hi); }
            if (!ps || row < 0 || row >= (int)g_shown.size()) return 0;
            g_ctxIdx = g_shown[(size_t)row];
            const ModRef& mr = ps->mods[(size_t)g_ctxIdx];
            bool locked = g_settings.locks.count(ps->name) && g_settings.locks[ps->name].count(mr.id);
            bool ovr = false; int cur = catOfMod(mr.id, &ovr);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (locked ? MF_CHECKED : 0), ID_CTX_LOCK, L"Lock position (Auto Sort never moves it)");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            for (int c = 0; c < CAT_COUNT; c++) AppendMenuW(menu, MF_STRING | (ovr && cur == c ? MF_CHECKED : 0), ID_CTX_CAT + c, (L"Type: " + W(catName(c))).c_str());
            AppendMenuW(menu, MF_STRING | (!ovr ? MF_CHECKED : 0), ID_CTX_CAT + CAT_COUNT, L"Type: automatic");
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
            DestroyMenu(menu);
            return 0;
        }
        case WM_SIZE: if (w != SIZE_MINIMIZED) layout(); return 0;
        case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize.x = S(1200); mi->ptMinTrackSize.y = S(420); return 0; }
        case WM_COMMAND:
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) return 0;
            try { onCommand(LOWORD(w), HIWORD(w)); }
            catch (const std::exception& e) { reportError(e.what()); }
            catch (...) { reportError(nullptr); }
            return 0;
        case WM_NOTIFY:
            try { return onNotify(l); }
            catch (const std::exception& e) { reportError(e.what()); }
            catch (...) { reportError(nullptr); }
            return 0;
        case WM_MOUSEMOVE:
            if (g_drag) {
                POINT pt{(short)LOWORD(l), (short)HIWORD(l)};
                MapWindowPoints(hMain, hList, &pt, 1);
                LVINSERTMARK im{sizeof(LVINSERTMARK), 0, -1, 0};
                if (ListView_InsertMarkHitTest(hList, &pt, &im)) g_mark = im; else g_mark.iItem = -1;
                ListView_SetInsertMark(hList, &g_mark);
                SetCursor(LoadCursor(nullptr, IDC_SIZENS));
            }
            return 0;
        case WM_LBUTTONUP: endDrag(true); return 0;
        case WM_CAPTURECHANGED: if (g_drag) endDrag(false); return 0;
        case WM_DESTROY: stopScan(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g_inst = inst;
    // Only one copy at a time: two copies would each keep their own picture of the playset files and overwrite each other's changes.
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\TheRoyalCourt.SingleInstance");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(L"RoyalCourtMain", nullptr)) {
            if (IsIconic(other)) ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&g_gdip, &gsi, nullptr);

    HDC dc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    {
        LOGFONTW lf = ncm.lfMessageFont;
        wcscpy(lf.lfFaceName, L"Segoe UI Symbol");
        g_fontSym = CreateFontIndirectW(&lf);
        lf = ncm.lfMessageFont; lf.lfWeight = FW_SEMIBOLD;
        g_fontBold = CreateFontIndirectW(&lf);
        lf = ncm.lfMessageFont; wcscpy(lf.lfFaceName, L"Segoe UI Symbol"); lf.lfHeight = -S(24);
        g_fontCrown = CreateFontIndirectW(&lf);
        lf = ncm.lfMessageFont; wcscpy(lf.lfFaceName, L"Georgia"); lf.lfHeight = -S(24); lf.lfWeight = FW_BOLD;
        g_fontTitle = CreateFontIndirectW(&lf);
        lf = ncm.lfMessageFont; lf.lfHeight = -S(11); lf.lfWeight = FW_SEMIBOLD;
        g_fontSub = CreateFontIndirectW(&lf);
    }
    loadSettings(g_settings);
    g_dark = g_settings.theme.empty() ? systemPrefersDark() : g_settings.theme == "dark";
    rebuildBrushes();

    g_appData = U(wenv(L"APPDATA"));
    PWSTR docs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs) { g_docRoots.push_back(U(docs)); CoTaskMemFree(docs); }
    std::wstring up = wenv(L"USERPROFILE");
    if (!up.empty()) g_docRoots.push_back(U(up) + "/Documents");
    for (const wchar_t* e : {L"OneDrive", L"OneDriveConsumer", L"OneDriveCommercial"}) {
        std::wstring v = wenv(e);
        if (!v.empty()) g_docRoots.push_back(U(v) + "/Documents");
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = MainProc; wc.hInstance = inst; wc.lpszClassName = L"RoyalCourtMain";
    wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    RegisterClassExW(&wc);

    std::wstring title = L"The Royal Court | CK3 Mod Manager  v" + W(VERSION);
    HWND win = CreateWindowExW(WS_EX_CONTROLPARENT, L"RoyalCourtMain", title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1260), S(660), nullptr, nullptr, inst, nullptr);
    if (!win) return 1;

    try { reload(); }
    catch (const std::exception& e) { reportError(e.what()); }
    catch (...) { reportError(nullptr); }
    ShowWindow(win, show);
    UpdateWindow(win);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(win, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    Gdiplus::GdiplusShutdown(g_gdip);
    return (int)msg.wParam;
}
