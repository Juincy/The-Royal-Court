using System.Diagnostics;
using RoyalCourt.Core;
using Xunit;
using Xunit.Abstractions;
namespace RoyalCourt.Tests;

public class PerfTests
{
    readonly ITestOutputHelper log;
    public PerfTests(ITestOutputHelper o) { log = o; }

    /// <summary>A 300-mod playset with about 90,000 files: every step the program repeats while you use it must stay quick.</summary>
    [Fact]
    public void BigPlaysetStaysFast()
    {
        var rng = new Random(99); var db = Known.Builtin();
        var ms = new List<ModInfo>(); var idx = new Dictionary<string, ModFiles>(); var names = new HashSet<string>();
        int want = int.TryParse(Environment.GetEnvironmentVariable("RC_PERF_MODS"), out var wm) ? wm : 300;
        while (ms.Count < want)
        {
            string name = rng.Next(3) == 0 ? db[rng.Next(db.Count)].Names[0] : "Generic Mod " + ms.Count + (rng.Next(5) == 0 ? " Compatch" : "");
            if (!names.Add(name)) continue;
            var mi = new ModInfo { Id = $"ugc_{1000000000 + ms.Count}.mod", Name = name }; ms.Add(mi);
            int fc = 20 + rng.Next(rng.Next(6) == 0 ? 1500 : 400);
            var files = new HashSet<string>();
            for (int f = 0; f < fc; f++) files.Add(rng.Next(3) == 0 ? "common/shared_dir/file" + rng.Next(3000) + ".txt" : "common/m" + ms.Count + "/f" + f + ".txt");
            idx[mi.Id] = new ModFiles { Files = files.OrderBy(x => x, StringComparer.Ordinal).ToList(), Complete = true };
        }
        int total = idx.Values.Sum(v => v.Files.Count);
        var ps = new Playset { Name = "big" };
        foreach (int i in Enumerable.Range(0, ms.Count).OrderBy(_ => rng.Next())) ps.Mods.Add(new ModRef(ms[i].Id, true, ""));
        var info = Playsets.InfoMap(ms);
        long mem0 = GC.GetTotalMemory(true);
        var sw = Stopwatch.StartNew();
        var cr = Conflicts.Find(ps, info, idx); long tConf = sw.ElapsedMilliseconds; sw.Restart();
        var plan = Sorter.Plan(ps, info, new HashSet<string>(), new Dictionary<string, int>(), cr, idx, db); long tPlan = sw.ElapsedMilliseconds; sw.Restart();
        var hr = Health.Check(ps, info, "1.19.0", new HashSet<string>(), new Dictionary<string, int>(), cr, idx, db); long tHealth = sw.ElapsedMilliseconds; sw.Restart();
        var plan0 = Sorter.Plan(ps, info, new HashSet<string>(), new Dictionary<string, int>(), null, null, db); long tPlanNoFiles = sw.ElapsedMilliseconds;
        long memPeak = GC.GetTotalMemory(false); long mem1 = GC.GetTotalMemory(true); GC.KeepAlive(cr); GC.KeepAlive(plan); GC.KeepAlive(hr);
        log.WriteLine($"mods {ms.Count} files {total}  conflicts {tConf} ms ({cr.Pairs.Count} pairs, {cr.Files.Count} files)  sort {tPlan} ms  health {tHealth} ms  sort-without-files {tPlanNoFiles} ms  heap kept +{(mem1 - mem0) / (1 << 20)} MB, with garbage +{(memPeak - mem0) / (1 << 20)} MB");
        Assert.True((mem1 - mem0) < (400L << 20), "results of a big playset must not hold hundreds of MB");
        Assert.True(tConf < 5000 && tPlan < 5000 && tHealth < 8000 && tPlanNoFiles < 2000, "a big playset must stay quick");
        Assert.True(plan.Order.Length == want && hr.Enabled == want && plan0.Order.Length == want);
    }
}
