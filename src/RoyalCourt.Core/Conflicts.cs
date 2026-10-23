namespace RoyalCourt.Core;

public enum Sev { Low = 0, Med = 1, High = 2 }

public sealed class FileConflict { public string Path = ""; public List<int> Mods = new(); public int Sev; public bool Vanilla; }
public sealed class PairConflict { public int A, B, Count, Sev; public List<KeyValuePair<string, int>> Areas = new(); }
public sealed class WipeHit { public int Replacer, Victim, Files; public string Folder = ""; }
public sealed class VanillaFile { public string Path = ""; public List<int> Mods = new(); public int Sev; }
public sealed class VanillaWipe { public int Mod, Files; public string Folder = ""; }
public sealed class ConflictReport
{
    public List<FileConflict> Files = new();
    public List<PairConflict> Pairs = new();
    public List<WipeHit> Wipes = new();
    public List<VanillaFile> Vanilla = new();
    public List<VanillaWipe> VanillaWipes = new();
    public int[] Loses = Array.Empty<int>(), Wins = Array.Empty<int>(), VanillaCount = Array.Empty<int>();
    public int ModsIndexed, FilesIndexed, ModsWithoutFiles;
    public bool VanillaChecked, Valid;
}

public static class Conflicts
{
    public static string SevName(int s) => s >= (int)Sev.High ? Tr("High") : s == (int)Sev.Med ? Tr("Medium") : Tr("Low");

    public static int AreaSeverity(string area)
    {
        bool S(string p) => area.StartsWith(p, StringComparison.Ordinal);
        if (S("common/genes") || S("common/ethnicities") || S("common/bookmark_portraits") || S("common/coat_of_arms") || S("common/named_colors") ||
            S("common/accessories") || S("common/portrait") || S("common/ai_war_stances")) return S("common/ai_war_stances") ? (int)Sev.High : (int)Sev.Low;
        if (S("common") || area == "events" || S("history") || S("map_data")) return (int)Sev.High;
        if (S("gfx") || S("music") || S("sound") || S("sfx") || S("fonts") || S("licenses") || S("dlc_metadata")) return (int)Sev.Low;
        return (int)Sev.Med;
    }

    /// <summary>"common/traits/x.txt" -> "common/traits", "events/x.txt" -> "events"</summary>
    public static string AreaOf(string path)
    {
        int a = path.IndexOf('/');
        if (a < 0) return path;
        int b = path.IndexOf('/', a + 1);
        return b < 0 ? path[..a] : path[..b];
    }

    static string CleanFolder(string folder)
    {
        folder = Walk.Norm(folder);
        folder = folder.TrimEnd('/').TrimStart('/');
        return folder;
    }

