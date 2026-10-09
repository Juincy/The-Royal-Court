// The Royal Court | CK3 Mod Manager - core logic (no Windows APIs, testable anywhere)
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "lang.hpp"

namespace fs = std::filesystem;

namespace rc {

inline const char* VERSION = "0.18.0";
inline std::string g_appData;  // set by the GUI (%APPDATA%), UTF-8

// ---------- small helpers ----------
inline fs::path P(const std::string& s) { try { return fs::u8path(s); } catch (...) { return fs::path(); } }   // text that is not valid UTF-8 gives an empty path (every file operation on it just fails)

inline bool readFile(const fs::path& p, std::string& out, std::uintmax_t maxBytes = 8u << 20) {
    std::error_code ec;
    std::uintmax_t sz = fs::file_size(p, ec);
    if (ec || sz > maxBytes) return false;  // refuse to load huge files into memory
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
inline bool writeFile(const fs::path& p, const std::string& data) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size());
    f.close();                 // the final flush happens here; a full disk shows up as a failure now
    return !f.fail();
}
// Writes to a temporary file and renames it over the target, so a crash never leaves a half-written file.
inline bool writeFileAtomic(const fs::path& p, const std::string& data) {
    fs::path tmp = p;
    tmp += ".tmp";
    if (!writeFile(tmp, data)) return false;
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}

inline std::string lower(std::string s) {
    for (auto& c : s) if ((unsigned char)c < 128) c = (char)tolower((unsigned char)c);
    return s;
}

// ---------- minimal JSON ----------
struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<J> a;
    std::vector<std::string> keys;
    std::vector<J> vals;
    static J str(std::string v) { J j; j.t = Str; j.s = std::move(v); return j; }
    static J num(double v) { J j; j.t = Num; j.n = v; return j; }
    static J boolean(bool v) { J j; j.t = Bool; j.b = v; return j; }
    static J arr() { J j; j.t = Arr; return j; }
    static J obj() { J j; j.t = Obj; return j; }
    J* get(const std::string& k) {
        for (size_t i = 0; i < keys.size(); i++) if (keys[i] == k) return &vals[i];
        return nullptr;
    }
    void set(const std::string& k, J v) {
        if (J* p = get(k)) *p = std::move(v);
        else { keys.push_back(k); vals.push_back(std::move(v)); }
    }
};

struct Parser {
    const std::string& s;
    size_t p = 0;
    bool ok = true;
    explicit Parser(const std::string& str) : s(str) {}
    void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++; }
    static void utf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    unsigned hex4() {
        unsigned v = 0;
        for (int i = 0; i < 4 && p < s.size(); i++) {
            char c = s[p++]; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else ok = false;
        }
        return v;
    }
    std::string str() {
        std::string o; p++;
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c == '\\' && p < s.size()) {
                char e = s[p++];
                switch (e) {
                    case 'n': o += '\n'; break;
                    case 't': o += '\t'; break;
                    case 'r': o += '\r'; break;
                    case 'b': o += '\b'; break;
                    case 'f': o += '\f'; break;
                    case 'u': {
                        unsigned cp = hex4();
                        if (cp >= 0xD800 && cp < 0xDC00 && p + 1 < s.size() && s[p] == '\\' && s[p + 1] == 'u') {
                            p += 2; unsigned lo = hex4();
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(o, cp); break;
                    }
                    default: o += e;
                }
            } else o += c;
        }
        if (p < s.size()) p++; else ok = false;
        return o;
    }
    J val(int depth = 0) {
        ws();
        J j;
        if (p >= s.size() || depth > 64) { ok = false; return j; }
        char c = s[p];
        if (c == '{') {
            j.t = J::Obj; p++; ws();
            if (p < s.size() && s[p] == '}') { p++; return j; }
            while (ok) {
                ws();
                if (p >= s.size() || s[p] != '"') { ok = false; break; }
                std::string k = str(); ws();
                if (p >= s.size() || s[p] != ':') { ok = false; break; }
                p++;
                J v = val(depth + 1);
                j.keys.push_back(k); j.vals.push_back(std::move(v)); ws();
                if (p < s.size() && s[p] == ',') { p++; continue; }
                if (p < s.size() && s[p] == '}') { p++; break; }
                ok = false;
            }
        } else if (c == '[') {
            j.t = J::Arr; p++; ws();
            if (p < s.size() && s[p] == ']') { p++; return j; }
            while (ok) {
                j.a.push_back(val(depth + 1)); ws();
                if (p < s.size() && s[p] == ',') { p++; continue; }
                if (p < s.size() && s[p] == ']') { p++; break; }
                ok = false;
            }
        } else if (c == '"') { j.t = J::Str; j.s = str(); }
        else if (s.compare(p, 4, "true") == 0) { j.t = J::Bool; j.b = true; p += 4; }
        else if (s.compare(p, 5, "false") == 0) { j.t = J::Bool; p += 5; }
        else if (s.compare(p, 4, "null") == 0) { p += 4; }
        else {
            char* e; j.n = strtod(s.c_str() + p, &e);
            if (e == s.c_str() + p) ok = false; else { j.t = J::Num; p = (size_t)(e - s.c_str()); }
        }
        return j;
    }
};

inline bool parseJson(const std::string& text, J& out) {
    Parser ps(text);
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) ps.p = 3;
    out = ps.val();
    return ps.ok && out.t != J::Null;
}

inline void dumpStr(std::string& o, const std::string& s) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    o += '"';
}
inline void dump(const J& j, std::string& o, int ind = 0) {
    switch (j.t) {
        case J::Null: o += "null"; break;
        case J::Bool: o += j.b ? "true" : "false"; break;
        case J::Num: { char b[40]; snprintf(b, sizeof b, "%.15g", j.n); o += b; break; }
        case J::Str: dumpStr(o, j.s); break;
        case J::Arr:
            if (j.a.empty()) { o += "[]"; break; }
            o += "[\n";
            for (size_t i = 0; i < j.a.size(); i++) {
                o.append((size_t)ind + 1, '\t'); dump(j.a[i], o, ind + 1);
                if (i + 1 < j.a.size()) o += ',';
                o += '\n';
            }
            o.append((size_t)ind, '\t'); o += ']';
            break;
        case J::Obj:
            if (j.keys.empty()) { o += "{}"; break; }
            o += "{\n";
            for (size_t i = 0; i < j.keys.size(); i++) {
                o.append((size_t)ind + 1, '\t'); dumpStr(o, j.keys[i]); o += ": "; dump(j.vals[i], o, ind + 1);
                if (i + 1 < j.keys.size()) o += ',';
                o += '\n';
            }
            o.append((size_t)ind, '\t'); o += '}';
            break;
    }
}

inline void dumpMin(const J& j, std::string& o) {
    switch (j.t) {
        case J::Arr:
            o += '[';
            for (size_t i = 0; i < j.a.size(); i++) { if (i) o += ','; dumpMin(j.a[i], o); }
            o += ']';
            break;
        case J::Obj:
            o += '{';
            for (size_t i = 0; i < j.keys.size(); i++) { if (i) o += ','; dumpStr(o, j.keys[i]); o += ':'; dumpMin(j.vals[i], o); }
            o += '}';
            break;
        default: dump(j, o, 0);
    }
}

// ---------- data model ----------
struct ModRef { std::string id; bool enabled = false; std::string name; };  // name = display name from the playset file
struct Playset { std::string name; std::vector<ModRef> mods; };
struct ModInfo {
    std::string id, name, version, supported, source;
    std::string path, archive;                       // as written in the descriptor
    std::vector<std::string> deps, replacePaths, tags;
    std::string contentDir;                          // resolved folder with the mod's files ("" if unknown / archive)
    int contentState = 0;                            // 0 unknown (archive or no path), 1 folder exists, 2 folder missing
    bool pending = false;                            // found in Steam's Workshop folder, but the Paradox launcher has not created its ugc_ descriptor yet
};
struct Settings {
    std::string ck3Dir, active, gameExe, theme;                       // theme: "dark", "light" or "" (follow Windows)
    std::map<std::string, std::set<std::string>> locks;               // playset name -> mod ids that Auto Sort never moves
    std::map<std::string, int> cats;                                  // mod id -> category chosen by the user (overrides the guess)
    std::set<std::string> hidden;                                     // mods the user removed from the list (never touched on disk)
    std::map<std::string, std::string> launch;                        // playset name -> extra command line options for the game
    std::map<std::string, std::string> seen;                          // mod id -> fingerprint when Play was last pressed (update detection)
    int winX = 0, winY = 0, winW = 0, winH = 0; bool winMax = false;  // remembered window position and size (winW == 0: none)
    std::vector<int> colW;                                            // remembered widths of the list columns (0 = automatic)
    bool checkUpdates = false;                                        // look for a newer release when the program starts (off by default)
    std::string playGameVer, playName;                                // game version and playset name when Play was last pressed
    long long playTime = 0;                                           // when (seconds since 1970; 0 = never)
    std::vector<std::string> playMods;                                // mods that were enabled then
    std::string crashSeen;                                            // newest crash folder name the user has been told about (or that existed at first start)
    bool tutorialDone = false;                                        // the first-launch tutorial has been shown
    std::string language = "auto";                                    // "auto" (follow Windows) or a language code from LANGS
};

// A playset IS a Paradox Launcher playset file: <Playsets folder>/<name>.json. The file name is the playset name.
// Only the launcher's own JSON format is used; there is no separate database.

// ---------- file names ----------
inline bool reservedName(const std::string& n) {
    std::string b = lower(n.substr(0, n.find('.')));
    for (const char* r : {"con", "prn", "aux", "nul"}) if (b == r) return true;
    return b.size() == 4 && (b.rfind("com", 0) == 0 || b.rfind("lpt", 0) == 0) && b[3] >= '1' && b[3] <= '9';
}
inline std::string sanitizeFileName(std::string n) {
    for (auto& c : n) if (std::string("\\/:*?\"<>|").find(c) != std::string::npos || (unsigned char)c < 0x20) c = '_';
    size_t st = 0;
    while (st < n.size() && n[st] == ' ') st++;
    n = n.substr(st);
    auto trim = [&] { while (!n.empty() && (n.back() == ' ' || n.back() == '.')) n.pop_back(); };
    trim();
    if (n.size() > 80) {
        size_t cut = 80;
        while (cut > 0 && ((unsigned char)n[cut] & 0xC0) == 0x80) cut--;  // do not cut inside a UTF-8 character
        n.resize(cut);
        trim();
    }
    if (n.empty()) n = "playset";
    if (reservedName(n)) n += "_";
    return n;
}
// "C:\x\My Playset.json" -> "My Playset"
inline std::string fileStem(const std::string& pathUtf8) { return P(pathUtf8).stem().u8string(); }


// ---------- app data ----------
inline std::string dataDir() {
    std::string d;
    if (const char* e = getenv("RC_DATA_DIR"); e && *e) d = e;
    else d = (g_appData.empty() ? std::string(".") : g_appData) + "/TheRoyalCourt";
    std::error_code ec;
    fs::create_directories(P(d), ec);
    return d;
}
inline fs::path playsetsDir() {
    fs::path d = P(dataDir()) / "Playsets";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}
inline fs::path settingsPath() { return P(dataDir()) / "settings.json"; }

// a number from a file, safely turned into an int (NaN, infinity and huge values are possible in a damaged file)
inline int clampInt(double d) { return !(d == d) ? 0 : d > 1e8 ? 100000000 : d < -1e8 ? -100000000 : (int)d; }
inline void loadSettingsExtra(Settings& st, J& root) {
    if (J* h = root.get("hidden"); h && h->t == J::Arr) for (auto& e : h->a) if (e.t == J::Str) st.hidden.insert(e.s);
    if (J* l = root.get("launch"); l && l->t == J::Obj)
        for (size_t i = 0; i < l->keys.size(); i++) if (l->vals[i].t == J::Str && !l->vals[i].s.empty()) st.launch[l->keys[i]] = l->vals[i].s;
    if (J* l = root.get("seen"); l && l->t == J::Obj)
        for (size_t i = 0; i < l->keys.size(); i++) if (l->vals[i].t == J::Str) st.seen[l->keys[i]] = l->vals[i].s;
    if (J* u = root.get("checkUpdates"); u && u->t == J::Bool) st.checkUpdates = u->b;
    if (J* lp = root.get("lastPlay"); lp && lp->t == J::Obj) {
        if (J* v = lp->get("game"); v && v->t == J::Str) st.playGameVer = v->s.substr(0, 40);
        if (J* v = lp->get("playset"); v && v->t == J::Str) st.playName = v->s.substr(0, 100);
        if (J* v = lp->get("time"); v && v->t == J::Num && v->n >= 0 && v->n < 4e10) st.playTime = (long long)v->n;
        if (J* v = lp->get("mods"); v && v->t == J::Arr) for (auto& e : v->a) if (e.t == J::Str && st.playMods.size() < 5000) st.playMods.push_back(e.s);
    }
    if (J* w = root.get("window"); w && w->t == J::Obj) {
        auto num = [&](const char* k, int& o) { if (J* v = w->get(k); v && v->t == J::Num) o = clampInt(v->n); };
        num("x", st.winX); num("y", st.winY); num("w", st.winW); num("h", st.winH);
        if (J* v = w->get("max"); v && v->t == J::Bool) st.winMax = v->b;
        if (J* c = w->get("cols"); c && c->t == J::Arr) for (auto& e : c->a) st.colW.push_back(e.t == J::Num ? clampInt(e.n) : 0);
    }
}
inline void loadSettings(Settings& st) {
    st = Settings{};
    std::string text; J root;
    if (!readFile(settingsPath(), text)) return;
    if (!parseJson(text, root) || root.t != J::Obj) {
        // A damaged settings file is replaced on the next save: keep what was there so it can be recovered by hand.
        std::error_code ec;
        fs::path bad = settingsPath(); bad += ".bad";
        if (!fs::exists(bad, ec)) writeFile(bad, text);
        return;
    }
    if (J* d = root.get("ck3Dir"); d && d->t == J::Str) st.ck3Dir = d->s;
    if (J* a = root.get("active"); a && a->t == J::Str) st.active = a->s;
    if (J* g = root.get("gameExe"); g && g->t == J::Str) st.gameExe = g->s;
    if (J* th = root.get("theme"); th && th->t == J::Str && (th->s == "dark" || th->s == "light")) st.theme = th->s;
    if (J* cs = root.get("crashSeen"); cs && cs->t == J::Str && cs->s.size() < 100) st.crashSeen = cs->s;
    if (J* td = root.get("tutorialDone"); td && td->t == J::Bool) st.tutorialDone = td->b;
    if (J* lg = root.get("language"); lg && lg->t == J::Str && (lg->s == "auto" || langFromCode(lg->s) >= 0)) st.language = lg->s;
    if (J* lk = root.get("locks"); lk && lk->t == J::Obj)
        for (size_t i = 0; i < lk->keys.size(); i++)
            if (lk->vals[i].t == J::Arr) for (auto& e : lk->vals[i].a) if (e.t == J::Str) st.locks[lk->keys[i]].insert(e.s);
    if (J* ct = root.get("categories"); ct && ct->t == J::Obj)
        for (size_t i = 0; i < ct->keys.size(); i++)
            if (ct->vals[i].t == J::Num && ct->vals[i].n >= 0 && ct->vals[i].n < 7) st.cats[ct->keys[i]] = (int)ct->vals[i].n;
    loadSettingsExtra(st, root);
}
inline bool saveSettings(const Settings& st) {
    J root = J::obj();
    root.set("ck3Dir", J::str(st.ck3Dir));
    root.set("active", J::str(st.active));
    root.set("gameExe", J::str(st.gameExe));
    if (!st.theme.empty()) root.set("theme", J::str(st.theme));
    if (!st.crashSeen.empty()) root.set("crashSeen", J::str(st.crashSeen));
    if (st.tutorialDone) root.set("tutorialDone", J::boolean(true));
    if (st.language != "auto") root.set("language", J::str(st.language));
    J lk = J::obj();
    for (auto& kv : st.locks) {
        if (kv.second.empty()) continue;
        J a = J::arr();
        for (auto& id : kv.second) a.a.push_back(J::str(id));
        lk.set(kv.first, a);
    }
    if (!lk.keys.empty()) root.set("locks", lk);
    J ct = J::obj();
    for (auto& kv : st.cats) ct.set(kv.first, J::num(kv.second));
    if (!ct.keys.empty()) root.set("categories", ct);
    if (!st.hidden.empty()) { J a = J::arr(); for (auto& id : st.hidden) a.a.push_back(J::str(id)); root.set("hidden", a); }
    if (!st.launch.empty()) { J o = J::obj(); for (auto& kv : st.launch) o.set(kv.first, J::str(kv.second)); root.set("launch", o); }
    if (!st.seen.empty()) { J o = J::obj(); for (auto& kv : st.seen) o.set(kv.first, J::str(kv.second)); root.set("seen", o); }
    if (st.checkUpdates) root.set("checkUpdates", J::boolean(true));
    if (st.playTime > 0) {
        J lp = J::obj();
        lp.set("game", J::str(st.playGameVer)); lp.set("playset", J::str(st.playName)); lp.set("time", J::num((double)st.playTime));
        J a = J::arr(); for (auto& id : st.playMods) a.a.push_back(J::str(id)); lp.set("mods", a);
        root.set("lastPlay", lp);
    }
    if (st.winW > 0) {
        J w = J::obj();
        w.set("x", J::num(st.winX)); w.set("y", J::num(st.winY)); w.set("w", J::num(st.winW)); w.set("h", J::num(st.winH));
        w.set("max", J::boolean(st.winMax));
        J c = J::arr(); for (int v : st.colW) c.a.push_back(J::num(v)); w.set("cols", c);
        root.set("window", w);
    }
    std::string out; dump(root, out); out += "\n";
    return writeFileAtomic(settingsPath(), out);
}

// ---------- CK3 folder / mods ----------
inline std::string autoFindCK3Dir(const std::vector<std::string>& docRoots) {
    if (const char* e = getenv("RC_CK3_DIR"); e && *e) return e;
    for (auto& r : docRoots) {
        fs::path c = P(r) / "Paradox Interactive" / "Crusader Kings III";
        std::error_code ec;
        if (fs::is_directory(c, ec)) return c.u8string();
    }
    return "";
}

