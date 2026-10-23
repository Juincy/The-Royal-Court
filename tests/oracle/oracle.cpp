// Differential-test oracle: runs the C++ core on a scenario folder and prints a canonical report.
// The C# oracle (tests/RoyalCourt.Oracle) prints exactly the same text; the two outputs must be identical.
#include <iostream>
#include <sstream>
#include "core.hpp"
#include "gamelog.hpp"
#include "scripttime.hpp"
#include "crash.hpp"
#include "share.hpp"
#include "changes.hpp"
using namespace rc;

static std::string esc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) { if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t"; else if (c == '\\') o += "\\\\"; else o += (char)c; }
    return o;
}
static std::string join(const std::vector<std::string>& v, const char* sep = ",") { std::string o; for (size_t i = 0; i < v.size(); i++) o += (i ? sep : "") + esc(v[i]); return o; }
static std::string ints(const std::vector<int>& v) { std::string o; for (size_t i = 0; i < v.size(); i++) o += (i ? "," : "") + std::to_string(v[i]); return o; }

static void scenario(const std::string& root) {
    std::string opts;
    readFile(P(root) / "options.txt", opts);
    std::string gameVer = "1.14.2"; std::set<std::string> locked; std::map<std::string, int> over;
    {
        std::istringstream is(opts); std::string line;
        while (std::getline(is, line)) {
            if (line.rfind("game ", 0) == 0) gameVer = line.substr(5);
            else if (line.rfind("lock ", 0) == 0) locked.insert(line.substr(5));
            else if (line.rfind("cat ", 0) == 0) { size_t sp = line.rfind(' '); over[line.substr(4, sp - 4)] = std::atoi(line.c_str() + sp + 1); }
        }
    }
    std::string dir = (P(root) / "ck3").u8string();
    auto mods = scanMods(dir);
    std::stable_sort(mods.begin(), mods.end(), [](const ModInfo& a, const ModInfo& b) { return lower(a.name) != lower(b.name) ? lower(a.name) < lower(b.name) : a.id < b.id; });
    std::printf("== mods %zu\n", mods.size());
    for (auto& m : mods) std::printf("%s|%s|%s|%s|%s|%s|%d|%s|%s|%s\n", esc(m.id).c_str(), esc(m.name).c_str(), esc(m.version).c_str(), esc(m.supported).c_str(), m.source.c_str(), esc(m.path).c_str(), m.contentState, join(m.deps).c_str(), join(m.replacePaths).c_str(), join(m.tags).c_str());
    std::string ptext; readFile(P(root) / "playset.json", ptext);
    auto pr = parsePlaysetFile(ptext, mods);
    std::printf("== playset ok=%d err=%s invalid=%d unmatched=%d name=%s\n", pr.ok, esc(pr.error).c_str(), pr.invalid, pr.unmatched, esc(pr.playset.name).c_str());
    if (!pr.ok) return;
    Playset ps = pr.playset;
    syncPlayset(ps, mods);
    auto info = infoMap(mods);
    for (size_t i = 0; i < ps.mods.size(); i++) std::printf("P%zu|%s|%d|%s\n", i, esc(ps.mods[i].id).c_str(), ps.mods[i].enabled, esc(ps.mods[i].name).c_str());
    std::printf("== export\n%s\n", exportLauncherPlayset(ps, info).c_str());
    // files
    std::map<std::string, ModFiles> index; std::map<std::string, ModDefs> defs;
    for (auto& m : mods) {
        if (m.contentDir.empty() || m.contentState != 1) continue;
        ModFiles f = indexModFiles(m.contentDir);
        std::sort(f.files.begin(), f.files.end());
        // order of files is not specified; the report sorts them
        ModFiles g; g.complete = f.complete; g.files = f.files; g.deep = "";
        index[m.id] = g;
        defs[m.id] = indexModDefs(m.contentDir, g.files);
    }
    std::printf("== files\n");
    for (auto& kv : index) std::printf("F|%s|%d|%zu|%s\n", esc(kv.first).c_str(), kv.second.complete, kv.second.files.size(), join(kv.second.files, ";").c_str());
    // vanilla
    std::shared_ptr<VanillaIndex> van;
    std::string exe = (P(root) / "install" / "binaries" / "ck3.exe").u8string();
    std::string gdir = vanillaGameDir(exe);
    if (!gdir.empty()) van = indexVanilla(gdir);
    std::printf("== vanilla %s %zu %zu\n", gdir.empty() ? "none" : "yes", van ? van->files.size() : 0, van ? van->dlcFiles : 0);
    auto issues = analyzePlayset(ps, info, gameVer);
    ConflictReport cr = findConflicts(ps, info, index, van.get());
    std::printf("== conflicts valid=%d idx=%d files=%d without=%d vchecked=%d\n", cr.valid, cr.modsIndexed, cr.filesIndexed, cr.modsWithoutFiles, cr.vanillaChecked);
    for (auto& f : cr.files) std::printf("C|%s|%s|%d|%d\n", esc(f.path).c_str(), ints(f.mods).c_str(), f.sev, f.vanilla);
    for (auto& p : cr.pairs) { std::string a; for (auto& x : p.areas) a += x.first + ":" + std::to_string(x.second) + ";"; std::printf("PAIR|%d|%d|%d|%d|%s\n", p.a, p.b, p.count, p.sev, esc(a).c_str()); }
    for (auto& w : cr.wipes) std::printf("WIPE|%d|%d|%d|%s\n", w.replacer, w.victim, w.files, esc(w.folder).c_str());
    for (auto& v : cr.vanilla) std::printf("VAN|%s|%s|%d\n", esc(v.path).c_str(), ints(v.mods).c_str(), v.sev);
    for (auto& w : cr.vanillaWipes) std::printf("VW|%d|%d|%s\n", w.mod, w.files, esc(w.folder).c_str());
    std::printf("LOSES|%s\nWINS|%s\nVC|%s\n", ints(cr.loses).c_str(), ints(cr.wins).c_str(), ints(cr.vanillaCount).c_str());
    ScriptReport sr = findScriptConflicts(ps, info, defs);
    std::printf("== script valid=%d read=%d missing=%d\n", sr.valid, sr.modsRead, sr.modsMissing);
    for (auto& it : sr.items) {
        std::string h; for (auto& x : it.hits) h += std::to_string(x.mod) + ":" + x.file + ":" + std::to_string(x.flags) + ";";
        std::printf("S|%d|%d|%d|%s|%s|%s|%s\n", it.kind, it.sev, it.winner, esc(it.area).c_str(), esc(it.key).c_str(), esc(it.note).c_str(), esc(h).c_str());
    }
    for (size_t i = 0; i < sr.perMod.size(); i++) { std::printf("SPM|%zu", i); for (int k = 0; k < DK_KINDS; k++) std::printf("|%d", sr.perMod[i][(size_t)k]); std::printf("|%d\n", sr.unclear[i]); }
    // defs summary per mod
    for (auto& kv : defs) {
        std::vector<std::string> names; for (auto& d : kv.second.defs) names.push_back(std::to_string(d.kind) + ":" + std::to_string(d.flags) + ":" + kv.second.nameOf(d) + "@" + kv.second.files[d.file]);
        std::sort(names.begin(), names.end());
        std::printf("D|%s|%zu|%s\n", esc(kv.first).c_str(), kv.second.bytesRead, join(names, ";").c_str());
    }
    addConflictIssues(issues, cr, ps, info, gameVer);
    addScriptIssues(issues, sr, ps);
    std::printf("== issues\n");
    for (size_t i = 0; i < issues.size(); i++) for (auto& is : issues[i]) std::printf("I|%zu|%d|%s\n", i, is.sev, esc(is.text).c_str());
    // known / sort
    auto db = builtinKnownMods();
    std::printf("== known %zu\n", db.size());
    for (auto& m : mods) std::printf("K|%s|%d\n", esc(m.id).c_str(), findKnown(db, m));
    SortPlan plan = planSort(ps, info, locked, over, &cr, &index, &db);
    std::printf("== sort changed=%d known=%d links=%d choices=%d\n", plan.changed, plan.knownCount, plan.patchLinks, plan.conflictChoices);
    std::printf("ORDER|%s\nNEWPOS|%s\nCAT|%s\nTIER|%s\n", ints(plan.order).c_str(), ints(plan.newPos).c_str(), ints(plan.cat).c_str(), ints(plan.posTier).c_str());
    for (size_t i = 0; i < plan.cat.size(); i++) std::printf("CW|%zu|%s|%s\n", i, esc(plan.catWhy[i]).c_str(), esc(plan.known[i]).c_str());
    for (auto& m : plan.moves) std::printf("MV|%d|%d|%s|%s\n", m.from, m.to, esc(m.id).c_str(), esc(m.reason).c_str());
    for (auto& w : plan.warnings) std::printf("WARN|%s\n", esc(w).c_str());
    for (auto& b : plan.broken) std::printf("BRK|%d|%d|%d|%s\n", b.mod, b.other, b.kind, esc(b.text).c_str());
    for (auto& e : plan.edges) std::printf("EDGE|%d|%d|%d|%d|%s\n", e.from, e.to, e.kind, e.subj, esc(e.text).c_str());
    for (int p = 0; p < (int)ps.mods.size(); p++) std::printf("EXPL|%d\n%s\n", p, esc(explainPosition(plan, ps, info, p, locked.count(ps.mods[(size_t)p].id) > 0)).c_str());
    auto hr = checkHealth(ps, info, gameVer, locked, over, &cr, &index, &db);
    std::printf("== health\n%s\n", esc(healthText(hr, ps.name)).c_str());
    auto ch = findCompatches(ps, info);
    std::printf("== compat\n%s\n", esc(compatText(ch, ps.name)).c_str());
    for (auto& h : ch) std::printf("CH|%d|%s|%s|%s|%d\n", h.state, esc(h.patch).c_str(), h.id.c_str(), join(h.parents).c_str(), h.patchPos);
    // sharing
    std::string c1 = makeShareCode(ps, info, false), c2 = makeShareCode(ps, info, true);
    std::printf("== share\n%s\n%s\n", c1.c_str(), c2.c_str());
    for (auto* c : {&c1, &c2}) { auto d = decodeShareCode(*c); std::printf("DEC|%d|%s|%d|%zu\n", d.ok, esc(d.name).c_str(), d.withNames, d.mods.size()); for (auto& m : d.mods) std::printf("DM|%s|%s|%d|%d\n", m.workshopId.c_str(), esc(m.name).c_str(), m.enabled, m.local); }
    std::printf("== report\n%s\n", esc(buildSortReport(ps, info, db, plan.cat, locked, gameVer, 0)).c_str());
    // playset compare against itself reversed
    Playset rev = ps; std::reverse(rev.mods.begin(), rev.mods.end()); rev.name = "rev";
    auto pd = comparePlaysets(ps, rev, info);
    std::printf("== compare shared=%d same=%d\nOA|%s\nOB|%s\nED|%s\nMV|%s\n", pd.shared, pd.sameOrder, join(pd.onlyA).c_str(), join(pd.onlyB).c_str(), join(pd.enabledDiffers).c_str(), join(pd.moved).c_str());
}


