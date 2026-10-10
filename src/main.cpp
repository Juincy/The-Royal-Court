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
#include "gamelog.hpp"
#include "crash.hpp"
#include "share.hpp"
#include "changes.hpp"

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

// ---------- languages (see lang.hpp) ----------
// TL() is for a fixed text written as a literal: the pointer stays valid until the language is changed.
// TLF() is for a sentence with numbers or names in it: TLF(L"{0} of {1} mods", {a, b}).
static std::unordered_map<const wchar_t*, std::wstring> g_wcache;
#define TLK(x) x   // marks a literal that is translated later through a variable (a table, a parameter): the extractor collects it
static const wchar_t* TL(const wchar_t* key) {
    if (g_lang == 0 && !g_langPseudo) return key;
    auto it = g_wcache.find(key);
    if (it == g_wcache.end()) it = g_wcache.emplace(key, W(trText(U(key).c_str()))).first;
    return it->second.c_str();
}
struct WA {
    std::wstring s;
    WA(const wchar_t* v) : s(v) {}
    WA(const std::wstring& v) : s(v) {}
    WA(const std::string& v) : s(W(v)) {}
    WA(const char* v) : s(W(v)) {}
    WA(int v) : s(std::to_wstring(v)) {}
    WA(unsigned v) : s(std::to_wstring(v)) {}
    WA(long v) : s(std::to_wstring(v)) {}
    WA(unsigned long v) : s(std::to_wstring(v)) {}
    WA(long long v) : s(std::to_wstring(v)) {}
    WA(unsigned long long v) : s(std::to_wstring(v)) {}
};
static std::wstring TLF(const wchar_t* key, std::initializer_list<WA> args) {
    std::vector<std::wstring> v;
    for (auto& a : args) v.push_back(a.s);
    return langFill<std::wstring>(std::wstring(TL(key)), v);
}

enum {
    ID_COMBO = 100, ID_NEW, ID_DUP, ID_REN, ID_DEL, ID_EXPORT, ID_IMPORT, ID_PLAY, ID_LOG, ID_FILTER, ID_ALLON, ID_ALLOFF,
    ID_UP, ID_DOWN, ID_FOLDER, ID_LIST, ID_DIR, ID_BROWSE, ID_SAVEDIR, ID_STATUS, ID_COUNT, ID_L1, ID_L2, ID_L3, ID_GAMEVER, ID_RESYNC, ID_THEME, ID_CONFLICTS, ID_CF_PAIR, ID_CF_FILE, ID_CF_FILTER, ID_CF_RESCAN, ID_CF_LIST, ID_ADV, ID_ADV_DELSAVES, ID_ADV_OPENSAVES, ID_ADV_OPENLOGS, ID_SORT, ID_UNDOSORT, ID_ADV_BACKUP, ID_ADV_RESTORE, ID_ADV_OPENBACKUPS, ID_CF_DEF, ID_CTX_REMOVE = 730, ID_CTX_DELFILES, ID_CTX_UNSUB, ID_ADV_LAUNCH, ID_ADV_COMPARE, ID_ADV_UNHIDE, ID_UPDATE, ID_ADV_LOG, ID_ADV_AUTOUPD, ID_ADV_SORTREPORT, ID_CF_VAN, ID_ADV_GAMELOG, ID_ADV_SINCE, ID_ADV_CRASH, ID_ADV_TUTORIAL, ID_EXP_FILE, ID_EXP_CODE, ID_EXP_CODEN, ID_IMP_FILE, ID_IMP_CODE, ID_RP_A, ID_RP_B, ID_RP_COPY, ID_RP_RELOAD, ID_RP_FILTER, ID_RP_LIST, ID_RPM_COPY, ID_RPM_REPORT,
    ID_TREE, ID_EXPAND, ID_COLLAPSE,
    ID_LANG, ID_LANG_BASE = 7100   // ID_LANG_BASE: the language menu uses ID_LANG_BASE .. ID_LANG_BASE + LANG_COUNT
};

