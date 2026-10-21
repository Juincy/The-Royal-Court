// The Royal Court - which mod's scripts use the most time.
//
// While the game runs it keeps its own stopwatch for every event and every on_action and writes the totals to
// <CK3 folder>/logs/eventstats.csv and onactionstats.csv (";"-separated, one row per event / on_action, a "#ID;..." header). The columns
// "Sum" hold the time spent, "ExecuteCount" how many times it ran. An on_action's Sum also contains the events and on_actions it started,
// and those have rows of their own, so its OWN time is Sum - ExecuteEventsTotal - ExecuteOnActionsTotal (the rest would be counted twice).
// custom_automated_stats.log holds yearly counts ("[AUTOMATION_STATS][living characters][Year][.]:[22520][1276]") that show why a long
// game slows down.
//
// An event id ("namespace.number") is defined in exactly one mod, or in the base game: the mod that loads last and defines it is credited.
// An on_action name can be defined by many mods at once and also by the base game, so one is credited to a mod only when exactly one
// mod defines it AND its first word (POD_...) is also the first word of that mod's event namespaces. Everything else stays under
// "On_actions", never blamed on a guess.
#pragma once
#include <cmath>
#include <cstdlib>
#include "core.hpp"

namespace rc {

struct TimeRow { std::string id; double sum = 0, own = 0, share = 0; long long count = 0; };
struct TimeTable { std::vector<TimeRow> rows; double total = 0; bool ok = false; };

inline std::vector<std::string> splitSemicolons(const std::string& s) {
    std::vector<std::string> v;
    size_t p = 0;
    while (p <= s.size()) {
        size_t q = s.find(';', p);
        if (q == std::string::npos) q = s.size();
        v.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    return v;
}
inline double toDouble(const std::string& s) {
    if (s.empty()) return 0;
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    return (end == s.c_str() || !(d == d) || d < 0) ? 0 : d;
}

// events: time = Sum. on_actions: time = own part. share = time / sum of all times in the table.
inline TimeTable parseStatsCsv(const std::string& text, bool onAction) {
    TimeTable t;
    std::map<std::string, size_t> col;
    bool header = false;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(p, e - p);
        p = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> c = splitSemicolons(line);
        if (!header) {
            if (line[0] != '#') continue;
            c[0].erase(0, 1);
            for (size_t i = 0; i < c.size(); i++) col[c[i]] = i;
            if (!col.count("ID") || !col.count("Sum")) return t;
            header = true;
            continue;
        }
        auto get = [&](const char* name) { auto it = col.find(name); return it != col.end() && it->second < c.size() ? toDouble(c[it->second]) : 0.0; };
        if (c.empty() || c[0].empty() || c.size() + 1 < col.size()) continue;      // a damaged or cut-off row
        TimeRow r;
        r.id = c[0];
        r.sum = get("Sum");
        r.count = (long long)std::max({get("ExecuteCount"), get("IsValidToTriggerCount"), get("ImmediateEffectCount")});   // events that only run their trigger/effect leave ExecuteCount at 0
        if (onAction) {
            r.own = r.sum - get("ExecuteEventsTotal") - get("ExecuteOnActionsTotal");
            if (r.own < 0) r.own = 0;
        } else r.own = r.sum;
        t.total += r.own;
        t.rows.push_back(std::move(r));
    }
    t.ok = header;
    for (auto& r : t.rows) r.share = t.total > 0 ? r.own / t.total : 0;
    std::stable_sort(t.rows.begin(), t.rows.end(), [](const TimeRow& a, const TimeRow& b) { return a.own > b.own; });
    return t;
}

struct TimeMod { int mod = -1; double time = 0; long long runs = 0; size_t events = 0; std::string slowest; double slowestTime = 0; };
struct TimeReport {
    TimeTable events, onActions;
    std::vector<int> eventMod;                   // per events row: index into the playset's mods, -1 = base game / not found in your mods
    std::vector<int> actionMod;                  // per onActions row: index into the playset's mods, -1 = not credited to one mod
    std::vector<std::vector<int>> actionDefiners;  // per onActions row: the mods that define it (playset indexes)
    std::vector<TimeMod> perMod;                 // most time first
    double baseEvents = 0, sharedActions = 0, all = 0;   // base-game events, on_actions not credited to a mod, and everything
    long long baseRuns = 0, sharedRuns = 0;
    int actionsByName = 0;                       // on_actions credited because only one mod defines them and the base game does not
};

inline std::string firstWord(const std::string& s) {
    size_t u = s.find('_');
    std::string w = s.substr(0, u == std::string::npos ? s.size() : u);
    size_t d = w.find('.');
    if (d != std::string::npos) w.erase(d);
    for (auto& ch : w) ch = (char)tolower((unsigned char)ch);
    return w;
}

// The names of the base game's on_actions (game/common/on_action and the same folder in every DLC). An on_action that only ONE of your mods
// defines and that the base game does not have belongs to that mod without any guessing.
inline std::unordered_set<std::string> baseOnActionNames(const std::string& gameDir) {
    std::unordered_set<std::string> names;
    if (gameDir.empty()) return names;
    std::error_code ec;
    std::vector<fs::path> roots{P(gameDir) / "common" / "on_action"};
    for (auto& dd : vanillaDlcDirs(gameDir))
        for (fs::directory_iterator it(P(dd), ec), end; !ec && it != end; it.increment(ec)) roots.push_back(it->path() / "common" / "on_action");
    for (auto& r : roots) {
        if (!fs::is_directory(r, ec)) continue;
        for (fs::recursive_directory_iterator it(r, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || lower(it->path().extension().u8string()) != ".txt") continue;
            std::string text;
            if (!readFile(it->path(), text, 8u << 20)) continue;
            std::vector<ScriptKey> keys;
            scanScriptKeys(text, 0, keys);
            for (auto& k : keys) names.insert(k.key);
        }
    }
    return names;
}

inline TimeReport attributeTime(TimeTable&& events, TimeTable&& actions, const Playset& ps, const std::map<std::string, ModDefs>& defs, const std::unordered_set<std::string>* baseActions = nullptr) {
    TimeReport r;
    r.events = std::move(events);
    r.onActions = std::move(actions);
    std::unordered_set<std::string> wantEv, wantOa;
    for (auto& x : r.events.rows) wantEv.insert(x.id);
    for (auto& x : r.onActions.rows) wantOa.insert(x.id);
    std::unordered_map<std::string, int> evOwner;
    std::unordered_map<std::string, std::vector<int>> oaDef;
    std::vector<std::set<std::string>> words(ps.mods.size());          // first words of each mod's event namespaces
    for (size_t i = 0; i < ps.mods.size(); i++) {
        if (!ps.mods[i].enabled) continue;
        auto d = defs.find(ps.mods[i].id);
        if (d == defs.end()) continue;
        for (auto& de : d->second.defs) {
            if (de.kind == DK_EVENT) {
                std::string nm = d->second.nameOf(de);
                size_t dot = nm.find('.');
                if (dot != std::string::npos) { std::string w = firstWord(nm); if (w.size() >= 2) words[i].insert(w); }
                if (wantEv.count(nm)) evOwner[nm] = (int)i;
            } else if (de.kind == DK_ONACTION) {
                std::string nm = d->second.nameOf(de);
                if (wantOa.count(nm)) { auto& v = oaDef[nm]; if (v.empty() || v.back() != (int)i) v.push_back((int)i); }
            }
        }
    }
    std::map<int, TimeMod> pm;
    r.eventMod.assign(r.events.rows.size(), -1);
    for (size_t k = 0; k < r.events.rows.size(); k++) {
        const TimeRow& x = r.events.rows[k];
        auto it = evOwner.find(x.id);
        if (it == evOwner.end()) { r.baseEvents += x.own; r.baseRuns += x.count; continue; }
        r.eventMod[k] = it->second;
        TimeMod& m = pm[it->second];
        m.mod = it->second; m.time += x.own; m.runs += x.count; m.events++;
        if (x.own > m.slowestTime) { m.slowestTime = x.own; m.slowest = x.id; }
    }
    r.actionMod.assign(r.onActions.rows.size(), -1);
    r.actionDefiners.assign(r.onActions.rows.size(), {});
    for (size_t k = 0; k < r.onActions.rows.size(); k++) {
        const TimeRow& x = r.onActions.rows[k];
        auto it = oaDef.find(x.id);
        if (it != oaDef.end()) r.actionDefiners[k] = it->second;
        std::string w = firstWord(x.id);
        bool ownName = baseActions && !baseActions->empty() && it != oaDef.end() && it->second.size() == 1 && !baseActions->count(x.id);   // only one mod has it and the base game does not
        if (ownName || (it != oaDef.end() && it->second.size() == 1 && w.size() >= 2 && w != "on" && words[(size_t)it->second[0]].count(w))) {
            int mi = it->second[0];
            if (ownName) r.actionsByName++;
            r.actionMod[k] = mi;
            TimeMod& m = pm[mi];
            m.mod = mi; m.time += x.own; m.runs += x.count;
            if (x.own > m.slowestTime) { m.slowestTime = x.own; m.slowest = x.id; }
        } else { r.sharedActions += x.own; r.sharedRuns += x.count; }
    }
    for (auto& kv : pm) r.perMod.push_back(kv.second);
    std::stable_sort(r.perMod.begin(), r.perMod.end(), [](const TimeMod& a, const TimeMod& b) { return a.time > b.time; });
    r.all = r.events.total + r.onActions.total;
    return r;
}


// ---------- history: one snapshot per game session, to see whether a mod got slower ----------
struct SnapMod { std::string id, name; double time = 0, share = 0; long long runs = 0; };
struct TimeSnap { long long when = 0; double total = 0; size_t events = 0; std::string playset, game; std::vector<SnapMod> mods; };

inline std::string snapClean(std::string s) { for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' '; return s; }
inline std::string snapToText(const std::vector<TimeSnap>& v) {
    std::string o;
    char b[64];
    for (auto& s : v) {
        snprintf(b, sizeof b, "%lld\t%.6g\t%zu\t", s.when, s.total, s.events);
        o += std::string("S\t") + b + snapClean(s.playset) + "\t" + snapClean(s.game) + "\n";
        for (auto& m : s.mods) {
            snprintf(b, sizeof b, "%.6g\t%.6g\t%lld\t", m.time, m.share, m.runs);
            o += "M\t" + snapClean(m.id) + "\t" + b + snapClean(m.name) + "\n";
        }
    }
    return o;
}
inline std::vector<TimeSnap> snapFromText(const std::string& text) {
    std::vector<TimeSnap> v;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(p, e - p);
        p = e + 1;
        std::vector<std::string> c;
        size_t q = 0;
        while (q <= line.size()) { size_t t = line.find('\t', q); if (t == std::string::npos) t = line.size(); c.push_back(line.substr(q, t - q)); q = t + 1; }
        if (c.size() >= 6 && c[0] == "S") { TimeSnap s; s.when = std::atoll(c[1].c_str()); s.total = toDouble(c[2]); s.events = (size_t)std::atoll(c[3].c_str()); s.playset = c[4]; s.game = c[5]; v.push_back(std::move(s)); }
        else if (c.size() >= 6 && c[0] == "M" && !v.empty()) { SnapMod m; m.id = c[1]; m.time = toDouble(c[2]); m.share = toDouble(c[3]); m.runs = std::atoll(c[4].c_str()); m.name = c[5]; v.back().mods.push_back(std::move(m)); }
    }
    return v;
}
// Adds a snapshot of this report (replaces one with the same time), keeps the newest `keep`, oldest first.
inline void snapAdd(std::vector<TimeSnap>& v, TimeSnap s, size_t keep = 30) {
    for (auto& x : v) if (x.when == s.when) { x = std::move(s); return; }
    v.push_back(std::move(s));
    std::stable_sort(v.begin(), v.end(), [](const TimeSnap& a, const TimeSnap& b) { return a.when < b.when; });
    while (v.size() > keep) v.erase(v.begin());
}
inline TimeSnap makeSnap(const TimeReport& r, const Playset& ps, const std::map<std::string, ModInfo>& info, long long when, const std::string& game) {
    TimeSnap s; s.when = when; s.total = r.all; s.events = r.events.rows.size(); s.playset = ps.name; s.game = game;
    const double all = r.all > 0 ? r.all : 1;
    for (auto& m : r.perMod) {
        if (m.mod < 0 || (size_t)m.mod >= ps.mods.size()) continue;
        SnapMod sm; sm.id = ps.mods[(size_t)m.mod].id;
        auto it = info.find(sm.id); sm.name = it != info.end() ? it->second.name : sm.id;
        sm.time = m.time; sm.share = m.time / all; sm.runs = m.runs;
        s.mods.push_back(std::move(sm));
    }
    if (r.baseEvents > 0) s.mods.push_back({"*base", "", r.baseEvents, r.baseEvents / all, r.baseRuns});
    if (r.sharedActions > 0) s.mods.push_back({"*shared", "", r.sharedActions, r.sharedActions / all, r.sharedRuns});
    return s;
}
struct HistRow { std::string id, name; double now = 0, before = 0, change = 0; bool inNow = false, inBefore = false; int seen = 0; long long runsNow = 0; };
// Compares the newest snapshot with the one before it (same playset name preferred). Rows: biggest share now first, then mods that only appear before.
inline std::vector<HistRow> historyRows(const std::vector<TimeSnap>& v, bool* haveBefore = nullptr) {
    std::vector<HistRow> rows;
    if (haveBefore) *haveBefore = false;
    if (v.empty()) return rows;
    const TimeSnap& now = v.back();
    const TimeSnap* before = nullptr;
    for (size_t i = v.size() - 1; i-- > 0;) if (v[i].playset == now.playset) { before = &v[i]; break; }
    if (!before && v.size() >= 2) before = &v[v.size() - 2];
    if (haveBefore) *haveBefore = before != nullptr;
    std::map<std::string, size_t> at;
    for (auto& m : now.mods) { HistRow r; r.id = m.id; r.name = m.name; r.now = m.share; r.inNow = true; r.runsNow = m.runs; at[m.id] = rows.size(); rows.push_back(r); }
    if (before) for (auto& m : before->mods) {
        auto f = at.find(m.id);
        if (f == at.end()) { HistRow r; r.id = m.id; r.name = m.name; r.before = m.share; r.inBefore = true; at[m.id] = rows.size(); rows.push_back(r); }
        else { rows[f->second].before = m.share; rows[f->second].inBefore = true; }
    }
    for (auto& r : rows) { r.change = r.now - r.before; for (auto& s : v) for (auto& m : s.mods) if (m.id == r.id) { r.seen++; break; } }
    std::stable_sort(rows.begin(), rows.end(), [](const HistRow& a, const HistRow& b) { return a.now != b.now ? a.now > b.now : a.before > b.before; });
    return rows;
}

