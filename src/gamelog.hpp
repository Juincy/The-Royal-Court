// The Royal Court - reading the game's own log and tying each message to the mod that caused it.
//
// The game writes its script problems to <CK3 folder>/logs/game.log (older builds: error.log). A message looks like
//   [08:41:13][E][modifier_instance.cpp:390]: Unknown modifier type 'x' at file: common/traits/zz_gptev_traits.txt line: 776 (gpt_crow)
// or spreads over several lines:
//   [08:41:13][E][jomini_script_system.cpp:304]: Script system error!
//     Error: stress_impact effect [ Cannot find scholar in trait database ]
//     Script location: file: events/VIET_events_travel.txt line: 4129 (VIETmisc.7039:option)
// The message names a file path (relative to the game's root), never a mod. So the path is looked up in the mods' file lists:
// the mod that loads last and ships that path is the one whose copy the game used. If no path is named, quoted names
// ('VIETmisc.1084') are looked up among the definitions the mods make.
#pragma once
#include "core.hpp"

namespace rc {

struct LogEntry {
    char level = 'E';              // 'E' error, 'W' warning
    std::string src;               // "jomini_script_system.cpp:304"
    std::string msg;               // text after the header
    std::string detail;            // the indented lines under the header, joined with " | "
    std::string file;              // path named in the message, lower case with '/' ("" if none)
    int line = 0;
    std::vector<std::string> names;   // quoted names and the script name in "( )", used when no file is named
    int count = 1;
};

struct LogParse {
    std::vector<LogEntry> entries;   // identical messages are merged (count), first seen first
    size_t lines = 0, errors = 0, warnings = 0, dropped = 0;   // dropped: distinct messages beyond the cap
};

inline bool logHeaderParse(const std::string& s, char& lvl, std::string& src, std::string& msg) {
    if (s.size() < 12 || s[0] != '[') return false;
    size_t p = s.find(']');
    if (p == std::string::npos || p < 2 || p > 12) return false;
    for (size_t i = 1; i < p; i++) if (!(s[i] >= '0' && s[i] <= '9') && s[i] != ':') return false;
    if (p + 4 >= s.size() || s[p + 1] != '[' || s[p + 3] != ']' || s[p + 4] != '[') return false;
    lvl = s[p + 2];
    size_t q = s.find(']', p + 5);
    if (q == std::string::npos) return false;
    src = s.substr(p + 5, q - p - 5);
    msg = s.substr(q + 1);
    if (msg.compare(0, 2, ": ") == 0) msg.erase(0, 2); else if (!msg.empty() && msg[0] == ':') msg.erase(0, 1);
    return true;
}

// "... file: common/traits/x.txt line: 776 (gpt_crow)" -> file, line, and the name in the last "( )"
inline void logLocate(const std::string& text, LogEntry& e) {
    size_t f = text.find("file: ");
    if (f != std::string::npos) {
        size_t b = f + 6;
        size_t l = text.find(" line: ", b);
        size_t end = l != std::string::npos ? l : text.find_first_of(" '\")(", b);
        if (end == std::string::npos) end = text.size();
        if (end > b && end - b < 400) {
            std::string path = text.substr(b, end - b);
            for (auto& c : path) { if (c == '\\') c = '/'; }
            e.file = lower(path);
            if (l != std::string::npos) {
                size_t d = l + 7; int n = 0; int digits = 0;
                while (d < text.size() && text[d] >= '0' && text[d] <= '9' && digits < 9) { n = n * 10 + (text[d] - '0'); d++; digits++; }
                e.line = n;
            }
        }
    }
}
inline void logNames(const std::string& text, std::vector<std::string>& out) {
    size_t i = 0;
    while (i < text.size() && out.size() < 8) {   // 'quoted names'
        size_t a = text.find('\'', i);
        if (a == std::string::npos) break;
        size_t b = text.find('\'', a + 1);
        if (b == std::string::npos) break;
        std::string n = text.substr(a + 1, b - a - 1);
        if (n.size() >= 3 && n.size() <= 80 && n.find(' ') == std::string::npos) out.push_back(n);
        i = b + 1;
    }
    size_t o = text.rfind(" (");   // the script name at the end: "(VIETmisc.7039:option)"
    if (o != std::string::npos && text.back() == ')' ) {
        std::string n = text.substr(o + 2, text.size() - o - 3);
        size_t c = n.find(':');
        if (c != std::string::npos) n.resize(c);
        if (n.size() >= 3 && n.size() <= 80 && n.find(' ') == std::string::npos) out.push_back(n);
    }
}

inline LogParse parseGameLog(const std::string& text, bool withWarnings = true, size_t maxEntries = 20000) {
    LogParse r;
    std::unordered_map<std::string, size_t> seen;
    LogEntry cur; bool have = false;
    auto flush = [&]() {
        if (!have) return;
        have = false;
        if (cur.level == 'E') r.errors++; else r.warnings++;
        std::string whole = cur.msg + (cur.detail.empty() ? "" : " | " + cur.detail);
        logLocate(whole, cur);
        logNames(cur.msg, cur.names);
        if (!cur.detail.empty()) logNames(cur.detail, cur.names);
        std::string key; key.reserve(whole.size() + cur.src.size() + 4);
        key += cur.level; key += '|'; key += cur.src; key += '|'; key += whole;
        auto it = seen.find(key);
        if (it != seen.end()) { r.entries[it->second].count++; return; }
        if (r.entries.size() >= maxEntries) { r.dropped++; return; }
        seen.emplace(std::move(key), r.entries.size());
        r.entries.push_back(std::move(cur));
    };
    size_t pos = 0, n = text.size();
    while (pos < n) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = n;
        size_t end = nl;
        if (end > pos && text[end - 1] == '\r') end--;
        std::string line = text.substr(pos, end - pos);
        pos = nl + 1;
        r.lines++;
        char lvl; std::string src, msg;
        if (logHeaderParse(line, lvl, src, msg)) {
            flush();
            if (lvl == 'E' || (withWarnings && lvl == 'W')) {
                cur = LogEntry(); cur.level = lvl; cur.src = src; cur.msg = msg; have = true;
            }
        } else if (have && !line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            size_t a = line.find_first_not_of(" \t");
            if (a != std::string::npos && cur.detail.size() < 1500) { if (!cur.detail.empty()) cur.detail += " | "; cur.detail += line.substr(a); }
        } else flush();
    }
    flush();
    return r;
}