static HINSTANCE g_inst;
static HWND hMain, hL1, hL2, hL3, hLang, hCombo, hNew, hDup, hRen, hDel, hExport, hImport, hPlay, hAdv, hLog, hUpd, hFilter, hAllOn, hAllOff,
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
static int g_repViewForBtn = 0;   // which view of the game log window is open (for the button highlight)

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
static HWND g_tour = nullptr;   // the quick tour window
static void languageMenu(const RECT* at);
static void showTutorial();
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
static const UINT TIMER_UNSUB = 80;   // watches for Steam finishing the removal of unsubscribed mods
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
        // the version number is taken from g_gameVer itself, so the surrounding wording can be translated freely
        std::wstring num = g_gameVer.empty() ? L"" : W(g_gameVer);
        size_t numAt = num.empty() ? std::wstring::npos : all.rfind(num);
        bool known = numAt != std::wstring::npos;
        const std::wstring head = known ? all.substr(0, numAt) : std::wstring();
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
    bool primary = id == ID_PLAY || (id == ID_CF_PAIR && g_cfView == 0) || (id == ID_CF_FILE && g_cfView == 1) || (id == ID_CF_DEF && g_cfView == 2) || (id == ID_CF_VAN && g_cfView == 3) || (id == ID_RP_A && g_repViewForBtn == 0) || (id == ID_RP_B && g_repViewForBtn == 1);
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
        DrawTextW(dc, label.c_str(), -1, &lr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_NOCLIP);
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
    const wchar_t* title = L"The Royal Court";   // the program name stays as it is
    SIZE ts{0, 0}; GetTextExtentPoint32W(dc, title, (int)wcslen(title), &ts);
    RECT tr = {S(58), cy - ts.cy / 2 - S(1), S(58) + ts.cx + S(4), cy + ts.cy / 2 + S(2)};
    DrawTextW(dc, title, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, g_fontSub);
    SetTextColor(dc, t.muted);
    SetTextCharacterExtra(dc, S(2) / 1);
    std::wstring sub = TLF(L"CK3 MOD MANAGER  ·  v{0}", {VERSION});
    RECT sr = {tr.right + S(10), cy - S(6) + S(5), tr.right + S(400), cy + S(16)};
    DrawTextW(dc, sub.c_str(), -1, &sr, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
    SetTextCharacterExtra(dc, 0);
    {   // small author line under the subtitle
        SelectObject(dc, g_fontTiny);
        SetTextColor(dc, mix(t.muted, t.banner, 30));
        RECT ar = {sr.left, sr.top + S(14), sr.left + S(300), sr.top + S(25)};   // stays well above the gold line under the banner
        DrawTextW(dc, TLF(L"Author: {0}", {"Juincy"}).c_str(), -1, &ar, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
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
#ifdef RC_PROF
static double g_pf[6]; static std::string g_pfChild;
static double pfNow(){LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return (double)c.QuadPart*1000.0/(double)f.QuadPart;}
#endif
static void applyThemeColors(bool otherWindows, bool caption = true, bool frame = true, bool full = true, bool listNow = true) {
    const Theme& t = T();
#ifdef RC_PROF
    double a = pfNow();
#endif
    rebuildBrushes();
#ifdef RC_PROF
    double b = pfNow(); g_pf[0] += b - a;
#endif
    if (caption) themeFrameColors(hMain);
#ifdef RC_PROF
    double c = pfNow(); g_pf[1] += c - b;
#endif
    if (hList) {
        ListView_SetBkColor(hList, t.list);
        ListView_SetTextBkColor(hList, t.list);
        ListView_SetTextColor(hList, t.text);
    }
#ifdef RC_PROF
    double d = pfNow(); g_pf[2] += d - c;
    // redraw every child on its own to see which one is slow
    for (HWND ch = GetWindow(hMain, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT)) {
        double x = pfNow();
        RedrawWindow(ch, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        double y = pfNow() - x;
        if (y > 1.5) { wchar_t cn[64] = L""; GetClassNameW(ch, cn, 64); char u[64]; WideCharToMultiByte(CP_UTF8, 0, cn, -1, u, 64, nullptr, nullptr); g_pfChild += std::string(u) + "=" + std::to_string((int)(y + 0.5)) + " "; }
    }
    double e = pfNow(); g_pf[3] += e - d;
#endif
    // Paint the window's own background once, then every control without a second erase.
    if (full) {
        RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | (frame ? RDW_FRAME : 0));
    } else {
        RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_NOCHILDREN | RDW_UPDATENOW);
        for (HWND ch = GetWindow(hMain, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT))
            if (ch != hList || listNow) RedrawWindow(ch, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
#ifdef RC_PROF
    g_pf[4] += pfNow() - e;
#endif
    if (otherWindows) {
        if (HWND lg = FindWindowW(L"RCLog", nullptr)) SendMessageW(lg, WM_APP + 1, 0, 0);
        if (HWND cf = FindWindowW(L"RCConf", nullptr)) SendMessageW(cf, WM_APP + 1, 0, 0);
        if (HWND rp = FindWindowW(L"RCRep", nullptr)) SendMessageW(rp, WM_APP + 1, 0, 0);
        if (g_tour) SendMessageW(g_tour, WM_APP + 1, 0, 0);
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
    applyThemeColors(done, done || (g_animFrames & 1) == 0, done, done, true);   // title bar colour every other frame; frame repaint only at the end
    double ms = nowMs() - t0;
    g_paintSum += ms; if (ms > g_paintMax) g_paintMax = ms;
    if (done) {
        KillTimer(hMain, TIMER_TOGGLE);
        g_animating = false;
        if (g_periodOn) { timeEndPeriod(1); g_periodOn = false; }
#ifdef RC_PROF
        logLine("prof totals ms: brushes " + std::to_string((int)g_pf[0]) + ", caption " + std::to_string((int)g_pf[1]) + ", listview setters " + std::to_string((int)g_pf[2]) + ", per-child redraw " + std::to_string((int)g_pf[3]) + ", whole-window redraw after " + std::to_string((int)g_pf[4]) + " | slow children: " + g_pfChild);
        for (double& v : g_pf) v = 0; g_pfChild.clear();
#endif
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
    if (ps && !savePlayset(*ps, g_info)) say(TL(L"Warning: could not save the playset file."));
}
static void saveSettingsNow() {
    if (!saveSettings(g_settings)) say(TL(L"Warning: could not save settings."));
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
    std::wstring t = TLF(L"{0} of {1} enabled", {on, ps->mods.size()});
    if (bad > 0) t += L"  |  " + TLF(L"{0} with problems", {bad});
    if (g_scanning) t += L"  |  " + TLF(L"scanning mod files {0}/{1}", {g_scanDone, g_scanTotal});
    else if (g_confJob) t += L"  |  " + std::wstring(TL(L"calculating conflicts"));
    else if (g_conf.valid && !g_conf.files.empty()) {
        t += L"  |  " + TLF(L"{0} file conflicts", {g_conf.files.size()});
        if (g_script.valid && !g_script.items.empty()) t += L", " + TLF(L"{0} script overlaps", {g_script.items.size()});
    }
    if (!g_scanning && !g_confJob && g_conf.valid && g_conf.vanillaChecked) {
        int vm = 0; for (int c : g_conf.vanillaCount) if (c > 0) vm++;
        if (vm > 0) t += L"  |  " + TLF(L"{0} replace base-game files", {vm});
    }
    if (old > 0) t += L"  |  " + TLF(L"{0} may be outdated", {old});
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
    if (ps) if (auto pit = g_info.find(ps->mods[(size_t)i].id); pit != g_info.end() && pit->second.pending) return TL(L"Downloaded from Steam. Tick the box to add it");
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

// Type names for the screen (catName() stays English: it is also parsed).
static std::string catLabel(int c) { return catName(c); }
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
        return it != g_info.end() ? W(it->second.name) : TLF(L"{0}  (not installed)", {m.name.empty() ? m.id : m.name});
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
                case 4: return inst ? sourceLabel(it->second.source) : "";
                case 5: return inst ? catLabel(catOfMod(m.id)) : "";
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
        std::wstring ver = inst ? W(it->second.version) : L"", src = inst ? W(sourceLabel(it->second.source)) : L"";
        ListView_SetItemText(hList, row, 2, (LPWSTR)ver.c_str());
        std::wstring gv = inst ? W(it->second.supported) : L"";
        VerMatch vm = inst ? matchGameVersion(it->second.supported, g_gameVer) : VerMatch::Unknown;
        g_verState.push_back(vm == VerMatch::Match ? 1 : vm == VerMatch::Mismatch ? 2 : 0);
        ListView_SetItemText(hList, row, 3, (LPWSTR)gv.c_str());
        ListView_SetItemText(hList, row, 4, (LPWSTR)src.c_str());
        char nsev = 0;
        std::wstring note = noteText(i, nsev);
        bool ovr = false;
        std::wstring ty = inst ? W(catLabel(catOfMod(m.id, &ovr))) + (ovr ? L" *" : L"") : L"";
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
static void setGameVerText();
static void refreshGameVersion(const std::string& exe) {
    std::string ver = readGameVersion(exe);
    if (exe != g_gameExeUsed || ver != g_gameVer) {   // another install or a game update: the base game's file list is read again
        confQuiesce();
        g_vanilla.reset(); g_vanillaFailed = false; g_confSig.clear();
    }
    g_gameExeUsed = exe;
    g_gameVer = ver;
    setGameVerText();
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
        for (auto& e : listDir(P(wd))) {
            auto tc = fs::last_write_time(e.path(), e2);
            if (!e2) best = std::max(best, (long long)tc.time_since_epoch().count());
            e2.clear();
        }
    }
    return best;
}
static long long g_modStamp = 0;
static int g_hiddenPresent = 0;   // installed mods that are not shown because they were removed from the list

// Takes mods out of the program's own state (playsets, list, caches) after they were removed, deleted or unsubscribed from. Nothing on disk is touched here.
// purge: they are gone for good, so everything remembered about them is forgotten too.
static void dropMods(const std::vector<std::string>& done, bool purge) {
    if (done.empty()) return;
    confQuiesce();   // the conflict worker reads the file index
    std::set<std::string> gone(done.begin(), done.end());
    g_mods.erase(std::remove_if(g_mods.begin(), g_mods.end(), [&](const ModInfo& m) { return gone.count(m.id) != 0; }), g_mods.end());
    g_info = infoMap(g_mods); g_fpNow.clear(); g_confSig.clear();
    for (auto& pl : g_playsets) { pl.mods.erase(std::remove_if(pl.mods.begin(), pl.mods.end(), [&](const ModRef& m) { return gone.count(m.id) != 0; }), pl.mods.end()); savePlayset(pl, g_info); }
    for (const std::string& mid : done) {
        g_fileIndex.erase(mid); g_defIndex.erase(mid);
        if (purge) { g_settings.seen.erase(mid); g_settings.hidden.erase(mid); g_settings.unsubbed.erase(mid); g_settings.cats.erase(mid); for (auto& lk : g_settings.locks) lk.second.erase(mid); }
    }
}

// Mods the user unsubscribed from are already out of the list. Once Steam has deleted their files, the leftover descriptor (ugc_<id>.mod) in the CK3 mod folder
// is cleared here too, so they do not come back as "files not found" entries. Runs on every scan, before the hidden mods are filtered out.
static void clearUnsubscribed() {
    if (g_settings.unsubbed.empty()) return;
    std::string dir = effectiveDir();
    long long now = (long long)std::time(nullptr);
    std::vector<std::string> cleared;
    for (auto it = g_settings.unsubbed.begin(); it != g_settings.unsubbed.end();) {
        const std::string id = it->first;
        auto m = std::find_if(g_mods.begin(), g_mods.end(), [&](const ModInfo& x) { return x.id == id; });
        bool fin = false, unhide = false;
        if (m == g_mods.end() || steamIdOf(id).empty()) fin = true;                                   // nothing left to clear
        else if (!m->pending && m->contentState == 2 && !dir.empty()) {                               // Steam has removed the files: clear the leftover entry
            std::error_code e;
            fs::remove(P(dir) / "mod" / id, e);
            if (!e && !fs::exists(P(dir) / "mod" / id, e)) { logLine("cleared unsubscribed mod " + id); fin = true; }
        } else if (now - it->second > 900) { logLine("unsubscribed mod " + id + " still has files after 15 minutes: shown again"); fin = true; unhide = true; }   // Steam did not do it (offline?)
        if (!fin) { ++it; continue; }
        if (unhide) g_settings.hidden.erase(id);
        else cleared.push_back(id);
        it = g_settings.unsubbed.erase(it);
    }
    if (cleared.empty()) return;
    for (const std::string& id : cleared) { g_settings.seen.erase(id); g_settings.hidden.erase(id); g_settings.cats.erase(id); for (auto& lk : g_settings.locks) lk.second.erase(id); }
    g_mods.erase(std::remove_if(g_mods.begin(), g_mods.end(), [&](const ModInfo& m) { return std::find(cleared.begin(), cleared.end(), m.id) != cleared.end(); }), g_mods.end());
}

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
    clearUnsubscribed();
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
    if (!g_settings.unsubbed.empty() && hMain) SetTimer(hMain, TIMER_UNSUB, 3000, nullptr);   // watch for Steam finishing the removal
    fillCombo();
    populate();
    PostMessageW(hMain, WM_APP + 4, 1, 0);
    if (migrated > 0) say(TLF(L"Your {0} playset(s) from the older version were converted to launcher playset files.", {migrated}));
    else if (dir.empty()) say(TL(L"CK3 folder not found. Enter it below (the folder that contains \"mod\"), then press Save folder."));
    else if (g_mods.empty()) say(TLF(L"No mods found in {0}\\mod", {dir}));
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
            std::wstring ty = g_info.count(mid) ? W(catLabel(catOfMod(mid, &ovr))) + (ovr ? L" *" : L"") : L"";
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
    if (it == g_info.end()) t = (m.name.empty() ? m.id : m.name) + "\n" + trf("Not installed ({0})", {m.id}) + "\n";
    else {
        const ModInfo& mi = it->second;
        auto join = [](const std::vector<std::string>& v) { std::string o; for (size_t i = 0; i < v.size(); i++) o += (i ? ", " : "") + v[i]; return o.empty() ? std::string("-") : o; };
        std::string files = mi.path.empty() ? (mi.archive.empty() ? trf("Files: {0}", {"-"}) : trf("Files: archive {0}", {mi.archive}))
                                            : (mi.contentState == 2 ? trf("Files: {0}  (NOT FOUND)", {mi.contentDir}) : trf("Files: {0}", {mi.contentDir}));
        int kn = findKnown(g_known, mi);
        t = mi.name + "\n\n" + (m.enabled ? trf("Position in playset: {0} (enabled)", {idx + 1}) : trf("Position in playset: {0} (disabled)", {idx + 1})) +
            "\n" + trf("Descriptor: {0}", {mi.id}) + "\n" + trf("Source: {0}", {sourceLabel(mi.source)}) + "\n" + trf("Mod version: {0}", {mi.version.empty() ? "-" : mi.version}) +
            "\n" + trf("Game version: {0}", {mi.supported.empty() ? "-" : mi.supported}) +
            "\n" + files +
            "\n" + (g_settings.cats.count(mi.id) ? trf("Type: {0} (set by you)", {catLabel(catOfMod(mi.id))}) : trf("Type: {0} ({1})", {catLabel(catOfMod(mi.id)), guessCategory(mi).shown()})) +
            (lockedIds().count(mi.id) ? "\n" + tr("Position: locked (Auto Sort will not move it)") : "") +
            (kn >= 0 ? "\n" + trf("Known mod: {0}", {knownNoteText(g_known[(size_t)kn])}) : "") +
            "\n" + trf("Dependencies: {0}", {join(mi.deps)}) + "\n" + trf("Replaces vanilla folders: {0}", {join(mi.replacePaths)}) + "\n" + trf("Tags: {0}", {join(mi.tags)}) + "\n";
    }
    const auto& iss = g_issues[(size_t)idx];
    if (!iss.empty()) {
        t += "\n" + tr("Checks:") + "\n";
        for (auto& i : iss) t += (i.sev == 2 ? trf("  [problem] {0}", {i.text}) : i.sev == 1 ? trf("  [warning] {0}", {i.text}) : trf("  [info] {0}", {i.text})) + "\n";
    } else if (m.enabled) t += "\n" + tr("Checks: no problems found.") + "\n";
    MessageBoxW(hMain, W(t).c_str(), TL(L"Mod details"), MB_ICONINFORMATION);
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
    std::wstring msg = automatic ? TLF(L"The mod folder changed, list refreshed: {0} mods installed", {g_mods.size()}) : TLF(L"Rescanned: {0} mods installed", {g_mods.size()});
    if (added) msg += L", " + TLF(L"{0} new (added at the end of your playsets, disabled)", {added});
    if (gone) msg += L", " + TLF(L"{0} removed", {gone});
    msg = TLF(L"{0}.", {msg});
    if (pend) msg += L" " + TLF(L"{0} from Steam are not registered by the Paradox launcher yet (Play registers them).", {pend});
    if (g_hiddenPresent) msg += L" " + TLF(L"{0} hidden because you removed them (Advanced > Show removed mods).", {g_hiddenPresent});
    if (!automatic && !added) msg += std::wstring(L" ") + TL(L"Nothing new found: if you just subscribed, wait until Steam has finished downloading the mod, then rescan.");
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
    if (sel.empty()) { say(TL(L"Select a mod first.")); return; }
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
    HWND ok = mkBtn(d, L"\u2713", TL(L"OK"), IDOK);
    HWND ca = mkBtn(d, L"", TL(L"Cancel"), IDCANCEL);
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
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RCSort", TL(L"Auto Sort preview - The Royal Court"), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN, x, y, w, h, hMain, nullptr, g_inst, &ctx);
    std::wstring sum = TLF(L"Auto Sort will move {0} of {1} mods. Green = moves earlier, amber = moves later. Nothing is changed until you press Apply, and you can undo it afterwards.", {plan.moves.size(), ps.mods.size()});
    if (plan.knownCount > 0) sum += L" " + TLF(L"{0} mod(s) recognised from the known-mods list.", {plan.knownCount});
    if (plan.patchLinks > 0) sum += L" " + TLF(L"{0} patch(es) placed after the mods they are for.", {plan.patchLinks});
    if (plan.conflictChoices > 0) sum += L" " + TLF(L"{0} file-conflict tie-break(s) applied.", {plan.conflictChoices});
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
    const wchar_t* heads[5] = {TLK(L"New #"), TLK(L"Mod"), TLK(L"Type"), TLK(L"Change"), TLK(L"Why")};
    int widths[5] = {S(56), S(300), S(100), S(90), S(300)};
    for (int i = 0; i < 5; i++) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = (LPWSTR)TL(heads[i]); col.cx = widths[i]; ListView_InsertColumn(ctx.list, i, &col); }
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
        std::wstring ty = W(catLabel(plan.cat[(size_t)old]));
        ListView_SetItemText(ctx.list, p, 2, (LPWSTR)ty.c_str());
        std::wstring ch, why;
        if (lk.count(ps.mods[(size_t)old].id)) { ch = std::wstring(L"\U0001F512 ") + TL(L"locked"); }
        else if (old > p) { ch = L"▲ " + TLF(L"from {0}", {old + 1}); }
        else if (old < p) { ch = L"▼ " + TLF(L"from {0}", {old + 1}); }
        auto f = mv.find(old);
        if (f != mv.end()) why = W(f->second->reason);
        ListView_SetItemText(ctx.list, p, 3, (LPWSTR)ch.c_str());
        ListView_SetItemText(ctx.list, p, 4, (LPWSTR)why.c_str());
    }
    ctx.ok = mkBtn(d, L"✓", TL(L"Apply sort"), IDOK);
    ctx.cancel = mkBtn(d, L"", TL(L"Cancel"), IDCANCEL);
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
            if (!out.empty() && iswdigit(out[0])) out = TLF(L"Version {0}", {out});
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
            mkBtn(h, L"\u25BE", TL(L"Expand all"), ID_EXPAND);
            mkBtn(h, L"\u25B8", TL(L"Collapse all"), ID_COLLAPSE);
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
    g_logWin = CreateWindowExW(0, L"RCLog", TL(L"Changelog - The Royal Court"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, S(780), S(560), hMain, nullptr, g_inst, nullptr);
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
    if (g_cfView == 0) { cols.push_back({TLK(L"Severity"), 80}); cols.push_back({TLK(L"Loses (loads earlier)"), 270}); cols.push_back({TLK(L"Wins (loads later)"), 270}); cols.push_back({TLK(L"Files"), 60}); cols.push_back({TLK(L"Where"), 400}); }
    else if (g_cfView == 1) { cols.push_back({TLK(L"Severity"), 80}); cols.push_back({TLK(L"File"), 440}); cols.push_back({TLK(L"Winner (loads last)"), 270}); cols.push_back({TLK(L"Also in"), 380}); cols.push_back({TLK(L"Base game"), 90}); }
    else if (g_cfView == 3) { cols.push_back({TLK(L"Severity"), 80}); cols.push_back({TLK(L"Base-game file that mods replace"), 470}); cols.push_back({TLK(L"Replaced by (load order, last one is used)"), 520}); }
    else { cols.push_back({TLK(L"Severity"), 80}); cols.push_back({TLK(L"Type"), 110}); cols.push_back({TLK(L"Area"), 160}); cols.push_back({TLK(L"Name"), 240}); cols.push_back({TLK(L"Defined by (load order)"), 340}); cols.push_back({TLK(L"What happens"), 400}); }
    for (size_t i = 0; i < cols.size(); i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPWSTR)TL(cols[i].t);
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
    auto sevHit = [&](int sev) { return !f.empty() && (lower(sevName(sev)).find(f) != std::string::npos); };
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
    if (g_scanning) st = TLF(L"Reading mod files... {0}/{1}", {g_scanDone, g_scanTotal});
    else if (g_confJob) st = TL(L"Calculating conflicts...");
    else if (g_cfView == 2) {
        if (!g_script.valid) st = TL(L"No script data yet. Enable some mods that have a files folder, then press Rescan files.");
        else {
            int cnt[DK_KINDS] = {0};
            for (auto& sc : g_script.items) cnt[sc.kind]++;
            st = TLF(L"{0} overlaps", {g_script.items.size()}) + L"  |  " + TLF(L"{0} event IDs", {cnt[DK_EVENT]}) + L"  |  " + TLF(L"{0} definitions/defines", {cnt[DK_COMMON] + cnt[DK_DEFINE]}) +
                 L"  |  " + TLF(L"{0} on_actions", {cnt[DK_ONACTION]}) + L"  |  " + TLF(L"{0} localization keys.", {cnt[DK_LOC]}) + L"  " + TL(L"Double-click a row for the files.");
            if (g_script.modsMissing) st += L"  " + TLF(L"({0} mods not read)", {g_script.modsMissing});
        }
    }
    else if (!g_conf.valid) st = TL(L"No file data yet. Enable some mods that have a files folder, then press Rescan files.");
    else if (g_cfView == 3) {
        if (!g_conf.vanillaChecked) st = TL(L"The game's own files could not be read (is the game installed? Play once, or set the game in Advanced), so mods cannot be compared with them.");
        else {
            int mods = 0; for (int c : g_conf.vanillaCount) if (c > 0) mods++;
            if (g_conf.vanillaWipes.empty()) st = TLF(L"{0} base-game files are replaced by {1} mods.", {g_conf.vanilla.size(), mods});
            else st = TLF(L"{0} base-game files are replaced by {1} mods", {g_conf.vanilla.size(), mods}) + L"  |  " + TLF(L"{0} replace_path folders remove base-game files.", {g_conf.vanillaWipes.size()});
            st += L"  ";
            st += TL(L"Double-click a row for who wins, right-click to reorder.");
        }
    }
    else {
        st = TLF(L"{0} conflicting files", {g_conf.files.size()}) + L"  |  " + TLF(L"{0} mod pairs", {g_conf.pairs.size()}) + L"  |  " +
             TLF(L"{0} mods checked", {g_conf.modsIndexed});
        if (g_conf.modsWithoutFiles) st += L" " + TLF(L"({0} skipped: no readable files folder)", {g_conf.modsWithoutFiles});
        if (g_cfView == 1 && g_cfPairA >= 0) st = TLF(L"Files shared by {0}  and  {1}  (press \"By file\" to see all)", {modLabel(g_cfPairA), modLabel(g_cfPairB)}) + L"  |  " + TLF(L"{0} files", {g_cfRows.size()});
        else if (g_cfView == 0) { st += L".  "; st += TL(L"The later mod wins. Double-click a pair for its files, right-click to reorder."); }
        else { st += L".  "; st += TL(L"Double-click a file for who wins."); }
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
    if (sub == 0) { int sv = cfRowSev(row); return sv < 0 ? L"" : sv >= SEV_HIGH ? TL(L"High") : sv == SEV_MED ? TL(L"Medium") : TL(L"Low"); }
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
            g_cfText = sub == 0 ? W(vw.folder) + L"/   " + TLF(L"(the whole folder: {0} base-game files removed)", {vw.files}) : modLabel(vw.mod) + L"   (replace_path)";
            return g_cfText.c_str();
        }
        const auto& vf = g_conf.vanilla[idx];
        if (sub == 0) g_cfText = W(vf.path);
        else {
            for (size_t i = 0; i < vf.mods.size() && i < 5; i++) g_cfText += (i ? L";  " : L"") + modLabel(vf.mods[i]);
            if (vf.mods.size() > 5) g_cfText += L"  " + TLF(L"+{0} more", {vf.mods.size() - 5});
        }
    } else {
        if (idx >= g_conf.files.size()) return L"";
        const auto& fc = g_conf.files[idx];
        if (sub == 0) g_cfText = W(fc.path);
        else if (sub == 1) g_cfText = modLabel(fc.mods.back());
        else if (sub == 2) {
            for (size_t i = 0; i + 1 < fc.mods.size() && i < 4; i++) g_cfText += (i ? L";  " : L"") + modLabel(fc.mods[i]);
            if (fc.mods.size() > 5) g_cfText += L"  " + TLF(L"+{0} more", {fc.mods.size() - 5});
        } else g_cfText = fc.vanilla ? TL(L"yes") : L"";
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
    if (g_cfView == 0 && idx < g_conf.pairs.size()) { v = {g_conf.pairs[idx].a, g_conf.pairs[idx].b}; if (what) *what = TL(L"Files both mods ship"); }
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
    say(below ? TLF(L"Moved \"{0}\" below \"{1}\". Press Undo to go back.", {a, b}) : TLF(L"Moved \"{0}\" above \"{1}\". Press Undo to go back.", {a, b}));
    if (hConf) cfSetView(g_cfView == 1 ? 0 : g_cfView);
}

static void cfShowWinner(int row) {
    std::wstring what;
    std::vector<int> mods = cfRowMods(row, &what);
    if (mods.size() < 2 && g_cfView != 3) return;
    std::wstring t;
    if (g_cfView == 3 && (size_t)g_cfRows[(size_t)row] >= g_conf.vanilla.size()) {
        const auto& vw = g_conf.vanillaWipes[(size_t)g_cfRows[(size_t)row] - g_conf.vanilla.size()];
        t = what + L"\n\n" + TLF(L"{0} uses replace_path for this folder. While it is enabled, none of the {1} files the base game has there are loaded (and neither are files of mods that load before it). Total conversions do this on purpose; for any other mod it is worth a look.", {modLabel(vw.mod), vw.files});
        MessageBoxW(hConf, t.c_str(), L"replace_path", MB_ICONINFORMATION);
        return;
    }
    if (g_cfView == 3) {
        t = what + L"\n\n" + TL(L"This is a file of the base game. A mod's copy REPLACES it completely: the game does not merge them.\n\nShipped by (load order, the last one's copy is used):\n");
    } else {
        bool van = false;
        size_t idx = (size_t)g_cfRows[(size_t)row];
        if (g_cfView == 1 && idx < g_conf.files.size()) van = g_conf.files[idx].vanilla;
        t = what + L"\n\n" + (van ? TL(L"This file also exists in the base game, so the winner's copy replaces the game's.\n\n") : L"") + TL(L"Load order (the last mod's copy is used):\n");
    }
    Playset* ps = active();
    for (size_t k = 0; k < mods.size(); k++) {
        t += L"  " + modLabel(mods[k]);
        if (ps && mods[k] >= 0 && mods[k] < (int)ps->mods.size()) {
            auto it = g_info.find(ps->mods[(size_t)mods[k]].id);
            if (g_cfView == 3 && it != g_info.end() && matchGameVersion(it->second.supported, g_gameVer) == VerMatch::Mismatch) { t += L"   "; t += TL(L"(made for an older game version)"); }
        }
        t += L"     "; t += k + 1 == mods.size() ? TL(L"← wins") : TL(L"loses"); t += L"\n";
    }
    if (g_cfView == 3 && mods.size() == 1) { t += L"\n"; t += TL(L"The game's own copy is not used while this mod is enabled."); }
    t += L"\n"; t += TL(L"To change who wins, right-click the row and move one of the mods.");
    MessageBoxW(hConf, t.c_str(), g_cfView == 3 ? TL(L"Base-game file") : TL(L"Who wins"), MB_ICONINFORMATION);
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
        AppendMenuW(m, MF_STRING, ID_CFM_BELOW, TLF(L"Let \"{0}\" win: move it below \"{1}\"", {shortName(lo), shortName(hi)}).c_str());
        AppendMenuW(m, MF_STRING, ID_CFM_ABOVE, TLF(L"Let \"{0}\" win: move \"{1}\" above it", {shortName(lo), shortName(hi)}).c_str());
    }
    if (g_cfView == 0) AppendMenuW(m, MF_STRING, ID_CFM_FILES, TL(L"Show the files they share"));
    if (g_cfView == 1 || g_cfView == 3) AppendMenuW(m, MF_STRING, ID_CFM_COPY, TL(L"Copy file path"));
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
        if (!copyToClipboard(U(what))) info(TL(L"Could not copy to the clipboard (another program may be holding it). Try again."));
    }
}

static LRESULT CALLBACK ConfProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            hConf = h;
            mkBtn(h, L"↔", TL(L"By mod pair"), ID_CF_PAIR);
            mkBtn(h, L"☰", TL(L"By file"), ID_CF_FILE);
            mkBtn(h, L"{ }", TL(L"By definition"), ID_CF_DEF);
            mkBtn(h, L"\u25C6", TL(L"Base game"), ID_CF_VAN);
            mkBtn(h, L"↻", TL(L"Rescan files"), ID_CF_RESCAN);
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
                    std::wstring t = W(defKindName(sc.kind)) + L": " + W(sc.key) + L"\n" + W(sc.area) + L"\n\n" + W(sc.note) + L"\n\n";
                    t += TL(L"Defined in (load order):\n");
                    for (auto& hh : sc.hits) t += L"  " + modLabel(hh.mod) + L"\n      " + W(hh.file) + L"\n";
                    MessageBoxW(hConf, t.c_str(), TL(L"Script overlap"), MB_ICONINFORMATION);
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
    CreateWindowExW(0, L"RCConf", TL(L"Conflicts - The Royal Court"), WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1080), S(600), hMain, nullptr, g_inst, nullptr);
    if (!g_scanning) maybeScan(true);
    ensureConflicts();
    confRefresh();
}

// ---------- report window: the game log helper and "since you last pressed Play" ----------
// One window, two contents. The game log helper reads the game's own log and shows which mod each message points at.
// "Since last Play" lists the mods that were updated, are now older than the game, or were turned on or off.
struct RepRow { std::vector<std::wstring> cells; std::wstring hay, key; int sev = 0; int mod = -1; std::string copy; };
static HWND hRep = nullptr, hRepList = nullptr, hRepFilter = nullptr, hRepStatus = nullptr;
static int g_repKind = 0;            // 0 game log, 1 since last Play
static std::vector<RepRow> g_repRows;
static std::vector<int> g_repShown;
static std::unique_ptr<LogReport> g_logRep;
static std::wstring g_repStatusText;
static std::string g_repReport;      // what "Copy report" puts on the clipboard
static bool g_repWaitIndex = false;  // the game log was read before the mods' files were listed

static std::string readHeadOrTail(const fs::path& p, size_t maxBytes, bool tail) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if (sz <= 0) return "";
    std::streamoff start = (tail && sz > (std::streamoff)maxBytes) ? sz - (std::streamoff)maxBytes : 0;
    size_t len = (size_t)std::min<std::streamoff>(sz - start, (std::streamoff)maxBytes);
    f.seekg(start);
    std::string s(len, '\0');
    f.read(&s[0], (std::streamsize)len);
    s.resize((size_t)f.gcount());
    if (tail && start) { size_t nl = s.find('\n'); if (nl != std::string::npos) s.erase(0, nl + 1); }
    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    return s;
}
static std::wstring repWhen(long long t) {
    std::time_t tt = (std::time_t)t;
    wchar_t b[64] = L"";
    if (const std::tm* tm = std::localtime(&tt)) wcsftime(b, 64, L"%d %b %Y %H:%M", tm);
    return b;
}
static std::wstring repMessage(const LogEntry& e) {
    std::string d = e.detail;
    if (d.compare(0, 7, "Error: ") == 0) d.erase(0, 7);
    auto cut = [](std::string& t, const char* tok) { size_t k = t.find(tok); if (k != std::string::npos && k > 0) t.erase(k); };
    cut(d, " | Script location:"); cut(d, "Script location:");
    std::string m = e.msg;
    for (const char* tok : {" at file: ", ", at file: ", " in 'file: ", " at location 'file: ", " in file: "}) cut(m, tok);   // the file has its own column
    m = (e.msg == "Script system error!" && !d.empty()) ? d : (d.empty() ? m : m + " | " + d);
    return W(m);
}
static std::wstring repWhere(const LogEntry& e) { return e.file.empty() ? L"" : W(e.file + (e.line ? ":" + std::to_string(e.line) : "")); }
static void repAddRow(RepRow r) {
    std::wstring h;
    for (auto& c : r.cells) { h += c; h += L' '; }
    for (auto& ch : h) ch = (wchar_t)towlower(ch);
    r.hay = std::move(h);
    g_repRows.push_back(std::move(r));
}