// One mod's share of script time over the last sessions of the same playset, oldest first, in percent: "3.8 \xC2\xB7 4.1 \xC2\xB7 -" (- = the mod was not in that session).
inline std::string trendText(const std::vector<TimeSnap>& v, const std::string& id, size_t maxN = 6) {
    if (v.empty()) return "";
    std::vector<const TimeSnap*> pick;
    for (size_t i = v.size(); i-- > 0 && pick.size() < maxN;) if (v[i].playset == v.back().playset) pick.push_back(&v[i]);
    std::string out;
    for (size_t k = pick.size(); k-- > 0;) {
        std::string cell = "-";
        for (auto& m : pick[k]->mods) if (m.id == id) { char b[24]; std::snprintf(b, sizeof b, "%.1f", m.share * 100.0); cell = b; break; }
        out += (out.empty() ? "" : " \xC2\xB7 ") + cell;
    }
    return out;
}

// A plain-English note a player can send to a mod's author: what was measured, how much of the script time the mod took, and its slowest scripts.
// Events are credited to the last enabled mod that defines them; on_actions only when one mod alone defines them (see attributeTime).
inline std::string authorReport(const TimeReport& tr, int mod, const std::string& modName, const std::string& workshopId, const std::string& modVersion,
                                const std::string& gameVer, const std::string& toolVer, const std::string& trend, double modTime, long long modRuns) {
    char b[160];
    std::string o = "Script time report for \"" + modName + "\"" + (workshopId.empty() ? "" : " (Steam Workshop id " + workshopId + ")") + "\n";
    if (!modVersion.empty()) o += "Mod version: " + modVersion + "\n";
    if (!gameVer.empty()) o += "Game version: " + gameVer + "\n";
    o += "Measured with The Royal Court " + toolVer + ", from the game's own statistics files (eventstats.csv and onactionstats.csv).\n";
    double all = tr.all > 0 ? tr.all : 1;
    std::snprintf(b, sizeof b, "Share of all measured script time in this session: %.1f%% (%lld runs)\n", modTime / all * 100.0, modRuns);
    o += b;
    if (!trend.empty()) o += "Share over the last sessions with the same playset (percent, oldest first): " + trend + "\n";
    auto list = [&](const TimeTable& t, const std::vector<int>& owner, const char* head) {
        std::string s; int n = 0;
        for (size_t i = 0; i < t.rows.size() && i < owner.size() && n < 5; i++) {
            if (owner[i] != mod) continue;
            std::snprintf(b, sizeof b, "  %d. %s: %.1f%% of all script time, %lld runs\n", ++n, t.rows[i].id.c_str(), t.rows[i].own / all * 100.0, t.rows[i].count);
            s += b;
        }
        return s.empty() ? std::string() : std::string(head) + "\n" + s;
    };
    o += list(tr.events, tr.eventMod, "Slowest events:");
    o += list(tr.onActions, tr.actionMod, "Slowest on_actions (counted for this mod only because no other mod and not the base game defines them):");
    o += "A high share only means these scripts take a large part of the measured time; it is not a fault by itself. The time unit is the game's own and is not stated by it.\n";
    return o;
}

