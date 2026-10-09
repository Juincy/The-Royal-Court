// Mutation fuzzer for the parsers that read files the user (or the internet) controls. Build with sanitizers:
//   g++ -std=c++17 -O1 -g -fsanitize=address,undefined -o /tmp/fz src/tests/fuzz_core.cpp && /tmp/fz [iterations]
// Any crash, hang or sanitizer report is a bug.
#include "../core.hpp"
#include "../gamelog.hpp"
#include "../share.hpp"
#include "../changes.hpp"
#include <iostream>
#include <random>
using namespace rc;

int main(int argc, char** argv) {
    long iters = argc > 1 ? atol(argv[1]) : 200000;
    std::mt19937_64 rng(12345);
    std::vector<std::string> seeds = {
        "name=\"X\"\nversion=\"1.0\"\ntags={\n\t\"A\" \"B\"\n}\nsupported_version=\"1.20.*\"\npath=\"/a/b\"\nreplace_path=\"history\"\n",
        R"({"game":"ck3","name":"P","mods":[{"displayName":"A","position":0,"enabled":true,"steamId":"123","pdxId":"x","path":"C:\\x"}]})",
        R"({"tag_name":"v1.2.3","body":"x","html_url":"https://github.com/Juincy/the-royal-court/releases/tag/v1","assets":[{"name":"a","size":1,"browser_download_url":"https://github.com/Juincy/the-royal-court/releases/download/v1/a"}]})",
        "namespace = x\nx.1 = {\n type = character_event\n trigger = { always = yes }\n}\nscripted_trigger a = { x = 1 }\n",
        "l_english:\n key:0 \"text \\\"q\\\" $X$ #bold\"\n key2: \"v\"\n",
        "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Steam\"\n\t}\n}\n",
        R"({"format":1,"revision":2,"mods":[{"names":["A B"],"ids":["1"],"type":"Graphics","position":"top","rank":5,"after":["x"],"before":["y"],"note":"n"}]})",
        "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\n  Error: x [ y ]\n  Script location: file: events/a.txt line: 41 (ev.1:option)\n\n[08:41:14][W][a.cpp:1]: 'q' at file: common/x.txt line: 2 (n)\n",
        "Name|mod/ugc_1.mod|Enabled\nOther|mod/b.mod|Disabled\n",
        "RC1:AQAAAQ",
        "1.20.0.4 (Scythe)", "v0.14.1-beta", "{\"rawVersion\":\"1.20.0.4\"}",
        std::string(64, 'a') + "  TheRoyalCourt.exe\r\n",
    };
    auto mutate = [&](std::string s) {
        int n = 1 + (int)(rng() % 6);
        for (int i = 0; i < n; i++) {
            switch (rng() % 7) {
                case 0: if (!s.empty()) s[rng() % s.size()] = (char)rng(); break;
                case 1: s.insert(s.begin() + (long)(rng() % (s.size() + 1)), (char)rng()); break;
                case 2: if (!s.empty()) s.erase(rng() % s.size(), 1 + rng() % 8); break;
                case 3: if (!s.empty()) { size_t a = rng() % s.size(), l = rng() % (s.size() - a + 1); s.insert(rng() % (s.size() + 1), s.substr(a, l)); } break;
                case 4: s.insert(rng() % (s.size() + 1), std::string(1 + rng() % 40, "{}[]\"\\=#\n"[rng() % 9])); break;
                case 5: if (!s.empty()) s.resize(rng() % s.size()); break;
                default: s += seeds[rng() % seeds.size()]; break;
            }
            if (s.size() > 20000) s.resize(20000);
        }
        return s;
    };
    std::vector<ModInfo> installed;
    for (int i = 0; i < 4; i++) { ModInfo m; m.id = "ugc_" + std::to_string(100 + i) + ".mod"; m.name = "Mod " + std::to_string(i); installed.push_back(m); }
    // saved indexes: a valid file of each kind, damaged in random ways, must be refused or read, never crash
    fs::path fdir = fs::temp_directory_path() / "rc_fuzz_cache";
    fs::remove_all(fdir); fs::create_directories(fdir);
    std::string cdir = fdir.u8string();
    ModDefs seedDefs; seedDefs.complete = true; seedDefs.files = {"events/a.txt", "common/traits/t.txt"}; seedDefs.pool = "brave" "ee.1" "kind";
    { DefEntry d; d.h = 1; d.file = 0; d.nameOff = 0; d.nameLen = 5; d.kind = DK_COMMON; seedDefs.defs.push_back(d); d.h = 2; d.file = 1; d.nameOff = 5; d.nameLen = 4; d.kind = DK_EVENT; seedDefs.defs.push_back(d); }
    saveModCache(cdir, "m.mod", "deep-1", seedDefs);
    std::string modSeed, vanSeed;
    readFile(modCacheFile(cdir, "m.mod"), modSeed);
    { VanillaIndex v; v.key = "k"; v.dir = "d"; v.files = {"common/a.txt", "events/b.txt", "gfx/c.dds"}; v.finish(); saveVanillaCache(cdir, v); }
    readFile(vanillaCacheFile(cdir), vanSeed);
    std::string shareSeed;
    { Playset sp; sp.name = "S"; sp.mods = {{"ugc_100.mod", true, ""}, {"loc.mod", true, "Local"}}; std::map<std::string, ModInfo> si; ModInfo lm; lm.id = "loc.mod"; lm.name = "Local"; si[lm.id] = lm;
      std::string c = makeShareCode(sp, si, true); b64urlDecode(c.substr(4), shareSeed); }
    for (long it = 0; it < iters; it++) {
        std::string s = mutate(seeds[rng() % seeds.size()]);
        if (it % 4 == 0) {
            std::string mc = mutate(modSeed); writeFile(modCacheFile(cdir, "m.mod"), mc);
            ModDefs out; if (loadModCache(cdir, "m.mod", "deep-1", out)) for (auto& d : out.defs) (void)out.nameOf(d);
            std::string vc = mutate(vanSeed); writeFile(vanillaCacheFile(cdir), vc);
            if (auto v = loadVanillaCache(cdir, "k")) { (void)v->has(s); (void)v->countUnder(s); }
        }
        { auto lp = parseGameLog(s); Playset gp; gp.name = "g"; gp.mods = {{"ugc_100.mod", true, ""}};
          std::map<std::string, ModFiles> gf; gf["ugc_100.mod"].files = {s.substr(0, 20), "events/a.txt"}; gf["ugc_100.mod"].complete = true;
          std::map<std::string, ModDefs> gd; auto rep = attributeLog(std::move(lp), gp, gf, gd, nullptr); for (size_t k = 0; k < rep.parse.entries.size(); k++) (void)rep.how[k];
          (void)logModReport(rep, 0, s.substr(0, 10)); (void)parseLoadedMods(s); }
        { auto d = decodeShareCode(s); if (d.ok) (void)playsetFromShare(d, installed); (void)findShareCode(s); std::string bb; (void)b64urlDecode(s, bb); }
        { std::string code = std::string("RC1:") + b64urlEncode(mutate(shareSeed)); auto d = decodeShareCode(code); if (d.ok) (void)playsetFromShare(d, installed); }
        J j; parseJson(s, j); std::string o; dump(j, o);
        (void)parseDescriptor(s);
        (void)parsePlaysetFile(s, installed);
        (void)parseRelease(s);
        (void)findSumFor(s, "TheRoyalCourt.exe");
        (void)compareVersions(s, "0.14.0"); (void)parseVersion(s);
        (void)versionSegments(s); (void)matchGameVersion(s, "1.20.0.4"); (void)gameVersionFromSettings(s);
        (void)normName(s); (void)vdfValues(s, "path"); (void)acfInstallDir(s);
        (void)registeredDescriptorText(s, "123", "C:\\x\\y");
        (void)sanitizeFileName(s);
        std::vector<ScriptKey> keys; scanScriptKeys(s, (int)(rng() % 3), keys);
        std::vector<std::string> lk; scanLocKeys(s, lk);
        (void)sha256Hex(s);
        { std::vector<KnownMod> kv; int added = 0; applyKnownJson(kv, s, &added); (void)checkKnownOnline(s);
          ModInfo km; km.id = "ugc_1.mod"; km.name = s.substr(0, 40); (void)findKnown(kv, km);
          Playset kp; kp.name = s.substr(0, 20); kp.mods.push_back({km.id, true, km.name}); std::map<std::string, ModInfo> ki; ki[km.id] = km;
          (void)buildSortReport(kp, ki, kv, {CAT_CONTENT}, {km.id}, s.substr(0, 10), 3); }
    }
    std::cout << "fuzz ok: " << iters << " iterations\n";
}
