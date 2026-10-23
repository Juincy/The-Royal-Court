using RoyalCourt.Core;
using Xunit;
using Xunit.Abstractions;
namespace RoyalCourt.Tests;

public class PropertyTests
{
    readonly ITestOutputHelper log;
    public PropertyTests(ITestOutputHelper o) { log = o; }

    /// <summary>Random playsets built from the known-mods database must always sort into a valid, stable, rule-respecting order (port of prop_sort.cpp).</summary>
    [Fact]
    public void AutoSortProperty()
    {
        const int rounds = 2000;
        var rng = new Random(777); var db = Known.Builtin();
        int bad = 0, withWarn = 0, notIdem = 0, ruleBreak = 0;
        for (int r = 0; r < rounds; r++)
        {
            var ms = new List<ModInfo>(); var byName = new HashSet<string>();
            void Add(string name) { if (!byName.Add(name)) return; ms.Add(new ModInfo { Id = $"m{ms.Count}.mod", Name = name }); }
            int k = 3 + rng.Next(14);
            for (int i = 0; i < k; i++)
            {
                var e = db[rng.Next(db.Count)]; Add(e.Names[0]);
                foreach (var af in e.After) if (rng.Next(4) != 0) Add(af);
                foreach (var bf in e.Before) if (rng.Next(4) != 0) Add(bf);
            }
            int extra = rng.Next(6);
            string[] gen = { "Cool Events", "Better UI", "Nice Portraits", "More Stuff", "Compatibility Patch for X", "Lib Core Framework", "Dragons" };
            for (int i = 0; i < extra; i++) Add(gen[rng.Next(7)] + (rng.Next(2) == 0 ? "" : " " + i));
            var perm = Enumerable.Range(0, ms.Count).OrderBy(_ => rng.Next()).ToList();
            var ps = new Playset { Name = "p" };
            foreach (int i in perm) ps.Mods.Add(new ModRef(ms[i].Id, rng.Next(6) != 0, ""));
            var info = Playsets.InfoMap(ms);
            var locked = new HashSet<string>();
            if (rng.Next(4) == 0) locked.Add(ps.Mods[rng.Next(ps.Mods.Count)].Id);
            var a = H.Plan(ps, info, locked, null, db); var b = H.Plan(ps, info, locked, null, db);
            bool permOk = a.Order.OrderBy(x => x).SequenceEqual(Enumerable.Range(0, ps.Mods.Count));
            if (!permOk || !a.Order.SequenceEqual(b.Order)) { bad++; log.WriteLine($"BAD permutation/determinism round {r}"); continue; }
            for (int i = 0; i < ps.Mods.Count; i++) if (!ps.Mods[i].Enabled && a.Order[i] != i) { bad++; log.WriteLine($"BAD disabled mod moved round {r}"); }
            foreach (var mv in a.Moves) foreach (var m in ps.Mods) if (m.Id == mv.Id && !m.Enabled) { bad++; log.WriteLine($"BAD disabled mod in preview round {r}"); }
            for (int i = 0; i < ps.Mods.Count; i++) if (locked.Contains(ps.Mods[i].Id) && a.Order[i] != i) { bad++; log.WriteLine($"BAD lock moved round {r}"); }
            var q = new Playset { Name = "q" }; foreach (int o in a.Order) q.Mods.Add(ps.Mods[o]);
            var c = H.Plan(q, info, locked, null, db);
            if (c.Changed) notIdem++;
            if (a.Warnings.Count == 0 && locked.Count == 0 && c.Broken.Count > 0) { bad++; log.WriteLine($"BAD broken rule after sort round {r}: {c.Broken[0].Text}"); }
            if (a.Warnings.Count > 0) withWarn++;
            var pos = new Dictionary<string, int>(); int p = 0;
            foreach (var m in q.Mods) if (m.Enabled) pos[info[m.Id].Name] = p++;
            if (a.Warnings.Count == 0 && locked.Count == 0)
                foreach (var m in q.Mods)
                {
                    if (!m.Enabled) continue;
                    int ki = Known.Find(db, info[m.Id]); if (ki < 0) continue; var kn = db[ki];
                    foreach (var x in kn.After) if (pos.TryGetValue(x, out int px) && px > pos[info[m.Id].Name]) { ruleBreak++; log.WriteLine($"RULE: {info[m.Id].Name} should be after {x} round {r}"); }
                    foreach (var x in kn.Before) if (pos.TryGetValue(x, out int px) && px < pos[info[m.Id].Name]) { ruleBreak++; log.WriteLine($"RULE: {info[m.Id].Name} should be before {x} round {r}"); }
                }
        }
        log.WriteLine($"rounds {rounds} bad {bad} not-idempotent {notIdem} rule-breaks {ruleBreak} with-warnings {withWarn}");
        Assert.Equal(0, bad); Assert.Equal(0, ruleBreak);
    }

