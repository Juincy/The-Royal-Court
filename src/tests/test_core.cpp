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
    std::cout << "ALL CORE TESTS PASSED\n";
}