// ---------- .mod descriptor (Paradox script) ----------
// key="value"   key={ "a" "b" }   repeated keys (replace_path), comments (#), CRLF, BOM. Nested braces are flattened.
inline std::map<std::string, std::vector<std::string>> parseDescriptor(const std::string& text) {
    std::map<std::string, std::vector<std::string>> kv;
    struct Tok { char kind; std::string s; };  // 'w' word, '=' '{' '}'
    std::vector<Tok> t;
    size_t i = 0, n = text.size();
    if (n >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;
    while (i < n && t.size() < 200000) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
        if (c == '#') { while (i < n && text[i] != '\n') i++; continue; }
        if (c == '=' || c == '{' || c == '}') { t.push_back({c, ""}); i++; continue; }
        if (c == '"') {
            std::string v; i++;
            while (i < n && text[i] != '"' && text[i] != '\n') {
                if (text[i] == '\\' && i + 1 < n && (text[i + 1] == '"' || text[i + 1] == '\\')) { v += text[i + 1]; i += 2; }
                else v += text[i++];
            }
            if (i < n && text[i] == '"') i++;
            t.push_back({'w', v});
            continue;
        }
        std::string v;
        while (i < n && !strchr(" \t\r\n={}\"#", text[i])) v += text[i++];
        if (v.empty()) { i++; continue; }
        t.push_back({'w', v});
    }
    size_t p = 0;
    while (p < t.size()) {
        if (t[p].kind != 'w') { p++; continue; }
        std::string key = t[p].s;
        if (p + 1 >= t.size() || t[p + 1].kind != '=') { p++; continue; }
        p += 2;
        if (p >= t.size()) break;
        auto& vals = kv[key];
        if (t[p].kind == 'w') { vals.push_back(t[p].s); p++; }
        else if (t[p].kind == '{') {
            int depth = 1; p++;
            while (p < t.size() && depth > 0) {
                if (t[p].kind == '{') depth++;
                else if (t[p].kind == '}') depth--;
                else if (t[p].kind == 'w' && depth == 1) vals.push_back(t[p].s);
                p++;
            }
        }
    }
    return kv;
}

inline std::vector<ModInfo> scanMods(const std::string& dir) {
    std::vector<ModInfo> out;
    if (dir.empty()) return out;
    std::error_code ec;
    fs::path md = P(dir) / "mod";
    if (!fs::is_directory(md, ec)) return out;
    for (auto& e : fs::directory_iterator(md, ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string ext = lower(e.path().extension().u8string());
        if (ext != ".mod") continue;
        std::string text;
        if (!readFile(e.path(), text)) continue;
        auto kv = parseDescriptor(text);
        auto first = [&](const char* k) -> std::string { auto it = kv.find(k); return it == kv.end() || it->second.empty() ? std::string() : it->second[0]; };
        ModInfo mi;
        mi.id = e.path().filename().u8string();
        mi.source = mi.id.rfind("ugc_", 0) == 0 ? "Workshop" : "Local";
        mi.name = first("name"); mi.version = first("version"); mi.supported = first("supported_version");
        mi.path = first("path"); mi.archive = first("archive");
        if (kv.count("dependencies")) mi.deps = kv["dependencies"];
        if (kv.count("replace_path")) mi.replacePaths = kv["replace_path"];
        if (kv.count("tags")) mi.tags = kv["tags"];
        if (!mi.path.empty()) {
            fs::path cp = P(mi.path);
            if (!cp.is_absolute()) cp = P(dir) / cp;
            std::error_code e2;
            mi.contentDir = cp.u8string();
            mi.contentState = fs::is_directory(cp, e2) ? 1 : 2;
        }
        if (mi.name.empty()) mi.name = mi.id;
        out.push_back(std::move(mi));
    }
    std::sort(out.begin(), out.end(), [](const ModInfo& a, const ModInfo& b) { return lower(a.name) < lower(b.name); });
    return out;
}

// ---------- game version ----------
// "v1.14.*" / "1.14.1 (Scythe)" / "\"1.14\"" -> segments {"1","14","*"}; stops at the first character that is not a digit, dot or '*'.
inline std::vector<std::string> versionSegments(const std::string& s) {
    std::vector<std::string> seg;
    size_t i = 0;
    while (i < s.size() && !((s[i] >= '0' && s[i] <= '9') || s[i] == '*')) i++;
    std::string cur;
    for (; i < s.size(); i++) {
        char c = s[i];
        if ((c >= '0' && c <= '9') || c == '*') cur += c;
        else if (c == '.') { if (!cur.empty()) seg.push_back(cur); cur.clear(); }
        else break;
    }
    if (!cur.empty()) seg.push_back(cur);
    return seg;
}

// Installed game version from <game>/launcher/launcher-settings.json ("rawVersion", else "version").
// This file belongs to the game install, not to the Paradox Launcher's data.
inline std::string gameVersionFromSettings(const std::string& json) {
    J root;
    if (!parseJson(json, root) || root.t != J::Obj) return "";
    for (const char* key : {"rawVersion", "version"}) {
        J* v = root.get(key);
        if (v && v->t == J::Str) {
            auto seg = versionSegments(v->s);
            if (seg.empty() || seg[0] == "*") continue;
            std::string out;
            for (size_t i = 0; i < seg.size(); i++) { if (i) out += '.'; out += seg[i]; }
            return out;
        }
    }
    return "";
}

// exePath = ...\Crusader Kings III\binaries\ck3.exe  ->  ...\Crusader Kings III\launcher\launcher-settings.json
inline std::string readGameVersion(const std::string& exePath) {
    if (exePath.empty()) return "";
    fs::path root = P(exePath).parent_path().parent_path();
    std::string text;
    if (!readFile(root / "launcher" / "launcher-settings.json", text, 1u << 20)) return "";
    return gameVersionFromSettings(text);
}

enum class VerMatch { Unknown, Match, Mismatch };

// Does a mod's supported_version ("1.14.*", "1.14", "1.14.2") fit the installed game version? Compares major.minor only.
inline VerMatch matchGameVersion(const std::string& supported, const std::string& installed) {
    auto want = versionSegments(supported), have = versionSegments(installed);
    if (want.empty() || have.empty()) return VerMatch::Unknown;
    // Only major.minor (1.20) decides: patch releases (1.20.0.3 vs 1.20.0.4) keep working mods working.
    for (size_t i = 0; i < want.size() && i < 2; i++) {
        if (want[i] == "*") return VerMatch::Match;
        if (i >= have.size()) return VerMatch::Unknown;
        if (want[i].find('*') != std::string::npos) return VerMatch::Unknown;
        if (want[i].size() > 9 || have[i].size() > 9) return want[i] == have[i] ? VerMatch::Match : VerMatch::Mismatch;
        auto digits = [](const std::string& x) { return !x.empty() && x.find_first_not_of("0123456789") == std::string::npos; };
        if (!digits(want[i]) || !digits(have[i])) return VerMatch::Unknown;
        if (std::stol(want[i]) != std::stol(have[i])) return VerMatch::Mismatch;
    }
    return VerMatch::Match;
}

// ---------- saves ----------
struct SaveScan { std::vector<fs::path> entries; int files = 0; std::uintmax_t bytes = 0; fs::path folder; };

// Everything inside <CK3 user folder>\save games (the folder itself is kept). Symlinks are never followed.
inline SaveScan scanSaves(const std::string& ck3Dir) {
    SaveScan r;
    if (ck3Dir.empty()) return r;
    r.folder = P(ck3Dir) / "save games";
    std::error_code ec;
    if (!fs::is_directory(r.folder, ec)) return r;
    for (auto& e : fs::directory_iterator(r.folder, ec)) {
        r.entries.push_back(e.path());
        std::error_code e2;
        if (e.is_regular_file(e2)) { r.files++; auto sz = e.file_size(e2); if (!e2) r.bytes += sz; }
        else if (e.is_directory(e2) && !e.is_symlink(e2)) {
            for (fs::recursive_directory_iterator it(e.path(), e2), end; !e2 && it != end; it.increment(e2)) {
                std::error_code e3;
                if (it->is_regular_file(e3)) { r.files++; auto sz = it->file_size(e3); if (!e3) r.bytes += sz; }
            }
        }
    }
    return r;
}

// ---------- playset checks ----------
struct ModIssue { int sev = 0; std::string text; };  // sev: 0 info, 1 warning, 2 problem

inline std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
    return s.substr(a, b - a);
}

// Checks every mod of a playset against its descriptor. Result[i] belongs to ps.mods[i]. Disabled mods only get "not installed".
// Rules: files missing; dependency not enabled / not installed / loaded after the mod; same name enabled twice;
// made for another game version (major.minor); replace_path (info).
inline std::vector<std::vector<ModIssue>> analyzePlayset(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::string& gameVersion) {
    std::vector<std::vector<ModIssue>> out(ps.mods.size());
    std::map<std::string, int> enabledPos;                    // lower(name) -> first enabled position
    std::map<std::string, std::vector<int>> sameName;
    std::set<std::string> installedNames;
    for (auto& kv : info) installedNames.insert(lower(trimmed(kv.second.name)));
    for (size_t i = 0; i < ps.mods.size(); i++) {
        auto it = info.find(ps.mods[i].id);
        if (!ps.mods[i].enabled || it == info.end()) continue;
        std::string n = lower(trimmed(it->second.name));
        enabledPos.emplace(n, (int)i);
        sameName[n].push_back((int)i);
    }
    for (size_t i = 0; i < ps.mods.size(); i++) {
        auto& iss = out[i];
        auto it = info.find(ps.mods[i].id);
        if (it == info.end()) { iss.push_back({ps.mods[i].enabled ? 2 : 0, tr("Not installed")}); continue; }
        if (it->second.pending) iss.push_back({0, tr("Subscribed on Steam, not added yet. Tick the box to add it")});
        if (!ps.mods[i].enabled) continue;
        const ModInfo& m = it->second;
        std::string self = lower(trimmed(m.name));
        if (m.contentState == 2) iss.push_back({2, tr("Mod files not found")});
        for (auto& d : m.deps) {
            std::string dn = lower(trimmed(d));
            if (dn.empty() || dn == self) continue;
            auto ep = enabledPos.find(dn);
            if (ep == enabledPos.end())
                iss.push_back({2, installedNames.count(dn) ? trf("Needs \"{0}\" (installed, not enabled)", {d}) : trf("Needs \"{0}\" (not installed)", {d})});
            else if (ep->second > (int)i)
                iss.push_back({1, trf("\"{0}\" should load before this mod (it is #{1})", {d, ep->second + 1})});
        }
        auto sn = sameName.find(self);
        if (sn != sameName.end() && sn->second.size() > 1) {
            int other = sn->second[0] == (int)i ? sn->second[1] : sn->second[0];
            iss.push_back({1, trf("Same name as enabled mod #{0}", {other + 1})});
        }
        if (matchGameVersion(m.supported, gameVersion) == VerMatch::Mismatch)
            iss.push_back({1, trf("Made for game {0} (installed {1})", {m.supported, gameVersion})});
        if (!m.replacePaths.empty()) {
            std::string l;
            for (size_t k = 0; k < m.replacePaths.size(); k++) l += (k ? ", " : "") + m.replacePaths[k];
            iss.push_back({0, trf("Replaces vanilla folder(s): {0}", {l})});
        }
        std::stable_sort(iss.begin(), iss.end(), [](const ModIssue& a, const ModIssue& b) { return a.sev > b.sev; });
    }
    return out;
}

// One-line summary for the list: worst issue (problems and warnings only) plus how many more.
inline std::string issueSummary(const std::vector<ModIssue>& v) {
    int shown = 0, worst = -1;
    for (size_t i = 0; i < v.size(); i++) if (v[i].sev >= 1) { shown++; if (worst < 0) worst = (int)i; }
    if (worst < 0) return "";
    return shown > 1 ? trf("{0}  (+{1} more)", {v[(size_t)worst].text, shown - 1}) : v[(size_t)worst].text;
}
inline int issueSeverity(const std::vector<ModIssue>& v) { int s = -1; for (auto& i : v) if (i.sev > s) s = i.sev; return s < 1 ? 0 : s; }

// ---------- conflict finder ----------
// A mod's "files" are the game files it ships (paths relative to its folder, lower case, '/' separators).
// The game merges all enabled mods into one virtual folder: when two mods ship the same path, the one loaded LAST wins and the
// other file is ignored. replace_path in a mod's descriptor additionally drops everything in that folder that was loaded before it.
struct ModFiles {
    std::string fingerprint;                 // quick fingerprint (folder time + version), used to notice changes while the program runs
    std::vector<std::string> files;
    bool complete = false;
    std::string deep;                        // hash over every file's path, size and time: changes when ANY file in the mod changes
    std::vector<uint64_t> mix;               // deepMix of each file, in the same order as files (lets a later comparison tell which files changed)
};

// One file's contribution to a deep fingerprint. The contributions are added up, so the order the folder is listed in does not matter.
inline uint64_t deepMix(const std::string& rel, uint64_t size, int64_t mtime) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : rel) { h ^= c; h *= 1099511628211ULL; }
    auto put = [&](uint64_t v) { for (int i = 0; i < 8; i++) { h ^= (unsigned char)(v >> (i * 8)); h *= 1099511628211ULL; } };
    put(size); put((uint64_t)mtime);
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;   // spread the bits before adding
    return h;
}
inline std::string hex64(uint64_t v) {
    char b[17];
    std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
    return b;
}

inline bool skipFileName(const std::string& lowerName) {
    return lowerName == "thumbs.db" || lowerName == ".ds_store" || lowerName == ".gitignore" || lowerName == ".gitattributes" || lowerName == "desktop.ini";
}
inline bool skipDirName(const std::string& lowerName) {
    return lowerName == ".git" || lowerName == ".github" || lowerName == ".vscode" || lowerName == ".idea" || lowerName == ".svn" || lowerName == "__macosx";
}

// ---------- folder walking ----------
// Calls f(relative path, size, time) for every file below root: lower case, '/' separators, no .git-like folders, no Thumbs.db-like files,
// links are not followed. Two implementations give the same result: the portable one below, and a faster native one on Windows that the
// program installs in g_fastWalk (src/fastwalk.hpp). They are compared by a test.
enum WalkResult { WALK_OK = 0, WALK_CANCELLED = 1, WALK_FAILED = 2 };
using WalkCallback = std::function<void(const std::string& rel, uint64_t size, int64_t mtime)>;
inline int walkFilesPortable(const std::string& rootUtf8, const WalkCallback& f, const std::atomic<bool>* cancel) {
    fs::path root = P(rootUtf8);
    std::string rootStr = root.u8string();
    while (!rootStr.empty() && (rootStr.back() == '/' || rootStr.back() == '\\')) rootStr.pop_back();
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (cancel && cancel->load()) return WALK_CANCELLED;
        std::error_code e2;
        std::string name = lower(it->path().filename().u8string());
        if (it->is_directory(e2)) { if (skipDirName(name)) it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(e2) || skipFileName(name)) continue;
        std::string full = it->path().u8string();
        if (full.size() <= rootStr.size() + 1 || full.compare(0, rootStr.size(), rootStr) != 0) continue;
        std::string rel = full.substr(rootStr.size() + 1);
        for (auto& c : rel) { if (c == '\\') c = '/'; else if (c >= 'A' && c <= 'Z') c = (char)(c + 32); }
        std::error_code e3, e4;
        std::uintmax_t sz = it->file_size(e3);
        auto mt = it->last_write_time(e4);
        f(rel, e3 ? 0 : (uint64_t)sz, e4 ? 0 : (int64_t)mt.time_since_epoch().count());
    }
    return ec ? WALK_FAILED : WALK_OK;
}
inline std::function<int(const std::string&, const WalkCallback&, const std::atomic<bool>*)> g_fastWalk;   // optional native walker
inline int walkFiles(const std::string& rootUtf8, const WalkCallback& f, const std::atomic<bool>* cancel) {
    return g_fastWalk ? g_fastWalk(rootUtf8, f, cancel) : walkFilesPortable(rootUtf8, f, cancel);
}

// Lists a mod folder. Files directly in the root (descriptor.mod, thumbnail.png, readme) are not game files and are skipped.
// If cancelled, the result is marked incomplete.
inline ModFiles indexModFiles(const std::string& contentDir, const std::atomic<bool>* cancel = nullptr) {
    ModFiles r;
    if (contentDir.empty()) return r;
    uint64_t sum = 0;
    int rc = walkFiles(contentDir, [&](const std::string& rel, uint64_t size, int64_t mtime) {
        if (rel.find('/') == std::string::npos) return;   // root-level file
        uint64_t mx = deepMix(rel, size, mtime);
        sum += mx;
        r.files.push_back(rel);
        r.mix.push_back(mx);
    }, cancel);
    if (rc == WALK_CANCELLED) return r;
    r.complete = rc == WALK_OK;
    r.deep = hex64(sum) + "-" + std::to_string(r.files.size());
    return r;
}

// Changes when the mod's folder is replaced or re-created, or the mod's version string changes.
inline std::string modFingerprint(const ModInfo& m) {
    std::error_code ec;
    auto t = fs::last_write_time(P(m.contentDir), ec);
    return m.contentDir + "|" + m.version + "|" + (ec ? std::string("?") : std::to_string((long long)t.time_since_epoch().count()));
}

// ---------- the game's own files ----------
// A mod file with the same relative path as a file of the base game REPLACES that file (the game never merges them). That is
// what total conversions do on purpose, but a mod that replaces a vanilla file and was made for an older game version also
// undoes whatever the game update changed in that file. So the game's file list is read once and kept (cache/vanilla.txt).
struct VanillaIndex {
    std::string key;                         // which install and version the list was made for
    std::string dir;                         // the game folder that was read
    std::vector<std::string> files;          // sorted, lower case, '/' separators, relative to the game folder (DLC folders folded in)
    size_t dlcFiles = 0;
    VanillaIndex() = default;
    VanillaIndex(const VanillaIndex&) = delete;              // set_ holds views into files
    VanillaIndex& operator=(const VanillaIndex&) = delete;
    void finish() {
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
        set_.clear();
        set_.reserve(files.size() * 2);
        for (auto& f : files) set_.insert(std::string_view(f));
    }
    bool has(const std::string& rel) const { return set_.count(std::string_view(rel)) != 0; }
    // Files below a folder ("common/traits/"; the prefix ends with '/').
    size_t countUnder(const std::string& prefix) const {
        auto it = std::lower_bound(files.begin(), files.end(), prefix);
        size_t n = 0;
        while (it != files.end() && it->compare(0, prefix.size(), prefix) == 0) { n++; ++it; }
        return n;
    }
private:
    std::unordered_set<std::string_view> set_;
};

// ...\Crusader Kings III\binaries\ck3.exe  ->  ...\Crusader Kings III\game   ("" if it is not there)
inline std::string vanillaGameDir(const std::string& gameExe) {
    if (gameExe.empty()) return "";
    fs::path d = P(gameExe).parent_path().parent_path() / "game";
    std::error_code ec;
    return fs::is_directory(d, ec) ? d.u8string() : std::string();
}

