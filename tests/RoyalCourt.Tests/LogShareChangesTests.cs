using System.Reflection;
using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class LogShareChangesTests
{
    const string Log =
        "[08:40:44][W][game_database.h:272]: Overriding entry 'house_armagnac' for database 'common/dynasty_houses' in 'file: common/dynasty_houses/00_unop_dynasty_houses.txt line: 1'\r\n"
        + "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\r\n"
        + "  Error: stress_impact effect [ Cannot find scholar in trait database ]\r\n"
        + "  Script location: file: events/VIET_events_travel.txt line: 4129 (VIETmisc.7039:option)\r\n"
        + "\r\n"
        + "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\r\n"
        + "  Error: stress_impact effect [ Cannot find scholar in trait database ]\r\n"
        + "  Script location: file: events/VIET_events_travel.txt line: 4129 (VIETmisc.7039:option)\r\n"
        + "\r\n"
        + "[08:41:13][E][modifier_instance.cpp:390]: Unknown modifier type 'faith_creation_piety_cost_mult' at file: common/traits/zz_gptev_traits.txt line: 776 (gpt_crow)\r\n"
        + "[08:41:13][E][event.cpp:451]: fullscreen_event 'VIETmisc.1084' requires queue_icon\r\n"
        + "[08:41:13][E][weird.cpp:1]: something nobody can place\r\n"
        + "[08:41:13][E][base.cpp:1]: bad thing at file: common/traits/00_traits.txt line: 3 (x)\r\n"
        + "[08:41:14][I][info.cpp:1]: just information\r\n"
        + "[08:41:14][D][dbg.cpp:1]: debug chatter\r\n"
        + "not a log line at all\r\n";

    [Fact]
    public void GameLogShapesAndAttribution()
    {
        var lp = GameLog.Parse(Log);
        Assert.True(lp.Errors == 6 && lp.Warnings == 1);
        Assert.Equal(6, lp.Entries.Count);
        var se = lp.Entries.First(x => x.Src == "jomini_script_system.cpp:304");
        Assert.True(se.Count == 2 && se.File == "events/viet_events_travel.txt" && se.Line == 4129 && se.Detail.Contains("Cannot find scholar"));
        Assert.Contains("VIETmisc.7039", se.Names);
        var lw = GameLog.Parse(Log, false); Assert.True(lw.Warnings == 0 && lw.Errors == 6);
        var ps = new Playset { Name = "t" }; ps.Mods.Add(new ModRef("a.mod", true, "A")); ps.Mods.Add(new ModRef("b.mod", true, "B")); ps.Mods.Add(new ModRef("c.mod", false, "C"));
        var mf = new Dictionary<string, ModFiles>
        {
            ["a.mod"] = H.Files(true, new[] { "events/viet_events_travel.txt", "common/traits/zz_gptev_traits.txt" }),
            ["b.mod"] = H.Files(true, new[] { "common/traits/zz_gptev_traits.txt" }),
            ["c.mod"] = H.Files(true, new[] { "events/viet_events_travel.txt" }),
        };
        var bd = new ModDefs { Complete = true, Pool = "VIETmisc.1084" }; bd.Defs.Add(new DefEntry { NameOff = 0, NameLen = 13, Kind = (byte)DK.Event });
        var md = new Dictionary<string, ModDefs> { ["b.mod"] = bd };
        var van = new VanillaIndex(); van.Files.Add("common/traits/00_traits.txt"); van.Finish();
        var rep = GameLog.Attribute(GameLog.Parse(Log), ps, mf, md, van);
        for (int k = 0; k < rep.Parse.Entries.Count; k++)
        {
            var en = rep.Parse.Entries[k];
            if (en.Src == "jomini_script_system.cpp:304") Assert.True(rep.ModOf[k] == 0 && rep.How[k] == LogHow.File);
            if (en.Src == "modifier_instance.cpp:390") Assert.True(rep.ModOf[k] == 1 && rep.How[k] == LogHow.File);
            if (en.Src == "event.cpp:451") Assert.True(rep.ModOf[k] == 1 && rep.How[k] == LogHow.Name);
            if (en.Src == "weird.cpp:1") Assert.True(rep.ModOf[k] == -1 && rep.How[k] == LogHow.None);
            if (en.Src == "base.cpp:1") Assert.True(rep.ModOf[k] == -1 && rep.How[k] == LogHow.Base);
            if (en.Src == "game_database.h:272") Assert.Equal(LogHow.None, rep.How[k]);
        }
        Assert.True(rep.PerMod.Count == 2 && rep.PerMod[0].Mod == 0 && rep.PerMod[0].Errors == 2);
        Assert.True(rep.Base.Errors == 1 && rep.Unplaced.Errors == 1 && rep.Unplaced.Warnings == 1);
        Assert.Contains("Cannot find scholar", GameLog.ModReport(rep, 0, "A"));
        var lm = GameLog.ParseLoadedMods("junk\nBetter Population Control|mod/ugc_3425828418.mod|Enabled\nUnofficial Patch|mod/ugc_2871648329.mod|Enabled\nBetter Population Control|mod/ugc_3425828418.mod|Enabled\nbad|mod/../x.mod|Enabled\nX|mod/y.mod|Maybe\n");
        Assert.True(lm.Count == 2 && lm[0].Id == "ugc_3425828418.mod" && lm[1].Name == "Unofficial Patch");
        Assert.Empty(GameLog.Parse("").Entries);
        Assert.True(GameLog.Parse("[").Entries.Count == 0 && GameLog.Parse("[08:00:00][E][").Entries.Count == 0);
        var sb = new System.Text.StringBuilder(); for (int i = 0; i < 30000; i++) sb.Append($"[08:00:00][E][a.cpp:1]: msg {i} file: x/{i}.txt line: 1\n");
        var bp = GameLog.Parse(sb.ToString()); Assert.True(bp.Entries.Count == 20000 && bp.Dropped == 10000 && bp.Errors == 30000);
    }

    [Fact]
    public void ShareCodes()
    {
        var inst = new List<ModInfo> { new() { Id = "ugc_2871648329.mod", Name = "Unofficial Patch" }, new() { Id = "my_local.mod", Name = "My Local" }, new() { Id = "ugc_5.mod", Name = "Off" } };
        var ps = new Playset { Name = "Vanilla+ V1.0" };
        foreach (var (id, en, nm) in new[] { ("ugc_2871648329.mod", true, ""), ("my_local.mod", true, ""), ("ugc_5.mod", false, ""), ("ugc_3425828418.mod", true, "Gone Mod"), ("ugc_77.mod", false, "Off Missing"), ("local:Ghost", true, "Ghost") }) ps.Mods.Add(new ModRef(id, en, nm));
        var im = Playsets.InfoMap(inst);
        foreach (bool names in new[] { false, true })
        {
            string code = Share.Make(ps, im, names);
            Assert.True(code.StartsWith("RC1:") && code.IndexOfAny(" \n+/=".ToCharArray()) < 0);
            var d = Share.Decode("here you go: " + code + " enjoy");
            Assert.True(d.Ok && d.Name == "Vanilla+ V1.0" && d.WithNames == names);
            Assert.Equal(5, d.Mods.Count);
            Assert.True(d.Mods[0].WorkshopId == "2871648329" && d.Mods[0].Enabled && (names ? d.Mods[0].Name == "Unofficial Patch" : d.Mods[0].Name.Length == 0));
            Assert.True(d.Mods[1].Local && d.Mods[1].Name == "My Local");
            Assert.True(d.Mods[2].WorkshopId == "3425828418" && d.Mods[2].Enabled);
            Assert.True(d.Mods[3].WorkshopId == "77" && !d.Mods[3].Enabled);
            Assert.True(d.Mods[4].Local && d.Mods[4].Name == "Ghost");
            var back = Share.ToPlayset(d, inst);
            Assert.True(back.Mods.Count == 5 && back.Mods[0].Id == "ugc_2871648329.mod" && back.Mods[1].Id == "my_local.mod" && back.Mods[2].Id == "ugc_3425828418.mod");
            Assert.True(back.Mods[3].Id == "ugc_77.mod" && !back.Mods[3].Enabled && back.Mods[4].Id == "local:Ghost");
        }
        string c0 = Share.Make(ps, im, false);
        Assert.False(Share.Decode("").Ok); Assert.False(Share.Decode("RC1:").Ok); Assert.False(Share.Decode("hello").Ok);
        Assert.False(Share.Decode(c0.Substring(0, c0.Length - 5)).Ok);
        var bad = c0.ToCharArray(); bad[10] = bad[10] == 'A' ? 'B' : 'A'; Assert.False(Share.Decode(new string(bad)).Ok);
        var big = new Playset { Name = "big" }; for (int i = 0; i < 100; i++) big.Mods.Add(new ModRef("ugc_" + (2000000000u + (uint)i * 7919u) + ".mod", true, ""));
        Assert.True(Share.Make(big, new Dictionary<string, ModInfo>(), false).Length < 800);
        var evil = new List<byte> { 1, 0, 0, 0x87, 0x27 };   // claims 4,999 mods
        uint cc = Share.Crc32(evil.ToArray()); for (int i = 0; i < 4; i++) evil.Add((byte)((cc >> (i * 8)) & 0xFF));
        Assert.False(Share.Decode("RC1:" + Share.B64UrlEncode(evil.ToArray())).Ok);
        for (int n = 0; n < 40; n++) { var junk = new byte[n]; for (int k = 0; k < n; k++) junk[k] = (byte)(k * 37 + n); Assert.False(Share.Decode("RC1:" + Share.B64UrlEncode(junk)).Ok); }
    }

    [Fact]
    public void SinceLastPlay()
    {
        using var e = new TestEnv();
        var a = new ModFiles { Complete = true };
        for (int i = 0; i < 50; i++) { a.Files.Add($"common/traits/t{i}.txt"); a.Mix.Add(Walk.DeepMix(a.Files[^1], (ulong)(100 + i), 5)); }
        var m0 = Changes.ManifestOf(a);
        Assert.True(m0.V.Count == 50 && Changes.SaveManifest("a.mod", m0));
        var back = new Manifest(); Assert.True(Changes.LoadManifest("a.mod", back) && back.V.SequenceEqual(m0.V));
        var same = Changes.CompareManifest(back, a);
        Assert.True(same.Known && same.Same == 50 && same.Added == 0 && same.Changed == 0 && same.Removed == 0 && Changes.Describe(same) == "no file changes");
        var b = new ModFiles { Complete = true, Files = new List<string>(a.Files), Mix = new List<ulong>(a.Mix) };
        b.Mix[3] ^= 1; b.Files.RemoveAt(10); b.Mix.RemoveAt(10);
        b.Files.Add("events/new_a.txt"); b.Mix.Add(1); b.Files.Add("events/new_b.txt"); b.Mix.Add(2);
        var ch = Changes.CompareManifest(back, b);
        Assert.True(ch.Known && ch.Changed == 1 && ch.Added == 2 && ch.Removed == 1 && ch.Same == 48);
        Assert.True(ch.ChangedNames.Count == 1 && ch.ChangedNames[0] == "common/traits/t3.txt" && ch.AddedNames.Count == 2);
        Assert.StartsWith("1 file changed, 2 added, 1 removed", Changes.Describe(ch));
        Assert.False(Changes.CompareManifest(new Manifest(), a).Known);
        var incomplete = new ModFiles { Complete = false, Files = a.Files, Mix = a.Mix };
        Assert.True(Changes.ManifestOf(incomplete).V.Count == 0 && !Changes.CompareManifest(back, incomplete).Known);
        var nomix = new ModFiles { Complete = true, Files = a.Files }; Assert.Empty(Changes.ManifestOf(nomix).V);
        var raw = File.ReadAllBytes(Changes.ManifestPath("a.mod")); raw[20] ^= 1; File.WriteAllBytes(Changes.ManifestPath("a.mod"), raw);
        var bad2 = new Manifest(); Assert.False(Changes.LoadManifest("a.mod", bad2));
        File.WriteAllBytes(Changes.ManifestPath("a.mod"), raw.Take(raw.Length / 2).ToArray()); Assert.False(Changes.LoadManifest("a.mod", bad2));
        Assert.False(Changes.LoadManifest("nothing.mod", bad2));
        Changes.SaveManifest("a.mod", m0);
        var st = new Settings(); var ps = new Playset { Name = "P" };
        foreach (var (id, en, nm) in new[] { ("a.mod", true, "A"), ("o.mod", true, "Old"), ("n.mod", true, "New"), ("off.mod", false, "Off") }) ps.Mods.Add(new ModRef(id, en, nm));
        var inst = new List<ModInfo> { new() { Id = "a.mod", Name = "A", Supported = "1.21.*" }, new() { Id = "o.mod", Name = "Old", Supported = "1.18.*" }, new() { Id = "n.mod", Name = "New", Supported = "1.21.*" }, new() { Id = "off.mod", Name = "Off" } };
        var im = Playsets.InfoMap(inst);
        var files = new Dictionary<string, ModFiles> { ["a.mod"] = b };
        var now = new Dictionary<string, string> { ["a.mod"] = "d|1|1#d:bb-5" };
        var none = Changes.BuildPlayReport(st, ps, im, now, files, "1.21.0.1");
        Assert.True(!none.HasPrev && !none.GameChanged && none.Updated.Count == 0 && none.Outdated.Count == 1 && none.Outdated[0].Name == "Old");
        st.PlayTime = 1000; st.PlayGameVer = "1.20.0.4"; st.PlayName = "P"; st.PlayMods = new List<string> { "a.mod", "gone.mod", "off.mod" };
        st.Seen["a.mod"] = "d|1|1#d:aa-5";
        var rp = Changes.BuildPlayReport(st, ps, im, now, files, "1.21.0.1");
        Assert.True(rp.HasPrev && rp.GameChanged && rp.PrevGame == "1.20.0.4");
        Assert.True(rp.Updated.Count == 1 && rp.Updated[0].Text.StartsWith("1 file changed"));
        Assert.True(rp.Outdated.Count == 1 && rp.Outdated[0].ModId == "o.mod");
        Assert.Equal(2, rp.Added.Count);
        Assert.True(rp.Removed.Count == 2 && rp.Removed[0].Text == "no longer installed" && rp.Removed[1].Text == "turned off since then");
        var other = ps.Clone(); other.Name = "Other";
        Assert.Empty(Changes.BuildPlayReport(st, other, im, now, files, "1.21.0.1").Added);
        Changes.RecordPlay(st, ps, files, "1.21.0.1", 2000);
        Assert.True(st.PlayTime == 2000 && st.PlayGameVer == "1.21.0.1" && st.PlayMods.Count == 3);
        var rec = new Manifest(); Assert.True(Changes.LoadManifest("a.mod", rec) && rec.V.Count == b.Files.Count);
        st.Save(); var st2 = Settings.Load();
        Assert.True(st2.PlayTime == 2000 && st2.PlayGameVer == "1.21.0.1" && st2.PlayName == "P" && st2.PlayMods.SequenceEqual(st.PlayMods));
    }

    [Fact]
    public void CrashHelper()
    {
        var cf = new CrashFolder();
        Assert.True(Crash.NameParse("ck3_20261009_172556", cf) && cf.Y == 2026 && cf.Mo == 10 && cf.D == 9 && cf.H == 17 && cf.Mi == 25 && cf.S == 56);
        Assert.True(!Crash.NameParse("ck3_2026", cf) && !Crash.NameParse("ck3_20261309_172556", cf) && !Crash.NameParse("xyz_20261009_172556", cf) && !Crash.NameParse("ck3_2026100a_172556", cf));
        var d = new CrashData(); Crash.ParseException("Exception: EXCEPTION_ACCESS_VIOLATION reading 0x0\r\n  ck3.exe!Foo::bar() + 0x12\n  0x7ff6 something\n", d);
        Assert.True(d.HaveException && d.Kind == CrashKind.Access && d.Code == "EXCEPTION_ACCESS_VIOLATION" && d.Frames.Count == 2);
        var d2 = new CrashData(); Crash.ParseException("The process ran out of memory\n", d2); Assert.True(d2.Kind == CrashKind.Memory && d2.Code.Contains("memory"));
        var d3 = new CrashData(); Crash.ParseException("DXGI_ERROR_DEVICE_REMOVED", d3); Assert.Equal(CrashKind.Graphics, d3.Kind);
        var d4 = new CrashData(); Crash.ParseException("", d4); Assert.True(!d4.HaveException && d4.Kind == CrashKind.Unknown);
        var d5 = new CrashData(); Crash.ParseMeta("version: 1.20.0.4\nbuild: abc\nfoo: bar\n\x01\x02 garbage\n  platform: win\n", d5); Assert.Equal(3, d5.Meta.Count);
        string junk = new string('\x07', 5000); var d6 = new CrashData(); Crash.ParseException(junk, d6); Crash.ParseMeta(junk, d6);
        using var e = new TestEnv(); string root = e.P("crash");
        Directory.CreateDirectory(Path.Combine(root, "ck3_20261009_172556", "logs")); Directory.CreateDirectory(Path.Combine(root, "ck3_20250101_000000")); Directory.CreateDirectory(Path.Combine(root, "misc"));
        File.WriteAllText(Path.Combine(root, "ck3_20261009_172556", "exception.txt"), "EXCEPTION_STACK_OVERFLOW\n");
        File.WriteAllText(Path.Combine(root, "ck3_20261009_172556", "logs", "game.log"), "[08:41:13][E][modifier_instance.cpp:390]: Unknown modifier type 'x' at file: common/traits/a.txt line: 7 (t)\n");
        var cl = Crash.List(root);
        Assert.True(cl.Count == 3 && cl[0].Name == "ck3_20261009_172556" && cl[1].Name == "ck3_20250101_000000" && cl[2].Name == "misc");
        var dd = Crash.Read(cl[0].Path);
        Assert.True(dd.Kind == CrashKind.Stack && dd.LogsFromCrash && dd.GameLog.Contains("Unknown modifier"));
        Assert.True(Crash.List(Path.Combine(root, "none")).Count == 0 && !Crash.Read(Path.Combine(root, "none")).HaveException);
        Assert.Single(Crash.LastMessages(dd.GameLog).Entries);
        Assert.True(Crash.KindName(CrashKind.Access).Length > 5 && Crash.KindAdvice(CrashKind.Unknown).Length > 5);
    }

    static string Holes(string x) { var h = new List<char>(); for (int i = 0; i + 2 < x.Length; i++) if (x[i] == '{' && x[i + 1] >= '0' && x[i + 1] <= '9' && x[i + 2] == '}') h.Add(x[i + 1]); h.Sort(); return new string(h.ToArray()); }

    [Fact]
    public void LanguageTablesAreCompleteAndConsistent()
    {
        Assert.True(Lang.Count == 11 && Lang.Langs[0].Code == "en");
        Assert.True(Lang.FromTag("de-DE") == Lang.FromCode("de") && Lang.FromTag("pt-PT") == Lang.FromCode("pt-BR") && Lang.FromTag("zh-Hans-CN") == Lang.FromCode("zh"));
        Assert.True(Lang.FromTag("zh-TW") == 0 && Lang.FromTag("xx") == 0 && Lang.FromTag("") == 0 && Lang.FromCode("zz") == -1);
        var asm = typeof(Lang).Assembly; HashSet<string>? keys = null;
        for (int l = 1; l < Lang.Count; l++)
        {
            using var s = asm.GetManifestResourceStream("lang." + Lang.Langs[l].Code + ".json"); Assert.NotNull(s);
            using var r = new StreamReader(s!); Assert.True(J.TryParse(r.ReadToEnd(), out var root) && root.IsObj);
            var ks = new HashSet<string>(root.Keys);
            if (keys == null) keys = ks; else Assert.True(keys.SetEquals(ks), "every language has the same sentences: " + Lang.Langs[l].Code);
            for (int i = 0; i < root.Keys.Count; i++) { Assert.True(root.Vals[i].IsStr && root.Vals[i].S.Length > 0, Lang.Langs[l].Code + ": " + root.Keys[i]); Assert.Equal(Holes(root.Keys[i]), Holes(root.Vals[i].S)); }
        }
        Assert.True(keys!.Count >= 790);
        try
        {
            Lang.Set(Lang.FromCode("de"));
            Assert.NotEqual("Cancel", Lang.Tr("Cancel"));
            Assert.Equal("no such sentence xyz", Lang.Tr("no such sentence xyz"));
            Assert.Contains("1", Lang.Trf("{0} of {1}", 1, 2));
        }
        finally { Lang.Set(0); }
        Assert.Equal("Cancel", Lang.Tr("Cancel"));
        using var e = new TestEnv();
        var sl = new Settings { Language = "ru" }; sl.Save(); Assert.Equal("ru", Settings.Load().Language);
        sl.Language = "bogus"; sl.Save(); Assert.Equal("auto", Settings.Load().Language);
    }

    [Fact]
    public void SmallAuditFixes()
    {
        var kv = ModScan.ParseDescriptor("name=\"X\"\npath=\"C:\\mods\\foo\\\"\nversion=\"1\"\n");
        Assert.True(kv["path"].Count == 1 && kv["path"][0] == "C:\\mods\\foo\\" && kv["version"][0] == "1");
        Assert.Equal("A \"quoted\" name", ModScan.ParseDescriptor("name=\"A \\\"quoted\\\" name\"\n")["name"][0]);
        var r = Playsets.Parse("{\"game\":\"ck3\",\"name\":\"d\",\"mods\":[{\"steamId\":\"5\",\"enabled\":false,\"position\":0},{\"steamId\":\"5\",\"enabled\":true,\"position\":1}]}", new List<ModInfo>());
        Assert.True(r.Ok && r.Playset.Mods.Count == 1 && r.Playset.Mods[0].Enabled);
        using var e = new TestEnv();
        var a = new Settings(); a.Unsubbed["ugc_9.mod"] = 1760000000; a.Hidden.Add("ugc_9.mod"); Assert.True(a.Save());
        var b = Settings.Load(); Assert.True(b.Unsubbed.Count == 1 && b.Unsubbed["ugc_9.mod"] == 1760000000 && b.Hidden.Contains("ugc_9.mod"));
        foreach (var th in new[] { "dark", "light", "halloween" }) { var s = new Settings { Theme = th }; Assert.True(s.Save()); Assert.Equal(th, Settings.Load().Theme); }
        var p = new Settings { Theme = "pumpkin" }; Assert.True(p.Save()); Assert.Equal("", Settings.Load().Theme);
    }
}
