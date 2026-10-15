// Property test for Auto Sort: random playsets built from the known-mods database must always sort into a valid, stable, rule-respecting order.
//   g++ -std=c++17 -O1 -fsanitize=address,undefined -o /tmp/ps src/tests/prop_sort.cpp && /tmp/ps [rounds]
#include "../core.hpp"
#include <iostream>
#include <random>
using namespace rc;
int main(int argc, char** argv) {
    long rounds = argc > 1 ? atol(argv[1]) : 2000;
    std::mt19937_64 rng(777);
    auto db = builtinKnownMods();
    long bad = 0, withWarn = 0, notIdem = 0, ruleBreak = 0;
    for (long r = 0; r < rounds; r++) {
        std::vector<ModInfo> ms; std::map<std::string, int> byName;
        auto add = [&](const std::string& name) {
            if (byName.count(name)) return;
            ModInfo m; m.id = "m" + std::to_string(ms.size()) + ".mod"; m.name = name; byName[name] = (int)ms.size(); ms.push_back(m);
        };
        int k = 3 + (int)(rng() % 14);
        for (int i = 0; i < k; i++) {
            const KnownMod& e = db[rng() % db.size()];
            add(e.names[0]);
            for (auto& a : e.after) if (rng() % 4) add(a);
            for (auto& b : e.before) if (rng() % 4) add(b);
        }
        int extra = (int)(rng() % 6);
        const char* gen[] = {"Cool Events", "Better UI", "Nice Portraits", "More Stuff", "Compatibility Patch for X", "Lib Core Framework", "Dragons"};
        for (int i = 0; i < extra; i++) add(std::string(gen[rng() % 7]) + (rng() % 2 ? "" : " " + std::to_string(i)));
        // shuffle
        std::vector<int> perm(ms.size()); for (size_t i = 0; i < perm.size(); i++) perm[i] = (int)i;
        std::shuffle(perm.begin(), perm.end(), rng);
        Playset ps; ps.name = "p";
        for (int i : perm) { ps.mods.push_back({ms[(size_t)i].id, rng() % 6 != 0, ""}); }
        std::map<std::string, ModInfo> info; for (auto& m : ms) info[m.id] = m;
        std::set<std::string> locked;
        if (rng() % 4 == 0) locked.insert(ps.mods[rng() % ps.mods.size()].id);
        SortPlan a = planSort(ps, info, locked, {}, nullptr, nullptr, &db);
        SortPlan b = planSort(ps, info, locked, {}, nullptr, nullptr, &db);
        // permutation
        std::vector<int> sorted = a.order; std::sort(sorted.begin(), sorted.end());
        bool perm_ok = sorted.size() == ps.mods.size();
        for (size_t i = 0; perm_ok && i < sorted.size(); i++) perm_ok = sorted[i] == (int)i;
        if (!perm_ok || a.order != b.order) { bad++; std::cout << "BAD permutation/determinism round " << r << "\n"; continue; }
        // turned-off mods are never moved
        for (size_t i = 0; i < ps.mods.size(); i++) if (!ps.mods[i].enabled && a.order[i] != (int)i) { bad++; std::cout << "BAD disabled mod moved round " << r << "\n"; }
        for (auto& mv : a.moves) for (auto& m : ps.mods) if (m.id == mv.id && !m.enabled) { bad++; std::cout << "BAD disabled mod in preview round " << r << "\n"; }
        // locked keep position
        for (size_t i = 0; i < ps.mods.size(); i++) if (locked.count(ps.mods[i].id) && a.order[i] != (int)i) { bad++; std::cout << "BAD lock moved round " << r << "\n"; }
        // apply and re-sort: stable?
        Playset q; q.name = "q"; for (int o : a.order) q.mods.push_back(ps.mods[(size_t)o]);
        SortPlan c = planSort(q, info, locked, {}, nullptr, nullptr, &db);
        if (c.changed) notIdem++;
        if (a.warnings.empty() && locked.empty()) {          // a sorted playset has nothing left for the health check to flag
            SortPlan d = planSort(q, info, locked, {}, nullptr, nullptr, &db);
            if (!d.broken.empty()) { bad++; std::cout << "BAD broken rule after sort round " << r << ": " << d.broken[0].text << "\n"; }
        }
        if (!a.warnings.empty()) { withWarn++; for (auto& w : a.warnings) std::cout << "WARN: " << w << "\n"; }
        // enabled-only positions for rules
        std::map<std::string, int> pos; int p = 0;
        for (auto& m : q.mods) { if (m.enabled) pos[info[m.id].name] = p++; }
        if (a.warnings.empty() && locked.empty()) {
            for (auto& m : q.mods) {
                if (!m.enabled) continue;
                int ki = findKnown(db, info[m.id]); const KnownMod* kn = ki >= 0 ? &db[(size_t)ki] : nullptr;
                if (!kn) continue;
                for (auto& x : kn->after) { auto it = pos.find(x); if (it != pos.end() && it->second > pos[info[m.id].name]) { ruleBreak++; std::cout << "RULE: " << info[m.id].name << " should be after " << x << " round " << r << "\n"; } }
                for (auto& x : kn->before) { auto it = pos.find(x); if (it != pos.end() && it->second < pos[info[m.id].name]) { ruleBreak++; std::cout << "RULE: " << info[m.id].name << " should be before " << x << " round " << r << "\n"; } }
            }
        }
    }
    std::cout << "rounds " << rounds << " bad " << bad << " not-idempotent " << notIdem << " rule-breaks " << ruleBreak << " with-warnings " << withWarn << "\n";
    return bad || ruleBreak ? 1 : 0;
}
