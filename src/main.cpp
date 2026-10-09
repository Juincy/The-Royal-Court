// The Royal Court | CK3 Mod Manager - native Windows GUI (Win32 + common controls)
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "core.hpp"
#include "fastwalk.hpp"

#include <windows.h>
#include <mmsystem.h>
#include <winhttp.h>
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
#include <memory>

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
    ID_UP, ID_DOWN, ID_FOLDER, ID_LIST, ID_DIR, ID_BROWSE, ID_SAVEDIR, ID_STATUS, ID_COUNT, ID_L1, ID_L2, ID_L3, ID_GAMEVER, ID_RESYNC, ID_THEME, ID_CONFLICTS, ID_CF_PAIR, ID_CF_FILE, ID_CF_FILTER, ID_CF_RESCAN, ID_CF_LIST, ID_ADV, ID_ADV_DELSAVES, ID_ADV_OPENSAVES, ID_ADV_OPENLOGS, ID_SORT, ID_UNDOSORT, ID_ADV_BACKUP, ID_ADV_RESTORE, ID_ADV_OPENBACKUPS, ID_CF_DEF, ID_CTX_REMOVE = 730, ID_CTX_DELFILES, ID_ADV_LAUNCH, ID_ADV_COMPARE, ID_ADV_UNHIDE, ID_UPDATE, ID_ADV_LOG, ID_ADV_AUTOUPD, ID_ADV_SORTREPORT, ID_CF_VAN,
    ID_TREE, ID_EXPAND, ID_COLLAPSE
};

static HINSTANCE g_inst;
static HWND hMain, hL1, hL2, hL3, hCombo, hNew, hDup, hRen, hDel, hExport, hImport, hPlay, hAdv, hLog, hUpd, hFilter, hAllOn, hAllOff,
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

static int g_cfView = 0;   // conflicts window: 0 by mod pair, 1 by file, 2 by definition (script level), 3 base-game files replaced by mods

// ---------- theme: "Midnight Court" (dark) and "Parchment" (light) ----------
struct Theme { COLORREF bg, banner, list, alt, border, text, muted, accent, accentText, gold, btn, btnHot, btnDown, sel, selText, ok, warn, bad; };
static const Theme DARK_T = {RGB(0x14,0x12,0x1C), RGB(0x1C,0x19,0x28), RGB(0x19,0x17,0x23), RGB(0x1F,0x1C,0x2C), RGB(0x3A,0x35,0x50), RGB(0xEE,0xE9,0xDC), RGB(0x9C,0x96,0xB0),
                             RGB(0xD4,0xAF,0x37), RGB(0x1A,0x14,0x00), RGB(0xD4,0xAF,0x37), RGB(0x2A,0x26,0x40), RGB(0x3A,0x34,0x5C), RGB(0x22,0x1F,0x35), RGB(0x3A,0x33,0x66), RGB(0xFF,0xFF,0xFF),
                             RGB(0x6F,0xCF,0x97), RGB(0xE8,0xA8,0x38), RGB(0xFF,0x6B,0x6B)};
static const Theme LIGHT_T = {RGB(0xF4,0xEF,0xE4), RGB(0xE9,0xE1,0xCF), RGB(0xFC,0xFA,0xF4), RGB(0xF6,0xF1,0xE6), RGB(0xCF,0xC4,0xA8), RGB(0x2A,0x24,0x33), RGB(0x77,0x6E,0x88),
                              RGB(0x4B,0x2E,0x83), RGB(0xFF,0xFF,0xFF), RGB(0xB0,0x84,0x10), RGB(0xE2,0xD9,0xC4), RGB(0xD6,0xCB,0xB0), RGB(0xCB,0xBE,0xA0), RGB(0xDD,0xD1,0xF0), RGB(0x1E,0x16,0x30),
                              RGB(0x2E,0x7D,0x4F), RGB(0xB0,0x62,0x06), RGB(0xB3,0x26,0x1E)};
static bool g_dark = true;
static Theme g_curTheme = DARK_T;   // the colours on screen right now: one of the two palettes, or a blend of them while the theme changes
static const Theme& T() { return g_curTheme; }
static Theme lerpTheme(const Theme& a, const Theme& b, float e) {
    Theme o;
    const COLORREF* pa = (const COLORREF*)&a; const COLORREF* pb = (const COLORREF*)&b; COLORREF* po = (COLORREF*)&o;
    int pct = (int)(e * 100.0f + 0.5f);
    for (size_t i = 0; i < sizeof(Theme) / sizeof(COLORREF); i++) {
        COLORREF x = pa[i], y = pb[i];
        po[i] = RGB((GetRValue(x) * (100 - pct) + GetRValue(y) * pct) / 100, (GetGValue(x) * (100 - pct) + GetGValue(y) * pct) / 100, (GetBValue(x) * (100 - pct) + GetBValue(y) * pct) / 100);
    }
    return o;
}
static HBRUSH g_brBg, g_brBanner, g_brList;
static HFONT g_fontSym, g_fontCrown, g_fontTitle, g_fontSub, g_fontBold, g_fontTiny;
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
static void themeFrameColors(HWND h) {
    COLORREF cap = T().banner, txt = T().text;
    DwmSetWindowAttribute(h, 35, &cap, sizeof cap);  // caption colour (Windows 11)
    DwmSetWindowAttribute(h, 36, &txt, sizeof txt);
}
static void themeFrame(HWND h) {
    BOOL dark = g_dark;
    if (FAILED(DwmSetWindowAttribute(h, 20, &dark, sizeof dark))) DwmSetWindowAttribute(h, 19, &dark, sizeof dark);  // dark title bar
    themeFrameColors(h);
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

static void drawSun(Gdiplus::Graphics& g, float cx, float cy, float size, COLORREF c, int alpha = 255) {
    float r0 = size * 0.15f;
    Gdiplus::SolidBrush br(gc(c, alpha));
    g.FillEllipse(&br, cx - r0, cy - r0, r0 * 2, r0 * 2);
    Gdiplus::Pen pen(gc(c, alpha), size * 0.075f);
    pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 3.14159265f / 4.0f;
        g.DrawLine(&pen, cx + cosf(a) * r0 * 1.9f, cy + sinf(a) * r0 * 1.9f, cx + cosf(a) * r0 * 2.6f, cy + sinf(a) * r0 * 2.6f);
    }
}
// A crescent: one disc with a second, offset disc cut out of it (so it works on any background, gradients included).
static void drawMoon(Gdiplus::Graphics& g, float cx, float cy, float size, COLORREF c, int alpha = 255) {
    float r = size * 0.30f;
    Gdiplus::GraphicsPath disc, bite;
    disc.AddEllipse(cx - r, cy - r, r * 2, r * 2);
    float r2 = r * 0.82f;
    bite.AddEllipse(cx - r2 + r * 0.62f, cy - r2 - r * 0.38f, r2 * 2, r2 * 2);
    Gdiplus::Region reg(&disc);
    reg.Exclude(&bite);
    Gdiplus::SolidBrush br(gc(c, alpha));
    g.FillRegion(&br, &reg);
}

// The theme switch: a raised 3D panel switch. Dark mode = blue night track with the moon on a glossy knob at the right,
// light mode = warm sunrise track with the sun at the left. The other symbol stays faintly visible on the track.
// 0 = light (sun, knob left) .. 1 = dark (moon, knob right); eased between the two while the switch slides
static float g_togT = -1.0f, g_togFrom = 0, g_togTo = 0;
static ULONGLONG g_togStart = 0;
static const UINT TIMER_TOGGLE = 79;
static const DWORD TOGGLE_MS = 320;
static float easeInOut(float p) { p = p < 0 ? 0 : p > 1 ? 1 : p; return p * p * (3.0f - 2.0f * p); }
static COLORREF lerpC(COLORREF light, COLORREF dark, float t) { return mix(light, dark, (int)(t * 100.0f + 0.5f)); }

