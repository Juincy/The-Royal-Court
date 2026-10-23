using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class IndexTests
{
    [Fact]
    public void DeepFingerprintSavedIndexAndPruning()
    {
        using var e = new TestEnv(); string root = e.P("deepmod");
        e.W("deepmod/descriptor.mod", "name=\"x\"");
        e.W("deepmod/common/traits/t.txt", "brave = { x = 1 }\nkind = { y = 2 }\n");
        e.W("deepmod/events/e.txt", "namespace = ee\nee.1 = { type = character_event }\n");
        e.W("deepmod/localization/english/a_l_english.yml", "l_english:\n k1:0 \"v\"\n");
        var a1 = Walk.IndexModFiles(root); var a2 = Walk.IndexModFiles(root);
        Assert.True(a1.Complete && a1.Deep.Length > 0 && a1.Deep == a2.Deep);
        e.W("deepmod/descriptor.mod", "name=\"changed\"");
        Assert.Equal(a1.Deep, Walk.IndexModFiles(root).Deep);
        e.W("deepmod/common/traits/t.txt", "brave = { x = 1 }\nkind = { y = 2 }\nmore = { z = 3 }\n");
        var a3 = Walk.IndexModFiles(root);
        Assert.True(a3.Deep != a1.Deep && a3.Files.Count == a1.Files.Count);
        string ef = Path.Combine(root, "events", "e.txt");
        File.SetLastWriteTimeUtc(ef, File.GetLastWriteTimeUtc(ef).AddSeconds(7));
        var a4 = Walk.IndexModFiles(root); Assert.NotEqual(a3.Deep, a4.Deep);
        e.W("deepmod/common/traits/new.txt", "n = { }");
        Assert.NotEqual(a4.Deep, Walk.IndexModFiles(root).Deep);

        string cdir = e.P("cache1");
        var i1 = Scripts.IndexModCached(cdir, "m1.mod", root);
        Assert.True(i1.Files.Complete && i1.Defs.Complete && !i1.FromCache && i1.Defs.Defs.Count > 0);
        var i2 = Scripts.IndexModCached(cdir, "m1.mod", root);
        Assert.True(i2.FromCache && i2.Defs.Defs.Count == i1.Defs.Defs.Count && i2.Defs.Pool == i1.Defs.Pool && i2.Defs.Files.SequenceEqual(i1.Defs.Files) && i2.Defs.BytesRead == i1.Defs.BytesRead);
        for (int k = 0; k < i1.Defs.Defs.Count; k++) Assert.True(i1.Defs.Defs[k].H == i2.Defs.Defs[k].H && i1.Defs.Defs[k].File == i2.Defs.Defs[k].File);
        e.W("deepmod/common/traits/t.txt", "brave = { x = 1 }\nnewone = { y = 2 }\n");
        var i3 = Scripts.IndexModCached(cdir, "m1.mod", root);
        Assert.False(i3.FromCache);
        Assert.Contains(i3.Defs.Defs, d => i3.Defs.NameOf(d) == "newone");
        Assert.False(Scripts.IndexModCached(cdir, "m2.mod", root).FromCache);
        string cf = Scripts.ModCacheFile(cdir, "m1.mod");
        byte[] good = File.ReadAllBytes(cf);
        string deepNow = Walk.IndexModFiles(root).Deep;
        Assert.True(Scripts.LoadModCache(cdir, "m1.mod", deepNow, out _));
        Assert.False(Scripts.LoadModCache(cdir, "m1.mod", deepNow + "x", out _)); Assert.False(Scripts.LoadModCache(cdir, "other.mod", deepNow, out _));
        foreach (int cut in new[] { 0, 3, good.Length / 2, good.Length - 1 }) { File.WriteAllBytes(cf, good.Take(cut).ToArray()); Assert.False(Scripts.LoadModCache(cdir, "m1.mod", deepNow, out _)); }
        foreach (int at in new[] { 5, 40, good.Length / 2, good.Length - 12 }) { var bad = (byte[])good.Clone(); bad[at] ^= 0x5a; File.WriteAllBytes(cf, bad); Assert.False(Scripts.LoadModCache(cdir, "m1.mod", deepNow, out _)); }
        File.WriteAllBytes(cf, good); Assert.True(Scripts.LoadModCache(cdir, "m1.mod", deepNow, out _));
        {   // a cache file that lies about its sizes (valid checksum, impossible counts) is refused without crashing
            var ms = new MemoryStream(); var w = new BinaryWriter(ms, Util.Utf8);
            void Str(string s) { var b = Util.Utf8.GetBytes(s); w.Write((uint)b.Length); w.Write(b); }
            w.Write(System.Text.Encoding.ASCII.GetBytes("RCIX")); w.Write(Scripts.IndexCacheFormat); w.Write(Scripts.DefsParserVersion); Str("m1.mod"); Str(deepNow); w.Write((ulong)0);
            w.Write(0xFFFFFFF0u);
            w.Write(Scripts.Fnv64Bytes(ms.ToArray(), '\x1d', "RCIX"));
            File.WriteAllBytes(cf, ms.ToArray());
            Assert.False(Scripts.LoadModCache(cdir, "m1.mod", deepNow, out _));
        }
        File.WriteAllBytes(cf, good);
        Scripts.IndexModCached(cdir, "m2.mod", root);
        File.WriteAllText(Path.Combine(cdir, "notes.txt"), "keep me"); File.WriteAllText(Path.Combine(cdir, "m_zzzzzzzzzzzzzzzz.bin"), "not ours");
        Assert.Equal(1, Scripts.PruneModCache(cdir, new[] { "m1.mod" }));
        Assert.True(File.Exists(Scripts.ModCacheFile(cdir, "m1.mod")) && !File.Exists(Scripts.ModCacheFile(cdir, "m2.mod")) && File.Exists(Path.Combine(cdir, "notes.txt")) && File.Exists(Path.Combine(cdir, "m_zzzzzzzzzzzzzzzz.bin")));
        Assert.Equal(0, Scripts.PruneModCache("", Array.Empty<string>()));
    }

    static ModInfo M2(string id, string name, string sup, params string[] rp) => new() { Id = id, Name = name, Supported = sup, ReplacePaths = rp.ToList() };
    static ModFiles Mkf(IEnumerable<string> f) => new() { Files = f.ToList(), Complete = true };

    [Fact]
    public void GameFileListAndModsAgainstIt()
    {
        using var e = new TestEnv(); string inst = e.P("ck3install");
        e.W("ck3install/binaries/ck3.exe", "x");
        foreach (var (f, t) in new[] { ("game/common/traits/00_traits.txt", "a"), ("game/common/traits/01_more.txt", "a"), ("game/events/e1.txt", "a"), ("game/gfx/x.dds", "a"), ("game/readme.txt", "a"), ("game/.git/config", "a"), ("game/Thumbs.db", "a"), ("dlc/dlc001_x/common/traits/dlc_traits.txt", "a"), ("dlc/dlc001_x/dlc.json", "a") }) e.W("ck3install/" + f, t);
        string exe = Path.Combine(inst, "binaries", "ck3.exe");
        string gdir = Vanilla.GameDir(exe);
        Assert.True(gdir.Length > 0 && Path.GetFileName(gdir) == "game" && Vanilla.GameDir("").Length == 0 && Vanilla.GameDir(e.P("nope", "bin", "x.exe")).Length == 0);
        var v1 = Vanilla.Index(gdir)!;
        Assert.True(v1 != null && v1.Has("common/traits/00_traits.txt") && v1.Has("events/e1.txt") && v1.Has("gfx/x.dds") && v1.Has("common/traits/dlc_traits.txt"));
        Assert.True(!v1.Has("readme.txt") && !v1.Has(".git/config") && !v1.Has("thumbs.db") && !v1.Has("dlc.json") && !v1.Has("common/traits/nothing.txt"));
        Assert.True(v1.CountUnder("common/traits/") == 3 && v1.CountUnder("events/") == 1 && v1.CountUnder("common/trait/") == 0 && v1.DlcFiles == 1);
        string vdir = e.P("cache2"); var fc = new bool[1];
        var v2 = Vanilla.LoadOrIndex(vdir, exe, "1.20.0.4", default, fc);
        Assert.True(v2 != null && !fc[0] && v2.Files.SequenceEqual(v1.Files));
        var v3 = Vanilla.LoadOrIndex(vdir, exe, "1.20.0.4", default, fc);
        Assert.True(v3 != null && fc[0] && v3.Files.SequenceEqual(v1.Files) && v3.DlcFiles == 1 && v3.Has("gfx/x.dds"));
        Vanilla.LoadOrIndex(vdir, exe, "1.21.0.1", default, fc); Assert.False(fc[0]);
        e.W("ck3install/game/newfolder/a.txt", "a");
        var v4 = Vanilla.LoadOrIndex(vdir, exe, "1.21.0.1", default, fc)!;
        Assert.True(!fc[0] && v4.Has("newfolder/a.txt"));
        string vc = Path.Combine(vdir, "vanilla.txt"); string vtext = File.ReadAllText(vc);
        foreach (int cut in new[] { 0, 5, vtext.Length / 2, vtext.Length - 1 }) { File.WriteAllText(vc, vtext.Substring(0, cut)); Assert.Null(Vanilla.LoadCache(vdir, v4.Key)); }
        File.WriteAllText(vc, vtext + "extra/line.txt\n"); Assert.Null(Vanilla.LoadCache(vdir, v4.Key));
        File.WriteAllText(vc, vtext);
        Assert.True(Vanilla.LoadCache(vdir, v4.Key) != null && Vanilla.LoadCache(vdir, v4.Key + "x") == null);
        Assert.Null(Vanilla.Index(gdir, new CancellationToken(true)));

        var vm = new List<ModInfo> { M2("a.mod", "A", "1.1.*"), M2("b.mod", "B", "1.20.*"), M2("c.mod", "C", "1.20.*", "common/traits"), M2("d.mod", "D", "1.20.*"), M2("e.mod", "E", "1.20.*") };
        var vinfo = Playsets.InfoMap(vm);
        var gf = Enumerable.Range(0, 5).Select(i => $"gfx/pair{i}.dds").ToList();
        var vidx = new Dictionary<string, ModFiles>
        {
            ["a.mod"] = Mkf(new[] { "common/traits/00_traits.txt", "gfx/x.dds", "common/mine.txt" }), ["b.mod"] = Mkf(new[] { "common/traits/00_traits.txt", "events/new.txt" }),
            ["c.mod"] = Mkf(new[] { "common/other/o.txt" }), ["d.mod"] = Mkf(gf), ["e.mod"] = Mkf(gf),
        };
        var vp = H.PS("v", vm);
        var vcr = Conflicts.Find(vp, vinfo, vidx, v1);
        Assert.True(vcr.Valid && vcr.VanillaChecked && vcr.Vanilla.Count == 2);
        Assert.True(vcr.Vanilla[0].Path == "common/traits/00_traits.txt" && vcr.Vanilla[0].Mods.SequenceEqual(new[] { 0, 1 }) && vcr.Vanilla[0].Sev == (int)Sev.High);
        Assert.True(vcr.Vanilla[1].Path == "gfx/x.dds" && vcr.Vanilla[1].Mods.SequenceEqual(new[] { 0 }) && vcr.Vanilla[1].Sev == (int)Sev.Low);
        Assert.True(vcr.VanillaCount[0] == 2 && vcr.VanillaCount[1] == 1 && vcr.VanillaCount[2] == 0);
        Assert.True(vcr.Files.Count == 6 && vcr.Files[0].Path == "common/traits/00_traits.txt" && vcr.Files[0].Vanilla && vcr.Files[0].Sev == (int)Sev.High && !vcr.Files[1].Vanilla && vcr.Files[1].Sev == (int)Sev.Low);
        Assert.True(vcr.Pairs.Count == 2 && vcr.Pairs[0].A == 0 && vcr.Pairs[0].B == 1 && vcr.Pairs[0].Sev == (int)Sev.High && vcr.Pairs[1].Count == 5 && vcr.Pairs[1].Sev == (int)Sev.Low);
        Assert.True(vcr.VanillaWipes.Count == 1 && vcr.VanillaWipes[0].Mod == 2 && vcr.VanillaWipes[0].Folder == "common/traits" && vcr.VanillaWipes[0].Files == 3);
        var viss = Analyze.AnalyzePlayset(vp, vinfo, "1.20.0.4"); Analyze.AddConflictIssues(viss, vcr, vp, vinfo, "1.20.0.4");
        Assert.Contains(viss[0], i => i.Text.Contains("older game version") && i.Sev == 1);
        Assert.Contains(viss[2], i => i.Text.Contains("removes 3 file(s) of the base game"));
        Assert.Contains(viss[1], i => i.Text.Contains("Replaces 1 file(s) of the base game") && i.Sev == 0);
        var ncr = Conflicts.Find(vp, vinfo, vidx);
        Assert.True(ncr.Valid && !ncr.VanillaChecked && ncr.Vanilla.Count == 0 && ncr.VanillaWipes.Count == 0 && ncr.Files.Count == 6);
        var ni = Analyze.AnalyzePlayset(vp, vinfo, "1.20.0.4"); Analyze.AddConflictIssues(ni, ncr, vp, vinfo, "1.20.0.4");
        foreach (var i in ni[0]) Assert.DoesNotContain("base game", i.Text);
        var hugeIdx = new Dictionary<string, ModFiles>(); var hm = new List<ModInfo>(); var hp = new Playset { Name = "h" };
        for (int m = 0; m < 2; m++) { string id = $"h{m}.mod"; hm.Add(M2(id, id, "")); hugeIdx[id] = Mkf(Enumerable.Range(0, 40000).Select(i => $"common/x/f{i}.txt")); hp.Mods.Add(new ModRef(id, true, "")); }
        Assert.False(Conflicts.Find(hp, Playsets.InfoMap(hm), hugeIdx, null, new CancellationToken(true)).Valid);
        Assert.Equal(40000, Conflicts.Find(hp, Playsets.InfoMap(hm), hugeIdx).Files.Count);
        int S(string a) => Conflicts.AreaSeverity(a);
        Assert.True(S("common/traits") == 2 && S("events") == 2 && S("history/provinces") == 2 && S("map_data") == 2);
        Assert.True(S("gui") == 1 && S("localization/english") == 1 && S("something_else") == 1);
        Assert.True(S("gfx/portraits") == 0 && S("music") == 0 && S("common/genes") == 0 && S("common/bookmark_portraits") == 0);
        Assert.True(Conflicts.SevName(2) == "High" && Conflicts.SevName(1) == "Medium" && Conflicts.SevName(0) == "Low");
    }

    [Fact]
    public void OneKeyDefinedThousandsOfTimesIsNotQuadratic()
    {
        var pm = new List<ModInfo>(); var pd = new Dictionary<string, ModDefs>(); var pp = new Playset { Name = "perf" };
        for (int m = 0; m < 6; m++)
        {
            var mi = new ModInfo { Id = $"p{m}.mod" }; mi.Name = mi.Id; pm.Add(mi);
            var d = new ModDefs { Complete = true };
            for (int i = 0; i < 6000; i++) { d.Files.Add($"common/big/m{m}_{i}.txt"); d.Defs.Add(new DefEntry { H = Scripts.Fnv64("common/big", '\x1f', "same_key"), File = (uint)i, NameOff = 0, NameLen = 8, Kind = (byte)DK.Common }); }
            d.Pool = "same_key"; pd[mi.Id] = d; pp.Mods.Add(new ModRef(mi.Id, true, ""));
        }
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var sr = Scripts.FindScriptConflicts(pp, Playsets.InfoMap(pm), pd);
        Assert.True(sr.Valid && sr.Items.Count == 1 && sr.Items[0].Hits.Count == 36000 && sw.ElapsedMilliseconds < 5000, $"{sw.ElapsedMilliseconds} ms");
        var sm = new List<ModInfo>(); var sd = new Dictionary<string, ModDefs>(); var sp = new Playset { Name = "shadow" };
        for (int m = 0; m < 3; m++)
        {
            var mi = new ModInfo { Id = $"s{m}.mod" }; mi.Name = mi.Id; sm.Add(mi);
            var d = new ModDefs { Complete = true, Pool = "k" }; d.Files.Add(m < 2 ? "common/traits/same.txt" : "common/traits/other.txt");
            d.Defs.Add(new DefEntry { H = Scripts.Fnv64("common/traits", '\x1f', "k"), File = 0, NameOff = 0, NameLen = 1, Kind = (byte)DK.Common });
            sd[mi.Id] = d; sp.Mods.Add(new ModRef(mi.Id, true, ""));
        }
        var sr2 = Scripts.FindScriptConflicts(sp, Playsets.InfoMap(sm), sd);
        Assert.True(sr2.Items.Count == 1 && sr2.Items[0].Hits.Count == 2 && sr2.Items[0].Hits[0].Mod == 1 && sr2.Items[0].Hits[1].Mod == 2);
    }

    [Fact]
    public void UpdateCheckFingerprints()
    {
        bool Ch(string a, string b) => Compare.FingerprintChanged(a, b);
        Assert.True(!Ch("C:/m|1.0|100", "C:/m|1.0|100") && Ch("C:/m|1.0|100", "C:/m|1.0|101") && Ch("C:/m|1.0|100", "C:/m|1.1|100"));
        Assert.False(Ch("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100#d:abc-5"));
        Assert.False(Ch("C:/m|1.0|100#d:abc-5", "C:/m|1.0|999#d:abc-5"));
        Assert.True(Ch("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100#d:abd-5"));
        Assert.True(Ch("C:/m|1.0|100#d:abc-5", "C:/m|1.1|100#d:abc-5"));
        Assert.False(Ch("C:/m|1.0|100#d:abc-5", "C:/m|1.0|100"));
        Assert.False(Ch("C:/m|1.0|100", "C:/m|1.0|100#d:abc-5"));
        Assert.True(Ch("C:/m|1.0|100", "C:/m|1.0|101#d:abc-5"));
        Assert.True(!Ch("", "") && Ch("", "C:/m|1.0|1"));
        var um = new List<ModInfo> { new() { Id = "u.mod", Name = "U" } }; var up = new Playset { Name = "u" }; up.Mods.Add(new ModRef("u.mod", true, ""));
        var ui = Analyze.AnalyzePlayset(up, Playsets.InfoMap(um), "");
        Compare.AddUpdateIssues(ui, up, new Dictionary<string, string> { ["u.mod"] = "d|1|5#d:aa-2" }, new Dictionary<string, string> { ["u.mod"] = "d|1|9#d:aa-2" });
        Assert.Empty(ui[0]);
        Compare.AddUpdateIssues(ui, up, new Dictionary<string, string> { ["u.mod"] = "d|1|5#d:aa-2" }, new Dictionary<string, string> { ["u.mod"] = "d|1|5#d:ab-2" });
        Assert.True(ui[0].Count > 0 && ui[0][^1].Text.Contains("Updated since"));
    }
}
