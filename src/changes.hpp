// The Royal Court - what changed since you last pressed Play.
// When Play is pressed, a small record of each enabled mod's files is saved (a hash of every path, and of its path+size+time).
// Later the same record is compared with the files now: files added, files changed, files gone. The record holds no file names,
// only hashes, so it stays small (16 bytes a file); the names of added and changed files come from the current list.
#pragma once
#include "core.hpp"

namespace rc {

inline uint64_t pathHash(const std::string& rel) {
    uint64_t h = 1099511628211ULL ^ 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : rel) { h ^= c; h *= 1099511628211ULL; }
    h ^= h >> 31; h *= 0x7fb5d329728ea185ULL; h ^= h >> 27;
    return h;
}

struct Manifest { std::vector<std::pair<uint64_t, uint64_t>> v; };   // (path hash, file hash), sorted by path hash

inline Manifest manifestOf(const ModFiles& f) {
    Manifest m;
    if (!f.complete || f.mix.size() != f.files.size()) return m;
    m.v.reserve(f.files.size());
    for (size_t i = 0; i < f.files.size(); i++) m.v.emplace_back(pathHash(f.files[i]), f.mix[i]);
    std::sort(m.v.begin(), m.v.end());
    return m;
}

inline fs::path manifestDir() {
    fs::path d = P(dataDir()) / "seen";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}
inline fs::path manifestPath(const std::string& modId) {
    char b[24]; snprintf(b, sizeof b, "%016llx", (unsigned long long)fnv64(modId, '#', "manifest"));
    return manifestDir() / (std::string("p_") + b + ".bin");
}

inline bool saveManifest(const std::string& modId, const Manifest& m) {
    if (m.v.empty()) return false;
    std::string out = "RCM1";
    uint64_t n = m.v.size();
    out.append((const char*)&n, 8);
    for (auto& e : m.v) { out.append((const char*)&e.first, 8); out.append((const char*)&e.second, 8); }
    uint64_t sum = fnv64(out, '|', "end");
    out.append((const char*)&sum, 8);
    std::error_code ec;
    fs::path p = manifestPath(modId), tmp = p; tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(out.data(), (std::streamsize)out.size());
        if (!f) return false;
    }
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}

inline bool loadManifest(const std::string& modId, Manifest& m) {
    m.v.clear();
    std::string t;
    if (!readFile(manifestPath(modId), t, 64u << 20) || t.size() < 4 + 8 + 8 || t.compare(0, 4, "RCM1") != 0) return false;
    uint64_t n; memcpy(&n, t.data() + 4, 8);
    if (n > (64u << 20) / 16 || t.size() != 4 + 8 + n * 16 + 8) return false;
    uint64_t sum; memcpy(&sum, t.data() + t.size() - 8, 8);
    if (fnv64(t.substr(0, t.size() - 8), '|', "end") != sum) return false;
    m.v.resize((size_t)n);
    for (size_t i = 0; i < (size_t)n; i++) { memcpy(&m.v[i].first, t.data() + 12 + i * 16, 8); memcpy(&m.v[i].second, t.data() + 12 + i * 16 + 8, 8); }
    for (size_t i = 1; i < m.v.size(); i++) if (m.v[i - 1].first > m.v[i].first) { m.v.clear(); return false; }   // must be sorted
    return true;
}

struct ChangeSummary {
    bool known = false;                    // false: there is no earlier record to compare with
    size_t same = 0, added = 0, changed = 0, removed = 0;
    std::vector<std::string> addedNames, changedNames;   // first few, for display
    std::map<std::string, size_t> folders;               // "common/traits" -> number of added + changed files
};

// Compares the record from the last Play with the mod's files now. Names are listed up to maxNames each.
inline ChangeSummary compareManifest(const Manifest& old, const ModFiles& now, size_t maxNames = 40) {
    ChangeSummary s;
    if (old.v.empty() || !now.complete || now.mix.size() != now.files.size()) return s;
    s.known = true;
    size_t matched = 0;
    for (size_t i = 0; i < now.files.size(); i++) {
        uint64_t ph = pathHash(now.files[i]);
        auto it = std::lower_bound(old.v.begin(), old.v.end(), std::make_pair(ph, (uint64_t)0));
        bool added = false, changed = false;
        if (it == old.v.end() || it->first != ph) added = true;
        else {
            matched++;
            // two files may share a path hash only by accident; any entry with this path hash and the same file hash counts as unchanged
            bool same = false;
            for (auto j = it; j != old.v.end() && j->first == ph; ++j) if (j->second == now.mix[i]) { same = true; break; }
            changed = !same;
        }
        if (!added && !changed) { s.same++; continue; }
        auto& names = added ? s.addedNames : s.changedNames;
        (added ? s.added : s.changed)++;
        if (names.size() < maxNames) names.push_back(now.files[i]);
        std::string rel = now.files[i];
        size_t a = rel.find('/');
        size_t b = a == std::string::npos ? a : rel.find('/', a + 1);
        s.folders[b == std::string::npos ? rel.substr(0, a == std::string::npos ? 0 : a) : rel.substr(0, b)]++;
    }
    s.removed = old.v.size() > matched ? old.v.size() - matched : 0;
    return s;
}