static void drawToggle(const DRAWITEMSTRUCT* d, COLORREF around) {
    using namespace Gdiplus;
    if (g_togT < 0) g_togT = g_dark ? 1.0f : 0.0f;
    const float tt = g_togT;
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    HBRUSH bg = CreateSolidBrush(around); FillRect(dc, &r, bg); DeleteObject(bg);
    float W = (float)(r.right - r.left), H = (float)(r.bottom - r.top);
    float x = (float)r.left, y = (float)r.top;
    bool hot = g_hot == d->hwndItem, down = (d->itemState & ODS_SELECTED) != 0;
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float u = (float)g_dpi / 96.0f;
    // colours blend between the two looks as the switch moves
    COLORREF trackTop = lerpC(RGB(0xF2,0xD8,0xBC), RGB(0x07,0x0B,0x22), tt), trackBot = lerpC(RGB(0xFF,0xF1,0xDE), RGB(0x16,0x24,0x55), tt);
    COLORREF rimA = lerpC(RGB(0xFF,0x9E,0x3D), RGB(0x2F,0xB4,0xFF), tt), rimB = lerpC(RGB(0xC2,0x3B,0x66), RGB(0x5B,0x4B,0xFF), tt);
    COLORREF knA = lerpC(RGB(0xFF,0xC1,0x4A), RGB(0x3C,0xCB,0xFF), tt), knB = lerpC(RGB(0xF2,0x45,0x2B), RGB(0x5A,0x3C,0xF0), tt);
    if (hot) { rimA = mix(rimA, RGB(255,255,255), 22); rimB = mix(rimB, RGB(255,255,255), 22); }
    // soft shadow under the whole switch
    for (int i = 3; i >= 1; i--) {
        GraphicsPath sh; roundedPath(sh, x + 1.0f * u, y + (1.0f + (float)i * 0.8f) * u, W - 2.0f * u, H - 3.0f * u, H / 2);
        SolidBrush sb(Color((BYTE)(22 + 4 * tt), 0, 0, 0));
        g.FillPath(&sb, &sh);
    }
    float tx = x + 1.0f * u, ty = y + 1.0f * u, tw = W - 2.0f * u, th = H - 4.0f * u;
    GraphicsPath track; roundedPath(track, tx, ty, tw, th, th / 2);
    LinearGradientBrush tg(PointF(tx, ty), PointF(tx, ty + th), gc(trackTop), gc(trackBot));   // sunken track: dark at the top, lighter below
    g.FillPath(&tg, &track);
    {   // inner shadow along the top edge (the track is cut into the panel)
        GraphicsPath clipP; roundedPath(clipP, tx, ty, tw, th, th / 2);
        Region old; g.GetClip(&old);
        g.SetClip(&clipP, CombineModeIntersect);
        GraphicsPath inner; roundedPath(inner, tx, ty + 2.2f * u, tw, th, th / 2);
        Pen ip(Color((BYTE)(70 + 50 * tt), 0, 0, 0), 3.2f * u);
        g.DrawPath(&ip, &inner);
        g.SetClip(&old);
    }
    LinearGradientBrush rg(PointF(tx, ty), PointF(tx + tw, ty), gc(rimA), gc(rimB));
    Pen rim(&rg, 1.8f * u);
    g.DrawPath(&rim, &track);
    {   // thin highlight just inside the rim: the raised bezel
        GraphicsPath hl; roundedPath(hl, tx + 1.0f * u, ty + 1.0f * u, tw - 2.0f * u, th - 2.0f * u, (th - 2.0f * u) / 2);
        Pen hp(Color((BYTE)(90 - 50 * tt), 255, 255, 255), 1.0f * u);
        g.DrawPath(&hp, &hl);
    }
    float k = th - 5.0f * u;
    float pad = (th - k) / 2;
    float kxL = tx + pad, kxR = tx + tw - pad - k;
    float kx = kxL + (kxR - kxL) * tt;
    float ky = ty + pad + (down ? 0.8f * u : 0.0f);
    float sunX = kxL + k / 2, moonX = kxR + k / 2, cyT = ty + th / 2;
    // the symbol of the side the knob is not on stays faintly visible in the track (drawn before the knob so the knob covers it)
    drawSun(g, sunX, cyT, k, RGB(0xFF,0xB2,0x5A), (int)(95 * tt));
    drawMoon(g, moonX, cyT, k * 1.05f, RGB(0x5A,0x3C,0xF0), (int)(85 * (1.0f - tt)));
    for (int i = 3; i >= 1; i--) {             // knob cast shadow
        SolidBrush sb(Color((BYTE)(36 + 10 * tt), 0, 0, 0));
        g.FillEllipse(&sb, kx - (float)(i - 1) * 0.3f * u, ky + (1.0f + (float)i * 0.9f) * u, k + (float)(i - 1) * 0.6f * u, k);
    }
    LinearGradientBrush kg(PointF(kx, ky), PointF(kx + k, ky + k), gc(knA), gc(knB));
    g.FillEllipse(&kg, kx, ky, k, k);
    {   // bevel: light edge top-left, dark edge bottom-right
        LinearGradientBrush bv(PointF(kx, ky), PointF(kx + k, ky + k), Color(150, 255, 255, 255), Color(120, 0, 0, 0));
        Pen bp(&bv, 1.4f * u);
        g.DrawEllipse(&bp, kx + 0.7f * u, ky + 0.7f * u, k - 1.4f * u, k - 1.4f * u);
    }
    {   // gloss on the upper half
        GraphicsPath cap; cap.AddEllipse(kx + k * 0.10f, ky + k * 0.05f, k * 0.80f, k * 0.52f);
        LinearGradientBrush gl(PointF(kx, ky + k * 0.05f), PointF(kx, ky + k * 0.57f), Color(120, 255, 255, 255), Color(6, 255, 255, 255));
        g.FillPath(&gl, &cap);
    }
    // the symbol on the knob changes with it: the sun fades out as the moon fades in
    float kcx = kx + k / 2, kcy = ky + k / 2;
    if (tt < 0.999f) drawSun(g, kcx, kcy, k, RGB(255,255,255), (int)(255 * (1.0f - tt)));
    if (tt > 0.001f) drawMoon(g, kcx, kcy, k * 1.05f, RGB(255,255,255), (int)(255 * tt));
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
    if (id == ID_GAMEVER) {   // "CK3 version: " in the muted colour, only the number in colour
        HBRUSH gb = CreateSolidBrush(t.bg); FillRect(dc, &r, gb); DeleteObject(gb);
        std::wstring all = ctlText(d->hwndItem);
        const std::wstring head = L"CK3 version: ";
        std::wstring num = all.size() > head.size() && all.compare(0, head.size(), head) == 0 ? all.substr(head.size()) : L"";
        bool known = !num.empty() && num != L"unknown";
        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ of0 = SelectObject(dc, g_fontBold);
        SIZE ns{0, 0}; if (known) GetTextExtentPoint32W(dc, num.c_str(), (int)num.size(), &ns);
        SelectObject(dc, g_font);
        std::wstring lead = known ? head : all;
        SIZE ls0{0, 0}; GetTextExtentPoint32W(dc, lead.c_str(), (int)lead.size(), &ls0);
        int x0 = r.right - ns.cx - ls0.cx;
        RECT lr0 = {x0, r.top, x0 + ls0.cx + S(2), r.bottom};
        SetTextColor(dc, t.muted);
        DrawTextW(dc, lead.c_str(), -1, &lr0, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        if (known) {
            SelectObject(dc, g_fontBold);
            RECT nr0 = {x0 + ls0.cx, r.top, r.right, r.bottom};
            SetTextColor(dc, t.gold);
            DrawTextW(dc, num.c_str(), -1, &nr0, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        }
        SelectObject(dc, of0);
        return;
    }
    HBRUSH ab = CreateSolidBrush(around); FillRect(dc, &r, ab); DeleteObject(ab);
    bool dis = (d->itemState & ODS_DISABLED) != 0, down = (d->itemState & ODS_SELECTED) != 0, hot = g_hot == d->hwndItem;
    bool primary = id == ID_PLAY || (id == ID_CF_PAIR && g_cfView == 0) || (id == ID_CF_FILE && g_cfView == 1) || (id == ID_CF_DEF && g_cfView == 2) || (id == ID_CF_VAN && g_cfView == 3);
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

// ---------- themed popup menus ----------
// Menus are drawn by the program so they match the dark / light theme (the Windows default menu stays white).
struct MenuEnt { std::wstring text; bool sep = false; };
static std::vector<std::unique_ptr<MenuEnt>> g_menuEnts;
static void setMenuMode() {   // lets Windows itself draw the menu frame dark or light too (Windows 10 1903+; silently skipped on older systems)
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux) ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    using SetMode = int(WINAPI*)(int);
    using Flush = void(WINAPI*)();
    auto setMode = (SetMode)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    auto flush = (Flush)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (setMode) { setMode(g_dark ? 2 : 3); if (flush) flush(); }   // 2 = force dark, 3 = force light
}
static void themeMenu(HMENU m) {
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) {
        MENUITEMINFOW mi{}; mi.cbSize = sizeof mi; mi.fMask = MIIM_FTYPE | MIIM_STRING; wchar_t buf[512] = L""; mi.dwTypeData = buf; mi.cch = 511;
        if (!GetMenuItemInfoW(m, (UINT)i, TRUE, &mi)) continue;
        auto e = std::make_unique<MenuEnt>();
        e->sep = (mi.fType & MFT_SEPARATOR) != 0;
        e->text = e->sep ? L"" : buf;
        MENUITEMINFOW up{}; up.cbSize = sizeof up; up.fMask = MIIM_FTYPE | MIIM_DATA;
        up.fType = MFT_OWNERDRAW | (e->sep ? MFT_SEPARATOR : 0);
        up.dwItemData = (ULONG_PTR)e.get();
        SetMenuItemInfoW(m, (UINT)i, TRUE, &up);
        g_menuEnts.push_back(std::move(e));
    }
}
static int trackMenu(HMENU m, UINT flags, int x, int y, HWND owner) {
    themeMenu(m);
    int r = TrackPopupMenu(m, flags, x, y, 0, owner, nullptr);
    g_menuEnts.clear();
    return r;
}
static void measureMenuItem(MEASUREITEMSTRUCT* mi) {
    const MenuEnt* e = (const MenuEnt*)mi->itemData;
    if (!e) return;
    if (e->sep) { mi->itemWidth = 10; mi->itemHeight = (UINT)S(9); return; }
    HDC dc = GetDC(hMain);
    HGDIOBJ of = SelectObject(dc, g_font);
    SIZE sz{0, 0}; GetTextExtentPoint32W(dc, e->text.c_str(), (int)e->text.size(), &sz);
    SelectObject(dc, of); ReleaseDC(hMain, dc);
    mi->itemWidth = (UINT)(sz.cx + S(52));
    mi->itemHeight = (UINT)S(27);
}
static void drawMenuItem(const DRAWITEMSTRUCT* d) {
    const Theme& t = T();
    const MenuEnt* e = (const MenuEnt*)d->itemData;
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    HBRUSH bg = CreateSolidBrush(g_dark ? t.banner : t.list); FillRect(dc, &r, bg); DeleteObject(bg);
    if (!e) return;
    if (e->sep) {
        HPEN pn = CreatePen(PS_SOLID, 1, t.border); HGDIOBJ op = SelectObject(dc, pn);
        int y = (r.top + r.bottom) / 2;
        MoveToEx(dc, r.left + S(10), y, nullptr); LineTo(dc, r.right - S(10), y);
        SelectObject(dc, op); DeleteObject(pn);
        return;
    }
    bool dis = (d->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0, sel = (d->itemState & ODS_SELECTED) != 0 && !dis, chk = (d->itemState & ODS_CHECKED) != 0;
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (sel) {
            Gdiplus::GraphicsPath p;
            roundedPath(p, (float)r.left + S(3), (float)r.top + 1.0f, (float)(r.right - r.left) - S(6), (float)(r.bottom - r.top) - 2.0f, (float)S(5));
            Gdiplus::SolidBrush sb(gc(t.btnHot));
            g.FillPath(&sb, &p);
            Gdiplus::Pen ep(gc(t.gold, 160), 1.0f);
            g.DrawPath(&ep, &p);
        }
        if (chk) {   // gold tick
            float cx = (float)r.left + S(16), cy = (float)(r.top + r.bottom) / 2.0f, s = (float)S(4);
            Gdiplus::Pen tp(gc(dis ? t.muted : t.gold), 2.0f * g_dpi / 96.0f);
            tp.SetStartCap(Gdiplus::LineCapRound); tp.SetEndCap(Gdiplus::LineCapRound); tp.SetLineJoin(Gdiplus::LineJoinRound);
            Gdiplus::PointF pts[3] = {{cx - s, cy}, {cx - s * 0.2f, cy + s * 0.8f}, {cx + s, cy - s * 0.8f}};
            g.DrawLines(&tp, pts, 3);
        }
    }
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ of = SelectObject(dc, g_font);
    SetTextColor(dc, dis ? mix(t.muted, t.banner, 30) : t.text);
    RECT tr = {r.left + S(34), r.top, r.right - S(12), r.bottom};
    DrawTextW(dc, e->text.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, of);
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
    {   // small author line under the subtitle
        SelectObject(dc, g_fontTiny);
        SetTextColor(dc, mix(t.muted, t.banner, 30));
        RECT ar = {sr.left, sr.top + S(14), sr.left + S(300), sr.top + S(25)};   // stays well above the gold line under the banner
        DrawTextW(dc, L"Author: Juincy", -1, &ar, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
    }
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

// Parts of the theme that switch at once (control styles, menu mode, title bar mode) and the colours, which can be blended.
static void applyThemeControls() {
    setMenuMode();
    BOOL dark = g_dark;
    if (FAILED(DwmSetWindowAttribute(hMain, 20, &dark, sizeof dark))) DwmSetWindowAttribute(hMain, 19, &dark, sizeof dark);
    const wchar_t* ctl = g_dark ? L"DarkMode_CFD" : nullptr;
    const wchar_t* exp = g_dark ? L"DarkMode_Explorer" : L"Explorer";
    for (HWND h : {hCombo, hFilter, hDir}) if (h) SetWindowTheme(h, ctl, nullptr);
    if (hList) SetWindowTheme(hList, exp, nullptr);
}
// caption: also update the title bar colour (Windows 11; an expensive call for the window manager, so frames in between skip it);
// frame: also repaint the non-client area.
static void applyThemeColors(bool otherWindows, bool caption = true, bool frame = true) {
    const Theme& t = T();
    rebuildBrushes();
    if (caption) themeFrameColors(hMain);
    if (hList) {
        ListView_SetBkColor(hList, t.list);
        ListView_SetTextBkColor(hList, t.list);
        ListView_SetTextColor(hList, t.text);
    }
    RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | (frame ? RDW_FRAME : 0));
    if (otherWindows) {
        if (HWND lg = FindWindowW(L"RCLog", nullptr)) SendMessageW(lg, WM_APP + 1, 0, 0);
        if (HWND cf = FindWindowW(L"RCConf", nullptr)) SendMessageW(cf, WM_APP + 1, 0, 0);
    }
}
static void applyTheme() {   // immediate (start-up)
    g_curTheme = g_dark ? DARK_T : LIGHT_T;
    applyThemeControls();
    applyThemeColors(true);
}

// ---------- smooth theme change ----------
// The colours of the whole window are blended from the old palette to the new one over a third of a second, frame by frame,
// while the theme switch slides. Everything is drawn with the blended colours, so nothing depends on screenshots or overlay windows.
static Theme g_themeFrom = DARK_T, g_themeTo = DARK_T;
static int g_animFrames = 0;
static bool g_animating = false, g_periodOn = false;
static double g_paintSum = 0, g_paintMax = 0;           // milliseconds spent painting, for the log
static double nowMs() { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
static void stepThemeAnim() {
    float p = (float)(GetTickCount64() - g_togStart) / (float)TOGGLE_MS;
    bool done = p >= 1.0f;
    float e = done ? 1.0f : easeInOut(p);
    g_togT = g_togFrom + (g_togTo - g_togFrom) * e;
    g_curTheme = done ? g_themeTo : lerpTheme(g_themeFrom, g_themeTo, e);
    g_animFrames++;
    double t0 = nowMs();
    applyThemeColors(done, done || (g_animFrames & 1) == 0, done);   // title bar colour every other frame; frame repaint only at the end
    double ms = nowMs() - t0;
    g_paintSum += ms; if (ms > g_paintMax) g_paintMax = ms;
    if (done) {
        KillTimer(hMain, TIMER_TOGGLE);
        g_animating = false;
        if (g_periodOn) { timeEndPeriod(1); g_periodOn = false; }
        logLine("theme change: " + std::to_string(g_animFrames) + " frames in " + std::to_string((unsigned long long)(GetTickCount64() - g_togStart)) + " ms, painting " +
                std::to_string((int)(g_paintSum / g_animFrames + 0.5)) + " ms per frame (slowest " + std::to_string((int)(g_paintMax + 0.5)) + " ms)");
    }
}
static void startThemeAnim() {   // call after g_dark was flipped
    if (g_togT < 0) g_togT = g_dark ? 0.0f : 1.0f;
    g_themeFrom = g_curTheme;
    g_themeTo = g_dark ? DARK_T : LIGHT_T;
    g_togFrom = g_togT; g_togTo = g_dark ? 1.0f : 0.0f;
    g_animFrames = 0; g_paintSum = 0; g_paintMax = 0;
    g_animating = true;
    if (!g_periodOn) { timeBeginPeriod(1); g_periodOn = true; }   // without this a 20 ms timer fires only every 31 ms
    applyThemeControls();
    g_togStart = GetTickCount64();     // the slow part above is not counted against the animation
    stepThemeAnim();
    if (g_animating) SetTimer(hMain, TIMER_TOGGLE, 4, nullptr);
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
static std::map<std::string, ModDefs> g_defIndex;    // mod id -> script definitions (events, common/ objects, localization keys)
static ScriptReport g_script;
static ConflictReport g_conf;
static std::string g_confSig;                         // which playset state g_conf was computed for
static bool g_scanning = false;
static int g_scanDone = 0, g_scanTotal = 0;
static HWND hConf = nullptr;                          // conflicts window, if open
static void confRefresh();
static bool copyToClipboard(const std::string& utf8);
static void info(const wchar_t* text);

static std::shared_ptr<VanillaIndex> g_vanilla;      // the base game's file list (null: not read yet, or the game folder was not found)
static bool g_vanillaFailed = false;                  // reading it failed or there is no game folder: not tried again until the game install or version changes
static std::string g_gameExeUsed;                     // the ck3.exe the version and file list belong to
static ULONGLONG g_lastDeepVerify = 0;                // when every enabled mod was last compared file by file with what we know

struct ScanItem { std::string id, dir, fp, knownDeep; };   // knownDeep: set when the mod is already indexed and only needs to be checked for changes
struct ScanJob {
    std::vector<ScanItem> items;
    std::vector<ModFiles> out;
    std::vector<ModDefs> defs;
    std::vector<char> unchanged, fromCache;
    std::string cacheDir, gameExe, gameVer;
    bool wantVanilla = false, quiet = false;
    std::shared_ptr<VanillaIndex> vanilla;
    bool vanillaFromCache = false;
    ULONGLONG ms = 0;
    std::atomic<bool> cancel{false};
    HWND notify = nullptr;
};
static ScanJob* g_job = nullptr;
static std::map<std::string, std::string> g_scanFailed;   // mod id -> fingerprint of a folder that could not be read completely (not retried until it changes)
static HANDLE g_jobThread = nullptr;

static DWORD WINAPI scanThread(LPVOID p) {
    ScanJob* j = (ScanJob*)p;
    ULONGLONG t0 = GetTickCount64();
    j->out.resize(j->items.size());
    j->defs.resize(j->items.size());
    j->unchanged.assign(j->items.size(), 0);
    j->fromCache.assign(j->items.size(), 0);
    try {
        if (j->wantVanilla && !j->cancel.load()) j->vanilla = loadOrIndexVanilla(j->cacheDir, j->gameExe, j->gameVer, &j->cancel, &j->vanillaFromCache);
        for (size_t i = 0; i < j->items.size() && !j->cancel.load(); i++) {
            const ScanItem& it = j->items[i];
            bool done = false;
            if (!it.knownDeep.empty()) {   // already indexed: only look at whether anything changed (list the folder, no file is read)
                ModFiles mf = indexModFiles(it.dir, &j->cancel);
                if (mf.complete && mf.deep == it.knownDeep) { j->unchanged[i] = 1; done = true; }
            }
            if (!done) {
                IndexedMod im = indexModCached(j->cacheDir, it.id, it.dir, &j->cancel);
                j->out[i] = std::move(im.files);
                j->out[i].fingerprint = it.fp;
                if (j->out[i].complete) { j->defs[i] = std::move(im.defs); j->defs[i].fingerprint = it.fp; j->fromCache[i] = im.fromCache ? 1 : 0; }
            }
            PostMessageW(j->notify, WM_APP + 2, (WPARAM)(i + 1), (LPARAM)j->items.size());
        }
    } catch (...) {   // never let an exception end the program from a worker thread; the mods not finished simply stay unread
        for (auto& o : j->out) if (!o.complete) o.files.clear();
    }
    j->ms = GetTickCount64() - t0;
    PostMessageW(j->notify, WM_APP + 3, (WPARAM)j, 0);
    return 0;
}

static std::string playsetSignature(const Playset& ps) {
    std::string s = ps.name;
    for (auto& m : ps.mods) if (m.enabled) { s += '\n'; s += m.id; }
    s += "\n#"; if (g_vanilla) s += g_vanilla->key;      // the report also depends on the game's file list
    return s;
}

// ---------- computing the conflict report ----------
// Small playsets are computed on the spot. A big one (hundreds of thousands of files) is computed on a worker thread so the window
// never stops answering; while it runs, the old report is dropped (its positions would be wrong for the new order) and the
// rows simply have no conflict notes for a moment. The worker only READS g_fileIndex / g_defIndex, so everything that changes
// them first calls confQuiesce().
struct ConfJob {
    unsigned id = 0;
    Playset ps;
    std::map<std::string, ModInfo> info;
    std::shared_ptr<VanillaIndex> van;
    std::string sig;
    ConflictReport conf;
    ScriptReport script;
    std::atomic<bool> cancel{false};
    HWND notify = nullptr;
    ULONGLONG ms = 0;
    std::string err;           // set when the worker failed
};
static ConfJob* g_confJob = nullptr;
static HANDLE g_confThread = nullptr;
static unsigned g_confJobSeq = 0;
static bool g_forceAsyncConf = false;                 // test hook (RC_ASYNC_CONF=1): always use the worker thread
static const size_t CONF_ASYNC_FILES = 150000;        // above this many files in the enabled mods the report is computed on a worker thread

static DWORD WINAPI confThread(LPVOID p) {
    ConfJob* j = (ConfJob*)p;
    ULONGLONG t0 = GetTickCount64();
    try {
        j->conf = findConflicts(j->ps, j->info, g_fileIndex, j->van.get(), &j->cancel);
        if (!j->cancel.load()) j->script = findScriptConflicts(j->ps, j->info, g_defIndex);
    } catch (const std::exception& e) { j->conf = ConflictReport(); j->script = ScriptReport(); j->err = e.what(); }
    catch (...) { j->conf = ConflictReport(); j->script = ScriptReport(); j->err = "unknown error"; }
    j->ms = GetTickCount64() - t0;
    PostMessageW(j->notify, WM_APP + 9, (WPARAM)j->id, 0);
    return 0;
}
static void confQuiesce() {
    if (!g_confJob) return;
    g_confJob->cancel = true;
    if (g_confThread) { WaitForSingleObject(g_confThread, INFINITE); CloseHandle(g_confThread); g_confThread = nullptr; }
    delete g_confJob; g_confJob = nullptr;
    g_confSig.clear(); g_conf = ConflictReport(); g_script = ScriptReport();
}
static bool vanillaPending() { return !g_vanilla && !g_vanillaFailed && !g_gameExeUsed.empty() && g_scanning; }

// Rebuilds the conflict report from the cached file lists when the enabled mods or their order changed (no disk access).
static void ensureConflicts() {
    Playset* ps = active();
    if (!ps) return;
    std::string sig = playsetSignature(*ps);
    if (sig == g_confSig && (g_conf.valid || g_scanning || g_confJob)) return;
    if (g_confJob) {   // a result for an older state is useless: stop it, and ask again when it has ended
        g_confJob->cancel = true;
        g_confSig.clear(); g_conf = ConflictReport(); g_script = ScriptReport();
        return;
    }
    bool ready = !vanillaPending();
    if (ready) for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        auto it = g_info.find(m.id);
        if (it == g_info.end() || it->second.contentState != 1) continue;
        auto f = g_fileIndex.find(m.id);
        if (f == g_fileIndex.end() || !f->second.complete) {
            if (auto bad = g_scanFailed.find(m.id); bad != g_scanFailed.end() && bad->second == modFingerprint(it->second)) continue;   // unreadable folder: skipped, not waited for
            ready = false; break;
        }
    }
    bool defsReady = ready;
    if (ready) for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        auto it = g_info.find(m.id);
        if (it == g_info.end() || it->second.contentState != 1) continue;
        auto d = g_defIndex.find(m.id);
        if (d == g_defIndex.end() || !d->second.complete) {
            if (auto bad = g_scanFailed.find(m.id); bad != g_scanFailed.end() && bad->second == modFingerprint(it->second)) continue;
            defsReady = false; break;
        }
    }
    g_confSig = sig;
    if (!ready) { g_conf = ConflictReport(); g_script = ScriptReport(); confRefresh(); return; }
    size_t total = 0;
    for (auto& m : ps->mods) if (m.enabled) { auto f = g_fileIndex.find(m.id); if (f != g_fileIndex.end()) total += f->second.files.size(); }
    if (total > CONF_ASYNC_FILES || g_forceAsyncConf) {
        auto* j = new ConfJob();
        j->id = ++g_confJobSeq; j->ps = *ps; j->info = g_info; j->van = g_vanilla; j->sig = sig; j->notify = hMain;
        g_confJob = j;
        logLine("conflicts: " + std::to_string(total) + " files, computing on a worker thread");
        g_confThread = CreateThread(nullptr, 0, confThread, j, 0, nullptr);
        if (g_confThread) { g_conf = ConflictReport(); g_script = ScriptReport(); confRefresh(); return; }
        g_confJob = nullptr; delete j;      // no thread: do it here after all
    }
    g_conf = findConflicts(*ps, g_info, g_fileIndex, g_vanilla.get());
    g_script = defsReady ? findScriptConflicts(*ps, g_info, g_defIndex) : ScriptReport();
    confRefresh();
}

// Starts a background scan for enabled mods that have no file list yet, and (when verify is set) checks the others for changes.
// A mod that is already indexed is only listed again, not read, unless something in it changed.
static void maybeScan(bool verify) {
    if (g_job) return;
    Playset* ps = active();
    if (!ps) return;
    ULONGLONG now = GetTickCount64();
    bool deepDue = verify && now - g_lastDeepVerify > 120000;   // Steam can replace files deep inside a mod without the mod's folder showing it
    auto* job = new ScanJob();
    for (auto& m : ps->mods) {
        if (!m.enabled) continue;
        auto it = g_info.find(m.id);
        if (it == g_info.end() || it->second.contentState != 1) continue;
        std::string fp = modFingerprint(it->second);
        if (auto bad = g_scanFailed.find(m.id); bad != g_scanFailed.end() && bad->second == fp) continue;   // failed before and nothing changed since
        auto f = g_fileIndex.find(m.id);
        auto d = g_defIndex.find(m.id);
        bool have = f != g_fileIndex.end() && f->second.complete && d != g_defIndex.end() && d->second.complete;
        if (have) {
            if (!verify || (f->second.fingerprint == fp && !deepDue)) continue;
            job->items.push_back({m.id, it->second.contentDir, fp, f->second.deep});
        } else job->items.push_back({m.id, it->second.contentDir, fp, ""});
    }
    bool wantVan = !g_vanilla && !g_vanillaFailed && !g_gameExeUsed.empty();
    if (job->items.empty() && !wantVan) { delete job; ensureConflicts(); return; }
    bool quiet = !wantVan;
    for (auto& it : job->items) if (it.knownDeep.empty()) quiet = false;   // pure change checks run silently
    job->notify = hMain;
    job->cacheDir = dataDir() + "/cache";
    job->gameExe = g_gameExeUsed; job->gameVer = g_gameVer;
    job->wantVanilla = wantVan; job->quiet = quiet;
    g_job = job;
    if (deepDue) g_lastDeepVerify = now;
    g_scanning = !quiet; g_scanDone = 0; g_scanTotal = (int)job->items.size();
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
    else if (g_confJob) t += L"  |  calculating conflicts";
    else if (g_conf.valid && !g_conf.files.empty()) t += L"  |  " + std::to_wstring(g_conf.files.size()) + L" file conflicts" + (g_script.valid && !g_script.items.empty() ? L", " + std::to_wstring(g_script.items.size()) + L" script overlaps" : L"");
    if (!g_scanning && !g_confJob && g_conf.valid && g_conf.vanillaChecked) {
        int vm = 0; for (int c : g_conf.vanillaCount) if (c > 0) vm++;
        if (vm > 0) t += L"  |  " + std::to_wstring(vm) + L" replace base-game files";
    }
    if (old > 0) t += L"  |  " + std::to_wstring(old) + L" may be outdated";
    SetWindowTextW(hCount, t.c_str());
}

static bool g_inSizeMove = false;
static bool g_windowReady = false;     // set once the window is shown; before that nothing about the window is saved
// Remembers where the window is, how big it is and how wide the columns are (written to settings.json straight away,
// so it survives however the program ends).
static void saveWindowState(HWND h) {
    if (!g_windowReady || g_inSizeMove || !hList || !IsWindow(hList)) return;
    WINDOWPLACEMENT wp{}; wp.length = sizeof wp;
    if (!GetWindowPlacement(h, &wp)) return;
    const RECT& n = wp.rcNormalPosition;
    if (n.right - n.left < 200 || n.bottom - n.top < 200) return;
    Settings& st = g_settings;
    std::vector<int> cols;
    for (int c = 0; c < 7; c++) cols.push_back(ListView_GetColumnWidth(hList, c));
    bool max = wp.showCmd == SW_SHOWMAXIMIZED;
    if (st.winX == n.left && st.winY == n.top && st.winW == n.right - n.left && st.winH == n.bottom - n.top && st.winMax == max && st.colW == cols) return;
    st.winX = n.left; st.winY = n.top; st.winW = n.right - n.left; st.winH = n.bottom - n.top; st.winMax = max; st.colW = cols;
    saveSettings(st);
}
static void resizeCols();
// Fingerprints of the installed mods right now (cleared when the mod folder is rescanned).
static std::map<std::string, std::string> g_fpNow;
static const std::map<std::string, std::string>& fpNow() {
    if (g_fpNow.empty()) for (auto& kv : g_info) if (!kv.second.contentDir.empty()) {
        std::string fp = modFingerprint(kv.second);
        auto f = g_fileIndex.find(kv.first);
        if (f != g_fileIndex.end() && f->second.complete && !f->second.deep.empty()) fp += "#d:" + f->second.deep;   // indexed: every file's name, size and time count
        g_fpNow[kv.first] = fp;
    }
    return g_fpNow;
}
static std::wstring noteText(int i, char& sev) {
    Playset* ps = active();
    const auto& iss = g_issues[(size_t)i];
    sev = (char)issueSeverity(iss);
    std::string sum = issueSummary(iss);
    if (!sum.empty()) return W(sum);
    if (ps) if (auto pit = g_info.find(ps->mods[(size_t)i].id); pit != g_info.end() && pit->second.pending) return L"Downloaded from Steam. Tick the box to add it";
    if (ps && ps->mods[(size_t)i].enabled && g_info.count(ps->mods[(size_t)i].id)) { sev = 3; return L"\u2713"; }  // 3 = checked, all fine
    return L"";
}

// ---------- auto sort state ----------
static std::vector<KnownMod> g_known;           // built-in known mods + downloaded list + knownmods.json
static int g_knownRev = 0;                      // revision of the downloaded list in use (0 = none)
static std::vector<std::string> g_undoIds;     // load order (mod ids) before the last Auto Sort
static std::string g_undoPlayset;
static std::vector<int> g_ctxSel;               // playset positions the context menu acts on (the clicked row alone, or the whole selection)
static int g_ctxIdx = -1;                      // playset position of the row the context menu was opened on
static const int ID_CTX_LOCK = 700, ID_CTX_CAT = 710;   // ID_CTX_CAT + category; ID_CTX_CAT + CAT_COUNT = automatic

static std::set<std::string>& lockedIds() { static std::set<std::string> none; Playset* ps = active(); return ps ? g_settings.locks[ps->name] : none; }
static int catOfMod(const std::string& id, bool* overridden = nullptr) {
    auto ov = g_settings.cats.find(id);
    if (overridden) *overridden = ov != g_settings.cats.end();
    if (ov != g_settings.cats.end()) return ov->second;
    auto it = g_info.find(id);
    if (it == g_info.end()) return (int)CAT_CONTENT;
    CatGuess g = guessCategory(it->second);
    if (g.why == "default") {
        auto f = g_fileIndex.find(id);
        if (f != g_fileIndex.end()) {   // looking through a big mod's whole file list on every redraw is slow: remembered until its index changes
            static std::map<std::string, std::pair<std::string, CatGuess>> cache;
            std::string key = f->second.fingerprint + "#" + std::to_string(f->second.files.size()) + (f->second.complete ? "c" : "i");
            auto& c = cache[id];
            if (c.first != key) c = {key, guessFromFiles(f->second)};
            g = c.second;
        }
    }
    int k = findKnown(g_known, it->second);
    if (k >= 0 && g_known[(size_t)k].cat >= 0) g.cat = g_known[(size_t)k].cat;
    return g.cat;
}
// Symbolic links and junctions: a mod folder that is one must never be erased through (its target could be anywhere).
static bool isReparsePoint(const fs::path& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
// A mod that is removed or deleted disappears from every playset, so every playset that has it is backed up first.
static void backupAffected(const std::vector<std::string>& ids, const std::string& reason) {
    std::set<std::string> want(ids.begin(), ids.end());
    for (auto& pl : g_playsets) {
        bool has = false;
        for (auto& m : pl.mods) if (want.count(m.id)) { has = true; break; }
        if (has) backupPlayset(pl, g_info, reason);
    }
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
    if (g_conf.valid) addConflictIssues(g_issues, g_conf, *ps, g_info, g_gameVer);
    if (g_script.valid) addScriptIssues(g_issues, g_script, *ps);
    addUpdateIssues(g_issues, *ps, g_info, g_settings.seen, fpNow());
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
static std::vector<std::string> workshopDirs();
static void refreshGameVersion(const std::string& exe) {
    std::string ver = readGameVersion(exe);
    if (exe != g_gameExeUsed || ver != g_gameVer) {   // another install or a game update: the base game's file list is read again
        confQuiesce();
        g_vanilla.reset(); g_vanillaFailed = false; g_confSig.clear();
    }
    g_gameExeUsed = exe;
    g_gameVer = ver;
    SetWindowTextW(hGameVer, g_gameVer.empty() ? L"CK3 version: unknown" : (L"CK3 version: " + W(g_gameVer)).c_str());
}

// Changes whenever a mod descriptor appears in the mod folder or a new Workshop download appears/finishes in Steam's folder.
static long long modDirStamp() {
    std::string dir = effectiveDir();
    if (dir.empty()) return 0;
    std::error_code ec;
    long long best = 0;
    auto t = fs::last_write_time(P(dir) / "mod", ec);
    if (!ec) best = (long long)t.time_since_epoch().count();
    for (auto& wd : workshopDirs()) {
        std::error_code e2;
        auto tw = fs::last_write_time(P(wd), e2);
        if (e2) continue;
        best = std::max(best, (long long)tw.time_since_epoch().count());
        for (auto& e : fs::directory_iterator(P(wd), e2)) {
            auto tc = fs::last_write_time(e.path(), e2);
            if (!e2) best = std::max(best, (long long)tc.time_since_epoch().count());
            e2.clear();
        }
    }
    return best;
}
static long long g_modStamp = 0;
static int g_hiddenPresent = 0;   // installed mods that are not shown because they were removed from the list

static void reload() {
    std::error_code gec;
    refreshGameVersion((!g_settings.gameExe.empty() && fs::exists(P(g_settings.gameExe), gec)) ? g_settings.gameExe : findGameExeInSteam());
    std::string dir = effectiveDir();
    g_mods = scanMods(dir);
    {
        std::set<std::string> have;
        for (auto& m : g_mods) have.insert(m.id);
        auto pend = scanWorkshopFolders(workshopDirs(), have);
        for (auto& m : pend) g_mods.push_back(std::move(m));
        if (!g_mods.empty()) std::sort(g_mods.begin(), g_mods.end(), [](const ModInfo& a, const ModInfo& b) { return lower(a.name) < lower(b.name); });
    }
    g_hiddenPresent = 0;
    for (auto& m : g_mods) if (g_settings.hidden.count(m.id)) g_hiddenPresent++;
    if (!g_settings.hidden.empty()) g_mods.erase(std::remove_if(g_mods.begin(), g_mods.end(), [](const ModInfo& m) { return g_settings.hidden.count(m.id) > 0; }), g_mods.end());
    g_fpNow.clear();
    g_known = loadKnownMods(&g_knownRev);
    g_info = infoMap(g_mods);
    int migrated = migrateLegacyStore(g_mods, g_settings);
    g_playsets = loadPlaysets(g_mods);
    if (!g_settings.hidden.empty())
        for (auto& pl : g_playsets) pl.mods.erase(std::remove_if(pl.mods.begin(), pl.mods.end(), [](const ModRef& m) { return g_settings.hidden.count(m.id) > 0; }), pl.mods.end());
    if (g_playsets.empty()) {
        Playset d;
        d.name = "Default";
        syncPlayset(d, g_mods);
        savePlayset(d, g_info);
        g_playsets.push_back(d);
    }
    if (!active()) g_settings.active = g_playsets[0].name;
    saveSettingsNow();
    { static bool startupDone = false; if (!startupDone) { startupDone = true; if (Playset* a = active()) backupPlayset(*a, g_info, "startup"); } }
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
    if (g_conf.valid) addConflictIssues(g_issues, g_conf, *ps, g_info, g_gameVer);
    if (g_script.valid) addScriptIssues(g_issues, g_script, *ps);
    addUpdateIssues(g_issues, *ps, g_info, g_settings.seen, fpNow());
    g_populating = true;
    for (size_t r = 0; r < g_shown.size(); r++) {
        char nsev = 0;
        std::wstring note = noteText(g_shown[r], nsev);
        ListView_SetItemText(hList, (int)r, 6, (LPWSTR)note.c_str());
        {
            const std::string& mid = ps->mods[(size_t)g_shown[r]].id;
            bool ovr = false;
            std::wstring ty = g_info.count(mid) ? W(catName(catOfMod(mid, &ovr))) + (ovr ? L" *" : L"") : L"";
            ListView_SetItemText(hList, (int)r, 5, (LPWSTR)ty.c_str());
        }
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
            (findKnown(g_known, mi) >= 0 ? "\nKnown mod: " + g_known[(size_t)findKnown(g_known, mi)].note : "") +
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
    int pend = 0;
    for (auto& m : g_mods) if (m.pending) pend++;
    std::wstring msg = std::wstring(automatic ? L"The mod folder changed, list refreshed: " : L"Rescanned: ") + std::to_wstring(g_mods.size()) + L" mods installed";
    if (added) msg += L", " + std::to_wstring(added) + L" new (added at the end of your playsets, disabled)";
    if (gone) msg += L", " + std::to_wstring(gone) + L" removed";
    msg += L".";
    if (pend) msg += L" " + std::to_wstring(pend) + L" from Steam are not registered by the Paradox launcher yet (Play registers them).";
    if (g_hiddenPresent) msg += L" " + std::to_wstring(g_hiddenPresent) + L" hidden because you removed them (Advanced > Show removed mods).";
    if (!automatic && !added) msg += L" Nothing new found: if you just subscribed, wait until Steam has finished downloading the mod, then rescan.";
    say(msg);
    logLine("rescan: " + std::to_string(g_mods.size()) + " mods, " + std::to_string(added) + " new, " + std::to_string(pend) + " pending, " + std::to_string(g_hiddenPresent) + " hidden");
    g_resyncing = false;
}

static void resizeCols() {
    RECT r; GetClientRect(hList, &r);
    int fixed = 0;
    for (int c : {0, 2, 3, 4, 5}) fixed += ListView_GetColumnWidth(hList, c);
    int notes = S(280);
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

// Playset positions of all selected rows, in list order.
static std::vector<int> selectedMods() {
    std::vector<int> v;
    for (int r = ListView_GetNextItem(hList, -1, LVNI_SELECTED); r >= 0; r = ListView_GetNextItem(hList, r, LVNI_SELECTED))
        if (r < (int)g_shown.size()) v.push_back(g_shown[(size_t)r]);
    return v;
}
// Moves every selected mod one step up or down (a block stays together; unselected mods hop over it).
static void moveSelection(bool up) {
    Playset* ps = active();
    if (!ps) return;
    std::vector<int> sel = selectedMods();
    if (sel.empty()) { say(L"Select a mod first."); return; }
    std::vector<char> isSel(ps->mods.size(), 0);
    for (int i : sel) isSel[(size_t)i] = 1;
    int n = (int)ps->mods.size();
    bool moved = false;
    if (up) { for (int i = 1; i < n; i++) if (isSel[(size_t)i] && !isSel[(size_t)i - 1]) { std::swap(ps->mods[(size_t)i], ps->mods[(size_t)i - 1]); std::swap(isSel[(size_t)i], isSel[(size_t)i - 1]); moved = true; } }
    else { for (int i = n - 2; i >= 0; i--) if (isSel[(size_t)i] && !isSel[(size_t)i + 1]) { std::swap(ps->mods[(size_t)i], ps->mods[(size_t)i + 1]); std::swap(isSel[(size_t)i], isSel[(size_t)i + 1]); moved = true; } }
    if (!moved) return;
    saveActive();
    populate();
    ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
    int first = -1;
    for (int i = 0; i < n; i++) if (isSel[(size_t)i]) { ListView_SetItemState(hList, i, LVIS_SELECTED, LVIS_SELECTED); if (first < 0) first = i; }
    if (first >= 0) ListView_EnsureVisible(hList, first, FALSE);
}

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
    while (!ctx.done) {
        BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) { PostQuitMessage((int)msg.wParam); break; }   // the program is being closed: pass it on to the main loop
        if (got < 0) break;
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
    if (plan.knownCount > 0) sum += L" " + std::to_wstring(plan.knownCount) + L" mod(s) recognised from the known-mods list.";
    if (plan.patchLinks > 0) sum += L" " + std::to_wstring(plan.patchLinks) + L" patch(es) placed after the mods they are for.";
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
    while (!ctx.done) {
        BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) { PostQuitMessage((int)msg.wParam); break; }   // the program is being closed: pass it on to the main loop
        if (got < 0) break;
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

// ---------- simple text window (used for the playset comparison) ----------
static LRESULT CALLBACK TextProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_SIZE: { RECT r; GetClientRect(h, &r); MoveWindow(GetDlgItem(h, 1), S(8), S(8), r.right - S(16), r.bottom - S(16), TRUE); return 0; }
        case WM_CLOSE: DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static void showText(const std::wstring& title, const std::wstring& text) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = TextProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCText";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    HWND w = CreateWindowExW(0, L"RCText", title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, S(720), S(520), hMain, nullptr, g_inst, nullptr);
    HWND e = CreateWindowExW(0, L"EDIT", text.c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 10, 10, w, (HMENU)1, g_inst, nullptr);
    setFont(e);
    themeFrame(w);
    RECT r; GetClientRect(w, &r);
    MoveWindow(e, S(8), S(8), r.right - S(16), r.bottom - S(16), TRUE);
    ShowWindow(w, SW_SHOW);
}

// ---------- conflicts window ----------
static int g_cfPairA = -1, g_cfPairB = -1;           // set when a pair row was opened: the file list shows only their shared files
static std::vector<int> g_cfRows;                    // visible rows: indexes into pairs / files / definitions of the open view
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
    if (g_cfView == 0) { cols.push_back({L"Severity", 80}); cols.push_back({L"Loses (loads earlier)", 270}); cols.push_back({L"Wins (loads later)", 270}); cols.push_back({L"Files", 60}); cols.push_back({L"Where", 400}); }
    else if (g_cfView == 1) { cols.push_back({L"Severity", 80}); cols.push_back({L"File", 440}); cols.push_back({L"Winner (loads last)", 270}); cols.push_back({L"Also in", 380}); cols.push_back({L"Base game", 90}); }
    else if (g_cfView == 3) { cols.push_back({L"Severity", 80}); cols.push_back({L"Base-game file that mods replace", 470}); cols.push_back({L"Replaced by (load order, last one is used)", 520}); }
    else { cols.push_back({L"Severity", 80}); cols.push_back({L"Type", 110}); cols.push_back({L"Area", 160}); cols.push_back({L"Name", 240}); cols.push_back({L"Defined by (load order)", 340}); cols.push_back({L"What happens", 400}); }
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
    auto sevHit = [&](int sev) { return !f.empty() && lower(sevName(sev)).find(f) != std::string::npos; };
    if (g_conf.valid) {
        if (g_cfView == 0) {
            for (size_t i = 0; i < g_conf.pairs.size(); i++) {
                const auto& p = g_conf.pairs[i];
                if (!f.empty()) {
                    bool hit = sevHit(p.sev) || nm(p.a).find(f) != std::string::npos || nm(p.b).find(f) != std::string::npos;
                    for (auto& a : p.areas) if (!hit && a.first.find(f) != std::string::npos) hit = true;
                    if (!hit) continue;
                }
                g_cfRows.push_back((int)i);
            }
        } else if (g_cfView == 1) {
            for (size_t i = 0; i < g_conf.files.size(); i++) {
                const auto& fc = g_conf.files[i];
                if (g_cfPairA >= 0 && (std::find(fc.mods.begin(), fc.mods.end(), g_cfPairA) == fc.mods.end() || std::find(fc.mods.begin(), fc.mods.end(), g_cfPairB) == fc.mods.end())) continue;
                if (!f.empty() && !sevHit(fc.sev) && fc.path.find(f) == std::string::npos && nm(fc.mods.back()).find(f) == std::string::npos) continue;
                g_cfRows.push_back((int)i);
            }
        } else if (g_cfView == 3) {
            for (size_t i = 0; i < g_conf.vanillaWipes.size(); i++) {   // replace_path folders first: they remove whole folders of the base game
                const auto& vw = g_conf.vanillaWipes[i];
                if (!f.empty() && !sevHit(SEV_HIGH) && vw.folder.find(f) == std::string::npos && nm(vw.mod).find(f) == std::string::npos && std::string("replace_path").find(f) == std::string::npos) continue;
                g_cfRows.push_back((int)(g_conf.vanilla.size() + i));
            }
            for (size_t i = 0; i < g_conf.vanilla.size(); i++) {
                const auto& vf = g_conf.vanilla[i];
                if (!f.empty() && !sevHit(vf.sev) && vf.path.find(f) == std::string::npos) {
                    bool hit = false;
                    for (int m : vf.mods) if (nm(m).find(f) != std::string::npos) { hit = true; break; }
                    if (!hit) continue;
                }
                g_cfRows.push_back((int)i);
            }
        }
    }
    if (g_cfView == 2 && g_script.valid) {
        for (size_t i = 0; i < g_script.items.size(); i++) {
            const auto& sc = g_script.items[i];
            if (!f.empty()) {
                bool hit = sevHit(sc.sev ? SEV_MED : SEV_LOW) || lower(sc.key).find(f) != std::string::npos || lower(sc.area).find(f) != std::string::npos || lower(defKindName(sc.kind)).find(f) != std::string::npos;
                for (size_t x = 0; x < sc.hits.size() && !hit; x++) hit = nm(sc.hits[x].mod).find(f) != std::string::npos;
                if (!hit) continue;
            }
            g_cfRows.push_back((int)i);
        }
    }
    ListView_SetItemCountEx(hCfList, (int)g_cfRows.size(), LVSICF_NOINVALIDATEALL);
    InvalidateRect(hCfList, nullptr, TRUE);
    std::wstring st;
    if (g_scanning) st = L"Reading mod files... " + std::to_wstring(g_scanDone) + L"/" + std::to_wstring(g_scanTotal);
    else if (g_confJob) st = L"Calculating conflicts...";
    else if (g_cfView == 2) {
        if (!g_script.valid) st = L"No script data yet. Enable some mods that have a files folder, then press Rescan files.";
        else {
            int cnt[DK_KINDS] = {0};
            for (auto& sc : g_script.items) cnt[sc.kind]++;
            st = std::to_wstring(g_script.items.size()) + L" overlaps  |  " + std::to_wstring(cnt[DK_EVENT]) + L" event IDs  |  " + std::to_wstring(cnt[DK_COMMON] + cnt[DK_DEFINE]) +
                 L" definitions/defines  |  " + std::to_wstring(cnt[DK_ONACTION]) + L" on_actions  |  " + std::to_wstring(cnt[DK_LOC]) + L" localization keys.  Double-click a row for the files.";
            if (g_script.modsMissing) st += L"  (" + std::to_wstring(g_script.modsMissing) + L" mods not read)";
        }
    }
    else if (!g_conf.valid) st = L"No file data yet. Enable some mods that have a files folder, then press Rescan files.";
    else if (g_cfView == 3) {
        if (!g_conf.vanillaChecked) st = L"The game's own files could not be read (is the game installed? Play once, or set the game in Advanced), so mods cannot be compared with them.";
        else {
            int mods = 0; for (int c : g_conf.vanillaCount) if (c > 0) mods++;
            st = std::to_wstring(g_conf.vanilla.size()) + L" base-game files are replaced by " + std::to_wstring(mods) + L" mods";
            if (!g_conf.vanillaWipes.empty()) st += L"  |  " + std::to_wstring(g_conf.vanillaWipes.size()) + L" replace_path folders remove base-game files";
            st += L".  Double-click a row for who wins, right-click to reorder.";
        }
    }
    else {
        st = std::to_wstring(g_conf.files.size()) + L" conflicting files  |  " + std::to_wstring(g_conf.pairs.size()) + L" mod pairs  |  " +
             std::to_wstring(g_conf.modsIndexed) + L" mods checked";
        if (g_conf.modsWithoutFiles) st += L" (" + std::to_wstring(g_conf.modsWithoutFiles) + L" skipped: no readable files folder)";
        if (g_cfView == 1 && g_cfPairA >= 0) st = L"Files shared by " + modLabel(g_cfPairA) + L"  and  " + modLabel(g_cfPairB) + L"  (press \"By file\" to see all)  |  " + std::to_wstring(g_cfRows.size()) + L" files";
        else if (g_cfView == 0) st += L".  The later mod wins. Double-click a pair for its files, right-click to reorder.";
        else st += L".  Double-click a file for who wins.";
    }
    SetWindowTextW(hCfStatus, st.c_str());
}

static void cfEmptyRows();
static void confRefresh() { if (hConf) { cfEmptyRows(); cfRebuildRows(); } }

// Severity of a row of the open view (-1: none).
static int cfRowSev(int row) {
    if (row < 0 || row >= (int)g_cfRows.size()) return -1;
    size_t idx = (size_t)g_cfRows[(size_t)row];
    if (g_cfView == 0) return idx < g_conf.pairs.size() ? g_conf.pairs[idx].sev : -1;
    if (g_cfView == 1) return idx < g_conf.files.size() ? g_conf.files[idx].sev : -1;
    if (g_cfView == 3) return idx < g_conf.vanilla.size() ? g_conf.vanilla[idx].sev : idx - g_conf.vanilla.size() < g_conf.vanillaWipes.size() ? (int)SEV_HIGH : -1;
    return idx < g_script.items.size() ? (g_script.items[idx].sev ? (int)SEV_MED : (int)SEV_LOW) : -1;
}

static const wchar_t* cfCell(int row, int sub) {
    if (row < 0 || row >= (int)g_cfRows.size()) return L"";
    size_t idx = (size_t)g_cfRows[(size_t)row];
    g_cfText.clear();
    if (sub == 0) { int sv = cfRowSev(row); return sv < 0 ? L"" : sv >= SEV_HIGH ? L"High" : sv == SEV_MED ? L"Medium" : L"Low"; }
    sub--;
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
    } else if (g_cfView == 2) {
        if (idx >= g_script.items.size()) return L"";
        const auto& sc = g_script.items[idx];
        if (sub == 0) g_cfText = W(defKindName(sc.kind));
        else if (sub == 1) g_cfText = W(sc.area);
        else if (sub == 2) g_cfText = W(sc.key);
        else if (sub == 3) {
            std::set<int> seen;
            for (auto& h : sc.hits) if (seen.insert(h.mod).second) g_cfText += (g_cfText.empty() ? L"" : L";  ") + modLabel(h.mod);
        } else {
            g_cfText = W(sc.note);
            if (sc.winner >= 0) g_cfText += L"  (" + modLabel(sc.winner) + L")";
        }
    } else if (g_cfView == 3) {
        if (idx >= g_conf.vanilla.size()) {
            size_t wi = idx - g_conf.vanilla.size();
            if (wi >= g_conf.vanillaWipes.size()) return L"";
            const auto& vw = g_conf.vanillaWipes[wi];
            g_cfText = sub == 0 ? W(vw.folder) + L"/   (the whole folder: " + std::to_wstring(vw.files) + L" base-game files removed)" : modLabel(vw.mod) + L"   (replace_path)";
            return g_cfText.c_str();
        }
        const auto& vf = g_conf.vanilla[idx];
        if (sub == 0) g_cfText = W(vf.path);
        else {
            for (size_t i = 0; i < vf.mods.size() && i < 5; i++) g_cfText += (i ? L";  " : L"") + modLabel(vf.mods[i]);
            if (vf.mods.size() > 5) g_cfText += L"  +" + std::to_wstring(vf.mods.size() - 5) + L" more";
        }
    } else {
        if (idx >= g_conf.files.size()) return L"";
        const auto& fc = g_conf.files[idx];
        if (sub == 0) g_cfText = W(fc.path);
        else if (sub == 1) g_cfText = modLabel(fc.mods.back());
        else if (sub == 2) {
            for (size_t i = 0; i + 1 < fc.mods.size() && i < 4; i++) g_cfText += (i ? L";  " : L"") + modLabel(fc.mods[i]);
            if (fc.mods.size() > 5) g_cfText += L"  +" + std::to_wstring(fc.mods.size() - 5) + L" more";
        } else g_cfText = fc.vanilla ? L"yes" : L"";
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
    for (int id : {ID_CF_PAIR, ID_CF_FILE, ID_CF_DEF, ID_CF_VAN}) InvalidateRect(GetDlgItem(hConf, id), nullptr, FALSE);
}

static void cfLayout() {
    RECT r; GetClientRect(hConf, &r);
    int m = S(12), y = m, x = m, bh = S(30), gap = S(6);
    MoveWindow(GetDlgItem(hConf, ID_CF_PAIR), x, y, S(140), bh, TRUE); x += S(140) + gap;
    MoveWindow(GetDlgItem(hConf, ID_CF_FILE), x, y, S(110), bh, TRUE); x += S(110) + gap;
    MoveWindow(GetDlgItem(hConf, ID_CF_DEF), x, y, S(150), bh, TRUE); x += S(150) + gap;
    MoveWindow(GetDlgItem(hConf, ID_CF_VAN), x, y, S(130), bh, TRUE); x += S(130) + gap;
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

// "who wins" for the row of the open view: the mods that ship it, in load order (the last one's copy is used).
static std::vector<int> cfRowMods(int row, std::wstring* what = nullptr) {
    std::vector<int> v;
    if (row < 0 || row >= (int)g_cfRows.size()) return v;
    size_t idx = (size_t)g_cfRows[(size_t)row];
    if (g_cfView == 0 && idx < g_conf.pairs.size()) { v = {g_conf.pairs[idx].a, g_conf.pairs[idx].b}; if (what) *what = L"Files both mods ship"; }
    else if (g_cfView == 1 && idx < g_conf.files.size()) { v = g_conf.files[idx].mods; if (what) *what = W(g_conf.files[idx].path); }
    else if (g_cfView == 3 && idx < g_conf.vanilla.size()) { v = g_conf.vanilla[idx].mods; if (what) *what = W(g_conf.vanilla[idx].path); }
    else if (g_cfView == 3 && idx - g_conf.vanilla.size() < g_conf.vanillaWipes.size()) { const auto& vw = g_conf.vanillaWipes[idx - g_conf.vanilla.size()]; v = {vw.mod}; if (what) *what = W(vw.folder) + L"/"; }
    else if (g_cfView == 2 && idx < g_script.items.size()) {
        std::set<int> seen;
        for (auto& h : g_script.items[idx].hits) if (seen.insert(h.mod).second) v.push_back(h.mod);
        std::sort(v.begin(), v.end());
        if (what) *what = W(g_script.items[idx].key);
    }
    return v;
}

static std::wstring shortName(int pos) {
    Playset* ps = active();
    if (!ps || pos < 0 || pos >= (int)ps->mods.size()) return L"?";
    auto it = g_info.find(ps->mods[(size_t)pos].id);
    std::wstring n = W(it != g_info.end() ? it->second.name : ps->mods[(size_t)pos].id);
    if (n.size() > 38) n = n.substr(0, 37) + L"…";
    return n;
}

// Moves one mod right next to another so the mod that should win loads last. The old order is kept for the Undo button.
static void cfMoveFor(int mover, int target) {
    Playset* ps = active();
    if (!ps || mover == target || mover < 0 || target < 0 || mover >= (int)ps->mods.size() || target >= (int)ps->mods.size()) return;
    g_undoIds.clear();
    for (auto& m : ps->mods) g_undoIds.push_back(m.id);
    g_undoPlayset = ps->name;
    std::wstring a = shortName(mover), b = shortName(target);
    bool below = mover < target;
    moveMod(mover, target);     // erased first, then inserted at the target's position: right after it when moving down, right before it when moving up
    g_cfPairA = g_cfPairB = -1;
    say(L"Moved \"" + a + L"\" " + (below ? L"below \"" : L"above \"") + b + L"\". Press Undo to go back.");
    if (hConf) cfSetView(g_cfView == 1 ? 0 : g_cfView);
}

static void cfShowWinner(int row) {
    std::wstring what;
    std::vector<int> mods = cfRowMods(row, &what);
    if (mods.size() < 2 && g_cfView != 3) return;
    std::wstring t;
    if (g_cfView == 3 && (size_t)g_cfRows[(size_t)row] >= g_conf.vanilla.size()) {
        const auto& vw = g_conf.vanillaWipes[(size_t)g_cfRows[(size_t)row] - g_conf.vanilla.size()];
        t = what + L"\n\n" + modLabel(vw.mod) + L" uses replace_path for this folder. While it is enabled, none of the " + std::to_wstring(vw.files) +
            L" files the base game has there are loaded (and neither are files of mods that load before it). Total conversions do this on purpose; for any other mod it is worth a look.";
        MessageBoxW(hConf, t.c_str(), L"replace_path", MB_ICONINFORMATION);
        return;
    }
    if (g_cfView == 3) {
        t = what + L"\n\nThis is a file of the base game. A mod's copy REPLACES it completely: the game does not merge them.\n\nShipped by (load order, the last one's copy is used):\n";
    } else {
        bool van = false;
        size_t idx = (size_t)g_cfRows[(size_t)row];
        if (g_cfView == 1 && idx < g_conf.files.size()) van = g_conf.files[idx].vanilla;
        t = what + L"\n\n" + (van ? L"This file also exists in the base game, so the winner's copy replaces the game's.\n\n" : L"") + L"Load order (the last mod's copy is used):\n";
    }
    Playset* ps = active();
    for (size_t k = 0; k < mods.size(); k++) {
        t += L"  " + modLabel(mods[k]);
        if (ps && mods[k] >= 0 && mods[k] < (int)ps->mods.size()) {
            auto it = g_info.find(ps->mods[(size_t)mods[k]].id);
            if (g_cfView == 3 && it != g_info.end() && matchGameVersion(it->second.supported, g_gameVer) == VerMatch::Mismatch) t += L"   (made for an older game version)";
        }
        t += k + 1 == mods.size() ? L"     ← wins\n" : L"     loses\n";
    }
    if (g_cfView == 3 && mods.size() == 1) t += L"\nThe game's own copy is not used while this mod is enabled.";
    t += L"\nTo change who wins, right-click the row and move one of the mods.";
    MessageBoxW(hConf, t.c_str(), g_cfView == 3 ? L"Base-game file" : L"Who wins", MB_ICONINFORMATION);
}

static const int ID_CFM_BELOW = 900, ID_CFM_ABOVE = 901, ID_CFM_FILES = 902, ID_CFM_COPY = 903;
static void cfContextMenu(int row, POINT pt) {
    std::wstring what;
    std::vector<int> mods = cfRowMods(row, &what);
    if (row < 0 || row >= (int)g_cfRows.size()) return;
    HMENU m = CreatePopupMenu();
    int lo = -1, hi = -1;
    if (mods.size() >= 2) { lo = mods[mods.size() - 2]; hi = mods.back(); }   // the runner-up and the mod that wins now
    if (lo >= 0 && hi >= 0 && lo != hi) {
        AppendMenuW(m, MF_STRING, ID_CFM_BELOW, (L"Let \"" + shortName(lo) + L"\" win: move it below \"" + shortName(hi) + L"\"").c_str());
        AppendMenuW(m, MF_STRING, ID_CFM_ABOVE, (L"Let \"" + shortName(lo) + L"\" win: move \"" + shortName(hi) + L"\" above it").c_str());
    }
    if (g_cfView == 0) AppendMenuW(m, MF_STRING, ID_CFM_FILES, L"Show the files they share");
    if (g_cfView == 1 || g_cfView == 3) AppendMenuW(m, MF_STRING, ID_CFM_COPY, L"Copy file path");
    if (GetMenuItemCount(m) == 0) { DestroyMenu(m); return; }
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, hConf);
    DestroyMenu(m);
    if (cmd == ID_CFM_BELOW) cfMoveFor(lo, hi);
    else if (cmd == ID_CFM_ABOVE) cfMoveFor(hi, lo);
    else if (cmd == ID_CFM_FILES && g_cfView == 0) {
        size_t idx = (size_t)g_cfRows[(size_t)row];
        if (idx < g_conf.pairs.size()) {
            int a = g_conf.pairs[idx].a, b = g_conf.pairs[idx].b;
            cfEmptyRows(); g_cfView = 1; g_cfPairA = a; g_cfPairB = b; cfColumns(); cfRebuildRows();
            for (int id : {ID_CF_PAIR, ID_CF_FILE, ID_CF_DEF, ID_CF_VAN}) InvalidateRect(GetDlgItem(hConf, id), nullptr, FALSE);
        }
    } else if (cmd == ID_CFM_COPY) {
        if (!copyToClipboard(U(what))) info(L"Could not copy to the clipboard (another program may be holding it). Try again.");
    }
}

static LRESULT CALLBACK ConfProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            hConf = h;
            mkBtn(h, L"↔", L"By mod pair", ID_CF_PAIR);
            mkBtn(h, L"☰", L"By file", ID_CF_FILE);
            mkBtn(h, L"{ }", L"By definition", ID_CF_DEF);
            mkBtn(h, L"\u25C6", L"Base game", ID_CF_VAN);
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
        case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize.x = S(900); mi->ptMinTrackSize.y = S(360); return 0; }
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_MEASUREITEM: if (((MEASUREITEMSTRUCT*)l)->CtlType == ODT_MENU) { measureMenuItem((MEASUREITEMSTRUCT*)l); return TRUE; } break;
        case WM_DRAWITEM:
            if (((DRAWITEMSTRUCT*)l)->CtlType == ODT_MENU) drawMenuItem((DRAWITEMSTRUCT*)l); else drawButton((DRAWITEMSTRUCT*)l);
            return TRUE;
        case WM_APP + 1: cfTheme(); return 0;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_CF_PAIR: g_cfPairA = g_cfPairB = -1; cfSetView(0); break;
                case ID_CF_FILE: g_cfPairA = g_cfPairB = -1; cfSetView(1); break;
                case ID_CF_DEF: g_cfPairA = g_cfPairB = -1; cfSetView(2); break;
                case ID_CF_VAN: g_cfPairA = g_cfPairB = -1; cfSetView(3); break;
                case ID_CF_FILTER: if (HIWORD(w) == EN_CHANGE) cfRebuildRows(); break;
                case ID_CF_RESCAN: confQuiesce(); g_fileIndex.clear(); g_defIndex.clear(); g_vanilla.reset(); g_vanillaFailed = false; g_confSig.clear(); g_conf = ConflictReport(); g_script = ScriptReport(); maybeScan(false); cfRebuildRows(); refreshNotes(); break;
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
            if (nh->code == NM_RCLICK) {
                auto* ia = (NMITEMACTIVATE*)l;
                if (ia->iItem >= 0) { POINT pt; GetCursorPos(&pt); ListView_SetItemState(hCfList, ia->iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); cfContextMenu(ia->iItem, pt); }
                return 0;
            }
            if (nh->code == NM_DBLCLK) {
                int row = ((NMITEMACTIVATE*)l)->iItem;
                if ((g_cfView == 1 || g_cfView == 3) && row >= 0 && row < (int)g_cfRows.size()) cfShowWinner(row);
                else if (g_cfView == 0 && row >= 0 && row < (int)g_cfRows.size()) {
                    const auto& p = g_conf.pairs[(size_t)g_cfRows[(size_t)row]];
                    int a = p.a, b = p.b;
                    cfEmptyRows();
                    g_cfView = 1; g_cfPairA = a; g_cfPairB = b;
                    cfColumns(); cfRebuildRows();
                    for (int id : {ID_CF_PAIR, ID_CF_FILE, ID_CF_DEF, ID_CF_VAN}) InvalidateRect(GetDlgItem(hConf, id), nullptr, FALSE);
                } else if (g_cfView == 2 && row >= 0 && row < (int)g_cfRows.size() && (size_t)g_cfRows[(size_t)row] < g_script.items.size()) {
                    const auto& sc = g_script.items[(size_t)g_cfRows[(size_t)row]];
                    std::wstring t = W(defKindName(sc.kind)) + L": " + W(sc.key) + L"\n" + W(sc.area) + L"\n\n" + W(sc.note) + L"\n\nDefined in (load order):\n";
                    for (auto& hh : sc.hits) t += L"  " + modLabel(hh.mod) + L"\n      " + W(hh.file) + L"\n";
                    MessageBoxW(hConf, t.c_str(), L"Script overlap", MB_ICONINFORMATION);
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
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {   // the severity cell is coloured, the others are plain
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    bool sel = (ListView_GetItemState(hCfList, (int)row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    cd->clrTextBk = sel ? t.sel : (row & 1) ? t.alt : t.list;
                    cd->clrText = sel ? t.selText : t.text;
                    if (cd->iSubItem == 0 && !sel) {
                        int sv = cfRowSev((int)row);
                        cd->clrText = sv >= SEV_HIGH ? t.bad : sv == SEV_MED ? t.warn : t.muted;
                    }
                    return CDRF_NEWFONT;
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
static std::vector<std::string> steamLibraries() {
    std::string steam = regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (steam.empty()) steam = regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (steam.empty()) return {};
    std::vector<std::string> libs = {steam};
    std::string vdf;
    if (readFile(P(steam) / "steamapps" / "libraryfolders.vdf", vdf)) for (auto& l : steamLibraryPaths(vdf)) libs.push_back(l);
    return libs;
}
// Where Steam keeps subscribed CK3 Workshop mods (RC_WORKSHOP_DIR overrides it, for tests).
static std::vector<std::string> workshopDirs() {
    if (const char* e = getenv("RC_WORKSHOP_DIR"); e && *e) return {e};
    std::vector<std::string> out;
    for (auto& lib : steamLibraries()) out.push_back((P(lib) / "steamapps" / "workshop" / "content" / CK3_APPID).u8string());
    return out;
}
static std::string findGameExeInSteam() {
    std::vector<std::string> libs = steamLibraries();
    if (libs.empty()) return "";
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

static bool startProgram(const std::string& exeUtf8, std::wstring& err, const std::string& args = "") {
    fs::path exe = P(exeUtf8);
    std::wstring wexe = exe.wstring(), cwd = exe.parent_path().wstring();
    std::wstring cmd = L"\"" + wexe + L"\"" + (args.empty() ? L"" : L" " + W(args));
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

static void layout();
// ---------- DPI (per monitor) ----------
static NONCLIENTMETRICSW g_ncm;     // system font metrics, measured at the system DPI
static int g_sysDpi = 96;
static void createFonts() {
    for (HFONT* f : {&g_font, &g_fontSym, &g_fontBold, &g_fontCrown, &g_fontTitle, &g_fontSub, &g_fontTiny}) if (*f) { DeleteObject(*f); *f = nullptr; }
    LOGFONTW base = g_ncm.lfMessageFont;
    base.lfHeight = MulDiv(base.lfHeight, g_dpi, g_sysDpi);   // the message font is measured at the system DPI
    g_font = CreateFontIndirectW(&base);
    LOGFONTW lf = base;
    wcscpy(lf.lfFaceName, L"Segoe UI Symbol");
    g_fontSym = CreateFontIndirectW(&lf);
    lf = base; lf.lfWeight = FW_SEMIBOLD;
    g_fontBold = CreateFontIndirectW(&lf);
    lf = base; wcscpy(lf.lfFaceName, L"Segoe UI Symbol"); lf.lfHeight = -S(24);
    g_fontCrown = CreateFontIndirectW(&lf);
    lf = base; wcscpy(lf.lfFaceName, L"Georgia"); lf.lfHeight = -S(24); lf.lfWeight = FW_BOLD;
    g_fontTitle = CreateFontIndirectW(&lf);
    lf = base; lf.lfHeight = -S(11); lf.lfWeight = FW_SEMIBOLD;
    g_fontSub = CreateFontIndirectW(&lf);
    lf = base; lf.lfHeight = -S(9); lf.lfWeight = FW_NORMAL;
    g_fontTiny = CreateFontIndirectW(&lf);
}
static UINT windowDpi(HWND h) {
    using Fn = UINT(WINAPI*)(HWND);
    static Fn f = (Fn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");   // Windows 10 1607+; older systems keep the system DPI
    return f ? f(h) : 0;
}
// The window moved to a monitor with another scale (or the user changed it): rebuild fonts and redo the layout.
static void applyDpi(int nd, const RECT* suggested) {
    if (nd <= 0 || nd == g_dpi) return;
    int old = g_dpi;
    g_dpi = nd;
    if (hList) for (int c = 0; c < 7; c++) ListView_SetColumnWidth(hList, c, MulDiv(ListView_GetColumnWidth(hList, c), nd, old));
    createFonts();
    EnumChildWindows(hMain, [](HWND c, LPARAM) -> BOOL { SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE); return TRUE; }, 0);
    SendMessageW(hL2, WM_SETFONT, (WPARAM)g_fontCrown, TRUE);
    if (suggested) SetWindowPos(hMain, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    layout();
    logLine("DPI changed to " + std::to_string(nd));
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
    place(hTheme, rx - S(78), by - S(1), S(78), S(34)); rx -= S(78) + S(14);
    place(hLog, rx - S(126), by, S(126), S(32)); rx -= S(126) + gap;
    place(hAdv, rx - S(116), by, S(116), S(32)); rx -= S(116) + gap;
    place(hUpd, rx - S(104), by, S(104), S(32)); rx -= S(104) + gap;
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
    hUpd = mkBtn(hMain, L"\u2B06", L"Updates", ID_UPDATE);
    hTheme = mkBtn(hMain, L"", L"", ID_THEME);
    hL2 = mk(L"STATIC", L"\u2315", SS_CENTER, ID_L2);
    SendMessageW(hL2, WM_SETFONT, (WPARAM)g_fontCrown, TRUE);
    hFilter = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_FILTER, WS_EX_CLIENTEDGE);
    hAllOn = mkBtn(hMain, L"\u2611", L"Enable shown", ID_ALLON);
    hAllOff = mkBtn(hMain, L"\u2610", L"Disable shown", ID_ALLOFF);
    hConflicts = mkBtn(hMain, L"\u2694", L"Conflicts", ID_CONFLICTS);
    hSort = mkBtn(hMain, L"\u21C5", L"Auto Sort", ID_SORT);
    hUndo = mkBtn(hMain, L"\u21B6", L"Undo order", ID_UNDOSORT);
    EnableWindow(hUndo, FALSE);
    hCount = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_COUNT);
    hUp = mkBtn(hMain, L"\u25B2", L"Up", ID_UP);
    hDown = mkBtn(hMain, L"\u25BC", L"Down", ID_DOWN);
    hList = mk(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP, ID_LIST, 0);
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
    if (g_settings.colW.size() == 7)
        for (int c : {0, 2, 3, 4, 5}) if (g_settings.colW[(size_t)c] >= 30 && g_settings.colW[(size_t)c] <= 800) ListView_SetColumnWidth(hList, c, g_settings.colW[(size_t)c]);
    hStatus = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_STATUS);
    hGameVer = mk(L"STATIC", L"", SS_OWNERDRAW, ID_GAMEVER);
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
    tip(hNew, L"Create an empty playset");
    tip(hDup, L"Copy the selected playset under a new name");
    tip(hRen, L"Rename the selected playset");
    tip(hDel, L"Delete the selected playset (your mods are not touched)");
    tip(hExport, L"Save this playset as a Paradox Launcher playset file");
    tip(hImport, L"Load a Paradox Launcher playset file");
    tip(hPlay, L"Start Crusader Kings III with this playset (skips the launcher)");
    tip(hAdv, L"Backups, compare playsets, launch options, saves, updates and the diagnostics log");
    tip(hLog, L"What changed in each version");
    tip(hUpd, L"Check GitHub for a newer version of The Royal Court, and for a newer known-mods list");
    tip(hAllOn, L"Enable every mod currently shown in the list");
    tip(hAllOff, L"Disable every mod currently shown in the list");
    tip(hConflicts, L"Find mods that change the same files or definitions");
    tip(hSort, L"Propose a better load order (you see a preview first)");
    tip(hUndo, L"Go back to the load order from before the last Auto Sort or the last move made from the conflicts window");
    tip(hBrowse, L"Pick your Crusader Kings III folder");
    tip(hSaveDir, L"Remember this folder");
    tip(hResync, L"Rescan the mod folder (use after subscribing to a mod while the app is open)");
    tip(hTheme, L"Switch between dark and light");
    tip(hUp, L"Move the selected mod(s) up in the load order");
    tip(hDown, L"Move the selected mod(s) down in the load order");
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

static void restoreBackup() {
    Playset* ps = active();
    if (!ps) return;
    if (listBackups(ps->name).empty()) { info(L"This playset has no backups yet. One is made automatically before every Auto Sort, and when the app starts if the playset changed."); return; }
    std::wstring dir = backupsDir(ps->name).wstring();
    wchar_t buf[MAX_PATH * 2] = {0};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hMain;
    o.lpstrTitle = L"Restore this playset from a backup (the newest are listed first by date in the name)";
    o.lpstrFilter = L"Playset backups (*.json)\0*.json\0";
    o.lpstrInitialDir = dir.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = (DWORD)(sizeof buf / sizeof buf[0]);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o)) return;
    std::string text;
    if (!readFile(P(U(buf)), text, 5u << 20)) { info(L"Could not read that backup."); return; }
    ImportResult r = parsePlaysetFile(text, g_mods);
    if (!r.ok) { info(W(r.error).c_str()); return; }
    std::wstring q = L"Restore \"" + W(ps->name) + L"\" to the load order and enabled mods saved in:\n" + W(fileStem(U(buf))) + L"\n\nYour current state is backed up first, so you can come back.";
    if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    backupPlayset(*ps, g_info, "before restore");
    Playset from = r.playset;
    syncPlayset(from, g_mods);
    applyBackupOrder(*ps, from);
    g_undoIds.clear();
    saveActive(); populate();
    say(L"Playset restored from the backup.");
}

// ---------- updates (GitHub Releases) ----------
// Nothing is contacted until the Updates button is pressed (or, if switched on in Advanced, once at startup).
// Only https://github.com/<this project>/releases/... is ever downloaded, and the exe must match SHA256SUMS.txt from the same release.
static constexpr UINT WM_UPD_CHECKED = WM_APP + 5, WM_UPD_DOWNLOADED = WM_APP + 6;
static bool g_updBusy = false;
static std::vector<std::pair<UINT, LPARAM>> g_deferred;   // update messages that arrived while a dialog was open
static ReleaseInfo g_updRel;                       // the release found by the last successful check

static bool httpGet(const std::wstring& url, size_t maxBytes, std::string& body, std::wstring& err) {
#ifdef RC_TEST_HOOKS   // test builds only: serve the "release" from a folder instead of GitHub (never compiled into the shipped exe)
    if (const wchar_t* testDir = _wgetenv(L"RC_UPDATE_DIR"); testDir && *testDir) {   // test hook (like RC_DATA_DIR): serve the "release" from a folder instead of GitHub
        size_t cut = url.find_last_of(L'/');
        std::wstring leaf = url.find(L"/releases/latest") != std::wstring::npos ? L"latest.json" : url.substr(cut + 1);
        if (readFile(fs::path(testDir) / leaf, body, maxBytes)) return true;
        err = L"Test release file missing: " + leaf;
        return false;
    }
#endif
    wchar_t host[256] = L"", path[2048] = L"", extra[1024] = L"";
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) { err = L"Bad update address."; return false; }
    std::wstring ua = L"TheRoyalCourt/" + W(VERSION);
    HINTERNET ses = WinHttpOpen(ua.c_str(), 4 /* WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY */, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) ses = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { err = L"Could not start the Windows network library."; return false; }
    DWORD protos = 0x800;   // TLS 1.2 (also on Windows 7)
    WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protos, sizeof protos);
    WinHttpSetTimeouts(ses, 10000, 10000, 20000, 30000);
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", (std::wstring(path) + extra).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!req) err = L"Could not reach GitHub.";
    else if (!WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(req, nullptr)) err = L"Could not reach GitHub (are you online?).";
    else {
        DWORD status = 0, sz = sizeof status;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr);
        if (status == 404) err = L"No release was found on GitHub yet.";
        else if (status == 403 || status == 429) err = L"GitHub is limiting requests right now. Try again in a little while.";
        else if (status != 200) err = L"GitHub answered with error " + std::to_wstring(status) + L".";
        else {
            ok = true;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; err = L"The connection was interrupted."; break; }
                if (avail == 0) break;
                if (body.size() + avail > maxBytes) { ok = false; err = L"The download is larger than expected."; break; }
                size_t at = body.size();
                body.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(req, &body[at], avail, &got)) { ok = false; err = L"The connection was interrupted."; break; }
                body.resize(at + got);
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

struct UpdJob { bool silent = false; bool ok = false; std::wstring err; ReleaseInfo rel; std::string exeData; std::string knownText; };
static std::string exePath() { wchar_t b[MAX_PATH * 2]; DWORD n = GetModuleFileNameW(nullptr, b, MAX_PATH * 2); return U(std::wstring(b, n)); }

static DWORD WINAPI updCheckThread(LPVOID p) {
    UpdJob* j = (UpdJob*)p;
    std::string body;
    if (httpGet(W(std::string("https://api.github.com/repos/") + UPDATE_REPO + "/releases/latest"), 1u << 20, body, j->err)) {
        j->rel = parseRelease(body);
        j->ok = j->rel.ok;
        if (!j->ok) j->err = L"GitHub's answer could not be read.";
    }
    // The shared known-mods list comes from the same place (fixed address in this project's repo); it is validated before it is kept.
    std::wstring kerr;
    if (!httpGet(W(std::string("https://raw.githubusercontent.com/") + UPDATE_REPO + "/main/knownmods.json"), 512u << 10, j->knownText, kerr)) j->knownText.clear();
    if (!PostMessageW(hMain, WM_UPD_CHECKED, 0, (LPARAM)j)) delete j;
    return 0;
}
// Downloads the exe and SHA256SUMS.txt of the release, and keeps the exe only if its checksum matches.
static DWORD WINAPI updDownloadThread(LPVOID p) {
    UpdJob* j = (UpdJob*)p;
    const ReleaseAsset* ea = j->rel.asset(UPDATE_EXE_ASSET);
    const ReleaseAsset* sa = j->rel.asset(UPDATE_SUMS_ASSET);
    std::string sums;
    if (!ea || !sa) j->err = L"This release has no update file attached.";
    else if (httpGet(W(sa->url), 1u << 16, sums, j->err) && httpGet(W(ea->url), 64u << 20, j->exeData, j->err)) {
        std::string want = findSumFor(sums, UPDATE_EXE_ASSET);
        if (want.empty()) j->err = L"The release's checksum list does not include the program.";
        else if (j->exeData.size() < 100000 || j->exeData.compare(0, 2, "MZ") != 0) j->err = L"The downloaded file is not a program.";
        else if (sha256Hex(j->exeData) != want) j->err = L"The downloaded file does not match its checksum, so it was thrown away.";
        else j->ok = true;
    }
    if (!j->ok) j->exeData.clear();
    if (!PostMessageW(hMain, WM_UPD_DOWNLOADED, 0, (LPARAM)j)) delete j;
    return 0;
}
static void startUpdateCheck(bool silent) {
    if (g_updBusy) return;
    UpdJob* j = new UpdJob;
    j->silent = silent;
    g_updBusy = true;
    if (!silent) say(L"Checking GitHub for a newer version...");
    HANDLE t = CreateThread(nullptr, 0, updCheckThread, j, 0, nullptr);
    if (t) CloseHandle(t); else { g_updBusy = false; delete j; }
}
static std::wstring plainNotes(const std::string& md) {
    std::string o;
    for (size_t i = 0; i < md.size() && o.size() < 3000; i++) {
        char c = md[i];
        if (c == '*' || c == '`' || c == '\r') continue;
        if (c == '#') { while (i + 1 < md.size() && (md[i + 1] == '#' || md[i + 1] == ' ')) i++; continue; }
        o += c;
    }
    return W(o);
}
// Swaps the running exe for the downloaded one (Windows lets a running exe be renamed, not overwritten) and starts the new copy.
static bool installUpdate(const std::string& data, std::wstring& err) {
    std::wstring self = W(exePath());
    std::wstring neu = self + L".new", old = self + L".old";
    if (!writeFile(P(U(neu)), data)) { err = L"Could not write next to the program (is it in a protected folder like Program Files?). Download the new version from GitHub instead."; return false; }
    DeleteFileW(old.c_str());
    if (!MoveFileExW(self.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(neu.c_str()); err = L"Could not replace the program file (Windows error " + std::to_wstring(GetLastError()) + L")."; return false; }
    if (!MoveFileExW(neu.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING);   // put the old one back
        DeleteFileW(neu.c_str());
        err = L"Could not put the new program in place.";
        return false;
    }
    std::wstring args = L"--wait-pid " + std::to_wstring(GetCurrentProcessId());
    if (!startProgram(U(self), err, U(args))) {
        err = L"The update was installed but the new copy could not be started (" + err + L"). Start the program again.";
        return false;
    }
    return true;
}
using TaskDialogIndirectFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
static void showUpdateDialog(const ReleaseInfo& r) {
    bool canInstall = r.asset(UPDATE_EXE_ASSET) && r.asset(UPDATE_SUMS_ASSET);
    std::wstring head = L"Version " + W(r.tag) + L" is available";
    std::wstring body = L"You have version " + W(VERSION) + L"." + (canInstall ? L" \"Update now\" downloads the new program, checks it against the release's checksum, replaces this one and restarts. Your playsets and settings are not touched." : L" Open the release page to download it.");
    std::wstring notes = plainNotes(r.body);
    int choice = IDCANCEL;
    static TaskDialogIndirectFn td = (TaskDialogIndirectFn)(void*)GetProcAddress(GetModuleHandleW(L"comctl32.dll"), "TaskDialogIndirect");
    if (td) {
        TASKDIALOG_BUTTON btns[2]; int nb = 0;
        if (canInstall) btns[nb++] = {1001, L"Update now\nDownload, verify and restart"};
        if (!r.pageUrl.empty()) btns[nb++] = {1002, L"Open the release page\nSee what changed and download it yourself"};
        TASKDIALOGCONFIG c{};
        c.cbSize = sizeof c; c.hwndParent = hMain; c.hInstance = g_inst;
        c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_EXPAND_FOOTER_AREA;
        c.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        c.pszWindowTitle = L"The Royal Court"; c.pszMainInstruction = head.c_str(); c.pszContent = body.c_str();
        c.pszMainIcon = TD_INFORMATION_ICON;
        c.cButtons = (UINT)nb; c.pButtons = btns;
        if (!notes.empty()) { c.pszExpandedInformation = notes.c_str(); c.pszCollapsedControlText = L"What's new"; c.pszExpandedControlText = L"Hide"; }
        int pressed = 0;
        if (SUCCEEDED(td(&c, &pressed, nullptr, nullptr))) choice = pressed;
    } else {
        std::wstring q = head + L"\n\n" + body + L"\n\nYes = " + (canInstall ? L"update now" : L"open release page") + L", No = not now.";
        if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONINFORMATION) == IDYES) choice = canInstall ? 1001 : 1002;
    }
    if (choice == 1002 && !r.pageUrl.empty()) ShellExecuteW(hMain, L"open", W(r.pageUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (choice == 1001) {
        UpdJob* j = new UpdJob;
        j->rel = r;
        g_updBusy = true;
        say(L"Downloading version " + W(r.tag) + L"...");
        HANDLE t = CreateThread(nullptr, 0, updDownloadThread, j, 0, nullptr);
        if (t) CloseHandle(t); else { g_updBusy = false; delete j; }
    }
}
// Keeps a downloaded known-mods list if it is valid and newer than the one in use. Returns a sentence for the user ("" = nothing changed).
static std::wstring takeOnlineKnown(const std::string& text) {
    if (text.empty()) return L"";
    KnownOnlineInfo oi = checkKnownOnline(text);
    if (!oi.ok) { logLine("known-mods list from GitHub was not usable, ignored"); return L""; }
    if (oi.revision <= g_knownRev) { logLine("known-mods list is up to date (revision " + std::to_string(g_knownRev) + ")"); return L""; }
    if (!writeFileAtomic(knownOnlineFile(), text)) { logLine("could not save the known-mods list"); return L""; }
    g_known = loadKnownMods(&g_knownRev);
    logLine("known-mods list updated to revision " + std::to_string(g_knownRev) + " (" + std::to_string(oi.count) + " entries)");
    return L"Known-mods list updated (revision " + std::to_wstring(g_knownRev) + L"). Press Auto Sort to use it.";
}
static void onUpdateChecked(UpdJob* j) {
    g_updBusy = false;
    std::unique_ptr<UpdJob> job(j);
    std::wstring knownMsg = takeOnlineKnown(job->knownText);
    if (!job->ok) {
        logLine("update check failed: " + U(job->err));
        if (job->silent) return;
        say(knownMsg.empty() ? L"Update check failed." : knownMsg);
        info((job->err + L"\n\nYou can always get the latest version from the GitHub page." + (knownMsg.empty() ? L"" : L"\n\n" + knownMsg)).c_str());
        return;
    }
    bool newer = compareVersions(job->rel.tag, VERSION) > 0;
    logLine("update check: latest " + job->rel.tag + (newer ? " (newer)" : " (up to date)"));
    if (!newer) {
        if (!job->silent) { say(knownMsg.empty() ? L"You have the latest version." : knownMsg); info((L"You have the latest version (" + W(VERSION) + L")." + (knownMsg.empty() ? L"" : L"\n\n" + knownMsg)).c_str()); }
        else if (!knownMsg.empty()) say(knownMsg);
        return;
    }
    g_updRel = job->rel;
    if (job->silent) { say(L"A newer version (" + W(job->rel.tag) + L") is available - press Updates."); return; }
    say(L"A newer version is available: " + W(job->rel.tag) + L".");
    showUpdateDialog(job->rel);
}
static void onUpdateDownloaded(UpdJob* j) {
    g_updBusy = false;
    std::unique_ptr<UpdJob> job(j);
    std::wstring err = job->err;
    if (job->ok && installUpdate(job->exeData, err)) {
        logLine("updated to " + job->rel.tag + ", restarting");
        PostMessageW(hMain, WM_CLOSE, 0, 0);   // saves the window state, then the new copy takes over
        return;
    }
    logLine("update failed: " + U(err));
    say(L"Update failed.");
    info((err + L"\n\nYour current version was left as it is.").c_str());
}

static bool copyToClipboard(const std::string& utf8) {
    std::wstring w = W(utf8);
    std::wstring crlf;
    for (wchar_t c : w) { if (c == L'\n') crlf += L'\r'; crlf += c; }   // plain Windows line breaks so it pastes cleanly anywhere
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (crlf.size() + 1) * sizeof(wchar_t));
    if (!h) return false;
    void* p = GlobalLock(h);
    if (!p) { GlobalFree(h); return false; }
    memcpy(p, crlf.c_str(), (crlf.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(h);
    if (!OpenClipboard(hMain)) { GlobalFree(h); return false; }
    EmptyClipboard();
    bool ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
    if (!ok) GlobalFree(h);
    CloseClipboard();
    return ok;
}
// Copies the load order with types and known-mod flags to the clipboard and offers to open the GitHub issue page to paste it into.
static void sortReport() {
    Playset* ps = active();
    if (!ps || ps->mods.empty()) { info(L"There is no load order to report yet."); return; }
    std::vector<int> cats;
    for (auto& m : ps->mods) cats.push_back(catOfMod(m.id));
    std::string text = buildSortReport(*ps, g_info, g_known, cats, lockedIds(), g_gameVer, g_knownRev);
    if (!copyToClipboard(text)) { info(L"Could not copy to the clipboard (another program may be holding it). Try again."); return; }
    logLine("sort report copied (" + std::to_string(ps->mods.size()) + " mods)");
    say(L"Sort report copied to the clipboard.");
    std::wstring q = L"The load order of \"" + W(ps->name) + L"\" is on your clipboard: mod names, Steam ids, types and which mods the program recognises. No file paths or personal information.\n\nOpen the GitHub issues page now? Write what you expected at the bottom and paste (Ctrl+V).";
    if (MessageBoxW(hMain, q.c_str(), L"Report a sort problem", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(hMain, L"open", W(std::string("https://github.com/") + UPDATE_REPO + "/issues/new").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void advancedMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_FOLDER, L"Open playsets folder");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_BACKUP, L"Back up this playset now");
    AppendMenuW(m, MF_STRING, ID_ADV_RESTORE, L"Restore this playset from a backup...");
    AppendMenuW(m, MF_STRING, ID_ADV_OPENBACKUPS, L"Open backups folder");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_COMPARE, L"Compare with another playset...");
    AppendMenuW(m, MF_STRING, ID_ADV_LAUNCH, L"Game launch options for this playset...");
    if (!g_settings.hidden.empty()) AppendMenuW(m, MF_STRING, ID_ADV_UNHIDE, (L"Show removed mods (" + std::to_wstring(g_settings.hidden.size()) + L")").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_DELSAVES, L"Delete all saves...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_OPENSAVES, L"Open saves folder");
    AppendMenuW(m, MF_STRING, ID_ADV_OPENLOGS, L"Open game logs folder");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_settings.checkUpdates ? MF_CHECKED : 0), ID_ADV_AUTOUPD, L"Check for updates when the program starts");
    AppendMenuW(m, MF_STRING, ID_ADV_SORTREPORT, L"Report a sort problem (copy details)...");
    AppendMenuW(m, MF_STRING, ID_ADV_LOG, L"Open diagnostics log (for bug reports)");
    RECT r; GetWindowRect(hAdv, &r);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, hMain);
    DestroyMenu(m);
    std::string dir = effectiveDir();
    if (cmd == ID_FOLDER) ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (cmd == ID_ADV_DELSAVES) deleteAllSaves();
    else if (cmd == ID_ADV_BACKUP) {
        Playset* ps = active();
        if (!ps) return;
        std::string f = backupPlayset(*ps, g_info, "manual");
        say(f.empty() ? L"This playset is already backed up exactly as it is now." : L"Backup saved: " + W(fileStem(f)) + L".");
    }
    else if (cmd == ID_ADV_OPENBACKUPS) {
        Playset* ps = active();
        ShellExecuteW(hMain, L"open", (ps ? backupsDir(ps->name) : P(dataDir()) / "Backups").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (cmd == ID_ADV_AUTOUPD) { g_settings.checkUpdates = !g_settings.checkUpdates; saveSettingsNow(); say(g_settings.checkUpdates ? L"The program will look for updates when it starts." : L"The program will no longer look for updates by itself."); }
    else if (cmd == ID_ADV_LOG) {
        std::error_code lec;
        if (!fs::exists(logPath(), lec)) logLine("log opened by user");
        ShellExecuteW(hMain, L"open", logPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (cmd == ID_ADV_SORTREPORT) sortReport();
    else if (cmd == ID_ADV_RESTORE) restoreBackup();
    else if (cmd == ID_ADV_UNHIDE) { g_settings.hidden.clear(); saveSettingsNow(); resync(false); }
    else if (cmd == ID_ADV_LAUNCH) {
        Playset* ps = active();
        if (!ps) return;
        std::wstring v = g_settings.launch.count(ps->name) ? W(g_settings.launch[ps->name]) : L"";
        if (!askText(L"Game launch options", L"Extra options for the game, e.g. -debug_mode (empty = none):", v)) return;
        std::string t = U(v);
        while (!t.empty() && t.front() == ' ') t.erase(0, 1);
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (t.empty()) g_settings.launch.erase(ps->name); else g_settings.launch[ps->name] = t;
        saveSettingsNow();
        say(t.empty() ? L"Launch options cleared." : L"Launch options saved for \"" + W(ps->name) + L"\".");
    }
    else if (cmd == ID_ADV_COMPARE) {
        Playset* ps = active();
        if (!ps) return;
        HMENU pm = CreatePopupMenu();
        std::vector<int> idxs;
        for (size_t i = 0; i < g_playsets.size(); i++) if (g_playsets[i].name != ps->name) { idxs.push_back((int)i); AppendMenuW(pm, MF_STRING, 6000 + idxs.size() - 1, W(g_playsets[i].name).c_str()); }
        if (idxs.empty()) { DestroyMenu(pm); info(L"You need a second playset to compare with."); return; }
        int pick = trackMenu(pm, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, hMain);
        DestroyMenu(pm);
        if (pick < 6000) return;
        const Playset& other = g_playsets[(size_t)idxs[(size_t)(pick - 6000)]];
        PlaysetDiff d = comparePlaysets(*ps, other, g_info);
        auto sect = [&](const std::string& head, const std::vector<std::string>& v) {
            std::wstring o = W(head) + L" (" + std::to_wstring(v.size()) + L")\r\n";
            if (v.empty()) o += L"   -\r\n";
            for (auto& x : v) o += L"   " + W(x) + L"\r\n";
            return o + L"\r\n";
        };
        std::wstring t = L"\"" + W(ps->name) + L"\"  vs  \"" + W(other.name) + L"\"\r\n\r\n" + std::to_wstring(d.shared) + L" mods are enabled in both.  Load order of those: " + (d.sameOrder ? L"identical." : L"different.") + L"\r\n\r\n";
        t += sect("Only enabled in " + ps->name, d.onlyA) + sect("Only enabled in " + other.name, d.onlyB) + sect("Enabled in one, disabled in the other", d.enabledDiffers);
        if (!d.sameOrder) t += sect("Mods at a different place in the shared load order", d.moved);
        showText(L"Compare playsets", t);
    }
    else if (cmd == ID_ADV_OPENSAVES || cmd == ID_ADV_OPENLOGS) {
        std::error_code ec;
        fs::path p = P(dir) / (cmd == ID_ADV_OPENSAVES ? "save games" : "logs");
        if (dir.empty() || !fs::is_directory(p, ec)) info(L"That folder does not exist yet.");
        else ShellExecuteW(hMain, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// A Steam download that the launcher has not set up yet becomes a normal mod when the user ticks it: its ugc_<id>.mod is written (what the launcher would do).
static bool addPendingMod(const std::string& mid, std::wstring& err) {
    auto it = g_info.find(mid);
    if (it == g_info.end() || !it->second.pending) return true;
    std::string dir = effectiveDir(), e;
    Playset one;
    one.mods.push_back({mid, true, ""});
    int n = registerPendingMods(dir, one, g_info, &e);
    std::error_code ec;
    if (n == 0 && !fs::exists(P(dir) / "mod" / mid, ec)) { err = L"Could not add \"" + W(it->second.name) + L"\" to your mod folder." + (e.empty() ? L"" : L" " + W(e)); logLine("add Workshop mod failed: " + mid + " " + e); return false; }
    logLine("added Workshop mod " + mid);
    it->second.pending = false;
    for (auto& m : g_mods) if (m.id == mid) m.pending = false;
    g_modStamp = modDirStamp();   // our own write must not look like a change made by someone else
    return true;
}

static void setShown(bool on) {
    Playset* ps = active();
    if (!ps) return;
    std::wstring err;
    for (int idx : g_shown) {
        if (on && !addPendingMod(ps->mods[(size_t)idx].id, err)) continue;   // could not be added: stays off
        ps->mods[(size_t)idx].enabled = on;
    }
    saveActive();
    populate();
    if (!err.empty()) say(err);
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
        if (ps) {
            for (int i : g_ctxSel) {
                if (i < 0 || i >= (int)ps->mods.size()) continue;
                const std::string& mid = ps->mods[(size_t)i].id;
                if (id == ID_CTX_CAT + CAT_COUNT) g_settings.cats.erase(mid); else g_settings.cats[mid] = id - ID_CTX_CAT;
            }
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
            if (g_settings.launch.count(oldName)) { g_settings.launch[ps->name] = g_settings.launch[oldName]; g_settings.launch.erase(oldName); }
            if (g_undoPlayset == oldName) g_undoPlayset = ps->name;
            g_settings.active = ps->name;
            sortPlaysets(); saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_DEL: {
            if (!ps) break;
            if (g_playsets.size() < 2) { info(L"You need at least one playset."); break; }
            std::wstring q = L"Delete playset \"" + W(ps->name) + L"\"? Its file will be removed. A copy is kept in the Backups folder (Advanced > Open backups folder).";
            if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            std::string gone = ps->name;
            backupPlayset(*ps, g_info, "before deleting the playset");
            deletePlayset(gone);
            g_settings.locks.erase(gone);
            g_settings.launch.erase(gone);
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
            moveSelection(id == ID_UP);
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
            std::string regErr;
            int registered = registerPendingMods(dir, *ps, g_info, &regErr);
            if (registered) logLine("registered " + std::to_string(registered) + " Workshop mod(s) that the launcher had not set up yet");
            if (!regErr.empty()) logLine("register failed: " + regErr);
            int notAdded = 0;
            for (auto& m : ps->mods) {   // a Steam download that could not be written into the mod folder cannot be loaded: leave it out and say so
                std::error_code pe;
                if (m.enabled && inst.count(m.id) && g_info.count(m.id) && g_info[m.id].pending && !fs::exists(P(dir) / "mod" / m.id, pe)) { inst.erase(m.id); notAdded++; }
            }
            ApplyResult r = writeGameModList(dir, *ps, inst);
            if (!r.ok) { info(W(r.message).c_str()); break; }
            std::wstring err;
            std::string args;
            if (auto lo = g_settings.launch.find(ps->name); lo != g_settings.launch.end()) args = lo->second;
            if (!startProgram(exe, err, args)) { info((L"Could not start the game (" + err + L").").c_str()); break; }
            for (auto it = g_settings.seen.begin(); it != g_settings.seen.end();) it = g_info.count(it->first) ? std::next(it) : g_settings.seen.erase(it);   // forget mods that are gone
            for (auto& m : ps->mods) if (m.enabled) if (auto f = fpNow().find(m.id); f != fpNow().end()) g_settings.seen[m.id] = f->second;
            saveSettingsNow(); refreshNotes();
            say(W(r.message) + L". Starting Crusader Kings III directly (launcher skipped)." + (registered ? L" Registered " + std::to_wstring(registered) + L" new Steam mod(s) for you." : L"") + (notAdded ? L" WARNING: " + std::to_wstring(notAdded) + L" Steam mod(s) could not be added to your mod folder and were left out." : L""));
            break;
        }
        case ID_FOLDER: ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_LOG: showChangelog(); break;
        case ID_ADV: advancedMenu(); break;
        case ID_UPDATE:
            if (g_updBusy) { say(L"Already checking..."); break; }
            if (!g_updRel.tag.empty() && compareVersions(g_updRel.tag, VERSION) > 0) { showUpdateDialog(g_updRel); break; }   // already found by the startup check
            startUpdateCheck(false);
            break;
        case ID_RESYNC: confQuiesce(); g_fileIndex.clear(); g_defIndex.clear(); g_confSig.clear(); resync(false); break;
        case ID_CONFLICTS: showConflicts(); break;
        case ID_CTX_LOCK: {
            if (!ps || g_ctxIdx < 0 || g_ctxIdx >= (int)ps->mods.size()) break;
            auto& lk = g_settings.locks[ps->name];
            bool unlock = lk.count(ps->mods[(size_t)g_ctxIdx].id) != 0;   // the row that was clicked decides for the whole selection
            for (int i : g_ctxSel) {
                if (i < 0 || i >= (int)ps->mods.size()) continue;
                const std::string& modId = ps->mods[(size_t)i].id;
                if (unlock) lk.erase(modId); else lk.insert(modId);
            }
            saveSettingsNow(); populate();
            break;
        }
        case ID_CTX_REMOVE:
        case ID_CTX_DELFILES: {
            if (!ps) break;
            bool files = id == ID_CTX_DELFILES;
            std::vector<std::string> ids;
            for (int i : g_ctxSel) {
                if (i < 0 || i >= (int)ps->mods.size()) continue;
                const std::string& mid = ps->mods[(size_t)i].id;
                if (files && !g_info.count(mid)) continue;   // nothing on disk to delete
                ids.push_back(mid);
            }
            if (ids.empty()) break;
            confQuiesce();   // the conflict worker reads the file index, which is about to change
            auto nameOf = [&](const std::string& mid) { auto it = g_info.find(mid); return W(it != g_info.end() ? it->second.name : mid); };
            std::wstring q, firstName = nameOf(ids[0]);
            if (ids.size() == 1) {
                auto mit = g_info.find(ids[0]);
                std::wstring nm = nameOf(ids[0]);
                bool ws = mit != g_info.end() && mit->second.source == "Workshop";
                if (files && ws) q = L"Permanently delete \"" + nm + L"\"?\n\nThis erases the mod's entry (its descriptor file in your CK3 mod folder) from your disk and from every playset. This cannot be undone." +
                    (mit->second.contentState == 2 ? L"\n\nThe mod's files are already gone (you unsubscribed on Steam), so this just clears the leftover entry." : L"\n\nThe mod is still installed through Steam. Unsubscribe on Steam first, otherwise Steam will bring it back.");
                else if (files) q = L"Permanently delete \"" + nm + L"\"?\n\nIts descriptor file and its folder inside your CK3 mod folder are erased from your disk. This cannot be undone.";
                else q = L"Remove \"" + nm + L"\" from the list?\n\nNothing is deleted from your disk. The mod disappears from every playset and from this list (Advanced > Show removed mods brings it back).";
            } else {
                std::wstring cnt = std::to_wstring(ids.size()), list;
                for (size_t k = 0; k < ids.size() && k < 8; k++) list += L"\n  \u2022 " + nameOf(ids[k]);
                if (ids.size() > 8) list += L"\n  ... and " + std::to_wstring(ids.size() - 8) + L" more";
                if (files) q = L"Permanently delete these " + cnt + L" mods?" + list + L"\n\nTheir descriptor files and folders inside your CK3 mod folder are erased from your disk and from every playset. This cannot be undone.\n\nMods that are still installed through Steam come back unless you unsubscribe on Steam first.";
                else q = L"Remove these " + cnt + L" mods from the list?" + list + L"\n\nNothing is deleted from your disk. They disappear from every playset and from this list (Advanced > Show removed mods brings them back).";
            }
            if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            std::string dir = effectiveDir();
            std::vector<std::string> done;
            int failed = 0;
            if (files) {
                if (dir.empty()) break;
                backupAffected(ids, "before deleting mods");
                std::error_code ec;
                fs::path modDir = fs::weakly_canonical(P(dir) / "mod", ec);
                for (const std::string& mid : ids) {
                    auto mit = g_info.find(mid);
                    if (mit == g_info.end()) continue;
                    bool bad = false;
                    fs::path desc = modDir / mid;
                    // the mod's own folder is erased only if it really lies inside the CK3 mod folder (never the folder itself, never a link)
                    if (!mit->second.contentDir.empty()) {
                        std::error_code e1;
                        fs::path content = fs::weakly_canonical(P(mit->second.contentDir), e1);
                        fs::path rel = content.lexically_relative(modDir);
                        bool inside = !e1 && !rel.empty() && rel != "." && *rel.begin() != ".." && fs::is_directory(content, e1) && !fs::is_symlink(P(mit->second.contentDir), e1) && !isReparsePoint(P(mit->second.contentDir));
                        if (inside) { fs::remove_all(content, e1); if (e1 || fs::exists(content, e1)) bad = true; }
                    }
                    std::error_code e2;
                    fs::remove(desc, e2);
                    if (e2 || fs::exists(desc, e2)) bad = true;
                    if (bad) { failed++; g_fileIndex.erase(mid); logLine("delete failed for " + mid); }
                    else { done.push_back(mid); logLine("deleted mod " + mid); }
                }
            } else {
                backupAffected(ids, ids.size() == 1 ? "before removing a mod" : "before removing mods");
                for (const std::string& mid : ids) { g_settings.hidden.insert(mid); done.push_back(mid); logLine("removed mod from list " + mid); }
                saveSettingsNow();
            }
            if (files) g_modStamp = modDirStamp();   // our own deletions are not a change made by someone else
            if (!done.empty()) {
                std::set<std::string> gone(done.begin(), done.end());
                for (auto& pl : g_playsets) { pl.mods.erase(std::remove_if(pl.mods.begin(), pl.mods.end(), [&](const ModRef& m) { return gone.count(m.id) != 0; }), pl.mods.end()); savePlayset(pl, g_info); }
                g_mods.erase(std::remove_if(g_mods.begin(), g_mods.end(), [&](const ModInfo& m) { return gone.count(m.id) != 0; }), g_mods.end());
                g_info = infoMap(g_mods); g_fpNow.clear(); g_confSig.clear();
                for (const std::string& mid : done) {
                    g_fileIndex.erase(mid); g_defIndex.erase(mid);
                    if (files) { g_settings.seen.erase(mid); g_settings.hidden.erase(mid); g_settings.cats.erase(mid); for (auto& lk : g_settings.locks) lk.second.erase(mid); }
                }
                if (files) saveSettingsNow();
                populate();
            }
            std::wstring what = files ? L"Deleted " : L"Removed ";
            if (!done.empty()) say(what + (done.size() == 1 ? L"\"" + firstName + L"\"" : std::to_wstring(done.size()) + L" mods") + (files ? L"." : L" from the list."));
            if (failed) info((std::to_wstring(failed) + L" mod(s) could not be fully deleted (are their files open in another program?). They are left in the list; close the other program and try again.").c_str());
            break;
        }
        case ID_SORT: {
            if (!ps || ps->mods.size() < 2) { say(L"Nothing to sort."); break; }
            ensureConflicts();
            SortPlan plan = planSort(*ps, g_info, lockedIds(), g_settings.cats, g_conf.valid ? &g_conf : nullptr, &g_fileIndex, &g_known);
            if (!plan.changed) {
                std::wstring m = L"Already in a good order, nothing to move.";
                for (auto& wn : plan.warnings) m += L"  ⚠ " + W(wn);
                say(m); break;
            }
            if (!showSortPreview(plan, *ps)) { say(L"Auto Sort cancelled, nothing changed."); break; }
            backupPlayset(*ps, g_info, "before auto sort");
            g_undoIds.clear();
            for (auto& m : ps->mods) g_undoIds.push_back(m.id);
            g_undoPlayset = ps->name;
            std::vector<ModRef> nm;
            for (int o : plan.order) nm.push_back(ps->mods[(size_t)o]);
            ps->mods = nm;
            saveActive(); populate();
            say(L"Auto Sort moved " + std::to_wstring(plan.moves.size()) + L" mods. Press Undo order to go back.");
            break;
        }
        case ID_UNDOSORT: {
            if (!ps || g_undoIds.empty() || g_undoPlayset != ps->name) break;
            std::map<std::string, ModRef> byId;
            for (auto& m : ps->mods) byId.emplace(m.id, m);
            std::vector<ModRef> nm; std::set<std::string> used;
            for (auto& uid : g_undoIds) { auto f = byId.find(uid); if (f != byId.end() && used.insert(uid).second) nm.push_back(f->second); }
            for (auto& m : ps->mods) if (!used.count(m.id)) nm.push_back(m);
            ps->mods = nm;
            g_undoIds.clear();
            saveActive(); populate();
            say(L"Load order restored to how it was before the last change.");
            break;
        }

        case ID_THEME:
            g_dark = !g_dark;
            g_settings.theme = g_dark ? "dark" : "light";
            saveSettingsNow();
            startThemeAnim();
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
                if (!rowSel && ps && row < g_shown.size()) {   // a Steam download that is not added yet: fainter than a disabled mod
                    auto pit = g_info.find(ps->mods[(size_t)g_shown[row]].id);
                    if (pit != g_info.end() && pit->second.pending)
                        rowFg = RGB((GetRValue(rowFg) + GetRValue(rowBg)) / 2, (GetGValue(rowFg) + GetGValue(rowBg)) / 2, (GetBValue(rowFg) + GetBValue(rowBg)) / 2);
                }
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
                bool on = ListView_GetCheckState(hList, nm->iItem) != 0;
                std::wstring perr;
                if (on && !addPendingMod(ps->mods[(size_t)g_shown[(size_t)nm->iItem]].id, perr)) {
                    g_populating = true; ListView_SetCheckState(hList, nm->iItem, FALSE); g_populating = false;
                    say(perr);
                    return 0;
                }
                ps->mods[(size_t)g_shown[(size_t)nm->iItem]].enabled = on;
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
        case WM_MEASUREITEM: if (((MEASUREITEMSTRUCT*)l)->CtlType == ODT_MENU) { measureMenuItem((MEASUREITEMSTRUCT*)l); return TRUE; } break;
        case WM_DRAWITEM:
            if (((DRAWITEMSTRUCT*)l)->CtlType == ODT_MENU) drawMenuItem((DRAWITEMSTRUCT*)l); else drawButton((DRAWITEMSTRUCT*)l);
            return TRUE;
        case WM_APP + 2: g_scanDone = (int)w; updateCount(); confRefresh(); return 0;
        case WM_APP + 3: {   // background scan finished
            ScanJob* j = (ScanJob*)w;
            if (g_jobThread) { WaitForSingleObject(g_jobThread, 5000); CloseHandle(g_jobThread); g_jobThread = nullptr; }
            confQuiesce();   // the index is about to change
            bool cancelled = j->cancel.load();
            int hits = 0, read = 0, same = 0;
            for (size_t i = 0; i < j->items.size() && i < j->out.size(); i++) {
                if (i < j->unchanged.size() && j->unchanged[i]) {   // checked file by file: nothing changed
                    auto f = g_fileIndex.find(j->items[i].id);
                    if (f != g_fileIndex.end()) f->second.fingerprint = j->items[i].fp;
                    auto d = g_defIndex.find(j->items[i].id);
                    if (d != g_defIndex.end()) d->second.fingerprint = j->items[i].fp;
                    same++;
                    continue;
                }
                if (!j->out[i].complete) {
                    if (!cancelled) { g_scanFailed[j->items[i].id] = j->items[i].fp; logLine("could not read all files of mod " + j->items[i].id + ", skipped in conflict checks"); }
                    continue;
                }
                g_scanFailed.erase(j->items[i].id);
                g_fileIndex[j->items[i].id] = std::move(j->out[i]);
                if (i < j->defs.size() && j->defs[i].complete) g_defIndex[j->items[i].id] = std::move(j->defs[i]);
                if (i < j->fromCache.size() && j->fromCache[i]) hits++; else read++;
            }
            if (j->wantVanilla && !cancelled) {
                if (j->vanilla) {
                    g_vanilla = j->vanilla;
                    logLine("game files: " + std::to_string(g_vanilla->files.size()) + " in " + g_vanilla->dir + (g_vanilla->dlcFiles ? " (" + std::to_string(g_vanilla->dlcFiles) + " from DLC folders)" : "") + (j->vanillaFromCache ? ", from the saved list" : ", read from disk"));
                } else { g_vanillaFailed = true; logLine("game files: could not read the game folder, base-game checks are off"); }
            }
            if (!j->items.empty() && (read + hits > 0 || !j->quiet))
                logLine("scan: " + std::to_string(j->items.size()) + " mods in " + std::to_string((unsigned long long)j->ms) + " ms (" + std::to_string(hits) + " from the saved index, " + std::to_string(read) + " read" + (same ? ", " + std::to_string(same) + " unchanged" : std::string()) + ")");
            g_lastDeepVerify = GetTickCount64();
            g_fpNow.clear();   // the update check now knows each indexed mod's deep fingerprint
            {   // saved indexes of mods that are no longer installed
                static bool pruned = false;
                if (!pruned && !cancelled && !g_info.empty()) {
                    pruned = true;
                    std::set<std::string> keep;
                    for (auto& kv : g_info) keep.insert(kv.first);
                    pruneModCache(dataDir() + "/cache", keep);
                }
            }
            delete j; g_job = nullptr; g_scanning = false;
            if (!cancelled) { g_confSig.clear(); refreshNotes(); maybeScan(false); }
            return 0;
        }
        case WM_APP + 9: {   // conflict report computed on the worker thread
            if (!g_confJob || g_confJob->id != (unsigned)w) return 0;
            if (g_confThread) { WaitForSingleObject(g_confThread, INFINITE); CloseHandle(g_confThread); g_confThread = nullptr; }
            ConfJob* j = g_confJob; g_confJob = nullptr;
            bool use = !j->cancel.load() && j->conf.valid;
            if (!j->err.empty()) {   // the worker failed: compute here instead (and say so), never start another worker for this state
                logLine("conflict worker failed (" + j->err + "), computing on the main thread");
                Playset* ps = active();
                if (ps && j->sig == playsetSignature(*ps)) {
                    g_conf = findConflicts(*ps, g_info, g_fileIndex, g_vanilla.get());
                    g_script = findScriptConflicts(*ps, g_info, g_defIndex);
                    g_confSig = j->sig;
                } else g_confSig.clear();
                delete j;
                refreshNotes(); confRefresh();
                return 0;
            }
            if (use) {
                g_conf = std::move(j->conf); g_script = std::move(j->script); g_confSig = j->sig;
                logLine("conflicts: " + std::to_string(g_conf.files.size()) + " files, " + std::to_string(g_script.items.size()) + " script overlaps, computed in " + std::to_string((unsigned long long)j->ms) + " ms on the worker thread");
            } else { g_confSig.clear(); logLine(std::string("conflicts: worker result dropped (") + (j->cancel.load() ? "cancelled" : "invalid") + ")"); }
            delete j;
            refreshNotes();      // asks again when the result was thrown away
            confRefresh();
            return 0;
        }
        case WM_UPD_CHECKED:
        case WM_UPD_DOWNLOADED:
            if (!IsWindowEnabled(h)) { g_deferred.push_back({m, l}); SetTimer(h, 77, 400, nullptr); return 0; }   // a dialog is open: wait until it is closed
            if (m == WM_UPD_CHECKED) onUpdateChecked((UpdJob*)l); else onUpdateDownloaded((UpdJob*)l);
            return 0;
        case WM_APP + 4: try { maybeScan(w != 0); } catch (...) {} return 0;
        case WM_ACTIVATE:
            // Never rescan from inside this message: it is also sent while a message box or dialog is closing, in the middle of a handler
            // that still holds pointers into the playset data. Look again from the main loop, and only when no dialog is open.
            if (LOWORD(w) != WA_INACTIVE) PostMessageW(h, WM_APP + 7, 0, 0);
            return 0;
        case WM_APP + 7:
            if (IsWindowEnabled(h) && g_modStamp && modDirStamp() != g_modStamp) {
                try { resync(true); } catch (...) { g_resyncing = false; }
            } else if (IsWindowEnabled(h) && !g_job) {
                try { maybeScan(true); } catch (...) {}   // also looks inside the mods for changes (at most every two minutes), silently
            }
            return 0;
        case WM_TIMER:
            if (w == TIMER_TOGGLE) { stepThemeAnim(); return 0; }
            if (w == 78) {
                KillTimer(h, 78);
                std::wstring old = W(exePath()) + L".old";
                DeleteFileW(old.c_str());
                return 0;
            }
            if (w == 77) {   // messages that arrived while a dialog was open are handled once it is closed
                if (!IsWindowEnabled(h)) return 0;
                KillTimer(h, 77);
                std::vector<std::pair<UINT, LPARAM>> todo;
                todo.swap(g_deferred);
                for (auto& d : todo) SendMessageW(h, d.first, 0, d.second);
                return 0;
            }
            break;
        case WM_CONTEXTMENU: {
            if ((HWND)w != hList) break;
            Playset* ps = active();
            POINT pt{(short)LOWORD(l), (short)HIWORD(l)};
            int row = -1;
            if (l == -1) { row = ListView_GetNextItem(hList, -1, LVNI_SELECTED); RECT ir; if (row >= 0 && ListView_GetItemRect(hList, row, &ir, LVIR_BOUNDS)) { pt = {ir.left + S(60), ir.bottom}; ClientToScreen(hList, &pt); } }
            else { POINT cp = pt; ScreenToClient(hList, &cp); LVHITTESTINFO hi{}; hi.pt = cp; row = ListView_HitTest(hList, &hi); }
            if (!ps || row < 0 || row >= (int)g_shown.size()) return 0;
            g_ctxIdx = g_shown[(size_t)row];
            if (!(ListView_GetItemState(hList, row, LVIS_SELECTED) & LVIS_SELECTED)) selectRow(row);   // right-click on an unselected row selects just that row
            g_ctxSel = selectedMods();
            if (g_ctxSel.empty()) g_ctxSel.push_back(g_ctxIdx);
            const size_t nsel = g_ctxSel.size();
            const std::wstring mods = nsel > 1 ? std::to_wstring(nsel) + L" mods" : L"";
            const ModRef& mr = ps->mods[(size_t)g_ctxIdx];
            bool locked = g_settings.locks.count(ps->name) && g_settings.locks[ps->name].count(mr.id);
            bool ovr = false; int cur = catOfMod(mr.id, &ovr);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (locked && nsel == 1 ? MF_CHECKED : 0), ID_CTX_LOCK, nsel > 1 ? (L"Lock position of " + mods + L" (Auto Sort never moves them)").c_str() : L"Lock position (Auto Sort never moves it)");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            for (int c = 0; c < CAT_COUNT; c++) AppendMenuW(menu, MF_STRING | (nsel == 1 && ovr && cur == c ? MF_CHECKED : 0), ID_CTX_CAT + c, ((nsel > 1 ? L"Type for all: " : L"Type: ") + W(catName(c))).c_str());
            AppendMenuW(menu, MF_STRING | (nsel == 1 && !ovr ? MF_CHECKED : 0), ID_CTX_CAT + CAT_COUNT, nsel > 1 ? L"Type for all: automatic" : L"Type: automatic");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, ID_CTX_REMOVE, nsel > 1 ? (L"Remove " + mods + L" from list...").c_str() : L"Remove from list...");
            auto cit = g_info.find(mr.id);
            if (cit != g_info.end()) AppendMenuW(menu, MF_STRING, ID_CTX_DELFILES, nsel > 1 ? (L"Delete " + mods + L" permanently...").c_str() : L"Delete mod permanently...");
            trackMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, h);
            DestroyMenu(menu);
            return 0;
        }
        case WM_CLOSE: saveWindowState(h); DestroyWindow(h); return 0;
        case WM_ENDSESSION: if (w) saveWindowState(h); return 0;
        case WM_ENTERSIZEMOVE: g_inSizeMove = true; return 0;
        case WM_EXITSIZEMOVE: g_inSizeMove = false; saveWindowState(h); return 0;
        case WM_SIZE:
            if (w != SIZE_MINIMIZED) layout();
            if (g_windowReady && (w == SIZE_MAXIMIZED || w == SIZE_RESTORED)) saveWindowState(h);   // maximize / restore buttons (a drag-resize saves when the drag ends)
            return 0;
        case WM_DPICHANGED: applyDpi((int)HIWORD(w), (const RECT*)l); return 0;
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
        case WM_DESTROY: g_windowReady = false; stopScan(); confQuiesce(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g_inst = inst;
    installFastWalker();   // folder listings for the file index use the native Windows walker
#ifdef RC_TEST_HOOKS
    if (const char* e = getenv("RC_ASYNC_CONF"); e && *e == '1') g_forceAsyncConf = true;   // test builds only: compute every conflict report on the worker thread
#endif
    // Started by an update: wait for the old copy to finish closing, so the one-copy rule below does not turn the new one away.
    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i + 1 < argc; i++)
            if (wcscmp(argv[i], L"--wait-pid") == 0) {
                if (HANDLE ph = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)_wtoi(argv[i + 1]))) { WaitForSingleObject(ph, 15000); CloseHandle(ph); }
            }
        if (argv) LocalFree(argv);
    }
    // Only one copy at a time: two copies would each keep their own picture of the playset files and overwrite each other's changes.
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\TheRoyalCourt.SingleInstance");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(L"RoyalCourtMain", nullptr)) {
            if (IsIconic(other)) ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        char b[96];
        snprintf(b, sizeof b, "CRASH: exception %08lx at %p", (unsigned long)ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
        logLine(b);
        return EXCEPTION_CONTINUE_SEARCH;
    });
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&g_gdip, &gsi, nullptr);

    HDC dc = GetDC(nullptr);
    g_dpi = g_sysDpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    g_ncm.cbSize = sizeof g_ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof g_ncm, &g_ncm, 0);
    createFonts();
    g_appData = U(wenv(L"APPDATA"));   // must be known before anything reads or writes settings.json
    loadSettings(g_settings);
    g_dark = g_settings.theme.empty() ? systemPrefersDark() : g_settings.theme == "dark";
    rebuildBrushes();

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
    int wx = CW_USEDEFAULT, wy = CW_USEDEFAULT, ww = S(1260), wh = S(660);
    if (g_settings.winW >= S(1200) && g_settings.winH >= S(420)) {
        RECT want{g_settings.winX, g_settings.winY, g_settings.winX + g_settings.winW, g_settings.winY + g_settings.winH};
        if (MonitorFromRect(&want, MONITOR_DEFAULTTONULL)) { wx = want.left; wy = want.top; ww = g_settings.winW; wh = g_settings.winH; }
    }
    HWND win = CreateWindowExW(WS_EX_CONTROLPARENT, L"RoyalCourtMain", title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, wx, wy, ww, wh, nullptr, nullptr, inst, nullptr);
    if (!win) return 1;
    if (UINT wd = windowDpi(win); wd && (int)wd != g_dpi) applyDpi((int)wd, nullptr);   // started on a monitor with another scale than the system one
    if (g_settings.winMax && show != SW_SHOWMINIMIZED) show = SW_SHOWMAXIMIZED;

    try { reload(); }
    catch (const std::exception& e) { reportError(e.what()); }
    catch (...) { reportError(nullptr); }
    logLine(std::string("start v") + VERSION + ", " + std::to_string(g_mods.size()) + " mods, " + std::to_string(g_playsets.size()) + " playsets, dpi " + std::to_string(g_dpi) + ", game " + (g_gameVer.empty() ? "?" : g_gameVer));
    ShowWindow(win, show);
    UpdateWindow(win);
    g_windowReady = true;
    SetTimer(win, 78, 20000, nullptr);   // the old program file from an update is removed only once this version has run for a while (until then it is the way back)
    if (g_settings.checkUpdates) startUpdateCheck(true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(win, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    Gdiplus::GdiplusShutdown(g_gdip);
    return (int)msg.wParam;
}
