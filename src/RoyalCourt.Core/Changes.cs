using System.Text;
namespace RoyalCourt.Core;

// What changed since you last pressed Play (see changes.hpp). The record holds hashes only, 16 bytes a file.

public sealed class Manifest { public List<(ulong, ulong)> V = new(); }   // (path hash, file hash), sorted by path hash
public sealed class ChangeSummary
{
    public bool Known;
    public long Same, Added, Changed, Removed;
    public List<string> AddedNames = new(), ChangedNames = new();
    public SortedDictionary<string, long> Folders = new(StringComparer.Ordinal);
}
public sealed class PlayItem { public string ModId = "", Name = "", Text = ""; public int Sev; }
public sealed class PlayReport
{
    public bool HasPrev;
    public long Time;
    public string PrevGame = "", NowGame = "";
    public bool GameChanged, SamePlayset = true;
    public List<PlayItem> Updated = new(), Outdated = new(), Added = new(), Removed = new();
    public bool Empty => !GameChanged && Updated.Count == 0 && Outdated.Count == 0 && Added.Count == 0 && Removed.Count == 0;
}

public static class Changes
{
    public static ulong PathHash(string rel)
    {
        ulong h = 1099511628211UL ^ 0x9E3779B97F4A7C15UL;
        foreach (byte c in Encoding.UTF8.GetBytes(rel)) { h ^= c; h *= 1099511628211UL; }
        h ^= h >> 31; h *= 0x7fb5d329728ea185UL; h ^= h >> 27;
        return h;
    }

    public static Manifest ManifestOf(ModFiles f)
    {
        var m = new Manifest();
        if (!f.Complete || f.Mix.Count != f.Files.Count) return m;
        m.V.Capacity = f.Files.Count;
        for (int i = 0; i < f.Files.Count; i++) m.V.Add((PathHash(f.Files[i]), f.Mix[i]));
        m.V.Sort();
        return m;
    }

    static string ManifestDir() { string d = Path.Combine(Util.DataDir(), "seen"); Util.CreateDirs(d); return d; }
    public static string ManifestPath(string modId) => Path.Combine(ManifestDir(), "p_" + Walk.Hex64(Scripts.Fnv64(modId, '#', "manifest")) + ".bin");

    public static bool SaveManifest(string modId, Manifest m)
    {
        if (m.V.Count == 0) return false;
        var ms = new MemoryStream();
        var w = new BinaryWriter(ms);
        w.Write(Encoding.ASCII.GetBytes("RCM1"));
        w.Write((ulong)m.V.Count);
        foreach (var e in m.V) { w.Write(e.Item1); w.Write(e.Item2); }
        w.Flush();
        ulong sum = Scripts.Fnv64Bytes(ms.ToArray(), '|', "end");
        w.Write(sum); w.Flush();
        return Util.WriteFileAtomicBytes(ManifestPath(modId), ms.ToArray());
    }

    public static bool LoadManifest(string modId, Manifest m)
    {
        m.V.Clear();
        if (!Util.ReadBytes(ManifestPath(modId), out var t, 64L << 20) || t.Length < 4 + 8 + 8 || t[0] != (byte)'R' || t[1] != (byte)'C' || t[2] != (byte)'M' || t[3] != (byte)'1') return false;
        ulong n = BitConverter.ToUInt64(t, 4);
        if (n > (64UL << 20) / 16 || (ulong)t.Length != 4 + 8 + n * 16 + 8) return false;
        ulong sum = BitConverter.ToUInt64(t, t.Length - 8);
        if (Scripts.Fnv64Bytes(new ReadOnlySpan<byte>(t, 0, t.Length - 8), '|', "end") != sum) return false;
        m.V.Capacity = (int)n;
        for (int i = 0; i < (int)n; i++) m.V.Add((BitConverter.ToUInt64(t, 12 + i * 16), BitConverter.ToUInt64(t, 12 + i * 16 + 8)));
        for (int i = 1; i < m.V.Count; i++) if (m.V[i - 1].Item1 > m.V[i].Item1) { m.V.Clear(); return false; }
        return true;
    }

    /// <summary>Compares the record from the last Play with the mod's files now. Names are listed up to maxNames each.</summary>
    public static ChangeSummary CompareManifest(Manifest old, ModFiles now, int maxNames = 40)
    {
        var s = new ChangeSummary();
        if (old.V.Count == 0 || !now.Complete || now.Mix.Count != now.Files.Count) return s;
        s.Known = true;
        long matched = 0;
        for (int i = 0; i < now.Files.Count; i++)
        {
            ulong ph = PathHash(now.Files[i]);
            int lo = 0, hi = old.V.Count;
            while (lo < hi) { int mid = (lo + hi) / 2; if (old.V[mid].Item1 < ph) lo = mid + 1; else hi = mid; }
            bool added = false, changed = false;
            if (lo >= old.V.Count || old.V[lo].Item1 != ph) added = true;
            else
            {
                matched++;
                bool same = false;
                for (int j = lo; j < old.V.Count && old.V[j].Item1 == ph; j++) if (old.V[j].Item2 == now.Mix[i]) { same = true; break; }
                changed = !same;
            }
            if (!added && !changed) { s.Same++; continue; }
            var names = added ? s.AddedNames : s.ChangedNames;
            if (added) s.Added++; else s.Changed++;
            if (names.Count < maxNames) names.Add(now.Files[i]);
            string rel = now.Files[i];
            int a = rel.IndexOf('/');
            int b = a < 0 ? -1 : rel.IndexOf('/', a + 1);
            string folder = b < 0 ? rel.Substring(0, a < 0 ? 0 : a) : rel.Substring(0, b);
            s.Folders[folder] = s.Folders.GetValueOrDefault(folder) + 1;
        }
        s.Removed = old.V.Count > matched ? old.V.Count - matched : 0;
        return s;
    }

