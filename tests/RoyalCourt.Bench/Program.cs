using System.Diagnostics;
using RoyalCourt.Core;

// Synthetic playset benchmark: dotnet RoyalCourt.Bench.dll MODS FILES_PER_MOD   (the C++ audit used 400 x 150 and 1200 x 120)
int nMods = int.Parse(args[0]), perMod = int.Parse(args[1]);
string root = Path.Combine(Path.GetTempPath(), "rc_bench_" + nMods);
Environment.SetEnvironmentVariable("RC_DATA_DIR", Path.Combine(root, "data"));
if (Directory.Exists(root)) Directory.Delete(root, true);
var rng = new Random(42);
var mods = new List<ModInfo>(); var ps = new Playset { Name = "bench" };
string[] areas = { "common/traits", "common/decisions", "events", "common/on_action", "localization/english", "gfx/portraits", "common/script_values", "history/characters", "gui" };
var shared = Enumerable.Range(0, 3000).Select(i => $"{areas[i % areas.Length]}/shared_{i}.txt").ToList();
var sw = Stopwatch.StartNew();
for (int m = 0; m < nMods; m++)
{
    string dir = Path.Combine(root, "mods", "m" + m); Directory.CreateDirectory(dir);
    var used = new HashSet<string>();
    for (int f = 0; f < perMod; f++)
    {
        string rel = rng.Next(5) == 0 ? shared[rng.Next(shared.Count)] : $"{areas[rng.Next(areas.Length)]}/m{m}_{f}.txt";
        if (!used.Add(rel)) continue;
        string path = Path.Combine(dir, rel.Replace('/', Path.DirectorySeparatorChar)); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        string body = rel.StartsWith("events/") ? $"namespace = n{m}\nn{m}.{f} = {{ type = character_event }}\n"
            : rel.StartsWith("localization/") ? $"l_english:\n k{m}_{f}:0 \"x\"\n" : $"key_{m}_{f} = {{ a = 1 }}\nshared_key_{f % 40} = {{ b = 2 }}\n";
        File.WriteAllText(path, body);
    }
    var mi = new ModInfo { Id = $"ugc_{1000 + m}.mod", Name = $"Mod {m}", ContentDir = dir, ContentState = 1, Source = "Workshop" };
    mods.Add(mi); ps.Mods.Add(new ModRef(mi.Id, true, mi.Name));
}
Console.WriteLine($"generated {nMods} mods in {sw.ElapsedMilliseconds} ms");
var info = Playsets.InfoMap(mods);
string cache = Path.Combine(root, "cache");
var index = new Dictionary<string, ModFiles>(); var defs = new Dictionary<string, ModDefs>();
sw.Restart();
foreach (var m in mods) { var im = Scripts.IndexModCached(cache, m.Id, m.ContentDir); index[m.Id] = im.Files; defs[m.Id] = im.Defs; }
long files = index.Values.Sum(v => (long)v.Files.Count);
Console.WriteLine($"cold index ({files} files): {sw.ElapsedMilliseconds} ms");
sw.Restart();
foreach (var m in mods) { var im = Scripts.IndexModCached(cache, m.Id, m.ContentDir); index[m.Id] = im.Files; defs[m.Id] = im.Defs; }
Console.WriteLine($"warm index (saved copy): {sw.ElapsedMilliseconds} ms");
sw.Restart(); var cr = Conflicts.Find(ps, info, index); Console.WriteLine($"conflict finding: {sw.ElapsedMilliseconds} ms ({cr.Files.Count} files, {cr.Pairs.Count} pairs)");
sw.Restart(); var sr = Scripts.FindScriptConflicts(ps, info, defs); Console.WriteLine($"script conflicts: {sw.ElapsedMilliseconds} ms ({sr.Items.Count} items)");
var db = Known.Builtin();
sw.Restart(); var plan = Sorter.Plan(ps, info, new HashSet<string>(), new Dictionary<string, int>(), cr, index, db); Console.WriteLine($"auto sort: {sw.ElapsedMilliseconds} ms ({plan.Moves.Count} moves)");
sw.Restart(); var iss = Analyze.AnalyzePlayset(ps, info, "1.20.0.4"); Analyze.AddConflictIssues(iss, cr, ps, info); Scripts.AddScriptIssues(iss, sr, ps); Console.WriteLine($"issues: {sw.ElapsedMilliseconds} ms");
GC.Collect(); Console.WriteLine($"managed heap after: {GC.GetTotalMemory(true) / (1 << 20)} MB, working set {Process.GetCurrentProcess().WorkingSet64 / (1 << 20)} MB, peak {Process.GetCurrentProcess().PeakWorkingSet64 / (1 << 20)} MB");
Directory.Delete(root, true);
