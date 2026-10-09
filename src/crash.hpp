// The Royal Court - the crash helper.
//
// When Crusader Kings III crashes it writes a folder  <CK3 folder>/crashes/ck3_YYYYMMDD_HHMMSS  holding (names as far as known)
//   exception.txt   what Windows reported (an exception code and often a few call-stack lines)
//   meta.yml        key: value lines about the build
//   logs/           a copy of the game's logs at that moment (game.log, error.log, debug.log), or the files at the top of the folder
//   a minidump      binary, never read
// Nothing here is guaranteed, so every file is optional and every reader tolerates garbage.
// The program cannot name the mod that crashed the game from a minidump. It can say what kind of crash it was, which mods the
// last things the game logged belong to, whether the playset changed since, and what usually helps. It says "likely", never "is".
#pragma once
#include "gamelog.hpp"

namespace rc {

enum CrashKind : int { CK_UNKNOWN = 0, CK_ACCESS, CK_MEMORY, CK_STACK, CK_GRAPHICS, CK_ASSERT, CK_ILLEGAL, CK_DIVIDE };

struct CrashFolder { std::string name; std::string path; int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0; };

// "ck3_20261009_172556" -> fields; false when the name does not have that shape
inline bool crashNameParse(const std::string& name, CrashFolder& f) {
    if (name.size() < 19 || name.compare(0, 4, "ck3_") != 0 || name[12] != '_') return false;
    for (size_t i = 4; i < 19; i++) if (i != 12 && !isdigit((unsigned char)name[i])) return false;
    auto num = [&](size_t a, size_t n) { return std::stoi(name.substr(a, n)); };
    f.name = name;
    f.y = num(4, 4); f.mo = num(8, 2); f.d = num(10, 2); f.h = num(13, 2); f.mi = num(15, 2); f.s = num(17, 2);
    return f.mo >= 1 && f.mo <= 12 && f.d >= 1 && f.d <= 31 && f.h < 24 && f.mi < 60 && f.s < 61;
}

// Crash folders, newest first. Folders with other names are listed after the dated ones, by name.
inline std::vector<CrashFolder> listCrashes(const std::string& crashesDir, size_t maxN = 200) {
    std::vector<CrashFolder> v;
    std::error_code ec;
    if (crashesDir.empty() || !fs::is_directory(P(crashesDir), ec)) return v;
    for (fs::directory_iterator it(P(crashesDir), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end && v.size() < maxN * 4; it.increment(ec)) {
        std::error_code e2;
        if (!it->is_directory(e2)) continue;
        CrashFolder f;
        std::string n = it->path().filename().u8string();
        if (!crashNameParse(n, f)) { f = CrashFolder(); f.name = n; }
        f.path = it->path().u8string();
        v.push_back(f);
    }
    std::sort(v.begin(), v.end(), [](const CrashFolder& a, const CrashFolder& b) {
        bool da = a.y > 0, db = b.y > 0;
        if (da != db) return da;
        return a.name > b.name;   // the dated names sort by time as text
    });
    if (v.size() > maxN) v.resize(maxN);
    return v;
}

struct CrashData {
    CrashKind kind = CK_UNKNOWN;
    std::string code;                    // "EXCEPTION_ACCESS_VIOLATION" or the first line of the exception text
    std::vector<std::string> frames;     // up to 8 call-stack lines that name a module or function
    std::vector<std::string> meta;       // a few interesting key: value lines of meta.yml
    bool haveException = false, haveMeta = false;
    std::string gameLog;                 // tail of game.log / error.log from the crash folder ("" if none)
    std::string debugLog;                // debug.log from the crash folder ("" if none)
    bool logsFromCrash = false;          // the logs came from the crash folder (otherwise the caller may fall back)
    std::vector<std::string> files;      // names of the files found in the folder (for the report)
};

inline std::string crashLower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

inline CrashKind crashKindOf(const std::string& text) {
    std::string t = crashLower(text);
    auto has = [&](const char* k) { return t.find(k) != std::string::npos; };
    if (has("out_of_memory") || has("out of memory") || has("bad_alloc") || has("std::bad_alloc") || has("memory allocation") || has("not enough memory")) return CK_MEMORY;
    if (has("stack_overflow") || has("stack overflow")) return CK_STACK;
    if (has("device_removed") || has("device removed") || has("device_hung") || has("d3d") || has("dxgi") || has("vulkan") || has("nvwgf") || has("atidxx") || has("nvoglv")) return CK_GRAPHICS;
    if (has("illegal_instruction") || has("privileged_instruction")) return CK_ILLEGAL;
    if (has("divide_by_zero") || has("int_divide")) return CK_DIVIDE;
    if (has("assert")) return CK_ASSERT;
    if (has("access_violation") || has("in_page_error") || has("array_bounds")) return CK_ACCESS;
    return CK_UNKNOWN;
}

// exception.txt: find the exception code and the first call-stack lines.
inline void crashParseException(const std::string& text, CrashData& out) {
    if (text.empty()) return;
    out.haveException = true;
    out.kind = crashKindOf(text);
    size_t pos = 0, lines = 0;
    while (pos < text.size() && lines < 400) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1; lines++;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t a = 0; while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) a++;
        line.erase(0, a);
        if (line.empty() || line.size() > 300) continue;
        if (out.code.empty()) {
            size_t k = line.find("EXCEPTION_");
            if (k != std::string::npos) {
                size_t e = k; while (e < line.size() && (isupper((unsigned char)line[e]) || line[e] == '_' || isdigit((unsigned char)line[e]))) e++;
                out.code = line.substr(k, e - k);
            }
        }
        std::string low = crashLower(line);
        bool frame = low.find(".dll") != std::string::npos || low.find(".exe") != std::string::npos || low.find("::") != std::string::npos || low.find("0x") != std::string::npos;
        if (frame && out.frames.size() < 8 && line.find("EXCEPTION_") == std::string::npos) out.frames.push_back(line);
    }
    if (out.code.empty()) {   // no code word: the first non-empty line says something
        size_t e = text.find('\n');
        std::string first = text.substr(0, e == std::string::npos ? text.size() : e);
        while (!first.empty() && (first.back() == '\r' || first.back() == ' ')) first.pop_back();
        if (first.size() > 160) first.resize(160);
        out.code = first;
    }
}