    /// <summary>One line for a list or a log: "3 files changed, 2 added, 1 removed (mostly common/traits)"</summary>
    public static string Describe(ChangeSummary s)
    {
        if (!s.Known) return "";
        if (s.Added == 0 && s.Changed == 0 && s.Removed == 0) return Tr("no file changes");
        string o = "";
        void Part(string t) { if (o.Length > 0) o += ", "; o += t; }
        if (s.Changed > 0) Part(s.Changed == 1 ? Trf("{0} file changed", s.Changed) : Trf("{0} files changed", s.Changed));
        if (s.Added > 0) Part(Trf("{0} added", s.Added));
        if (s.Removed > 0) Part(Trf("{0} removed", s.Removed));
        if (s.Folders.Count > 0)
        {
            KeyValuePair<string, long> best = s.Folders.First();
            foreach (var kv in s.Folders) if (kv.Value > best.Value) best = kv;
            if (best.Key.Length > 0 && s.Folders.Count > 1 && best.Value * 2 > s.Added + s.Changed) o = Trf("{0} (mostly {1})", o, best.Key);
            else if (s.Folders.Count == 1 && best.Key.Length > 0) o = Trf("{0} (in {1})", o, best.Key);
        }
        return o;
    }

    public static PlayReport BuildPlayReport(Settings st, Playset ps, IReadOnlyDictionary<string, ModInfo> info,
                                             IReadOnlyDictionary<string, string> fpNow, IReadOnlyDictionary<string, ModFiles> files, string gameVer)
    {
        var r = new PlayReport { HasPrev = st.PlayTime > 0, Time = st.PlayTime, PrevGame = st.PlayGameVer, NowGame = gameVer };
        r.GameChanged = r.HasPrev && st.PlayGameVer.Length > 0 && gameVer.Length > 0 && st.PlayGameVer != gameVer;
        string NameOf(ModRef m) => info.TryGetValue(m.Id, out var mi) && mi.Name.Length > 0 ? mi.Name : (m.Name.Length == 0 ? m.Id : m.Name);
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled || !info.TryGetValue(m.Id, out var mi)) continue;
            if (gameVer.Length > 0 && ModScan.MatchGameVersion(mi.Supported, gameVer) == VerMatch.Mismatch)
                r.Outdated.Add(new PlayItem { ModId = m.Id, Name = NameOf(m), Text = Trf("made for game version {0}, you have {1}", mi.Supported, gameVer), Sev = 1 });
            if (st.Seen.TryGetValue(m.Id, out var s) && fpNow.TryGetValue(m.Id, out var n) && Compare.FingerprintChanged(s, n))
            {
                string t = Tr("updated or edited");
                var old = new Manifest();
                if (files.TryGetValue(m.Id, out var f) && LoadManifest(m.Id, old))
                {
                    string d = Describe(CompareManifest(old, f));
                    if (d.Length > 0) t = d;
                }
                r.Updated.Add(new PlayItem { ModId = m.Id, Name = NameOf(m), Text = t });
            }
        }
        if (r.HasPrev && st.PlayName == ps.Name)
        {
            var was = new HashSet<string>(st.PlayMods, StringComparer.Ordinal);
            var isOn = new HashSet<string>(StringComparer.Ordinal);
            foreach (var m in ps.Mods) if (m.Enabled) isOn.Add(m.Id);
            foreach (var m in ps.Mods) if (m.Enabled && !was.Contains(m.Id)) r.Added.Add(new PlayItem { ModId = m.Id, Name = NameOf(m), Text = Tr("enabled since then") });
            foreach (var id in st.PlayMods)
            {
                if (isOn.Contains(id)) continue;
                if (!info.TryGetValue(id, out var mi)) r.Removed.Add(new PlayItem { ModId = id, Name = id, Text = Tr("no longer installed"), Sev = 1 });
                else r.Removed.Add(new PlayItem { ModId = id, Name = mi.Name, Text = Tr("turned off since then") });
            }
        }
        else r.SamePlayset = false;
        return r;
    }

    /// <summary>Saves what is needed to compare against at the next Play: the enabled mods' file records and the list of enabled mods.</summary>
    public static void RecordPlay(Settings st, Playset ps, IReadOnlyDictionary<string, ModFiles> files, string gameVer, long now)
    {
        st.PlayGameVer = gameVer; st.PlayName = ps.Name; st.PlayTime = now; st.PlayMods.Clear();
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled) continue;
            st.PlayMods.Add(m.Id);
            if (files.TryGetValue(m.Id, out var f)) { var mf = ManifestOf(f); if (mf.V.Count > 0) SaveManifest(m.Id, mf); }
        }
    }
}