// The mods the game says it loaded: debug.log has one "Name|mod/ugc_123.mod|Enabled" line per mod.
struct LoadedMod { std::string name, id; bool enabled = true; };
inline std::vector<LoadedMod> parseLoadedMods(const std::string& text) {
    std::vector<LoadedMod> v;
    std::set<std::string> seen;
    size_t pos = 0;
    while (pos < text.size() && v.size() < 5000) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t a = line.find("|mod/");
        if (a == std::string::npos || a == 0 || line.size() > 600) continue;
        size_t b = line.find('|', a + 5);
        if (b == std::string::npos) continue;
        std::string id = line.substr(a + 5, b - a - 5), state = line.substr(b + 1);
        if (!validID(id) || (state != "Enabled" && state != "Disabled")) continue;
        if (!seen.insert(id).second) continue;
        v.push_back({line.substr(0, a), id, state == "Enabled"});
    }
    return v;
}

enum LogHow : int { LH_NONE = 0, LH_FILE = 1, LH_NAME = 2, LH_BASE = 3 };
struct LogReport {
    LogParse parse;
    std::vector<int> modOf;      // per entry: index into the playset's mods, -1 if none
    std::vector<int> how;        // per entry: LogHow
    struct PerMod { int mod = -1; size_t errors = 0, warnings = 0, entries = 0; };
    std::vector<PerMod> perMod;  // mods with at least one message, most errors first
    PerMod base, unplaced;       // messages from files only the base game has / that could not be placed
};