// Where DLC content may live: next to the base files ("game/dlc/<name>/...") or beside the game folder ("<install>/dlc/<name>/...").
// A DLC folder is laid over the base game, so its files count as vanilla files under their path inside the DLC folder.
inline std::vector<std::string> vanillaDlcDirs(const std::string& gameDir) {
    std::vector<std::string> v;
    fs::path g = P(gameDir);
    std::error_code ec;
    for (fs::path d : {g / "dlc", g.parent_path() / "dlc"}) if (fs::is_directory(d, ec)) v.push_back(d.u8string());
    return v;
}

// Identifies the install and version: the game folder, its version and the times of its top-level folders (they change when the game is updated).
inline std::string vanillaKey(const std::string& gameDir, const std::string& gameVer) {
    uint64_t sum = 0;
    size_t n = 0;
    auto add = [&](const fs::path& d) {
        std::error_code ec;
        for (fs::directory_iterator it(d, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code e2, e3;
            if (!it->is_directory(e2)) continue;
            auto t = it->last_write_time(e3);
            sum += deepMix(lower(it->path().filename().u8string()), 0, e3 ? 0 : (int64_t)t.time_since_epoch().count());
            n++;
        }
    };
    add(P(gameDir));
    for (auto& d : vanillaDlcDirs(gameDir)) add(P(d));
    return gameDir + "|" + gameVer + "|" + hex64(sum) + "-" + std::to_string(n);
}

inline std::shared_ptr<VanillaIndex> indexVanilla(const std::string& gameDir, const std::atomic<bool>* cancel = nullptr) {
    auto idx = std::make_shared<VanillaIndex>();
    idx->dir = gameDir;
    auto walk = [&](const std::string& rootIn, bool isDlc) -> bool {   // isDlc: the folder is one DLC's own folder, its files count as base-game paths
        int rc = walkFiles(rootIn, [&](const std::string& rel, uint64_t, int64_t) {
            if (rel.find('/') == std::string::npos || rel.find('\n') != std::string::npos) return;   // root-level files are not game content
            if (isDlc) idx->dlcFiles++;
            idx->files.push_back(rel);
        }, cancel);
        return rc != WALK_CANCELLED;
    };
    if (!walk(gameDir, false)) return nullptr;
    // Each DLC folder (inside game/ or beside it) is laid over the base game, so its files are added under their path inside the DLC folder.
    std::vector<std::string> dlcRoots = vanillaDlcDirs(gameDir);
    for (auto& d : dlcRoots) {
        std::error_code ec;
        for (fs::directory_iterator it(P(d), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            if (cancel && cancel->load()) return nullptr;
            std::error_code e2;
            if (!it->is_directory(e2)) continue;
            if (!walk(it->path().u8string(), true)) return nullptr;
        }
    }
    idx->finish();
    return idx;
}

inline fs::path vanillaCacheFile(const std::string& cacheDir) { return P(cacheDir) / "vanilla.txt"; }
inline bool saveVanillaCache(const std::string& cacheDir, const VanillaIndex& v) {
    if (cacheDir.empty()) return false;
    std::error_code ec;
    fs::create_directories(P(cacheDir), ec);
    std::string out = "RCVAN1\n" + v.key + "\n" + v.dir + "\n" + std::to_string(v.dlcFiles) + "\n" + std::to_string(v.files.size()) + "\n";
    size_t total = out.size();
    for (auto& f : v.files) total += f.size() + 1;
    out.reserve(total);
    for (auto& f : v.files) { out += f; out += '\n'; }
    return writeFileAtomic(vanillaCacheFile(cacheDir), out);
}
// The saved list, if it was made for exactly this install and version ("key"); anything damaged or different gives nullptr.
inline std::shared_ptr<VanillaIndex> loadVanillaCache(const std::string& cacheDir, const std::string& key) {
    if (cacheDir.empty()) return nullptr;
    std::string t;
    if (!readFile(vanillaCacheFile(cacheDir), t, 256u << 20)) return nullptr;
    size_t pos = 0;
    auto line = [&](std::string& out) { size_t e = t.find('\n', pos); if (e == std::string::npos) return false; out = t.substr(pos, e - pos); pos = e + 1; return true; };
    std::string magic, k, dir, dlc, cnt;
    if (!line(magic) || magic != "RCVAN1" || !line(k) || k != key || !line(dir) || !line(dlc) || !line(cnt)) return nullptr;
    char* e1 = nullptr; char* e2 = nullptr;
    unsigned long long d = std::strtoull(dlc.c_str(), &e1, 10), n = std::strtoull(cnt.c_str(), &e2, 10);
    if (dlc.empty() || cnt.empty() || *e1 || *e2 || n > 5000000ULL) return nullptr;
    auto v = std::make_shared<VanillaIndex>();
    v->key = k; v->dir = dir; v->dlcFiles = (size_t)d;
    v->files.reserve((size_t)n);
    std::string f;
    while (v->files.size() < n && line(f)) { if (f.empty() || f.find('/') == std::string::npos) return nullptr; v->files.push_back(std::move(f)); }
    if (v->files.size() != n || pos != t.size()) return nullptr;     // cut off or extra text
    v->finish();
    if (v->files.size() != n) return nullptr;                        // duplicate lines: not something we wrote
    return v;
}
// The game's file list: from the saved copy when the install and version are unchanged, otherwise read from disk and saved.
inline std::shared_ptr<VanillaIndex> loadOrIndexVanilla(const std::string& cacheDir, const std::string& gameExe, const std::string& gameVer, const std::atomic<bool>* cancel = nullptr, bool* fromCache = nullptr) {
    if (fromCache) *fromCache = false;
    std::string dir = vanillaGameDir(gameExe);
    if (dir.empty()) return nullptr;
    std::string key = vanillaKey(dir, gameVer);
    if (auto c = loadVanillaCache(cacheDir, key)) { if (fromCache) *fromCache = true; return c; }
    auto v = indexVanilla(dir, cancel);
    if (!v || v->files.empty()) return nullptr;
    v->key = key;
    saveVanillaCache(cacheDir, *v);
    return v;
}

// How much a replaced file matters. A whole-file replacement drops everything the losing mod put in that file, so game rules and
// events (common/, events/, history/, map_data/) are the serious ones; interface and text are in between; art and sound just look different.
enum Sev : int { SEV_LOW = 0, SEV_MED = 1, SEV_HIGH = 2 };
inline std::string sevName(int s) { return s >= SEV_HIGH ? tr("High") : s == SEV_MED ? tr("Medium") : tr("Low"); }
inline int areaSeverity(const std::string& area) {
    auto starts = [&](const char* p) { return area.rfind(p, 0) == 0; };
    if (starts("common/genes") || starts("common/ethnicities") || starts("common/bookmark_portraits") || starts("common/coat_of_arms") || starts("common/named_colors") ||
        starts("common/accessories") || starts("common/portrait") || starts("common/ai_war_stances")) return starts("common/ai_war_stances") ? SEV_HIGH : SEV_LOW;
    if (starts("common") || area == "events" || starts("history") || starts("map_data")) return SEV_HIGH;
    if (starts("gfx") || starts("music") || starts("sound") || starts("sfx") || starts("fonts") || starts("licenses") || starts("dlc_metadata")) return SEV_LOW;
    return SEV_MED;     // gui, localization, anything else
}

struct FileConflict { std::string path; std::vector<int> mods; int sev = 0; bool vanilla = false; };   // vanilla: the path is also a file of the base game
struct PairConflict { int a = 0, b = 0, count = 0, sev = 0; std::vector<std::pair<std::string, int>> areas; };  // a loads before b; b wins
struct WipeHit { int replacer = 0, victim = 0, files = 0; std::string folder; };
struct VanillaFile { std::string path; std::vector<int> mods; int sev = 0; };       // enabled mods that ship this base-game file, in load order; the last one's copy is used
struct VanillaWipe { int mod = 0, files = 0; std::string folder; };                 // replace_path that removes this many base-game files
struct ConflictReport {
    std::vector<FileConflict> files;
    std::vector<PairConflict> pairs;
    std::vector<WipeHit> wipes;
    std::vector<VanillaFile> vanilla;
    std::vector<VanillaWipe> vanillaWipes;
    std::vector<int> loses, wins;     // per playset position: files this mod loses / wins against another mod
    std::vector<int> vanillaCount;    // per playset position: base-game files this mod replaces
    int modsIndexed = 0, filesIndexed = 0, modsWithoutFiles = 0;
    bool vanillaChecked = false;      // false: the game's file list was not available
    bool valid = false;
};

// "common/traits/x.txt" -> "common/traits", "events/x.txt" -> "events"
inline std::string areaOf(const std::string& path) {
    size_t a = path.find('/');
    if (a == std::string::npos) return path;
    size_t b = path.find('/', a + 1);
    return b == std::string::npos ? path.substr(0, a) : path.substr(0, b);
}

inline ConflictReport findConflicts(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::map<std::string, ModFiles>& index, const VanillaIndex* van = nullptr, const std::atomic<bool>* cancel = nullptr) {
    ConflictReport r;
    r.loses.assign(ps.mods.size(), 0);
    r.wins.assign(ps.mods.size(), 0);
    r.vanillaCount.assign(ps.mods.size(), 0);
    r.vanillaChecked = van != nullptr;
    std::unordered_map<std::string, std::vector<int>> byPath;
    std::vector<const ModFiles*> mf(ps.mods.size(), nullptr);
    size_t total = 0;
    for (size_t i = 0; i < ps.mods.size(); i++) {
        if (!ps.mods[i].enabled || !info.count(ps.mods[i].id)) continue;
        auto it = index.find(ps.mods[i].id);
        if (it == index.end() || !it->second.complete) { r.modsWithoutFiles++; continue; }
        mf[i] = &it->second;
        total += it->second.files.size();
        r.modsIndexed++;
    }
    byPath.reserve(total);
    for (size_t i = 0; i < ps.mods.size(); i++) {
        if (!mf[i]) continue;
        for (auto& f : mf[i]->files) {
            auto& v = byPath[f];
            if (v.empty() || v.back() != (int)i) v.push_back((int)i);
        }
        r.filesIndexed += (int)mf[i]->files.size();
    }
    struct PairAcc { int count = 0, sev = 0; std::map<std::string, int> areas; };
    std::map<std::pair<int, int>, PairAcc> pairs;
    size_t tick = 0;
    for (auto& kv : byPath) {
        if (cancel && (++tick & 0x3fff) == 0 && cancel->load()) return ConflictReport();
        const auto& v = kv.second;
        bool isVan = van && van->has(kv.first);
        if (!isVan && v.size() < 2) continue;
        std::string area = areaOf(kv.first);
        int sev = areaSeverity(area);
        if (isVan) {
            r.vanilla.push_back({kv.first, v, sev});
            for (int m : v) r.vanillaCount[(size_t)m]++;
        }
        if (v.size() < 2) continue;
        r.files.push_back({kv.first, v, sev, isVan});
        r.wins[(size_t)v.back()]++;
        for (size_t k = 0; k + 1 < v.size(); k++) r.loses[(size_t)v[k]]++;
        auto add = [&](int a, int b) { auto& p = pairs[{a, b}]; p.count++; p.areas[area]++; if (sev > p.sev) p.sev = sev; };
        if (v.size() <= 12) { for (size_t x = 0; x < v.size(); x++) for (size_t y = x + 1; y < v.size(); y++) add(v[x], v[y]); }
        else for (size_t x = 0; x + 1 < v.size(); x++) add(v[x], v.back());
    }
    std::sort(r.files.begin(), r.files.end(), [](const FileConflict& a, const FileConflict& b) { return a.sev != b.sev ? a.sev > b.sev : a.path < b.path; });
    std::sort(r.vanilla.begin(), r.vanilla.end(), [](const VanillaFile& a, const VanillaFile& b) { return a.sev != b.sev ? a.sev > b.sev : a.path < b.path; });
    for (auto& kv : pairs) {
        PairConflict pc;
        pc.a = kv.first.first; pc.b = kv.first.second; pc.count = kv.second.count; pc.sev = kv.second.sev;
        for (auto& ar : kv.second.areas) pc.areas.push_back(ar);
        std::sort(pc.areas.begin(), pc.areas.end(), [](const std::pair<std::string, int>& x, const std::pair<std::string, int>& y) { return x.second != y.second ? x.second > y.second : x.first < y.first; });
        r.pairs.push_back(std::move(pc));
    }
    std::sort(r.pairs.begin(), r.pairs.end(), [](const PairConflict& x, const PairConflict& y) { return x.sev != y.sev ? x.sev > y.sev : x.count != y.count ? x.count > y.count : (x.a != y.a ? x.a < y.a : x.b < y.b); });
    // replace_path: everything under that folder that was loaded BEFORE the replacing mod is dropped
    for (size_t rp = 0; rp < ps.mods.size(); rp++) {
        auto it = info.find(ps.mods[rp].id);
        if (!mf[rp] || it == info.end()) continue;
        for (auto folder : it->second.replacePaths) {
            for (auto& c : folder) { if (c == '\\') c = '/'; else if (c >= 'A' && c <= 'Z') c = (char)(c + 32); }
            while (!folder.empty() && folder.back() == '/') folder.pop_back();
            while (!folder.empty() && folder.front() == '/') folder.erase(0, 1);
            if (folder.empty()) continue;
            std::string prefix = folder + "/";
            for (size_t v = 0; v < rp; v++) {
                if (!mf[v]) continue;
                int n = 0;
                for (auto& f : mf[v]->files) if (f.compare(0, prefix.size(), prefix) == 0) n++;
                if (n > 0) r.wipes.push_back({(int)rp, (int)v, n, folder});
            }
        }
    }
    if (van) {   // replace_path also removes the base game's own files in that folder
        for (size_t rp = 0; rp < ps.mods.size(); rp++) {
            auto it = info.find(ps.mods[rp].id);
            if (!ps.mods[rp].enabled || it == info.end()) continue;
            for (auto folder : it->second.replacePaths) {
                for (auto& c : folder) { if (c == '\\') c = '/'; else if (c >= 'A' && c <= 'Z') c = (char)(c + 32); }
                while (!folder.empty() && folder.back() == '/') folder.pop_back();
                while (!folder.empty() && folder.front() == '/') folder.erase(0, 1);
                if (folder.empty()) continue;
                size_t n = van->countUnder(folder + "/");
                if (n > 0) r.vanillaWipes.push_back({(int)rp, (int)n, folder});
            }
        }
    }
    r.valid = true;
    return r;
}

// Adds the conflict results to the per-mod checks from analyzePlayset (same indexing).
inline void addConflictIssues(std::vector<std::vector<ModIssue>>& issues, const ConflictReport& cr, const Playset& ps, const std::map<std::string, ModInfo>& info, const std::string& gameVer = "") {
    if (!cr.valid || issues.size() != ps.mods.size()) return;
    auto nameOf = [&](int i) { auto it = info.find(ps.mods[(size_t)i].id); return it == info.end() ? ps.mods[(size_t)i].id : it->second.name; };
    for (auto& w : cr.wipes) {
        issues[(size_t)w.replacer].push_back({1, trf("replace_path \"{0}\" removes {1} file(s) of \"{2}\" (loaded earlier)", {w.folder, w.files, nameOf(w.victim)})});
        issues[(size_t)w.victim].push_back({1, trf("{0} file(s) in \"{1}\" removed by \"{2}\" (replace_path)", {w.files, w.folder, nameOf(w.replacer)})});
    }
    if (cr.vanillaChecked && cr.vanillaCount.size() == ps.mods.size()) {
        for (auto& w : cr.vanillaWipes)
            issues[(size_t)w.mod].push_back({1, trf("replace_path \"{0}\" removes {1} file(s) of the base game", {w.folder, w.files})});
        for (size_t i = 0; i < ps.mods.size(); i++) {
            if (cr.vanillaCount[i] <= 0) continue;
            auto it = info.find(ps.mods[i].id);
            bool old = it != info.end() && matchGameVersion(it->second.supported, gameVer) == VerMatch::Mismatch;
            std::string t = old ? trf("Replaces {0} file(s) of the base game and was made for an older game version: changes the update made to those files are lost", {cr.vanillaCount[i]})
                                : trf("Replaces {0} file(s) of the base game", {cr.vanillaCount[i]});
            issues[i].push_back({old ? 1 : 0, t});
        }
    }
    for (size_t i = 0; i < ps.mods.size(); i++) {
        if (cr.wins[i] > 0) issues[i].push_back({0, trf("Overrides {0} file(s) from earlier mods", {cr.wins[i]})});
        if (cr.loses[i] > 0) issues[i].push_back({0, trf("{0} of its files are overridden by later mods", {cr.loses[i]})});
        std::stable_sort(issues[i].begin(), issues[i].end(), [](const ModIssue& a, const ModIssue& b) { return a.sev > b.sev; });
    }
}


// ---------- script-level conflicts ----------
// Two mods can clash without sharing a file: both define the trait "brave" in different files, both use event id
// "my_events.1", both set the same localization key. This reads the definitions of every mod once (cached) and compares them.
enum DefKind : uint8_t { DK_COMMON = 0, DK_EVENT = 1, DK_LOC = 2, DK_DEFINE = 3, DK_ONACTION = 4, DK_KINDS = 5 };
enum DefFlag : uint8_t { DF_REPLACE = 1, DF_PRIORITY = 2, DF_OVERWRITES = 4 };   // replace-folder loc / event with id_override_priority / on_action that sets effect or trigger
struct DefEntry { uint64_t h = 0; uint32_t file = 0, nameOff = 0; uint16_t nameLen = 0; uint8_t kind = 0, flags = 0; };
struct ModDefs {
    std::string fingerprint;
    bool complete = false;
    std::vector<std::string> files;        // relative paths of the files that were read
    std::vector<DefEntry> defs;
    std::string pool;                      // key texts, back to back
    size_t bytesRead = 0;
    std::string nameOf(const DefEntry& d) const { return pool.substr(d.nameOff, d.nameLen); }
};

inline uint64_t fnv64(const std::string& a, char sep, const std::string& b) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : a) { h ^= c; h *= 1099511628211ULL; }
    h ^= (unsigned char)sep; h *= 1099511628211ULL;
    for (unsigned char c : b) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

struct ScriptKey { std::string key; uint8_t flags = 0; };
// Reads the top-level definitions of a Paradox script file.
//   mode 0: "name = { ... }" objects only; mode 1: objects and "name = value"; mode 2 (defines): "Block = { name = value / { } }" -> "Block.name"
inline void scanScriptKeys(const std::string& t, int mode, std::vector<ScriptKey>& out) {
    size_t i = 0, n = t.size();
    if (n >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) i = 3;
    int depth = 0;
    std::string cand, block;
    int candDepth = -1;
    bool hasCand = false, afterOp = false;
    long cur = -1;                                      // index into out of the object we are inside (modes 0/1)
    auto isOp = [](char c) { return c == '=' || c == '<' || c == '>' || c == '!' || c == '?'; };
    auto emit = [&](const std::string& k, bool object) {
        if (k.empty() || k[0] == '@') { cur = -1; return; }
        if (mode == 0 && !object) return;
        ScriptKey sk; sk.key = k;
        out.push_back(std::move(sk));
        cur = object ? (long)out.size() - 1 : -1;
    };
    while (i < n) {
        char c = t[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
        if (c == '#') { while (i < n && t[i] != '\n') i++; continue; }
        if (c == '{') {
            if (hasCand && afterOp) {
                if (mode == 2) { if (candDepth == 0) block = cand; else if (candDepth == 1) emit(block + "." + cand, true); }
                else if (candDepth == 0) emit(cand, true);
            }
            depth++; hasCand = false; afterOp = false; i++; continue;
        }
        if (c == '}') { if (depth > 0) depth--; if (depth == 0) cur = -1; hasCand = false; afterOp = false; i++; continue; }
        if (isOp(c)) {
            while (i < n && isOp(t[i])) i++;
            if (hasCand) {
                afterOp = true;
                if (mode != 2 && candDepth == 1 && cur >= 0) {
                    if (cand == "effect" || cand == "trigger") out[(size_t)cur].flags |= DF_OVERWRITES;
                    else if (cand == "id_override_priority") out[(size_t)cur].flags |= DF_PRIORITY;
                }
            }
            continue;
        }
        std::string tok;
        if (c == '"') {
            i++;
            while (i < n && t[i] != '"') { if (t[i] == '\\' && i + 1 < n) i++; tok += t[i]; i++; }
            if (i < n) i++;
        } else {
            size_t s = i;
            while (i < n) { char d = t[i]; if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == '{' || d == '}' || d == '"' || d == '#' || isOp(d)) break; i++; }
            tok = t.substr(s, i - s);
        }
        if (hasCand && afterOp) {                       // a plain value: "name = value"
            if (mode == 2) { if (candDepth == 1) emit(block + "." + cand, false); }
            else if (mode == 1 && candDepth == 0) emit(cand, false);
            hasCand = false; afterOp = false;
            continue;
        }
        if ((mode == 2 && depth <= 1) || (mode != 2 && depth == 0) || (mode != 2 && depth == 1)) { cand = tok; candDepth = depth; hasCand = true; afterOp = false; }
        else hasCand = false;
    }
}

// "localization/english/foo_l_english.yml" lines: key:0 "text"
inline void scanLocKeys(const std::string& t, std::vector<std::string>& out) {
    size_t i = 0, n = t.size();
    if (n >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) i = 3;
    while (i < n) {
        size_t e = t.find('\n', i);
        if (e == std::string::npos) e = n;
        size_t s = i;
        while (s < e && (t[s] == ' ' || t[s] == '\t')) s++;
        if (s < e && t[s] != '#') {
            size_t colon = t.find(':', s);
            if (colon != std::string::npos && colon < e && colon > s) {
                size_t q = colon + 1;
                while (q < e && ((t[q] >= '0' && t[q] <= '9') || t[q] == ' ' || t[q] == '\t')) q++;
                bool key = true;
                for (size_t k = s; k < colon; k++) if (t[k] == ' ' || t[k] == '\t' || t[k] == '"') { key = false; break; }
                if (key && q < e && t[q] == '"') out.push_back(t.substr(s, colon - s));
            }
        }
        i = e + 1;
    }
}

// What kind of definitions a mod file holds: kind (-1 = none), the "area" used to compare, the scan mode and flags.
struct DefFileKind { int kind = -1; std::string area; int mode = 0; uint8_t flags = 0; };
inline DefFileKind defFileKind(const std::string& rel) {
    DefFileKind k;
    auto ends = [&](const char* e) { size_t l = std::strlen(e); return rel.size() > l && rel.compare(rel.size() - l, l, e) == 0; };
    if (ends(".txt")) {
        if (rel.rfind("events/", 0) == 0) { k.kind = DK_EVENT; k.area = "events"; return k; }
        if (rel.rfind("common/", 0) == 0) {
            size_t b = rel.find('/', 7);
            if (b == std::string::npos) return k;
            std::string sub = rel.substr(7, b - 7);
            k.area = "common/" + sub;
            if (sub == "defines") { k.kind = DK_DEFINE; k.mode = 2; }
            else if (sub == "on_action" || sub == "on_actions") k.kind = DK_ONACTION;
            else { k.kind = DK_COMMON; k.mode = sub == "script_values" ? 1 : 0; }
        }
    } else if (ends(".yml") && rel.rfind("localization/", 0) == 0) {
        std::vector<std::string> parts;
        size_t p = 0;
        while (p < rel.size()) { size_t s = rel.find('/', p); if (s == std::string::npos) s = rel.size(); parts.push_back(rel.substr(p, s - p)); p = s + 1; }
        if (parts.size() < 3) return k;                  // localization/<file> without a language folder
        std::string lang;
        if (parts[1] == "replace") { lang = parts[2]; k.flags |= DF_REPLACE; }
        else { lang = parts[1]; if (parts[2] == "replace") k.flags |= DF_REPLACE; }
        k.kind = DK_LOC; k.area = "localization/" + lang;
    }
    return k;
}

inline ModDefs indexModDefs(const std::string& contentDir, const std::vector<std::string>& files, const std::atomic<bool>* cancel = nullptr) {
    ModDefs md;
    std::vector<ScriptKey> keys;
    std::vector<std::string> locKeys;
    for (const std::string& rel : files) {
        if (cancel && cancel->load()) return md;
        DefFileKind fk = defFileKind(rel);
        if (fk.kind < 0) continue;
        std::string text;
        if (!readFile(P(contentDir) / P(rel), text, 96u << 20)) continue;
        md.bytesRead += text.size();
        uint32_t fi = (uint32_t)md.files.size();
        md.files.push_back(rel);
        auto add = [&](const std::string& key, uint8_t flags) {
            if (key.empty() || key.size() > 65000) return;
            DefEntry d;
            d.h = fnv64(fk.area, '\x1f', key);
            d.file = fi; d.nameOff = (uint32_t)md.pool.size(); d.nameLen = (uint16_t)key.size();
            d.kind = (uint8_t)fk.kind; d.flags = flags;
            md.pool += key;
            md.defs.push_back(d);
        };
        if (fk.kind == DK_LOC) {
            locKeys.clear();
            scanLocKeys(text, locKeys);
            for (auto& k : locKeys) add(k, fk.flags);
        } else {
            keys.clear();
            scanScriptKeys(text, fk.mode, keys);
            for (auto& k : keys) {
                if (fk.kind == DK_EVENT && k.key.find('.') == std::string::npos) continue;   // events are "namespace.number"
                add(k.key, k.flags);
            }
        }
    }
    md.complete = true;
    return md;
}

// ---------- saved file index ----------
// Reading and parsing every script file of every mod is the slow part of a scan, and it only has to be done again for a mod whose
// files changed. What is parsed (the definitions) is saved per mod in <data>/cache/m_<hash>.bin together with the mod's deep
// fingerprint; the folder is still listed on every start (that is cheap), and the saved copy is used when the fingerprint matches.
// The cache is only a speed-up: deleting it, or any damage to it, just means the mod is read again.
static const uint32_t INDEX_CACHE_FORMAT = 1;
static const uint32_t DEFS_PARSER_VERSION = 1;      // raise when scanScriptKeys / scanLocKeys / defFileKind change what they find

inline fs::path modCacheFile(const std::string& cacheDir, const std::string& id) {
    return P(cacheDir) / ("m_" + hex64(fnv64(id, '\x1e', "RCIX")) + ".bin");
}

struct CacheOut {
    std::string b;
    void u8(uint8_t v) { b += (char)v; }
    void u16(uint16_t v) { for (int i = 0; i < 2; i++) b += (char)(v >> (i * 8)); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b += (char)(v >> (i * 8)); }
    void u64(uint64_t v) { for (int i = 0; i < 8; i++) b += (char)(v >> (i * 8)); }
    void str(const std::string& s) { u32((uint32_t)s.size()); b += s; }
};
struct CacheIn {
    const std::string& b; size_t p = 0; bool ok = true;
    explicit CacheIn(const std::string& s) : b(s) {}
    bool need(size_t n) { if (!ok || b.size() - p < n) ok = false; return ok; }
    uint8_t u8() { return need(1) ? (uint8_t)b[p++] : 0; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = 0; for (int i = 0; i < 2; i++) v |= (uint16_t)((uint8_t)b[p++]) << (i * 8); return v; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)((uint8_t)b[p++]) << (i * 8); return v; }
    uint64_t u64() { if (!need(8)) return 0; uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)((uint8_t)b[p++]) << (i * 8); return v; }
    std::string str() { uint32_t n = u32(); if (!need(n)) return ""; std::string s = b.substr(p, n); p += n; return s; }
};

