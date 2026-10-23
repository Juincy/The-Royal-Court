using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class CoreTests2
{
    static ModInfo Mi(string id, string name) => new() { Id = id, Name = name };

    [Fact]
    public void Backups()
    {
        using var e = new TestEnv();
        var ms = Enumerable.Range(0, 4).Select(i => Mi($"m{i}.mod", $"Mod {i}")).ToList();
        var inf = Playsets.InfoMap(ms);
        var ps = new Playset { Name = "My Set" }; foreach (var m in ms) ps.Mods.Add(new ModRef(m.Id, true, m.Name));
        string b1 = Playsets.Backup(ps, inf, "before sort");
        Assert.True(b1.Length > 0 && File.Exists(b1) && Playsets.ListBackups("My Set").Count == 1);
        Assert.Equal("", Playsets.Backup(ps, inf, "again"));
        var changed = ps.Clone(); (changed.Mods[0], changed.Mods[3]) = (changed.Mods[3], changed.Mods[0]); changed.Mods[1].Enabled = false;
        string b2 = Playsets.Backup(changed, inf, "x");
        Assert.True(b2.Length > 0 && Playsets.ListBackups("My Set").Count == 2 && Playsets.ListBackups("My Set")[0] == b2);
        Assert.True(Util.ReadFile(b1, out var text));
        var imp = Playsets.Parse(text, ms); Assert.True(imp.Ok);
        var now = changed.Clone(); now.Mods.Add(new ModRef("new.mod", false, "New"));
        Playsets.ApplyBackupOrder(now, imp.Playset);
        Assert.True(now.Mods.Count == 5 && now.Mods[0].Id == "m0.mod" && now.Mods[3].Id == "m3.mod" && now.Mods[1].Enabled && now.Mods[4].Id == "new.mod");
        string lastExpected = "";
        for (int i = 0; i < 6; i++)
        {
            var p2 = ps.Clone(); p2.Mods[0].Enabled = i % 2 == 0; p2.Mods[1].Enabled = i < 3; p2.Mods[2].Enabled = i < 4; p2.Mods[3].Enabled = i < 5;
            Playsets.Backup(p2, inf, "n", 3); lastExpected = Playsets.ExportLauncher(p2, inf);
            Thread.Sleep(1100);   // backup names carry a one-second stamp
        }
        var lb = Playsets.ListBackups("My Set"); Assert.Equal(3, lb.Count);
        Util.ReadFile(lb[0], out var lastData);
        Assert.Equal(lastExpected, lastData);
    }

    [Fact]
    public void ScriptParser()
    {
        var k = new List<ScriptKey>();
        Scripts.ScanScriptKeys("\uFEFF# comment\n@my_var = 5\nbrave = { # trailing\n  index = 1\n  opposites = { craven }\n}\nnamespace = x\ncraven={ a = \"}{\" }\n  \"quoted\" = { }\nmulti_line\n=\n{\n}\n", 0, k);
        Assert.True(k.Count == 4 && k[0].Key == "brave" && k[1].Key == "craven" && k[2].Key == "quoted" && k[3].Key == "multi_line");
        k.Clear(); Scripts.ScanScriptKeys("a = 1\nb = { }\n@c = 2\nd ?= 3\n", 1, k);
        Assert.True(k.Count == 3 && k[0].Key == "a" && k[1].Key == "b" && k[2].Key == "d");
        k.Clear(); Scripts.ScanScriptKeys("on_birth = {\n  effect = { x = 1 }\n  events = { a.1 }\n}\non_death = { events = { a.2 } }\n", 0, k);
        Assert.True(k.Count == 2 && (k[0].Flags & DF.Overwrites) != 0 && (k[1].Flags & DF.Overwrites) == 0);
        k.Clear(); Scripts.ScanScriptKeys("my.1 = { id_override_priority = 5 type = character_event }\nmy.2 = { }\n", 0, k);
        Assert.True(k.Count == 2 && (k[0].Flags & DF.Priority) != 0 && (k[1].Flags & DF.Priority) == 0);
        k.Clear(); Scripts.ScanScriptKeys("NGame = {\n  START_DATE = \"1066.9.15\"\n  LIST = { 1 2 3 }\n}\nNCharacter = { MAX = 5 }\n", 2, k);
        Assert.True(k.Count == 3 && k[0].Key == "NGame.START_DATE" && k[1].Key == "NGame.LIST" && k[2].Key == "NCharacter.MAX");
        var lk = new List<string>();
        Scripts.ScanLocKeys("\uFEFFl_english:\n # c\n trait_brave:0 \"Brave\"\n trait_x: \"Y\"\n bad key:0 \"no\"\n event.1.t:1 \"T\"\n", lk);
        Assert.True(lk.Count == 3 && lk[0] == "trait_brave" && lk[1] == "trait_x" && lk[2] == "event.1.t");
        Assert.True(Scripts.FileKind("localization/english/a_l_english.yml").Area == "localization/english" && (Scripts.FileKind("localization/english/a_l_english.yml").Flags & DF.Replace) == 0);
        Assert.True((Scripts.FileKind("localization/english/replace/a.yml").Flags & DF.Replace) != 0 && (Scripts.FileKind("localization/replace/english/a.yml").Flags & DF.Replace) != 0);
        Assert.True(Scripts.FileKind("common/traits/a.txt").Area == "common/traits" && Scripts.FileKind("common/defines/a.txt").Kind == DK.Define && Scripts.FileKind("gfx/a.txt").Kind < 0);
    }

    [Fact]
    public void ScriptConflictsOnDisk()
    {
        using var e = new TestEnv(); string root = e.P("script");
        void Put(string mod, string rel, string text) => e.W($"script/{mod}/{rel}", text);
        Put("a", "common/traits/00_a.txt", "brave = { x = 1 }\nonly_a = { }\n");
        Put("b", "common/traits/zz_b.txt", "brave = { x = 2 }\n");
        Put("a", "events/a_events.txt", "namespace = e\ne.1 = { }\ne.2 = { }\n");
        Put("b", "events/b_events.txt", "namespace = e\ne.1 = { }\ne.3 = { id_override_priority = 3 }\n");
        Put("c", "events/c_events.txt", "namespace = e\ne.3 = { }\n");
        Put("a", "localization/english/a_l_english.yml", "l_english:\n k1:0 \"a\"\n k2:0 \"a\"\n");
        Put("b", "localization/english/b_l_english.yml", "l_english:\n k1:0 \"b\"\n");
        Put("c", "localization/english/replace/c_l_english.yml", "l_english:\n k2:0 \"c\"\n");
        Put("a", "common/on_action/a.txt", "on_birth = { effect = { a = 1 } }\non_x = { events = { e.1 } }\n");
        Put("b", "common/on_action/b.txt", "on_birth = { effect = { b = 1 } }\non_x = { events = { e.2 } }\n");
        Put("c", "common/traits/00_shared.txt", "shared = { }\n");
        Put("a", "common/traits/00_shared.txt", "shared = { }\n");
        Put("a", "common/traits/00_x.txt", "shadow = { }\n");
        Put("b", "common/traits/zz_y.txt", "shadow = { }\n");
        Put("c", "common/traits/00_x.txt", "shadow = { }\n");
        Put("a", "common/traits/zz_a.txt", "unclear = { }\n");
        Put("b", "common/traits/00_b.txt", "unclear = { }\n");
        var ms = new List<ModInfo>(); foreach (var id in new[] { "a", "b", "c" }) ms.Add(new ModInfo { Id = id + ".mod", Name = id, ContentDir = Path.Combine(root, id), ContentState = 1 });
        var inf = Playsets.InfoMap(ms); var ps = new Playset { Name = "s" }; foreach (var m in ms) ps.Mods.Add(new ModRef(m.Id, true, ""));
        var idx = new Dictionary<string, ModDefs>();
        foreach (var m in ms) { var mf = Walk.IndexModFiles(m.ContentDir); Assert.True(mf.Complete); idx[m.Id] = Scripts.IndexModDefs(m.ContentDir, mf.Files); Assert.True(idx[m.Id].Complete); }
        var sr = Scripts.FindScriptConflicts(ps, inf, idx);
        Assert.True(sr.Valid && sr.ModsRead == 3);
        Scripts.ScriptConflict? Find(int kind, string key) => sr.Items.FirstOrDefault(c => c.Kind == kind && c.Key == key);
        var brave = Find(DK.Common, "brave");
        Assert.True(brave != null && brave.Hits.Count == 2 && brave.Winner == 1 && brave.Sev == 0);
        Assert.Null(Find(DK.Common, "shared"));
        var shadow = Find(DK.Common, "shadow");
        Assert.True(shadow != null && shadow.Hits.Count == 2 && shadow.Hits[0].Mod == 1 && shadow.Hits[1].Mod == 2 && shadow.Winner == -1);
        Assert.True(Find(DK.Event, "e.1") != null && Find(DK.Event, "e.1")!.Sev == 1);
        Assert.True(Find(DK.Event, "e.3") == null && Find(DK.Event, "e.2") == null);
        var k1 = Find(DK.Loc, "k1"); Assert.True(k1 != null && k1.Hits.Count == 2);
        Assert.Null(Find(DK.Loc, "k2"));
        Assert.True(Find(DK.OnAction, "on_birth") != null && Find(DK.OnAction, "on_x") == null);
        var unc = Find(DK.Common, "unclear"); Assert.True(unc != null && unc.Winner == -1 && unc.Sev == 1);
        Assert.Null(Find(DK.Common, "only_a"));
        var iss = Analyze.AnalyzePlayset(ps, inf, ""); Scripts.AddScriptIssues(iss, sr, ps);
        Assert.True(Analyze.IssueSeverity(iss[0]) == 1 && Analyze.IssueSeverity(iss[1]) == 1 && !Analyze.IssueSummary(iss[2]).Contains("event"));
        ps.Mods[1].Enabled = false;
        var sr2 = Scripts.FindScriptConflicts(ps, inf, idx);
        Assert.True(sr2.Items.Count < sr.Items.Count); foreach (var c in sr2.Items) foreach (var h in c.Hits) Assert.NotEqual(1, h.Mod);
        idx.Remove("c.mod"); Assert.Equal(1, Scripts.FindScriptConflicts(ps, inf, idx).ModsMissing);
    }

    [Fact]
    public void SettingsExtrasPlaysetComparisonUpdateDetection()
    {
        using var e = new TestEnv();
        var st = new Settings(); st.Hidden.Add("x.mod"); st.Hidden.Add("y.mod"); st.Launch["Main"] = "-debug_mode"; st.Seen["a.mod"] = "fp1";
        st.WinX = 10; st.WinY = 20; st.WinW = 1300; st.WinH = 700; st.WinMax = true; st.ColW = new List<int> { 64, 0, 100, 110, 90, 90, 0 };
        Assert.True(st.Save());
        var b = Settings.Load();
        Assert.True(b.Hidden.SetEquals(st.Hidden) && b.Launch.SequenceEqual(st.Launch) && b.Seen.SequenceEqual(st.Seen));
        Assert.True(b.WinX == 10 && b.WinY == 20 && b.WinW == 1300 && b.WinH == 700 && b.WinMax && b.ColW.SequenceEqual(st.ColW));
        File.Delete(Util.SettingsPath()); var e2 = Settings.Load(); Assert.True(e2.WinW == 0 && e2.Hidden.Count == 0);

        var inf = new Dictionary<string, ModInfo>(); foreach (var n in "abcde") inf[n + ".mod"] = Mi(n + ".mod", "Mod " + n);
        Playset Mk(string name, params (string, bool)[] l) { var p = new Playset { Name = name }; foreach (var (id, en) in l) p.Mods.Add(new ModRef(id + ".mod", en, "")); return p; }
        var A = Mk("A", ("a", true), ("b", true), ("c", true), ("d", true), ("e", false));
        var B = Mk("B", ("b", true), ("c", true), ("d", true), ("a", true), ("e", true));
        var d = Compare.ComparePlaysets(A, B, inf);
        Assert.True(d.Shared == 4 && !d.SameOrder);
        Assert.True(d.Moved.Count == 1 && d.Moved[0] == "Mod a");
        Assert.True(d.OnlyA.Count == 0 && d.OnlyB.Count == 0 && d.EnabledDiffers.Count == 1);
        var d2 = Compare.ComparePlaysets(A, Mk("C", ("a", true), ("b", true)), inf);
        Assert.True(d2.OnlyA.Count == 2 && d2.SameOrder);
        var iss = new List<ModIssue>[A.Mods.Count]; for (int i = 0; i < iss.Length; i++) iss[i] = new List<ModIssue>();
        var seen = new Dictionary<string, string> { ["a.mod"] = "1", ["b.mod"] = "1", ["e.mod"] = "1" };
        var now = new Dictionary<string, string> { ["a.mod"] = "1", ["b.mod"] = "2", ["c.mod"] = "9", ["e.mod"] = "2" };
        Compare.AddUpdateIssues(iss, A, seen, now);
        Assert.True(iss[0].Count == 0 && iss[1].Count == 1 && iss[1][0].Sev == 1);
        Assert.Empty(iss[2]); Assert.Empty(iss[4]);
    }

    [Fact]
    public void AgotFamilyAndTotalConversions()
    {
        Assert.True(Names.Norm("AGOT+") == "agot plus" && !Names.NameIs(Names.Norm("AGOT+"), "agot") && !Names.NameIs(Names.Norm("AGOT: Brightboar - Westerosi House Flavor"), "agot"));
        var ms = new List<ModInfo> { Mi("ugc_3554844335.mod", "Rise and Fall"), Mi("rui.mod", "RUI"), Mi("miv.mod", "More Interactive Vassals"), Mi("r1.mod", "Random Content"), Mi("vs.mod", "Valyrian Steel"), Mi("ap.mod", "AGOT+"), Mi("core.mod", "AGOT Submod Core"), Mi("agot.mod", "A Game of Thrones"),
            Mi("epe.mod", "Ethnicities & Portraits Expanded"), Mi("bg.mod", "Battle Graphics"), Mi("redux.mod", "Battle Graphics Redux Compatch"), Mi("cfp.mod", "Community Flavor Pack") };
        var ps = H.PS("agot", ms); var plan = H.Plan(ps, Playsets.InfoMap(ms), null, null, H.Db);
        var ids = H.Ids(plan, ps); int Pos(string id) => ids.IndexOf(id);
        Assert.True(Pos("agot.mod") == 0 && Pos("core.mod") == 1);
        Assert.True(Pos("vs.mod") > Pos("core.mod") && Pos("ap.mod") > Pos("core.mod"));
        Assert.True(Pos("miv.mod") > Pos("r1.mod") && Pos("miv.mod") > Pos("bg.mod") && Pos("ugc_3554844335.mod") == Pos("miv.mod") + 1 && Pos("rui.mod") == ids.Count - 1);
        Assert.True(Pos("bg.mod") > Pos("cfp.mod") && Pos("bg.mod") > Pos("epe.mod") && Pos("redux.mod") > Pos("bg.mod"));
        ms.Add(Mi("up.mod", "Unofficial Patch")); ms.Add(Mi("pod.mod", "Princes of Darkness: Extended Edition")); ms.Add(Mi("ui.mod", "Unique Artifacts + 1.20")); ms.Add(Mi("vd.mod", "Visible Disfigurement - No More Masks"));
        for (int i = ps.Mods.Count; i < ms.Count; i++) ps.Mods.Add(new ModRef(ms[i].Id, true, ""));
        var plan2 = H.Plan(ps, Playsets.InfoMap(ms), null, null, H.Db); var ids2 = H.Ids(plan2, ps); int P2(string id) => ids2.IndexOf(id);
        Assert.Equal(ps.Mods.Count, plan2.Order.Length);
        Assert.True(P2("agot.mod") < P2("core.mod") && P2("pod.mod") < P2("up.mod") && P2("core.mod") < P2("up.mod") && P2("agot.mod") < P2("up.mod") && P2("up.mod") == 3);
        Assert.True(P2("vd.mod") > P2("cfp.mod") && P2("vd.mod") > P2("epe.mod"));
        Assert.NotEmpty(plan2.Known[ms.FindIndex(m => m.Id == "ui.mod")]);
    }

    [Fact]
    public void ShaReleaseParsingLog()
    {
        Assert.Equal("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", Release.Sha256Hex(""));
        Assert.Equal("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", Release.Sha256Hex("abc"));
        Assert.Equal("41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3", Release.Sha256Hex(new string('a', 1000)));
        Assert.True(Release.CompareVersions("v0.14.0", "0.13.3") > 0 && Release.CompareVersions("0.13.3", "v0.13.3") == 0 && Release.CompareVersions("0.13", "0.13.0") == 0 && Release.CompareVersions("0.9.9", "0.10.0") < 0 && Release.CompareVersions("v1.0.0-beta", "0.99.9") > 0);
        Assert.Empty(Release.ParseVersion("garbage"));
        string js = "{\"tag_name\":\"v0.14.1\",\"name\":\"Release\",\"body\":\"notes\\nline\",\"html_url\":\"https://github.com/Juincy/the-royal-court/releases/tag/v0.14.1\",\"assets\":["
            + "{\"name\":\"TheRoyalCourt.exe\",\"size\":1234,\"browser_download_url\":\"https://github.com/Juincy/the-royal-court/releases/download/v0.14.1/TheRoyalCourt.exe\"},"
            + "{\"name\":\"evil.exe\",\"size\":1,\"browser_download_url\":\"https://evil.example.com/evil.exe\"},"
            + "{\"name\":\"SHA256SUMS.txt\",\"size\":99,\"browser_download_url\":\"https://github.com/juincy/The-Royal-Court/releases/download/v0.14.1/SHA256SUMS.txt\"}]}";
        var ri = Release.Parse(js);
        Assert.True(ri.Ok && ri.Tag == "v0.14.1" && ri.Body == "notes\nline" && ri.PageUrl.Length > 0);
        Assert.True(ri.Asset("TheRoyalCourt.exe") != null && ri.Asset("TheRoyalCourt.exe")!.Size == 1234 && ri.Asset("SHA256SUMS.txt") != null && ri.Asset("evil.exe") == null);
        Assert.False(Release.Parse("{}").Ok); Assert.False(Release.Parse("not json").Ok); Assert.False(Release.Parse("{\"tag_name\":\"nightly\"}").Ok);
        Assert.False(Release.TrustedUrl("https://github.com.evil.com/Juincy/the-royal-court/releases/x")); Assert.False(Release.TrustedUrl("http://github.com/Juincy/the-royal-court/releases/x"));
        string sums = new string('A', 64) + "  TheRoyalCourt.exe\r\n" + new string('b', 64) + " *other.zip\n";
        Assert.True(Release.FindSumFor(sums, "TheRoyalCourt.exe") == new string('a', 64) && Release.FindSumFor(sums, "other.zip") == new string('b', 64) && Release.FindSumFor(sums, "nope").Length == 0);
        using var e = new TestEnv();
        var st = new Settings { CheckUpdates = true, Pictures = false }; Assert.True(st.Save()); Assert.True(Settings.Load().CheckUpdates); Assert.False(Settings.Load().Pictures); st.Pictures = true; Assert.True(st.Save()); Assert.True(Settings.Load().Pictures);
        st.CheckUpdates = false; st.Save(); Assert.False(Settings.Load().CheckUpdates);
        Log.Line("test line"); Assert.True(Util.ReadFile(Log.LogPath(), out var lg) && lg.Contains("test line"));
    }

    [Fact]
    public void WorkshopDownloadsWithoutADescriptorYet()
    {
        using var e = new TestEnv(); string W = e.P("ws"); string content = Path.Combine(W, "content", "1158310");
        foreach (var d in new[] { "111111", "222222", "333333", "notanid" }) Directory.CreateDirectory(Path.Combine(content, d));
        File.WriteAllText(Path.Combine(content, "111111", "descriptor.mod"), "version=\"2.0\"\ntags={\n\t\"Gameplay\"\n}\nname=\"Fresh Mod\"\nsupported_version=\"1.20.*\"\npath=\"/old/place\"\n");
        File.WriteAllText(Path.Combine(content, "222222", "descriptor.mod"), "name=\"Already Registered\"\n");
        var pend = Steam.ScanWorkshopFolders(new[] { content, Path.Combine(W, "missing") }, new HashSet<string> { "ugc_222222.mod" });
        Assert.True(pend.Count == 1 && pend[0].Id == "ugc_111111.mod" && pend[0].Name == "Fresh Mod" && pend[0].Pending && pend[0].Source == "Workshop" && pend[0].ContentState == 1 && pend[0].Supported == "1.20.*");
        string reg = Steam.RegisteredDescriptorText("name=\"X\"\npath=\"/old/place\"\nremote_file_id=\"9\"\ntags={\n\t\"A\"\n}\n", "111111", "C:\\Steam\\content\\111111");
        Assert.True(!reg.Contains("/old/place") && reg.Contains("remote_file_id=\"111111\"") && reg.Contains("path=\"C:/Steam/content/111111\"") && reg.Contains("tags={"));
        string ck3 = Path.Combine(W, "ck3"); Directory.CreateDirectory(Path.Combine(ck3, "mod"));
        var ps = new Playset { Name = "P" }; ps.Mods.Add(new ModRef("ugc_111111.mod", true, "")); ps.Mods.Add(new ModRef("ugc_222222.mod", true, ""));
        var inf = new Dictionary<string, ModInfo> { [pend[0].Id] = pend[0] };
        Assert.Equal(1, Steam.RegisterPendingMods(ck3, ps, inf, out _));
        Assert.Contains("name=\"Fresh Mod\"", File.ReadAllText(Path.Combine(ck3, "mod", "ugc_111111.mod")));
        Assert.Equal(0, Steam.RegisterPendingMods(ck3, ps, inf, out _));
        var again = ModScan.ScanMods(ck3);
        Assert.True(again.Count == 1 && again[0].Id == "ugc_111111.mod" && again[0].Name == "Fresh Mod" && again[0].ContentState == 1);
        Assert.Empty(Steam.ScanWorkshopFolders(new[] { content }, new HashSet<string> { "ugc_111111.mod", "ugc_222222.mod" }));
        var ps2 = new Playset(); ps2.Mods.Add(new ModRef("ugc_111111.mod", true, ""));
        Assert.Contains(Analyze.AnalyzePlayset(ps2, inf, "1.20.0")[0], i => i.Text.Contains("Tick the box"));
    }

    [Fact]
    public void AuditFixes_v0150()
    {
        {   // local mods keep their identity when a name changes or repeats
            var inst = new List<ModInfo> { Mi("ugc_1.mod", "Same"), Mi("local1.mod", "Same"), Mi("local2.mod", "Old Name v1") };
            var ps = new Playset { Name = "p" }; foreach (var m in inst) ps.Mods.Add(new ModRef(m.Id, true, m.Name));
            string js = Playsets.ExportLauncher(ps, Playsets.InfoMap(inst));
            Assert.True(js.Contains("\"gameRegistryId\":\"mod/local1.mod\"") && !js.Contains("\"gameRegistryId\":\"mod/ugc_1.mod\""));
            inst[2].Name = "Old Name v2";
            var r = Playsets.Parse(js, inst); Playsets.Sync(r.Playset, inst);
            Assert.True(r.Playset.Mods.Count == 3 && r.Playset.Mods[0].Id == "ugc_1.mod" && r.Playset.Mods[1].Id == "local1.mod" && r.Playset.Mods[2].Id == "local2.mod");
            foreach (var m in r.Playset.Mods) Assert.True(m.Enabled);
        }
        {   // short known names only match exactly; every built-in name is already normalised
            var db = Known.Builtin();
            foreach (var k in db) foreach (var n in k.Names) Assert.Equal(n, Names.Norm(n));
            foreach (var nm in new[] { "AGOT Dothraki Rework", "MIV Lite", "Unofficial Patch Extras", "CFP Tweaks", "EPE Hairstyles" }) Assert.True(Known.Find(db, Mi("x.mod", nm)) < 0, nm);
            Assert.True(Known.Find(db, Mi("x.mod", "Elder Kings 2")) >= 0 && Known.Find(db, Mi("x.mod", "AGOT")) >= 0 && Known.Find(db, Mi("x.mod", "Rise and Fall 1.2")) >= 0);
            Assert.True(Known.Find(db, Mi("x.mod", "Rise and Fall Compatibility Patch")) < 0);
            var layered = Known.Builtin();
            Assert.True(Known.ApplyJson(layered, "{\"mods\":[{\"ids\":[\"3554844335\"],\"position\":\"top\",\"note\":\"mine\"}]}", out _));
            int k2 = Known.Find(layered, Mi("ugc_3554844335.mod", "Rise and Fall"));
            Assert.True(k2 >= 0 && layered[k2].Note == "mine" && layered[k2].Pos == -2);
        }
        {   // a circular dependency is broken inside the cycle, not at an innocent mod that waits on it
            var ps = new Playset { Name = "t" }; var info = new Dictionary<string, ModInfo>();
            void Add(string id, string name, params string[] deps) { var m = Mi(id, name); m.Deps = deps.ToList(); info[id] = m; ps.Mods.Add(new ModRef(id, true, name)); }
            Add("a.mod", "Alpha Content", "Beta Content"); Add("b.mod", "Beta Content", "Alpha Content"); Add("c.mod", "Gamma Library Core", "Alpha Content");
            var p = H.Plan(ps, info);
            int posA = -1, posC = -1;
            for (int i = 0; i < p.Order.Length; i++) { if (ps.Mods[p.Order[i]].Id == "a.mod") posA = i; if (ps.Mods[p.Order[i]].Id == "c.mod") posC = i; }
            Assert.True(posA >= 0 && posC > posA && p.Warnings.Count > 0);
        }
        Assert.Equal(VerMatch.Unknown, ModScan.MatchGameVersion("1.20", "1.*"));
        {   // renaming a playset keeps its backups
            using var e = new TestEnv();
            var ms = new List<ModInfo> { Mi("m0.mod", "Mod 0"), Mi("m1.mod", "Mod 1") }; var inf = Playsets.InfoMap(ms);
            var ps = new Playset { Name = "Old Name" }; foreach (var m in ms) ps.Mods.Add(new ModRef(m.Id, true, m.Name));
            Playsets.Save(ps, inf);
            Assert.True(Playsets.Backup(ps, inf, "x").Length > 0 && Playsets.ListBackups("Old Name").Count == 1);
            Assert.True(Playsets.Rename(ps, "New Name", inf));
            Assert.True(Playsets.ListBackups("New Name").Count == 1 && Playsets.ListBackups("Old Name").Count == 0);
            string longName = new string('x', 100);
            File.WriteAllText(Path.Combine(Util.PlaysetsDir(), longName + ".json"), Playsets.ExportLauncher(ps, inf));
            var loaded = Playsets.LoadAll(ms);
            Assert.True(loaded.Any(pl => pl.Name == Util.SanitizeFileName(longName)) && loaded.Count == 2);
            foreach (var pl in loaded) Playsets.Save(pl, inf);
            Assert.Equal(2, Playsets.LoadAll(ms).Count);
        }
        {   // a damaged settings file is kept as .bad; absurd numbers do not break loading
            using var e = new TestEnv();
            File.WriteAllText(Util.SettingsPath(), "{broken");
            var st = Settings.Load();
            Assert.True(Util.ReadFile(Util.SettingsPath() + ".bad", out var kept) && kept == "{broken");
            File.WriteAllText(Util.SettingsPath(), "{\"window\":{\"x\":1e30,\"y\":-1e30,\"w\":1300,\"h\":700,\"cols\":[100,1e99]}}");
            st = Settings.Load();
            Assert.True(st.WinW == 1300 && st.WinX == 100000000 && st.WinY == -100000000 && st.ColW.Count == 2 && st.ColW[1] == 100000000);
        }
        Assert.NotEmpty(Steam.RegisteredDescriptorText("name=\"x\"\n", "1", "C:/a"));
    }
}