// One line for a list or a log: "3 files changed, 2 added, 1 removed (mostly common/traits)"
inline std::string describeChanges(const ChangeSummary& s) {
    if (!s.known) return "";
    if (!s.added && !s.changed && !s.removed) return tr("no file changes");
    std::string o;
    auto part = [&](const std::string& t) { if (!o.empty()) o += ", "; o += t; };
    if (s.changed) part(s.changed == 1 ? trf("{0} file changed", {s.changed}) : trf("{0} files changed", {s.changed}));
    if (s.added) part(trf("{0} added", {s.added}));
    if (s.removed) part(trf("{0} removed", {s.removed}));
    if (!s.folders.empty()) {
        auto best = s.folders.begin();
        for (auto it = s.folders.begin(); it != s.folders.end(); ++it) if (it->second > best->second) best = it;
        if (!best->first.empty() && s.folders.size() > 1 && best->second * 2 > s.added + s.changed) o = trf("{0} (mostly {1})", {o, best->first});
        else if (s.folders.size() == 1 && !best->first.empty()) o = trf("{0} (in {1})", {o, best->first});
    }
    return o;
}

// ---------- the "since last Play" report ----------
struct PlayItem { std::string modId, name, text; int sev = 0; };   // sev 1 = needs attention
struct PlayReport {
    bool hasPrev = false;                       // Play has been pressed before (with this program version)
    long long time = 0;
    std::string prevGame, nowGame;
    bool gameChanged = false;
    bool samePlayset = true;
    std::vector<PlayItem> updated, outdated, added, removed;
    bool empty() const { return !gameChanged && updated.empty() && outdated.empty() && added.empty() && removed.empty(); }
};

inline PlayReport buildPlayReport(const Settings& st, const Playset& ps, const std::map<std::string, ModInfo>& info,
                                  const std::map<std::string, std::string>& fpNow, const std::map<std::string, ModFiles>& files,
                                  const std::string& gameVer) {
    PlayReport r;
    r.hasPrev = st.playTime > 0;
    r.time = st.playTime; r.prevGame = st.playGameVer; r.nowGame = gameVer;
    r.gameChanged = r.hasPrev && !st.playGameVer.empty() && !gameVer.empty() && st.playGameVer != gameVer;
    auto nameOf = [&](const ModRef& m) { auto it = info.find(m.id); return it != info.end() && !it->second.name.empty() ? it->second.name : (m.name.empty() ? m.id : m.name); };
    for (auto& m : ps.mods) {
        if (!m.enabled) continue;
        auto it = info.find(m.id);
        if (it == info.end()) continue;
        if (!gameVer.empty() && matchGameVersion(it->second.supported, gameVer) == VerMatch::Mismatch)
            r.outdated.push_back({m.id, nameOf(m), trf("made for game version {0}, you have {1}", {it->second.supported, gameVer}), 1});
        auto s = st.seen.find(m.id), n = fpNow.find(m.id);
        if (s != st.seen.end() && n != fpNow.end() && fingerprintChanged(s->second, n->second)) {
            std::string t = tr("updated or edited");
            Manifest old;
            auto f = files.find(m.id);
            if (f != files.end() && loadManifest(m.id, old)) {
                std::string d = describeChanges(compareManifest(old, f->second));
                if (!d.empty()) t = d;
            }
            r.updated.push_back({m.id, nameOf(m), t, 0});
        }
    }
    if (r.hasPrev && st.playName == ps.name) {
        std::set<std::string> was(st.playMods.begin(), st.playMods.end()), is;
        for (auto& m : ps.mods) if (m.enabled) is.insert(m.id);
        for (auto& m : ps.mods) if (m.enabled && !was.count(m.id)) r.added.push_back({m.id, nameOf(m), tr("enabled since then"), 0});
        for (auto& id : st.playMods) {
            if (is.count(id)) continue;
            auto it = info.find(id);
            if (it == info.end()) r.removed.push_back({id, id, tr("no longer installed"), 1});
            else r.removed.push_back({id, it->second.name, tr("turned off since then"), 0});
        }
    } else r.samePlayset = false;
    return r;
}

// Saves what is needed to compare against at the next Play: the enabled mods' file records and the list of enabled mods.
inline void recordPlay(Settings& st, const Playset& ps, const std::map<std::string, ModFiles>& files, const std::string& gameVer, long long now) {
    st.playGameVer = gameVer; st.playName = ps.name; st.playTime = now; st.playMods.clear();
    for (auto& m : ps.mods) {
        if (!m.enabled) continue;
        st.playMods.push_back(m.id);
        auto f = files.find(m.id);
        if (f != files.end()) { Manifest mf = manifestOf(f->second); if (!mf.v.empty()) saveManifest(m.id, mf); }
    }
}

}  // namespace rc