inline bool saveModCache(const std::string& cacheDir, const std::string& id, const std::string& deep, const ModDefs& md) {
    if (cacheDir.empty() || deep.empty() || !md.complete) return false;
    std::error_code ec;
    fs::create_directories(P(cacheDir), ec);
    CacheOut o;
    o.b += "RCIX";
    o.u32(INDEX_CACHE_FORMAT); o.u32(DEFS_PARSER_VERSION);
    o.str(id); o.str(deep);
    o.u64(md.bytesRead);
    o.u32((uint32_t)md.files.size());
    for (auto& f : md.files) o.str(f);
    o.u32((uint32_t)md.defs.size());
    for (auto& d : md.defs) { o.u64(d.h); o.u32(d.file); o.u32(d.nameOff); o.u16(d.nameLen); o.u8(d.kind); o.u8(d.flags); }
    o.str(md.pool);
    uint64_t sum = fnv64(o.b, '\x1d', "RCIX");
    o.u64(sum);
    return writeFileAtomic(modCacheFile(cacheDir, id), o.b);
}

// The saved definitions of a mod, if the file is intact, belongs to this mod id and was made for the same deep fingerprint.
inline bool loadModCache(const std::string& cacheDir, const std::string& id, const std::string& deep, ModDefs& md) {
    if (cacheDir.empty() || deep.empty()) return false;
    std::string t;
    if (!readFile(modCacheFile(cacheDir, id), t, 512u << 20) || t.size() < 4 + 4 + 4 + 8) return false;
    std::string body = t.substr(0, t.size() - 8);
    CacheIn tail(t); tail.p = t.size() - 8;
    if (tail.u64() != fnv64(body, '\x1d', "RCIX")) return false;           // cut off or damaged
    CacheIn in(body);
    if (body.compare(0, 4, "RCIX") != 0) return false;
    in.p = 4;
    if (in.u32() != INDEX_CACHE_FORMAT || in.u32() != DEFS_PARSER_VERSION) return false;
    if (in.str() != id || in.str() != deep) return false;
    ModDefs r;
    r.bytesRead = (size_t)in.u64();
    uint32_t nf = in.u32();
    if (!in.ok || nf > (body.size() - in.p) / 4) return false;
    for (uint32_t i = 0; i < nf; i++) r.files.push_back(in.str());
    uint32_t nd = in.u32();
    if (!in.ok || nd > (body.size() - in.p) / 20) return false;
    r.defs.reserve(nd);
    for (uint32_t i = 0; i < nd; i++) {
        DefEntry d;
        d.h = in.u64(); d.file = in.u32(); d.nameOff = in.u32(); d.nameLen = in.u16(); d.kind = in.u8(); d.flags = in.u8();
        r.defs.push_back(d);
    }
    r.pool = in.str();
    if (!in.ok || in.p != body.size()) return false;
    for (auto& d : r.defs)                                                  // every reference must point inside what was loaded
        if (d.file >= r.files.size() || d.kind >= DK_KINDS || (uint64_t)d.nameOff + d.nameLen > r.pool.size()) return false;
    r.complete = true;
    md = std::move(r);
    return true;
}

// Removes saved indexes of mods that are no longer installed. Only files that look like ours are touched.
inline int pruneModCache(const std::string& cacheDir, const std::set<std::string>& keepIds) {
    if (cacheDir.empty()) return 0;
    std::set<std::string> keep;
    for (auto& id : keepIds) keep.insert(modCacheFile(cacheDir, id).filename().u8string());
    int removed = 0;
    std::error_code ec;
    std::vector<fs::path> kill;
    for (fs::directory_iterator it(P(cacheDir), ec), end; !ec && it != end; it.increment(ec)) {
        std::string n = it->path().filename().u8string();
        bool ours = n.size() == 2 + 16 + 4 && n.compare(0, 2, "m_") == 0 && n.compare(n.size() - 4, 4, ".bin") == 0;
        for (size_t i = 2; ours && i < 18; i++) ours = std::isxdigit((unsigned char)n[i]) != 0;
        if (ours && !keep.count(n)) kill.push_back(it->path());
    }
    for (auto& k : kill) { std::error_code e2; if (fs::remove(k, e2)) removed++; }
    return removed;
}

struct IndexedMod { ModFiles files; ModDefs defs; bool fromCache = false; };
// Lists a mod's folder and gets its definitions, from the saved copy when nothing in the mod changed.
inline IndexedMod indexModCached(const std::string& cacheDir, const std::string& id, const std::string& contentDir, const std::atomic<bool>* cancel = nullptr) {
    IndexedMod r;
    r.files = indexModFiles(contentDir, cancel);
    if (!r.files.complete) return r;
    if (loadModCache(cacheDir, id, r.files.deep, r.defs)) { r.fromCache = true; return r; }
    r.defs = indexModDefs(contentDir, r.files.files, cancel);
    if (r.defs.complete) saveModCache(cacheDir, id, r.files.deep, r.defs);
    return r;
}

struct ScriptHit { int mod = 0; std::string file; uint8_t flags = 0; };
struct ScriptConflict {
    int kind = 0, sev = 0, winner = -1;              // winner: playset position, -1 = unclear / not applicable
    std::string area, key, note;
    std::vector<ScriptHit> hits;                     // in load order
};
struct ScriptReport {
    std::vector<ScriptConflict> items;
    std::vector<std::array<int, DK_KINDS>> perMod;   // per playset position: conflicts it takes part in, by kind
    std::vector<int> unclear;                        // per playset position: definitions where it is unclear who wins
    int modsRead = 0, modsMissing = 0;
    bool valid = false;
};
inline std::string defKindName(int k) {
    static const char* n[DK_KINDS] = {K("Definition"), K("Event ID"), K("Localization key"), K("Define"), K("On_action")};
    return (k >= 0 && k < DK_KINDS) ? tr(n[k]) : std::string();
}

inline ScriptReport findScriptConflicts(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::map<std::string, ModDefs>& index) {
    ScriptReport r;
    size_t n = ps.mods.size();
    r.perMod.assign(n, std::array<int, DK_KINDS>{});
    r.unclear.assign(n, 0);
    struct Rec { uint64_t h; int32_t mod; uint32_t idx; };
    std::vector<Rec> recs;
    std::vector<const ModDefs*> md(n, nullptr);
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ps.mods[i].enabled || !info.count(ps.mods[i].id)) continue;
        auto it = index.find(ps.mods[i].id);
        if (it == index.end() || !it->second.complete) { r.modsMissing++; continue; }
        md[i] = &it->second;
        total += it->second.defs.size();
        r.modsRead++;
    }
    recs.reserve(total);
    for (size_t i = 0; i < n; i++) if (md[i]) for (size_t k = 0; k < md[i]->defs.size(); k++) recs.push_back({md[i]->defs[k].h, (int32_t)i, (uint32_t)k});
    std::sort(recs.begin(), recs.end(), [](const Rec& a, const Rec& b) { return a.h != b.h ? a.h < b.h : a.mod != b.mod ? a.mod < b.mod : a.idx < b.idx; });
    auto baseName = [](const std::string& p) { size_t s = p.rfind('/'); return s == std::string::npos ? p : p.substr(s + 1); };
    for (size_t a = 0; a < recs.size();) {
        size_t b = a;
        while (b < recs.size() && recs[b].h == recs[a].h) b++;
        if (recs[a].mod != recs[b - 1].mod) {            // defined by at least two different mods
            ScriptConflict sc;
            std::set<std::string> paths;
            bool anyPriority = false, anyReplace = false;
            int overwriters = 0;
            for (size_t x = a; x < b; x++) {
                const ModDefs& m = *md[(size_t)recs[x].mod];
                const DefEntry& d = m.defs[recs[x].idx];
                ScriptHit h; h.mod = recs[x].mod; h.file = m.files[d.file]; h.flags = d.flags;
                paths.insert(h.file);
                if (d.flags & DF_PRIORITY) anyPriority = true;
                if (d.flags & DF_REPLACE) anyReplace = true;
                if (d.flags & DF_OVERWRITES) overwriters++;
                if (x == a) { sc.kind = d.kind; sc.key = m.nameOf(d); sc.area = defFileKind(m.files[d.file]).area; }
                sc.hits.push_back(std::move(h));
            }
            // A later mod's file with the same path replaces the earlier file completely: those earlier definitions do not exist in the game.
            {   // (one pass: a definition is shadowed when a later mod ships the same file)
                std::unordered_map<std::string, int> lastMod;
                for (auto& h : sc.hits) { int& m = lastMod[h.file]; if (h.mod > m) m = h.mod; }
                size_t out = 0;
                for (size_t x = 0; x < sc.hits.size(); x++) if (sc.hits[x].mod >= lastMod[sc.hits[x].file]) { if (out != x) sc.hits[out] = std::move(sc.hits[x]); out++; }
                sc.hits.resize(out);
            }
            std::set<int> modsLeft;
            for (auto& h : sc.hits) modsLeft.insert(h.mod);
            bool keep = true;
            if (modsLeft.size() < 2) keep = false;
            else if (paths.size() == 1) keep = false;                                  // the very same file in several mods: already a file conflict
            else if (sc.kind == DK_EVENT && anyPriority) keep = false;             // id_override_priority: the clash is intended
            else if (sc.kind == DK_LOC && anyReplace) keep = false;                // a replace folder: overriding is intended
            else if (sc.kind == DK_ONACTION && overwriters < 2) keep = false;      // events/on_actions blocks are appended; only effect/trigger overwrite
            if (keep) {
                if (sc.kind == DK_COMMON || sc.kind == DK_DEFINE) {
                    // Two sources disagree on who wins (file name order vs load order), so a winner is only named when both agree.
                    int byLoad = sc.hits.back().mod, byName = sc.hits[0].mod;
                    std::string bestKey, bestFile;
                    for (auto& h : sc.hits) { std::string k = lower(baseName(h.file)) + "|" + lower(h.file); if (bestKey.empty() || k > bestKey) { bestKey = k; byName = h.mod; bestFile = baseName(h.file); } }
                    if (byLoad == byName) { sc.winner = byLoad; sc.sev = 0; sc.note = tr("The later definition replaces the earlier ones"); }
                    else { sc.winner = -1; sc.sev = 1; sc.note = trf("Unclear who wins: file name order favours \"{0}\", load order favours the later mod", {bestFile}); }
                } else if (sc.kind == DK_EVENT) { sc.sev = 1; sc.note = tr("Duplicate event ID: the game logs an error and ignores one of them"); }
                else if (sc.kind == DK_ONACTION) { sc.sev = 1; sc.note = tr("Several mods set effect/trigger of this on_action: only one takes effect"); }
                else { sc.sev = 0; sc.note = tr("Same text key defined by several mods"); }
                std::set<int> seen;
                for (auto& h : sc.hits) if (seen.insert(h.mod).second) { r.perMod[(size_t)h.mod][(size_t)sc.kind]++; if (sc.winner < 0 && sc.sev > 0) r.unclear[(size_t)h.mod]++; }
                r.items.push_back(std::move(sc));
            }
        }
        a = b;
    }
    std::sort(r.items.begin(), r.items.end(), [](const ScriptConflict& x, const ScriptConflict& y) {
        if (x.sev != y.sev) return x.sev > y.sev;
        if (x.kind != y.kind) return x.kind < y.kind;
        if (x.area != y.area) return x.area < y.area;
        return x.key < y.key;
    });
    r.valid = true;
    return r;
}