    /// <summary>Mutation fuzzer for every parser that reads files the user or the internet controls (port of fuzz_core.cpp): nothing may throw or hang.</summary>
    [Fact]
    public void ParserFuzz()
    {
        int iters = int.TryParse(Environment.GetEnvironmentVariable("RC_FUZZ_ITERS"), out var n) ? n : 20000;
        var rng = new Random(12345);
        var seeds = new List<string>
        {
            "name=\"X\"\nversion=\"1.0\"\ntags={\n\t\"A\" \"B\"\n}\nsupported_version=\"1.20.*\"\npath=\"/a/b\"\nreplace_path=\"history\"\n",
            "{\"game\":\"ck3\",\"name\":\"P\",\"mods\":[{\"displayName\":\"A\",\"position\":0,\"enabled\":true,\"steamId\":\"123\",\"pdxId\":\"x\",\"path\":\"C:\\\\x\"}]}",
            "{\"tag_name\":\"v1.2.3\",\"body\":\"x\",\"html_url\":\"https://github.com/Juincy/the-royal-court/releases/tag/v1\",\"assets\":[{\"name\":\"a\",\"size\":1,\"browser_download_url\":\"https://github.com/Juincy/the-royal-court/releases/download/v1/a\"}]}",
            "namespace = x\nx.1 = {\n type = character_event\n trigger = { always = yes }\n}\nscripted_trigger a = { x = 1 }\n",
            "l_english:\n key:0 \"text \\\"q\\\" $X$ #bold\"\n key2: \"v\"\n",
            "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Steam\"\n\t}\n}\n",
            "{\"format\":1,\"revision\":2,\"mods\":[{\"names\":[\"A B\"],\"ids\":[\"1\"],\"type\":\"Graphics\",\"position\":\"top\",\"rank\":5,\"after\":[\"x\"],\"before\":[\"y\"],\"note\":\"n\"}]}",
            "[08:41:13][E][jomini_script_system.cpp:304]: Script system error!\n  Error: x [ y ]\n  Script location: file: events/a.txt line: 41 (ev.1:option)\n\n[08:41:14][W][a.cpp:1]: 'q' at file: common/x.txt line: 2 (n)\n",
            "Name|mod/ugc_1.mod|Enabled\nOther|mod/b.mod|Disabled\n", "RC1:AQAAAQ", "1.20.0.4 (Scythe)", "v0.14.1-beta", "{\"rawVersion\":\"1.20.0.4\"}",
            new string('a', 64) + "  TheRoyalCourt.exe\r\n",
        };
        string Cut(string s, int a, int l) => a >= s.Length ? "" : s.Substring(a, Math.Min(l, s.Length - a));
        string Mutate(string s)
        {
            int cnt = 1 + rng.Next(6);
            for (int i = 0; i < cnt; i++)
            {
                switch (rng.Next(7))
                {
                    case 0: if (s.Length > 0) { var c = s.ToCharArray(); c[rng.Next(s.Length)] = (char)rng.Next(256); s = new string(c); } break;
                    case 1: s = s.Insert(rng.Next(s.Length + 1), ((char)rng.Next(256)).ToString()); break;
                    case 2: if (s.Length > 0) { int a = rng.Next(s.Length); s = s.Remove(a, Math.Min(1 + rng.Next(8), s.Length - a)); } break;
                    case 3: if (s.Length > 0) { int a = rng.Next(s.Length), l = rng.Next(s.Length - a + 1); s = s.Insert(rng.Next(s.Length + 1), s.Substring(a, l)); } break;
                    case 4: s = s.Insert(rng.Next(s.Length + 1), new string("{}[]\"\\=#\n"[rng.Next(9)], 1 + rng.Next(40))); break;
                    case 5: if (s.Length > 0) s = s.Substring(0, rng.Next(s.Length)); break;
                    default: s += seeds[rng.Next(seeds.Count)]; break;
                }
                if (s.Length > 20000) s = s.Substring(0, 20000);
            }
            return s;
        }
        var installed = Enumerable.Range(0, 4).Select(i => new ModInfo { Id = $"ugc_{100 + i}.mod", Name = $"Mod {i}" }).ToList();
        using var env = new TestEnv(); string cdir = env.P("fuzz_cache");
        var seedDefs = new ModDefs { Complete = true, Pool = "braveee.1kind" }; seedDefs.Files.AddRange(new[] { "events/a.txt", "common/traits/t.txt" });
        seedDefs.Defs.Add(new DefEntry { H = 1, File = 0, NameOff = 0, NameLen = 5, Kind = (byte)DK.Common }); seedDefs.Defs.Add(new DefEntry { H = 2, File = 1, NameOff = 5, NameLen = 4, Kind = (byte)DK.Event });
        Scripts.SaveModCache(cdir, "m.mod", "deep-1", seedDefs);
        byte[] modSeed = File.ReadAllBytes(Scripts.ModCacheFile(cdir, "m.mod"));
        var van = new VanillaIndex { Key = "k", Dir = "d" }; van.Files.AddRange(new[] { "common/a.txt", "events/b.txt", "gfx/c.dds" }); van.Finish(); Vanilla.SaveCache(cdir, van);
        string vanSeed = File.ReadAllText(Path.Combine(cdir, "vanilla.txt"));
        var sp = new Playset { Name = "S" }; sp.Mods.Add(new ModRef("ugc_100.mod", true, "")); sp.Mods.Add(new ModRef("loc.mod", true, "Local"));
        var si = new Dictionary<string, ModInfo> { ["loc.mod"] = new ModInfo { Id = "loc.mod", Name = "Local" } };
        Share.B64UrlDecode(Share.Make(sp, si, true).Substring(4), out var shareSeed);
        byte[] MutBytes(byte[] b) { var s = Mutate(new string(b.Select(x => (char)x).ToArray())); return s.Select(ch => (byte)ch).ToArray(); }
        for (int it = 0; it < iters; it++)
        {
            string s = Mutate(seeds[rng.Next(seeds.Count)]);
            if (it % 4 == 0)
            {
                File.WriteAllBytes(Scripts.ModCacheFile(cdir, "m.mod"), MutBytes(modSeed));
                if (Scripts.LoadModCache(cdir, "m.mod", "deep-1", out var od)) foreach (var d in od.Defs) _ = od.NameOf(d);
                File.WriteAllText(Path.Combine(cdir, "vanilla.txt"), Mutate(vanSeed));
                var v = Vanilla.LoadCache(cdir, "k"); if (v != null) { _ = v.Has(s); _ = v.CountUnder(s); }
            }
            {
                var lp = GameLog.Parse(s); var gp = new Playset { Name = "g" }; gp.Mods.Add(new ModRef("ugc_100.mod", true, ""));
                var gf = new Dictionary<string, ModFiles> { ["ugc_100.mod"] = H.Files(true, new[] { Cut(s, 0, 20), "events/a.txt" }) };
                var rep = GameLog.Attribute(lp, gp, gf, new Dictionary<string, ModDefs>(), null);
                for (int k = 0; k < rep.Parse.Entries.Count; k++) _ = rep.How[k];
                _ = GameLog.ModReport(rep, 0, Cut(s, 0, 10)); _ = GameLog.ParseLoadedMods(s);
            }
            {
                var d = Share.Decode(s); if (d.Ok) _ = Share.ToPlayset(d, installed); _ = Share.Find(s); Share.B64UrlDecode(s, out _);
                var d2 = Share.Decode("RC1:" + Share.B64UrlEncode(MutBytes(shareSeed))); if (d2.Ok) _ = Share.ToPlayset(d2, installed);
            }
            if (J.TryParse(s, out var j)) _ = j.Dump();
            ModScan.ParseDescriptor(s); Playsets.Parse(s, installed); Release.Parse(s); Release.FindSumFor(s, "TheRoyalCourt.exe");
            Release.CompareVersions(s, "0.14.0"); Release.ParseVersion(s);
            ModScan.VersionSegments(s); ModScan.MatchGameVersion(s, "1.20.0.4"); ModScan.GameVersionFromSettings(s);
            Names.Norm(s); Steam.VdfValues(s, "path"); Steam.AcfInstallDir(s); Steam.RegisteredDescriptorText(s, "123", "C:\\x\\y"); Util.SanitizeFileName(s);
            var keys = new List<ScriptKey>(); Scripts.ScanScriptKeys(s, rng.Next(3), keys); Scripts.ScanLocKeys(s, new List<string>());
            Release.Sha256Hex(s);
            {
                var kv = new List<KnownMod>(); Known.ApplyJson(kv, s, out _); Known.CheckOnline(s);
                var km = new ModInfo { Id = "ugc_1.mod", Name = Cut(s, 0, 40) }; Known.Find(kv, km);
                var kp = new Playset { Name = Cut(s, 0, 20) }; kp.Mods.Add(new ModRef(km.Id, true, km.Name));
                Release.BuildSortReport(kp, new Dictionary<string, ModInfo> { [km.Id] = km }, kv, new[] { Cat.Content }, new HashSet<string> { km.Id }, Cut(s, 0, 10), 3);
            }
        }
    }
}