// ---------- yearly counts ----------
struct YearStats { int year = 0; std::map<std::string, double> v; };
inline std::vector<YearStats> parseYearStats(const std::string& text) {
    std::map<int, std::map<std::string, double>> by;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        size_t a = text.find("[AUTOMATION_STATS][", p);
        if (a != std::string::npos && a < e) {
            // [name][Year][.]:[value][year]
            size_t n0 = a + 19, n1 = text.find(']', n0);
            size_t colon = n1 == std::string::npos ? n1 : text.find("]:[", n1);
            if (colon != std::string::npos && colon < e) {
                size_t v0 = colon + 3, v1 = text.find(']', v0);
                size_t y0 = v1 == std::string::npos ? v1 : text.find('[', v1);
                size_t y1 = y0 == std::string::npos ? y0 : text.find(']', y0);
                if (y1 != std::string::npos && y1 <= e) {
                    int year = std::atoi(text.substr(y0 + 1, y1 - y0 - 1).c_str());
                    if (year > 0) by[year][text.substr(n0, n1 - n0)] = toDouble(text.substr(v0, v1 - v0));
                }
            }
        }
        p = e + 1;
    }
    std::vector<YearStats> out;
    for (auto& kv : by) { YearStats y; y.year = kv.first; y.v = kv.second; out.push_back(std::move(y)); }
    return out;
}

}  // namespace rc