static void repBuildLog() {
    g_repRows.clear(); g_logRep.reset(); g_repReport.clear(); g_repWaitIndex = false;
    Playset* ps = active();
    std::string dir = effectiveDir();
    if (!ps || dir.empty()) { g_repStatusText = TL(L"The CK3 folder is not set. Enter it in the main window first."); return; }
    fs::path logs = P(dir) / "logs";
    std::error_code ec;
    std::string text = readHeadOrTail(logs / "game.log", 32u << 20, true);
    std::string old = readHeadOrTail(logs / "error.log", 8u << 20, true);
    if (text.empty() && old.empty()) { g_repStatusText = TLF(L"There is no game log yet ({0}). Start the game once, then open this again.", {logs.u8string()}); return; }
    text += "\n"; text += old;
    int enabled = 0, ready = 0;
    for (auto& m : ps->mods) if (m.enabled) {
        enabled++;
        auto f = g_fileIndex.find(m.id);
        if (f != g_fileIndex.end() && f->second.complete) ready++;
    }
    if (ready < enabled) { g_repWaitIndex = true; if (!g_scanning) maybeScan(true); }
    g_logRep = std::make_unique<LogReport>(attributeLog(parseGameLog(text, true), *ps, g_fileIndex, g_defIndex, g_vanilla.get()));
    const LogReport& lr = *g_logRep;
    // Which playset did the game load? debug.log lists the mods it started with.
    std::wstring same;
    std::vector<LoadedMod> loaded = parseLoadedMods(readHeadOrTail(logs / "debug.log", 6u << 20, false));
    if (!loaded.empty()) {
        std::set<std::string> was, is;
        for (auto& m : loaded) if (m.enabled) was.insert(m.id);
        for (auto& m : ps->mods) if (m.enabled) is.insert(m.id);
        int onlyNow = 0, onlyThen = 0;
        for (auto& id : is) if (!was.count(id)) onlyNow++;
        for (auto& id : was) if (!is.count(id)) onlyThen++;
        if (!onlyNow && !onlyThen) same = std::wstring(L" · ") + TL(L"the game ran with the mods you have enabled now");
        else same = L" · " + TLF(L"NOT the playset you have now ({0} turned on, {1} turned off since); messages may point at the wrong mods", {onlyNow, onlyThen});
    }
    auto ft = fs::last_write_time(logs / "game.log", ec);
    long long when = 0;
    if (!ec) when = (long long)std::time(nullptr) - (long long)std::chrono::duration_cast<std::chrono::seconds>(fs::file_time_type::clock::now() - ft).count();
    size_t byFile = 0, byName = 0;
    for (size_t k = 0; k < lr.how.size(); k++) { if (lr.modOf[k] >= 0) (lr.how[k] == LH_FILE ? byFile : byName)++; }
    g_repStatusText = TLF(L"Log from {0}", {when ? repWhen(when) : std::wstring(L"?")}) + L" · " + TLF(L"{0} errors, {1} warnings ({2} different)", {lr.parse.errors, lr.parse.warnings, lr.parse.entries.size()}) + same
        + (g_repWaitIndex ? std::wstring(L" · ") + TL(L"still reading your mods' files, this updates by itself") : std::wstring())
        + (!g_vanilla ? std::wstring(L" · ") + TL(L"base-game file list not loaded") : std::wstring());
    g_repReport = "Game log summary (" + std::to_string(lr.parse.errors) + " errors, " + std::to_string(lr.parse.warnings) + " warnings)\n";
    // rows
    auto modName = [&](int pos) { auto it = g_info.find(ps->mods[(size_t)pos].id); return it != g_info.end() ? it->second.name : ps->mods[(size_t)pos].id; };
    auto topMsg = [&](int mod) {
        const LogEntry* best = nullptr;
        for (size_t k = 0; k < lr.parse.entries.size(); k++) {
            if (lr.modOf[k] != mod) continue;
            const LogEntry& e = lr.parse.entries[k];
            if (!best || (e.level == 'E' && best->level != 'E') || (e.level == best->level && e.count > best->count)) best = &e;
        }
        return best ? repMessage(*best) : std::wstring();
    };
    auto groupRows = [&](int mod, const std::wstring& label, const LogReport::PerMod& pmv, int sevBase) {
        (void)pmv; (void)sevBase;
        std::vector<size_t> ks;
        for (size_t k = 0; k < lr.parse.entries.size(); k++) {
            bool in = mod >= 0 ? lr.modOf[k] == mod : mod == -2 ? (lr.modOf[k] < 0 && lr.how[k] == LH_BASE) : (lr.modOf[k] < 0 && lr.how[k] != LH_BASE);
            if (in) ks.push_back(k);
        }
        std::stable_sort(ks.begin(), ks.end(), [&](size_t a, size_t b) {
            const LogEntry& x = lr.parse.entries[a]; const LogEntry& y = lr.parse.entries[b];
            if ((x.level == 'E') != (y.level == 'E')) return x.level == 'E';
            return x.count > y.count; });
        for (size_t k : ks) {
            const LogEntry& e = lr.parse.entries[k];
            RepRow r; r.mod = mod; r.sev = e.level == 'E' ? 2 : 1;
            r.cells = {label + (lr.how[k] == LH_NAME ? std::wstring(L"  ") + TL(L"(matched by name)") : std::wstring()), e.level == 'E' ? TL(L"Error") : TL(L"Warning"), std::to_wstring(e.count), repMessage(e), repWhere(e)};
            r.key = label;
            r.copy = U(std::wstring(e.level == 'E' ? L"[error] " : L"[warning] ") + repMessage(e) + (e.file.empty() ? L"" : L"  (" + repWhere(e) + L")") + (e.count > 1 ? L"  x" + std::to_wstring(e.count) : L""));
            repAddRow(std::move(r));
        }
    };
    if (g_repViewForBtn == 0) {
        for (auto& pm : lr.perMod) {
            RepRow r; r.mod = pm.mod; r.sev = pm.errors ? 2 : 1;
            std::wstring lab = modLabel(pm.mod);
            r.cells = {lab, std::to_wstring(pm.errors), std::to_wstring(pm.warnings), std::to_wstring(pm.entries), topMsg(pm.mod)};
            r.key = W(modName(pm.mod));
            r.copy = U(lab + L": " + std::to_wstring(pm.errors) + L" errors, " + std::to_wstring(pm.warnings) + L" warnings");
            g_repReport += U(lab) + ": " + std::to_string(pm.errors) + " errors, " + std::to_string(pm.warnings) + " warnings\n";
            repAddRow(std::move(r));
        }
        if (lr.base.entries) {
            RepRow r; r.mod = -2; r.sev = 0; r.key = TL(L"Base game");
            r.cells = {TL(L"Base game files (no mod replaces them)"), std::to_wstring(lr.base.errors), std::to_wstring(lr.base.warnings), std::to_wstring(lr.base.entries), TL(L"Problems in the game's own files, or caused by a mod referring to something that is missing")};
            repAddRow(std::move(r));
            g_repReport += "Base game files: " + std::to_string(lr.base.errors) + " errors, " + std::to_string(lr.base.warnings) + " warnings\n";
        }
        if (lr.unplaced.entries) {
            RepRow r; r.mod = -3; r.sev = 0; r.key = L"Unplaced";
            r.cells = {TL(L"Could not be tied to a mod"), std::to_wstring(lr.unplaced.errors), std::to_wstring(lr.unplaced.warnings), std::to_wstring(lr.unplaced.entries), TL(L"No file or name in the message matches a mod you have enabled")};
            repAddRow(std::move(r));
            g_repReport += "Could not be tied to a mod: " + std::to_string(lr.unplaced.errors) + " errors, " + std::to_string(lr.unplaced.warnings) + " warnings\n";
        }
    } else {
        for (auto& pm : lr.perMod) groupRows(pm.mod, modLabel(pm.mod), pm, 0);
        groupRows(-2, TL(L"Base game"), lr.base, 0);
        groupRows(-3, TL(L"Not tied to a mod"), lr.unplaced, 0);
        g_repReport += "(all messages: use the By mod view for a summary)\n";
    }
}

static void repBuildSince() {
    g_repRows.clear(); g_repReport.clear();
    Playset* ps = active();
    if (!ps) { g_repStatusText = TL(L"No playset."); return; }
    PlayReport pr = buildPlayReport(g_settings, *ps, g_info, fpNow(), g_fileIndex, g_gameVer);
    g_repStatusText = pr.hasPrev ? TLF(L"Since you last pressed Play: {0}", {repWhen(pr.time)}) + (pr.prevGame.empty() ? std::wstring() : L", " + TLF(L"game {0}", {pr.prevGame})) + (pr.samePlayset ? std::wstring() : std::wstring(L" ") + TL(L"(a different playset, so turned-on and turned-off mods are not compared)"))
                                 : std::wstring(TL(L"Play has not been pressed with this version yet. After you press Play, this window shows what changed since."));
    auto add = [&](const wchar_t* what, const std::string& name, const std::string& text, int sev) {
        RepRow r; r.sev = sev; r.cells = {TL(what), W(name), W(text)}; r.copy = U(std::wstring(what) + L": " + W(name) + L" - " + W(text));
        g_repReport += r.copy + "\n";
        repAddRow(std::move(r));
    };
    if (pr.gameChanged) add(TLK(L"Game updated"), "Crusader Kings III", pr.prevGame + " -> " + pr.nowGame, 1);
    for (auto& i : pr.outdated) add(TLK(L"Made for an older game"), i.name, i.text, 1);
    for (auto& i : pr.updated) add(TLK(L"Updated"), i.name, i.text, 0);
    for (auto& i : pr.added) add(TLK(L"Turned on"), i.name, i.text, 0);
    for (auto& i : pr.removed) add(TLK(L"Gone"), i.name, i.text, i.sev);
    if (g_repRows.empty()) { RepRow r; r.cells = {pr.hasPrev ? TL(L"Nothing has changed since you last pressed Play.") : TL(L"Nothing to compare yet."), L"", L""}; repAddRow(std::move(r)); }
}