static std::string unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) { char c = s[++i]; o += c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c; }
        else o += s[i];
    }
    return o;
}
static std::vector<std::string> splitTab(const std::string& s) { std::vector<std::string> v; size_t p = 0; while (true) { size_t q = s.find('\t', p); if (q == std::string::npos) { v.push_back(unesc(s.substr(p))); break; } v.push_back(unesc(s.substr(p, q - p))); p = q + 1; } return v; }
static std::string d2s(double d) { char b[64]; std::snprintf(b, sizeof b, "%.10g", d); return b; }
static std::string tableOut(const TimeTable& t) { std::string o = std::to_string(t.ok) + ";" + d2s(t.total); for (auto& r : t.rows) o += ";" + r.id + "," + d2s(r.sum) + "," + d2s(r.own) + "," + d2s(r.share) + "," + std::to_string(r.count); return o; }
static void funcs(const std::string& file) {
    std::string text; readFile(P(file), text, 256u << 20);
    std::istringstream is(text); std::string line; int idx = 0;
    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto a = splitTab(line);
        if (a.empty()) continue;
        std::string fn = a[0], out;
        auto arg = [&](size_t i) -> std::string { return i < a.size() ? a[i] : ""; };
        if (fn == "desc") { auto kv = parseDescriptor(arg(1)); for (auto& e : kv) { out += e.first + "=["; for (auto& v : e.second) out += esc(v) + ","; out += "];"; } }
        else if (fn == "ver") { out = std::to_string((int)matchGameVersion(arg(1), arg(2))) + "|" + join(versionSegments(arg(1))) + "|" + join(versionSegments(arg(2))); }
        else if (fn == "gv") out = gameVersionFromSettings(arg(1));
        else if (fn == "nat") out = std::to_string(naturalCompare(arg(1), arg(2)));
        else if (fn == "norm") out = normName(arg(1)) + "|" + acronymOf(normName(arg(1)));
        else if (fn == "nameis") out = std::to_string(nameIs(arg(1), arg(2)));
        else if (fn == "keys") { std::vector<ScriptKey> k; scanScriptKeys(arg(2), std::atoi(arg(1).c_str()), k); for (auto& x : k) out += x.key + ":" + std::to_string(x.flags) + ";"; }
        else if (fn == "loc") { std::vector<std::string> k; scanLocKeys(arg(1), k); out = join(k, ";"); }
        else if (fn == "fk") { auto k = defFileKind(arg(1)); out = std::to_string(k.kind) + "|" + k.area + "|" + std::to_string(k.mode) + "|" + std::to_string(k.flags); }
        else if (fn == "json") { J r; bool ok = parseJson(arg(1), r); out = std::to_string(ok); if (ok) { std::string d, m; dump(r, d); dumpMin(r, m); out += "|" + d + "|" + m; } }
        else if (fn == "csvev") out = tableOut(parseStatsCsv(arg(1), false));
        else if (fn == "csvoa") out = tableOut(parseStatsCsv(arg(1), true));
        else if (fn == "year") { for (auto& y : parseYearStats(arg(1))) { out += std::to_string(y.year) + "{"; for (auto& kv : y.v) out += kv.first + "=" + d2s(kv.second) + ","; out += "};"; } }
        else if (fn == "log") { auto r = parseGameLog(arg(1)); out = std::to_string(r.lines) + "," + std::to_string(r.errors) + "," + std::to_string(r.warnings) + "," + std::to_string(r.dropped) + ";"; for (auto& e : r.entries) out += std::string(1, e.level) + "|" + e.src + "|" + e.msg + "|" + e.detail + "|" + e.file + "|" + std::to_string(e.line) + "|" + join(e.names, "~") + "|" + std::to_string(e.count) + ";"; }
        else if (fn == "loaded") { for (auto& m : parseLoadedMods(arg(1))) out += m.name + "|" + m.id + "|" + std::to_string(m.enabled) + ";"; }
        else if (fn == "cex") { CrashData d; crashParseException(arg(1), d); out = std::to_string(d.kind) + "|" + std::to_string(d.haveException) + "|" + d.code + "|" + join(d.frames, ";"); }
        else if (fn == "cmeta") { CrashData d; crashParseMeta(arg(1), d); out = std::to_string(d.haveMeta) + "|" + join(d.meta, ";"); }
        else if (fn == "ckind") out = std::to_string((int)crashKindOf(arg(1)));
        else if (fn == "cname") { CrashFolder f; bool ok = crashNameParse(arg(1), f); out = std::to_string(ok) + "|" + std::to_string(f.y) + "," + std::to_string(f.mo) + "," + std::to_string(f.d) + "," + std::to_string(f.h) + "," + std::to_string(f.mi) + "," + std::to_string(f.s); }
        else if (fn == "tail") out = crashTailLines(arg(1), (size_t)std::atoi(arg(2).c_str()));
        else if (fn == "vdf") out = join(vdfValues(arg(1), arg(2)), ";");
        else if (fn == "regdesc") out = registeredDescriptorText(arg(1), arg(2), arg(3));
        else if (fn == "rel") { auto r = parseRelease(arg(1)); out = std::to_string(r.ok) + "|" + r.tag + "|" + r.name + "|" + r.body + "|" + r.pageUrl; for (auto& x : r.assets) out += "|" + x.name + "," + x.url + "," + std::to_string(x.size); }
        else if (fn == "cmpver") out = std::to_string(compareVersions(arg(1), arg(2)));
        else if (fn == "sum") out = findSumFor(arg(1), arg(2));
        else if (fn == "share") { auto d = decodeShareCode(arg(1)); out = std::to_string(d.ok) + "|" + d.error + "|" + d.name + "|" + std::to_string(d.withNames); for (auto& m : d.mods) out += "|" + m.workshopId + "," + m.name + "," + std::to_string(m.enabled) + "," + std::to_string(m.local); }
        else if (fn == "sha") out = sha256Hex(arg(1));
        else if (fn == "sani") out = sanitizeFileName(arg(1));
        else if (fn == "trim") out = trimmed(arg(1));
        else if (fn == "snap") { auto v = snapFromText(arg(1)); out = snapToText(v); }
        else if (fn == "fpc") out = std::to_string(fingerprintChanged(arg(1), arg(2)));
        else if (fn == "steamid") out = steamIdOf(arg(1)) + "|" + std::to_string(validID(arg(1)));
        else if (fn == "lower") out = lower(arg(1));
        else if (fn == "tr") out = trf(arg(1).c_str(), {arg(2), arg(3)});
        else continue;
        std::printf("%s|%d|%s\n", fn.c_str(), idx++, esc(out).c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: oracle scenario <dir>\n"); return 2; }
    std::string cmd = argv[1];
    if (cmd == "scenario") scenario(argv[2]);
    else if (cmd == "funcs") funcs(argv[2]);
    return 0;
}