// meta.yml: keep lines that tell the build (version, build, checksum, platform).
inline void crashParseMeta(const std::string& text, CrashData& out) {
    if (text.empty()) return;
    out.haveMeta = true;
    size_t pos = 0, lines = 0;
    while (pos < text.size() && lines < 300 && out.meta.size() < 8) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1; lines++;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t a = 0; while (a < line.size() && line[a] == ' ') a++;
        line.erase(0, a);
        size_t c = line.find(':');
        if (c == std::string::npos || c == 0 || line.size() > 200) continue;
        std::string key = crashLower(line.substr(0, c));
        if (key.find("version") != std::string::npos || key.find("build") != std::string::npos || key.find("checksum") != std::string::npos
            || key == "platform" || key == "os" || key == "gpu" || key == "cpu") out.meta.push_back(line);
    }
}

// Reads what is in a crash folder (and one level of subfolders such as logs/). maxLog bounds each log file read.
inline CrashData crashRead(const std::string& folder, size_t maxLog = 4u << 20) {
    CrashData d;
    std::error_code ec;
    fs::path root = P(folder);
    if (!fs::is_directory(root, ec)) return d;
    std::map<std::string, fs::path> found;   // lower-case file name -> path (the shallowest wins)
    auto scan = [&](const fs::path& dir, int depth, auto&& self) -> void {
        std::error_code e;
        size_t n = 0;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, e), end; !e && it != end && n < 500; it.increment(e), n++) {
            std::error_code e2;
            std::string nm = it->path().filename().u8string();
            if (it->is_directory(e2)) { if (depth < 2) self(it->path(), depth + 1, self); continue; }
            std::string lo = crashLower(nm);
            if (!found.count(lo)) found[lo] = it->path();
            if (depth == 0 || nm.size() < 60) { if (d.files.size() < 40) d.files.push_back(nm); }
        }
    };
    scan(root, 0, scan);
    auto readFile = [&](const char* name, bool tail) -> std::string {
        auto it = found.find(name);
        if (it == found.end()) return "";
        std::ifstream f(it->second, std::ios::binary);
        if (!f) return "";
        f.seekg(0, std::ios::end);
        std::streamoff sz = f.tellg();
        if (sz <= 0) return "";
        std::streamoff start = (tail && sz > (std::streamoff)maxLog) ? sz - (std::streamoff)maxLog : 0;
        size_t len = (size_t)std::min<std::streamoff>(sz - start, (std::streamoff)maxLog);
        f.seekg(start);
        std::string s(len, '\0');
        f.read(&s[0], (std::streamsize)len);
        s.resize((size_t)f.gcount());
        if (tail && start) { size_t nl = s.find('\n'); if (nl != std::string::npos) s.erase(0, nl + 1); }
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
        return s;
    };
    crashParseException(readFile("exception.txt", false), d);
    crashParseMeta(readFile("meta.yml", false), d);
    d.gameLog = readFile("game.log", true);
    std::string old = readFile("error.log", true);
    if (!old.empty()) { if (!d.gameLog.empty()) d.gameLog += "\n"; d.gameLog += old; }
    d.debugLog = readFile("debug.log", false);
    d.logsFromCrash = !d.gameLog.empty() || !d.debugLog.empty();
    if (d.kind == CK_UNKNOWN && !d.haveException) {
        // no exception.txt: a few log lines sometimes say "out of memory" and the like
        std::string tailTxt = d.gameLog.size() > 20000 ? d.gameLog.substr(d.gameLog.size() - 20000) : d.gameLog;
        CrashKind k = crashKindOf(tailTxt);
        if (k == CK_MEMORY || k == CK_STACK) d.kind = k;
    }
    return d;
}

