#include <cassert>
#include <chrono>
#include <iostream>
#include "../core.hpp"
#include "../gamelog.hpp"
#include "../crash.hpp"
#include "../share.hpp"
#include "../changes.hpp"
using namespace rc;

static std::string fx(const char* name) {
    std::string s;
    bool ok = readFile(fs::path(__FILE__).parent_path() / "fixtures" / name, s);
    assert(ok); (void)ok;
    return s;
}

int main() {
    fs::path T = fs::temp_directory_path() / "rc_core_test";
    fs::remove_all(T);
    fs::create_directories(T / "ck3" / "mod"); fs::create_directories(T / "data");
    setenv("RC_DATA_DIR", (T / "data").string().c_str(), 1);

    // ---- mod scanning ----
    writeFile(T / "ck3/mod/ugc_111.mod", "name=\"Rise and Fall\"\r\nversion=\"1.2\"\r\nsupported_version=\"1.20.*\"\r\nnamespace=\"x\"\r\n");
    writeFile(T / "ck3/mod/local.mod", "  name = \"My Local Mod\"\nversion=\"0.3\"\n");
    writeFile(T / "ck3/mod/ugc_222.mod", "name=\"Sub Rosa\"\n");
    writeFile(T / "ck3/mod/readme.txt", "x");
    std::string dir = (T / "ck3").string();
    auto mods = scanMods(dir);
    assert(mods.size() == 3);
    for (auto& m : mods) if (m.id == "ugc_111.mod") assert(m.name == "Rise and Fall" && m.version == "1.2" && m.supported == "1.20.*" && m.source == "Workshop");
    assert(scanMods("").empty() && scanMods((T / "nope").string()).empty());

    // ---- the real launcher export round-trips byte for byte ----
    std::string real = fx("launcher_export_Vanilla+.json");
    auto r = parsePlaysetFile(real, {});
    assert(r.ok && r.playset.name == "Vanilla+" && r.playset.mods.size() == 18 && r.unmatched == 0 && r.invalid == 0);
    assert(r.playset.mods[0].id == "ugc_2697392271.mod" && r.playset.mods[17].id == "ugc_2220098919.mod");
    assert(exportLauncherPlayset(r.playset, {}) == real);

    // ---- other launcher-file shapes ----
    std::string lf = "{\"game\":\"ck3\",\"name\":\"Initial playset\",\"mods\":["
        "{\"displayName\":\"Sub Rosa\",\"enabled\":true,\"position\":\"2\",\"steamId\":\"222\"},"
        "{\"displayName\":\"Rise and Fall\",\"enabled\":false,\"position\":1,\"steamId\":111},"
        "{\"displayName\":\"my local mod\",\"enabled\":true,\"position\":0},"
        "{\"displayName\":\"Totally Unknown\",\"enabled\":true,\"position\":3},"
        "{\"displayName\":\"Registry One\",\"gameRegistryId\":\"mod/ugc_333.mod\",\"position\":4},"
        "{\"displayName\":\"Evil\",\"gameRegistryId\":\"mod/../../evil.mod\",\"position\":5,\"steamId\":\"x\"},"
        "{\"enabled\":true},42]}";
    auto r2 = parsePlaysetFile(lf, mods);
    assert(r2.ok && r2.invalid == 2 && r2.unmatched == 2);   // 42 and the entry with no name/id are invalid; "Totally Unknown" and "Evil" kept as placeholders
    assert(r2.playset.mods[0].id == "local.mod");              // position 0, matched by name
    assert(r2.playset.mods[1].id == "ugc_111.mod" && !r2.playset.mods[1].enabled);
    assert(r2.playset.mods[2].id == "ugc_222.mod");
    assert(r2.playset.mods[3].id == "local:Totally Unknown");
    assert(r2.playset.mods[4].id == "ugc_333.mod");
    assert(!parsePlaysetFile("{\"game\":\"stellaris\",\"name\":\"x\",\"mods\":[]}", {}).ok);
    assert(!parsePlaysetFile("{bad", {}).ok && !parsePlaysetFile("{\"hello\":1}", {}).ok && !parsePlaysetFile("[]", {}).ok);

    // ---- sync: placeholders re-attach when the mod is later installed; other mods listed as disabled ----
    Playset sp = r2.playset;
    syncPlayset(sp, mods);
    bool sawSubRosa = false;
    for (auto& m : sp.mods) if (m.id == "ugc_222.mod") { sawSubRosa = true; assert(m.name == "Sub Rosa"); }
    assert(sawSubRosa);
    ModInfo later; later.id = "unknown_later.mod"; later.name = "Totally Unknown";
    auto withLater = mods; withLater.push_back(later);
    syncPlayset(sp, withLater);
    bool sawLater = false, sawPlaceholder = false;
    for (auto& m : sp.mods) { if (m.id == "unknown_later.mod") { sawLater = true; assert(m.enabled); } if (m.id.rfind("local:", 0) == 0 && m.name == "Totally Unknown") sawPlaceholder = true; }
    assert(sawLater && !sawPlaceholder);

    // ---- export rules: enabled mods + uninstalled disabled ghosts; installed disabled mods are left out ----
    Playset p; p.name = "Out";
    p.mods.push_back({"ugc_111.mod", true, "Rise and Fall"});
    p.mods.push_back({"off_installed.mod", false, "Off"});
    p.mods.push_back({"ugc_999.mod", false, "Ghost Off"});
    p.mods.push_back({"local:Placeholder", true, "Placeholder"});
    std::map<std::string, ModInfo> info; ModInfo oi; oi.id = "off_installed.mod"; oi.name = "Off"; info[oi.id] = oi;
    std::string ex = exportLauncherPlayset(p, info);
    assert(ex.find("\"Off\"") == std::string::npos && ex.find("Ghost Off") != std::string::npos && ex.find("Placeholder") != std::string::npos);
    J doc; assert(parseJson(ex, doc) && doc.get("mods")->a.size() == 3);
    assert(doc.get("mods")->a[1].get("enabled")->b == false && !doc.get("mods")->a[2].get("steamId"));

    // ---- playset files are the database ----
    Settings se; loadSettings(se); assert(se.ck3Dir.empty() && se.active.empty());
    se.ck3Dir = dir; se.active = "Vanilla+"; assert(saveSettings(se));
    Settings se2; loadSettings(se2); assert(se2.ck3Dir == dir && se2.active == "Vanilla+" && se2.theme.empty());
    se2.theme = "light"; assert(saveSettings(se2)); Settings se3; loadSettings(se3); assert(se3.theme == "light");
    se3.theme = "bogus"; saveSettings(se3); Settings se4; loadSettings(se4); assert(se4.theme.empty());

    auto all = loadPlaysets(mods); assert(all.empty());
    Playset a = r.playset; a.name = "Vanilla+";
    auto im = infoMap(mods);
    assert(savePlayset(a, im));
    assert(fs::exists(playsetsDir() / "Vanilla+.json"));
    std::string onDisk; readFile(playsetsDir() / "Vanilla+.json", onDisk); assert(onDisk == real);   // file on disk is the launcher's own format
    assert(!fs::exists(playsetsDir() / "Vanilla+.json.tmp"));
    Playset b; b.name = "Second"; b.mods.push_back({"ugc_111.mod", true, ""}); assert(savePlayset(b, im));
    all = loadPlaysets(mods);
    assert(all.size() == 2 && all[0].name == "Second" && all[1].name == "Vanilla+");
    assert(all[0].mods[0].id == "ugc_111.mod" && all[0].mods[0].enabled && all[0].mods.size() == 3);  // + 2 other installed mods, disabled
    // rename keeps contents, name inside matches new file name
    assert(renamePlayset(all[0], "Renamed", im));
    assert(!fs::exists(playsetsDir() / "Second.json") && fs::exists(playsetsDir() / "Renamed.json"));
    std::string rn; readFile(playsetsDir() / "Renamed.json", rn); assert(rn.find("\"name\":\"Renamed\"") != std::string::npos);
    // a file renamed by hand takes the file's name
    fs::rename(playsetsDir() / "Renamed.json", playsetsDir() / "By Hand.json");
    all = loadPlaysets(mods); assert(all.size() == 2 && all[0].name == "By Hand");
    assert(deletePlayset("By Hand") && loadPlaysets(mods).size() == 1);
    writeFile(playsetsDir() / "garbage.json", "not json at all");
    assert(loadPlaysets(mods).size() == 1);                       // unreadable files are ignored, not fatal

    // ---- names ----
    assert(sanitizeFileName("a/b:c*?") == "a_b_c__" && sanitizeFileName("...") == "playset" && sanitizeFileName("  x  ") == "x");
    assert(sanitizeFileName("CON") == "CON_" && sanitizeFileName("com3.txt") == "com3.txt_" && sanitizeFileName("Vanilla+") == "Vanilla+");
    assert(sanitizeFileName(std::string(200, 'a')).size() == 80);
    std::string jp; for (int i = 0; i < 60; i++) jp += "\xC3\xA9"; assert(sanitizeFileName(jp).size() == 80);   // 2-byte chars: cut on a boundary
    assert(fileStem("/tmp/Vanilla+.json") == "Vanilla+" && fileStem("/tmp/My Set.v2.json") == "My Set.v2");
    std::vector<Playset> names = {a}; assert(uniqueName(names, "vanilla+") == "vanilla+ (2)" && uniqueName(names, "Fresh") == "Fresh" && uniqueName(names, "") == "Imported playset");

    // ---- size cap on reads ----
    std::string big(9 << 20, 'x'); writeFile(T / "big.json", big);
    std::string tmp; assert(!readFile(T / "big.json", tmp));
    assert(steamIdOf("ugc_5.mod") == "5" && steamIdOf("ugc_x.mod").empty() && steamIdOf("local.mod").empty());
    assert(!validID("../x.mod") && !validID("a/b.mod") && validID("ugc_1.mod"));

    // ---- one-time migration from the old v0.1-0.3 database ----
    fs::remove_all(T / "data");
    fs::create_directories(T / "data");
    writeFile(T / "data/playsets.json", "{\"active\":\"Imported\",\"ck3Dir\":\"\",\"playsets\":[{\"name\":\"Imported\",\"mods\":["
        "{\"id\":\"ugc_222.mod\",\"enabled\":true,\"name\":\"Sub Rosa\"},{\"id\":\"ugc_111.mod\",\"enabled\":true,\"name\":\"Rise and Fall\"},"
        "{\"id\":\"local.mod\",\"enabled\":false},{\"id\":\"gone.mod\",\"enabled\":true}]},"
        "{\"name\":\"Bad/Name\",\"mods\":[]}]}");
    Settings ms2; assert(migrateLegacyStore(mods, ms2) == 2);
    assert(ms2.active == "Imported" && !fs::exists(T / "data/playsets.json") && fs::exists(T / "data/playsets.json.migrated"));
    auto mig = loadPlaysets(mods);
    assert(mig.size() == 2 && mig[0].name == "Bad_Name" && mig[1].name == "Imported");
    assert(mig[1].mods[0].id == "ugc_222.mod" && mig[1].mods[1].id == "ugc_111.mod" && mig[1].mods[2].id == "gone.mod" && mig[1].mods[2].enabled);   // uninstalled local mod: its file id is kept, so it matches again when it is reinstalled
    assert(migrateLegacyStore(mods, ms2) == 0);                    // never runs twice

    // ---- v0.5.0: starting the game without the launcher ----
    {
        std::string vdf = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t\t\"label\"\t\t\"\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"1158310\"\t\t\"123\"\n\t\t}\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t}\n}\n";
        auto libs = steamLibraryPaths(vdf);
        assert(libs.size() == 2 && libs[0] == "C:\\Program Files (x86)\\Steam" && libs[1] == "D:\\SteamLibrary");
        assert(acfInstallDir("\"AppState\"\n{\n\t\"appid\"\t\t\"1158310\"\n\t\"installdir\"\t\t\"Crusader Kings III\"\n}") == "Crusader Kings III");
        assert(acfInstallDir("\"AppState\" { \"installdir\" \"CK3 Custom\" }") == "CK3 Custom" && acfInstallDir("junk") == "Crusader Kings III");

        fs::remove_all(T / "game"); fs::create_directories(T / "game");
        std::string gdir = (T / "game").string();
        Playset gp; gp.name = "Play me";
        gp.mods.push_back({"ugc_222.mod", true, "Sub Rosa"});
        gp.mods.push_back({"ugc_111.mod", false, "Rise and Fall"});         // disabled: not written
        gp.mods.push_back({"local.mod", true, "My Local Mod"});
        gp.mods.push_back({"ugc_999.mod", true, "Not installed"});          // enabled but missing: skipped
        gp.mods.push_back({"local:Placeholder", true, "Placeholder"});      // placeholder: skipped
        std::set<std::string> instSet; for (auto& m : mods) instSet.insert(m.id);
        // no file yet: created, nothing to keep
        auto w1 = writeGameModList(gdir, gp, instSet);
        assert(w1.ok && w1.written == 2 && w1.skipped == 2 && !fs::exists(T / "game/dlc_load.json.rc-original"));
        std::string out1; readFile(T / "game/dlc_load.json", out1);
        J d1; assert(parseJson(out1, d1) && d1.get("enabled_mods")->a.size() == 2 && d1.get("enabled_mods")->a[0].s == "mod/ugc_222.mod" && d1.get("enabled_mods")->a[1].s == "mod/local.mod" && d1.get("disabled_dlcs"));
        // existing user file with other settings: kept once, other keys preserved, updates do not overwrite the kept copy
        std::string users = "{\"enabled_mods\":[\"mod/their_own.mod\"],\"disabled_dlcs\":[\"dlc001\"],\"extra\":{\"a\":1}}";
        writeFile(T / "game/dlc_load.json", users);
        assert(writeGameModList(gdir, gp, instSet).ok);
        std::string kept; readFile(T / "game/dlc_load.json.rc-original", kept); assert(kept == users);
        std::string out2; readFile(T / "game/dlc_load.json", out2);
        J d2; assert(parseJson(out2, d2) && d2.get("extra") && d2.get("disabled_dlcs")->a.size() == 1 && d2.get("enabled_mods")->a.size() == 2);
        assert(writeGameModList(gdir, gp, instSet).ok);
        readFile(T / "game/dlc_load.json.rc-original", kept); assert(kept == users);   // still the user's original
        assert(!fs::exists(T / "game/dlc_load.json.tmp"));
        // corrupt existing file is replaced, empty playset = no mods, missing folder = clear failure
        writeFile(T / "game/dlc_load.json", "{not json");
        Playset empty; empty.name = "Vanilla";
        auto w3 = writeGameModList(gdir, empty, instSet); assert(w3.ok && w3.written == 0);
        assert(!writeGameModList("", gp, instSet).ok && !writeGameModList((T / "no_such_dir").string(), gp, instSet).ok);
        // settings remember the game path
        Settings gs; gs.gameExe = "C:\\Games\\ck3.exe"; gs.active = "x"; assert(saveSettings(gs));
        Settings gs2; loadSettings(gs2); assert(gs2.gameExe == "C:\\Games\\ck3.exe");
    }
    // ---- v0.5.1: audit fixes ----
    {
        // a descriptor saved with a byte-order mark still gives its real name
        writeFile(T / "ck3/mod/bom.mod", std::string("\xEF\xBB\xBF") + "name=\"Bom Mod\"\nversion=\"2\"\n");
        bool sawBom = false;
        for (auto& m : scanMods(dir)) if (m.id == "bom.mod") { sawBom = true; assert(m.name == "Bom Mod" && m.version == "2"); }
        assert(sawBom);
        fs::remove(T / "ck3/mod/bom.mod");

        // a game mod list we cannot read is never overwritten
        fs::create_directories(T / "game2");
        writeFile(T / "game2/dlc_load.json", std::string(9u << 20, 'x'));
        Playset gp2; gp2.name = "p";
        auto wr = writeGameModList((T / "game2").string(), gp2, {});
        assert(!wr.ok && fs::file_size(T / "game2/dlc_load.json") == (9u << 20) && !fs::exists(T / "game2/dlc_load.json.rc-original"));

        // names also look at the real folder
        fs::remove_all(playsetsDir()); fs::create_directories(playsetsDir());
        writeFile(playsetsDir() / "Zed.json", "{}");
        std::vector<Playset> none;
        assert(nameTaken(none, "Zed") && uniqueName(none, "Zed") == "Zed (2)" && !nameTaken(none, "Other"));

        // renaming that only changes capitals really renames the file and the name inside it
        Playset cv; cv.name = "vanilla"; cv.mods.push_back({"ugc_111.mod", true, "Rise and Fall"});
        auto im2 = infoMap(mods);
        assert(savePlayset(cv, im2));
        assert(!nameTaken({cv}, "Vanilla", "vanilla"));
        assert(renamePlayset(cv, "Vanilla", im2) && cv.name == "Vanilla");
        int spellings = 0;
        for (auto& e : fs::directory_iterator(playsetsDir())) if (lower(e.path().filename().string()) == "vanilla.json") spellings++;
        assert(spellings == 1 && fs::exists(playsetsDir() / "Vanilla.json"));
        std::string inside; readFile(playsetsDir() / "Vanilla.json", inside); assert(inside.find("\"name\":\"Vanilla\"") != std::string::npos);

        // same-named installed mods: the first one wins, matching ignores case
        ModInfo d1; d1.id = "a1.mod"; d1.name = "Same Name";
        ModInfo d2; d2.id = "b2.mod"; d2.name = "same name";
        auto rr = parsePlaysetFile("{\"game\":\"ck3\",\"name\":\"d\",\"mods\":[{\"displayName\":\"SAME NAME\",\"enabled\":true,\"position\":0}]}", {d1, d2});
        assert(rr.ok && rr.playset.mods.size() == 1 && rr.playset.mods[0].id == "a1.mod");

        // many local mods are all matched by name (this used to be quadratic)
        std::vector<ModInfo> many; std::string big = "{\"game\":\"ck3\",\"name\":\"big\",\"mods\":[";
        for (int i = 0; i < 2000; i++) {
            ModInfo mi; mi.id = "local" + std::to_string(i) + ".mod"; mi.name = "Local Mod " + std::to_string(i); many.push_back(mi);
            if (i) big += ',';
            big += "{\"displayName\":\"local mod " + std::to_string(i) + "\",\"enabled\":true,\"position\":" + std::to_string(i) + "}";
        }
        big += "]}";
        auto rb = parsePlaysetFile(big, many);
        assert(rb.ok && rb.playset.mods.size() == 2000 && rb.unmatched == 0 && rb.playset.mods[1999].id == "local1999.mod");
#ifdef __linux__
        assert(!writeFile("/dev/full", "x"));      // a failed final flush is reported, not hidden
#endif
    }
    {   // v0.6.0 game version
        assert(gameVersionFromSettings("{\"rawVersion\":\"1.14.1\",\"version\":\"1.14.1 (Scythe)\"}") == "1.14.1");
        assert(gameVersionFromSettings("{\"version\":\"v1.14.1.2 (Scythe)\"}") == "1.14.1.2");
        assert(gameVersionFromSettings("{\"rawVersion\":5}") == "" && gameVersionFromSettings("garbage") == "");
        assert(matchGameVersion("1.14.*", "1.14.1") == VerMatch::Match);
        assert(matchGameVersion("\"v1.14.*\"", "1.14.1") == VerMatch::Match);
        assert(matchGameVersion("1.13.*", "1.14.1") == VerMatch::Mismatch);
        assert(matchGameVersion("1.14", "1.14.3") == VerMatch::Match);
        assert(matchGameVersion("1.14.2", "1.14.3") == VerMatch::Match);
        assert(matchGameVersion("1.19.0", "1.20.0") == VerMatch::Mismatch && matchGameVersion("2.0", "1.20.0") == VerMatch::Mismatch);
        assert(matchGameVersion("1.14.3.1", "1.14.3") == VerMatch::Match);
        assert(matchGameVersion("1.20.0.3", "1.20.0.4") == VerMatch::Match && matchGameVersion("1.*.*", "1.20.0.4") == VerMatch::Match && matchGameVersion("1.20.*.*", "1.20.0.4") == VerMatch::Match);
        assert(matchGameVersion("1.*", "1.14.3") == VerMatch::Match);
        assert(matchGameVersion("1.1*", "1.14.3") == VerMatch::Unknown);
        assert(matchGameVersion("", "1.14.3") == VerMatch::Unknown && matchGameVersion("1.14.*", "") == VerMatch::Unknown);
        assert(matchGameVersion("1.014.*", "1.14.0") == VerMatch::Match);
        assert(matchGameVersion("99999999999.*", "1.0") == VerMatch::Mismatch);
        fs::path g = fs::temp_directory_path() / "rc_gv_test";
        fs::remove_all(g);
        fs::create_directories(g / "binaries"); fs::create_directories(g / "launcher");
        writeFile(g / "launcher" / "launcher-settings.json", "{\"rawVersion\":\"1.15.0\"}");
        assert(readGameVersion((g / "binaries" / "ck3.exe").u8string()) == "1.15.0");
        assert(readGameVersion((g / "nope" / "x" / "ck3.exe").u8string()).empty() && readGameVersion("").empty());
        fs::remove_all(g);
    }
    {   // v0.6.1 saves scan
        fs::path d = fs::temp_directory_path() / "rc_save_test";
        fs::remove_all(d);
        assert(scanSaves((d).u8string()).entries.empty() && scanSaves("").entries.empty());
        fs::create_directories(d / "save games" / "sub");
        writeFile(d / "save games" / "a.ck3", "12345"); writeFile(d / "save games" / "sub" / "b.ck3", "123");
        auto sc = scanSaves(d.u8string());
        assert(sc.entries.size() == 2 && sc.files == 2 && sc.bytes == 8);
        fs::remove_all(d);
    }
    {   // v0.7.0 descriptor reader, checks, natural sort
        std::string d = "\xEF\xBB\xBF# comment\r\nversion=\"1.4\"\r\ntags={\r\n\t\"Gameplay\"\r\n\t\"Balance\"\r\n}\r\nname=\"Foo \\\"Bar\\\"\"\r\nsupported_version=\"1.20.*\"\r\n"
                    "path=\"mod/foo\"\ndependencies = { \"Dep One\" \"Dep Two\" } # trailing\nreplace_path=\"history/titles\"\nreplace_path=\"common/traits\"\nremote_file_id=\"123\"\nempty={ }\nnested={ a={ x } }\n";
        auto kv = parseDescriptor(d);
        assert(kv["name"].size() == 1 && kv["name"][0] == "Foo \"Bar\"");
        assert(kv["version"][0] == "1.4" && kv["supported_version"][0] == "1.20.*" && kv["path"][0] == "mod/foo");
        assert(kv["tags"].size() == 2 && kv["tags"][1] == "Balance");
        assert(kv["dependencies"].size() == 2 && kv["dependencies"][1] == "Dep Two");
        assert(kv["replace_path"].size() == 2 && kv["replace_path"][1] == "common/traits");
        assert(kv["empty"].empty() && kv["nested"].size() == 1 && kv["nested"][0] == "a");
        assert(parseDescriptor("").empty());
        parseDescriptor("name=\"unterminated\nversion=");  // garbage must not crash
        parseDescriptor("= = { } } { name");
        parseDescriptor(std::string(1000, '{'));
        parseDescriptor(std::string("a\0b=\"c\"", 8));

        // scanMods fills the new fields and resolves the content folder
        fs::path root = fs::temp_directory_path() / "rc_desc_test";
        fs::remove_all(root);
        fs::create_directories(root / "mod" / "foo");
        writeFile(root / "mod" / "foo.mod", d);
        writeFile(root / "mod" / "gone.mod", "name=\"Gone\"\npath=\"mod/nothere\"\n");
        writeFile(root / "mod" / "arch.mod", "name=\"Arch\"\narchive=\"x.zip\"\n");
        auto mods = scanMods(root.u8string());
        assert(mods.size() == 3);
        std::map<std::string, ModInfo> inf = infoMap(mods);
        assert(inf["foo.mod"].contentState == 1 && inf["gone.mod"].contentState == 2 && inf["arch.mod"].contentState == 0);
        assert(inf["foo.mod"].deps.size() == 2 && inf["foo.mod"].replacePaths.size() == 2 && inf["foo.mod"].tags.size() == 2);
        fs::remove_all(root);

        // analyzePlayset
        auto mk = [](const char* id, const char* name, std::vector<std::string> deps = {}, const char* sup = "") {
            ModInfo m; m.id = id; m.name = name; m.deps = deps; m.supported = sup; return m; };
        std::vector<ModInfo> im = {mk("a.mod", "Alpha", {"Lib", "Missing", "Off"}), mk("lib.mod", "Lib"), mk("off.mod", "Off"), mk("late.mod", "Late", {"Alpha"}), mk("d1.mod", "Dup"), mk("d2.mod", "dup", {}, "1.19.*")};
        im[1].replacePaths = {"history/x"};
        auto info2 = infoMap(im);
        Playset p; p.name = "t";
        p.mods = {{"late.mod", true, ""}, {"a.mod", true, ""}, {"lib.mod", true, ""}, {"off.mod", false, ""}, {"d1.mod", true, ""}, {"d2.mod", true, ""}, {"ghost.mod", true, ""}, {"ghost2.mod", false, ""}};
        auto r = analyzePlayset(p, info2, "1.20.0.4");
        auto has = [&](size_t i, const std::string& sub) { for (auto& x : r[i]) if (x.text.find(sub) != std::string::npos) return true; return false; };
        assert(has(0, "should load before"));                       // Late needs Alpha, which is below it
        assert(has(1, "\"Missing\" (not installed)") && has(1, "\"Off\" (installed, not enabled)") && has(1, "\"Lib\" should load before"));
        assert(r[2].size() == 1 && r[2][0].sev == 0);               // replace_path info only
        assert(r[3].empty());                                       // disabled mod
        assert(has(4, "Same name as enabled mod #6") && has(5, "Same name as enabled mod #5") && has(5, "Made for game 1.19.*"));
        assert(r[6].size() == 1 && r[6][0].sev == 2 && r[7].size() == 1 && r[7][0].sev == 0);
        assert(issueSeverity(r[1]) == 2 && issueSeverity(r[2]) == 0 && issueSummary(r[2]).empty() && !issueSummary(r[1]).empty());
        assert(issueSummary(r[1]).find("(+") != std::string::npos);

        assert(naturalCompare("1.9", "1.10") < 0 && naturalCompare("1.10", "1.9") > 0 && naturalCompare("a", "A") == 0);
        assert(naturalCompare("v0.6", "1.0") > 0 && naturalCompare("", "x") < 0 && naturalCompare("007", "7") == 0 && naturalCompare("1.20.*", "1.20.0") != 0);
    }
    {   // v0.9.0 conflict finder
        fs::path root = fs::temp_directory_path() / "rc_conf_test";
        fs::remove_all(root);
        for (const char* f : {"common/traits/a.txt", "Events/B.TXT", ".git/config", "gfx/x/y.dds", "descriptor.mod", "thumbnail.png", "localization/english/l.yml"}) {
            fs::create_directories((root / f).parent_path());
            writeFile(root / f, "x");
        }
        auto mf = indexModFiles(root.u8string());
        assert(mf.complete && mf.files.size() == 4);
        assert(std::find(mf.files.begin(), mf.files.end(), "events/b.txt") != mf.files.end());
        assert(std::find(mf.files.begin(), mf.files.end(), ".git/config") == mf.files.end());
        std::atomic<bool> stop(true);
        assert(!indexModFiles(root.u8string(), &stop).complete);
        assert(!indexModFiles((root / "nope").u8string()).complete || indexModFiles((root / "nope").u8string()).files.empty());
        fs::remove_all(root);
        assert(areaOf("common/traits/x.txt") == "common/traits" && areaOf("events/x.txt") == "events" && areaOf("x") == "x");

        auto mk = [](const char* id, const char* name, std::vector<std::string> rp = {}) { ModInfo m; m.id = id; m.name = name; m.replacePaths = rp; return m; };
        std::vector<ModInfo> im = {mk("a.mod", "A"), mk("b.mod", "B"), mk("c.mod", "C", {"common/traits"}), mk("d.mod", "D")};
        auto info = infoMap(im);
        std::map<std::string, ModFiles> idx;
        idx["a.mod"] = {"", {"common/traits/t.txt", "events/e.txt", "gfx/g.dds", "common/only_a.txt"}, true};
        idx["b.mod"] = {"", {"common/traits/t.txt", "events/e.txt", "gfx/gb.dds"}, true};
        idx["c.mod"] = {"", {"common/traits/new.txt", "events/e.txt"}, true};
        idx["d.mod"] = {"", {"x/y.txt"}, false};      // not fully indexed
        Playset p; p.name = "t"; p.mods = {{"a.mod", true, ""}, {"b.mod", true, ""}, {"c.mod", true, ""}, {"d.mod", true, ""}, {"gone.mod", true, ""}};
        auto cr = findConflicts(p, info, idx);
        assert(cr.valid && cr.modsIndexed == 3 && cr.modsWithoutFiles == 1);
        assert(cr.files.size() == 2);                                     // traits/t.txt (a,b) and events/e.txt (a,b,c)
        assert(cr.files[0].path == "common/traits/t.txt" && cr.files[0].mods == std::vector<int>({0, 1}));
        assert(cr.files[1].path == "events/e.txt" && cr.files[1].mods == std::vector<int>({0, 1, 2}));
        assert(cr.wins[1] == 1 && cr.wins[2] == 1 && cr.loses[0] == 2 && cr.loses[1] == 1 && cr.loses[2] == 0);
        assert(cr.pairs.size() == 3 && cr.pairs[0].a == 0 && cr.pairs[0].b == 1 && cr.pairs[0].count == 2);   // a vs b share two files
        assert(cr.pairs[0].areas.size() == 2);
        assert(cr.wipes.size() == 2 && cr.wipes[0].replacer == 2 && cr.wipes[0].folder == "common/traits");
        assert((cr.wipes[0].victim == 0 || cr.wipes[0].victim == 1) && cr.wipes[0].files == 1);               // a and b each had 1 file under common/traits
        auto iss = analyzePlayset(p, info, "");
        addConflictIssues(iss, cr, p, info);
        assert(issueSeverity(iss[2]) == 1 && issueSeverity(iss[0]) == 1 && issueSummary(iss[3]).empty());
        // disabled mods take no part
        p.mods[1].enabled = false;
        auto cr2 = findConflicts(p, info, idx);
        assert(cr2.files.size() == 1 && cr2.files[0].mods == std::vector<int>({0, 2}));
        // many mods on one file: loser-vs-winner pairs only
        std::vector<ModInfo> big; std::map<std::string, ModFiles> bi; Playset bp; bp.name = "b";
        for (int i = 0; i < 20; i++) { std::string id = "m" + std::to_string(i) + ".mod"; big.push_back(mk(id.c_str(), id.c_str())); bi[id] = {"", {"common/x.txt"}, true}; bp.mods.push_back({id, true, ""}); }
        auto cr3 = findConflicts(bp, infoMap(big), bi);
        assert(cr3.files.size() == 1 && cr3.pairs.size() == 19 && cr3.wins[19] == 1 && cr3.loses[0] == 1);
    }
    {   // ---- auto sort ----
        auto mkm = [](const char* id, const char* name, std::vector<std::string> deps = {}, std::vector<std::string> tags = {}) {
            ModInfo m; m.id = id; m.name = name; m.deps = deps; m.tags = tags; return m;
        };
        std::vector<ModInfo> ms = {
            mkm("p.mod", "Big Content Patch"), mkm("c1.mod", "Cool Events", {"Lib Core Framework"}), mkm("ui.mod", "Better UI"),
            mkm("lib.mod", "Lib Core Framework"), mkm("g.mod", "Nice Portraits"), mkm("t.mod", "Foo", {}, {"Translation"}),
            mkm("c2.mod", "More Stuff")};
        auto inf = infoMap(ms);
        Playset ps; ps.name = "s";
        for (auto& m : ms) ps.mods.push_back({m.id, true, ""});
        assert(guessCategory(inf["p.mod"]).cat == CAT_PATCH && guessCategory(inf["lib.mod"]).cat == CAT_LIBRARY);
        assert(guessCategory(inf["ui.mod"]).cat == CAT_UI && guessCategory(inf["g.mod"]).cat == CAT_GRAPHICS && guessCategory(inf["t.mod"]).cat == CAT_TRANSLATION);
        assert(guessCategory(inf["c1.mod"]).cat == CAT_CONTENT && guessCategory(mkm("x", "Compatibility for X")).cat == CAT_PATCH);
        assert(guessCategory(mkm("x", "Quiet Hill")).cat == CAT_CONTENT);   // "ui" inside a word must not match
        auto plan = planSort(ps, inf, {}, {}, nullptr, nullptr);
        std::vector<std::string> ids;
        for (int o : plan.order) ids.push_back(ps.mods[(size_t)o].id);
        assert((ids == std::vector<std::string>{"lib.mod", "c1.mod", "c2.mod", "g.mod", "ui.mod", "t.mod", "p.mod"}));
        assert(plan.changed && plan.warnings.empty() && !plan.moves.empty());
        // sorting an already sorted playset changes nothing
        Playset sorted; sorted.name = "s2";
        for (auto& id : ids) sorted.mods.push_back({id, true, ""});
        assert(!planSort(sorted, inf, {}, {}, nullptr, nullptr).changed);
        // a locked mod keeps its position; the rest still sorts around it
        auto pl = planSort(ps, inf, {"p.mod"}, {}, nullptr, nullptr);
        assert(pl.order[0] == 0 && pl.changed);
        // override moves a mod to another group
        auto po = planSort(ps, inf, {}, {{"c2.mod", CAT_LIBRARY}}, nullptr, nullptr);
        assert(ps.mods[(size_t)po.order[0]].id == "c2.mod" || ps.mods[(size_t)po.order[1]].id == "c2.mod");
        // dependency on a LATER, lower-priority mod still ends up in order; cycles do not hang
        std::vector<ModInfo> cy = {mkm("a.mod", "A", {"B"}), mkm("b.mod", "B", {"A"}), mkm("c.mod", "C", {"C"})};
        Playset cp; cp.name = "cy"; for (auto& m : cy) cp.mods.push_back({m.id, true, ""});
        auto pc = planSort(cp, infoMap(cy), {}, {}, nullptr, nullptr);
        assert(pc.order.size() == 3 && !pc.warnings.empty());
        std::set<int> seen(pc.order.begin(), pc.order.end()); assert(seen.size() == 3);
        // locked mod that blocks a dependency is reported
        std::vector<ModInfo> lk = {mkm("x.mod", "X", {"Y"}), mkm("y.mod", "Y")};
        Playset lp; lp.name = "lk"; for (auto& m : lk) lp.mods.push_back({m.id, true, ""});
        auto plk = planSort(lp, infoMap(lk), {"x.mod"}, {}, nullptr, nullptr);
        assert(!plk.warnings.empty());
        // conflict tie-break: small targeted mod goes after the big one it overlaps
        std::vector<ModInfo> cf = {mkm("small.mod", "Small Tweaks"), mkm("big.mod", "Big Pack Stuff")};
        Playset fp; fp.name = "cf"; for (auto& m : cf) fp.mods.push_back({m.id, true, ""});
        std::map<std::string, ModFiles> fi;
        ModFiles sm; sm.complete = true; sm.files = {"common/a.txt", "common/b.txt", "common/c.txt"};
        ModFiles bg; bg.complete = true; bg.files = {"common/a.txt", "common/b.txt", "common/c.txt"};
        for (int i = 0; i < 20; i++) bg.files.push_back("events/e" + std::to_string(i) + ".txt");
        fi["small.mod"] = sm; fi["big.mod"] = bg;
        auto fcr = findConflicts(fp, infoMap(cf), fi);
        auto pf = planSort(fp, infoMap(cf), {}, {}, &fcr, &fi);
        assert(pf.conflictChoices == 1 && fp.mods[(size_t)pf.order[1]].id == "small.mod");
        // settings round trip for locks and categories
        Settings st; st.ck3Dir = "x"; st.locks["Default"] = {"a.mod", "b.mod"}; st.cats["a.mod"] = CAT_PATCH;
        setenv("RC_DATA_DIR", "/tmp/rc_sort_settings", 1);
        assert(saveSettings(st));
        Settings back; loadSettings(back);
        assert(back.locks["Default"].size() == 2 && back.cats["a.mod"] == CAT_PATCH);
    }
    {   // ---- known mods, patch links, file-based types ----
        assert(normName("Rise & Fall [1.17] v2.3") == "rise and fall" && normName("Better Barbershop Mod 1.2.3") == "better barbershop mod");
        assert(nameIs("rise and fall", "rise and fall") && nameIs("rise and fall extended", "rise and fall"));
        assert(!nameIs("rise and fall compatibility patch", "rise and fall") && !nameIs("rise and fall of rome", "rise and fall"));
        assert(acronymOf("community flavor pack") == "cfp" && acronymOf("ethnicities and portraits expanded") == "epe" && acronymOf("two words").empty());
        auto mkm = [](const char* id, const char* name, std::vector<std::string> deps = {}, std::vector<std::string> tags = {}) {
            ModInfo m; m.id = id; m.name = name; m.deps = deps; m.tags = tags; return m;
        };
        std::vector<ModInfo> ms = {
            mkm("ugc_3554844335.mod", "Anything at all"),                         // Rise and Fall recognised by its Workshop id
            mkm("rui.mod", "RUI"), mkm("x1.mod", "Some Content"),
            mkm("bb.mod", "Better Barbershop"), mkm("cfp.mod", "Community Flavor Pack"), mkm("epe.mod", "Ethnicities & Portraits Expanded"),
            mkm("up.mod", "Unofficial Patch"), mkm("x2.mod", "More Content"),
            mkm("pt.mod", "CFP + Some Content compatibility patch"), mkm("rf2.mod", "Rise and Fall Compatibility Patch")};
        auto inf = infoMap(ms);
        Playset ps; ps.name = "k";
        for (auto& m : ms) ps.mods.push_back({m.id, true, ""});
        auto db = builtinKnownMods();
        auto plan = planSort(ps, inf, {}, {}, nullptr, nullptr, &db);
        std::vector<std::string> ids; for (int o : plan.order) ids.push_back(ps.mods[(size_t)o].id);
        auto pos = [&](const char* id) { return (int)(std::find(ids.begin(), ids.end(), id) - ids.begin()); };
        assert(pos("up.mod") == 0);                                            // "must be first"
        assert(pos("ugc_3554844335.mod") > pos("pt.mod") && pos("ugc_3554844335.mod") > pos("rf2.mod") && pos("ugc_3554844335.mod") > pos("x2.mod"));   // very bottom...
        assert(pos("rui.mod") == pos("ugc_3554844335.mod") + 1 && pos("rui.mod") == (int)ids.size() - 1);       // ...except RUI, which goes below it
        assert(pos("bb.mod") > pos("cfp.mod") && pos("bb.mod") > pos("epe.mod"));                         // barbershop below CFP and EPE
        assert(pos("pt.mod") > pos("cfp.mod") && pos("pt.mod") > pos("x1.mod") && plan.patchLinks >= 2);   // patch named for CFP and "Some Content"
        assert(plan.knownCount >= 6);
        // the user can still lock a known mod
        auto pl = planSort(ps, inf, {"ugc_3554844335.mod"}, {}, nullptr, nullptr, &db);
        assert(pl.order[0] == 0);
        // without the database nothing changes for them
        auto none = planSort(ps, inf, {}, {}, nullptr, nullptr, nullptr);
        assert(none.knownCount == 0);
        // files tell what an unnamed mod is
        ModFiles gf; gf.complete = true; for (int i = 0; i < 20; i++) gf.files.push_back("gfx/portraits/a" + std::to_string(i) + ".dds");
        assert(guessFromFiles(gf).cat == CAT_GRAPHICS);
        ModFiles lf; lf.complete = true; for (int i = 0; i < 20; i++) lf.files.push_back("localization/english/a" + std::to_string(i) + ".yml");
        assert(guessFromFiles(lf).cat == CAT_TRANSLATION);
        ModFiles uf; uf.complete = true; for (int i = 0; i < 20; i++) uf.files.push_back("gui/a" + std::to_string(i) + ".gui");
        assert(guessFromFiles(uf).cat == CAT_UI);
        ModFiles cf; cf.complete = true; cf.files = {"common/traits/a.txt", "events/b.txt", "gfx/x.dds"};
        assert(guessFromFiles(cf).cat == CAT_CONTENT);
        // user database file adds and overrides entries
        setenv("RC_DATA_DIR", "/tmp/rc_known", 1);
        fs::create_directories("/tmp/rc_known");
        writeFile("/tmp/rc_known/knownmods.json", R"({"mods":[{"names":["My Cool Mod"],"type":"Graphics","position":"last","note":"mine"},{"names":["Rise and Fall"],"position":"first","note":"changed"}]})");
        auto udb = loadKnownMods();
        int mine = -1, rf = -1;
        for (size_t i = 0; i < udb.size(); i++) { for (auto& nm : udb[i].names) { if (nm == "my cool mod") mine = (int)i; if (nm == "rise and fall") rf = (int)i; } }
        assert(mine >= 0 && udb[(size_t)mine].cat == CAT_GRAPHICS && udb[(size_t)mine].pos == 1);
        assert(rf >= 0 && udb[(size_t)rf].pos == -1 && udb[(size_t)rf].ids.empty());                       // replaced the built-in entry
        writeFile("/tmp/rc_known/knownmods.json", "{not json");
        assert(loadKnownMods().size() == builtinKnownMods().size());                                         // a broken file is ignored
        // downloaded list: layered between the built-in list and the user's file
        fs::remove("/tmp/rc_known/knownmods.json");
        std::string online = R"({"format":1,"revision":4,"mods":[{"names":["Online Mod"],"type":"Interface","position":"first","note":"from github"},{"names":["Rise and Fall"],"position":"last","note":"online"}]})";
        assert(checkKnownOnline(online).ok && checkKnownOnline(online).revision == 4 && checkKnownOnline(online).count == 2);
        assert(!checkKnownOnline("").ok && !checkKnownOnline("{not json").ok);
        assert(!checkKnownOnline(R"({"format":2,"mods":[]})").ok && !checkKnownOnline(R"({"mods":[]})").ok && !checkKnownOnline(R"({"format":1})").ok);
        assert(!checkKnownOnline(std::string(600 * 1024, ' ')).ok);
        writeFile(knownOnlineFile(), online);
        int rev = -1;
        auto odb = loadKnownMods(&rev);
        assert(rev == 4);
        int om = -1; for (size_t i = 0; i < odb.size(); i++) for (auto& nm : odb[i].names) if (nm == "online mod") om = (int)i;
        assert(om >= 0 && odb[(size_t)om].cat == CAT_UI && odb[(size_t)om].pos == -1);
        // the user's file wins over the downloaded one
        writeFile("/tmp/rc_known/knownmods.json", R"({"mods":[{"names":["Rise and Fall"],"position":"top","note":"user"}]})");
        auto both = loadKnownMods(&rev);
        int rf2 = -1; for (size_t i = 0; i < both.size(); i++) for (auto& nm : both[i].names) if (nm == "rise and fall") rf2 = (int)i;
        assert(rf2 >= 0 && both[(size_t)rf2].note == "user" && both[(size_t)rf2].pos == -2);
        // a broken downloaded file is ignored
        fs::remove("/tmp/rc_known/knownmods.json");
        writeFile(knownOnlineFile(), "{\"format\":1,\"mods\":\"x\"}");
        assert(loadKnownMods(&rev).size() == builtinKnownMods().size() && rev == 0);
        fs::remove(knownOnlineFile());
        // the list shipped in the repository must always be one the program accepts
        for (const char* path : {"knownmods.json", "../knownmods.json", "../../knownmods.json"}) {
            std::string shipped;
            if (readFile(path, shipped)) { assert(checkKnownOnline(shipped).ok && checkKnownOnline(shipped).revision >= 1); break; }
        }
        // sort report: names, ids, types and flags only
        {
            ModInfo a; a.id = "ugc_3554844335.mod"; a.name = "Rise and Fall"; a.path = "C:/secret/path";
            ModInfo b; b.id = "mod/local.mod"; b.name = "Local Mod"; b.path = "D:/private/dir";
            auto inf2 = infoMap({a, b});
            Playset rp; rp.name = "Test Set"; rp.mods = {{a.id, true, a.name}, {b.id, false, b.name}, {"ugc_999.mod", true, "Gone Mod"}};
            std::string rep = buildSortReport(rp, inf2, builtinKnownMods(), {CAT_CONTENT, CAT_GRAPHICS, CAT_CONTENT}, {b.id}, "1.20.0.4", 4);
            assert(rep.find("Program ") != std::string::npos && rep.find("1.20.0.4") != std::string::npos && rep.find("online revision 4") != std::string::npos);
            assert(rep.find("1. [on] Rise and Fall (Steam 3554844335) | type Content | known mod") != std::string::npos);
            assert(rep.find("2. [off] Local Mod | type Graphics | locked") != std::string::npos);
            assert(rep.find("3. [on] Gone Mod (Steam 999)") != std::string::npos && rep.find("not installed") != std::string::npos);
            assert(rep.find("secret") == std::string::npos && rep.find("private") == std::string::npos);
        }
    }
    {   // ---- backups ----
        setenv("RC_DATA_DIR", "/tmp/rc_backup_test", 1);
        std::error_code ec; fs::remove_all("/tmp/rc_backup_test", ec);
        std::vector<ModInfo> ms;
        for (int i = 0; i < 4; i++) { ModInfo m; m.id = "m" + std::to_string(i) + ".mod"; m.name = "Mod " + std::to_string(i); ms.push_back(m); }
        auto inf = infoMap(ms);
        Playset ps; ps.name = "My Set";
        for (auto& m : ms) ps.mods.push_back({m.id, true, m.name});
        std::string b1 = backupPlayset(ps, inf, "before sort");
        assert(!b1.empty() && fs::exists(b1) && listBackups("My Set").size() == 1);
        assert(backupPlayset(ps, inf, "again").empty());                       // identical to the newest: not duplicated
        Playset changed = ps; std::swap(changed.mods[0], changed.mods[3]); changed.mods[1].enabled = false;
        std::string b2 = backupPlayset(changed, inf, "x");
        assert(!b2.empty() && listBackups("My Set").size() == 2 && listBackups("My Set")[0].u8string() == b2);   // newest first
        // restoring: read the old backup, apply its order and enabled flags, keep mods it did not know
        std::string text; assert(readFile(P(b1), text));
        auto imp = parsePlaysetFile(text, ms); assert(imp.ok);
        Playset now = changed; ModRef extra{"new.mod", false, "New"}; now.mods.push_back(extra);
        applyBackupOrder(now, imp.playset);
        assert(now.mods.size() == 5 && now.mods[0].id == "m0.mod" && now.mods[3].id == "m3.mod" && now.mods[1].enabled && now.mods[4].id == "new.mod");
        // only the newest N are kept
        std::string lastExpected;
        for (int i = 0; i < 6; i++) { Playset p2 = ps; p2.mods[0].enabled = (i % 2) == 0; p2.mods[1].enabled = i < 3; p2.mods[2].enabled = i < 4; p2.mods[3].enabled = i < 5;
            backupPlayset(p2, inf, "n", 3); lastExpected = exportLauncherPlayset(p2, inf); }
        assert(listBackups("My Set").size() == 3);
        std::string lastData, newest; auto lb = listBackups("My Set");
        readFile(lb.front(), lastData);
        assert(lastData == lastExpected);                      // the newest backup survived the pruning
    }
    {   // ---- script-level conflicts ----
        // parser
        std::vector<ScriptKey> k;
        scanScriptKeys("\xEF\xBB\xBF# comment\n@my_var = 5\nbrave = { # trailing\n  index = 1\n  opposites = { craven }\n}\nnamespace = x\ncraven={ a = \"}{\" }\n  \"quoted\" = { }\nmulti_line\n=\n{\n}\n", 0, k);
        assert(k.size() == 4 && k[0].key == "brave" && k[1].key == "craven" && k[2].key == "quoted" && k[3].key == "multi_line");
        k.clear(); scanScriptKeys("a = 1\nb = { }\n@c = 2\nd ?= 3\n", 1, k);
        assert(k.size() == 3 && k[0].key == "a" && k[1].key == "b" && k[2].key == "d");
        k.clear(); scanScriptKeys("on_birth = {\n  effect = { x = 1 }\n  events = { a.1 }\n}\non_death = { events = { a.2 } }\n", 0, k);
        assert(k.size() == 2 && (k[0].flags & DF_OVERWRITES) && !(k[1].flags & DF_OVERWRITES));
        k.clear(); scanScriptKeys("my.1 = { id_override_priority = 5 type = character_event }\nmy.2 = { }\n", 0, k);
        assert(k.size() == 2 && (k[0].flags & DF_PRIORITY) && !(k[1].flags & DF_PRIORITY));
        k.clear(); scanScriptKeys("NGame = {\n  START_DATE = \"1066.9.15\"\n  LIST = { 1 2 3 }\n}\nNCharacter = { MAX = 5 }\n", 2, k);
        assert(k.size() == 3 && k[0].key == "NGame.START_DATE" && k[1].key == "NGame.LIST" && k[2].key == "NCharacter.MAX");
        std::vector<std::string> lk;
        scanLocKeys("\xEF\xBB\xBFl_english:\n # c\n trait_brave:0 \"Brave\"\n trait_x: \"Y\"\n bad key:0 \"no\"\n event.1.t:1 \"T\"\n", lk);
        assert(lk.size() == 3 && lk[0] == "trait_brave" && lk[1] == "trait_x" && lk[2] == "event.1.t");
        assert(defFileKind("localization/english/a_l_english.yml").area == "localization/english" && !(defFileKind("localization/english/a_l_english.yml").flags & DF_REPLACE));
        assert((defFileKind("localization/english/replace/a.yml").flags & DF_REPLACE) && (defFileKind("localization/replace/english/a.yml").flags & DF_REPLACE));
        assert(defFileKind("common/traits/a.txt").area == "common/traits" && defFileKind("common/defines/a.txt").kind == DK_DEFINE && defFileKind("gfx/a.txt").kind < 0);
        // end to end on disk: three mods
        fs::path root = "/tmp/rc_script_test"; std::error_code ec; fs::remove_all(root, ec);
        auto put = [&](const char* mod, const char* rel, const std::string& text) { fs::path f = root / mod / rel; fs::create_directories(f.parent_path(), ec); writeFile(f, text); };
        put("a", "common/traits/00_a.txt", "brave = { x = 1 }\nonly_a = { }\n");
        put("b", "common/traits/zz_b.txt", "brave = { x = 2 }\n");                     // same key, later file name AND later mod: clear winner
        put("a", "events/a_events.txt", "namespace = e\ne.1 = { }\ne.2 = { }\n");
        put("b", "events/b_events.txt", "namespace = e\ne.1 = { }\ne.3 = { id_override_priority = 3 }\n");   // e.1 duplicated
        put("c", "events/c_events.txt", "namespace = e\ne.3 = { }\n");                  // e.3 has an override priority in b: intended
        put("a", "localization/english/a_l_english.yml", "l_english:\n k1:0 \"a\"\n k2:0 \"a\"\n");
        put("b", "localization/english/b_l_english.yml", "l_english:\n k1:0 \"b\"\n");     // k1 duplicated
        put("c", "localization/english/replace/c_l_english.yml", "l_english:\n k2:0 \"c\"\n");   // replace folder: intended
        put("a", "common/on_action/a.txt", "on_birth = { effect = { a = 1 } }\non_x = { events = { e.1 } }\n");
        put("b", "common/on_action/b.txt", "on_birth = { effect = { b = 1 } }\non_x = { events = { e.2 } }\n");   // both set effect: conflict; on_x merges
        put("c", "common/traits/00_shared.txt", "shared = { }\n");
        put("a", "common/traits/00_shared.txt", "shared = { }\n");              // same FILE in two mods: a file conflict, not a script conflict
        put("a", "common/traits/00_x.txt", "shadow = { }\n");
        put("b", "common/traits/zz_y.txt", "shadow = { }\n");
        put("c", "common/traits/00_x.txt", "shadow = { }\n");                   // replaces a's whole 00_x.txt: only b and c remain
        put("a", "common/traits/zz_a.txt", "unclear = { }\n");                         // load order says b, file name order says "zz_a" vs "00_b" -> a... see below
        put("b", "common/traits/00_b.txt", "unclear = { }\n");
        std::vector<ModInfo> ms; std::vector<std::string> ids = {"a", "b", "c"};
        for (auto& id : ids) { ModInfo m; m.id = id + ".mod"; m.name = id; m.contentDir = (root / id).u8string(); m.contentState = 1; ms.push_back(m); }
        auto inf = infoMap(ms);
        Playset ps; ps.name = "s"; for (auto& m : ms) ps.mods.push_back({m.id, true, ""});
        std::map<std::string, ModDefs> idx;
        for (auto& m : ms) { ModFiles mf = indexModFiles(m.contentDir); assert(mf.complete); idx[m.id] = indexModDefs(m.contentDir, mf.files); assert(idx[m.id].complete); }
        auto sr = findScriptConflicts(ps, inf, idx);
        assert(sr.valid && sr.modsRead == 3);
        auto find = [&](int kind, const char* key) -> const ScriptConflict* { for (auto& c : sr.items) if (c.kind == kind && c.key == key) return &c; return nullptr; };
        auto* brave = find(DK_COMMON, "brave");
        assert(brave && brave->hits.size() == 2 && brave->winner == 1 && brave->sev == 0);      // a/00_a.txt vs b/zz_b.txt: later file name and later mod
        assert(!find(DK_COMMON, "shared"));
        auto* shadow = find(DK_COMMON, "shadow");
        assert(shadow && shadow->hits.size() == 2 && shadow->hits[0].mod == 1 && shadow->hits[1].mod == 2 && shadow->winner == -1);   // file name order says b, load order says c
        assert(find(DK_EVENT, "e.1") && find(DK_EVENT, "e.1")->sev == 1);
        assert(!find(DK_EVENT, "e.3") && !find(DK_EVENT, "e.2"));
        auto* k1 = find(DK_LOC, "k1"); assert(k1 && k1->hits.size() == 2);
        assert(!find(DK_LOC, "k2"));
        assert(find(DK_ONACTION, "on_birth") && !find(DK_ONACTION, "on_x"));
        auto* unc = find(DK_COMMON, "unclear");
        assert(unc && unc->winner == -1 && unc->sev == 1);                                       // file name order: zz_a (mod a); load order: mod b
        assert(!find(DK_COMMON, "only_a"));
        auto iss = analyzePlayset(ps, inf, ""); addScriptIssues(iss, sr, ps);
        assert(issueSeverity(iss[0]) == 1 && issueSeverity(iss[1]) == 1 && issueSummary(iss[2]).find("event") == std::string::npos);
        // disabled mods take no part; incomplete data is reported, not guessed
        ps.mods[1].enabled = false;
        auto sr2 = findScriptConflicts(ps, inf, idx);
        assert(sr2.items.size() < sr.items.size()); for (auto& c : sr2.items) for (auto& h : c.hits) assert(h.mod != 1);
        idx.erase("c.mod"); auto sr3 = findScriptConflicts(ps, inf, idx); assert(sr3.modsMissing == 1);
    }
    {   // v0.13: settings extras, playset comparison, update detection
        setenv("RC_DATA_DIR", "/tmp/rc_extra", 1);
        std::filesystem::remove_all("/tmp/rc_extra");
        Settings st; st.hidden = {"x.mod", "y.mod"}; st.launch["Main"] = "-debug_mode"; st.seen["a.mod"] = "fp1";
        st.winX = 10; st.winY = 20; st.winW = 1300; st.winH = 700; st.winMax = true; st.colW = {64, 0, 100, 110, 90, 90, 0};
        assert(saveSettings(st));
        Settings b; loadSettings(b);
        assert(b.hidden == st.hidden && b.launch == st.launch && b.seen == st.seen);
        assert(b.winX == 10 && b.winY == 20 && b.winW == 1300 && b.winH == 700 && b.winMax && b.colW == st.colW);
        Settings e; std::filesystem::remove_all("/tmp/rc_extra"); loadSettings(e); assert(e.winW == 0 && e.hidden.empty());

        std::map<std::string, ModInfo> inf;
        for (auto n : {"a", "b", "c", "d", "e"}) { ModInfo m; m.id = std::string(n) + ".mod"; m.name = std::string("Mod ") + n; inf[m.id] = m; }
        auto mk = [](std::initializer_list<std::pair<const char*, bool>> l, const char* name) { Playset p; p.name = name; for (auto& x : l) p.mods.push_back({std::string(x.first) + ".mod", x.second, ""}); return p; };
        Playset A = mk({{"a", true}, {"b", true}, {"c", true}, {"d", true}, {"e", false}}, "A");
        Playset B = mk({{"b", true}, {"c", true}, {"d", true}, {"a", true}, {"e", true}}, "B");
        PlaysetDiff d = comparePlaysets(A, B, inf);
        assert(d.shared == 4 && !d.sameOrder);
        assert(d.moved.size() == 1 && d.moved[0] == "Mod a");           // only the mod that jumped, not b, c, d
        assert(d.onlyA.empty() && d.onlyB.empty() && d.enabledDiffers.size() == 1);
        Playset C = mk({{"a", true}, {"b", true}}, "C");
        PlaysetDiff d2 = comparePlaysets(A, C, inf);
        assert(d2.onlyA.size() == 2 && d2.sameOrder);

        std::vector<std::vector<ModIssue>> iss(A.mods.size());
        std::map<std::string, std::string> seen{{"a.mod", "1"}, {"b.mod", "1"}, {"e.mod", "1"}}, now{{"a.mod", "1"}, {"b.mod", "2"}, {"c.mod", "9"}, {"e.mod", "2"}};
        addUpdateIssues(iss, A, inf, seen, now);
        assert(iss[0].empty() && iss[1].size() == 1 && iss[1][0].sev == 1);   // b changed
        assert(iss[2].empty());                                               // c: never played before, nothing to compare
        assert(iss[4].empty());                                               // e: disabled
    }
    {   // ---- v0.13.3 known mods: AGOT family, More Interactive Vassals, Battle Graphics ----
        assert(normName("AGOT+") == "agot plus" && !nameIs(normName("AGOT+"), "agot") && !nameIs(normName("AGOT: Brightboar - Westerosi House Flavor"), "agot"));
        auto mkm = [](const char* id, const char* name) { ModInfo m; m.id = id; m.name = name; return m; };
        std::vector<ModInfo> ms = {
            mkm("ugc_3554844335.mod", "Rise and Fall"), mkm("rui.mod", "RUI"), mkm("miv.mod", "More Interactive Vassals"), mkm("r1.mod", "Random Content"),
            mkm("vs.mod", "Valyrian Steel"), mkm("ap.mod", "AGOT+"), mkm("core.mod", "AGOT Submod Core"), mkm("agot.mod", "A Game of Thrones"),
            mkm("epe.mod", "Ethnicities & Portraits Expanded"), mkm("bg.mod", "Battle Graphics"), mkm("redux.mod", "Battle Graphics Redux Compatch"), mkm("cfp.mod", "Community Flavor Pack")};
        auto inf = infoMap(ms);
        Playset ps; ps.name = "agot";
        for (auto& m : ms) ps.mods.push_back({m.id, true, ""});
        auto db = builtinKnownMods();
        auto plan = planSort(ps, inf, {}, {}, nullptr, nullptr, &db);
        std::vector<std::string> ids; for (int o : plan.order) ids.push_back(ps.mods[(size_t)o].id);
        auto pos = [&](const char* id) { return (int)(std::find(ids.begin(), ids.end(), id) - ids.begin()); };
        assert(pos("agot.mod") == 0 && pos("core.mod") == 1);                                   // AGOT on top, the submod core right below it
        assert(pos("vs.mod") > pos("core.mod") && pos("ap.mod") > pos("core.mod"));            // submods after the core
        assert(pos("miv.mod") > pos("r1.mod") && pos("miv.mod") > pos("bg.mod") && pos("ugc_3554844335.mod") == pos("miv.mod") + 1 && pos("rui.mod") == (int)ids.size() - 1);
        assert(pos("bg.mod") > pos("cfp.mod") && pos("bg.mod") > pos("epe.mod") && pos("redux.mod") > pos("bg.mod"));
        // a total conversion goes above even the Unofficial Patch, and the AGOT core still sits right below AGOT
        ms.push_back(mkm("up.mod", "Unofficial Patch")); ms.push_back(mkm("pod.mod", "Princes of Darkness: Extended Edition")); ms.push_back(mkm("ui.mod", "Unique Artifacts + 1.20")); ms.push_back(mkm("vd.mod", "Visible Disfigurement - No More Masks"));
        inf = infoMap(ms); for (size_t i = ps.mods.size(); i < ms.size(); i++) ps.mods.push_back({ms[i].id, true, ""});
        auto plan2 = planSort(ps, inf, {}, {}, nullptr, nullptr, &db);
        std::vector<std::string> ids2; for (int o : plan2.order) ids2.push_back(ps.mods[(size_t)o].id);
        auto pos2 = [&](const char* id) { return (int)(std::find(ids2.begin(), ids2.end(), id) - ids2.begin()); };
        assert(plan2.order.size() == ps.mods.size());
        assert(pos2("agot.mod") < pos2("core.mod") && pos2("pod.mod") < pos2("up.mod") && pos2("core.mod") < pos2("up.mod") && pos2("agot.mod") < pos2("up.mod") && pos2("up.mod") == 3);
        assert(pos2("vd.mod") > pos2("cfp.mod") && pos2("vd.mod") > pos2("epe.mod"));
        assert(!plan2.known[(size_t)(std::find_if(ms.begin(), ms.end(), [](const ModInfo& m) { return m.id == "ui.mod"; }) - ms.begin())].empty());   // Unique Artifacts + recognised (note shown)
    }
    // v0.14: SHA-256, version compare, release parsing, settings flag, log
    {
        assert(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        assert(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert(sha256Hex(std::string(1000, 'a')) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
        assert(compareVersions("v0.14.0", "0.13.3") > 0 && compareVersions("0.13.3", "v0.13.3") == 0 && compareVersions("0.13", "0.13.0") == 0 && compareVersions("0.9.9", "0.10.0") < 0 && compareVersions("v1.0.0-beta", "0.99.9") > 0);
        assert(parseVersion("garbage").empty());
        std::string js = R"({"tag_name":"v0.14.1","name":"Release","body":"notes\nline","html_url":"https://github.com/Juincy/the-royal-court/releases/tag/v0.14.1","assets":[
            {"name":"TheRoyalCourt.exe","size":1234,"browser_download_url":"https://github.com/Juincy/the-royal-court/releases/download/v0.14.1/TheRoyalCourt.exe"},
            {"name":"evil.exe","size":1,"browser_download_url":"https://evil.example.com/evil.exe"},
            {"name":"SHA256SUMS.txt","size":99,"browser_download_url":"https://github.com/juincy/The-Royal-Court/releases/download/v0.14.1/SHA256SUMS.txt"}]})";
        ReleaseInfo ri = parseRelease(js);
        assert(ri.ok && ri.tag == "v0.14.1" && ri.body == "notes\nline" && !ri.pageUrl.empty());
        assert(ri.asset("TheRoyalCourt.exe") && ri.asset("TheRoyalCourt.exe")->size == 1234 && ri.asset("SHA256SUMS.txt") && !ri.asset("evil.exe"));
        assert(!parseRelease("{}").ok && !parseRelease("not json").ok && !parseRelease(R"({"tag_name":"nightly"})").ok);
        assert(!trustedReleaseUrl("https://github.com.evil.com/Juincy/the-royal-court/releases/x") && !trustedReleaseUrl("http://github.com/Juincy/the-royal-court/releases/x"));
        std::string sums = std::string(64, 'A') + "  TheRoyalCourt.exe\r\n" + std::string(64, 'b') + " *other.zip\n";
        assert(findSumFor(sums, "TheRoyalCourt.exe") == std::string(64, 'a') && findSumFor(sums, "other.zip") == std::string(64, 'b') && findSumFor(sums, "nope").empty());
        Settings st; st.checkUpdates = true;
        assert(saveSettings(st));
        Settings st2; loadSettings(st2);
        assert(st2.checkUpdates);
        st.checkUpdates = false; saveSettings(st); loadSettings(st2);
        assert(!st2.checkUpdates);
        logLine("test line");
        std::string lg; assert(readFile(logPath(), lg) && lg.find("test line") != std::string::npos);
    }
    // v0.14.1: Workshop downloads without a launcher descriptor yet
    {
        fs::path W = T / "ws"; fs::path content = W / "content" / "1158310";
        fs::create_directories(content / "111111"); fs::create_directories(content / "222222"); fs::create_directories(content / "333333"); fs::create_directories(content / "notanid");
        writeFile(content / "111111" / "descriptor.mod", "version=\"2.0\"\ntags={\n\t\"Gameplay\"\n}\nname=\"Fresh Mod\"\nsupported_version=\"1.20.*\"\npath=\"/old/place\"\n");
        writeFile(content / "222222" / "descriptor.mod", "name=\"Already Registered\"\n");
        // 333333 has no descriptor.mod: still downloading
        std::set<std::string> known = {"ugc_222222.mod"};
        auto pend = scanWorkshopFolders({content.u8string(), (W / "missing").u8string()}, known);
        assert(pend.size() == 1 && pend[0].id == "ugc_111111.mod" && pend[0].name == "Fresh Mod" && pend[0].pending && pend[0].source == "Workshop" && pend[0].contentState == 1 && pend[0].supported == "1.20.*");
        std::string reg = registeredDescriptorText("name=\"X\"\npath=\"/old/place\"\nremote_file_id=\"9\"\ntags={\n\t\"A\"\n}\n", "111111", "C:\\Steam\\content\\111111");
        assert(reg.find("/old/place") == std::string::npos && reg.find("remote_file_id=\"111111\"") != std::string::npos && reg.find("path=\"C:/Steam/content/111111\"") != std::string::npos && reg.find("tags={") != std::string::npos);
        fs::path ck3 = W / "ck3"; fs::create_directories(ck3 / "mod");
        Playset ps; ps.name = "P"; ps.mods = {{"ugc_111111.mod", true, ""}, {"ugc_222222.mod", true, ""}};
        std::map<std::string, ModInfo> inf; inf[pend[0].id] = pend[0];
        assert(registerPendingMods(ck3.u8string(), ps, inf) == 1);
        std::string wrote; assert(readFile(ck3 / "mod" / "ugc_111111.mod", wrote) && wrote.find("name=\"Fresh Mod\"") != std::string::npos);
        assert(registerPendingMods(ck3.u8string(), ps, inf) == 0);   // never overwrites
        auto again = scanMods(ck3.u8string());
        assert(again.size() == 1 && again[0].id == "ugc_111111.mod" && again[0].name == "Fresh Mod" && again[0].contentState == 1);
        assert(scanWorkshopFolders({content.u8string()}, {"ugc_111111.mod", "ugc_222222.mod"}).empty());
        Playset ps2; ps2.mods = {{"ugc_111111.mod", true, ""}};
        auto iss = analyzePlayset(ps2, inf, "1.20.0");
        bool hasNote = false; for (auto& i : iss[0]) if (i.text.find("Tick the box") != std::string::npos) hasNote = true;
        assert(hasNote);
    }
    {   // ---- audit fixes (v0.15.0) ----
        auto mk = [](const std::string& id, const std::string& name) { ModInfo m; m.id = id; m.name = name; return m; };
        // local mods keep their identity when a name changes or repeats
        {
            std::vector<ModInfo> inst = {mk("ugc_1.mod", "Same"), mk("local1.mod", "Same"), mk("local2.mod", "Old Name v1")};
            auto info = infoMap(inst);
            Playset ps; ps.name = "p"; for (auto& m : inst) ps.mods.push_back({m.id, true, m.name});
            std::string js = exportLauncherPlayset(ps, info);
            assert(js.find("\"gameRegistryId\":\"mod/local1.mod\"") != std::string::npos && js.find("\"gameRegistryId\":\"mod/ugc_1.mod\"") == std::string::npos);
            inst[2].name = "Old Name v2";
            ImportResult r = parsePlaysetFile(js, inst);
            syncPlayset(r.playset, inst);
            assert(r.playset.mods.size() == 3 && r.playset.mods[0].id == "ugc_1.mod" && r.playset.mods[1].id == "local1.mod" && r.playset.mods[2].id == "local2.mod");
            for (auto& m : r.playset.mods) assert(m.enabled);
        }
        // short known names only match exactly; every built-in name is already in normalised form
        {
            auto db = builtinKnownMods();
            for (auto& k : db) for (auto& n : k.names) assert(normName(n) == n);
            for (const char* nm : {"AGOT Dothraki Rework", "MIV Lite", "Unofficial Patch Extras", "CFP Tweaks", "EPE Hairstyles"}) assert(findKnown(db, mk("x.mod", nm)) < 0);
            assert(findKnown(db, mk("x.mod", "Elder Kings 2")) >= 0 && findKnown(db, mk("x.mod", "AGOT")) >= 0 && findKnown(db, mk("x.mod", "Rise and Fall 1.2")) >= 0);
            assert(findKnown(db, mk("x.mod", "Rise and Fall Compatibility Patch")) < 0);
            // a later layer wins, also when it names the mod by id only
            std::vector<KnownMod> layered = builtinKnownMods();
            assert(applyKnownJson(layered, R"({"mods":[{"ids":["3554844335"],"position":"top","note":"mine"}]})"));
            int k = findKnown(layered, mk("ugc_3554844335.mod", "Rise and Fall"));
            assert(k >= 0 && layered[(size_t)k].note == "mine" && layered[(size_t)k].pos == -2);
        }
        // a circular dependency is broken inside the cycle, not at an innocent mod that waits on it
        {
            Playset ps; ps.name = "t"; std::map<std::string, ModInfo> info;
            auto add = [&](const std::string& id, const std::string& name, std::vector<std::string> deps) { ModInfo m = mk(id, name); m.deps = deps; info[id] = m; ps.mods.push_back({id, true, name}); };
            add("a.mod", "Alpha Content", {"Beta Content"});
            add("b.mod", "Beta Content", {"Alpha Content"});
            add("c.mod", "Gamma Library Core", {"Alpha Content"});
            SortPlan p = planSort(ps, info, {}, {}, nullptr, nullptr, nullptr);
            int posA = -1, posC = -1;
            for (size_t i = 0; i < p.order.size(); i++) { if (ps.mods[(size_t)p.order[i]].id == "a.mod") posA = (int)i; if (ps.mods[(size_t)p.order[i]].id == "c.mod") posC = (int)i; }
            assert(posA >= 0 && posC > posA && !p.warnings.empty());
        }
        // odd version strings never throw
        assert(matchGameVersion("1.20", "1.*") == VerMatch::Unknown);
        // renaming a playset keeps its backups
        {
            setenv("RC_DATA_DIR", "/tmp/rc_rename_test", 1);
            std::error_code ec; fs::remove_all("/tmp/rc_rename_test", ec);
            std::vector<ModInfo> ms = {mk("m0.mod", "Mod 0"), mk("m1.mod", "Mod 1")};
            auto inf = infoMap(ms);
            Playset ps; ps.name = "Old Name"; for (auto& m : ms) ps.mods.push_back({m.id, true, m.name});
            savePlayset(ps, inf);
            assert(!backupPlayset(ps, inf, "x").empty() && listBackups("Old Name").size() == 1);
            assert(renamePlayset(ps, "New Name", inf));
            assert(listBackups("New Name").size() == 1 && listBackups("Old Name").empty());
            // a playset file with a name that would be saved under another file name is renamed once, not duplicated
            std::string longName(100, 'x');
            writeFile(playsetsDir() / (longName + ".json"), exportLauncherPlayset(ps, inf));
            auto loaded = loadPlaysets(ms);
            bool found = false;
            for (auto& pl : loaded) if (pl.name == sanitizeFileName(longName)) found = true;
            assert(found && loaded.size() == 2);
            for (auto& pl : loaded) savePlayset(pl, inf);
            assert(loadPlaysets(ms).size() == 2);
        }
        // a damaged settings file is kept as .bad; absurd numbers do not break loading
        {
            setenv("RC_DATA_DIR", "/tmp/rc_settings_bad", 1);
            std::error_code ec; fs::remove_all("/tmp/rc_settings_bad", ec); fs::create_directories("/tmp/rc_settings_bad", ec);
            writeFile(settingsPath(), "{broken");
            Settings st; loadSettings(st);
            std::string kept; assert(readFile(settingsPath().string() + ".bad", kept) && kept == "{broken");
            writeFile(settingsPath(), R"({"window":{"x":1e30,"y":-1e30,"w":1300,"h":700,"cols":[100,1e99]}})");
            loadSettings(st);
            assert(st.winW == 1300 && st.winX == 100000000 && st.winY == -100000000 && st.colW.size() == 2 && st.colW[1] == 100000000);
        }
        // the Steam descriptor is written whole or not at all
        assert(fs::path(registeredDescriptorText("name=\"x\"\n", "1", "C:/a")).empty() == false);
    }
    {   // ---- deep fingerprint, saved index, game file list, severity ----
        fs::path root = T / "deepmod";
        fs::remove_all(root);
        fs::create_directories(root / "common/traits"); fs::create_directories(root / "events"); fs::create_directories(root / "localization/english");
        writeFile(root / "descriptor.mod", "name=\"x\"");
        writeFile(root / "common/traits/t.txt", "brave = { x = 1 }\nkind = { y = 2 }\n");
        writeFile(root / "events/e.txt", "namespace = ee\nee.1 = { type = character_event }\n");
        writeFile(root / "localization/english/a_l_english.yml", "l_english:\n k1:0 \"v\"\n");
        auto a1 = indexModFiles(root.u8string()), a2 = indexModFiles(root.u8string());
        assert(a1.complete && !a1.deep.empty() && a1.deep == a2.deep);
        writeFile(root / "descriptor.mod", "name=\"changed\"");                      // root-level files are not part of it
        assert(indexModFiles(root.u8string()).deep == a1.deep);
        writeFile(root / "common/traits/t.txt", "brave = { x = 1 }\nkind = { y = 2 }\nmore = { z = 3 }\n");   // size changes
        auto a3 = indexModFiles(root.u8string());
        assert(a3.deep != a1.deep && a3.files.size() == a1.files.size());
        auto t0 = fs::last_write_time(root / "events/e.txt");
        fs::last_write_time(root / "events/e.txt", t0 + std::chrono::seconds(7));    // only the time changes
        auto a4 = indexModFiles(root.u8string());
        assert(a4.deep != a3.deep);
        writeFile(root / "common/traits/new.txt", "n = { }");                       // a file is added deep inside
        assert(indexModFiles(root.u8string()).deep != a4.deep);

        // saved index: first read parses, the second one comes from the saved copy and gives the same result
        std::string cdir = (T / "cache1").u8string();
        fs::remove_all(P(cdir));
        auto i1 = indexModCached(cdir, "m1.mod", root.u8string());
        assert(i1.files.complete && i1.defs.complete && !i1.fromCache && !i1.defs.defs.empty());
        auto i2 = indexModCached(cdir, "m1.mod", root.u8string());
        assert(i2.fromCache && i2.defs.defs.size() == i1.defs.defs.size() && i2.defs.pool == i1.defs.pool && i2.defs.files == i1.defs.files && i2.defs.bytesRead == i1.defs.bytesRead);
        for (size_t k = 0; k < i1.defs.defs.size(); k++) assert(i1.defs.defs[k].h == i2.defs.defs[k].h && i1.defs.defs[k].file == i2.defs.defs[k].file);
        // a change inside the mod is noticed
        writeFile(root / "common/traits/t.txt", "brave = { x = 1 }\nnewone = { y = 2 }\n");
        auto i3 = indexModCached(cdir, "m1.mod", root.u8string());
        assert(!i3.fromCache);
        bool sawNew = false;
        for (auto& d : i3.defs.defs) if (i3.defs.nameOf(d) == "newone") sawNew = true;
        assert(sawNew);
        // another mod id never reads this mod's file, and damage of any kind means "read it again"
        assert(!indexModCached(cdir, "m2.mod", root.u8string()).fromCache);
        fs::path cf = modCacheFile(cdir, "m1.mod");
        std::string good; assert(readFile(cf, good));
        ModDefs probe;
        std::string deepNow = indexModFiles(root.u8string()).deep;
        assert(loadModCache(cdir, "m1.mod", deepNow, probe));
        assert(!loadModCache(cdir, "m1.mod", deepNow + "x", probe) && !loadModCache(cdir, "other.mod", deepNow, probe));
        for (size_t cut : {size_t(0), size_t(3), good.size() / 2, good.size() - 1}) {
            writeFile(cf, good.substr(0, cut));
            assert(!loadModCache(cdir, "m1.mod", deepNow, probe));
        }
        for (size_t at : {size_t(5), size_t(40), good.size() / 2, good.size() - 12}) {
            std::string bad = good; bad[at] = (char)(bad[at] ^ 0x5a);
            writeFile(cf, bad);
            assert(!loadModCache(cdir, "m1.mod", deepNow, probe));
        }
        writeFile(cf, good);
        assert(loadModCache(cdir, "m1.mod", deepNow, probe));
        // a cache file that lies about its sizes (valid checksum, impossible counts) is refused without crashing
        {
            CacheOut o; o.b += "RCIX"; o.u32(INDEX_CACHE_FORMAT); o.u32(DEFS_PARSER_VERSION); o.str("m1.mod"); o.str(deepNow); o.u64(0);
            o.u32(0xFFFFFFF0u);
            o.u64(fnv64(o.b, '\x1d', "RCIX"));
            writeFile(cf, o.b);
            assert(!loadModCache(cdir, "m1.mod", deepNow, probe));
        }
        // pruning removes only our own files of mods that are gone
        writeFile(cf, good);
        indexModCached(cdir, "m2.mod", root.u8string());
        writeFile(P(cdir) / "notes.txt", "keep me"); writeFile(P(cdir) / "m_zzzzzzzzzzzzzzzz.bin", "not ours");
        assert(pruneModCache(cdir, {"m1.mod"}) == 1);
        assert(fs::exists(modCacheFile(cdir, "m1.mod")) && !fs::exists(modCacheFile(cdir, "m2.mod")) && fs::exists(P(cdir) / "notes.txt") && fs::exists(P(cdir) / "m_zzzzzzzzzzzzzzzz.bin"));
        assert(pruneModCache("", {}) == 0);

        // the game's own file list
        fs::path inst = T / "ck3install";
        fs::remove_all(inst);
        fs::create_directories(inst / "binaries"); fs::create_directories(inst / "game/common/traits"); fs::create_directories(inst / "game/events");
        fs::create_directories(inst / "game/gfx"); fs::create_directories(inst / "dlc/dlc001_x/common/traits"); fs::create_directories(inst / "game/.git");
        writeFile(inst / "binaries/ck3.exe", "x");
        writeFile(inst / "game/common/traits/00_traits.txt", "a"); writeFile(inst / "game/common/traits/01_more.txt", "a");
        writeFile(inst / "game/events/e1.txt", "a"); writeFile(inst / "game/gfx/x.dds", "a"); writeFile(inst / "game/readme.txt", "a");
        writeFile(inst / "game/.git/config", "a"); writeFile(inst / "game/Thumbs.db", "a");
        writeFile(inst / "dlc/dlc001_x/common/traits/dlc_traits.txt", "a"); writeFile(inst / "dlc/dlc001_x/dlc.json", "a");
        std::string exe = (inst / "binaries/ck3.exe").u8string();
        std::string gdir = vanillaGameDir(exe);
        assert(!gdir.empty() && fs::path(P(gdir)).filename() == "game" && vanillaGameDir("").empty() && vanillaGameDir((T / "nope/bin/x.exe").u8string()).empty());
        auto v1 = indexVanilla(gdir);
        assert(v1 && v1->has("common/traits/00_traits.txt") && v1->has("events/e1.txt") && v1->has("gfx/x.dds") && v1->has("common/traits/dlc_traits.txt"));
        assert(!v1->has("readme.txt") && !v1->has(".git/config") && !v1->has("thumbs.db") && !v1->has("dlc.json") && !v1->has("common/traits/nothing.txt"));
        assert(v1->countUnder("common/traits/") == 3 && v1->countUnder("events/") == 1 && v1->countUnder("common/trait/") == 0 && v1->dlcFiles == 1);
        std::string vdir = (T / "cache2").u8string();
        fs::remove_all(P(vdir));
        bool fc = true;
        auto v2 = loadOrIndexVanilla(vdir, exe, "1.20.0.4", nullptr, &fc);
        assert(v2 && !fc && v2->files == v1->files);
        auto v3 = loadOrIndexVanilla(vdir, exe, "1.20.0.4", nullptr, &fc);
        assert(v3 && fc && v3->files == v1->files && v3->dlcFiles == 1 && v3->has("gfx/x.dds"));
        loadOrIndexVanilla(vdir, exe, "1.21.0.1", nullptr, &fc); assert(!fc);                    // a game update: list is read again
        fs::create_directories(inst / "game/newfolder"); writeFile(inst / "game/newfolder/a.txt", "a");   // a new top-level folder changes the key
        auto v4 = loadOrIndexVanilla(vdir, exe, "1.21.0.1", nullptr, &fc);
        assert(!fc && v4->has("newfolder/a.txt"));
        std::string vtext; assert(readFile(vanillaCacheFile(vdir), vtext));
        for (size_t cut : {size_t(0), size_t(5), vtext.size() / 2, vtext.size() - 1}) {          // damaged copies are never trusted
            writeFile(vanillaCacheFile(vdir), vtext.substr(0, cut));
            assert(!loadVanillaCache(vdir, v4->key));
        }
        writeFile(vanillaCacheFile(vdir), vtext + "extra/line.txt\n");
        assert(!loadVanillaCache(vdir, v4->key));
        writeFile(vanillaCacheFile(vdir), vtext);
        assert(loadVanillaCache(vdir, v4->key) && !loadVanillaCache(vdir, v4->key + "x"));
        std::atomic<bool> stopV(true);
        assert(!indexVanilla(gdir, &stopV));

        // mods against the game's files
        auto mk2 = [](const char* id, const char* name, const char* sup, std::vector<std::string> rp = {}) { ModInfo m; m.id = id; m.name = name; m.supported = sup; m.replacePaths = rp; return m; };
        std::vector<ModInfo> vm = {mk2("a.mod", "A", "1.1.*"), mk2("b.mod", "B", "1.20.*"), mk2("c.mod", "C", "1.20.*", {"common/traits"}), mk2("d.mod", "D", "1.20.*"), mk2("e.mod", "E", "1.20.*")};
        auto vinfo = infoMap(vm);
        std::map<std::string, ModFiles> vidx;
        auto mkf = [](std::vector<std::string> f) { ModFiles m; m.files = f; m.complete = true; return m; };
        std::vector<std::string> gf; for (int i = 0; i < 5; i++) gf.push_back("gfx/pair" + std::to_string(i) + ".dds");
        auto dfiles = gf; dfiles.push_back("localization/english/x.yml");
        vidx["a.mod"] = mkf({"common/traits/00_traits.txt", "gfx/x.dds", "common/mine.txt"});
        vidx["b.mod"] = mkf({"common/traits/00_traits.txt", "events/new.txt"});
        vidx["c.mod"] = mkf({"common/other/o.txt"});
        vidx["d.mod"] = mkf(gf);
        vidx["e.mod"] = mkf(gf);
        Playset vp; vp.name = "v"; vp.mods = {{"a.mod", true, ""}, {"b.mod", true, ""}, {"c.mod", true, ""}, {"d.mod", true, ""}, {"e.mod", true, ""}};
        auto vcr = findConflicts(vp, vinfo, vidx, v1.get());
        assert(vcr.valid && vcr.vanillaChecked && vcr.vanilla.size() == 2);
        assert(vcr.vanilla[0].path == "common/traits/00_traits.txt" && vcr.vanilla[0].mods == std::vector<int>({0, 1}) && vcr.vanilla[0].sev == SEV_HIGH);
        assert(vcr.vanilla[1].path == "gfx/x.dds" && vcr.vanilla[1].mods == std::vector<int>({0}) && vcr.vanilla[1].sev == SEV_LOW);
        assert(vcr.vanillaCount[0] == 2 && vcr.vanillaCount[1] == 1 && vcr.vanillaCount[2] == 0);
        assert(vcr.files.size() == 1 + 5 && vcr.files[0].path == "common/traits/00_traits.txt" && vcr.files[0].vanilla && vcr.files[0].sev == SEV_HIGH && !vcr.files[1].vanilla && vcr.files[1].sev == SEV_LOW);
        assert(vcr.pairs.size() == 2 && vcr.pairs[0].a == 0 && vcr.pairs[0].b == 1 && vcr.pairs[0].sev == SEV_HIGH && vcr.pairs[1].count == 5 && vcr.pairs[1].sev == SEV_LOW);   // serious first, even though the other pair has more files
        assert(vcr.vanillaWipes.size() == 1 && vcr.vanillaWipes[0].mod == 2 && vcr.vanillaWipes[0].folder == "common/traits" && vcr.vanillaWipes[0].files == 3);
        auto viss = analyzePlayset(vp, vinfo, "1.20.0.4");
        addConflictIssues(viss, vcr, vp, vinfo, "1.20.0.4");
        bool oldNote = false, wipeNote = false, newNote = false;
        for (auto& is : viss[0]) if (is.text.find("older game version") != std::string::npos && is.sev == 1) oldNote = true;
        for (auto& is : viss[2]) if (is.text.find("removes 3 file(s) of the base game") != std::string::npos) wipeNote = true;
        for (auto& is : viss[1]) if (is.text.find("Replaces 1 file(s) of the base game") != std::string::npos && is.sev == 0) newNote = true;
        assert(oldNote && wipeNote && newNote);
        auto ncr = findConflicts(vp, vinfo, vidx);                         // without the game's list nothing is claimed about it
        assert(ncr.valid && !ncr.vanillaChecked && ncr.vanilla.empty() && ncr.vanillaWipes.empty() && ncr.files.size() == 1 + 5);
        { std::vector<std::vector<ModIssue>> ni = analyzePlayset(vp, vinfo, "1.20.0.4"); size_t before = ni[0].size(); addConflictIssues(ni, ncr, vp, vinfo, "1.20.0.4");
          for (auto& is : ni[0]) assert(is.text.find("base game") == std::string::npos); (void)before; }
        // a cancelled computation gives nothing
        std::map<std::string, ModFiles> hugeIdx; std::vector<ModInfo> hm; Playset hp; hp.name = "h";
        for (int m = 0; m < 2; m++) {
            std::string id = "h" + std::to_string(m) + ".mod"; hm.push_back(mk2(id.c_str(), id.c_str(), ""));
            ModFiles f; f.complete = true;
            for (int i = 0; i < 40000; i++) f.files.push_back("common/x/f" + std::to_string(i) + ".txt");
            hugeIdx[id] = std::move(f); hp.mods.push_back({id, true, ""});
        }
        std::atomic<bool> stopC(true);
        assert(!findConflicts(hp, infoMap(hm), hugeIdx, nullptr, &stopC).valid && findConflicts(hp, infoMap(hm), hugeIdx).files.size() == 40000);
        // severity
        assert(areaSeverity("common/traits") == SEV_HIGH && areaSeverity("events") == SEV_HIGH && areaSeverity("history/provinces") == SEV_HIGH && areaSeverity("map_data") == SEV_HIGH);
        assert(areaSeverity("gui") == SEV_MED && areaSeverity("localization/english") == SEV_MED && areaSeverity("something_else") == SEV_MED);
        assert(areaSeverity("gfx/portraits") == SEV_LOW && areaSeverity("music") == SEV_LOW && areaSeverity("common/genes") == SEV_LOW && areaSeverity("common/bookmark_portraits") == SEV_LOW);
        assert(std::string(sevName(SEV_HIGH)) == "High" && std::string(sevName(SEV_MED)) == "Medium" && std::string(sevName(SEV_LOW)) == "Low");
    }
    {   // ---- one key defined thousands of times across mods must not take quadratic time ----
        std::vector<ModInfo> pm; std::map<std::string, ModDefs> pd; Playset pp; pp.name = "perf";
        for (int m = 0; m < 6; m++) {
            ModInfo mi; mi.id = "p" + std::to_string(m) + ".mod"; mi.name = mi.id; pm.push_back(mi);
            ModDefs d; d.complete = true;
            for (int i = 0; i < 6000; i++) {
                d.files.push_back("common/big/m" + std::to_string(m) + "_" + std::to_string(i) + ".txt");
                DefEntry e; e.h = fnv64("common/big", '\x1f', "same_key"); e.file = (uint32_t)i; e.nameOff = 0; e.nameLen = 8; e.kind = DK_COMMON;
                d.defs.push_back(e);
            }
            d.pool = "same_key";
            pd[mi.id] = std::move(d); pp.mods.push_back({mi.id, true, ""});
        }
        auto t0 = std::chrono::steady_clock::now();
        auto sr = findScriptConflicts(pp, infoMap(pm), pd);
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        assert(sr.valid && sr.items.size() == 1 && sr.items[0].hits.size() == 36000 && ms < 5000);
        // shadowing still works: the same file path in an earlier and a later mod keeps only the later one
        std::vector<ModInfo> sm; std::map<std::string, ModDefs> sd; Playset sp; sp.name = "shadow";
        for (int m = 0; m < 3; m++) {
            ModInfo mi; mi.id = "s" + std::to_string(m) + ".mod"; mi.name = mi.id; sm.push_back(mi);
            ModDefs d; d.complete = true; d.pool = "k";
            d.files.push_back(m < 2 ? "common/traits/same.txt" : "common/traits/other.txt");
            DefEntry e; e.h = fnv64("common/traits", '\x1f', "k"); e.file = 0; e.nameOff = 0; e.nameLen = 1; e.kind = DK_COMMON;
            d.defs.push_back(e);
            sd[mi.id] = std::move(d); sp.mods.push_back({mi.id, true, ""});
        }
        auto sr2 = findScriptConflicts(sp, infoMap(sm), sd);
        assert(sr2.items.size() == 1 && sr2.items[0].hits.size() == 2 && sr2.items[0].hits[0].mod == 1 && sr2.items[0].hits[1].mod == 2);   // mod 0's copy was replaced by mod 1's file
    }
    {   // ---- update check: fingerprints with and without the deep part ----
        assert(!fingerprintChanged("C:/m|1.0|100", "C:/m|1.0|100") && fingerprintChanged("C:/m|1.0|100", "C:/m|1.0|101") && fingerprintChanged("C:/m|1.0|100", "C:/m|1.1|100"));
        assert(!fingerprintChanged("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100#d:abc-5"));
        assert(!fingerprintChanged("C:/m|1.0|100#d:abc-5", "C:/m|1.0|999#d:abc-5"));      // only the folder's time moved: not an update
        assert(fingerprintChanged("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100#d:abd-5"));       // a file deep inside changed
        assert(fingerprintChanged("C:/m|1.0|100#d:abc-5", "C:/m|1.1|100#d:abc-5"));       // the mod's version changed
        assert(!fingerprintChanged("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100"));              // not indexed yet: plain comparison
        assert(!fingerprintChanged("C:/m|1.0|100", "C:/m|1.0|100#d:abc-5"));              // an older record: plain comparison
        assert(fingerprintChanged("C:/m|1.0|100", "C:/m|1.0|101#d:abc-5"));
        assert(!fingerprintChanged("", "") && fingerprintChanged("", "C:/m|1.0|1"));
        std::vector<ModInfo> um; ModInfo u1; u1.id = "u.mod"; u1.name = "U"; um.push_back(u1);
        Playset up; up.name = "u"; up.mods = {{"u.mod", true, ""}};
        auto ui = analyzePlayset(up, infoMap(um), "");
        addUpdateIssues(ui, up, infoMap(um), {{"u.mod", "d|1|5#d:aa-2"}}, {{"u.mod", "d|1|9#d:aa-2"}});
        assert(ui[0].empty());
        addUpdateIssues(ui, up, infoMap(um), {{"u.mod", "d|1|5#d:aa-2"}}, {{"u.mod", "d|1|5#d:ab-2"}});
        assert(!ui[0].empty() && ui[0].back().text.find("Updated since") != std::string::npos);
    }

    {   // ---- game log: real message shapes, attribution to mods ----
        std::string log =
            "[08:40:44][W][game_database.h:272]: Overriding entry 'house_armagnac' for database 'common/dynasty_houses' in 'file: common/dynasty_houses/00_unop_dynasty_houses.txt line: 1'\r\n"
            "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\r\n"
            "  Error: stress_impact effect [ Cannot find scholar in trait database ]\r\n"
            "  Script location: file: events/VIET_events_travel.txt line: 4129 (VIETmisc.7039:option)\r\n"
            "\r\n"
            "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\r\n"
            "  Error: stress_impact effect [ Cannot find scholar in trait database ]\r\n"
            "  Script location: file: events/VIET_events_travel.txt line: 4129 (VIETmisc.7039:option)\r\n"
            "\r\n"
            "[08:41:13][E][modifier_instance.cpp:390]: Unknown modifier type 'faith_creation_piety_cost_mult' at file: common/traits/zz_gptev_traits.txt line: 776 (gpt_crow)\r\n"
            "[08:41:13][E][event.cpp:451]: fullscreen_event 'VIETmisc.1084' requires queue_icon\r\n"
            "[08:41:13][E][weird.cpp:1]: something nobody can place\r\n"
            "[08:41:13][E][base.cpp:1]: bad thing at file: common/traits/00_traits.txt line: 3 (x)\r\n"
            "[08:41:14][I][info.cpp:1]: just information\r\n"
            "[08:41:14][D][dbg.cpp:1]: debug chatter\r\n"
            "not a log line at all\r\n";
        auto lp = parseGameLog(log);
        assert(lp.errors == 6 && lp.warnings == 1);                 // the duplicated script error counts twice, information and debug are skipped
        assert(lp.entries.size() == 6);                             // ...but is listed once
        const LogEntry* se = nullptr; for (auto& e : lp.entries) if (e.src == "jomini_script_system.cpp:304") se = &e;
        assert(se && se->count == 2 && se->file == "events/viet_events_travel.txt" && se->line == 4129 && se->detail.find("Cannot find scholar") != std::string::npos);
        bool names = false; for (auto& n : se->names) if (n == "VIETmisc.7039") names = true;
        assert(names);
        auto lw = parseGameLog(log, false);
        assert(lw.warnings == 0 && lw.errors == 6);
        // attribution: mod 0 ships the events file, mod 1 the trait file and defines event VIETmisc.1084, the base game has 00_traits.txt
        Playset ps; ps.name = "t"; ps.mods = {{"a.mod", true, "A"}, {"b.mod", true, "B"}, {"c.mod", false, "C"}};
        std::map<std::string, ModFiles> mf; mf["a.mod"].files = {"events/viet_events_travel.txt", "common/traits/zz_gptev_traits.txt"}; mf["a.mod"].complete = true;
        mf["b.mod"].files = {"common/traits/zz_gptev_traits.txt"}; mf["b.mod"].complete = true;
        mf["c.mod"].files = {"events/viet_events_travel.txt"}; mf["c.mod"].complete = true;   // disabled: never blamed
        std::map<std::string, ModDefs> md; ModDefs bd; bd.complete = true; bd.pool = "VIETmisc.1084"; DefEntry de; de.nameOff = 0; de.nameLen = 13; de.kind = DK_EVENT; bd.defs.push_back(de); md["b.mod"] = bd;
        VanillaIndex van; van.files = {"common/traits/00_traits.txt"}; van.finish();
        auto rep = attributeLog(parseGameLog(log), ps, mf, md, &van);
        for (size_t k = 0; k < rep.parse.entries.size(); k++) {
            auto& e = rep.parse.entries[k];
            if (e.src == "jomini_script_system.cpp:304") assert(rep.modOf[k] == 0 && rep.how[k] == LH_FILE);
            if (e.src == "modifier_instance.cpp:390") assert(rep.modOf[k] == 1 && rep.how[k] == LH_FILE);   // both ship it; the later mod's copy is the one the game used
            if (e.src == "event.cpp:451") assert(rep.modOf[k] == 1 && rep.how[k] == LH_NAME);
            if (e.src == "weird.cpp:1") assert(rep.modOf[k] == -1 && rep.how[k] == LH_NONE);
            if (e.src == "base.cpp:1") assert(rep.modOf[k] == -1 && rep.how[k] == LH_BASE);
            if (e.src == "game_database.h:272") assert(rep.how[k] == LH_NONE);   // no mod ships that file here
        }
        assert(rep.perMod.size() == 2 && rep.perMod[0].mod == 0 && rep.perMod[0].errors == 2);
        assert(rep.base.errors == 1 && rep.unplaced.errors == 1 && rep.unplaced.warnings == 1);
        assert(logModReport(rep, 0, "A").find("Cannot find scholar") != std::string::npos);
        // loaded mods from debug.log
        auto lm = parseLoadedMods("junk\nBetter Population Control|mod/ugc_3425828418.mod|Enabled\nUnofficial Patch|mod/ugc_2871648329.mod|Enabled\nBetter Population Control|mod/ugc_3425828418.mod|Enabled\nbad|mod/../x.mod|Enabled\nX|mod/y.mod|Maybe\n");
        assert(lm.size() == 2 && lm[0].id == "ugc_3425828418.mod" && lm[1].name == "Unofficial Patch");
        // robustness: nothing in, nothing out; huge and odd input does not crash
        assert(parseGameLog("").entries.empty());
        assert(parseGameLog("[", true).entries.empty() && parseGameLog("[08:00:00][E][", true).entries.empty());
        std::string big; for (int i = 0; i < 30000; i++) big += "[08:00:00][E][a.cpp:1]: msg " + std::to_string(i) + " file: x/" + std::to_string(i) + ".txt line: 1\n";
        auto bp = parseGameLog(big); assert(bp.entries.size() == 20000 && bp.dropped == 10000 && bp.errors == 30000);
    }
    {   // ---- share codes ----
        std::vector<ModInfo> inst; ModInfo i1; i1.id = "ugc_2871648329.mod"; i1.name = "Unofficial Patch"; inst.push_back(i1);
        ModInfo i2; i2.id = "my_local.mod"; i2.name = "My Local"; inst.push_back(i2);
        ModInfo i3; i3.id = "ugc_5.mod"; i3.name = "Off"; inst.push_back(i3);
        Playset ps; ps.name = "Vanilla+ V1.0"; ps.mods = {{"ugc_2871648329.mod", true, ""}, {"my_local.mod", true, ""}, {"ugc_5.mod", false, ""}, {"ugc_3425828418.mod", true, "Gone Mod"}, {"ugc_77.mod", false, "Off Missing"}, {"local:Ghost", true, "Ghost"}};
        auto im = infoMap(inst);
        for (bool names : {false, true}) {
            std::string code = makeShareCode(ps, im, names);
            assert(code.rfind("RC1:", 0) == 0 && code.find_first_of(" \n+/=") == std::string::npos);
            auto d = decodeShareCode("here you go: " + code + " enjoy");
            assert(d.ok && d.name == "Vanilla+ V1.0" && d.withNames == names);
            assert(d.mods.size() == 5);                                   // the disabled installed mod is left out; the disabled missing one is kept
            assert(d.mods[0].workshopId == "2871648329" && d.mods[0].enabled && (names ? d.mods[0].name == "Unofficial Patch" : d.mods[0].name.empty()));
            assert(d.mods[1].local && d.mods[1].name == "My Local");
            assert(d.mods[2].workshopId == "3425828418" && d.mods[2].enabled);
            assert(d.mods[3].workshopId == "77" && !d.mods[3].enabled);
            assert(d.mods[4].local && d.mods[4].name == "Ghost");
            Playset back = playsetFromShare(d, inst);
            assert(back.mods.size() == 5 && back.mods[0].id == "ugc_2871648329.mod" && back.mods[1].id == "my_local.mod" && back.mods[2].id == "ugc_3425828418.mod");
            assert(back.mods[3].id == "ugc_77.mod" && !back.mods[3].enabled && back.mods[4].id == "local:Ghost");
        }
        std::string code = makeShareCode(ps, im, false);
        assert(!decodeShareCode("").ok && !decodeShareCode("RC1:").ok && !decodeShareCode("hello").ok);
        std::string cut = code.substr(0, code.size() - 5); assert(!decodeShareCode(cut).ok);
        std::string bad = code; bad[10] = bad[10] == 'A' ? 'B' : 'A'; assert(!decodeShareCode(bad).ok);   // one wrong character is caught
        Playset big; big.name = "big"; std::map<std::string, ModInfo> bi;
        for (int i = 0; i < 100; i++) big.mods.push_back({"ugc_" + std::to_string(2000000000u + (unsigned)i * 7919u) + ".mod", true, ""});
        assert(makeShareCode(big, bi, false).size() < 800);               // 100 Workshop mods fit in one chat message
        // a code with an absurd count or a cut-off name cannot read past its end
        std::string evil; evil += (char)1; evil += (char)0; evil += (char)0; putVar(evil, 4999); uint32_t cc = crc32Of(evil); for (int i = 0; i < 4; i++) evil += (char)((cc >> (i * 8)) & 0xFF);
        assert(!decodeShareCode(std::string("RC1:") + b64urlEncode(evil)).ok);
        for (size_t n = 0; n < 40; n++) { std::string junk; for (size_t k = 0; k < n; k++) junk += (char)(k * 37 + n); assert(!decodeShareCode("RC1:" + b64urlEncode(junk)).ok); }
    }
    {   // ---- since last Play: file records and the report ----
        setenv("RC_DATA_DIR", "/tmp/rc_seen_test", 1);
        fs::remove_all("/tmp/rc_seen_test");
        ModFiles a; a.complete = true;
        for (int i = 0; i < 50; i++) { a.files.push_back("common/traits/t" + std::to_string(i) + ".txt"); a.mix.push_back(deepMix(a.files.back(), 100 + i, 5)); }
        Manifest m0 = manifestOf(a);
        assert(m0.v.size() == 50 && saveManifest("a.mod", m0));
        Manifest back; assert(loadManifest("a.mod", back) && back.v == m0.v);
        auto same = compareManifest(back, a);
        assert(same.known && same.same == 50 && !same.added && !same.changed && !same.removed && describeChanges(same) == "no file changes");
        ModFiles b = a;   // one file changed, one removed, two added
        b.mix[3] ^= 1; b.files.erase(b.files.begin() + 10); b.mix.erase(b.mix.begin() + 10);
        b.files.push_back("events/new_a.txt"); b.mix.push_back(1); b.files.push_back("events/new_b.txt"); b.mix.push_back(2);
        auto ch = compareManifest(back, b);
        assert(ch.known && ch.changed == 1 && ch.added == 2 && ch.removed == 1 && ch.same == 48);
        assert(ch.changedNames.size() == 1 && ch.changedNames[0] == "common/traits/t3.txt" && ch.addedNames.size() == 2);
        assert(describeChanges(ch).find("1 file changed, 2 added, 1 removed") == 0);
        assert(!compareManifest(Manifest(), a).known);
        ModFiles incomplete = a; incomplete.complete = false; assert(manifestOf(incomplete).v.empty() && !compareManifest(back, incomplete).known);
        ModFiles nomix = a; nomix.mix.clear(); assert(manifestOf(nomix).v.empty());
        // a damaged record is ignored
        std::string raw; readFile(manifestPath("a.mod"), raw); raw[20] ^= 1; writeFile(manifestPath("a.mod"), raw);
        Manifest bad2; assert(!loadManifest("a.mod", bad2));
        writeFile(manifestPath("a.mod"), raw.substr(0, raw.size() / 2)); assert(!loadManifest("a.mod", bad2));
        assert(!loadManifest("nothing.mod", bad2));
        // the report
        saveManifest("a.mod", m0);
        Settings st; Playset ps; ps.name = "P"; ps.mods = {{"a.mod", true, "A"}, {"o.mod", true, "Old"}, {"n.mod", true, "New"}, {"off.mod", false, "Off"}};
        std::vector<ModInfo> inst; ModInfo ia; ia.id = "a.mod"; ia.name = "A"; ia.supported = "1.21.*"; inst.push_back(ia);
        ModInfo io; io.id = "o.mod"; io.name = "Old"; io.supported = "1.18.*"; inst.push_back(io);
        ModInfo in; in.id = "n.mod"; in.name = "New"; in.supported = "1.21.*"; inst.push_back(in);
        ModInfo iof; iof.id = "off.mod"; iof.name = "Off"; inst.push_back(iof);
        auto im = infoMap(inst);
        std::map<std::string, ModFiles> files; files["a.mod"] = b;
        std::map<std::string, std::string> now{{"a.mod", "d|1|1#d:bb-5"}};
        auto none = buildPlayReport(st, ps, im, now, files, "1.21.0.1");
        assert(!none.hasPrev && !none.gameChanged && none.updated.empty() && none.outdated.size() == 1 && none.outdated[0].name == "Old");   // Play was never pressed: only the version warning
        st.playTime = 1000; st.playGameVer = "1.20.0.4"; st.playName = "P"; st.playMods = {"a.mod", "gone.mod", "off.mod"};
        st.seen["a.mod"] = "d|1|1#d:aa-5";
        auto rp = buildPlayReport(st, ps, im, now, files, "1.21.0.1");
        assert(rp.hasPrev && rp.gameChanged && rp.prevGame == "1.20.0.4");
        assert(rp.updated.size() == 1 && rp.updated[0].text.find("1 file changed") == 0);
        assert(rp.outdated.size() == 1 && rp.outdated[0].modId == "o.mod");
        assert(rp.added.size() == 2);                                      // o.mod and n.mod were not enabled at the last Play
        assert(rp.removed.size() == 2 && rp.removed[0].text == "no longer installed" && rp.removed[1].text == "turned off since then");
        Playset other = ps; other.name = "Other";
        assert(buildPlayReport(st, other, im, now, files, "1.21.0.1").added.empty());   // a different playset: no added/removed comparison
        // recordPlay writes the records and the settings round-trip
        recordPlay(st, ps, files, "1.21.0.1", 2000);
        assert(st.playTime == 2000 && st.playGameVer == "1.21.0.1" && st.playMods.size() == 3);
        Manifest rec; assert(loadManifest("a.mod", rec) && rec.v.size() == b.files.size());
        setenv("RC_DATA_DIR", "/tmp/rc_seen_test", 1);
        saveSettings(st);
        Settings st2; loadSettings(st2);
        assert(st2.playTime == 2000 && st2.playGameVer == "1.21.0.1" && st2.playName == "P" && st2.playMods == st.playMods);
    }
    {   // crash helper: folder names, exception.txt, meta.yml, damaged input, scoring
        CrashFolder cf;
        assert(crashNameParse("ck3_20261009_172556", cf) && cf.y == 2026 && cf.mo == 10 && cf.d == 9 && cf.h == 17 && cf.mi == 25 && cf.s == 56);
        assert(!crashNameParse("ck3_2026", cf) && !crashNameParse("ck3_20261309_172556", cf) && !crashNameParse("xyz_20261009_172556", cf) && !crashNameParse("ck3_2026100a_172556", cf));
        CrashData d;
        crashParseException("Exception: EXCEPTION_ACCESS_VIOLATION reading 0x0\r\n  ck3.exe!Foo::bar() + 0x12\n  0x7ff6 something\n", d);
        assert(d.haveException && d.kind == CK_ACCESS && d.code == "EXCEPTION_ACCESS_VIOLATION" && d.frames.size() == 2);
        CrashData d2; crashParseException("The process ran out of memory\n", d2); assert(d2.kind == CK_MEMORY && d2.code.find("memory") != std::string::npos);
        CrashData d3; crashParseException("DXGI_ERROR_DEVICE_REMOVED", d3); assert(d3.kind == CK_GRAPHICS);
        CrashData d4; crashParseException("", d4); assert(!d4.haveException && d4.kind == CK_UNKNOWN);
        CrashData d5; crashParseMeta("version: 1.20.0.4\nbuild: abc\nfoo: bar\n\x01\x02 garbage\n  platform: win\n", d5); assert(d5.meta.size() == 3);
        std::string junk(5000, '\x07'); CrashData d6; crashParseException(junk, d6); crashParseMeta(junk, d6);
        // a folder on disk with logs in a subfolder, and a stray folder
        fs::remove_all("/tmp/rc_crash_t"); fs::create_directories("/tmp/rc_crash_t/ck3_20261009_172556/logs"); fs::create_directories("/tmp/rc_crash_t/ck3_20250101_000000"); fs::create_directories("/tmp/rc_crash_t/misc");
        { std::ofstream("/tmp/rc_crash_t/ck3_20261009_172556/exception.txt") << "EXCEPTION_STACK_OVERFLOW\n";
          std::ofstream("/tmp/rc_crash_t/ck3_20261009_172556/logs/game.log") << "[08:41:13][E][modifier_instance.cpp:390]: Unknown modifier type 'x' at file: common/traits/a.txt line: 7 (t)\n"; }
        auto cl = listCrashes("/tmp/rc_crash_t");
        assert(cl.size() == 3 && cl[0].name == "ck3_20261009_172556" && cl[1].name == "ck3_20250101_000000" && cl[2].name == "misc");
        CrashData dd = crashRead(cl[0].path);
        assert(dd.kind == CK_STACK && dd.logsFromCrash && dd.gameLog.find("Unknown modifier") != std::string::npos);
        assert(listCrashes("/tmp/rc_crash_t/none").empty() && !crashRead("/tmp/rc_crash_t/none").haveException);
        LogParse lp = crashLastMessages(dd.gameLog); assert(lp.entries.size() == 1);
        assert(std::string(crashKindName(CK_ACCESS)).size() > 5 && std::string(crashKindAdvice(CK_UNKNOWN)).size() > 5);
    }
    {   // languages: tables are complete, placeholders agree with the English text, lookups fall back to English
        assert(LANG_COUNT == 11 && std::string(LANGS[0].code) == "en");
        assert(langFromTag("de-DE") == langFromCode("de") && langFromTag("pt-PT") == langFromCode("pt-BR") && langFromTag("zh-Hans-CN") == langFromCode("zh"));
        assert(langFromTag("zh-TW") == 0 && langFromTag("xx") == 0 && langFromTag("") == 0 && langFromCode("zz") == -1);
        auto holes = [](const std::string& x) { std::string h; for (size_t i = 0; i + 2 < x.size(); i++) if (x[i] == '{' && x[i + 1] >= '0' && x[i + 1] <= '9' && x[i + 2] == '}') h += x[i + 1]; std::sort(h.begin(), h.end()); return h; };
        int missing = 0;
        for (int l = 1; l < LANG_COUNT; l++) for (int i = 0; i < LANG_N; i++) {
            const char* v = LANG_VALUES[l][i];
            if (!v || !v[0]) { missing++; continue; }
            assert(holes(v) == holes(LANG_KEYS[i]));
        }
        assert(missing == 0);
        setLanguage(langFromCode("de"));
        assert(trText("Cancel") != "Cancel" || std::string(LANG_KEYS[0]).empty());
        assert(trText("no such sentence xyz") == "no such sentence xyz");
        assert(trf("{0} of {1}", {1, 2}).find('1') != std::string::npos);
        setLanguage(0);
        assert(trText("Cancel") == "Cancel");
        Settings sl; sl.language = "ru"; setenv("RC_DATA_DIR", "/tmp/rc_lang_test", 1); saveSettings(sl);
        Settings sl2; loadSettings(sl2); assert(sl2.language == "ru");
        sl.language = "bogus"; saveSettings(sl); Settings sl3; loadSettings(sl3); assert(sl3.language == "auto");
    }
    // ---- audit fixes ----
    {   // a descriptor path that ends in a backslash still closes its string; UNC paths keep both slashes
        auto kv = parseDescriptor("name=\"X\"\npath=\"C:\\mods\\foo\\\"\nversion=\"1\"\n");
        assert(kv["path"].size() == 1 && kv["path"][0] == "C:\\mods\\foo\\" && kv["version"][0] == "1");
        auto kv2 = parseDescriptor("name=\"A \\\"quoted\\\" name\"\n");
        assert(kv2["name"][0] == "A \"quoted\" name");
    }
    {   // duplicates in a playset file: enabled if either entry is
        std::vector<ModInfo> none;
        std::string js = "{\"game\":\"ck3\",\"name\":\"d\",\"mods\":[{\"steamId\":\"5\",\"enabled\":false,\"position\":0},{\"steamId\":\"5\",\"enabled\":true,\"position\":1}]}";
        ImportResult r = parsePlaysetFile(js, none);
        assert(r.ok && r.playset.mods.size() == 1 && r.playset.mods[0].enabled);
    }
    {   // unsubscribed list survives a settings round trip
        Settings a; a.unsubbed["ugc_9.mod"] = 1760000000; a.hidden.insert("ugc_9.mod");
        assert(saveSettings(a));
        Settings b; loadSettings(b);
        assert(b.unsubbed.size() == 1 && b.unsubbed["ugc_9.mod"] == 1760000000 && b.hidden.count("ugc_9.mod"));
        a.unsubbed.clear(); a.hidden.clear(); saveSettings(a);
    }

    std::cout << "ALL CORE TESTS PASSED\n";
}