// Adds the script-level results to the per-mod checks (same indexing as analyzePlayset).
inline void addScriptIssues(std::vector<std::vector<ModIssue>>& issues, const ScriptReport& sr, const Playset& ps) {
    if (!sr.valid || issues.size() != ps.mods.size() || sr.perMod.size() != ps.mods.size()) return;
    for (size_t i = 0; i < ps.mods.size(); i++) {
        const auto& c = sr.perMod[i];
        int ev = c[DK_EVENT], oa = c[DK_ONACTION], com = c[DK_COMMON] + c[DK_DEFINE], loc = c[DK_LOC];
        if (ev > 0) issues[i].push_back({1, trf("{0} event ID(s) are also used by other mods (duplicate event IDs)", {ev})});
        // Only duplicate event IDs can really break something, so only they are a warning. Everything else
        // (on_action effects, definitions where the winner is unclear, plain overrides) is normal modding
        // practice - patches and compatibility mods do it on purpose - so it is information only.
        int other = sr.unclear[i] - ev - oa; if (other < 0) other = 0;
        if (oa > 0) issues[i].push_back({0, trf("{0} on_action(s) are also extended by other mods", {oa})});
        if (other > 0) issues[i].push_back({0, trf("{0} definition(s) are also defined by other mods (see Conflicts > By definition)", {other})});
        if (com > 0 && other == 0) issues[i].push_back({0, trf("{0} script definition(s) are also defined by other mods", {com})});
        if (loc > 0) issues[i].push_back({0, trf("{0} localization key(s) are also defined by other mods", {loc})});
        std::stable_sort(issues[i].begin(), issues[i].end(), [](const ModIssue& a, const ModIssue& b) { return a.sev > b.sev; });
    }
}


// ---------- playset comparison ----------
struct PlaysetDiff {
    std::vector<std::string> onlyA, onlyB;                      // enabled in one playset only
    std::vector<std::string> enabledDiffers;                    // present in both but enabled in one only: "name (on in A, off in B)"
    std::vector<std::string> moved;                             // enabled in both, load position differs (relative order among shared mods)
    int shared = 0; bool sameOrder = true;
};
inline PlaysetDiff comparePlaysets(const Playset& a, const Playset& b, const std::map<std::string, ModInfo>& info) {
    PlaysetDiff d;
    auto nm = [&](const ModRef& m) { auto it = info.find(m.id); return it != info.end() ? it->second.name : (m.name.empty() ? m.id : m.name); };
    std::map<std::string, const ModRef*> ma, mb;
    for (auto& m : a.mods) ma[m.id] = &m;
    for (auto& m : b.mods) mb[m.id] = &m;
    std::vector<std::string> oa, ob;                            // ids enabled in both, in each playset's load order
    for (auto& m : a.mods) {
        auto it = mb.find(m.id);
        bool eb = it != mb.end() && it->second->enabled;
        if (m.enabled && !eb) { (it == mb.end() ? d.onlyA : d.enabledDiffers).push_back(it == mb.end() ? nm(m) : trf("{0} (on in {1}, off in {2})", {nm(m), a.name, b.name})); }
        else if (m.enabled && eb) oa.push_back(m.id);
    }
    for (auto& m : b.mods) {
        auto it = ma.find(m.id);
        bool ea = it != ma.end() && it->second->enabled;
        if (m.enabled && !ea) { if (it == ma.end()) d.onlyB.push_back(nm(m)); else d.enabledDiffers.push_back(trf("{0} (on in {1}, off in {2})", {nm(m), b.name, a.name})); }
        else if (m.enabled && ea) ob.push_back(m.id);
    }
    d.shared = (int)oa.size();
    d.sameOrder = oa == ob;
    if (!d.sameOrder) {
        // Mods that moved = those outside the longest run that keeps the same relative order in both playsets,
        // so one mod jumping far does not make everything between its old and new place look "moved".
        std::map<std::string, int> pb; for (size_t i = 0; i < ob.size(); i++) pb[ob[i]] = (int)i;
        size_t n = oa.size();
        std::vector<int> best(n, 1), prev(n, -1);
        for (size_t i = 0; i < n; i++) for (size_t j = 0; j < i; j++) if (pb[oa[j]] < pb[oa[i]] && best[j] + 1 > best[i]) { best[i] = best[j] + 1; prev[i] = (int)j; }
        int end = 0; for (size_t i = 0; i < n; i++) if (best[i] > best[(size_t)end]) end = (int)i;
        std::set<int> keep; for (int i = end; i >= 0; i = prev[(size_t)i]) keep.insert(i);
        for (size_t i = 0; i < n; i++) if (!keep.count((int)i)) { auto it = info.find(oa[i]); d.moved.push_back(it != info.end() ? it->second.name : oa[i]); }
    }
    return d;
}

// Mods whose files changed since the user last pressed Play (Workshop update, or an edit of a local mod).
// `seen` holds the fingerprint recorded at Play; mods without a record are not flagged (nothing to compare with).
// A recorded fingerprint is "<folder>|<version>|<folder time>" and, when the mod's files had been indexed, "#d:<deep fingerprint>" after it.
// With a deep part on both sides only the deep part (and folder and version) decides, so a harmless change of the folder's time is not
// an update and an edit deep inside the mod is one. Without it (older records, mods not indexed yet) the plain text is compared.
inline bool fingerprintChanged(const std::string& stored, const std::string& now) {
    auto split = [](const std::string& f, std::string& quick, std::string& base, std::string& deep) {
        size_t d = f.find("#d:");
        quick = d == std::string::npos ? f : f.substr(0, d);
        deep = d == std::string::npos ? "" : f.substr(d + 3);
        size_t t = quick.rfind('|');
        base = t == std::string::npos ? quick : quick.substr(0, t);
    };
    std::string q1, b1, d1, q2, b2, d2;
    split(stored, q1, b1, d1); split(now, q2, b2, d2);
    if (!d1.empty() && !d2.empty()) return b1 != b2 || d1 != d2;
    return q1 != q2;
}

inline void addUpdateIssues(std::vector<std::vector<ModIssue>>& issues, const Playset& ps, const std::map<std::string, ModInfo>& info,
                            const std::map<std::string, std::string>& seen, const std::map<std::string, std::string>& now) {
    if (issues.size() != ps.mods.size()) return;
    for (size_t i = 0; i < ps.mods.size(); i++) {
        if (!ps.mods[i].enabled) continue;
        auto s = seen.find(ps.mods[i].id), n = now.find(ps.mods[i].id);
        if (s == seen.end() || n == now.end() || !fingerprintChanged(s->second, n->second)) continue;
        issues[i].push_back({1, tr("Updated since you last pressed Play (Workshop update or edited files)")});
    }
    (void)info;
}

// ---------- auto sort ----------
// Rules, strongest first: (1) dependencies and the load-order instructions of known mods (the built-in database and
// knownmods.json), (2) "patch for X" mods load after X, (3) mods are grouped by type (libraries first ... patches last),
// (4) when two mods in a group overwrite the same files, the smaller, more targeted one loads last (so it wins),
// (5) otherwise the current order is kept. Locked mods never move.
enum Cat { CAT_LIBRARY = 0, CAT_OVERHAUL = 1, CAT_CONTENT = 2, CAT_GRAPHICS = 3, CAT_UI = 4, CAT_TRANSLATION = 5, CAT_PATCH = 6, CAT_COUNT = 7 };
// English name (for knownmods.json "type" values and the English sort report); catName() is the translated one for the screen.
inline const char* catNameEn(int c) {
    static const char* n[CAT_COUNT] = {K("Library"), K("Overhaul"), K("Content"), K("Graphics"), K("Interface"), K("Translation"), K("Patch")};
    return (c >= 0 && c < CAT_COUNT) ? n[c] : "Content";
}
inline std::string catName(int c) { return tr(catNameEn(c)); }
// ModInfo.source is "Workshop" or "Local" (kept as data); this is the text shown for it.
inline std::string sourceLabel(const std::string& s) { return s == "Workshop" ? tr("Workshop") : s == "Local" ? tr("Local") : s; }
inline int catFromName(const std::string& s) {
    std::string l = lower(s);
    for (int c = 0; c < CAT_COUNT; c++) if (l == lower(catNameEn(c))) return c;
    return -1;
}
// whole-word match inside a lower-case string; prefix=true also accepts longer words ("compat" -> "compatibility")
inline bool hasWord(const std::string& hay, const char* w, bool prefix = false) {
    size_t wl = std::strlen(w), pos = 0;
    auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    while ((pos = hay.find(w, pos)) != std::string::npos) {
        bool left = pos == 0 || !alnum(hay[pos - 1]);
        bool right = pos + wl >= hay.size() || !alnum(hay[pos + wl]) || prefix;
        if (left && right) return true;
        pos++;
    }
    return false;
}
// Mod names as people write them differ ("Rise and Fall 1.17", "Rise & Fall [1.17]"): lower case, "&" -> "and",
// no brackets, punctuation as spaces, trailing version numbers removed.
inline std::string normName(const std::string& in) {
    std::string t;
    int depth = 0;
    for (char c : in) {
        if (c == '[' || c == '(') { depth++; t += ' '; continue; }
        if (c == ']' || c == ')') { if (depth > 0) depth--; t += ' '; continue; }
        if (depth > 0) continue;
        if (c == '&') { t += " and "; continue; }
        if (c == '+') { t += " plus "; continue; }       // "AGOT+" is not "AGOT"
        char l = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        t += ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || ((unsigned char)l >= 0x80)) ? l : (l == '.' ? '.' : ' ');
    }
    std::vector<std::string> w;
    std::istringstream is(t);
    for (std::string x; is >> x;) { while (!x.empty() && x.back() == '.') x.pop_back(); while (!x.empty() && x.front() == '.') x.erase(0, 1); if (!x.empty()) w.push_back(x); }
    auto isVersion = [](const std::string& x) {
        size_t i = (x.size() > 1 && x[0] == 'v') ? 1 : 0;
        if (i >= x.size()) return false;
        bool digit = false;
        for (; i < x.size(); i++) { if (x[i] >= '0' && x[i] <= '9') digit = true; else if (x[i] != '.') return false; }
        return digit;
    };
    while (w.size() > 1 && isVersion(w.back())) w.pop_back();
    std::string o;
    for (size_t i = 0; i < w.size(); i++) o += (i ? " " : "") + w[i];
    return o;
}
// Does a (normalised) mod name stand for the known name `pat`? Exact, or `pat` plus up to two extra words that do not
// say it is some other kind of mod ("Rise and Fall Compatibility Patch" is not Rise and Fall).
inline bool nameIs(const std::string& norm, const std::string& pat) {
    if (norm == pat) return true;
    if (norm.size() <= pat.size() + 1 || norm.compare(0, pat.size(), pat) != 0 || norm[pat.size()] != ' ') return false;
    // Short names and acronyms ("agot", "cfp", "unofficial patch") are too common as the start of other mods' names: they must match exactly.
    if (std::count(pat.begin(), pat.end(), ' ') < 2 || pat.size() < 10) return false;
    std::string rest = norm.substr(pat.size() + 1);
    if (std::count(rest.begin(), rest.end(), ' ') > 1) return false;
    for (const char* bad : {"patch", "patches", "compat", "compatch", "compatibility", "translation", "submod", "addon", "for", "plus", "extension", "expansion", "bridge", "of", "in", "and", "with"})
        if (hasWord(rest, bad, std::strcmp(bad, "compat") == 0)) return false;
    return true;
}
inline std::string acronymOf(const std::string& norm) {      // "community flavor pack" -> "cfp"; "and"/"the"/"of" are skipped
    std::string a; std::istringstream is(norm); int words = 0;
    for (std::string x; is >> x;) { words++; if (x == "and" || x == "the" || x == "of" || x == "a" || x == "for") continue; a += x[0]; }
    return words >= 3 ? a : "";
}

