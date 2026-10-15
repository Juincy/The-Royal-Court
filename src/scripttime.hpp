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
};

inline std::string firstWord(const std::string& s) {
    size_t u = s.find('_');
    std::string w = s.substr(0, u == std::string::npos ? s.size() : u);
    size_t d = w.find('.');
    if (d != std::string::npos) w.erase(d);
    for (auto& ch : w) ch = (char)tolower((unsigned char)ch);
    return w;
}

inline TimeReport attributeTime(TimeTable&& events, TimeTable&& actions, const Playset& ps, const std::map<std::string, ModDefs>& defs) {
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
        if (it != oaDef.end() && it->second.size() == 1 && w.size() >= 2 && w != "on" && words[(size_t)it->second[0]].count(w)) {
            int mi = it->second[0];
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