// The last `maxLines` lines of a log as plain text.
inline std::string crashTailLines(const std::string& text, size_t maxLines) {
    size_t pos = text.size(), n = 0;
    while (pos > 0 && n <= maxLines) {
        size_t p = text.rfind('\n', pos - 1);
        if (p == std::string::npos) { pos = 0; break; }
        pos = p; n++;
        if (pos == 0) break;
    }
    return text.substr(pos == 0 ? 0 : pos + 1);
}

// What the game was doing at the end: parse the last lines of the log. Messages that name a file or a name become entries;
// the caller attributes them to mods with attributeLog() (they come out in log order, so later = closer to the crash).
inline LogParse crashLastMessages(const std::string& gameLog, size_t tailBytes = 24000) {
    std::string t = gameLog.size() > tailBytes ? gameLog.substr(gameLog.size() - tailBytes) : gameLog;
    if (t.size() < gameLog.size()) { size_t nl = t.find('\n'); if (nl != std::string::npos) t.erase(0, nl + 1); }
    return parseGameLog(t, true, 400);
}

// How many of the entries from the end (the last `window`) point at each mod, newest weighing more. Returns mod index -> score.
inline std::map<int, double> crashScores(const LogReport& r, size_t window = 60) {
    std::map<int, double> sc;
    size_t n = r.parse.entries.size();
    size_t from = n > window ? n - window : 0;
    for (size_t k = from; k < n; k++) {
        if (r.modOf[k] < 0) continue;
        const LogEntry& e = r.parse.entries[k];
        double recency = 1.0 + 2.0 * (double)(k - from) / (double)std::max<size_t>(1, n - from);   // 1 .. 3
        sc[r.modOf[k]] += (e.level == 'E' ? 2.0 : 0.5) * recency;
    }
    return sc;
}

// Plain-language kinds: a short name and a short text of what usually helps (English; main.cpp translates through K()).
inline const char* crashKindName(CrashKind k) {
    switch (k) {
        case CK_ACCESS: return K("The game touched memory it should not (access violation)");
        case CK_MEMORY: return K("The game ran out of memory");
        case CK_STACK: return K("The game's call stack overflowed");
        case CK_GRAPHICS: return K("A graphics driver or graphics-card error");
        case CK_ASSERT: return K("The game stopped itself on an internal check (assertion)");
        case CK_ILLEGAL: return K("A processor instruction the computer could not run");
        case CK_DIVIDE: return K("The game divided by zero");
        default: return K("A crash with no recognised cause");
    }
}
inline const char* crashKindAdvice(CrashKind k) {
    switch (k) {
        case CK_ACCESS: return K("This is the most common crash with mods. It is usually a script or data file the game cannot handle: a mod that is out of date for this game version, two mods that overwrite the same files, or a mod loaded in the wrong order. Check the suspects below, then try turning off the most recently added or updated mods first.");
        case CK_MEMORY: return K("A big playset on a long game can use more memory than the computer has. Close other programs, turn off graphics-heavy and map mods first, and restart the game now and then in a long campaign.");
        case CK_STACK: return K("This is almost always a mod that makes events or effects call each other forever. Look at the suspects below, and at mods that changed recently.");
        case CK_GRAPHICS: return K("This points at the graphics driver or card rather than a script mod. Update the graphics driver, and turn off mods that add many or large textures, 3D models or shaders.");
        case CK_ASSERT: return K("The game found data that does not make sense. A mod that is out of date for this game version is the usual reason.");
        case CK_ILLEGAL: return K("This usually means damaged game files or an unstable overclock. Verify the game files in Steam.");
        case CK_DIVIDE: return K("A mod gave the game a value of zero where it must not. Check the suspects below.");
        default: return K("The crash files do not say what went wrong. The suspects below come from what the game wrote to its log just before it stopped.");
    }
}

}  // namespace rc