    public static ConflictReport Find(Playset ps, IReadOnlyDictionary<string, ModInfo> info, IReadOnlyDictionary<string, ModFiles> index, VanillaIndex? van = null, CancellationToken cancel = default)
    {
        var r = new ConflictReport();
        int N = ps.Mods.Count;
        r.Loses = new int[N]; r.Wins = new int[N]; r.VanillaCount = new int[N];
        r.VanillaChecked = van != null;
        var mf = new ModFiles?[N];
        int total = 0;
        for (int i = 0; i < N; i++)
        {
            if (!ps.Mods[i].Enabled || !info.ContainsKey(ps.Mods[i].Id)) continue;
            if (!index.TryGetValue(ps.Mods[i].Id, out var f) || !f.Complete) { r.ModsWithoutFiles++; continue; }
            mf[i] = f; total += f.Files.Count; r.ModsIndexed++;
        }
        var byPath = new Dictionary<string, List<int>>(total, StringComparer.Ordinal);
        for (int i = 0; i < N; i++)
        {
            var m = mf[i];
            if (m == null) continue;
            foreach (var f in m.Files)
            {
                if (!byPath.TryGetValue(f, out var v)) byPath[f] = v = new List<int>(1);
                if (v.Count == 0 || v[^1] != i) v.Add(i);
            }
            r.FilesIndexed += m.Files.Count;
        }
        var pairs = new Dictionary<(int, int), (int count, int sev, Dictionary<string, int> areas)>();
        long tick = 0;
        foreach (var kv in byPath)
        {
            if ((++tick & 0x3fff) == 0 && cancel.IsCancellationRequested) return new ConflictReport();
            var v = kv.Value;
            bool isVan = van != null && van.Has(kv.Key);
            if (!isVan && v.Count < 2) continue;
            string area = AreaOf(kv.Key);
            int sev = AreaSeverity(area);
            if (isVan)
            {
                r.Vanilla.Add(new VanillaFile { Path = kv.Key, Mods = v, Sev = sev });
                foreach (int m in v) r.VanillaCount[m]++;
            }
            if (v.Count < 2) continue;
            r.Files.Add(new FileConflict { Path = kv.Key, Mods = v, Sev = sev, Vanilla = isVan });
            r.Wins[v[^1]]++;
            for (int k = 0; k + 1 < v.Count; k++) r.Loses[v[k]]++;
            void Add(int a, int b)
            {
                if (!pairs.TryGetValue((a, b), out var p)) p = (0, 0, new Dictionary<string, int>(StringComparer.Ordinal));
                p.count++;
                p.areas[area] = p.areas.GetValueOrDefault(area) + 1;
                if (sev > p.sev) p.sev = sev;
                pairs[(a, b)] = p;
            }
            if (v.Count <= 12) { for (int x = 0; x < v.Count; x++) for (int y = x + 1; y < v.Count; y++) Add(v[x], v[y]); }
            else for (int x = 0; x + 1 < v.Count; x++) Add(v[x], v[^1]);
        }
        r.Files.Sort((a, b) => a.Sev != b.Sev ? b.Sev.CompareTo(a.Sev) : string.CompareOrdinal(a.Path, b.Path));
        r.Vanilla.Sort((a, b) => a.Sev != b.Sev ? b.Sev.CompareTo(a.Sev) : string.CompareOrdinal(a.Path, b.Path));
        foreach (var kv in pairs)
        {
            var pc = new PairConflict { A = kv.Key.Item1, B = kv.Key.Item2, Count = kv.Value.count, Sev = kv.Value.sev };
            foreach (var ar in kv.Value.areas) pc.Areas.Add(ar);
            pc.Areas.Sort((x, y) => x.Value != y.Value ? y.Value.CompareTo(x.Value) : string.CompareOrdinal(x.Key, y.Key));
            r.Pairs.Add(pc);
        }
        r.Pairs.Sort((x, y) => x.Sev != y.Sev ? y.Sev.CompareTo(x.Sev) : x.Count != y.Count ? y.Count.CompareTo(x.Count) : x.A != y.A ? x.A.CompareTo(y.A) : x.B.CompareTo(y.B));
        for (int rp = 0; rp < N; rp++)
        {
            if (mf[rp] == null || !info.TryGetValue(ps.Mods[rp].Id, out var mi)) continue;
            foreach (var f0 in mi.ReplacePaths)
            {
                string folder = CleanFolder(f0);
                if (folder.Length == 0) continue;
                string prefix = folder + "/";
                for (int v = 0; v < rp; v++)
                {
                    var m = mf[v];
                    if (m == null) continue;
                    int n = 0;
                    foreach (var f in m.Files) if (f.StartsWith(prefix, StringComparison.Ordinal)) n++;
                    if (n > 0) r.Wipes.Add(new WipeHit { Replacer = rp, Victim = v, Files = n, Folder = folder });
                }
            }
        }
        if (van != null)
        {
            for (int rp = 0; rp < N; rp++)
            {
                if (!ps.Mods[rp].Enabled || !info.TryGetValue(ps.Mods[rp].Id, out var mi)) continue;
                foreach (var f0 in mi.ReplacePaths)
                {
                    string folder = CleanFolder(f0);
                    if (folder.Length == 0) continue;
                    int n = van.CountUnder(folder + "/");
                    if (n > 0) r.VanillaWipes.Add(new VanillaWipe { Mod = rp, Files = n, Folder = folder });
                }
            }
        }
        r.Valid = true;
        return r;
    }
}
