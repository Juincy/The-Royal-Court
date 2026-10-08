#include <cassert>
#include <iostream>
#include "../core.hpp"
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
    assert(mig[1].mods[0].id == "ugc_222.mod" && mig[1].mods[1].id == "ugc_111.mod" && mig[1].mods[2].id == "local:gone.mod" && mig[1].mods[2].enabled);   // uninstalled local mod: kept by name (launcher files have no id for local mods)
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
    std::cout << "ALL CORE TESTS PASSED\n";
}
