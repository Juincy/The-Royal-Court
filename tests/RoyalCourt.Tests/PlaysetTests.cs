using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class PlaysetTests
{
    [Fact]
    public void ModScanning()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        Assert.Equal(3, mods.Count);
        var rf = mods.First(m => m.Id == "ugc_111.mod");
        Assert.True(rf.Name == "Rise and Fall" && rf.Version == "1.2" && rf.Supported == "1.20.*" && rf.Source == "Workshop");
        Assert.Empty(ModScan.ScanMods("")); Assert.Empty(ModScan.ScanMods(e.P("nope")));
    }

    [Fact]
    public void RealLauncherExportRoundTripsByteForByte()
    {
        string real = TestEnv.Fx("launcher_export_Vanilla+.json");
        var r = Playsets.Parse(real, new List<ModInfo>());
        Assert.True(r.Ok && r.Playset.Name == "Vanilla+" && r.Playset.Mods.Count == 18 && r.Unmatched == 0 && r.Invalid == 0);
        Assert.True(r.Playset.Mods[0].Id == "ugc_2697392271.mod" && r.Playset.Mods[17].Id == "ugc_2220098919.mod");
        Assert.Equal(real, Playsets.ExportLauncher(r.Playset, new Dictionary<string, ModInfo>()));
    }

    const string LauncherShapes = "{\"game\":\"ck3\",\"name\":\"Initial playset\",\"mods\":["
        + "{\"displayName\":\"Sub Rosa\",\"enabled\":true,\"position\":\"2\",\"steamId\":\"222\"},"
        + "{\"displayName\":\"Rise and Fall\",\"enabled\":false,\"position\":1,\"steamId\":111},"
        + "{\"displayName\":\"my local mod\",\"enabled\":true,\"position\":0},"
        + "{\"displayName\":\"Totally Unknown\",\"enabled\":true,\"position\":3},"
        + "{\"displayName\":\"Registry One\",\"gameRegistryId\":\"mod/ugc_333.mod\",\"position\":4},"
        + "{\"displayName\":\"Evil\",\"gameRegistryId\":\"mod/../../evil.mod\",\"position\":5,\"steamId\":\"x\"},"
        + "{\"enabled\":true},42]}";

    [Fact]
    public void OtherLauncherFileShapes()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        var r2 = Playsets.Parse(LauncherShapes, mods);
        Assert.True(r2.Ok && r2.Invalid == 2 && r2.Unmatched == 2);
        Assert.Equal("local.mod", r2.Playset.Mods[0].Id);
        Assert.True(r2.Playset.Mods[1].Id == "ugc_111.mod" && !r2.Playset.Mods[1].Enabled);
        Assert.Equal("ugc_222.mod", r2.Playset.Mods[2].Id);
        Assert.Equal("local:Totally Unknown", r2.Playset.Mods[3].Id);
        Assert.Equal("ugc_333.mod", r2.Playset.Mods[4].Id);
        Assert.False(Playsets.Parse("{\"game\":\"stellaris\",\"name\":\"x\",\"mods\":[]}", new List<ModInfo>()).Ok);
        Assert.False(Playsets.Parse("{bad", new List<ModInfo>()).Ok);
        Assert.False(Playsets.Parse("{\"hello\":1}", new List<ModInfo>()).Ok);
        Assert.False(Playsets.Parse("[]", new List<ModInfo>()).Ok);
    }

    [Fact]
    public void SyncReattachesPlaceholders()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        var sp = Playsets.Parse(LauncherShapes, mods).Playset;
        Playsets.Sync(sp, mods);
        var sub = sp.Mods.FirstOrDefault(m => m.Id == "ugc_222.mod"); Assert.NotNull(sub); Assert.Equal("Sub Rosa", sub!.Name);
        var later = new ModInfo { Id = "unknown_later.mod", Name = "Totally Unknown" };
        var withLater = new List<ModInfo>(mods) { later };
        Playsets.Sync(sp, withLater);
        var l = sp.Mods.FirstOrDefault(m => m.Id == "unknown_later.mod");
        Assert.True(l != null && l.Enabled);
        Assert.DoesNotContain(sp.Mods, m => m.Id.StartsWith("local:") && m.Name == "Totally Unknown");
    }

    [Fact]
    public void ExportRules()
    {
        var p = new Playset { Name = "Out" };
        p.Mods.Add(new ModRef("ugc_111.mod", true, "Rise and Fall"));
        p.Mods.Add(new ModRef("off_installed.mod", false, "Off"));
        p.Mods.Add(new ModRef("ugc_999.mod", false, "Ghost Off"));
        p.Mods.Add(new ModRef("local:Placeholder", true, "Placeholder"));
        var info = new Dictionary<string, ModInfo> { ["off_installed.mod"] = new ModInfo { Id = "off_installed.mod", Name = "Off" } };
        string ex = Playsets.ExportLauncher(p, info);
        Assert.DoesNotContain("\"Off\"", ex); Assert.Contains("Ghost Off", ex); Assert.Contains("Placeholder", ex);
        Assert.True(J.TryParse(ex, out var doc)); Assert.Equal(3, doc.Get("mods")!.A.Count);
        Assert.False(doc.Get("mods")!.A[1].Get("enabled")!.B); Assert.Null(doc.Get("mods")!.A[2].Get("steamId"));
    }

    [Fact]
    public void PlaysetFilesAreTheDatabase()
    {
        using var e = new TestEnv(); var mods = e.BaseMods(); string dir = e.Ck3;
        string real = TestEnv.Fx("launcher_export_Vanilla+.json");
        var r = Playsets.Parse(real, new List<ModInfo>());
        var se = Settings.Load(); Assert.True(se.Ck3Dir.Length == 0 && se.Active.Length == 0);
        se.Ck3Dir = dir; se.Active = "Vanilla+"; Assert.True(se.Save());
        var se2 = Settings.Load(); Assert.True(se2.Ck3Dir == dir && se2.Active == "Vanilla+" && se2.Theme.Length == 0);
        se2.Theme = "light"; Assert.True(se2.Save()); Assert.Equal("light", Settings.Load().Theme);
        var se3 = Settings.Load(); se3.Theme = "bogus"; se3.Save(); Assert.Equal("", Settings.Load().Theme);

        Assert.Empty(Playsets.LoadAll(mods));
        var a = r.Playset; a.Name = "Vanilla+";
        var im = Playsets.InfoMap(mods);
        Assert.True(Playsets.Save(a, im));
        string pd = Util.PlaysetsDir();
        Assert.True(File.Exists(Path.Combine(pd, "Vanilla+.json")));
        Assert.Equal(real, File.ReadAllText(Path.Combine(pd, "Vanilla+.json")));
        Assert.False(File.Exists(Path.Combine(pd, "Vanilla+.json.tmp")));
        var b = new Playset { Name = "Second" }; b.Mods.Add(new ModRef("ugc_111.mod", true, "")); Assert.True(Playsets.Save(b, im));
        var all = Playsets.LoadAll(mods);
        Assert.True(all.Count == 2 && all[0].Name == "Second" && all[1].Name == "Vanilla+");
        Assert.True(all[0].Mods[0].Id == "ugc_111.mod" && all[0].Mods[0].Enabled && all[0].Mods.Count == 3);
        Assert.True(Playsets.Rename(all[0], "Renamed", im));
        Assert.True(!File.Exists(Path.Combine(pd, "Second.json")) && File.Exists(Path.Combine(pd, "Renamed.json")));
        Assert.Contains("\"name\":\"Renamed\"", File.ReadAllText(Path.Combine(pd, "Renamed.json")));
        File.Move(Path.Combine(pd, "Renamed.json"), Path.Combine(pd, "By Hand.json"));
        all = Playsets.LoadAll(mods); Assert.True(all.Count == 2 && all[0].Name == "By Hand");
        Assert.True(Playsets.Delete("By Hand") && Playsets.LoadAll(mods).Count == 1);
        File.WriteAllText(Path.Combine(pd, "garbage.json"), "not json at all");
        Assert.Single(Playsets.LoadAll(mods));
    }

    [Fact]
    public void FileNames()
    {
        Assert.True(Util.SanitizeFileName("a/b:c*?") == "a_b_c__" && Util.SanitizeFileName("...") == "playset" && Util.SanitizeFileName("  x  ") == "x");
        Assert.True(Util.SanitizeFileName("CON") == "CON_" && Util.SanitizeFileName("com3.txt") == "com3.txt_" && Util.SanitizeFileName("Vanilla+") == "Vanilla+");
        Assert.Equal(80, Util.SanitizeFileName(new string('a', 200)).Length);
        string jp = string.Concat(Enumerable.Repeat("\u00e9", 100));
        Assert.Equal(80, Util.SanitizeFileName(jp).Length);   // deliberate divergence: the C++ cut at 80 bytes, the port at 80 characters (never inside a surrogate pair)
        Assert.Equal(80, Util.SanitizeFileName(string.Concat(Enumerable.Repeat("\U0001F451", 60))).Length);
        Assert.True(Util.FileStem("/tmp/Vanilla+.json") == "Vanilla+" && Util.FileStem("/tmp/My Set.v2.json") == "My Set.v2");
        var names = new List<Playset> { new() { Name = "Vanilla+" } };
        using var e = new TestEnv();
        Assert.True(Playsets.UniqueName(names, "vanilla+") == "vanilla+ (2)" && Playsets.UniqueName(names, "Fresh") == "Fresh" && Playsets.UniqueName(names, "") == "Imported playset");
    }

    [Fact]
    public void SizeCapAndIds()
    {
        using var e = new TestEnv();
        File.WriteAllText(e.P("big.json"), new string('x', 9 << 20));
        Assert.False(Util.ReadFile(e.P("big.json"), out _));
        Assert.True(Names.SteamIdOf("ugc_5.mod") == "5" && Names.SteamIdOf("ugc_x.mod").Length == 0 && Names.SteamIdOf("local.mod").Length == 0);
        Assert.True(!Playsets.ValidID("../x.mod") && !Playsets.ValidID("a/b.mod") && Playsets.ValidID("ugc_1.mod"));
    }

    [Fact]
    public void LegacyMigration()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        e.W("data/playsets.json", "{\"active\":\"Imported\",\"ck3Dir\":\"\",\"playsets\":[{\"name\":\"Imported\",\"mods\":["
            + "{\"id\":\"ugc_222.mod\",\"enabled\":true,\"name\":\"Sub Rosa\"},{\"id\":\"ugc_111.mod\",\"enabled\":true,\"name\":\"Rise and Fall\"},"
            + "{\"id\":\"local.mod\",\"enabled\":false},{\"id\":\"gone.mod\",\"enabled\":true}]},"
            + "{\"name\":\"Bad/Name\",\"mods\":[]}]}");
        var ms2 = new Settings(); Assert.Equal(2, Playsets.MigrateLegacyStore(mods, ms2));
        Assert.True(ms2.Active == "Imported" && !e.Ex("data/playsets.json") && e.Ex("data/playsets.json.migrated"));
        var mig = Playsets.LoadAll(mods);
        Assert.True(mig.Count == 2 && mig[0].Name == "Bad_Name" && mig[1].Name == "Imported");
        Assert.True(mig[1].Mods[0].Id == "ugc_222.mod" && mig[1].Mods[1].Id == "ugc_111.mod" && mig[1].Mods[2].Id == "gone.mod" && mig[1].Mods[2].Enabled);
        Assert.Equal(0, Playsets.MigrateLegacyStore(mods, ms2));
    }

    [Fact]
    public void StartingTheGameWithoutTheLauncher()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        string vdf = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t\t\"label\"\t\t\"\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"1158310\"\t\t\"123\"\n\t\t}\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t}\n}\n";
        var libs = Steam.LibraryPaths(vdf);
        Assert.True(libs.Count == 2 && libs[0] == "C:\\Program Files (x86)\\Steam" && libs[1] == "D:\\SteamLibrary");
        Assert.Equal("Crusader Kings III", Steam.AcfInstallDir("\"AppState\"\n{\n\t\"appid\"\t\t\"1158310\"\n\t\"installdir\"\t\t\"Crusader Kings III\"\n}"));
        Assert.True(Steam.AcfInstallDir("\"AppState\" { \"installdir\" \"CK3 Custom\" }") == "CK3 Custom" && Steam.AcfInstallDir("junk") == "Crusader Kings III");

        Directory.CreateDirectory(e.P("game")); string gdir = e.P("game");
        var gp = new Playset { Name = "Play me" };
        gp.Mods.Add(new ModRef("ugc_222.mod", true, "Sub Rosa"));
        gp.Mods.Add(new ModRef("ugc_111.mod", false, "Rise and Fall"));
        gp.Mods.Add(new ModRef("local.mod", true, "My Local Mod"));
        gp.Mods.Add(new ModRef("ugc_999.mod", true, "Not installed"));
        gp.Mods.Add(new ModRef("local:Placeholder", true, "Placeholder"));
        var inst = new HashSet<string>(mods.Select(m => m.Id));
        var w1 = Steam.WriteGameModList(gdir, gp, inst);
        Assert.True(w1.Ok && w1.Written == 2 && w1.Skipped == 2 && !e.Ex("game/dlc_load.json.rc-original"));
        Assert.True(J.TryParse(e.R("game/dlc_load.json"), out var d1));
        Assert.True(d1.Get("enabled_mods")!.A.Count == 2 && d1.Get("enabled_mods")!.A[0].S == "mod/ugc_222.mod" && d1.Get("enabled_mods")!.A[1].S == "mod/local.mod" && d1.Get("disabled_dlcs") != null);
        string users = "{\"enabled_mods\":[\"mod/their_own.mod\"],\"disabled_dlcs\":[\"dlc001\"],\"extra\":{\"a\":1}}";
        e.W("game/dlc_load.json", users);
        Assert.True(Steam.WriteGameModList(gdir, gp, inst).Ok);
        Assert.Equal(users, e.R("game/dlc_load.json.rc-original"));
        Assert.True(J.TryParse(e.R("game/dlc_load.json"), out var d2));
        Assert.True(d2.Get("extra") != null && d2.Get("disabled_dlcs")!.A.Count == 1 && d2.Get("enabled_mods")!.A.Count == 2);
        Assert.True(Steam.WriteGameModList(gdir, gp, inst).Ok);
        Assert.Equal(users, e.R("game/dlc_load.json.rc-original"));
        Assert.False(e.Ex("game/dlc_load.json.tmp"));
        e.W("game/dlc_load.json", "{not json");
        var w3 = Steam.WriteGameModList(gdir, new Playset { Name = "Vanilla" }, inst); Assert.True(w3.Ok && w3.Written == 0);
        Assert.False(Steam.WriteGameModList("", gp, inst).Ok); Assert.False(Steam.WriteGameModList(e.P("no_such_dir"), gp, inst).Ok);
        var gs = new Settings { GameExe = "C:\\Games\\ck3.exe", Active = "x" }; Assert.True(gs.Save());
        Assert.Equal("C:\\Games\\ck3.exe", Settings.Load().GameExe);
    }

    [Fact]
    public void AuditFixes_v051()
    {
        using var e = new TestEnv(); var mods = e.BaseMods();
        File.WriteAllBytes(e.P("ck3", "mod", "bom.mod"), new byte[] { 0xEF, 0xBB, 0xBF }.Concat(System.Text.Encoding.UTF8.GetBytes("name=\"Bom Mod\"\nversion=\"2\"\n")).ToArray());
        var bom = ModScan.ScanMods(e.Ck3).FirstOrDefault(m => m.Id == "bom.mod");
        Assert.True(bom != null && bom.Name == "Bom Mod" && bom.Version == "2");
        File.Delete(e.P("ck3", "mod", "bom.mod"));

        Directory.CreateDirectory(e.P("game2"));
        File.WriteAllText(e.P("game2", "dlc_load.json"), new string('x', 9 << 20));
        var wr = Steam.WriteGameModList(e.P("game2"), new Playset { Name = "p" }, new HashSet<string>());
        Assert.True(!wr.Ok && new FileInfo(e.P("game2", "dlc_load.json")).Length == (9L << 20) && !e.Ex("game2/dlc_load.json.rc-original"));

        string pd = Util.PlaysetsDir(); foreach (var f in Directory.GetFiles(pd)) File.Delete(f);
        File.WriteAllText(Path.Combine(pd, "Zed.json"), "{}");
        var none = new List<Playset>();
        Assert.True(Playsets.NameTaken(none, "Zed") && Playsets.UniqueName(none, "Zed") == "Zed (2)" && !Playsets.NameTaken(none, "Other"));

        var cv = new Playset { Name = "vanilla" }; cv.Mods.Add(new ModRef("ugc_111.mod", true, "Rise and Fall"));
        var im2 = Playsets.InfoMap(mods);
        Assert.True(Playsets.Save(cv, im2));
        Assert.False(Playsets.NameTaken(new List<Playset> { cv }, "Vanilla", "vanilla"));
        Assert.True(Playsets.Rename(cv, "Vanilla", im2) && cv.Name == "Vanilla");
        Assert.Equal(1, Directory.GetFiles(pd).Count(f => Path.GetFileName(f).ToLowerInvariant() == "vanilla.json"));
        Assert.True(File.Exists(Path.Combine(pd, "Vanilla.json")));
        Assert.Contains("\"name\":\"Vanilla\"", File.ReadAllText(Path.Combine(pd, "Vanilla.json")));

        var d1 = new ModInfo { Id = "a1.mod", Name = "Same Name" }; var d2 = new ModInfo { Id = "b2.mod", Name = "same name" };
        var rr = Playsets.Parse("{\"game\":\"ck3\",\"name\":\"d\",\"mods\":[{\"displayName\":\"SAME NAME\",\"enabled\":true,\"position\":0}]}", new List<ModInfo> { d1, d2 });
        Assert.True(rr.Ok && rr.Playset.Mods.Count == 1 && rr.Playset.Mods[0].Id == "a1.mod");

        var many = new List<ModInfo>(); var sb = new System.Text.StringBuilder("{\"game\":\"ck3\",\"name\":\"big\",\"mods\":[");
        for (int i = 0; i < 2000; i++)
        {
            many.Add(new ModInfo { Id = "local" + i + ".mod", Name = "Local Mod " + i });
            if (i > 0) sb.Append(',');
            sb.Append("{\"displayName\":\"local mod " + i + "\",\"enabled\":true,\"position\":" + i + "}");
        }
        sb.Append("]}");
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var rb = Playsets.Parse(sb.ToString(), many);
        Assert.True(rb.Ok && rb.Playset.Mods.Count == 2000 && rb.Unmatched == 0 && rb.Playset.Mods[1999].Id == "local1999.mod");
        Assert.True(sw.ElapsedMilliseconds < 3000, "matching 2000 local mods must not be quadratic");
    }

    [Fact]
    public void GameVersion()
    {
        Assert.Equal("1.14.1", ModScan.GameVersionFromSettings("{\"rawVersion\":\"1.14.1\",\"version\":\"1.14.1 (Scythe)\"}"));
        Assert.Equal("1.14.1.2", ModScan.GameVersionFromSettings("{\"version\":\"v1.14.1.2 (Scythe)\"}"));
        Assert.True(ModScan.GameVersionFromSettings("{\"rawVersion\":5}") == "" && ModScan.GameVersionFromSettings("garbage") == "");
        VerMatch M(string s, string i) => ModScan.MatchGameVersion(s, i);
        Assert.Equal(VerMatch.Match, M("1.14.*", "1.14.1")); Assert.Equal(VerMatch.Match, M("\"v1.14.*\"", "1.14.1"));
        Assert.Equal(VerMatch.Mismatch, M("1.13.*", "1.14.1")); Assert.Equal(VerMatch.Match, M("1.14", "1.14.3")); Assert.Equal(VerMatch.Match, M("1.14.2", "1.14.3"));
        Assert.Equal(VerMatch.Mismatch, M("1.19.0", "1.20.0")); Assert.Equal(VerMatch.Mismatch, M("2.0", "1.20.0"));
        Assert.Equal(VerMatch.Match, M("1.14.3.1", "1.14.3"));
        Assert.Equal(VerMatch.Match, M("1.20.0.3", "1.20.0.4")); Assert.Equal(VerMatch.Match, M("1.*.*", "1.20.0.4")); Assert.Equal(VerMatch.Match, M("1.20.*.*", "1.20.0.4"));
        Assert.Equal(VerMatch.Match, M("1.*", "1.14.3")); Assert.Equal(VerMatch.Unknown, M("1.1*", "1.14.3"));
        Assert.Equal(VerMatch.Unknown, M("", "1.14.3")); Assert.Equal(VerMatch.Unknown, M("1.14.*", ""));
        Assert.Equal(VerMatch.Match, M("1.014.*", "1.14.0")); Assert.Equal(VerMatch.Mismatch, M("99999999999.*", "1.0"));
        using var e = new TestEnv();
        Directory.CreateDirectory(e.P("g", "binaries")); Directory.CreateDirectory(e.P("g", "launcher"));
        File.WriteAllText(e.P("g", "launcher", "launcher-settings.json"), "{\"rawVersion\":\"1.15.0\"}");
        Assert.Equal("1.15.0", ModScan.ReadGameVersion(e.P("g", "binaries", "ck3.exe")));
        Assert.True(ModScan.ReadGameVersion(e.P("g", "nope", "x", "ck3.exe")).Length == 0 && ModScan.ReadGameVersion("").Length == 0);
    }

    [Fact]
    public void SavesScan()
    {
        using var e = new TestEnv(); string d = e.P("saves");
        Assert.True(ModScan.ScanSaves(d).Entries.Count == 0 && ModScan.ScanSaves("").Entries.Count == 0);
        e.W("saves/save games/a.ck3", "12345"); e.W("saves/save games/sub/b.ck3", "123");
        var sc = ModScan.ScanSaves(d);
        Assert.True(sc.Entries.Count == 2 && sc.Files == 2 && sc.Bytes == 8);
    }

    static List<string> K(Dictionary<string, List<string>> kv, string k) => kv.TryGetValue(k, out var v) ? v : new List<string>();

    [Fact]
    public void DescriptorReaderChecksAndNaturalSort()
    {
        string d = "\uFEFF# comment\r\nversion=\"1.4\"\r\ntags={\r\n\t\"Gameplay\"\r\n\t\"Balance\"\r\n}\r\nname=\"Foo \\\"Bar\\\"\"\r\nsupported_version=\"1.20.*\"\r\n"
            + "path=\"mod/foo\"\ndependencies = { \"Dep One\" \"Dep Two\" } # trailing\nreplace_path=\"history/titles\"\nreplace_path=\"common/traits\"\nremote_file_id=\"123\"\nempty={ }\nnested={ a={ x } }\n";
        var kv = ModScan.ParseDescriptor(d);
        Assert.True(K(kv, "name").Count == 1 && K(kv, "name")[0] == "Foo \"Bar\"");
        Assert.True(K(kv, "version")[0] == "1.4" && K(kv, "supported_version")[0] == "1.20.*" && K(kv, "path")[0] == "mod/foo");
        Assert.True(K(kv, "tags").Count == 2 && K(kv, "tags")[1] == "Balance");
        Assert.True(K(kv, "dependencies").Count == 2 && K(kv, "dependencies")[1] == "Dep Two");
        Assert.True(K(kv, "replace_path").Count == 2 && K(kv, "replace_path")[1] == "common/traits");
        Assert.True(K(kv, "empty").Count == 0 && K(kv, "nested").Count == 1 && K(kv, "nested")[0] == "a");
        Assert.Empty(ModScan.ParseDescriptor(""));
        ModScan.ParseDescriptor("name=\"unterminated\nversion="); ModScan.ParseDescriptor("= = { } } { name");
        ModScan.ParseDescriptor(new string('{', 1000)); ModScan.ParseDescriptor("a\0b=\"c\"");

        using var e = new TestEnv(); string root = e.P("desc");
        Directory.CreateDirectory(Path.Combine(root, "mod", "foo"));
        File.WriteAllText(Path.Combine(root, "mod", "foo.mod"), d);
        File.WriteAllText(Path.Combine(root, "mod", "gone.mod"), "name=\"Gone\"\npath=\"mod/nothere\"\n");
        File.WriteAllText(Path.Combine(root, "mod", "arch.mod"), "name=\"Arch\"\narchive=\"x.zip\"\n");
        var mods = ModScan.ScanMods(root); Assert.Equal(3, mods.Count);
        var inf = Playsets.InfoMap(mods);
        Assert.True(inf["foo.mod"].ContentState == 1 && inf["gone.mod"].ContentState == 2 && inf["arch.mod"].ContentState == 0);
        Assert.True(inf["foo.mod"].Deps.Count == 2 && inf["foo.mod"].ReplacePaths.Count == 2 && inf["foo.mod"].Tags.Count == 2);
    }

    static ModInfo Mk(string id, string name, string[]? deps = null, string sup = "") => new() { Id = id, Name = name, Deps = (deps ?? Array.Empty<string>()).ToList(), Supported = sup };

    [Fact]
    public void AnalyzePlaysetRules()
    {
        var im = new List<ModInfo> { Mk("a.mod", "Alpha", new[] { "Lib", "Missing", "Off" }), Mk("lib.mod", "Lib"), Mk("off.mod", "Off"), Mk("late.mod", "Late", new[] { "Alpha" }), Mk("d1.mod", "Dup"), Mk("d2.mod", "dup", null, "1.19.*") };
        im[1].ReplacePaths = new List<string> { "history/x" };
        var info2 = Playsets.InfoMap(im);
        var p = new Playset { Name = "t" };
        foreach (var (id, en) in new[] { ("late.mod", true), ("a.mod", true), ("lib.mod", true), ("off.mod", false), ("d1.mod", true), ("d2.mod", true), ("ghost.mod", true), ("ghost2.mod", false) }) p.Mods.Add(new ModRef(id, en, ""));
        var r = Analyze.AnalyzePlayset(p, info2, "1.20.0.4");
        bool Has(int i, string sub) => r[i].Any(x => x.Text.Contains(sub));
        Assert.True(Has(0, "should load before"));
        Assert.True(Has(1, "\"Missing\" (not installed)") && Has(1, "\"Off\" (installed, not enabled)") && Has(1, "\"Lib\" should load before"));
        Assert.True(r[2].Count == 1 && r[2][0].Sev == 0);
        Assert.Empty(r[3]);
        Assert.True(Has(4, "Same name as enabled mod #6") && Has(5, "Same name as enabled mod #5") && Has(5, "Made for game 1.19.*"));
        Assert.True(r[6].Count == 1 && r[6][0].Sev == 2 && r[7].Count == 1 && r[7][0].Sev == 0);
        {
            var om = new List<ModInfo> { Mk("u.mod", "Old Workshop", null, "1.12.*"), Mk("v.mod", "Old Local", null, "1.12.*"), Mk("w.mod", "Fine Workshop", null, "1.20.*") };
            om[0].Source = "Workshop"; om[1].Source = "Local"; om[2].Source = "Workshop";
            var po = new Playset { Name = "o" }; foreach (var m in om) po.Mods.Add(new ModRef(m.Id, true, ""));
            var ro = Analyze.AnalyzePlayset(po, Playsets.InfoMap(om), "1.20.1.1");
            Assert.True(ro[0].Count == 2 && ro[0][0].Sev == 1 && ro[0][1].Sev == 0 && ro[0][1].Text.Contains("Steam may be serving"));
            Assert.True(ro[1].Count == 1 && ro[2].Count == 0);
        }
        Assert.True(Analyze.IssueSeverity(r[1]) == 2 && Analyze.IssueSeverity(r[2]) == 0 && Analyze.IssueSummary(r[2]).Length == 0 && Analyze.IssueSummary(r[1]).Length > 0);
        Assert.Contains("(+", Analyze.IssueSummary(r[1]));
        Assert.True(Health.NaturalCompare("1.9", "1.10") < 0 && Health.NaturalCompare("1.10", "1.9") > 0 && Health.NaturalCompare("a", "A") == 0);
        Assert.True(Health.NaturalCompare("v0.6", "1.0") > 0 && Health.NaturalCompare("", "x") < 0 && Health.NaturalCompare("007", "7") == 0 && Health.NaturalCompare("1.20.*", "1.20.0") != 0);
    }

    [Fact]
    public void ConflictFinder()
    {
        using var e = new TestEnv(); string root = e.P("conf");
        foreach (var f in new[] { "common/traits/a.txt", "Events/B.TXT", ".git/config", "gfx/x/y.dds", "descriptor.mod", "thumbnail.png", "localization/english/l.yml" }) e.W("conf/" + f, "x");
        var mf = Walk.IndexModFiles(root);
        Assert.True(mf.Complete && mf.Files.Count == 4);
        Assert.Contains("events/b.txt", mf.Files); Assert.DoesNotContain(".git/config", mf.Files);
        Assert.False(Walk.IndexModFiles(root, new CancellationToken(true)).Complete);
        Assert.True(!Walk.IndexModFiles(Path.Combine(root, "nope")).Complete || Walk.IndexModFiles(Path.Combine(root, "nope")).Files.Count == 0);
        Assert.True(Conflicts.AreaOf("common/traits/x.txt") == "common/traits" && Conflicts.AreaOf("events/x.txt") == "events" && Conflicts.AreaOf("x") == "x");

        ModInfo M(string id, string name, params string[] rp) => new() { Id = id, Name = name, ReplacePaths = rp.ToList() };
        var im = new List<ModInfo> { M("a.mod", "A"), M("b.mod", "B"), M("c.mod", "C", "common/traits"), M("d.mod", "D") };
        var info = Playsets.InfoMap(im);
        ModFiles F(bool complete, params string[] files) => new() { Files = files.ToList(), Complete = complete };
        var idx = new Dictionary<string, ModFiles>
        {
            ["a.mod"] = F(true, "common/traits/t.txt", "events/e.txt", "gfx/g.dds", "common/only_a.txt"),
            ["b.mod"] = F(true, "common/traits/t.txt", "events/e.txt", "gfx/gb.dds"),
            ["c.mod"] = F(true, "common/traits/new.txt", "events/e.txt"),
            ["d.mod"] = F(false, "x/y.txt"),
        };
        var p = new Playset { Name = "t" }; foreach (var id in new[] { "a.mod", "b.mod", "c.mod", "d.mod", "gone.mod" }) p.Mods.Add(new ModRef(id, true, ""));
        var cr = Conflicts.Find(p, info, idx);
        Assert.True(cr.Valid && cr.ModsIndexed == 3 && cr.ModsWithoutFiles == 1);
        Assert.Equal(2, cr.Files.Count);
        Assert.True(cr.Files[0].Path == "common/traits/t.txt" && cr.Files[0].Mods.SequenceEqual(new[] { 0, 1 }));
        Assert.True(cr.Files[1].Path == "events/e.txt" && cr.Files[1].Mods.SequenceEqual(new[] { 0, 1, 2 }));
        Assert.True(cr.Wins[1] == 1 && cr.Wins[2] == 1 && cr.Loses[0] == 2 && cr.Loses[1] == 1 && cr.Loses[2] == 0);
        Assert.True(cr.Pairs.Count == 3 && cr.Pairs[0].A == 0 && cr.Pairs[0].B == 1 && cr.Pairs[0].Count == 2);
        Assert.Equal(2, cr.Pairs[0].Areas.Count);
        Assert.True(cr.Wipes.Count == 2 && cr.Wipes[0].Replacer == 2 && cr.Wipes[0].Folder == "common/traits");
        Assert.True((cr.Wipes[0].Victim == 0 || cr.Wipes[0].Victim == 1) && cr.Wipes[0].Files == 1);
        var iss = Analyze.AnalyzePlayset(p, info, "");
        Analyze.AddConflictIssues(iss, cr, p, info);
        Assert.True(Analyze.IssueSeverity(iss[2]) == 1 && Analyze.IssueSeverity(iss[0]) == 1 && Analyze.IssueSummary(iss[3]).Length == 0);
        p.Mods[1].Enabled = false;
        var cr2 = Conflicts.Find(p, info, idx);
        Assert.True(cr2.Files.Count == 1 && cr2.Files[0].Mods.SequenceEqual(new[] { 0, 2 }));
        var big = new List<ModInfo>(); var bi = new Dictionary<string, ModFiles>(); var bp = new Playset { Name = "b" };
        for (int i = 0; i < 20; i++) { string id = "m" + i + ".mod"; big.Add(M(id, id)); bi[id] = F(true, "common/x.txt"); bp.Mods.Add(new ModRef(id, true, "")); }
        var cr3 = Conflicts.Find(bp, Playsets.InfoMap(big), bi);
        Assert.True(cr3.Files.Count == 1 && cr3.Pairs.Count == 19 && cr3.Wins[19] == 1 && cr3.Loses[0] == 1);
    }
}