inline LogReport attributeLog(LogParse&& parse, const Playset& ps, const std::map<std::string, ModFiles>& files,
                              const std::map<std::string, ModDefs>& defs, const VanillaIndex* van) {
    LogReport r;
    r.parse = std::move(parse);
    auto& es = r.parse.entries;
    r.modOf.assign(es.size(), -1);
    r.how.assign(es.size(), LH_NONE);
    std::unordered_set<std::string> wantFiles;
    for (auto& e : es) if (!e.file.empty()) wantFiles.insert(e.file);
    std::unordered_map<std::string, int> fileOwner;      // path -> index of the last enabled mod that ships it
    for (size_t i = 0; i < ps.mods.size() && !wantFiles.empty(); i++) {
        if (!ps.mods[i].enabled) continue;
        auto f = files.find(ps.mods[i].id);
        if (f == files.end()) continue;
        for (auto& rel : f->second.files) if (wantFiles.count(rel)) fileOwner[rel] = (int)i;
    }
    std::unordered_set<std::string> wantNames;
    for (auto& e : es) {
        bool placed = !e.file.empty() && (fileOwner.count(e.file) || (van && van->has(e.file)));
        if (!placed) for (auto& n : e.names) wantNames.insert(n);
    }
    std::unordered_map<std::string, int> nameOwner;
    for (size_t i = 0; i < ps.mods.size() && !wantNames.empty(); i++) {
        if (!ps.mods[i].enabled) continue;
        auto d = defs.find(ps.mods[i].id);
        if (d == defs.end()) continue;
        for (auto& de : d->second.defs) {
            std::string nm = d->second.nameOf(de);
            if (wantNames.count(nm)) nameOwner[nm] = (int)i;
        }
    }
    std::map<int, LogReport::PerMod> pm;
    auto add = [&](LogReport::PerMod& m, const LogEntry& e) {
        (e.level == 'E' ? m.errors : m.warnings) += (size_t)e.count;
        m.entries++;
    };
    for (size_t k = 0; k < es.size(); k++) {
        const LogEntry& e = es[k];
        if (!e.file.empty()) {
            if (auto it = fileOwner.find(e.file); it != fileOwner.end()) { r.modOf[k] = it->second; r.how[k] = LH_FILE; }
            else if (van && van->has(e.file)) r.how[k] = LH_BASE;
        }
        if (r.how[k] == LH_NONE) {
            for (auto& n : e.names) if (auto it = nameOwner.find(n); it != nameOwner.end()) { r.modOf[k] = it->second; r.how[k] = LH_NAME; break; }
        }
        if (r.modOf[k] >= 0) { auto& m = pm[r.modOf[k]]; m.mod = r.modOf[k]; add(m, e); }
        else if (r.how[k] == LH_BASE) add(r.base, e);
        else add(r.unplaced, e);
    }
    for (auto& kv : pm) r.perMod.push_back(kv.second);
    std::stable_sort(r.perMod.begin(), r.perMod.end(), [](const LogReport::PerMod& a, const LogReport::PerMod& b) {
        return a.errors != b.errors ? a.errors > b.errors : a.warnings > b.warnings; });
    return r;
}

// A short, shareable text for a mod author: the messages that point at one mod.
inline std::string logModReport(const LogReport& r, int mod, const std::string& modName, size_t maxLines = 60) {
    std::string out = "Messages from the game log that point at \"" + modName + "\":\n";
    size_t shown = 0, total = 0;
    for (size_t k = 0; k < r.parse.entries.size(); k++) {
        if (r.modOf[k] != mod) continue;
        total++;
        if (shown >= maxLines) continue;
        const LogEntry& e = r.parse.entries[k];
        out += std::string(e.level == 'E' ? "[error] " : "[warning] ") + e.msg;
        if (!e.detail.empty()) out += " | " + e.detail;
        if (e.count > 1) out += "  (x" + std::to_string(e.count) + ")";
        out += "\n";
        shown++;
    }
    if (total > shown) out += "... and " + std::to_string(total - shown) + " more\n";
    return out;
}

}  // namespace rc