static void repColumns() {
    while (ListView_DeleteColumn(hRepList, 0)) {}
    struct Col { const wchar_t* t; int w; };
    std::vector<Col> cols;
    if (g_repKind == 1) { cols.push_back({TLK(L"What"), 190}); cols.push_back({TLK(L"Mod"), 320}); cols.push_back({TLK(L"Details"), 620}); }
    else if (g_repViewForBtn == 0) { cols.push_back({TLK(L"Mod"), 320}); cols.push_back({TLK(L"Errors"), 70}); cols.push_back({TLK(L"Warnings"), 80}); cols.push_back({TLK(L"Different"), 80}); cols.push_back({TLK(L"Most common message"), 620}); }
    else { cols.push_back({TLK(L"Mod"), 260}); cols.push_back({TLK(L"Level"), 70}); cols.push_back({TLK(L"Times"), 60}); cols.push_back({TLK(L"Message"), 560}); cols.push_back({TLK(L"File"), 360}); }
    for (size_t i = 0; i < cols.size(); i++) {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)TL(cols[i].t); c.cx = S(cols[i].w);
        ListView_InsertColumn(hRepList, (int)i, &c);
    }
    ListView_SetColumnWidth(hRepList, (int)cols.size() - 1, LVSCW_AUTOSIZE_USEHEADER);
}
static void repFill() {
    g_repShown.clear();
    wchar_t buf[200] = L"";
    if (hRepFilter) GetWindowTextW(hRepFilter, buf, 199);
    std::wstring q = buf;
    for (auto& c : q) c = (wchar_t)towlower(c);
    for (size_t i = 0; i < g_repRows.size(); i++) if (q.empty() || g_repRows[i].hay.find(q) != std::wstring::npos) g_repShown.push_back((int)i);
    ListView_SetItemCountEx(hRepList, (int)g_repShown.size(), LVSICF_NOINVALIDATEALL);
    InvalidateRect(hRepList, nullptr, TRUE);
    std::wstring st = g_repStatusText;
    if (!q.empty()) st += L"   ·   " + TLF(L"showing {0} of {1}", {g_repShown.size(), g_repRows.size()});
    SetWindowTextW(hRepStatus, st.c_str());
}
static void repRefresh() {
    if (!hRep) return;
    if (g_repKind == 0) repBuildLog(); else repBuildSince();
    repColumns();
    repFill();
    for (int id : {ID_RP_A, ID_RP_B}) if (HWND b = GetDlgItem(hRep, id)) InvalidateRect(b, nullptr, FALSE);
}
static void repLayout() {
    RECT r; GetClientRect(hRep, &r);
    int m = S(12), y = m, x = m, bh = S(30), gap = S(6);
    if (g_repKind == 0) {
        MoveWindow(GetDlgItem(hRep, ID_RP_A), x, y, S(110), bh, TRUE); x += S(110) + gap;
        MoveWindow(GetDlgItem(hRep, ID_RP_B), x, y, S(150), bh, TRUE); x += S(150) + gap;
    }
    int right = r.right - m;
    MoveWindow(GetDlgItem(hRep, ID_RP_RELOAD), right - S(110), y, S(110), bh, TRUE); right -= S(110) + gap;
    MoveWindow(GetDlgItem(hRep, ID_RP_COPY), right - S(150), y, S(150), bh, TRUE); right -= S(150) + gap;
    int fw = right - x - gap;
    MoveWindow(hRepFilter, x + gap, y + S(3), fw < S(100) ? S(100) : fw, bh - S(6), TRUE);
    y += bh + gap;
    MoveWindow(hRepStatus, m, y + S(2), r.right - 2 * m, S(20), TRUE);
    y += S(26);
    MoveWindow(hRepList, m, y, r.right - 2 * m, r.bottom - y - m, TRUE);
    int nc = Header_GetItemCount(ListView_GetHeader(hRepList));
    if (nc > 0) ListView_SetColumnWidth(hRepList, nc - 1, LVSCW_AUTOSIZE_USEHEADER);
}
static void repTheme() {
    const Theme& t = T();
    themeFrame(hRep);
    SetWindowTheme(hRepList, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetWindowTheme(hRepFilter, g_dark ? L"DarkMode_CFD" : nullptr, nullptr);
    ListView_SetBkColor(hRepList, t.list); ListView_SetTextBkColor(hRepList, t.list); ListView_SetTextColor(hRepList, t.text);
    RedrawWindow(hRep, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

static LRESULT CALLBACK RepProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            hRep = h;
            if (g_repKind == 0) { mkBtn(h, L"☰", TL(L"By mod"), ID_RP_A); mkBtn(h, L"≡", TL(L"All messages"), ID_RP_B); }
            mkBtn(h, L"⎘", TL(L"Copy report"), ID_RP_COPY);
            mkBtn(h, L"↻", TL(L"Reload"), ID_RP_RELOAD);
            hRepFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_RP_FILTER, g_inst, nullptr);
            setFont(hRepFilter);
            hRepStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_STATUS, g_inst, nullptr);
            setFont(hRepStatus);
            hRepList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_RP_LIST, g_inst, nullptr);
            setFont(hRepList);
            ListView_SetExtendedListViewStyle(hRepList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowSubclass(hRepList, ListSub, 3, 0);
            repTheme();
            repRefresh();
            repLayout();
            return 0;
        }
        case WM_SIZE: if (hRepList) repLayout(); return 0;
        case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize.x = S(760); mi->ptMinTrackSize.y = S(320); return 0; }
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); paintBackdrop((HDC)w, rc, false); return 1; }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { LRESULT r; if (themeCtlColor(m, w, l, r)) return r; break; }
        case WM_MEASUREITEM: if (((MEASUREITEMSTRUCT*)l)->CtlType == ODT_MENU) { measureMenuItem((MEASUREITEMSTRUCT*)l); return TRUE; } break;
        case WM_DRAWITEM:
            if (((DRAWITEMSTRUCT*)l)->CtlType == ODT_MENU) drawMenuItem((DRAWITEMSTRUCT*)l); else drawButton((DRAWITEMSTRUCT*)l);
            return TRUE;
        case WM_APP + 1: repTheme(); return 0;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_RP_A: case ID_RP_B: g_repViewForBtn = LOWORD(w) == ID_RP_A ? 0 : 1; repRefresh(); break;
                case ID_RP_FILTER: if (HIWORD(w) == EN_CHANGE) repFill(); break;
                case ID_RP_RELOAD: repRefresh(); break;
                case ID_RP_COPY:
                    if (!copyToClipboard(g_repReport.empty() ? "(nothing to copy)" : g_repReport)) info(TL(L"Could not copy to the clipboard (another program may be holding it). Try again."));
                    else SetWindowTextW(hRepStatus, TL(L"Report copied to the clipboard."));
                    break;
                case ID_RPM_COPY: case ID_RPM_REPORT: break;
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR* nh = (NMHDR*)l;
            if (nh->hwndFrom != hRepList) return 0;
            if (nh->code == LVN_GETDISPINFOW) {
                auto* di = (NMLVDISPINFOW*)l;
                if ((di->item.mask & LVIF_TEXT) && di->item.iItem >= 0 && di->item.iItem < (int)g_repShown.size()) {
                    const auto& row = g_repRows[(size_t)g_repShown[(size_t)di->item.iItem]];
                    di->item.pszText = (LPWSTR)(di->item.iSubItem >= 0 && di->item.iSubItem < (int)row.cells.size() ? row.cells[(size_t)di->item.iSubItem].c_str() : L"");
                }
                return 0;
            }
            if (nh->code == NM_DBLCLK && g_repKind == 0 && g_repViewForBtn == 0) {   // a mod: show its messages
                int it = ((NMITEMACTIVATE*)l)->iItem;
                if (it >= 0 && it < (int)g_repShown.size()) {
                    std::wstring key = g_repRows[(size_t)g_repShown[(size_t)it]].key;
                    g_repViewForBtn = 1; SetWindowTextW(hRepFilter, key.c_str()); repRefresh();
                }
                return 0;
            }
            if (nh->code == NM_RCLICK) {
                int it = ((NMITEMACTIVATE*)l)->iItem;
                if (it < 0 || it >= (int)g_repShown.size()) return 0;
                ListView_SetItemState(hRepList, it, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                const auto& row = g_repRows[(size_t)g_repShown[(size_t)it]];
                HMENU mn = CreatePopupMenu();
                AppendMenuW(mn, MF_STRING, ID_RPM_COPY, TL(L"Copy this line"));
                if (g_repKind == 0 && row.mod >= 0) AppendMenuW(mn, MF_STRING, ID_RPM_REPORT, TL(L"Copy all messages for this mod (to send to its author)"));
                POINT pt; GetCursorPos(&pt);
                int cmd = trackMenu(mn, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, h);
                DestroyMenu(mn);
                if (cmd == ID_RPM_COPY) copyToClipboard(row.copy.empty() ? U(row.cells.empty() ? L"" : row.cells[0]) : row.copy);
                else if (cmd == ID_RPM_REPORT && g_logRep) {
                    Playset* ps = active();
                    if (ps && row.mod >= 0 && row.mod < (int)ps->mods.size()) {
                        auto mi = g_info.find(ps->mods[(size_t)row.mod].id);
                        copyToClipboard(logModReport(*g_logRep, row.mod, mi != g_info.end() ? mi->second.name : ps->mods[(size_t)row.mod].id));
                    }
                }
                return 0;
            }
            if (nh->code == NM_CUSTOMDRAW) {
                auto* cd = (NMLVCUSTOMDRAW*)l;
                const Theme& t = T();
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    bool sel = (ListView_GetItemState(hRepList, (int)row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                    cd->clrTextBk = sel ? t.sel : (row & 1) ? t.alt : t.list;
                    cd->clrText = sel ? t.selText : t.text;
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                    size_t row = (size_t)cd->nmcd.dwItemSpec;
                    bool sel = (ListView_GetItemState(hRepList, (int)row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    cd->clrTextBk = sel ? t.sel : (row & 1) ? t.alt : t.list;
                    cd->clrText = sel ? t.selText : t.text;
                    if (!sel && row < g_repShown.size()) {
                        int sv = g_repRows[(size_t)g_repShown[row]].sev;
                        bool levelCell = (g_repKind == 1 && cd->iSubItem == 0) || (g_repKind == 0 && g_repViewForBtn == 1 && cd->iSubItem == 1) || (g_repKind == 0 && g_repViewForBtn == 0 && cd->iSubItem <= 2);
                        if (levelCell) cd->clrText = sv >= 2 ? t.bad : sv == 1 ? t.warn : t.text;
                    }
                    return CDRF_NEWFONT;
                }
            }
            return CDRF_DODEFAULT;
        }
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: hRep = hRepList = hRepFilter = hRepStatus = nullptr; g_logRep.reset(); g_repRows.clear(); g_repShown.clear(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void showReport(int kind) {
    if (hRep) {
        if (g_repKind == kind) { if (IsIconic(hRep)) ShowWindow(hRep, SW_RESTORE); SetForegroundWindow(hRep); repRefresh(); return; }
        DestroyWindow(hRep);   // the other kind is open: replace it
    }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = RepProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCRep";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    g_repKind = kind; g_repViewForBtn = 0;
    CreateWindowExW(0, L"RCRep", kind == 0 ? TL(L"Game log - The Royal Court") : TL(L"Since you last pressed Play - The Royal Court"), WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1120), S(600), hMain, nullptr, g_inst, nullptr);
}

static bool g_sinceShown = false;
// Once per start, after the mods have been read: say in the status line when something changed since Play was last pressed.
static void sinceHint() {
    Playset* ps = active();
    if (!ps || g_settings.playTime <= 0) return;
    PlayReport pr = buildPlayReport(g_settings, *ps, g_info, fpNow(), g_fileIndex, g_gameVer);
    if (pr.empty()) return;
    std::wstring t = TL(L"Since you last played:");
    bool any = false;
    auto part = [&](size_t n, const wchar_t* tmpl) { if (!n) return; t += (any ? L", " : L" ") + TLF(tmpl, {n}); any = true; };
    if (pr.gameChanged) { t += L" " + TLF(L"the game updated to {0}", {pr.nowGame}); any = true; }
    part(pr.updated.size(), TLK(L"{0} mod(s) updated"));
    part(pr.outdated.size(), TLK(L"{0} made for an older game"));
    part(pr.added.size() + pr.removed.size(), TLK(L"{0} turned on or off"));
    t += L". ";
    t += TL(L"Details: Advanced > What changed since I last pressed Play.");
    say(t);
}


// ---------- first-launch tour ----------
struct TourPage { const wchar_t* glyph; const wchar_t* title; const wchar_t* intro; std::vector<const wchar_t*> pts; const wchar_t* tip; };
static const std::vector<TourPage>& tourPages() {
    static const std::vector<TourPage> v = {
        {L"♛", TLK(L"Welcome"),
         TLK(L"The Royal Court is a fast, light mod manager for Crusader Kings III. This short tour shows the features that save you the most trouble."),
         {TLK(L"Your playsets are saved as Paradox Launcher playset files, so nothing is locked in."),
          TLK(L"No installer and no browser. The program goes online only when you press Updates."),
          TLK(L"It never changes your mod files. It reads them to find problems."),
          TLK(L"You can open this tour again at any time from Advanced > Show the quick tour again.")},
         TLK(L"If your mods do not appear, type your Crusader Kings III folder at the bottom (the folder that contains \"mod\") and press Save folder.")},
        {L"☰", TLK(L"Playsets and load order"),
         TLK(L"A playset is a list of mods with an order. The order matters: when two mods change the same thing, the one lower in the list wins."),
         {TLK(L"Tick a mod to turn it on or off. Every change is saved at once."),
          TLK(L"Drag rows, or use Up and Down, to change the order. Hold Ctrl or Shift to select several mods and move them together."),
          TLK(L"Use New, Duplicate, Rename and Delete for your playsets. Duplicate one before you experiment."),
          TLK(L"Click a column header to sort the view. The Notes column flags missing mods, mods made for an older game and conflicts.")},
         TLK(L"The filter box next to the magnifier finds a mod by name as you type.")},
        {L"⇅", TLK(L"Auto Sort"),
         TLK(L"Press Auto Sort and the program proposes a better load order. Nothing changes until you look at the preview and press Apply."),
         {TLK(L"Dependencies go first, then libraries, overhauls, content, graphics, interface, translations and patches."),
          TLK(L"Total conversions load first, and well-known mods such as Rise and Fall are placed where their authors say."),
          TLK(L"Right-click a mod to lock it so Auto Sort never moves it, or to correct its type."),
          TLK(L"Not happy with the result? Undo order puts the list back as it was.")},
         TLK(L"Sorted something wrongly? Advanced > Report a sort problem copies the details so the known-mods list can be fixed.")},
        {L"⚠", TLK(L"Conflicts"),
         TLK(L"Conflicts shows where mods collide, before you spend an evening in a broken game."),
         {TLK(L"By mod pair or by file: mods that ship the same file. The mod loaded last usually wins."),
          TLK(L"By definition: mods that define the same trait, event, define or localization key."),
          TLK(L"Base game: mods that replace a file of the game itself. The game never merges those."),
          TLK(L"Double-click a file to see every mod that ships it and who wins. Right-click a row to let one mod win; the order is saved and checked again.")},
         TLK(L"Every conflict has a severity (High, Medium, Low). Type high, medium or low in the filter box to see only those.")},
        {L"▶", TLK(L"Play and backups"),
         TLK(L"Play starts Crusader Kings III directly with the playset you selected, without the Paradox Launcher."),
         {TLK(L"Steam must be running. The program writes your playset into the game's dlc_load.json first."),
          TLK(L"Your own dlc_load.json is kept next to it as dlc_load.json.rc-original."),
          TLK(L"Your playset is backed up automatically at startup and before Auto Sort."),
          TLK(L"Advanced has Back up now, Restore from a backup and Open backups folder.")},
         TLK(L"Game launch options for each playset (for example -debug_mode) are under Advanced.")},
        {L"⚒", TLK(L"When something goes wrong"),
         TLK(L"Three helpers read the game's own files and tell you where to look. They only read; they never change anything."),
         {TLK(L"Crash helper: after a crash it says what kind of crash it was and which of your mods the last log messages point at."),
          TLK(L"Game log: every error and warning of the game, grouped by the mod it comes from, with a copy button for the mod's author."),
          TLK(L"What changed since I last pressed Play: mods that updated, mods made for an older game, mods turned on or off, a game update."),
          TLK(L"The status line at the bottom tells you when the game crashed or when something changed.")},
         TLK(L"These helpers are in the Advanced menu.")},
        {L"✉", TLK(L"Share, language and look"),
         TLK(L"A few things that make the program yours."),
         {TLK(L"Export > Copy a share code turns a whole playset into one line of text. Import > Paste a share code builds it again for a friend."),
          TLK(L"The language button in the top bar switches the program between eleven languages at once."),
          TLK(L"The sun and moon switch changes between the dark and the light look."),
          TLK(L"Updates checks GitHub when you press it and can update the program in place.")},
         TLK(L"That is the tour. Enjoy your court!")},
    };
    return v;
}

static int g_tourPage = 0, g_tourHot = -1;
struct TourHit { RECT r; int id; };   // id: 0..n-1 a page, 100 back, 101 next, 102 skip
static std::vector<TourHit> g_tourHits;
static HFONT g_tourFonts[4] = {};     // body, title, sub, small; rebuilt whenever the tour opens

static HFONT tourFont(int px, int weight) {
    LOGFONTW lf{};
    GetObjectW(g_font, sizeof lf, &lf);
    lf.lfHeight = -S(px); lf.lfWidth = 0; lf.lfWeight = weight;
    return CreateFontIndirectW(&lf);
}
static void tourFonts(int bodyPx) {
    for (HFONT& f : g_tourFonts) if (f) { DeleteObject(f); f = nullptr; }
    g_tourFonts[0] = tourFont(bodyPx, FW_NORMAL);
    g_tourFonts[1] = tourFont(28, FW_BOLD);
    g_tourFonts[2] = tourFont(bodyPx, FW_SEMIBOLD);
    g_tourFonts[3] = tourFont(12, FW_NORMAL);
}
static int tourTextH(HDC dc, const std::wstring& t, int w) {
    RECT r = {0, 0, w, 0};
    DrawTextW(dc, t.c_str(), -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return r.bottom;
}

static void tourPaint(HWND h) {
    PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bmp);
    const Theme& t = T();
    const auto& pages = tourPages();
    const int n = (int)pages.size();
    g_tourPage = std::max(0, std::min(g_tourPage, n - 1));
    const TourPage& pg = pages[(size_t)g_tourPage];
    g_tourHits.clear();
    FillRect(dc, &rc, g_brBg);
    SetBkMode(dc, TRANSPARENT);
    int sideW = S(230);
    RECT langR = {S(14), rc.bottom - S(64) + S(14), sideW - S(18), rc.bottom - S(64) + S(14) + S(36)};
    // left panel
    RECT side = {0, 0, sideW, rc.bottom};
    FillRect(dc, &side, g_brBanner);
    { HBRUSH gb = CreateSolidBrush(t.gold); RECT ln = {sideW - S(2), 0, sideW, rc.bottom}; FillRect(dc, &ln, gb); DeleteObject(gb); }
    SelectObject(dc, g_fontCrown); SetTextColor(dc, t.gold);
    RECT cr = {S(22), S(18), sideW - S(10), S(58)};
    DrawTextW(dc, L"♛", -1, &cr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, g_fontBold); SetTextColor(dc, t.text);
    RECT nr = {S(56), S(18), sideW - S(10), S(58)};
    DrawTextW(dc, L"The Royal Court", -1, &nr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    int y = S(84);
    {
        using namespace Gdiplus;
        Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
        for (int i = 0; i < n; i++) {
            RECT row = {S(10), y, sideW - S(14), y + S(40)};
            bool cur = i == g_tourPage, hot = i == g_tourHot;
            if (cur || hot) {
                GraphicsPath gp; roundedPath(gp, (float)row.left, (float)row.top, (float)(row.right - row.left), (float)(row.bottom - row.top), (float)S(8));
                SolidBrush b(gc(cur ? t.btnHot : t.btn, cur ? 255 : 160)); g.FillPath(&b, &gp);
            }
            float cx = (float)(row.left + S(22)), cy = (float)((row.top + row.bottom) / 2), rad = (float)S(11);
            SolidBrush cb(gc(i < g_tourPage ? t.ok : (cur ? t.gold : t.border)));
            g.FillEllipse(&cb, cx - rad, cy - rad, rad * 2, rad * 2);
            g_tourHits.push_back({row, i});
            y += S(44);
        }
    }
    for (int i = 0; i < n; i++) {
        RECT row = g_tourHits[(size_t)i].r;
        SelectObject(dc, i < g_tourPage ? g_fontSym : g_tourFonts[3]); SetTextColor(dc, i == g_tourPage ? t.accentText : t.text);
        RECT nb = {row.left + S(11), row.top, row.left + S(33), row.bottom};
        std::wstring num = i < g_tourPage ? L"✓" : std::to_wstring(i + 1);
        if (i >= g_tourPage && i != g_tourPage) SetTextColor(dc, t.muted);
        DrawTextW(dc, num.c_str(), -1, &nb, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        SelectObject(dc, i == g_tourPage ? g_tourFonts[2] : g_tourFonts[0]); SetTextColor(dc, i == g_tourPage ? t.text : t.muted);
        RECT tr = {row.left + S(40), row.top, row.right - S(6), row.bottom};
        DrawTextW(dc, TL(pages[(size_t)i].title), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    {   // language chip: the current language, click to choose another
        using namespace Gdiplus;
        Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath gp; roundedPath(gp, (float)langR.left, (float)langR.top, (float)(langR.right - langR.left), (float)(langR.bottom - langR.top), (float)S(9));
        SolidBrush b(gc(g_tourHot == 103 ? t.btnHot : t.btn)); g.FillPath(&b, &gp);
        Pen pn(gc(t.border), 1.0f); g.DrawPath(&pn, &gp);
        SelectObject(dc, g_fontSym); SetTextColor(dc, t.gold);
        RECT gl = {langR.left + S(10), langR.top, langR.left + S(34), langR.bottom};
        DrawTextW(dc, L"\u25CE", -1, &gl, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        SelectObject(dc, g_tourFonts[0]); SetTextColor(dc, t.text);
        std::wstring ln = W(LANGS[g_lang].native);
        RECT lt = {langR.left + S(36), langR.top, langR.right - S(24), langR.bottom};
        DrawTextW(dc, ln.c_str(), -1, &lt, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(dc, g_fontSym); SetTextColor(dc, t.muted);
        RECT ar = {langR.right - S(26), langR.top, langR.right - S(8), langR.bottom};
        DrawTextW(dc, L"\u25BE", -1, &ar, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        g_tourHits.push_back({langR, 103});
    }
    // right side: bottom bar first (its height is fixed), then the text above it
    int rx = sideW + S(34), rw = rc.right - rx - S(34);
    int barY = rc.bottom - S(64);
    // buttons
    auto button = [&](const std::wstring& label, int id, int x, int w, bool primary) {
        RECT r = {x, barY + S(14), x + w, barY + S(14) + S(36)};
        using namespace Gdiplus;
        Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath gp; roundedPath(gp, (float)r.left, (float)r.top, (float)w, (float)S(36), (float)S(9));
        bool hot = g_tourHot == id;
        COLORREF fill = primary ? (hot ? mix(t.accent, t.text, 12) : t.accent) : (hot ? t.btnHot : t.btn);
        SolidBrush b(gc(fill)); g.FillPath(&b, &gp);
        Pen pn(gc(primary ? fill : t.border), 1.0f); g.DrawPath(&pn, &gp);
        SelectObject(dc, primary ? g_tourFonts[2] : g_tourFonts[0]); SetTextColor(dc, primary ? t.accentText : t.text);
        DrawTextW(dc, label.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        g_tourHits.push_back({r, id});
    };
    auto btnW = [&](const std::wstring& label, bool primary) {
        SelectObject(dc, primary ? g_tourFonts[2] : g_tourFonts[0]);
        SIZE sz{0, 0}; GetTextExtentPoint32W(dc, label.c_str(), (int)label.size(), &sz);
        return std::max(S(96), (int)sz.cx + S(36));
    };
    bool last = g_tourPage == n - 1;
    std::wstring lNext = last ? std::wstring(TL(L"Start")) : std::wstring(TL(L"Next")) + L"  ›", lBack = L"‹  " + std::wstring(TL(L"Back")), lSkip = TL(L"Skip the tour");
    int wNext = btnW(lNext, true), wBack = btnW(lBack, false), wSkip = btnW(lSkip, false);
    int xr = rc.right - S(34);
    button(lNext, 101, xr - wNext, wNext, true); xr -= wNext + S(10);
    if (g_tourPage > 0) { button(lBack, 100, xr - wBack, wBack, false); xr -= wBack + S(10); }
    if (!last) button(lSkip, 102, rx, wSkip, false);
    // text
    int top = S(34);
    SelectObject(dc, g_fontCrown); SetTextColor(dc, t.gold);
    RECT gr = {rx, top, rx + S(44), top + S(44)};
    DrawTextW(dc, pg.glyph, -1, &gr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, g_tourFonts[1]); SetTextColor(dc, t.text);
    RECT ttr = {rx + S(48), top, rx + rw, top + S(44)};
    DrawTextW(dc, TL(pg.title), -1, &ttr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    { HBRUSH gb = CreateSolidBrush(t.gold); RECT ul = {rx, top + S(52), rx + S(64), top + S(55)}; FillRect(dc, &ul, gb); DeleteObject(gb); }
    int y0 = top + S(72);
    SelectObject(dc, g_tourFonts[0]); SetTextColor(dc, t.muted);
    std::wstring intro = TL(pg.intro);
    int ih = tourTextH(dc, intro, rw);
    RECT ir = {rx, y0, rx + rw, y0 + ih};
    DrawTextW(dc, intro.c_str(), -1, &ir, DT_WORDBREAK | DT_NOPREFIX);
    int yb = y0 + ih + S(16);
    for (const wchar_t* pt : pg.pts) {
        std::wstring txt = TL(pt);
        SelectObject(dc, g_tourFonts[0]);
        int bh = tourTextH(dc, txt, rw - S(26));
        SelectObject(dc, g_fontSym); SetTextColor(dc, t.gold);
        RECT mr = {rx, yb, rx + S(22), yb + S(22)};
        DrawTextW(dc, L"◆", -1, &mr, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
        SelectObject(dc, g_tourFonts[0]); SetTextColor(dc, t.text);
        RECT br = {rx + S(26), yb, rx + rw, yb + bh};
        DrawTextW(dc, txt.c_str(), -1, &br, DT_WORDBREAK | DT_NOPREFIX);
        yb += bh + S(14);
    }
    // tip box
    {
        std::wstring tip = TL(pg.tip);
        SelectObject(dc, g_tourFonts[0]);
        int th = tourTextH(dc, tip, rw - S(34));
        int boxH = th + S(24), boxY = std::max(yb + S(6), barY - boxH - S(12));
        using namespace Gdiplus;
        Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath gp; roundedPath(gp, (float)rx, (float)boxY, (float)rw, (float)boxH, (float)S(10));
        SolidBrush b(gc(t.list)); g.FillPath(&b, &gp);
        Pen pn(gc(t.gold, 150), 1.0f); g.DrawPath(&pn, &gp);
        SelectObject(dc, g_fontSym); SetTextColor(dc, t.gold);
        RECT lr = {rx + S(12), boxY + S(12), rx + S(30), boxY + S(34)};
        DrawTextW(dc, L"★", -1, &lr, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
        SelectObject(dc, g_tourFonts[0]); SetTextColor(dc, t.text);
        RECT tr = {rx + S(34), boxY + S(12), rx + rw - S(10), boxY + S(12) + th};
        DrawTextW(dc, tip.c_str(), -1, &tr, DT_WORDBREAK | DT_NOPREFIX);
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bmp); DeleteDC(dc);
    EndPaint(h, &ps);
}

static void tourFinish() {
    if (!g_settings.tutorialDone) { g_settings.tutorialDone = true; saveSettingsNow(); }
    if (g_tour) DestroyWindow(g_tour);
}
static int tourHitAt(int x, int y) {
    POINT p{x, y};
    for (auto& h : g_tourHits) if (PtInRect(&h.r, p)) return h.id;
    return -1;
}
static LRESULT CALLBACK TourProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: tourPaint(h); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_APP + 1: themeFrame(h); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_MOUSEMOVE: {
            int id = tourHitAt((short)LOWORD(l), (short)HIWORD(l));
            if (id != g_tourHot) { g_tourHot = id; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT te{sizeof te, TME_LEAVE, h, 0}; TrackMouseEvent(&te);
            return 0;
        }
        case WM_MOUSELEAVE: if (g_tourHot != -1) { g_tourHot = -1; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_SETCURSOR: {
            POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
            if (tourHitAt(p.x, p.y) >= 0 && LOWORD(l) == HTCLIENT) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
            break;
        }
        case WM_LBUTTONUP: {
            int id = tourHitAt((short)LOWORD(l), (short)HIWORD(l));
            int n = (int)tourPages().size();
            if (id >= 0 && id < n) g_tourPage = id;
            else if (id == 100 && g_tourPage > 0) g_tourPage--;
            else if (id == 101) { if (g_tourPage >= n - 1) { tourFinish(); return 0; } g_tourPage++; }
            else if (id == 102) { tourFinish(); return 0; }
            else if (id == 103) {
                for (auto& hh : g_tourHits) if (hh.id == 103) {
                    POINT tl{hh.r.left, hh.r.top}; ClientToScreen(h, &tl);
                    RECT at = {tl.x, tl.y - S(2), tl.x + (hh.r.right - hh.r.left), tl.y};
                    languageMenu(&at);
                    break;
                }
                return 0;   // the tour window may have been rebuilt in the new language
            }
            else return 0;
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            int n = (int)tourPages().size();
            if (w == VK_RIGHT || w == VK_RETURN || w == VK_SPACE) { if (g_tourPage >= n - 1) { if (w != VK_RIGHT) tourFinish(); return 0; } g_tourPage++; InvalidateRect(h, nullptr, FALSE); }
            else if (w == VK_LEFT && g_tourPage > 0) { g_tourPage--; InvalidateRect(h, nullptr, FALSE); }
            else if (w == VK_ESCAPE) tourFinish();
            return 0;
        }
        case WM_CLOSE: tourFinish(); return 0;
        case WM_DESTROY:
            g_tour = nullptr;
            for (HFONT& f : g_tourFonts) if (f) { DeleteObject(f); f = nullptr; }
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static void showTutorial() {
    if (g_tour) { SetForegroundWindow(g_tour); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = TourProc; wc.hInstance = g_inst; wc.lpszClassName = L"RCTour";
        wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); reg = true;
    }
    g_tourPage = 0; g_tourHot = -1;
    tourFonts(16);
    RECT wr = {0, 0, S(900), S(600)};
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    RECT mr; GetWindowRect(hMain, &mr);
    int w = wr.right - wr.left, hh = wr.bottom - wr.top;
    int x = mr.left + ((mr.right - mr.left) - w) / 2, y = mr.top + ((mr.bottom - mr.top) - hh) / 2;
    g_tour = CreateWindowExW(0, L"RCTour", TL(L"Quick tour - The Royal Court"), style, std::max(0, x), std::max(0, y), w, hh, hMain, nullptr, g_inst, nullptr);
    if (g_tour) { themeFrame(g_tour); SetFocus(g_tour); }
}

// ---------- crash helper ----------
static std::string crashesDir() { std::string d = effectiveDir(); return d.empty() ? "" : (P(d) / "crashes").u8string(); }
static std::wstring crashWhen(const CrashFolder& c) {
    if (c.y <= 0) return W(c.name);
    wchar_t b[64];
    swprintf(b, 64, L"%04d-%02d-%02d %02d:%02d", c.y, c.mo, c.d, c.h, c.mi);
    return b;
}
// Once per start: a crash folder that is newer than the last one the user was told about.
static void crashHint() {
    auto list = listCrashes(crashesDir(), 5);
    if (list.empty()) return;
    const std::string& newest = list[0].name;
    if (g_settings.crashSeen.empty()) { g_settings.crashSeen = newest; saveSettingsNow(); return; }   // crashes from before this program was used are not news
    if (newest <= g_settings.crashSeen) return;
    say(TLF(L"The game crashed on {0}. Advanced > Crash helper shows the likely cause.", {crashWhen(list[0])}));
}

static std::wstring TS(const wchar_t* k) { return TL(k); }
static std::wstring crashReport(const CrashFolder& cf, bool newest) {
    Playset* ps = active();
    CrashData cd = crashRead(cf.path);
    std::string gameLog = cd.gameLog, debugLog = cd.debugLog;
    std::wstring t;
    t += TLF(L"Crash from {0}", {crashWhen(cf)}) + L"   (" + W(cf.name) + L")\r\n\r\n";
    t += TS(L"WHAT HAPPENED") + L"\r\n";
    t += std::wstring(L"  ") + W(tr(crashKindName(cd.kind)));
    if (!cd.code.empty() && cd.code.compare(0, 10, "EXCEPTION_") == 0) t += L"  [" + W(cd.code) + L"]";
    t += L"\r\n\r\n";
    t += W(tr(crashKindAdvice(cd.kind))) + L"\r\n\r\n";
    // logs: the crash folder's own copy; the live log only for the newest crash
    std::wstring note;
    if (gameLog.empty() && newest) {
        std::string dir = effectiveDir();
        if (!dir.empty()) {
            fs::path logs = P(dir) / "logs";
            gameLog = readHeadOrTail(logs / "game.log", 4u << 20, true);
            std::string old = readHeadOrTail(logs / "error.log", 2u << 20, true);
            if (!old.empty()) { gameLog += "\n"; gameLog += old; }
            if (debugLog.empty()) debugLog = readHeadOrTail(logs / "debug.log", 6u << 20, false);
            if (!gameLog.empty()) note = TS(L"The crash folder had no copy of the logs, so the game's current logs are used. If you started the game again after the crash, they may not match.");
        }
    }
    // suspects
    t += TS(L"LIKELY SUSPECTS") + L"\r\n";
    bool suspects = false;
    if (ps && !gameLog.empty()) {
        int enabled = 0, ready = 0;
        for (auto& m : ps->mods) if (m.enabled) { enabled++; auto f = g_fileIndex.find(m.id); if (f != g_fileIndex.end() && f->second.complete) ready++; }
        if (ready < enabled) { if (!g_scanning) maybeScan(true); note += (note.empty() ? L"" : L" ") + TS(L"Your mods' files are still being read, so some messages could not be matched yet. Open this again in a moment.");  }
        LogReport lr = attributeLog(crashLastMessages(gameLog), *ps, g_fileIndex, g_defIndex, g_vanilla.get());
        std::map<int, double> sc = crashScores(lr);
        std::vector<std::pair<double, int>> order;
        for (auto& kv : sc) order.push_back({kv.second, kv.first});
        std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first > b.first; });
        int rank = 0;
        for (auto& o : order) {
            if (rank >= 6) break;
            int mod = o.second;
            size_t er = 0, wa = 0; const LogEntry* top = nullptr;
            for (size_t k = 0; k < lr.parse.entries.size(); k++) if (lr.modOf[k] == mod) {
                const LogEntry& e = lr.parse.entries[k];
                (e.level == 'E' ? er : wa) += (size_t)e.count;
                if (e.level == 'E' || !top) top = &e;
            }
            auto it = g_info.find(ps->mods[(size_t)mod].id);
            std::wstring nm = W(it != g_info.end() ? it->second.name : ps->mods[(size_t)mod].id);
            t += L"  " + std::to_wstring(++rank) + L".  " + nm + L"   (#" + std::to_wstring(mod + 1) + L")\r\n";
            t += L"      " + TLF(L"{0} errors and {1} warnings among the last messages the game wrote", {er, wa}) + L"\r\n";
            if (top) { std::wstring m = repMessage(*top); if (m.size() > 170) m = m.substr(0, 167) + L"..."; t += L"      " + m + L"\r\n"; }
            suspects = true;
        }
    }
    if (!suspects) t += L"  " + TS(L"None of the last messages the game wrote could be tied to one of your mods.") + L"\r\n";
    if (!note.empty()) t += L"\r\n  " + note + L"\r\n";
    t += L"\r\n";
    // playset then and now
    if (ps && !debugLog.empty()) {
        std::vector<LoadedMod> loaded = parseLoadedMods(debugLog);
        if (!loaded.empty()) {
            std::set<std::string> was, is;
            for (auto& m : loaded) if (m.enabled) was.insert(m.id);
            for (auto& m : ps->mods) if (m.enabled) is.insert(m.id);
            std::vector<std::wstring> added, removed;
            auto nameOf = [&](const std::string& id) { auto it = g_info.find(id); return W(it != g_info.end() ? it->second.name : id); };
            for (auto& id : is) if (!was.count(id)) added.push_back(nameOf(id));
            for (auto& id : was) if (!is.count(id)) { std::wstring n; for (auto& m : loaded) if (m.id == id) n = W(m.name); removed.push_back(n.empty() ? W(id) : n); }
            t += TS(L"THE PLAYSET AT THE TIME") + L"\r\n";
            if (added.empty() && removed.empty()) t += L"  " + TS(L"The game had the same mods turned on that you have turned on now.") + L"\r\n";
            else {
                t += L"  " + TLF(L"Not the playset you have now: {0} mod(s) turned on since, {1} turned off since.", {added.size(), removed.size()}) + L"\r\n";
                for (size_t i = 0; i < added.size() && i < 8; i++) t += L"      + " + added[i] + L"\r\n";
                for (size_t i = 0; i < removed.size() && i < 8; i++) t += L"      - " + removed[i] + L"\r\n";
            }
            t += L"\r\n";
        }
    }
    // what to try
    t += TS(L"WHAT TO TRY") + L"\r\n";
    t += L"  1. " + TS(L"Turn off the top suspect (or the mod you added or updated most recently) and start the game again.") + L"\r\n";
    t += L"  2. " + TS(L"Use Auto Sort and the Conflicts window to look for mods that overwrite each other or load in the wrong order.") + L"\r\n";
    t += L"  3. " + TS(L"Check the mod's Workshop page: is it made for your game version? Do other players report crashes?") + L"\r\n";
    t += L"  4. " + TS(L"If the crash keeps happening, turn mods off in halves to find the one that causes it.") + L"\r\n\r\n";
    if (!cd.frames.empty()) {
        t += TS(L"FIRST LINES OF THE CALL STACK (for bug reports)") + L"\r\n";
        for (auto& f : cd.frames) t += L"  " + W(f) + L"\r\n";
        t += L"\r\n";
    }
    if (!cd.meta.empty()) { t += TS(L"BUILD") + L"\r\n"; for (auto& m : cd.meta) t += L"  " + W(m) + L"\r\n"; t += L"\r\n"; }
    t += TLF(L"Crash folder: {0}", {W(cf.path)}) + L"\r\n";
    t += TS(L"The crash files stay as they are; this window only reads them. Select all (Ctrl+A) and copy (Ctrl+C) to send this text to a mod author.");
    return t;
}

static void crashHelper() {
    std::string dir = effectiveDir();
    if (dir.empty()) { info(TL(L"The CK3 folder is not set. Enter it in the main window first.")); return; }
    auto list = listCrashes(crashesDir(), 12);
    if (list.empty()) { info(TLF(L"No crash found. When the game crashes it writes a folder in {0}.", {W(crashesDir())}).c_str()); return; }
    size_t pick = 0;
    if (list.size() > 1) {
        HMENU m = CreatePopupMenu();
        for (size_t i = 0; i < list.size(); i++) AppendMenuW(m, MF_STRING, (UINT)(7200 + i), ((i == 0 ? std::wstring(TL(L"Newest: ")) : std::wstring()) + crashWhen(list[i])).c_str());
        RECT r; GetWindowRect(hAdv, &r);
        int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, hMain);
        DestroyMenu(m);
        if (cmd < 7200) return;
        pick = (size_t)(cmd - 7200);
    }
    showText(TLF(L"Crash helper - {0}", {crashWhen(list[pick])}), crashReport(list[pick], pick == 0));
    g_settings.crashSeen = list[0].name; saveSettingsNow();
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

static bool processRunning(const wchar_t* exeName);

// ---------- unsubscribe from Steam Workshop mods ----------
// Steam has no command line for this. The only supported way is Valve's Steamworks library (steam_api64.dll, which ships with the game):
// started with the game's app id it talks to the running Steam client and asks it to unsubscribe. Everything is optional: when the library,
// Steam or the functions are not there, the Workshop page of the mod is opened in Steam instead and the user presses Unsubscribe there.
// The program never touches the Paradox launcher or its data.
static std::string steamDllPath() {
    std::string exe = !g_settings.gameExe.empty() ? g_settings.gameExe : g_gameExeUsed;
    if (exe.empty()) exe = findGameExeInSteam();
    if (exe.empty()) return "";
    std::error_code ec;
    fs::path d = P(exe).parent_path();
    for (fs::path c : {d / "steam_api64.dll", d.parent_path() / "steam_api64.dll", d.parent_path() / "binaries" / "steam_api64.dll"})
        if (fs::is_regular_file(c, ec)) return c.u8string();
    return "";
}
// Returns how many unsubscribe requests were handed to Steam; -1 when it could not be done this way (why: a short English reason for the log).
template <class F> static F sym(HMODULE m, const char* n) { return reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(m, n))); }
static int steamUnsubscribe(const std::vector<std::string>& workshopIds, std::string& why) {
    std::string dll = steamDllPath();
    if (dll.empty()) { why = "steam_api64.dll not found"; return -1; }
    SetEnvironmentVariableW(L"SteamAppId", W(CK3_APPID).c_str());
    SetEnvironmentVariableW(L"SteamGameId", W(CK3_APPID).c_str());
    HMODULE m = LoadLibraryW(W(dll).c_str());
    if (!m) { why = "steam_api64.dll could not be loaded"; return -1; }
    using FInit = bool (*)(); using FInitFlat = int (*)(char*); using FVoid = void (*)(); using FUgc = void* (*)(); using FUnsub = unsigned long long (*)(void*, unsigned long long);
    FInit init = sym<FInit>(m, "SteamAPI_Init");
    FInitFlat initFlat = sym<FInitFlat>(m, "SteamAPI_InitFlat");
    FVoid shutdown = sym<FVoid>(m, "SteamAPI_Shutdown"), run = sym<FVoid>(m, "SteamAPI_RunCallbacks");
    FUnsub unsub = sym<FUnsub>(m, "SteamAPI_ISteamUGC_UnsubscribeItem");
    FUgc ugcOf = nullptr;
    for (int v = 40; v >= 8 && !ugcOf; v--) { char nm[64]; snprintf(nm, sizeof nm, "SteamAPI_SteamUGC_v%03d", v); ugcOf = sym<FUgc>(m, nm); }
    if (!ugcOf) ugcOf = sym<FUgc>(m, "SteamAPI_SteamUGC");
    if ((!init && !initFlat) || !shutdown || !run || !unsub || !ugcOf) { why = "the Steamworks functions were not found"; return -1; }
    bool ok = false;
    if (initFlat) { char msg[1024] = {0}; ok = initFlat(msg) == 0; if (!ok) why = std::string("Steam refused: ") + msg; }
    else ok = init();
    if (!ok) { if (why.empty()) why = "Steam did not accept the connection (is Steam running and signed in?)"; return -1; }
    void* ugc = ugcOf();
    int sent = 0;
    if (ugc) for (const std::string& id : workshopIds) { unsub(ugc, std::strtoull(id.c_str(), nullptr, 10)); sent++; }
    else why = "Steam's workshop service is not available";
    for (int i = 0; i < 30; i++) { run(); Sleep(80); }   // let Steam take the requests before the connection closes
    shutdown();
    return ugc ? sent : -1;
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
    o.lpstrTitle = TL(L"Find ck3.exe (in your Crusader Kings III folder, inside \"binaries\")");
    std::wstring filt = L"ck3.exe"; filt += L'\0'; filt += L"ck3.exe"; filt += L'\0'; filt += TL(L"Programs (*.exe)"); filt += L'\0'; filt += L"*.exe"; filt += L'\0';
    o.lpstrFilter = filt.c_str();
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
        err = TLF(L"Windows error {0}", {GetLastError()});
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
    bi.lpszTitle = TL(L"Select your Crusader Kings III folder (the one that contains \"mod\")");
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
    std::wstring filt = TL(L"Paradox Launcher playset (*.json)"); filt += L'\0'; filt += L"*.json"; filt += L'\0'; filt += TL(L"All files (*.*)"); filt += L'\0'; filt += L"*.*"; filt += L'\0';
    o.lpstrFilter = filt.c_str();
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
    {   // Chinese, Japanese and Korean get a font made for them (the default one only borrows glyphs, and shows the wrong regional forms)
        const std::string lc = LANGS[g_lang].code;
        const wchar_t* face = lc == "zh" ? L"Microsoft YaHei UI" : lc == "ja" ? L"Yu Gothic UI" : lc == "ko" ? L"Malgun Gothic" : nullptr;
        if (face) { wcscpy(base.lfFaceName, face); base.lfCharSet = DEFAULT_CHARSET; }
    }
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

// Width of a button for its glyph and text, never less than minW (96 dpi units): other languages need more room than English.
static int btnW(HWND h, int minW) {
    HDC dc = GetDC(h);
    std::wstring label = ctlText(h);
    auto g = g_glyph.find(GetDlgCtrlID(h));
    std::wstring glyph = g == g_glyph.end() ? L"" : g->second;
    SIZE gs{0, 0}, ls{0, 0};
    HGDIOBJ of = SelectObject(dc, g_fontSym);
    if (!glyph.empty()) GetTextExtentPoint32W(dc, glyph.c_str(), (int)glyph.size(), &gs);
    SelectObject(dc, GetDlgCtrlID(h) == ID_PLAY ? g_fontBold : g_font);
    if (!label.empty()) GetTextExtentPoint32W(dc, label.c_str(), (int)label.size(), &ls);
    SelectObject(dc, of);
    ReleaseDC(h, dc);
    int w = gs.cx + (!glyph.empty() && !label.empty() ? S(8) : 0) + ls.cx + S(36);
    return std::max(S(minW), w);
}
static int labelW(HWND h, int minW) {
    HDC dc = GetDC(h);
    HGDIOBJ of = SelectObject(dc, g_font);
    std::wstring t = ctlText(h);
    SIZE sz{0, 0};
    if (!t.empty()) GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
    SelectObject(dc, of);
    ReleaseDC(h, dc);
    return std::max(S(minW), (int)sz.cx + S(6));
}

static int g_needClientW = 0;   // client width the current language's top rows need (96 dpi scaled); raises the minimum window size
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
    int wLog = btnW(hLog, 126), wAdv = btnW(hAdv, 116), wUpd = btnW(hUpd, 104), wLang = btnW(hLang, 76);
    place(hLog, rx - wLog, by, wLog, S(32)); rx -= wLog + gap;
    place(hAdv, rx - wAdv, by, wAdv, S(32)); rx -= wAdv + gap;
    place(hUpd, rx - wUpd, by, wUpd, S(32)); rx -= wUpd + gap;
    place(hLang, rx - wLang, by, wLang, S(32)); rx -= wLang + gap;
    place(hResync, rx - S(36), by, S(36), S(32));

    int y = g_bannerH + m, x = m;
    int wL1 = labelW(hL1, 52);
    int wNew = btnW(hNew, 76), wDup = btnW(hDup, 110), wRen = btnW(hRen, 98), wDel = btnW(hDel, 90), wExp = btnW(hExport, 96), wImp = btnW(hImport, 96), wPlay = btnW(hPlay, 140);
    int fixed1 = m * 2 + wL1 + S(4) + gap * 2 + wNew + gap + wDup + gap + wRen + gap + wDel + gap * 2 + wExp + gap + wImp + gap * 2 + wPlay;
    int wCombo = std::max(S(110), std::min(S(220), Wd - fixed1));
    place(hL1, x, y + S(7), wL1, S(20)); x += wL1 + S(4);
    place(hCombo, x, y + S(2), wCombo, S(300)); x += wCombo + gap * 2;
    place(hNew, x, y, wNew, rowH); x += wNew + gap;
    place(hDup, x, y, wDup, rowH); x += wDup + gap;
    place(hRen, x, y, wRen, rowH); x += wRen + gap;
    place(hDel, x, y, wDel, rowH); x += wDel + gap * 2;
    place(hExport, x, y, wExp, rowH); x += wExp + gap;
    place(hImport, x, y, wImp, rowH);
    place(hPlay, Wd - m - wPlay, y, wPlay, rowH);

    y += rowH + gap + S(2); x = m;
    int wOn = btnW(hAllOn, 140), wOff = btnW(hAllOff, 146), wCf = btnW(hConflicts, 120), wSort = btnW(hSort, 124), wUndo = btnW(hUndo, 112), wDown = btnW(hDown, 90), wUp = btnW(hUp, 80);
    int fixed2 = m * 2 + S(30) + gap * 2 + wOn + gap + wOff + gap * 2 + wCf + gap + wSort + gap + wUndo + gap * 2 + wUp + gap + wDown + gap * 2 + S(190);
    int wFilter = std::max(S(90), std::min(S(230), Wd - fixed2 + S(110)));
    g_needClientW = std::max(fixed1 + S(110), fixed2 - S(190) + S(90) + S(190));
    place(hL2, x, y + S(4), S(24), S(24)); x += S(30);
    place(hFilter, x, y + S(2), wFilter, rowH - S(4)); x += wFilter + gap * 2;
    place(hAllOn, x, y, wOn, rowH); x += wOn + gap;
    place(hAllOff, x, y, wOff, rowH); x += wOff + gap * 2;
    place(hConflicts, x, y, wCf, rowH); x += wCf + gap;
    place(hSort, x, y, wSort, rowH); x += wSort + gap;
    place(hUndo, x, y, wUndo, rowH); x += wUndo + gap * 2;
    rx = Wd - m - wDown;
    place(hDown, rx, y, wDown, rowH);
    rx -= gap + wUp;
    place(hUp, rx, y, wUp, rowH);
    place(hCount, x, y + S(7), std::max(S(20), rx - gap * 2 - x), S(20));

    y += rowH + gap + S(2);
    int bottom = Ht - m - rowH - gap - S(22) - gap;
    g_listFrame = {m, y, Wd - m, bottom};
    place(hList, m + 1, y + 1, Wd - 2 * m - 2, bottom - y - 2);
    int sy = bottom + gap;
    int wVer = std::max(S(220), labelW(hGameVer, 220));
    place(hGameVer, Wd - m - wVer, sy, wVer, S(20));
    place(hStatus, m, sy, Wd - 2 * m - wVer - S(10), S(20));
    int dy = sy + S(22) + gap;
    int wL3 = labelW(hL3, 80), wBr = btnW(hBrowse, 110), wSv = btnW(hSaveDir, 120);
    place(hL3, m, dy + S(7), wL3, S(20));
    place(hDir, m + wL3 + S(4), dy + S(2), std::max(S(60), Wd - 2 * m - wL3 - S(4) - wBr - wSv - 2 * gap), rowH - S(4));
    place(hBrowse, Wd - m - wSv - gap - wBr, dy, wBr, rowH);
    place(hSaveDir, Wd - m - wSv, dy, wSv, rowH);
    if (g_dwp) { EndDeferWindowPos(g_dwp); g_dwp = nullptr; }
    resizeCols();
    RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static HWND mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hMain, (HMENU)(INT_PTR)id, g_inst, nullptr);
    setFont(h);
    return h;
}

// ---------- language: choosing it, and putting the texts on the controls ----------
static int detectSystemLang() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {0};
    if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0) return langFromTag(U(name));
    return 0;
}
// The language to use: RC_LANG (tests; "zz" is the marker language), the saved choice, otherwise Windows' own language.
static void initLanguage() {
    g_langPseudo = false;
    int idx = -1;
    if (const char* e = getenv("RC_LANG"); e && *e) { if (std::string(e) == "zz") { g_langPseudo = true; idx = 0; } else idx = langFromCode(e); }
    if (idx < 0 && g_settings.language != "auto") idx = langFromCode(g_settings.language);
    if (idx < 0) idx = detectSystemLang();
    setLanguage(idx);
    g_wcache.clear();
}
static void setGameVerText() {
    std::wstring v = g_gameVer.empty() ? std::wstring(TL(L"unknown")) : W(g_gameVer);
    SetWindowTextW(hGameVer, TLF(L"CK3 version: {0}", {v}).c_str());
}
static std::set<HWND> g_tipped;
static void setTip(HWND c, const wchar_t* text) {
    TTTOOLINFOW ti{};
    ti.cbSize = sizeof ti; ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND; ti.hwnd = hMain; ti.uId = (UINT_PTR)c; ti.lpszText = (LPWSTR)text;
    SendMessageW(hTip, g_tipped.insert(c).second ? TTM_ADDTOOLW : TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
}
static std::wstring langShortName() {   // "EN", "PT", "ZH" ... for the button
    std::string c = LANGS[g_lang].code;
    c = c.substr(0, c.find('-'));
    for (auto& ch : c) ch = (char)toupper((unsigned char)ch);
    return W(c);
}
// Sets every text of the main window in the active language: labels, buttons, column headers and tooltips.
static void ensureWide() {   // a longer translation can need more width than the window has: widen it (never when maximized)
    if (!hMain || IsZoomed(hMain) || IsIconic(hMain)) return;
    RECT wr, cr; GetWindowRect(hMain, &wr); GetClientRect(hMain, &cr);
    if (cr.right >= g_needClientW) return;
    SetWindowPos(hMain, nullptr, wr.left, wr.top, (wr.right - wr.left) + (g_needClientW - cr.right), wr.bottom - wr.top, SWP_NOZORDER | SWP_NOACTIVATE);
}
static void applyUiText() {
    auto st = [](HWND h, const wchar_t* t) { SetWindowTextW(h, t); };
    st(hL1, TL(L"Playset")); st(hL3, TL(L"CK3 folder"));
    st(hNew, TL(L"New")); st(hDup, TL(L"Duplicate")); st(hRen, TL(L"Rename")); st(hDel, TL(L"Delete"));
    st(hExport, TL(L"Export")); st(hImport, TL(L"Import")); st(hPlay, TL(L"Play"));
    st(hAdv, TL(L"Advanced")); st(hLog, TL(L"Changelog")); st(hUpd, TL(L"Updates"));
    st(hAllOn, TL(L"Enable shown")); st(hAllOff, TL(L"Disable shown"));
    st(hConflicts, TL(L"Conflicts")); st(hSort, TL(L"Auto Sort")); st(hUndo, TL(L"Undo order"));
    st(hUp, TL(L"Up")); st(hDown, TL(L"Down")); st(hBrowse, TL(L"Browse")); st(hSaveDir, TL(L"Save folder"));
    st(hLang, langShortName().c_str());
    const wchar_t* heads[7] = {TLK(L"#"), TLK(L"Mod"), TLK(L"Version"), TLK(L"Game Version"), TLK(L"Source"), TLK(L"Type"), TLK(L"Notes")};
    for (int i = 0; i < 7; i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT;
        c.pszText = (LPWSTR)TL(heads[i]);
        ListView_SetColumn(hList, i, &c);
    }
    setTip(hNew, TL(L"Create an empty playset"));
    setTip(hDup, TL(L"Copy the selected playset under a new name"));
    setTip(hRen, TL(L"Rename the selected playset"));
    setTip(hDel, TL(L"Delete the selected playset (your mods are not touched)"));
    setTip(hExport, TL(L"Save this playset as a Paradox Launcher playset file"));
    setTip(hImport, TL(L"Load a Paradox Launcher playset file"));
    setTip(hPlay, TL(L"Start Crusader Kings III with this playset (skips the launcher)"));
    setTip(hAdv, TL(L"Backups, compare playsets, launch options, saves, updates and the diagnostics log"));
    setTip(hLog, TL(L"What changed in each version"));
    setTip(hUpd, TL(L"Check GitHub for a newer version of The Royal Court, and for a newer known-mods list"));
    setTip(hAllOn, TL(L"Enable every mod currently shown in the list"));
    setTip(hAllOff, TL(L"Disable every mod currently shown in the list"));
    setTip(hConflicts, TL(L"Find mods that change the same files or definitions"));
    setTip(hSort, TL(L"Propose a better load order (you see a preview first)"));
    setTip(hUndo, TL(L"Go back to the load order from before the last Auto Sort or the last move made from the conflicts window"));
    setTip(hBrowse, TL(L"Pick your Crusader Kings III folder"));
    setTip(hSaveDir, TL(L"Remember this folder"));
    setTip(hResync, TL(L"Rescan the mod folder (use after subscribing to a mod while the app is open)"));
    setTip(hTheme, TL(L"Switch between dark and light"));
    setTip(hUp, TL(L"Move the selected mod(s) up in the load order"));
    setTip(hDown, TL(L"Move the selected mod(s) down in the load order"));
    setTip(hLang, TL(L"Language"));
    setGameVerText();
}
static void refreshFonts() {
    EnumChildWindows(hMain, [](HWND c, LPARAM) -> BOOL { SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE); return TRUE; }, 0);
    SendMessageW(hL2, WM_SETFONT, (WPARAM)g_fontCrown, TRUE);
}
// Switches the language while the program runs. Windows that show results (conflicts, reports, changelog) are closed; they open again in the new language.
static void setUiLanguage(const std::string& code) {
    confQuiesce();   // the background threads read the translations
    g_settings.language = code;
    saveSettingsNow();
    initLanguage();
    bool hadTour = g_tour != nullptr; int tourPage = g_tourPage;   // the tour is rebuilt in the new language on the same page
    if (g_tour) DestroyWindow(g_tour);
    for (const wchar_t* cls : {L"RCConf", L"RCRep", L"RCLog", L"RCText"}) if (HWND w = FindWindowW(cls, nullptr)) SendMessageW(w, WM_CLOSE, 0, 0);
    createFonts();
    refreshFonts();
    applyUiText();
    layout();
    ensureWide();
    populate();
    updateCount();
    say(TLF(L"Language: {0}", {LANGS[g_lang].native}));
    logLine(std::string("language: ") + LANGS[g_lang].code + (code == "auto" ? " (automatic)" : ""));
    if (hadTour) { showTutorial(); g_tourPage = tourPage; if (g_tour) InvalidateRect(g_tour, nullptr, FALSE); }
}
static void languageMenu(const RECT* at) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_settings.language == "auto" ? MF_CHECKED : 0), ID_LANG_BASE, TL(L"Automatic (same as Windows)"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < LANG_COUNT; i++)
        AppendMenuW(m, MF_STRING | (g_settings.language == LANGS[i].code ? MF_CHECKED : 0), ID_LANG_BASE + 1 + i, W(LANGS[i].native).c_str());
    RECT r; if (at) r = *at; else GetWindowRect(hLang, &r);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | (at ? TPM_BOTTOMALIGN : TPM_TOPALIGN), r.left, at ? r.top : r.bottom, hMain);
    DestroyMenu(m);
    if (cmd == ID_LANG_BASE) setUiLanguage("auto");
    else if (cmd > ID_LANG_BASE && cmd <= ID_LANG_BASE + LANG_COUNT) setUiLanguage(LANGS[cmd - ID_LANG_BASE - 1].code);
}

static void createControls() {
    // the texts are put on the controls by applyUiText() (also used when the language changes)
    hL1 = mk(L"STATIC", L"", 0, ID_L1);
    hCombo = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_COMBO);
    hNew = mkBtn(hMain, L"+", L"", ID_NEW);
    hDup = mkBtn(hMain, L"❐", L"", ID_DUP);
    hRen = mkBtn(hMain, L"✎", L"", ID_REN);
    hDel = mkBtn(hMain, L"✕", L"", ID_DEL);
    hExport = mkBtn(hMain, L"⇧", L"", ID_EXPORT);
    hImport = mkBtn(hMain, L"⇩", L"", ID_IMPORT);
    hPlay = mkBtn(hMain, L"▶", L"", ID_PLAY);
    hResync = mkBtn(hMain, L"↻", L"", ID_RESYNC);
    hLang = mkBtn(hMain, L"\U0001F310", L"", ID_LANG);
    hAdv = mkBtn(hMain, L"⚙", L"", ID_ADV);
    hLog = mkBtn(hMain, L"☰", L"", ID_LOG);
    hUpd = mkBtn(hMain, L"⬆", L"", ID_UPDATE);
    hTheme = mkBtn(hMain, L"", L"", ID_THEME);
    hL2 = mk(L"STATIC", L"⌕", SS_CENTER, ID_L2);
    SendMessageW(hL2, WM_SETFONT, (WPARAM)g_fontCrown, TRUE);
    hFilter = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_FILTER, WS_EX_CLIENTEDGE);
    hAllOn = mkBtn(hMain, L"☑", L"", ID_ALLON);
    hAllOff = mkBtn(hMain, L"☐", L"", ID_ALLOFF);
    hConflicts = mkBtn(hMain, L"⚔", L"", ID_CONFLICTS);
    hSort = mkBtn(hMain, L"⇅", L"", ID_SORT);
    hUndo = mkBtn(hMain, L"↶", L"", ID_UNDOSORT);
    EnableWindow(hUndo, FALSE);
    hCount = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_COUNT);
    hUp = mkBtn(hMain, L"▲", L"", ID_UP);
    hDown = mkBtn(hMain, L"▼", L"", ID_DOWN);
    hList = mk(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP, ID_LIST, 0);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(hList, ListSub, 2, 0);
    int widths[7] = {S(64), S(400), S(100), S(110), S(90), S(90), S(280)};
    for (int i = 0; i < 7; i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPWSTR)L"";
        c.cx = widths[i];
        ListView_InsertColumn(hList, i, &c);
    }
    if (g_settings.colW.size() == 7)
        for (int c : {0, 2, 3, 4, 5}) if (g_settings.colW[(size_t)c] >= 30 && g_settings.colW[(size_t)c] <= 800) ListView_SetColumnWidth(hList, c, g_settings.colW[(size_t)c]);
    hStatus = mk(L"STATIC", L"", SS_ENDELLIPSIS, ID_STATUS);
    hGameVer = mk(L"STATIC", L"", SS_OWNERDRAW, ID_GAMEVER);
    hL3 = mk(L"STATIC", L"", 0, ID_L3);
    hDir = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_DIR, WS_EX_CLIENTEDGE);
    hBrowse = mkBtn(hMain, L"…", L"", ID_BROWSE);
    hSaveDir = mkBtn(hMain, L"✓", L"", ID_SAVEDIR);
    hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, hMain, nullptr, g_inst, nullptr);
    applyUiText();
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
    if (dir.empty()) { info(TL(L"The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder.")); return; }
    if (processRunning(L"ck3.exe")) { info(TL(L"Crusader Kings III is running. Close the game first, then try again.")); return; }
    SaveScan sc = scanSaves(dir);
    if (sc.entries.empty()) { info(TLF(L"No saves found in {0}", {sc.folder.wstring()}).c_str()); return; }
    std::wstring q = TLF(L"Delete ALL saved games?\n\n{0} save file(s), {1}\nin {2}\n\nThey are moved to the Recycle Bin (anything too big for it is deleted permanently). If Steam Cloud sync is on, Steam may bring cloud copies back.",
                         {sc.files, sizeText(sc.bytes), sc.folder.wstring()});
    if (MessageBoxW(hMain, q.c_str(), TL(L"Delete all saves"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
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
        say(TLF(L"Some saves could not be deleted ({0} item(s) left in the save folder).", {after.entries.size()}));
    else
        say(TLF(L"Deleted {0} save file(s) ({1}) to the Recycle Bin.", {sc.files, sizeText(sc.bytes)}));
}

static void restoreBackup() {
    Playset* ps = active();
    if (!ps) return;
    if (listBackups(ps->name).empty()) { info(TL(L"This playset has no backups yet. One is made automatically before every Auto Sort, and when the app starts if the playset changed.")); return; }
    std::wstring dir = backupsDir(ps->name).wstring();
    wchar_t buf[MAX_PATH * 2] = {0};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hMain;
    o.lpstrTitle = TL(L"Restore this playset from a backup (the newest are listed first by date in the name)");
    std::wstring filt = TL(L"Playset backups (*.json)"); filt += L'\0'; filt += L"*.json"; filt += L'\0';
    o.lpstrFilter = filt.c_str();
    o.lpstrInitialDir = dir.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = (DWORD)(sizeof buf / sizeof buf[0]);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o)) return;
    std::string text;
    if (!readFile(P(U(buf)), text, 5u << 20)) { info(TL(L"Could not read that backup.")); return; }
    ImportResult r = parsePlaysetFile(text, g_mods);
    if (!r.ok) { info(W(r.error).c_str()); return; }
    std::wstring q = TLF(L"Restore \"{0}\" to the load order and enabled mods saved in:\n{1}\n\nYour current state is backed up first, so you can come back.", {ps->name, fileStem(U(buf))});
    if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    backupPlayset(*ps, g_info, "before restore");
    Playset from = r.playset;
    syncPlayset(from, g_mods);
    applyBackupOrder(*ps, from);
    g_undoIds.clear();
    saveActive(); populate();
    say(TL(L"Playset restored from the backup."));
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

// The network threads store their error texts in English (the text table is not safe to use from another thread);
// the main thread turns them into the shown language here.
static std::wstring trErr(const std::wstring& e) {
    if (e == L"Bad update address.") return TL(L"Bad update address.");
    if (e == L"Could not start the Windows network library.") return TL(L"Could not start the Windows network library.");
    if (e == L"Could not reach GitHub.") return TL(L"Could not reach GitHub.");
    if (e == L"Could not reach GitHub (are you online?).") return TL(L"Could not reach GitHub (are you online?).");
    if (e == L"No release was found on GitHub yet.") return TL(L"No release was found on GitHub yet.");
    if (e == L"GitHub is limiting requests right now. Try again in a little while.") return TL(L"GitHub is limiting requests right now. Try again in a little while.");
    if (e == L"The connection was interrupted.") return TL(L"The connection was interrupted.");
    if (e == L"The download is larger than expected.") return TL(L"The download is larger than expected.");
    if (e == L"GitHub's answer could not be read.") return TL(L"GitHub's answer could not be read.");
    if (e == L"This release has no update file attached.") return TL(L"This release has no update file attached.");
    if (e == L"The release's checksum list does not include the program.") return TL(L"The release's checksum list does not include the program.");
    if (e == L"The downloaded file is not a program.") return TL(L"The downloaded file is not a program.");
    if (e == L"The downloaded file does not match its checksum, so it was thrown away.") return TL(L"The downloaded file does not match its checksum, so it was thrown away.");
    static const wchar_t pre[] = L"GitHub answered with error ";
    if (e.compare(0, wcslen(pre), pre) == 0 && e.size() > wcslen(pre) + 1)
        return TLF(L"GitHub answered with error {0}.", {e.substr(wcslen(pre), e.size() - wcslen(pre) - 1)});
    return e;
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
    if (!silent) say(TL(L"Checking GitHub for a newer version..."));
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
    if (!writeFile(P(U(neu)), data)) { err = TL(L"Could not write next to the program (is it in a protected folder like Program Files?). Download the new version from GitHub instead."); return false; }
    DeleteFileW(old.c_str());
    if (!MoveFileExW(self.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(neu.c_str()); err = TLF(L"Could not replace the program file (Windows error {0}).", {GetLastError()}); return false; }
    if (!MoveFileExW(neu.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING);   // put the old one back
        DeleteFileW(neu.c_str());
        err = TL(L"Could not put the new program in place.");
        return false;
    }
    std::wstring args = L"--wait-pid " + std::to_wstring(GetCurrentProcessId());
    if (!startProgram(U(self), err, U(args))) {
        err = TLF(L"The update was installed but the new copy could not be started ({0}). Start the program again.", {err});
        return false;
    }
    return true;
}
using TaskDialogIndirectFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
static void showUpdateDialog(const ReleaseInfo& r) {
    bool canInstall = r.asset(UPDATE_EXE_ASSET) && r.asset(UPDATE_SUMS_ASSET);
    std::wstring head = TLF(L"Version {0} is available", {r.tag});
    std::wstring body = TLF(L"You have version {0}.", {VERSION}) + L" " + (canInstall ? TL(L"\"Update now\" downloads the new program, checks it against the release's checksum, replaces this one and restarts. Your playsets and settings are not touched.") : TL(L"Open the release page to download it."));
    std::wstring notes = plainNotes(r.body);
    int choice = IDCANCEL;
    static TaskDialogIndirectFn td = (TaskDialogIndirectFn)(void*)GetProcAddress(GetModuleHandleW(L"comctl32.dll"), "TaskDialogIndirect");
    if (td) {
        TASKDIALOG_BUTTON btns[2]; int nb = 0;
        if (canInstall) btns[nb++] = {1001, TL(L"Update now\nDownload, verify and restart")};
        if (!r.pageUrl.empty()) btns[nb++] = {1002, TL(L"Open the release page\nSee what changed and download it yourself")};
        TASKDIALOGCONFIG c{};
        c.cbSize = sizeof c; c.hwndParent = hMain; c.hInstance = g_inst;
        c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_EXPAND_FOOTER_AREA;
        c.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        c.pszWindowTitle = L"The Royal Court"; c.pszMainInstruction = head.c_str(); c.pszContent = body.c_str();
        c.pszMainIcon = TD_INFORMATION_ICON;
        c.cButtons = (UINT)nb; c.pButtons = btns;
        if (!notes.empty()) { c.pszExpandedInformation = notes.c_str(); c.pszCollapsedControlText = TL(L"What's new"); c.pszExpandedControlText = TL(L"Hide"); }
        int pressed = 0;
        if (SUCCEEDED(td(&c, &pressed, nullptr, nullptr))) choice = pressed;
    } else {
        std::wstring q = head + L"\n\n" + body + L"\n\n" + (canInstall ? TL(L"Yes = update now, No = not now.") : TL(L"Yes = open release page, No = not now."));
        if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONINFORMATION) == IDYES) choice = canInstall ? 1001 : 1002;
    }
    if (choice == 1002 && !r.pageUrl.empty()) ShellExecuteW(hMain, L"open", W(r.pageUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (choice == 1001) {
        UpdJob* j = new UpdJob;
        j->rel = r;
        g_updBusy = true;
        say(TLF(L"Downloading version {0}...", {r.tag}));
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
    return TLF(L"Known-mods list updated (revision {0}). Press Auto Sort to use it.", {g_knownRev});
}
static void onUpdateChecked(UpdJob* j) {
    g_updBusy = false;
    std::unique_ptr<UpdJob> job(j);
    std::wstring knownMsg = takeOnlineKnown(job->knownText);
    if (!job->ok) {
        logLine("update check failed: " + U(job->err));
        if (job->silent) return;
        say(knownMsg.empty() ? TL(L"Update check failed.") : knownMsg);
        info((trErr(job->err) + L"\n\n" + TL(L"You can always get the latest version from the GitHub page.") + (knownMsg.empty() ? L"" : L"\n\n" + knownMsg)).c_str());
        return;
    }
    bool newer = compareVersions(job->rel.tag, VERSION) > 0;
    logLine("update check: latest " + job->rel.tag + (newer ? " (newer)" : " (up to date)"));
    if (!newer) {
        if (!job->silent) { say(knownMsg.empty() ? TL(L"You have the latest version.") : knownMsg); info((TLF(L"You have the latest version ({0}).", {VERSION}) + (knownMsg.empty() ? L"" : L"\n\n" + knownMsg)).c_str()); }
        else if (!knownMsg.empty()) say(knownMsg);
        return;
    }
    g_updRel = job->rel;
    if (job->silent) { say(TLF(L"A newer version ({0}) is available - press Updates.", {job->rel.tag})); return; }
    say(TLF(L"A newer version is available: {0}.", {job->rel.tag}));
    showUpdateDialog(job->rel);
}
static void onUpdateDownloaded(UpdJob* j) {
    g_updBusy = false;
    std::unique_ptr<UpdJob> job(j);
    std::wstring err = trErr(job->err);
    if (job->ok && installUpdate(job->exeData, err)) {
        logLine("updated to " + job->rel.tag + ", restarting");
        PostMessageW(hMain, WM_CLOSE, 0, 0);   // saves the window state, then the new copy takes over
        return;
    }
    logLine("update failed: " + U(err));
    say(TL(L"Update failed."));
    info((err + L"\n\n" + TL(L"Your current version was left as it is.")).c_str());
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
    if (!ps || ps->mods.empty()) { info(TL(L"There is no load order to report yet.")); return; }
    std::vector<int> cats;
    for (auto& m : ps->mods) cats.push_back(catOfMod(m.id));
    std::string text = buildSortReport(*ps, g_info, g_known, cats, lockedIds(), g_gameVer, g_knownRev);
    if (!copyToClipboard(text)) { info(TL(L"Could not copy to the clipboard (another program may be holding it). Try again.")); return; }
    logLine("sort report copied (" + std::to_string(ps->mods.size()) + " mods)");
    say(TL(L"Sort report copied to the clipboard."));
    std::wstring q = TLF(L"The load order of \"{0}\" is on your clipboard: mod names, Steam ids, types and which mods the program recognises. No file paths or personal information.\n\nOpen the GitHub issues page now? Write what you expected at the bottom and paste (Ctrl+V).", {ps->name});
    if (MessageBoxW(hMain, q.c_str(), TL(L"Report a sort problem"), MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(hMain, L"open", W(std::string("https://github.com/") + UPDATE_REPO + "/issues/new").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::string pasteFromClipboard() {
    std::string out;
    if (!OpenClipboard(hMain)) return out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) {
            size_t n = 0; while (n < 2000000 && p[n]) n++;
            out = U(std::wstring(p, n));
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

// Share code from the clipboard -> a new playset. Mods you do not have are kept in the list (shown as not installed).
static void importShareCode() {
    std::string text = pasteFromClipboard();
    if (findShareCode(text).empty()) { info(TL(L"There is no share code on the clipboard. Copy one (it starts with RC1:) and try again.")); return; }
    ShareDecode d = decodeShareCode(text);
    if (!d.ok) { info(W(d.error).c_str()); return; }
    Playset p = playsetFromShare(d, g_mods);
    p.name = uniqueName(g_playsets, p.name.empty() ? std::string("Shared playset") : sanitizeFileName(p.name));
    syncPlayset(p, g_mods);
    std::vector<std::pair<std::string, std::string>> missing;   // Workshop id, name
    int missingLocal = 0;
    for (auto& m : p.mods) {
        if (!m.enabled || g_info.count(m.id)) continue;
        std::string sid = steamIdOf(m.id);
        if (!sid.empty()) missing.push_back({sid, m.name});
        else missingLocal++;
    }
    if (!savePlayset(p, g_info)) { info(TL(L"Could not save the imported playset.")); return; }
    g_playsets.push_back(p); sortPlaysets();
    g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
    logLine("imported a share code: " + std::to_string(p.mods.size()) + " mods, " + std::to_string(missing.size()) + " Workshop mods not installed");
    std::wstring msg = TLF(L"Imported \"{0}\" from the share code ({1} mods)", {p.name, p.mods.size()});
    if (!missing.empty() || missingLocal) msg += L"; " + TLF(L"{0} not installed", {missing.size() + (size_t)missingLocal});
    say(msg + L".");
    if (missing.empty() && !missingLocal) return;
    std::wstring t = TLF(L"The playset was imported, but {0} enabled mod(s) are not installed on this computer:", {missing.size() + (size_t)missingLocal}) + L"\n\n";
    size_t shown = 0;
    for (auto& mm : missing) { if (shown++ >= 14) break; t += L"  \u2022 " + (mm.second.empty() ? TLF(L"Workshop mod {0}", {mm.first}) : W(mm.second)) + L"\n"; }
    if (missing.size() > 14) t += L"  " + TLF(L"... and {0} more", {missing.size() - 14}) + L"\n";
    if (missingLocal) t += L"  \u2022 " + TLF(L"{0} local mod(s) that are not Workshop mods (ask the sender for them)", {missingLocal}) + L"\n";
    if (!missing.empty()) t += L"\n" + std::wstring(TL(L"Subscribe to them on the Steam Workshop, wait for Steam to download them, then press Rescan.\n\nCopy the Workshop links of the missing mods to the clipboard?"));
    if (missing.empty()) { MessageBoxW(hMain, t.c_str(), TL(L"Share code imported"), MB_ICONINFORMATION); return; }
    if (MessageBoxW(hMain, t.c_str(), TL(L"Share code imported"), MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        std::string links;
        for (auto& mm : missing) links += "https://steamcommunity.com/sharedfiles/filedetails/?id=" + mm.first + (mm.second.empty() ? "" : "   " + mm.second) + "\n";
        if (!copyToClipboard(links)) info(TL(L"Could not copy to the clipboard (another program may be holding it). Try again."));
        else say(TL(L"Workshop links copied to the clipboard."));
    }
}

static void crashHelper();
static void advancedMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_ADV_GAMELOG, TL(L"Game log: which mod causes the errors..."));
    AppendMenuW(m, MF_STRING, ID_ADV_SINCE, TL(L"What changed since I last pressed Play..."));
    AppendMenuW(m, MF_STRING, ID_ADV_CRASH, TL(L"Crash helper: why did the game crash..."));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_FOLDER, TL(L"Open playsets folder"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_BACKUP, TL(L"Back up this playset now"));
    AppendMenuW(m, MF_STRING, ID_ADV_RESTORE, TL(L"Restore this playset from a backup..."));
    AppendMenuW(m, MF_STRING, ID_ADV_OPENBACKUPS, TL(L"Open backups folder"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_COMPARE, TL(L"Compare with another playset..."));
    AppendMenuW(m, MF_STRING, ID_ADV_LAUNCH, TL(L"Game launch options for this playset..."));
    if (!g_settings.hidden.empty()) AppendMenuW(m, MF_STRING, ID_ADV_UNHIDE, TLF(L"Show removed mods ({0})", {g_settings.hidden.size()}).c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_DELSAVES, TL(L"Delete all saves..."));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ADV_OPENSAVES, TL(L"Open saves folder"));
    AppendMenuW(m, MF_STRING, ID_ADV_OPENLOGS, TL(L"Open game logs folder"));
    AppendMenuW(m, MF_STRING, ID_ADV_TUTORIAL, TL(L"Show the quick tour again..."));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_settings.checkUpdates ? MF_CHECKED : 0), ID_ADV_AUTOUPD, TL(L"Check for updates when the program starts"));
    AppendMenuW(m, MF_STRING, ID_ADV_SORTREPORT, TL(L"Report a sort problem (copy details)..."));
    AppendMenuW(m, MF_STRING, ID_ADV_LOG, TL(L"Open diagnostics log (for bug reports)"));
    RECT r; GetWindowRect(hAdv, &r);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, hMain);
    DestroyMenu(m);
    std::string dir = effectiveDir();
    if (cmd == ID_FOLDER) ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (cmd == ID_ADV_DELSAVES) deleteAllSaves();
    else if (cmd == ID_ADV_GAMELOG) showReport(0);
    else if (cmd == ID_ADV_SINCE) showReport(1);
    else if (cmd == ID_ADV_CRASH) crashHelper();
    else if (cmd == ID_ADV_TUTORIAL) showTutorial();
    else if (cmd == ID_ADV_BACKUP) {
        Playset* ps = active();
        if (!ps) return;
        std::string f = backupPlayset(*ps, g_info, "manual");
        say(f.empty() ? TL(L"This playset is already backed up exactly as it is now.") : TLF(L"Backup saved: {0}.", {fileStem(f)}));
    }
    else if (cmd == ID_ADV_OPENBACKUPS) {
        Playset* ps = active();
        ShellExecuteW(hMain, L"open", (ps ? backupsDir(ps->name) : P(dataDir()) / "Backups").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (cmd == ID_ADV_AUTOUPD) { g_settings.checkUpdates = !g_settings.checkUpdates; saveSettingsNow(); say(g_settings.checkUpdates ? TL(L"The program will look for updates when it starts.") : TL(L"The program will no longer look for updates by itself.")); }
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
        if (!askText(TL(L"Game launch options"), TL(L"Extra options for the game, e.g. -debug_mode (empty = none):"), v)) return;
        std::string t = U(v);
        while (!t.empty() && t.front() == ' ') t.erase(0, 1);
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (t.empty()) g_settings.launch.erase(ps->name); else g_settings.launch[ps->name] = t;
        saveSettingsNow();
        say(t.empty() ? TL(L"Launch options cleared.") : TLF(L"Launch options saved for \"{0}\".", {ps->name}));
    }
    else if (cmd == ID_ADV_COMPARE) {
        Playset* ps = active();
        if (!ps) return;
        HMENU pm = CreatePopupMenu();
        std::vector<int> idxs;
        for (size_t i = 0; i < g_playsets.size(); i++) if (g_playsets[i].name != ps->name) { idxs.push_back((int)i); AppendMenuW(pm, MF_STRING, 6000 + idxs.size() - 1, W(g_playsets[i].name).c_str()); }
        if (idxs.empty()) { DestroyMenu(pm); info(TL(L"You need a second playset to compare with.")); return; }
        int pick = trackMenu(pm, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, hMain);
        DestroyMenu(pm);
        if (pick < 6000) return;
        const Playset& other = g_playsets[(size_t)idxs[(size_t)(pick - 6000)]];
        PlaysetDiff d = comparePlaysets(*ps, other, g_info);
        auto sect = [&](const std::string& head, const std::vector<std::string>& v) {
            std::wstring o = TLF(L"{0} ({1})", {head, v.size()}) + L"\r\n";
            if (v.empty()) o += L"   -\r\n";
            for (auto& x : v) o += L"   " + W(x) + L"\r\n";
            return o + L"\r\n";
        };
        std::wstring t = TLF(L"\"{0}\"  vs  \"{1}\"", {ps->name, other.name}) + L"\r\n\r\n" + TLF(L"{0} mods are enabled in both.", {d.shared}) + L"  " + (d.sameOrder ? TL(L"Load order of those: identical.") : TL(L"Load order of those: different.")) + L"\r\n\r\n";
        t += sect(trf("Only enabled in {0}", {ps->name}), d.onlyA) + sect(trf("Only enabled in {0}", {other.name}), d.onlyB) + sect(tr("Enabled in one, disabled in the other"), d.enabledDiffers);
        if (!d.sameOrder) t += sect(tr("Mods at a different place in the shared load order"), d.moved);
        showText(TL(L"Compare playsets"), t);
    }
    else if (cmd == ID_ADV_OPENSAVES || cmd == ID_ADV_OPENLOGS) {
        std::error_code ec;
        fs::path p = P(dir) / (cmd == ID_ADV_OPENSAVES ? "save games" : "logs");
        if (dir.empty() || !fs::is_directory(p, ec)) info(TL(L"That folder does not exist yet."));
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
    if (n == 0 && !fs::exists(P(dir) / "mod" / mid, ec)) { err = TLF(L"Could not add \"{0}\" to your mod folder.", {it->second.name}) + (e.empty() ? L"" : L" " + W(e)); logLine("add Workshop mod failed: " + mid + " " + e); return false; }
    logLine("added Workshop mod " + mid);
    it->second.pending = false;
    for (auto& m : g_mods) if (m.id == mid) m.pending = false;
    g_modStamp = modDirStamp();   // our own write must not look like a change made by someone else
    return true;
}

// Mods that are turned off are not used at all, so they sit below the ones that are on (the order inside each group is kept).
// A mod you just turned off goes to the top of the off group; one you just turned on goes to the end of the on group.
static bool sinkDisabled(Playset& ps) {
    std::vector<std::string> before; before.reserve(ps.mods.size());
    for (auto& m : ps.mods) before.push_back(m.id);
    std::stable_partition(ps.mods.begin(), ps.mods.end(), [](const ModRef& m) { return m.enabled; });
    for (size_t i = 0; i < ps.mods.size(); i++) if (ps.mods[i].id != before[i]) return true;
    return false;
}

static void setShown(bool on) {
    Playset* ps = active();
    if (!ps) return;
    std::wstring err;
    for (int idx : g_shown) {
        if (on && !addPendingMod(ps->mods[(size_t)idx].id, err)) continue;   // could not be added: stays off
        ps->mods[(size_t)idx].enabled = on;
    }
    sinkDisabled(*ps);
    saveActive();
    populate();
    if (!err.empty()) say(err);
}

static void info(const wchar_t* text) { MessageBoxW(hMain, text, L"The Royal Court", MB_ICONINFORMATION); }

// A C++ exception must never escape into Windows' message dispatch (that ends the program without a word).
static void reportError(const char* what) {
    std::wstring m = std::wstring(TL(L"Something went wrong, but your playset files are safe.")) + L"\n\n" + (what && *what ? W(what) : std::wstring(TL(L"unknown error")));
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
            if (!askText(TL(L"New playset"), TL(L"Name for the new playset:"), v)) break;
            Playset p;
            p.name = uniqueName(g_playsets, U(v));
            syncPlayset(p, g_mods);
            if (!savePlayset(p, g_info)) { info(TL(L"Could not create the playset file.")); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            say(TLF(L"Created \"{0}\".", {p.name}));
            break;
        }
        case ID_DUP: {
            if (!ps) break;
            std::wstring v = TLF(L"{0} copy", {ps->name});
            if (!askText(TL(L"Duplicate playset"), TL(L"Name for the copy:"), v)) break;
            Playset p = *ps;
            p.name = uniqueName(g_playsets, U(v));
            if (!savePlayset(p, g_info)) { info(TL(L"Could not create the playset file.")); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_REN: {
            if (!ps) break;
            std::wstring v = W(ps->name);
            if (!askText(TL(L"Rename playset"), TL(L"New name:"), v)) break;
            std::string n = sanitizeFileName(U(v));
            if (n == ps->name) break;
            if (nameTaken(g_playsets, n, ps->name)) { info(TL(L"A playset with that name already exists.")); break; }
            std::string oldName = ps->name;
            if (!renamePlayset(*ps, n, g_info)) { info(TL(L"Could not rename the playset file.")); break; }
            if (g_settings.locks.count(oldName)) { g_settings.locks[ps->name] = g_settings.locks[oldName]; g_settings.locks.erase(oldName); }
            if (g_settings.launch.count(oldName)) { g_settings.launch[ps->name] = g_settings.launch[oldName]; g_settings.launch.erase(oldName); }
            if (g_undoPlayset == oldName) g_undoPlayset = ps->name;
            g_settings.active = ps->name;
            sortPlaysets(); saveSettingsNow(); fillCombo(); populate();
            break;
        }
        case ID_DEL: {
            if (!ps) break;
            if (g_playsets.size() < 2) { info(TL(L"You need at least one playset.")); break; }
            std::wstring q = TLF(L"Delete playset \"{0}\"? Its file will be removed. A copy is kept in the Backups folder (Advanced > Open backups folder).", {ps->name});
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
            HMENU mn = CreatePopupMenu();
            AppendMenuW(mn, MF_STRING, ID_EXP_FILE, TL(L"Save as a playset file (.json, also opens in the Paradox launcher)..."));
            AppendMenuW(mn, MF_STRING, ID_EXP_CODE, TL(L"Copy a share code (short, fits in a chat message)"));
            AppendMenuW(mn, MF_STRING, ID_EXP_CODEN, TL(L"Copy a share code with mod names (longer, shows what is missing)"));
            RECT br; GetWindowRect(hExport, &br);
            int xc = trackMenu(mn, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, br.left, br.bottom, hMain);
            DestroyMenu(mn);
            if (xc == ID_EXP_CODE || xc == ID_EXP_CODEN) {
                int n = 0;
                for (auto& m : ps->mods) if (m.enabled) n++;
                std::string code = makeShareCode(*ps, g_info, xc == ID_EXP_CODEN);
                if (!copyToClipboard(code)) { info(TL(L"Could not copy to the clipboard (another program may be holding it). Try again.")); break; }
                say(TLF(L"Share code for \"{0}\" copied ({1} enabled mods, {2} characters). Paste it in a chat; the other person uses Import > Paste a share code.", {ps->name, n, code.size()}));
                break;
            }
            if (xc != ID_EXP_FILE) break;
            std::wstring path;
            if (!pickFile(true, W(sanitizeFileName(ps->name)) + L".json", path)) break;
            // The name written inside the file is the file name you chose, so both always match.
            Playset outPs = *ps;
            std::string stem = fileStem(U(path));
            if (!stem.empty()) outPs.name = stem;
            if (!writeFile(P(U(path)), exportLauncherPlayset(outPs, g_info))) { info(TL(L"Could not write that file.")); break; }
            int n = 0;
            for (auto& m : ps->mods) if (m.enabled) n++;
            say(TLF(L"Exported \"{0}\" ({1} enabled mods) to {2}", {outPs.name, n, path}));
            break;
        }
        case ID_IMPORT: {
            HMENU mn = CreatePopupMenu();
            AppendMenuW(mn, MF_STRING, ID_IMP_FILE, TL(L"Open a playset file (.json)..."));
            AppendMenuW(mn, MF_STRING, ID_IMP_CODE, TL(L"Paste a share code from the clipboard"));
            RECT br; GetWindowRect(hImport, &br);
            int ic = trackMenu(mn, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, br.left, br.bottom, hMain);
            DestroyMenu(mn);
            if (ic == ID_IMP_CODE) { importShareCode(); break; }
            if (ic != ID_IMP_FILE) break;
            std::wstring path;
            if (!pickFile(false, L"", path)) break;
            std::string text;
            if (!readFile(P(U(path)), text, 5u << 20)) { info(TL(L"Could not read that file (is it larger than 5 MB?).")); break; }
            ImportResult r = parsePlaysetFile(text, g_mods);
            if (!r.ok) { info(W(r.error).c_str()); break; }
            Playset p = r.playset;
            std::string stem = fileStem(U(path));  // the imported playset keeps the file's name
            p.name = uniqueName(g_playsets, stem.empty() ? p.name : stem);
            syncPlayset(p, g_mods);
            int missing = 0;
            for (auto& m : p.mods) if (m.enabled && !g_info.count(m.id)) missing++;
            if (!savePlayset(p, g_info)) { info(TL(L"Could not save the imported playset.")); break; }
            g_playsets.push_back(p); sortPlaysets();
            g_settings.active = p.name; saveSettingsNow(); fillCombo(); populate();
            std::wstring msg = TLF(L"Imported \"{0}\"", {p.name});
            if (missing) msg += L": " + TLF(L"{0} mod(s) not installed (shown in the list)", {missing});
            if (r.invalid) msg += L"; " + TLF(L"{0} invalid entries ignored", {r.invalid});
            say(msg + L".");
            break;
        }
        case ID_UP:
        case ID_DOWN: {
            if (orderLocked()) { say(TL(L"Clear the filter and column sorting (click the # header) to change load order.")); break; }
            moveSelection(id == ID_UP);
            break;
        }
        case ID_PLAY: {
            if (!ps) break;
            std::string dir = effectiveDir();
            if (dir.empty()) { info(TL(L"The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder.")); break; }
            std::error_code ec;
            std::string exe = (!g_settings.gameExe.empty() && fs::exists(P(g_settings.gameExe), ec)) ? g_settings.gameExe : findGameExeInSteam();
            if (exe.empty()) {
                std::wstring chosen;
                if (!pickGameExe(chosen)) break;
                if (_wcsicmp(fs::path(chosen).filename().c_str(), L"ck3.exe") != 0) { info(TL(L"Please choose ck3.exe (it is in the \"binaries\" folder of your Crusader Kings III install).")); break; }
                exe = U(chosen);
            }
            if (exe != g_settings.gameExe) { g_settings.gameExe = exe; saveSettingsNow(); }
            if (g_gameVer.empty()) { refreshGameVersion(exe); populate(); }
            if (processRunning(L"ck3.exe")) { info(TL(L"Crusader Kings III is already running. Close the game first, then press Play again.")); break; }
            if (!processRunning(L"steam.exe")) { info(TL(L"Steam needs to be running to start the game without the launcher. Start Steam, then press Play again.")); break; }
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
            if (!startProgram(exe, err, args)) { info(TLF(L"Could not start the game ({0}).", {err}).c_str()); break; }
            for (auto it = g_settings.seen.begin(); it != g_settings.seen.end();) it = g_info.count(it->first) ? std::next(it) : g_settings.seen.erase(it);   // forget mods that are gone
            for (auto& m : ps->mods) if (m.enabled) if (auto f = fpNow().find(m.id); f != fpNow().end()) g_settings.seen[m.id] = f->second;
            recordPlay(g_settings, *ps, g_fileIndex, g_gameVer, (long long)std::time(nullptr));   // what "since you last pressed Play" compares against
            saveSettingsNow(); refreshNotes();
            say(TLF(L"{0}. Starting Crusader Kings III directly (launcher skipped).", {r.message}) + (registered ? L" " + TLF(L"Registered {0} new Steam mod(s) for you.", {registered}) : L"") + (notAdded ? L" " + TLF(L"WARNING: {0} Steam mod(s) could not be added to your mod folder and were left out.", {notAdded}) : L""));
            break;
        }
        case ID_FOLDER: ShellExecuteW(hMain, L"open", playsetsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_LOG: showChangelog(); break;
        case ID_ADV: advancedMenu(); break;
        case ID_LANG: languageMenu(nullptr); break;
        case ID_UPDATE:
            if (g_updBusy) { say(TL(L"Already checking...")); break; }
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
        case ID_CTX_UNSUB: {
            if (!ps) break;
            std::vector<std::string> wids, names;
            for (int i : g_ctxSel) {
                if (i < 0 || i >= (int)ps->mods.size()) continue;
                std::string sid = steamIdOf(ps->mods[(size_t)i].id);
                if (sid.empty()) continue;
                wids.push_back(sid);
                auto it = g_info.find(ps->mods[(size_t)i].id);
                names.push_back(it != g_info.end() ? it->second.name : ps->mods[(size_t)i].id);
            }
            if (wids.empty()) break;
            std::wstring q;
            if (wids.size() == 1) q = TLF(L"Unsubscribe from \"{0}\" on Steam?", {W(names[0])});
            else { q = TLF(L"Unsubscribe from these {0} Workshop mods on Steam?", {wids.size()}); for (size_t k = 0; k < names.size() && k < 8; k++) q += L"\n  • " + W(names[k]); if (names.size() > 8) q += L"\n  " + TLF(L"... and {0} more", {names.size() - 8}); }
            q += L"\n\n" + std::wstring(TL(L"Steam removes the mods' files from your computer and they stop loading in every playset. You can subscribe again on the Steam Workshop at any time."));
            if (MessageBoxW(hMain, q.c_str(), L"The Royal Court", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            if (processRunning(L"ck3.exe")) { info(TL(L"Crusader Kings III is running. Close the game first, then try again.")); break; }
            if (!processRunning(L"steam.exe")) { info(TL(L"Steam is not running. Start Steam and sign in, then try again.")); break; }
            backupAffected([&] { std::vector<std::string> v; for (int i : g_ctxSel) if (i >= 0 && i < (int)ps->mods.size() && !steamIdOf(ps->mods[(size_t)i].id).empty()) v.push_back(ps->mods[(size_t)i].id); return v; }(), "before unsubscribing");
            std::string why;
            HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            int sent = steamUnsubscribe(wids, why);
            SetCursor(oc);
            if (sent > 0) {
                logLine("unsubscribe requested for " + std::to_string(sent) + " mod(s)");
                std::vector<std::string> uids;
                for (int i : g_ctxSel) if (i >= 0 && i < (int)ps->mods.size() && !steamIdOf(ps->mods[(size_t)i].id).empty()) uids.push_back(ps->mods[(size_t)i].id);
                long long now = (long long)std::time(nullptr);
                for (auto& u : uids) { g_settings.unsubbed[u] = now; g_settings.hidden.insert(u); }   // out of the list now; the leftover entry is cleared once Steam has deleted the files
                saveSettingsNow();
                dropMods(uids, false);
                SetTimer(hMain, TIMER_UNSUB, 3000, nullptr);
                populate();
                say(TLF(L"Asked Steam to unsubscribe from {0} mod(s). They are out of your list and playsets now; Steam deletes the files itself.", {sent}));
            } else {
                logLine("unsubscribe through Steamworks failed: " + why);
                for (size_t k = 0; k < wids.size() && k < 5; k++) ShellExecuteW(hMain, L"open", W("steam://url/CommunityFilePage/" + wids[k]).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                info(TL(L"The program could not unsubscribe through Steam directly, so the Workshop page of the mod is opened in Steam instead. Press Unsubscribe there."));
            }
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
                if (files) { auto gi = g_info.find(mid); if (gi == g_info.end() || gi->second.pending) continue; }   // nothing of ours on disk to delete (a download Steam has not registered yet has no entry)
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
                if (files && ws) q = TLF(L"Permanently delete \"{0}\"?\n\nThis erases the mod's entry (its descriptor file in your CK3 mod folder) from your disk and from every playset. This cannot be undone.", {nm}) + L"\n\n" +
                    (mit->second.contentState == 2 ? TL(L"The mod's files are already gone (you unsubscribed on Steam), so this just clears the leftover entry.") : TL(L"The mod is still installed through Steam. Unsubscribe on Steam first, otherwise Steam will bring it back."));
                else if (files) q = TLF(L"Permanently delete \"{0}\"?\n\nIts descriptor file and its folder inside your CK3 mod folder are erased from your disk. This cannot be undone.", {nm});
                else q = TLF(L"Remove \"{0}\" from the list?\n\nNothing is deleted from your disk. The mod disappears from every playset and from this list (Advanced > Show removed mods brings it back).", {nm});
            } else {
                std::wstring cnt = std::to_wstring(ids.size()), list;
                for (size_t k = 0; k < ids.size() && k < 8; k++) list += L"\n  \u2022 " + nameOf(ids[k]);
                if (ids.size() > 8) list += L"\n  " + TLF(L"... and {0} more", {ids.size() - 8});
                if (files) q = TLF(L"Permanently delete these {0} mods?", {cnt}) + list + L"\n\n" + TL(L"Their descriptor files and folders inside your CK3 mod folder are erased from your disk and from every playset. This cannot be undone.\n\nMods that are still installed through Steam come back unless you unsubscribe on Steam first.");
                else q = TLF(L"Remove these {0} mods from the list?", {cnt}) + list + L"\n\n" + TL(L"Nothing is deleted from your disk. They disappear from every playset and from this list (Advanced > Show removed mods brings them back).");
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
                dropMods(done, files);
                if (files) saveSettingsNow();
                populate();
            }
            if (!done.empty()) say(files ? (done.size() == 1 ? TLF(L"Deleted \"{0}\".", {firstName}) : TLF(L"Deleted {0} mods.", {done.size()})) : (done.size() == 1 ? TLF(L"Removed \"{0}\" from the list.", {firstName}) : TLF(L"Removed {0} mods from the list.", {done.size()})));
            if (failed) info(TLF(L"{0} mod(s) could not be fully deleted (are their files open in another program?). They are left in the list; close the other program and try again.", {failed}).c_str());
            break;
        }
        case ID_SORT: {
            if (!ps || ps->mods.size() < 2) { say(TL(L"Nothing to sort.")); break; }
            ensureConflicts();
            SortPlan plan = planSort(*ps, g_info, lockedIds(), g_settings.cats, g_conf.valid ? &g_conf : nullptr, &g_fileIndex, &g_known);
            if (!plan.changed) {
                std::wstring m = TL(L"Already in a good order, nothing to move.");
                for (auto& wn : plan.warnings) m += L"  ⚠ " + W(wn);
                say(m); break;
            }
            if (!showSortPreview(plan, *ps)) { say(TL(L"Auto Sort cancelled, nothing changed.")); break; }
            backupPlayset(*ps, g_info, "before auto sort");
            g_undoIds.clear();
            for (auto& m : ps->mods) g_undoIds.push_back(m.id);
            g_undoPlayset = ps->name;
            std::vector<ModRef> nm;
            for (int o : plan.order) nm.push_back(ps->mods[(size_t)o]);
            ps->mods = nm;
            saveActive(); populate();
            say(TLF(L"Auto Sort moved {0} mods. Press Undo order to go back.", {plan.moves.size()}));
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
            say(TL(L"Load order restored to how it was before the last change."));
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
            if (!effectiveDir().empty()) say(TLF(L"Folder saved. Found {0} installed mods.", {g_mods.size()}));
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
                bool moved = sinkDisabled(*ps);
                saveActive();
                refreshNotes();
                PostMessageW(hMain, WM_APP + 4, 0, 0);
                if (moved) PostMessageW(hMain, WM_APP + 13, 0, 0);   // redraw the list in the new order (not from inside this notification)
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
        say(g_sortCol < 0 ? TL(L"Showing load order.") : TL(L"Sorted view (load order is unchanged). Click # to go back to load order."));
    } else if (h->code == NM_DBLCLK) {
        showDetails(((NMITEMACTIVATE*)l)->iItem);
    } else if (h->code == LVN_BEGINDRAG) {
        if (orderLocked()) { say(TL(L"Clear the filter and column sorting (click the # header) to change load order.")); return 0; }
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
            if (!cancelled) {
                g_confSig.clear(); refreshNotes(); maybeScan(false);
                if (hRep && (g_repWaitIndex || g_repKind == 1)) repRefresh();
                if (!g_sinceShown && !g_scanning) { g_sinceShown = true; sinceHint(); crashHint(); }
            }
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
        case WM_APP + 12: if (!g_settings.tutorialDone) showTutorial(); return 0;
        case WM_APP + 13: try { populate(); } catch (...) {} return 0;
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
            if (w == TIMER_UNSUB) {   // Steam is removing unsubscribed mods: refresh as soon as its folders change (also when this window stays in front)
                if (g_settings.unsubbed.empty()) { KillTimer(h, TIMER_UNSUB); return 0; }
                if (!IsWindowEnabled(h) || g_resyncing) return 0;
                long long now = (long long)std::time(nullptr);
                bool due = g_modStamp && modDirStamp() != g_modStamp;
                for (auto& kv : g_settings.unsubbed) if (now - kv.second > 900) due = true;
                if (due) { try { resync(true); } catch (...) { g_resyncing = false; } }
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
                        const ModRef& mr = ps->mods[(size_t)g_ctxIdx];
            bool locked = g_settings.locks.count(ps->name) && g_settings.locks[ps->name].count(mr.id);
            bool ovr = false; int cur = catOfMod(mr.id, &ovr);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (locked && nsel == 1 ? MF_CHECKED : 0), ID_CTX_LOCK, nsel > 1 ? TLF(L"Lock position of {0} mods (Auto Sort never moves them)", {nsel}).c_str() : TL(L"Lock position (Auto Sort never moves it)"));
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            for (int c = 0; c < CAT_COUNT; c++) AppendMenuW(menu, MF_STRING | (nsel == 1 && ovr && cur == c ? MF_CHECKED : 0), ID_CTX_CAT + c, (nsel > 1 ? TLF(L"Type for all: {0}", {catName(c)}) : TLF(L"Type: {0}", {catName(c)})).c_str());
            AppendMenuW(menu, MF_STRING | (nsel == 1 && !ovr ? MF_CHECKED : 0), ID_CTX_CAT + CAT_COUNT, nsel > 1 ? TL(L"Type for all: automatic") : TL(L"Type: automatic"));
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            bool anyWs = false; for (int i : g_ctxSel) if (i >= 0 && i < (int)ps->mods.size() && !steamIdOf(ps->mods[(size_t)i].id).empty()) anyWs = true;
            if (anyWs) AppendMenuW(menu, MF_STRING, ID_CTX_UNSUB, nsel > 1 ? TLF(L"Unsubscribe from {0} mods on Steam...", {nsel}).c_str() : TL(L"Unsubscribe on Steam..."));
            AppendMenuW(menu, MF_STRING, ID_CTX_REMOVE, nsel > 1 ? TLF(L"Remove {0} mods from list...", {nsel}).c_str() : TL(L"Remove from list..."));
            bool anyDel = false; for (int i : g_ctxSel) if (i >= 0 && i < (int)ps->mods.size()) { auto gi = g_info.find(ps->mods[(size_t)i].id); if (gi != g_info.end() && !gi->second.pending) anyDel = true; }
            if (anyDel) AppendMenuW(menu, MF_STRING, ID_CTX_DELFILES, nsel > 1 ? TLF(L"Delete {0} mods permanently...", {nsel}).c_str() : TL(L"Delete mod permanently..."));
            trackMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, h);
            DestroyMenu(menu);
            return 0;
        }
        case WM_CLOSE: saveWindowState(h); DestroyWindow(h); return 0;
        case WM_ENDSESSION: if (w) saveWindowState(h); return 0;
        case WM_ENTERSIZEMOVE: g_inSizeMove = true; return 0;
        case WM_EXITSIZEMOVE: g_inSizeMove = false; saveWindowState(h); return 0;
        case WM_SIZE:
            if (w != SIZE_MINIMIZED) { layout(); if (w == SIZE_RESTORED) ensureWide(); }
            if (g_windowReady && (w == SIZE_MAXIMIZED || w == SIZE_RESTORED)) saveWindowState(h);   // maximize / restore buttons (a drag-resize saves when the drag ends)
            return 0;
        case WM_DPICHANGED: applyDpi((int)HIWORD(w), (const RECT*)l); return 0;
        case WM_GETMINMAXINFO: {
            auto* mi = (MINMAXINFO*)l; RECT wr, cr; GetWindowRect(h, &wr); GetClientRect(h, &cr);
            int frame = (wr.right - wr.left) - cr.right;
            mi->ptMinTrackSize.x = std::max(S(1200), g_needClientW) + frame; mi->ptMinTrackSize.y = S(420); return 0; }
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
        case WM_DESTROY: g_windowReady = false; stopScan(); confQuiesce();
            { std::wstring mf = wenv(L"RC_LANG_MISSING");   // developer hook: list texts the active language has no translation for
              if (!mf.empty()) { std::ofstream o(U(mf), std::ios::binary); std::lock_guard<std::mutex> lk(g_langMissingLock); for (auto& m : g_langMissing) o << m << "\n"; } }
            PostQuitMessage(0); return 0;
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
    initLanguage();
    createFonts();
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
    if (!g_settings.tutorialDone) PostMessageW(win, WM_APP + 12, 0, 0);   // first start: the quick tour
    SetTimer(win, 78, 20000, nullptr);   // the old program file from an update is removed only once this version has run for a while (until then it is the way back)
    if (g_settings.checkUpdates) startUpdateCheck(true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(win, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    Gdiplus::GdiplusShutdown(g_gdip);
    return (int)msg.wParam;
}