// ---- known mods ----
struct KnownMod {
    std::vector<std::string> names;    // normalised names ("rise and fall")
    std::vector<std::string> ids;      // Steam Workshop ids
    int cat = -1;                      // -1 = keep the guessed type
    int pos = 0;                       // -2 very first (total conversions), -1 first, +1 last in the whole load order (after the other mods, before only what is listed in `after`)
    int rank = 0;                      // order inside its type (higher = later)
    std::vector<std::string> after, before;   // must load after / before these mods (normalised names)
    std::string note;                  // shown as the reason
};
// Only facts taken from the mods' own pages (author instructions) go in here. Everything else is left to the type rules.
inline std::vector<KnownMod> builtinKnownMods() {
    std::vector<KnownMod> v;
    { KnownMod k; k.names = {"rise and fall"}; k.ids = {"3554844335"}; k.pos = 1; k.note = K("Rise and Fall: its author says to place it at the very bottom of the load order"); v.push_back(k); }
    { KnownMod k; k.names = {"rui"}; k.after = {"rise and fall"}; k.note = K("RUI: its page says to place it below Rise and Fall"); v.push_back(k); }
    { KnownMod k; k.names = {"unofficial patch", "ck3 unofficial patch"}; k.ids = {"2871648329"}; k.pos = -1;
      k.note = K("Unofficial Patch: its page says it must be loaded at the very top of your mod list (only a total conversion goes above it)"); v.push_back(k); }
    { KnownMod k; k.names = {"better barbershop", "better barbershop mod"}; k.ids = {"2220326926"}; k.cat = CAT_GRAPHICS; k.rank = 5;
      k.after = {"community flavor pack", "ethnicities and portraits expanded", "a game of thrones", "agot"}; k.note = K("Better Barbershop: its page says to put it below CFP, EPE and AGOT"); v.push_back(k); }
    { KnownMod k; k.names = {"community flavor pack", "cfp"}; k.ids = {"2220098919"}; k.cat = CAT_CONTENT; k.note = K("Community Flavor Pack (known mod)"); v.push_back(k); }
    { KnownMod k; k.names = {"ethnicities and portraits expanded", "epe"}; k.ids = {"2507209632"}; k.cat = CAT_GRAPHICS; k.note = K("Ethnicities & Portraits Expanded (known mod)"); v.push_back(k); }
    { KnownMod k; k.names = {"better character ui"}; k.ids = {"2222540784"}; k.cat = CAT_UI; k.rank = 5; k.note = K("Better Character UI: its page says it must be placed under other UI mods"); v.push_back(k); }
    { KnownMod k; k.names = {"a game of thrones", "agot"}; k.ids = {"2962333032"}; k.cat = CAT_OVERHAUL; k.pos = -2;
      k.note = K("A Game of Thrones: the AGOT Submod Core page says it must remain at the top of the load order"); v.push_back(k); }
    // Total conversions replace the whole game setting, so they always go first. Their own Workshop pages give no load-order
    // sentence (checked), so this is the standard rule for total conversions rather than a quote.
    { KnownMod k; k.names = {"lotr realms in exile", "realms in exile"}; k.ids = {"2291024373"}; k.cat = CAT_OVERHAUL; k.pos = -2;
      k.note = K("LotR: Realms in Exile is a total conversion, so it goes first (standard rule for total conversions)"); v.push_back(k); }
    { KnownMod k; k.names = {"princes of darkness"}; k.ids = {"2216659254"}; k.cat = CAT_OVERHAUL; k.pos = -2;
      k.note = K("Princes of Darkness is a total conversion, so it goes first (standard rule for total conversions)"); v.push_back(k); }
    { KnownMod k; k.names = {"elder kings 2"}; k.ids = {"2887120253"}; k.cat = CAT_OVERHAUL; k.pos = -2;
      k.note = K("Elder Kings 2 is a total conversion, so it goes first (standard rule for total conversions)"); v.push_back(k); }
    { KnownMod k; k.names = {"agot submod core"}; k.cat = CAT_LIBRARY; k.pos = -2; k.after = {"a game of thrones", "agot"};
      k.note = K("AGOT Submod Core: its page says to load it immediately after A Game of Thrones"); v.push_back(k); }
    for (const char* sub : {"agot crowns of westeros", "armor of the kingsguard", "legacy of the dragon", "valyrian steel", "agot plus", "the golden company", "agot brightboar westerosi house flavor"}) {
        KnownMod k; k.names = {sub}; k.after = {"agot submod core"};
        k.note = K("AGOT submod: the AGOT Submod Core page says participating submods must load after AGOT Submod Core"); v.push_back(k);
    }
    { KnownMod k; k.names = {"more interactive vassals", "miv"}; k.ids = {"2712590542"}; k.pos = 1; k.before = {"rise and fall"};
      k.note = K("More Interactive Vassals: its page says to place it at the bottom of the load order (Rise and Fall still goes below it)"); v.push_back(k); }
    { KnownMod k; k.names = {"battle graphics"}; k.ids = {"3225355262"}; k.cat = CAT_GRAPHICS;
      k.after = {"community flavor pack", "ethnicities and portraits expanded"};
      k.note = K("Battle Graphics: its page lists the order: other mods, CFP, EPE, the CFP + EPE patch, then Battle Graphics, then its own compatibility patches"); v.push_back(k); }
    { KnownMod k; k.names = {"dynamic family portrait"}; k.ids = {"3608374693"}; k.cat = CAT_GRAPHICS;
      k.after = {"ethnicities and portraits expanded"}; k.note = K("Dynamic Family Portrait: its page says to load it below EPE (above EPE disables its brighter portraits)"); v.push_back(k); }
    { KnownMod k; k.names = {"visible disfigurement no more masks"}; k.ids = {"3245958435"}; k.after = {"community flavor pack", "ethnicities and portraits expanded"};
      k.note = K("Visible Disfigurement: its page says to load it after CFP / EPE"); v.push_back(k); }
    { KnownMod k; k.names = {"unique artifacts plus"}; k.ids = {"3718999968"};
      k.note = K("Unique Artifacts +: no load order is given, but its page warns that conflicts may occur with mods that modify adventure inspiration rewards, unique artifact generation, pilgrimage artifact rewards, legend-related artifact rewards or the same artifact creation effects; if another mod changes the same systems, a compatibility patch may be required"); v.push_back(k); }
    { KnownMod k; k.names = {"more lifestyles"}; k.ids = {"3013259695"}; k.cat = CAT_CONTENT; k.rank = 5;
      k.note = K("More Lifestyles: its page says to put it lower in the load order"); v.push_back(k); }
    // names are compared in their normalised form (trailing version numbers dropped: "elder kings 2" is "elder kings")
    for (auto& k : v) { for (auto& n : k.names) n = normName(n); for (auto& n : k.after) n = normName(n); for (auto& n : k.before) n = normName(n); }
    return v;
}
// The known-mods list has three layers; a later layer replaces an earlier entry that has the same name:
//   1. built into the program, 2. knownmods.online.json (downloaded from this project's GitHub when Updates is pressed), 3. your own knownmods.json
// {"format":1,"revision":3,"mods":[{"names":["my mod"],"ids":["123"],"type":"Graphics","position":"top|first|last","rank":0,"after":["x"],"before":["y"],"note":"why"}]}
inline fs::path knownOnlineFile() { return P(dataDir()) / "knownmods.online.json"; }
// Adds the entries of one list file to v. Returns false (and leaves v alone) when the text is not a usable list.
inline bool applyKnownJson(std::vector<KnownMod>& v, const std::string& text, int* added = nullptr) {
    J root;
    if (!parseJson(text, root) || root.t != J::Obj) return false;
    J* arr = root.get("mods");
    if (!arr || arr->t != J::Arr) return false;
    auto strs = [](J* a, bool norm) { std::vector<std::string> o; if (a && a->t == J::Arr) for (auto& e : a->a) if (e.t == J::Str && !e.s.empty() && e.s.size() < 300) o.push_back(norm ? normName(e.s) : e.s); return o; };
    int n = 0;
    for (auto& e : arr->a) {
        if (e.t != J::Obj) continue;
        KnownMod k;
        k.names = strs(e.get("names"), true); k.ids = strs(e.get("ids"), false);
        k.after = strs(e.get("after"), true); k.before = strs(e.get("before"), true);
        if (k.names.empty() && k.ids.empty()) continue;
        if (J* t = e.get("type"); t && t->t == J::Str) k.cat = catFromName(t->s);
        if (J* p = e.get("position"); p && p->t == J::Str) k.pos = lower(p->s) == "top" ? -2 : lower(p->s) == "first" ? -1 : lower(p->s) == "last" ? 1 : 0;
        if (J* r = e.get("rank"); r && r->t == J::Num) k.rank = (int)std::max(-100.0, std::min(100.0, r->n));
        if (J* nt = e.get("note"); nt && nt->t == J::Str) k.note = nt->s.substr(0, 1000);
        v.erase(std::remove_if(v.begin(), v.end(), [&](const KnownMod& b) {
            for (auto& nm : k.names) for (auto& m : b.names) if (nm == m) return true;
            for (auto& id : k.ids) for (auto& m : b.ids) if (id == m) return true;       // the same Workshop mod under another name is the same entry
            return false; }), v.end());
        v.push_back(std::move(k));
        n++;
    }
    if (added) *added = n;
    return true;
}
// A downloaded list is only kept if it is a list this version understands ("format":1) and is a sensible size.
struct KnownOnlineInfo { bool ok = false; int revision = 0, count = 0; };
inline KnownOnlineInfo checkKnownOnline(const std::string& text) {
    KnownOnlineInfo r;
    if (text.empty() || text.size() > (512u << 10)) return r;
    J root;
    if (!parseJson(text, root) || root.t != J::Obj) return r;
    J* f = root.get("format");
    if (!f || f->t != J::Num || (int)f->n != 1) return r;
    if (J* rv = root.get("revision"); rv && rv->t == J::Num) r.revision = (int)rv->n;
    std::vector<KnownMod> tmp;
    if (!applyKnownJson(tmp, text, &r.count) || r.count > 3000) return r;
    r.ok = true;
    return r;
}
inline std::vector<KnownMod> loadKnownMods(int* onlineRevision = nullptr) {
    std::vector<KnownMod> v = builtinKnownMods();
    std::string text;
    if (onlineRevision) *onlineRevision = 0;
    if (readFile(knownOnlineFile(), text)) {
        KnownOnlineInfo oi = checkKnownOnline(text);
        if (oi.ok && applyKnownJson(v, text) && onlineRevision) *onlineRevision = oi.revision;
    }
    if (readFile(P(dataDir()) / "knownmods.json", text)) applyKnownJson(v, text);
    return v;
}
// The note of a known-mod entry for display: the built-in notes are translatable (K() keys); notes from knownmods.json are shown as written.
inline std::string knownNoteText(const KnownMod& k) { return k.note.empty() ? std::string() : tr(k.note.c_str()); }
inline std::string steamIdOf(const std::string& id);
inline int findKnown(const std::vector<KnownMod>& db, const ModInfo& m) {
    std::string sid = steamIdOf(m.id), nn = normName(m.name);
    for (size_t i = db.size(); i-- > 0;) {                       // later layers (shared list, your own file) win over earlier ones
        for (auto& id : db[i].ids) if (!sid.empty() && id == sid) return (int)i;
        for (auto& n : db[i].names) if (nameIs(nn, n)) return (int)i;
    }
    return -1;
}

// why: "default" is a language-neutral marker (nothing specific said what the mod is); every other text is already translated.
// shown() is the text for display (translates the marker).
struct CatGuess { int cat = CAT_CONTENT; std::string why; std::string shown() const { return why == "default" ? tr("default") : why; } };
inline CatGuess guessCategory(const ModInfo& m) {
    std::string n = lower(m.name);
    std::set<std::string> tags;
    for (auto& t : m.tags) tags.insert(lower(trimmed(t)));
    auto name = [&](std::initializer_list<const char*> ws, bool prefix = false) -> const char* {
        for (const char* w : ws) if (hasWord(n, w, prefix)) return w;
        return nullptr;
    };
    if (const char* w = name({"patch", "patches", "hotfix", "bridge", "submod", "sub-mod", "addon", "add-on"})) return {CAT_PATCH, trf("name has \"{0}\"", {w})};
    if (const char* w = name({"compat", "compatch", "compatibility"}, true)) return {CAT_PATCH, trf("name has \"{0}\"", {w})};
    if (tags.count("fixes")) return {CAT_PATCH, tr("tag Fixes")};
    if (tags.count("translation")) return {CAT_TRANSLATION, tr("tag Translation")};
    if (const char* w = name({"translation", "localization", "localisation", "l10n", "locale"})) return {CAT_TRANSLATION, trf("name has \"{0}\"", {w})};
    if (const char* w = name({"library", "framework", "api", "lib", "core", "dependency", "dependencies", "requirements"})) return {CAT_LIBRARY, trf("name has \"{0}\"", {w})};
    if (tags.count("total conversion")) return {CAT_OVERHAUL, tr("tag Total Conversion")};
    if (const char* w = name({"overhaul", "total conversion"})) return {CAT_OVERHAUL, trf("name has \"{0}\"", {w})};
    if (m.replacePaths.size() >= 4) return {CAT_OVERHAUL, trf("replaces {0} vanilla folders", {m.replacePaths.size()})};
    if (tags.count("interface")) return {CAT_UI, tr("tag Interface")};
    if (const char* w = name({"ui", "gui", "hud", "interface", "tooltip", "tooltips"})) return {CAT_UI, trf("name has \"{0}\"", {w})};
    if (tags.count("graphics")) return {CAT_GRAPHICS, tr("tag Graphics")};
    if (tags.count("character models")) return {CAT_GRAPHICS, tr("tag Character Models")};
    if (tags.count("sound")) return {CAT_GRAPHICS, tr("tag Sound")};
    if (const char* w = name({"portrait", "portraits", "clothing", "clothes", "hair", "hairstyle", "hairstyles", "beard", "beards", "texture", "textures", "skin", "skins", "ethnicity", "ethnicities", "visual", "visuals", "3d", "gfx", "models", "music", "soundtrack", "artwork", "icons"})) return {CAT_GRAPHICS, trf("name has \"{0}\"", {w})};
    return {CAT_CONTENT, "default"};
}
// When names and tags say nothing, the files the mod ships say what it is.
inline CatGuess guessFromFiles(const ModFiles& mf) {
    size_t n = mf.files.size();
    if (!mf.complete || n == 0) return {CAT_CONTENT, "default"};
    size_t loc = 0, ui = 0, vis = 0, world = 0;
    for (auto& f : mf.files) {
        auto starts = [&](const char* p) { return f.rfind(p, 0) == 0; };
        if (starts("localization/")) loc++;
        else if (starts("gui/") || starts("gfx/interface/") || starts("interface/") || starts("fonts/")) ui++;
        else if (starts("gfx/") || starts("common/genes/") || starts("music/") || starts("sound/") || starts("sfx/") || starts("common/ethnicities/") || starts("common/bookmark_portraits/")) vis++;
        else if (starts("map_data/") || starts("history/provinces/") || starts("history/titles/") || starts("common/landed_titles/") || starts("history/characters/") || starts("history/cultures/")) world++;
    }
    auto pct = [&](size_t x) { return x * 100 >= n * 85; };
    if (pct(loc)) return {CAT_TRANSLATION, tr("files: mostly localization")};
    if (pct(ui)) return {CAT_UI, tr("files: mostly interface")};
    if (pct(vis)) return {CAT_GRAPHICS, tr("files: mostly graphics/audio")};
    if (n >= 200 && world * 100 >= n * 40) return {CAT_OVERHAUL, tr("files: rewrites the map and history")};
    return {CAT_CONTENT, "default"};
}

struct SortMove { int from = 0, to = 0; std::string id, reason; };
struct SortPlan {
    std::vector<int> order;                  // order[newPosition] = old position
    std::vector<SortMove> moves;             // only mods whose position changed, in new order
    std::vector<std::string> warnings;       // things the sort could not satisfy
    std::vector<int> cat;                    // per OLD position
    std::vector<std::string> catWhy;         // per OLD position
    std::vector<std::string> known;          // per OLD position: note of the known-mod entry ("" if none)
    int conflictChoices = 0;                 // how many conflict tie-breaks were applied
    int knownCount = 0;                      // how many mods were recognised from the known-mods list
    int patchLinks = 0;                      // how many "patch for X" links were found
    bool changed = false;
};

inline SortPlan planSort(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::set<std::string>& locked,
                         const std::map<std::string, int>& overrides, const ConflictReport* cr, const std::map<std::string, ModFiles>* index,
                         const std::vector<KnownMod>* db = nullptr) {
    SortPlan plan;
    const int n = (int)ps.mods.size();
    plan.cat.assign((size_t)n, CAT_CONTENT);
    plan.catWhy.assign((size_t)n, "");
    plan.known.assign((size_t)n, "");
    std::vector<int> tier((size_t)n, 0), rank((size_t)n, 0), kidx((size_t)n, -1);
    std::vector<std::string> norm((size_t)n);
    std::map<std::string, int> byName;                       // lower(name) -> first position (dependencies)
    for (int i = 0; i < n; i++) {
        auto it = info.find(ps.mods[(size_t)i].id);
        if (it == info.end()) { plan.catWhy[(size_t)i] = tr("not installed"); continue; }
        norm[(size_t)i] = normName(it->second.name);
        CatGuess g = guessCategory(it->second);
        if (g.why == "default" && index) { auto f = index->find(ps.mods[(size_t)i].id); if (f != index->end()) g = guessFromFiles(f->second); }
        if (db) {
            int k = findKnown(*db, it->second);
            if (k >= 0) {
                const KnownMod& km = (*db)[(size_t)k];
                kidx[(size_t)i] = k; plan.known[(size_t)i] = knownNoteText(km); plan.knownCount++;   // built-in notes are K() keys; notes from knownmods.json stay as written
                if (km.cat >= 0) g = {km.cat, plan.known[(size_t)i]};
                tier[(size_t)i] = km.pos; rank[(size_t)i] = km.rank;
            }
        }
        auto ov = overrides.find(ps.mods[(size_t)i].id);
        if (ov != overrides.end() && ov->second >= 0 && ov->second < CAT_COUNT) g = {ov->second, tr("set by you")};
        plan.cat[(size_t)i] = g.cat; plan.catWhy[(size_t)i] = g.why;
        byName.emplace(lower(trimmed(it->second.name)), i);
    }
    auto isLocked = [&](int i) { return locked.count(ps.mods[(size_t)i].id) > 0; };
    auto nameOf = [&](int i) { auto it = info.find(ps.mods[(size_t)i].id); return it == info.end() ? ps.mods[(size_t)i].id : it->second.name; };

    struct Edge { int from, to, kind; std::string text, note, other; bool after = false; };   // text: the reason shown for a move; note/other/after: what a known-mod rule says (for the locked-mod warning)   // from loads before to; kind 0 dependency, 1 conflict tie-break, 2 known-mod rule, 3 patch link
    std::vector<Edge> edges;
    std::vector<std::vector<int>> out((size_t)n);            // adjacency (edge indices)
    auto addEdge = [&](int a, int b, int kind, const std::string& text, const std::string& note = "", const std::string& other = "", bool after = false) { out[(size_t)a].push_back((int)edges.size()); edges.push_back({a, b, kind, text, note, other, after}); };
    for (int i = 0; i < n; i++) {
        auto it = info.find(ps.mods[(size_t)i].id);
        if (it == info.end()) continue;
        std::string self = lower(trimmed(it->second.name));
        for (auto& d : it->second.deps) {
            auto f = byName.find(lower(trimmed(d)));
            if (f == byName.end() || f->second == i || lower(trimmed(d)) == self) continue;
            addEdge(f->second, i, 0, trf("Needs \"{0}\" to load first", {nameOf(f->second)}));
        }
    }
    auto reaches = [&](int from, int target) {               // is there a path from -> target?
        std::vector<char> seen((size_t)n, 0);
        std::vector<int> st{from};
        while (!st.empty()) {
            int u = st.back(); st.pop_back();
            if (u == target) return true;
            if (seen[(size_t)u]) continue;
            seen[(size_t)u] = 1;
            for (int e : out[(size_t)u]) st.push_back(edges[(size_t)e].to);
        }
        return false;
    };
    // load-order instructions of known mods ("below X", "above Y")
    if (db) {
        for (int i = 0; i < n; i++) {
            if (kidx[(size_t)i] < 0) continue;
            const KnownMod& km = (*db)[(size_t)kidx[(size_t)i]];
            auto link = [&](const std::string& other, bool iAfter) {
                for (int j = 0; j < n; j++) {
                    if (j == i || norm[(size_t)j].empty()) continue;
                    bool same = nameIs(norm[(size_t)j], other);
                    if (!same && kidx[(size_t)j] >= 0) for (auto& kn : (*db)[(size_t)kidx[(size_t)j]].names) if (kn == other) same = true;   // recognised by Workshop id
                    if (!same) continue;
                    int a = iAfter ? j : i, b = iAfter ? i : j;
                    if (reaches(b, a)) { plan.warnings.push_back(iAfter ? trf("Could not put \"{0}\" below \"{1}\" (it would contradict a dependency).", {nameOf(i), nameOf(j)}) : trf("Could not put \"{0}\" above \"{1}\" (it would contradict a dependency).", {nameOf(i), nameOf(j)})); continue; }
                    std::string note = knownNoteText(km);
                    addEdge(a, b, 2, iAfter ? (note.empty() ? trf("loads after \"{0}\"", {nameOf(j)}) : trf("{0}: loads after \"{1}\"", {note, nameOf(j)}))
                                            : (note.empty() ? trf("loads before \"{0}\"", {nameOf(j)}) : trf("{0}: loads before \"{1}\"", {note, nameOf(j)})), note, nameOf(j), iAfter);
                }
            };
            for (auto& o : km.after) link(o, true);
            for (auto& o : km.before) link(o, false);
        }
    }
    // "X + Y compatibility patch": a patch loads after the mods it is named for
    {
        std::vector<std::string> acr((size_t)n);
        for (int j = 0; j < n; j++) acr[(size_t)j] = acronymOf(norm[(size_t)j]);
        for (int p = 0; p < n; p++) {
            if (plan.cat[(size_t)p] != CAT_PATCH || norm[(size_t)p].empty()) continue;
            for (int j = 0; j < n; j++) {
                if (j == p || plan.cat[(size_t)j] == CAT_PATCH || norm[(size_t)j].size() < 5) continue;
                bool hit = false;
                std::string padded = " " + norm[(size_t)p] + " ";
                if (padded.find(" " + norm[(size_t)j] + " ") != std::string::npos) hit = true;
                else if (acr[(size_t)j].size() >= 3 && padded.find(" " + acr[(size_t)j] + " ") != std::string::npos) hit = true;
                if (!hit || reaches(p, j)) continue;
                addEdge(j, p, 3, trf("Is a patch for \"{0}\", so it loads after it", {nameOf(j)}));
                plan.patchLinks++;
            }
        }
    }
    if (cr && cr->valid && index) {
        auto sizeOf = [&](int i) -> size_t { auto f = index->find(ps.mods[(size_t)i].id); return f == index->end() ? 0 : f->second.files.size(); };
        int budget = 3000;
        for (auto& p : cr->pairs) {
            if (budget-- <= 0 || p.count < 3) continue;
            int a = p.a, b = p.b;                            // a currently loads before b (b wins)
            if (a < 0 || b < 0 || a >= n || b >= n || isLocked(a) || isLocked(b) || plan.cat[(size_t)a] != plan.cat[(size_t)b]) continue;
            if (tier[(size_t)a] != tier[(size_t)b] || rank[(size_t)a] != rank[(size_t)b]) continue;
            size_t sa = sizeOf(a), sb = sizeOf(b);
            if (sb <= sa * 2 || sa == 0) continue;           // only act when b is clearly the bigger mod: it should load first
            if (reaches(a, b)) continue;                     // would contradict a dependency or an earlier choice
            addEdge(b, a, 1, /* never shown (the reason of a conflict tie-break is built below) */ "loads after \"" + nameOf(b) + "\" (they overwrite " + std::to_string(p.count) + " of the same files; the smaller, more targeted mod goes last so it wins)");
            plan.conflictChoices++;
        }
    }
    // Kahn's algorithm over the movable mods, always taking the ready mod with the lowest (first/last, type, rank, current position)
    using Key = std::tuple<int, int, int, int>;
    auto keyOf = [&](int i) { return Key{tier[(size_t)i], plan.cat[(size_t)i], rank[(size_t)i], i}; };
    std::vector<int> indeg((size_t)n, 0);
    std::vector<char> done((size_t)n, 0);
    for (auto& e : edges) if (!isLocked(e.from) && !isLocked(e.to)) indeg[(size_t)e.to]++;
    std::set<Key> ready;
    int freeCount = 0;
    for (int i = 0; i < n; i++) { if (isLocked(i)) continue; freeCount++; if (indeg[(size_t)i] == 0) ready.insert(keyOf(i)); }
    std::vector<int> seq;
    bool cycleWarned = false;
    while ((int)seq.size() < freeCount) {
        int pick = -1;
        if (!ready.empty()) { pick = std::get<3>(*ready.begin()); ready.erase(ready.begin()); }
        else {                                               // circular dependency: break it at the lowest-ranked mod that is really in the cycle
            for (int i = 0; i < n; i++) if (!isLocked(i) && !done[(size_t)i] && (pick < 0 || keyOf(i) < keyOf(pick))) pick = i;
            // every mod left is waiting for another one; walking back along "waits for" must end in a loop, and only mods in that loop may be freed
            auto waitsFor = [&](int u) { for (auto& e : edges) if (e.to == u && !isLocked(e.from) && !isLocked(e.to) && !done[(size_t)e.from]) return e.from; return -1; };
            std::vector<int> where((size_t)n, -1), path;
            for (int u = pick; u >= 0;) {
                if (where[(size_t)u] >= 0) {
                    int best = -1;
                    for (size_t q = (size_t)where[(size_t)u]; q < path.size(); q++) if (best < 0 || keyOf(path[q]) < keyOf(best)) best = path[q];
                    if (best >= 0) pick = best;
                    break;
                }
                where[(size_t)u] = (int)path.size(); path.push_back(u);
                u = waitsFor(u);
            }
            if (!cycleWarned) { plan.warnings.push_back(trf("Circular dependency around \"{0}\"; the dependency was ignored.", {nameOf(pick)})); cycleWarned = true; }
        }
        if (pick < 0 || done[(size_t)pick]) continue;
        done[(size_t)pick] = 1;
        seq.push_back(pick);
        for (int e : out[(size_t)pick]) {
            int to = edges[(size_t)e].to;
            if (isLocked(to) || isLocked(pick) || done[(size_t)to]) continue;
            if (--indeg[(size_t)to] == 0) ready.insert(keyOf(to));
        }
    }
    plan.order.assign((size_t)n, -1);
    size_t k = 0;
    for (int p = 0; p < n; p++) plan.order[(size_t)p] = isLocked(p) ? p : seq[k++];
    std::vector<int> newPos((size_t)n, 0);
    for (int p = 0; p < n; p++) newPos[(size_t)plan.order[(size_t)p]] = p;
    for (auto& e : edges) {
        if (e.kind == 1 || e.kind == 3 || newPos[(size_t)e.from] < newPos[(size_t)e.to]) continue;
        if (isLocked(e.from) || isLocked(e.to)) {
            if (e.kind == 0) plan.warnings.push_back(trf("\"{0}\" needs \"{1}\" to load first, but a locked mod prevents it.", {nameOf(e.to), nameOf(e.from)}));
            else if (e.note.empty()) plan.warnings.push_back(e.after ? trf("\"{0}\" loads after \"{1}\", but a locked mod prevents it.", {nameOf(e.to), e.other})
                                                                    : trf("\"{0}\" loads before \"{1}\", but a locked mod prevents it.", {nameOf(e.to), e.other}));
            else plan.warnings.push_back(e.after ? trf("\"{0}\" {1}: loads after \"{2}\", but a locked mod prevents it.", {nameOf(e.to), e.note, e.other})
                                                 : trf("\"{0}\" {1}: loads before \"{2}\", but a locked mod prevents it.", {nameOf(e.to), e.note, e.other}));
        }
    }
    for (int p = 0; p < n; p++) {
        int old = plan.order[(size_t)p];
        if (old != p) plan.changed = true;
        if (old == p) continue;
        SortMove mv; mv.from = old; mv.to = p; mv.id = ps.mods[(size_t)old].id;
        std::string why;
        for (auto& e : edges)                                // an instruction or dependency that used to be violated is the strongest reason
            if (e.to == old && e.kind != 1 && e.from > old && why.empty()) why = e.text;
        if (why.empty() && tier[(size_t)old] > 0) why = plan.known[(size_t)old];
        if (why.empty() && tier[(size_t)old] < 0) why = plan.known[(size_t)old];
        if (why.empty()) for (auto& e : edges) if (e.to == old && e.kind == 1) { why = trf("Loads after \"{0}\" (they overwrite the same files; this is the smaller mod, so it wins)", {nameOf(e.from)}); break; }
        if (why.empty()) why = plan.catWhy[(size_t)old].empty() || plan.catWhy[(size_t)old] == "default" ? trf("{0} group", {catName(plan.cat[(size_t)old])}) : trf("{0} group ({1})", {catName(plan.cat[(size_t)old]), plan.catWhy[(size_t)old]});
        mv.reason = why;
        plan.moves.push_back(std::move(mv));
    }
    return plan;
}

// Natural order for versions and names: digit runs compare as numbers ("1.9" < "1.10"), letters ignore case.
inline int naturalCompare(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        bool da = a[i] >= '0' && a[i] <= '9', db = b[j] >= '0' && b[j] <= '9';
        if (da && db) {
            size_t si = i, sj = j;
            while (si < a.size() && a[si] == '0') si++;
            while (sj < b.size() && b[sj] == '0') sj++;
            size_t ei = si, ej = sj;
            while (ei < a.size() && a[ei] >= '0' && a[ei] <= '9') ei++;
            while (ej < b.size() && b[ej] >= '0' && b[ej] <= '9') ej++;
            if (ei - si != ej - sj) return ei - si < ej - sj ? -1 : 1;
            int c = a.compare(si, ei - si, b, sj, ej - sj);
            if (c) return c < 0 ? -1 : 1;
            i = ei; j = ej;
        } else {
            unsigned char x = (unsigned char)std::tolower((unsigned char)a[i]), y = (unsigned char)std::tolower((unsigned char)b[j]);
            if (x != y) return x < y ? -1 : 1;
            i++; j++;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

// ---------- Paradox Launcher playset format ----------
// { "game": "ck3", "name": "...", "mods": [ { "displayName", "enabled", "position", "steamId" } ] }

// "ugc_2216660858.mod" -> "2216660858" (Steam Workshop id), otherwise "".
inline std::string steamIdOf(const std::string& id) {
    if (id.size() < 9 || id.rfind("ugc_", 0) != 0 || lower(id.substr(id.size() - 4)) != ".mod") return "";
    std::string d = id.substr(4, id.size() - 8);
    for (char c : d) if (c < '0' || c > '9') return "";
    return d;
}
inline bool validID(const std::string& id) {
    if (id.size() < 5 || lower(id.substr(id.size() - 4)) != ".mod") return false;
    return id.find('/') == std::string::npos && id.find('\\') == std::string::npos && id.find("..") == std::string::npos;
}

// Writes enabled mods in load order, plus disabled mods that are not installed (so nothing in a file is lost).
// Output is compact JSON identical to the launcher's own files.
inline std::string exportLauncherPlayset(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::string& game = "ck3") {
    J root = J::obj();
    root.set("game", J::str(game));
    root.set("name", J::str(ps.name));
    J ms = J::arr();
    int pos = 0;
    for (auto& m : ps.mods) {
        auto it = info.find(m.id);
        bool installed = it != info.end();
        if (!m.enabled && installed) continue;
        J mo = J::obj();
        std::string nm = installed ? it->second.name : m.name;
        if (nm.empty()) nm = m.id;
        mo.set("displayName", J::str(nm));
        mo.set("enabled", J::boolean(m.enabled));
        mo.set("position", J::num(pos++));
        std::string sid = steamIdOf(m.id);
        if (!sid.empty()) mo.set("steamId", J::str(sid));
        else if (installed && validID(m.id)) mo.set("gameRegistryId", J::str("mod/" + m.id));   // local mods are tied to their file, not to a display name that can change or repeat
        ms.a.push_back(std::move(mo));
    }
    root.set("mods", std::move(ms));
    std::string out; dumpMin(root, out);
    return out;
}

inline std::string jsonScalarText(const J* v) {
    if (!v) return "";
    if (v->t == J::Str) return v->s;
    if (v->t == J::Num) { char b[40]; snprintf(b, sizeof b, "%.0f", v->n); return b; }
    return "";
}

struct ImportResult { bool ok = false; std::string error; Playset playset; int invalid = 0; int unmatched = 0; };

// Mods that cannot be tied to an installed mod or a Workshop id get a placeholder id so they are kept (and shown as not installed).
inline const char* LOCAL_PREFIX = "local:";

inline ImportResult parsePlaysetFile(const std::string& text, const std::vector<ModInfo>& installed) {
    ImportResult r;
    if (text.size() > (5u << 20)) { r.error = tr("That file is too large to be a playset."); return r; }
    J root;
    if (!parseJson(text, root) || root.t != J::Obj) { r.error = tr("That file is not valid JSON."); return r; }
    if (J* g = root.get("game"); g && g->t == J::Str && !g->s.empty() && lower(g->s) != "ck3") {
        r.error = trf("That playset is for \"{0}\", not Crusader Kings III.", {g->s});
        return r;
    }
    J* ms = root.get("mods");
    if (!ms || ms->t != J::Arr) { r.error = tr("That file is not a playset (no mod list found)."); return r; }
    if (ms->a.size() > 5000) { r.error = tr("The playset file lists too many mods."); return r; }
    if (J* n = root.get("name"); n && n->t == J::Str) r.playset.name = n->s.substr(0, 100);

    struct E { long pos; size_t order; ModRef m; };
    std::vector<E> es;
    std::set<std::string> seen;
    size_t order = 0;
    std::map<std::string, std::string> byName;   // lower-case display name -> mod id, built only if some entry needs it
    bool byNameBuilt = false;
    for (auto& mo : ms->a) {
        if (mo.t != J::Obj) { r.invalid++; continue; }
        std::string display = jsonScalarText(mo.get("displayName"));
        std::string steam = jsonScalarText(mo.get("steamId"));
        std::string reg = jsonScalarText(mo.get("gameRegistryId"));
        std::replace(reg.begin(), reg.end(), '\\', '/');
        if (reg.rfind("mod/", 0) == 0) reg = reg.substr(4);
        std::string id;
        if (!reg.empty() && validID(reg)) id = reg;
        else if (!steam.empty() && steam.find_first_not_of("0123456789") == std::string::npos) id = "ugc_" + steam + ".mod";
        else if (!display.empty()) {
            if (!byNameBuilt) { for (auto& m : installed) byName.emplace(lower(m.name), m.id); byNameBuilt = true; }
            if (auto it = byName.find(lower(display)); it != byName.end()) id = it->second;
            if (id.empty()) { id = LOCAL_PREFIX + display; r.unmatched++; }
        }
        if (id.empty()) { r.invalid++; continue; }
        if (!seen.insert(id).second) continue;
        ModRef m; m.id = id; m.name = display; m.enabled = true;
        if (J* e = mo.get("enabled"); e && e->t == J::Bool) m.enabled = e->b;
        long pos = 1L << 30;
        std::string ps = jsonScalarText(mo.get("position"));
        if (!ps.empty() && ps.find_first_not_of("0123456789") == std::string::npos && ps.size() < 9) pos = std::stol(ps);
        es.push_back({pos, order++, std::move(m)});
    }
    std::stable_sort(es.begin(), es.end(), [](const E& a, const E& b) { return a.pos != b.pos ? a.pos < b.pos : a.order < b.order; });
    for (auto& e : es) r.playset.mods.push_back(std::move(e.m));
    r.ok = true;
    return r;
}

// Ties placeholder entries to installed mods (by name), refreshes names, and lists every other installed mod as disabled.
inline void syncPlayset(Playset& ps, const std::vector<ModInfo>& installed) {
    std::map<std::string, const ModInfo*> byId;
    std::map<std::string, const ModInfo*> byName;
    for (auto& m : installed) { byId[m.id] = &m; byName.emplace(lower(m.name), &m); }
    std::set<std::string> have;
    std::vector<ModRef> out;
    for (auto m : ps.mods) {
        if (m.id.rfind(LOCAL_PREFIX, 0) == 0) {
            if (auto it = byName.find(lower(m.name)); it != byName.end()) m.id = it->second->id;
        }
        if (auto it = byId.find(m.id); it != byId.end()) m.name = it->second->name;
        if (!have.insert(m.id).second) continue;
        out.push_back(std::move(m));
    }
    for (auto& m : installed) if (!have.count(m.id)) out.push_back({m.id, false, m.name});
    ps.mods = std::move(out);
}

// ---------- playset files ----------
inline std::map<std::string, ModInfo> infoMap(const std::vector<ModInfo>& mods) {
    std::map<std::string, ModInfo> info;
    for (auto& m : mods) info[m.id] = m;
    return info;
}
inline fs::path playsetPath(const std::string& name) { return playsetsDir() / P(sanitizeFileName(name) + ".json"); }

// True if `name` would collide with another playset. Besides the playsets in memory it checks the real folder, because
// Windows treats names that differ only by capitals (also non-English ones) as the same file.
// `except` is the playset being renamed: its own file does not count as a clash.
inline bool nameTaken(const std::vector<Playset>& all, const std::string& name, const std::string& except = "") {
    std::string n = sanitizeFileName(name);
    for (auto& p : all) {
        if (!except.empty() && p.name == except) continue;
        if (lower(p.name) == lower(n)) return true;
    }
    std::error_code ec;
    fs::path np = playsetPath(n);
    if (!fs::exists(np, ec)) return false;
    if (!except.empty()) {
        fs::path ep = playsetPath(except);
        if (fs::exists(ep, ec) && fs::equivalent(np, ep, ec)) return false;   // it is this playset's own file
    }
    return true;
}
inline std::string uniqueName(const std::vector<Playset>& all, const std::string& wanted) {
    std::string base = sanitizeFileName(wanted.empty() ? "Imported playset" : wanted);
    if (!nameTaken(all, base)) return base;
    for (int i = 2; i < 10000; i++) {
        std::string c = base + " (" + std::to_string(i) + ")";
        if (!nameTaken(all, c)) return c;
    }
    return base + " (new)";
}

inline bool savePlayset(const Playset& ps, const std::map<std::string, ModInfo>& info) {
    return writeFileAtomic(playsetPath(ps.name), exportLauncherPlayset(ps, info));
}

// ---------- playset backups ----------
// Every backup is a normal launcher-format playset file in <data>/Backups/<playset>/, named "<date time> <reason>.json".
inline fs::path backupsDir(const std::string& playsetName) {
    fs::path d = P(dataDir()) / "Backups" / P(sanitizeFileName(playsetName));
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}
inline std::string stampNow() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%d %H-%M-%S", &tmv);
    return b;
}
inline std::vector<fs::path> listBackups(const std::string& playsetName) {     // newest first
    std::vector<fs::path> v;
    std::error_code ec;
    fs::path d = P(dataDir()) / "Backups" / P(sanitizeFileName(playsetName));
    if (!fs::is_directory(d, ec)) return v;
    for (auto& e : fs::directory_iterator(d, ec)) if (e.is_regular_file(ec) && lower(e.path().extension().u8string()) == ".json") v.push_back(e.path());
    std::sort(v.begin(), v.end(), [](const fs::path& a, const fs::path& b) { return a.filename().u8string() > b.filename().u8string(); });
    return v;
}
// Writes a backup of the playset as it is right now. Returns the file, or "" if nothing was written
// (an identical backup is already the newest one, or the write failed). Only the newest `keep` backups are kept.
inline std::string backupPlayset(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::string& reason, size_t keep = 30) {
    std::string data = exportLauncherPlayset(ps, info);
    auto existing = listBackups(ps.name);
    if (!existing.empty()) {
        std::string last;
        if (readFile(existing.front(), last) && last == data) return "";
    }
    std::string why = sanitizeFileName(reason.empty() ? "backup" : reason);
    std::string stamp = stampNow();
    int seq = 0;                          // files made in the same second get "-02", "-03"... so newest-first ordering stays right
    for (auto& e : existing) {
        std::string nm = e.filename().u8string();
        if (nm.compare(0, stamp.size(), stamp) != 0) continue;
        int k = 1;
        if (nm.size() > stamp.size() + 3 && nm[stamp.size()] == '-') k = std::atoi(nm.substr(stamp.size() + 1, 2).c_str());
        seq = std::max(seq, k);
    }
    std::string suf;
    if (seq > 0) { char b2[16]; std::snprintf(b2, sizeof b2, "-%02d", seq + 1); suf = b2; }
    fs::path f = backupsDir(ps.name) / P(stamp + suf + " " + why + ".json");
    std::error_code ec;
    if (!writeFileAtomic(f, data)) return "";
    existing = listBackups(ps.name);
    for (size_t i = keep; i < existing.size(); i++) fs::remove(existing[i], ec);
    return f.u8string();
}
// Puts the mods of `from` (a backup) into `to`: backup order and enabled state first, then any mods the backup did not know about.
inline void applyBackupOrder(Playset& to, const Playset& from) {
    std::map<std::string, ModRef> cur;
    for (auto& m : to.mods) cur.emplace(m.id, m);
    std::vector<ModRef> out; std::set<std::string> used;
    for (auto& m : from.mods) if (cur.count(m.id) && used.insert(m.id).second) out.push_back(m);
    for (auto& m : to.mods) if (!used.count(m.id)) out.push_back(m);
    to.mods = out;
}
inline bool deletePlayset(const std::string& name) {
    std::error_code ec;
    return fs::remove(playsetPath(name), ec);
}
// Renames a playset: the file gets the new name and the name written inside it matches.
inline bool renamePlayset(Playset& ps, const std::string& newName, const std::map<std::string, ModInfo>& info) {
    std::string old = ps.name;
    fs::path op = playsetPath(old), np = playsetPath(newName);
    std::error_code ec;
    // Same file under a different spelling (a change of capitals on Windows): rename it in place.
    bool same = op != np && fs::exists(op, ec) && fs::exists(np, ec) && fs::equivalent(op, np, ec);
    if (same) {
        // Two steps through a temporary name: a rename that only changes capitals is ignored by some file systems.
        fs::path mid = op; mid += ".rename.tmp";
        fs::rename(op, mid, ec); if (ec) return false;
        fs::rename(mid, np, ec); if (ec) { std::error_code ec2; fs::rename(mid, op, ec2); return false; }
    }
    ps.name = newName;
    if (!savePlayset(ps, info)) { ps.name = old; return false; }
    if (!same && op != np) deletePlayset(old);
    // the playset keeps its backups under the new name
    {
        fs::path ob = P(dataDir()) / "Backups" / P(sanitizeFileName(old)), nb = P(dataDir()) / "Backups" / P(sanitizeFileName(newName));
        std::error_code bec;
        if (ob != nb && fs::is_directory(ob, bec)) {
            if (!fs::exists(nb, bec)) fs::rename(ob, nb, bec);
            else {
                fs::create_directories(nb, bec);
                for (auto& f : fs::directory_iterator(ob, bec)) { std::error_code mec; fs::path t = nb / f.path().filename(); if (!fs::exists(t, mec)) fs::rename(f.path(), t, mec); }
                fs::remove_all(ob, bec);
            }
        }
    }
    return true;
}

inline std::vector<Playset> loadPlaysets(const std::vector<ModInfo>& installed) {
    std::vector<Playset> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(playsetsDir(), ec)) {
        if (!e.is_regular_file(ec) || lower(e.path().extension().u8string()) != ".json") continue;
        std::string text;
        if (!readFile(e.path(), text, 5u << 20)) continue;
        ImportResult r = parsePlaysetFile(text, installed);
        if (!r.ok) continue;
        Playset p = r.playset;
        p.name = e.path().stem().u8string();  // the file name is the playset name
        if (std::string clean = sanitizeFileName(p.name); clean != p.name && !clean.empty()) {
            // a name that would be saved under another file name (too long, stray spaces) is renamed once now, so a save never creates a second copy
            std::error_code rec;
            fs::path target = playsetPath(clean);
            if (!fs::exists(target, rec)) { fs::rename(e.path(), target, rec); if (!rec) p.name = clean; }
        }
        syncPlayset(p, installed);
        out.push_back(std::move(p));
    }
    std::sort(out.begin(), out.end(), [](const Playset& a, const Playset& b) { return lower(a.name) < lower(b.name); });
    return out;
}

// One-time upgrade from the old app database (playsets.json, v0.1-0.3): each playset becomes a launcher-format file.
// The old file is kept, renamed to playsets.json.migrated. Returns the number of playsets converted.
inline int migrateLegacyStore(const std::vector<ModInfo>& installed, Settings& settings) {
    fs::path old = P(dataDir()) / "playsets.json";
    std::error_code ec;
    if (!fs::exists(old, ec)) return 0;
    for (auto& e : fs::directory_iterator(playsetsDir(), ec))
        if (lower(e.path().extension().u8string()) == ".json") return 0;  // already using files; leave the old file alone
    std::string text; J root;
    if (!readFile(old, text) || !parseJson(text, root) || root.t != J::Obj) return 0;
    auto info = infoMap(installed);
    std::vector<Playset> done;
    if (J* pl = root.get("playsets"); pl && pl->t == J::Arr) {
        for (auto& po : pl->a) {
            if (po.t != J::Obj) continue;
            Playset p;
            std::string nm;
            if (J* n = po.get("name"); n && n->t == J::Str) nm = n->s;
            p.name = uniqueName(done, nm);
            if (J* ms = po.get("mods"); ms && ms->t == J::Arr) {
                for (auto& mo : ms->a) {
                    if (mo.t != J::Obj) continue;
                    ModRef m;
                    if (J* i = mo.get("id"); i && i->t == J::Str) m.id = i->s;
                    if (J* e = mo.get("enabled"); e && e->t == J::Bool) m.enabled = e->b;
                    if (J* n2 = mo.get("name"); n2 && n2->t == J::Str) m.name = n2->s;
                    if (!m.id.empty()) p.mods.push_back(std::move(m));
                }
            }
            if (savePlayset(p, info)) done.push_back(std::move(p));
        }
    }
    if (settings.ck3Dir.empty()) if (J* d = root.get("ck3Dir"); d && d->t == J::Str) settings.ck3Dir = d->s;
    if (J* a = root.get("active"); a && a->t == J::Str) settings.active = sanitizeFileName(a->s);
    fs::path moved = old;
    moved += ".migrated";
    fs::rename(old, moved, ec);
    return (int)done.size();
}

// ---------- starting the game without the launcher ----------
// Steam's library files are simple text ("key" "value" pairs, nested in braces).
inline std::vector<std::string> vdfValues(const std::string& text, const std::string& key) {
    std::vector<std::string> tokens, out;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '"') {
            std::string v;
            i++;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.size()) { v += text[i + 1]; i += 2; }
                else v += text[i++];
            }
            i++;
            tokens.push_back(std::move(v));
        } else i++;
    }
    for (size_t k = 0; k + 1 < tokens.size(); k++) if (tokens[k] == key) out.push_back(tokens[k + 1]);
    return out;
}
inline std::vector<std::string> steamLibraryPaths(const std::string& libraryfoldersVdf) { return vdfValues(libraryfoldersVdf, "path"); }
inline std::string acfInstallDir(const std::string& appmanifestAcf) {
    auto v = vdfValues(appmanifestAcf, "installdir");
    return v.empty() || v[0].empty() ? "Crusader Kings III" : v[0];
}

// Steam keeps subscribed Workshop mods in <library>/steamapps/workshop/content/<CK3 app id>/<mod id>/ (with a descriptor.mod inside).
// The Paradox launcher turns each into mod/ugc_<id>.mod when it runs. Until it does, the mod has no descriptor in the mod folder,
// so scanMods() cannot see it. These are the downloads that are still waiting for that descriptor.
inline std::vector<ModInfo> scanWorkshopFolders(const std::vector<std::string>& contentDirs, const std::set<std::string>& knownIds) {
    std::vector<ModInfo> out;
    std::set<std::string> seen;
    std::error_code ec;
    for (auto& cd : contentDirs) {
        if (!fs::is_directory(P(cd), ec)) continue;
        for (auto& e : fs::directory_iterator(P(cd), ec)) {
            if (!e.is_directory(ec)) continue;
            std::string num = e.path().filename().u8string();
            if (num.empty() || num.size() > 20 || num.find_first_not_of("0123456789") != std::string::npos) continue;
            std::string id = "ugc_" + num + ".mod";
            if (knownIds.count(id) || !seen.insert(id).second) continue;
            std::string text;
            if (!readFile(e.path() / "descriptor.mod", text, 1u << 20)) continue;   // still downloading, or not a mod folder
            auto kv = parseDescriptor(text);
            auto first = [&](const char* k) -> std::string { auto it = kv.find(k); return it == kv.end() || it->second.empty() ? std::string() : it->second[0]; };
            ModInfo mi;
            mi.id = id; mi.source = "Workshop"; mi.pending = true;
            mi.name = first("name"); mi.version = first("version"); mi.supported = first("supported_version");
            if (kv.count("dependencies")) mi.deps = kv["dependencies"];
            if (kv.count("replace_path")) mi.replacePaths = kv["replace_path"];
            if (kv.count("tags")) mi.tags = kv["tags"];
            mi.path = mi.contentDir = e.path().u8string();
            mi.contentState = 1;
            if (mi.name.empty()) mi.name = num;
            out.push_back(std::move(mi));
        }
    }
    return out;
}
// The text of the ugc_<id>.mod the launcher would write: the mod's own descriptor.mod plus remote_file_id and path.
inline std::string registeredDescriptorText(const std::string& descriptorText, const std::string& workshopId, const std::string& folder) {
    std::string out, line;
    std::istringstream in(descriptorText);
    bool skipping = false;   // inside a multi-line value we are dropping
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trimmed(line);
        if (skipping) { if (t.find('}') != std::string::npos) skipping = false; continue; }
        if (t.rfind("path", 0) == 0 || t.rfind("remote_file_id", 0) == 0) {
            if (t.find('{') != std::string::npos && t.find('}') == std::string::npos) skipping = true;
            continue;
        }
        out += line + "\n";
    }
    std::string f = folder;
    for (char& c : f) if (c == '\\') c = '/';
    return out + "remote_file_id=\"" + workshopId + "\"\npath=\"" + f + "\"\n";
}
// Writes mod/ugc_<id>.mod for the enabled mods of the playset that only exist as Workshop downloads. Never overwrites an existing file.
inline int registerPendingMods(const std::string& dir, const Playset& ps, const std::map<std::string, ModInfo>& info, std::string* err = nullptr) {
    int n = 0;
    std::error_code ec;
    for (auto& m : ps.mods) {
        if (!m.enabled) continue;
        auto it = info.find(m.id);
        if (it == info.end() || !it->second.pending || !validID(m.id)) continue;
        fs::path target = P(dir) / "mod" / m.id;
        if (fs::exists(target, ec)) continue;
        std::string text;
        if (!readFile(P(it->second.contentDir) / "descriptor.mod", text, 1u << 20)) { if (err) *err = trf("Could not read the downloaded mod \"{0}\".", {it->second.name}); continue; }
        std::string wid = m.id.substr(4, m.id.size() - 8);
        if (writeFileAtomic(target, registeredDescriptorText(text, wid, it->second.contentDir))) n++;
        else if (err) *err = trf("Could not write {0} into your mod folder.", {m.id});
    }
    return n;
}

struct ApplyResult { bool ok = false; std::string message; int written = 0, skipped = 0; };

// The game (when started directly, without the launcher) reads which mods to load from dlc_load.json in the CK3 user folder.
// Writes the playset's enabled, installed mods there in load order, keeping any other settings in the file.
// The user's own file is kept once as dlc_load.json.rc-original before it is first changed.
inline ApplyResult writeGameModList(const std::string& dir, const Playset& ps, const std::set<std::string>& installed) {
    ApplyResult r;
    if (dir.empty()) { r.message = tr("The CK3 folder is not set."); return r; }
    fs::path path = P(dir) / "dlc_load.json";
    J doc = J::obj();
    std::string old;
    std::error_code ec0;
    if (fs::exists(path, ec0)) {
        if (!readFile(path, old)) { r.message = tr("Could not read your existing dlc_load.json (it may be in use or unusually large), so it was left unchanged."); return r; }
        J parsed;
        if (parseJson(old, parsed) && parsed.t == J::Obj) doc = std::move(parsed);
        fs::path keep = P(dir) / "dlc_load.json.rc-original";
        std::error_code ec;
        if (!fs::exists(keep, ec) && !writeFile(keep, old)) { r.message = tr("Could not save a copy of your dlc_load.json, so nothing was changed."); return r; }
    }
    if (!doc.get("disabled_dlcs")) doc.set("disabled_dlcs", J::arr());
    J list = J::arr();
    for (auto& m : ps.mods) {
        if (!m.enabled) continue;
        if (!validID(m.id) || !installed.count(m.id)) { r.skipped++; continue; }
        list.a.push_back(J::str("mod/" + m.id));
        r.written++;
    }
    doc.set("enabled_mods", std::move(list));
    std::string out; dump(doc, out);
    if (!writeFileAtomic(path, out)) { r.message = tr("Could not write dlc_load.json (is the CK3 folder writable?)."); return r; }
    r.ok = true;
    r.message = r.skipped ? trf("Loaded \"{0}\": {1} mods ({2} not installed, skipped)", {ps.name, r.written, r.skipped})
                          : trf("Loaded \"{0}\": {1} mods", {ps.name, r.written});
    return r;
}

// ---------- diagnostics log ----------
// A small text file next to settings.json (royalcourt.log, rotated at 256 KB). Written from the main thread only.
inline fs::path logPath() { return P(dataDir()) / "royalcourt.log"; }
inline void logLine(const std::string& msg) {
    try {
        fs::path p = logPath();
        std::error_code ec;
        if (fs::exists(p, ec) && fs::file_size(p, ec) > 256u * 1024u) { fs::path old = p; old += ".1"; fs::remove(old, ec); fs::rename(p, old, ec); }
        std::time_t t = std::time(nullptr);
        char buf[32] = "";
        if (const std::tm* tm = std::localtime(&t)) std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", tm);
        std::ofstream f(p, std::ios::app | std::ios::binary);
        f << buf << "  " << msg << "\n";
    } catch (...) {}
}

// ---------- SHA-256 (used to verify a downloaded update) ----------
inline std::string sha256Hex(const std::string& data) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    std::string m = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    m += (char)0x80;
    while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xFF);
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = ((uint32_t)(uint8_t)m[off + i * 4] << 24) | ((uint32_t)(uint8_t)m[off + i * 4 + 1] << 16) | ((uint32_t)(uint8_t)m[off + i * 4 + 2] << 8) | (uint32_t)(uint8_t)m[off + i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    char out[65];
    for (int i = 0; i < 8; i++) std::snprintf(out + i * 8, 9, "%08x", h[i]);
    return std::string(out, 64);
}

// ---------- update check (GitHub Releases) ----------
inline const char* UPDATE_REPO = "Juincy/the-royal-court";
inline const char* UPDATE_EXE_ASSET = "TheRoyalCourt.exe";
inline const char* UPDATE_SUMS_ASSET = "SHA256SUMS.txt";

// "v0.14.1" / "0.14" -> {0,14,1}; anything after the first non-numeric part is ignored
inline std::vector<int> parseVersion(const std::string& s) {
    std::vector<int> v;
    size_t i = 0;
    while (i < s.size() && (s[i] == 'v' || s[i] == 'V' || s[i] == ' ')) i++;
    int cur = 0; bool any = false;
    for (; i < s.size(); i++) {
        if (s[i] >= '0' && s[i] <= '9') { cur = std::min(cur * 10 + (s[i] - '0'), 1000000); any = true; }
        else if (s[i] == '.' && any) { v.push_back(cur); cur = 0; any = false; }
        else break;
    }
    if (any) v.push_back(cur);
    return v;
}
// <0 older, 0 same, >0 newer (a compared with b)
inline int compareVersions(const std::string& a, const std::string& b) {
    std::vector<int> x = parseVersion(a), y = parseVersion(b);
    size_t n = std::max(x.size(), y.size());
    for (size_t i = 0; i < n; i++) {
        int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}
struct ReleaseAsset { std::string name, url; long long size = 0; };
struct ReleaseInfo {
    bool ok = false;
    std::string tag, name, body, pageUrl;
    std::vector<ReleaseAsset> assets;
    const ReleaseAsset* asset(const std::string& n) const { for (auto& a : assets) if (a.name == n) return &a; return nullptr; }
};
// Only downloads from this project's own GitHub releases are ever followed.
inline bool trustedReleaseUrl(const std::string& url) {
    std::string pre = lower(std::string("https://github.com/") + UPDATE_REPO + "/releases/");
    return lower(url).compare(0, pre.size(), pre) == 0;
}
inline ReleaseInfo parseRelease(const std::string& json) {
    ReleaseInfo r;
    J root;
    if (!parseJson(json, root) || root.t != J::Obj) return r;
    auto str = [&](J& o, const char* k, std::string& out) { if (J* v = o.get(k); v && v->t == J::Str) out = v->s; };
    str(root, "tag_name", r.tag); str(root, "name", r.name); str(root, "body", r.body); str(root, "html_url", r.pageUrl);
    if (r.tag.empty() || parseVersion(r.tag).empty()) return r;
    if (!r.pageUrl.empty() && !trustedReleaseUrl(r.pageUrl)) r.pageUrl.clear();
    if (J* as = root.get("assets"); as && as->t == J::Arr)
        for (auto& a : as->a) {
            if (a.t != J::Obj) continue;
            ReleaseAsset x;
            str(a, "name", x.name); str(a, "browser_download_url", x.url);
            if (J* sz = a.get("size"); sz && sz->t == J::Num) x.size = (long long)sz->n;
            if (!x.name.empty() && trustedReleaseUrl(x.url)) r.assets.push_back(x);
        }
    r.ok = true;
    return r;
}
// SHA256SUMS.txt lines look like "<64 hex>  name" (sha256sum) or "<64 hex> *name"; returns "" when not listed
inline std::string findSumFor(const std::string& sums, const std::string& file) {
    std::istringstream in(sums);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 66) continue;
        std::string hash = lower(line.substr(0, 64));
        if (hash.find_first_not_of("0123456789abcdef") != std::string::npos) continue;
        size_t p = 64;
        while (p < line.size() && (line[p] == ' ' || line[p] == '*' || line[p] == '\t')) p++;
        if (line.substr(p) == file) return hash;
    }
    return "";
}


// ---- "Report a sort problem" ----
// Plain text a user can paste into a GitHub issue: the current load order with each mod's type and what the known-mods list says about it.
// Only mod names, Workshop ids and flags are included (no file paths, no user names).
inline std::string buildSortReport(const Playset& ps, const std::map<std::string, ModInfo>& info, const std::vector<KnownMod>& db,
                                   const std::vector<int>& cats, const std::set<std::string>& locked, const std::string& gameVer, int onlineRevision) {
    std::ostringstream o;
    o << "The Royal Court sort report\n";
    o << "Program " << VERSION << ", Crusader Kings III " << (gameVer.empty() ? "unknown" : gameVer)
      << ", known-mods list: built-in" << (onlineRevision > 0 ? " + online revision " + std::to_string(onlineRevision) : std::string()) << "\n";
    o << "Playset: " << ps.name << " (" << ps.mods.size() << " mods)\n\n";
    o << "Load order, first line loads first:\n";
    size_t n = 0;
    for (size_t i = 0; i < ps.mods.size() && n < 300; i++, n++) {
        const ModRef& m = ps.mods[i];
        auto it = info.find(m.id);
        std::string name = it != info.end() && !it->second.name.empty() ? it->second.name : (m.name.empty() ? m.id : m.name);
        o << (i + 1) << ". " << (m.enabled ? "[on] " : "[off] ") << name;
        std::string sid = steamIdOf(m.id);
        if (!sid.empty()) o << " (Steam " << sid << ")";
        if (i < cats.size()) o << " | type " << catNameEn(cats[i]);
        if (locked.count(m.id)) o << " | locked";
        if (it != info.end()) { int k = findKnown(db, it->second); if (k >= 0) o << " | known mod"; }
        if (it == info.end()) o << " | not installed";
        o << "\n";
    }
    if (ps.mods.size() > n) o << "... and " << (ps.mods.size() - n) << " more\n";
    o << "\nWhat I expected (which mod should be where, and why):\n\n";
    return o.str();
}

}  // namespace rc
