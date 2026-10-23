using System.Text;
namespace RoyalCourt.Core;

public static class DK { public const int Common = 0, Event = 1, Loc = 2, Define = 3, OnAction = 4, Kinds = 5; }
public static class DF { public const byte Replace = 1, Priority = 2, Overwrites = 4; }

public struct DefEntry { public ulong H; public uint File, NameOff; public ushort NameLen; public byte Kind, Flags; }

public sealed class ModDefs
{
    public string Fingerprint = "";
    public bool Complete;
    public List<string> Files = new();
    public List<DefEntry> Defs = new();
    public string Pool = "";             // key texts, back to back (offsets are in UTF-16 units)
    public long BytesRead;
    public string NameOf(in DefEntry d) => Pool.Substring((int)d.NameOff, d.NameLen);
}

public struct ScriptKey { public string Key; public byte Flags; }
public struct DefFileKind { public int Kind; public string Area; public int Mode; public byte Flags; }

public static class Scripts
{
    public static ulong Fnv64(string a, char sep, string b)
    {
        ulong h = 1469598103934665603UL;
        foreach (byte c in Encoding.UTF8.GetBytes(a)) { h ^= c; h *= 1099511628211UL; }
        h ^= (byte)sep; h *= 1099511628211UL;
        foreach (byte c in Encoding.UTF8.GetBytes(b)) { h ^= c; h *= 1099511628211UL; }
        return h;
    }
    public static ulong Fnv64Bytes(ReadOnlySpan<byte> a, char sep, string b)
    {
        ulong h = 1469598103934665603UL;
        foreach (byte c in a) { h ^= c; h *= 1099511628211UL; }
        h ^= (byte)sep; h *= 1099511628211UL;
        foreach (byte c in Encoding.UTF8.GetBytes(b)) { h ^= c; h *= 1099511628211UL; }
        return h;
    }

    static bool IsOp(char c) => c == '=' || c == '<' || c == '>' || c == '!' || c == '?';

    /// <summary>Reads the top-level definitions of a Paradox script file.
    /// mode 0: "name = { ... }" objects only; mode 1: objects and "name = value"; mode 2 (defines): "Block = { name = value / { } }" -> "Block.name"</summary>
    public static void ScanScriptKeys(string t, int mode, List<ScriptKey> outv)
    {
        int i = 0, n = t.Length;
        if (n >= 1 && t[0] == '﻿') i = 1;
        int depth = 0;
        string cand = "", block = "";
        int candDepth = -1;
        bool hasCand = false, afterOp = false;
        int cur = -1;
        void Emit(string k, bool obj)
        {
            if (k.Length == 0 || k[0] == '@') { cur = -1; return; }
            if (mode == 0 && !obj) return;
            outv.Add(new ScriptKey { Key = k });
            cur = obj ? outv.Count - 1 : -1;
        }
        while (i < n)
        {
            char c = t[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
            if (c == '#') { while (i < n && t[i] != '\n') i++; continue; }
            if (c == '{')
            {
                if (hasCand && afterOp)
                {
                    if (mode == 2) { if (candDepth == 0) block = cand; else if (candDepth == 1) Emit(block + "." + cand, true); }
                    else if (candDepth == 0) Emit(cand, true);
                }
                depth++; hasCand = false; afterOp = false; i++; continue;
            }
            if (c == '}') { if (depth > 0) depth--; if (depth == 0) cur = -1; hasCand = false; afterOp = false; i++; continue; }
            if (IsOp(c))
            {
                while (i < n && IsOp(t[i])) i++;
                if (hasCand)
                {
                    afterOp = true;
                    if (mode != 2 && candDepth == 1 && cur >= 0)
                    {
                        var sk = outv[cur];
                        if (cand == "effect" || cand == "trigger") { sk.Flags |= DF.Overwrites; outv[cur] = sk; }
                        else if (cand == "id_override_priority") { sk.Flags |= DF.Priority; outv[cur] = sk; }
                    }
                }
                continue;
            }
            string tok;
            if (c == '"')
            {
                i++;
                var sb = new StringBuilder();
                while (i < n && t[i] != '"') { if (t[i] == '\\' && i + 1 < n) i++; sb.Append(t[i]); i++; }
                if (i < n) i++;
                tok = sb.ToString();
            }
            else
            {
                int s = i;
                while (i < n) { char d = t[i]; if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == '{' || d == '}' || d == '"' || d == '#' || IsOp(d)) break; i++; }
                tok = t.Substring(s, i - s);
            }
            if (hasCand && afterOp)
            {
                if (mode == 2) { if (candDepth == 1) Emit(block + "." + cand, false); }
                else if (mode == 1 && candDepth == 0) Emit(cand, false);
                hasCand = false; afterOp = false;
                continue;
            }
            if ((mode == 2 && depth <= 1) || (mode != 2 && depth == 0) || (mode != 2 && depth == 1)) { cand = tok; candDepth = depth; hasCand = true; afterOp = false; }
            else hasCand = false;
        }
    }

    /// <summary>"localization/english/foo_l_english.yml" lines: key:0 "text"</summary>
    public static void ScanLocKeys(string t, List<string> outv)
    {
        int i = 0, n = t.Length;
        if (n >= 1 && t[0] == '﻿') i = 1;
        while (i < n)
        {
            int e = t.IndexOf('\n', i);
            if (e < 0) e = n;
            int s = i;
            while (s < e && (t[s] == ' ' || t[s] == '\t')) s++;
            if (s < e && t[s] != '#')
            {
                int colon = t.IndexOf(':', s);
                if (colon >= 0 && colon < e && colon > s)
                {
                    int q = colon + 1;
                    while (q < e && ((t[q] >= '0' && t[q] <= '9') || t[q] == ' ' || t[q] == '\t')) q++;
                    bool key = true;
                    for (int k = s; k < colon; k++) if (t[k] == ' ' || t[k] == '\t' || t[k] == '"') { key = false; break; }
                    if (key && q < e && t[q] == '"') outv.Add(t.Substring(s, colon - s));
                }
            }
            i = e + 1;
        }
    }

    public static DefFileKind FileKind(string rel)
    {
        var k = new DefFileKind { Kind = -1, Area = "" };
        bool Ends(string e) => rel.Length > e.Length && rel.EndsWith(e, StringComparison.Ordinal);
        if (Ends(".txt"))
        {
            if (rel.StartsWith("events/", StringComparison.Ordinal)) { k.Kind = DK.Event; k.Area = "events"; return k; }
            if (rel.StartsWith("common/", StringComparison.Ordinal))
            {
                int b = rel.IndexOf('/', 7);
                if (b < 0) return k;
                string sub = rel.Substring(7, b - 7);
                k.Area = "common/" + sub;
                if (sub == "defines") { k.Kind = DK.Define; k.Mode = 2; }
                else if (sub == "on_action" || sub == "on_actions") k.Kind = DK.OnAction;
                else { k.Kind = DK.Common; k.Mode = sub == "script_values" ? 1 : 0; }
            }
        }
        else if (Ends(".yml") && rel.StartsWith("localization/", StringComparison.Ordinal))
        {
            var parts = rel.Split('/');
            if (parts.Length < 3) return k;
            string lang;
            if (parts[1] == "replace") { lang = parts[2]; k.Flags |= DF.Replace; }
            else { lang = parts[1]; if (parts[2] == "replace") k.Flags |= DF.Replace; }
            k.Kind = DK.Loc; k.Area = "localization/" + lang;
        }
        return k;
    }

    public static ModDefs IndexModDefs(string contentDir, IReadOnlyList<string> files, CancellationToken cancel = default)
    {
        var md = new ModDefs();
        var keys = new List<ScriptKey>();
        var locKeys = new List<string>();
        var pool = new StringBuilder();
        foreach (string rel in files)
        {
            if (cancel.IsCancellationRequested) return md;
            var fk = FileKind(rel);
            if (fk.Kind < 0) continue;
            if (!Util.ReadFile(Path.Combine(contentDir, rel.Replace('/', Path.DirectorySeparatorChar)), out var text, 96L << 20)) continue;
            md.BytesRead += Util.Utf8.GetByteCount(text);
            uint fi = (uint)md.Files.Count;
            md.Files.Add(rel);
            void Add(string key, byte flags)
            {
                if (key.Length == 0 || key.Length > 65000) return;
                md.Defs.Add(new DefEntry { H = Fnv64(fk.Area, '\x1f', key), File = fi, NameOff = (uint)pool.Length, NameLen = (ushort)key.Length, Kind = (byte)fk.Kind, Flags = flags });
                pool.Append(key);
            }
            if (fk.Kind == DK.Loc)
            {
                locKeys.Clear();
                ScanLocKeys(text, locKeys);
                foreach (var k in locKeys) Add(k, fk.Flags);
            }
            else
            {
                keys.Clear();
                ScanScriptKeys(text, fk.Mode, keys);
                foreach (var k in keys)
                {
                    if (fk.Kind == DK.Event && k.Key.IndexOf('.') < 0) continue;
                    Add(k.Key, k.Flags);
                }
            }
        }
        md.Pool = pool.ToString();
        md.Complete = true;
        return md;
    }

    // ---------- saved file index ----------
    public const uint IndexCacheFormat = 1, DefsParserVersion = 1;

    public static string ModCacheFile(string cacheDir, string id) => Path.Combine(cacheDir, "m_" + Walk.Hex64(Fnv64(id, '\x1e', "RCIX")) + ".bin");

    sealed class Out
    {
        public MemoryStream M = new(); public BinaryWriter W;
        public Out() { W = new BinaryWriter(M, Util.Utf8); }
        public void Str(string s) { var b = Util.Utf8.GetBytes(s); W.Write((uint)b.Length); W.Write(b); }
    }
    sealed class In
    {
        readonly byte[] b; public int P; public bool Ok = true; readonly int end;
        public In(byte[] data, int len) { b = data; end = len; }
        bool Need(int n) { if (!Ok || n < 0 || end - P < n) Ok = false; return Ok; }
        public byte U8() => Need(1) ? b[P++] : (byte)0;
        public ushort U16() { if (!Need(2)) return 0; var v = BitConverter.ToUInt16(b, P); P += 2; return v; }
        public uint U32() { if (!Need(4)) return 0; var v = BitConverter.ToUInt32(b, P); P += 4; return v; }
        public ulong U64() { if (!Need(8)) return 0; var v = BitConverter.ToUInt64(b, P); P += 8; return v; }
        public string Str() { uint n = U32(); if (!Ok || n > int.MaxValue || !Need((int)n)) return ""; var s = Util.Utf8.GetString(b, P, (int)n); P += (int)n; return s; }
        public int Remaining => end - P;
    }

    public static bool SaveModCache(string cacheDir, string id, string deep, ModDefs md)
    {
        if (cacheDir.Length == 0 || deep.Length == 0 || !md.Complete) return false;
        Util.CreateDirs(cacheDir);
        var o = new Out();
        o.W.Write(Encoding.ASCII.GetBytes("RCIX"));
        o.W.Write(IndexCacheFormat); o.W.Write(DefsParserVersion);
        o.Str(id); o.Str(deep);
        o.W.Write((ulong)md.BytesRead);
        o.W.Write((uint)md.Files.Count);
        foreach (var f in md.Files) o.Str(f);
        o.W.Write((uint)md.Defs.Count);
        foreach (var d in md.Defs) { o.W.Write(d.H); o.W.Write(d.File); o.W.Write(d.NameOff); o.W.Write(d.NameLen); o.W.Write(d.Kind); o.W.Write(d.Flags); }
        o.Str(md.Pool);
        var body = o.M.ToArray();
        o.W.Write(Fnv64Bytes(body, '\x1d', "RCIX"));
        return Util.WriteFileAtomicBytes(ModCacheFile(cacheDir, id), o.M.ToArray());
    }

    public static bool LoadModCache(string cacheDir, string id, string deep, out ModDefs md)
    {
        md = new ModDefs();
        if (cacheDir.Length == 0 || deep.Length == 0) return false;
        if (!Util.ReadBytes(ModCacheFile(cacheDir, id), out var t, 512L << 20) || t.Length < 4 + 4 + 4 + 8) return false;
        int bodyLen = t.Length - 8;
        ulong sum = BitConverter.ToUInt64(t, bodyLen);
        if (sum != Fnv64Bytes(new ReadOnlySpan<byte>(t, 0, bodyLen), '\x1d', "RCIX")) return false;
        if (t[0] != (byte)'R' || t[1] != (byte)'C' || t[2] != (byte)'I' || t[3] != (byte)'X') return false;
        var inp = new In(t, bodyLen) { P = 4 };
        if (inp.U32() != IndexCacheFormat || inp.U32() != DefsParserVersion) return false;
        if (inp.Str() != id || inp.Str() != deep) return false;
        var r = new ModDefs { BytesRead = (long)inp.U64() };
        uint nf = inp.U32();
        if (!inp.Ok || nf > (uint)(inp.Remaining / 4)) return false;
        for (uint i = 0; i < nf; i++) r.Files.Add(inp.Str());
        uint nd = inp.U32();
        if (!inp.Ok || nd > (uint)(inp.Remaining / 20)) return false;
        r.Defs.Capacity = (int)nd;
        for (uint i = 0; i < nd; i++)
            r.Defs.Add(new DefEntry { H = inp.U64(), File = inp.U32(), NameOff = inp.U32(), NameLen = inp.U16(), Kind = inp.U8(), Flags = inp.U8() });
        r.Pool = inp.Str();
        if (!inp.Ok || inp.P != bodyLen) return false;
        foreach (var d in r.Defs)
            if (d.File >= r.Files.Count || d.Kind >= DK.Kinds || (long)d.NameOff + d.NameLen > r.Pool.Length) return false;
        r.Complete = true;
        md = r;
        return true;
    }

    /// <summary>Removes saved indexes of mods that are no longer installed. Only files that look like ours are touched.</summary>
    public static int PruneModCache(string cacheDir, IEnumerable<string> keepIds)
    {
        if (cacheDir.Length == 0 || !Directory.Exists(cacheDir)) return 0;
        var keep = new HashSet<string>(StringComparer.Ordinal);
        foreach (var id in keepIds) keep.Add(Path.GetFileName(ModCacheFile(cacheDir, id)));
        int removed = 0;
        var kill = new List<string>();
        foreach (var e in Util.ListDir(cacheDir))
        {
            if (e is not FileInfo) continue;
            string n = e.Name;
            bool ours = n.Length == 2 + 16 + 4 && n.StartsWith("m_", StringComparison.Ordinal) && n.EndsWith(".bin", StringComparison.Ordinal);
            for (int i = 2; ours && i < 18; i++) ours = Uri.IsHexDigit(n[i]);
            if (ours && !keep.Contains(n)) kill.Add(e.FullName);
        }
        foreach (var k in kill) { try { File.Delete(k); removed++; } catch { } }
        return removed;
    }

    public sealed class IndexedMod { public ModFiles Files = new(); public ModDefs Defs = new(); public bool FromCache; }

    public static IndexedMod IndexModCached(string cacheDir, string id, string contentDir, CancellationToken cancel = default)
    {
        var r = new IndexedMod { Files = Walk.IndexModFiles(contentDir, cancel) };
        if (!r.Files.Complete) return r;
        if (LoadModCache(cacheDir, id, r.Files.Deep, out var d)) { r.Defs = d; r.FromCache = true; return r; }
        r.Defs = IndexModDefs(contentDir, r.Files.Files, cancel);
        if (r.Defs.Complete) SaveModCache(cacheDir, id, r.Files.Deep, r.Defs);
        return r;
    }

    // ---------- script conflicts ----------
    public sealed class ScriptHit { public int Mod; public string File = ""; public byte Flags; }
    public sealed class ScriptConflict
    {
        public int Kind, Sev, Winner = -1;
        public string Area = "", Key = "", Note = "";
        public List<ScriptHit> Hits = new();
    }
    public sealed class ScriptReport
    {
        public List<ScriptConflict> Items = new();
        public int[][] PerMod = Array.Empty<int[]>();
        public int[] Unclear = Array.Empty<int>();
        public int ModsRead, ModsMissing;
        public bool Valid;
    }

    public static string DefKindName(int k)
    {
        string[] n = { K("Definition"), K("Event ID"), K("Localization key"), K("Define"), K("On_action") };
        return k >= 0 && k < DK.Kinds ? Tr(n[k]) : "";
    }

    struct Rec { public ulong H; public int Mod; public uint Idx; }

    static string BaseName(string p) { int s = p.LastIndexOf('/'); return s < 0 ? p : p.Substring(s + 1); }

    public static ScriptReport FindScriptConflicts(Playset ps, IReadOnlyDictionary<string, ModInfo> info, IReadOnlyDictionary<string, ModDefs> index)
    {
        var r = new ScriptReport();
        int n = ps.Mods.Count;
        r.PerMod = new int[n][];
        for (int i = 0; i < n; i++) r.PerMod[i] = new int[DK.Kinds];
        r.Unclear = new int[n];
        var md = new ModDefs?[n];
        int total = 0;
        for (int i = 0; i < n; i++)
        {
            if (!ps.Mods[i].Enabled || !info.ContainsKey(ps.Mods[i].Id)) continue;
            if (!index.TryGetValue(ps.Mods[i].Id, out var d) || !d.Complete) { r.ModsMissing++; continue; }
            md[i] = d; total += d.Defs.Count; r.ModsRead++;
        }
        var recs = new Rec[total];
        int w = 0;
        for (int i = 0; i < n; i++)
        {
            var m = md[i];
            if (m == null) continue;
            for (int k = 0; k < m.Defs.Count; k++) recs[w++] = new Rec { H = m.Defs[k].H, Mod = i, Idx = (uint)k };
        }
        Array.Sort(recs, (a, b) => a.H != b.H ? a.H.CompareTo(b.H) : a.Mod != b.Mod ? a.Mod.CompareTo(b.Mod) : a.Idx.CompareTo(b.Idx));
        for (int a = 0; a < recs.Length;)
        {
            int b = a;
            while (b < recs.Length && recs[b].H == recs[a].H) b++;
            if (recs[a].Mod != recs[b - 1].Mod)
            {
                var sc = new ScriptConflict();
                var paths = new HashSet<string>(StringComparer.Ordinal);
                bool anyPriority = false, anyReplace = false;
                int overwriters = 0;
                for (int x = a; x < b; x++)
                {
                    var m = md[recs[x].Mod]!;
                    var d = m.Defs[(int)recs[x].Idx];
                    var h = new ScriptHit { Mod = recs[x].Mod, File = m.Files[(int)d.File], Flags = d.Flags };
                    paths.Add(h.File);
                    if ((d.Flags & DF.Priority) != 0) anyPriority = true;
                    if ((d.Flags & DF.Replace) != 0) anyReplace = true;
                    if ((d.Flags & DF.Overwrites) != 0) overwriters++;
                    if (x == a) { sc.Kind = d.Kind; sc.Key = m.NameOf(d); sc.Area = FileKind(m.Files[(int)d.File]).Area; }
                    sc.Hits.Add(h);
                }
                {
                    var lastMod = new Dictionary<string, int>(StringComparer.Ordinal);
                    foreach (var h in sc.Hits) { lastMod.TryGetValue(h.File, out int m0); if (h.Mod > m0) lastMod[h.File] = h.Mod; else lastMod[h.File] = m0; }
                    int o = 0;
                    for (int x = 0; x < sc.Hits.Count; x++) if (sc.Hits[x].Mod >= lastMod[sc.Hits[x].File]) { if (o != x) sc.Hits[o] = sc.Hits[x]; o++; }
                    sc.Hits.RemoveRange(o, sc.Hits.Count - o);
                }
                var modsLeft = new HashSet<int>();
                foreach (var h in sc.Hits) modsLeft.Add(h.Mod);
                bool keep = true;
                if (modsLeft.Count < 2) keep = false;
                else if (paths.Count == 1) keep = false;
                else if (sc.Kind == DK.Event && anyPriority) keep = false;
                else if (sc.Kind == DK.Loc && anyReplace) keep = false;
                else if (sc.Kind == DK.OnAction && overwriters < 2) keep = false;
                if (keep)
                {
                    if (sc.Kind == DK.Common || sc.Kind == DK.Define)
                    {
                        int byLoad = sc.Hits[^1].Mod, byName = sc.Hits[0].Mod;
                        string bestKey = "", bestFile = "";
                        foreach (var h in sc.Hits)
                        {
                            string k = Util.Lower(BaseName(h.File)) + "|" + Util.Lower(h.File);
                            if (bestKey.Length == 0 || string.CompareOrdinal(k, bestKey) > 0) { bestKey = k; byName = h.Mod; bestFile = BaseName(h.File); }
                        }
                        if (byLoad == byName) { sc.Winner = byLoad; sc.Sev = 0; sc.Note = Tr("The later definition replaces the earlier ones"); }
                        else { sc.Winner = -1; sc.Sev = 1; sc.Note = Trf("Unclear who wins: file name order favours \"{0}\", load order favours the later mod", bestFile); }
                    }
                    else if (sc.Kind == DK.Event) { sc.Sev = 1; sc.Note = Tr("Duplicate event ID: the game logs an error and ignores one of them"); }
                    else if (sc.Kind == DK.OnAction) { sc.Sev = 1; sc.Note = Tr("Several mods set effect/trigger of this on_action: only one takes effect"); }
                    else { sc.Sev = 0; sc.Note = Tr("Same text key defined by several mods"); }
                    var seen = new HashSet<int>();
                    foreach (var h in sc.Hits) if (seen.Add(h.Mod)) { r.PerMod[h.Mod][sc.Kind]++; if (sc.Winner < 0 && sc.Sev > 0) r.Unclear[h.Mod]++; }
                    r.Items.Add(sc);
                }
            }
            a = b;
        }
        r.Items.Sort((x, y) =>
        {
            if (x.Sev != y.Sev) return y.Sev.CompareTo(x.Sev);
            if (x.Kind != y.Kind) return x.Kind.CompareTo(y.Kind);
            int c = string.CompareOrdinal(x.Area, y.Area);
            return c != 0 ? c : string.CompareOrdinal(x.Key, y.Key);
        });
        r.Valid = true;
        return r;
    }

    public static void AddScriptIssues(List<ModIssue>[] issues, ScriptReport sr, Playset ps)
    {
        if (!sr.Valid || issues.Length != ps.Mods.Count || sr.PerMod.Length != ps.Mods.Count) return;
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            var c = sr.PerMod[i];
            int ev = c[DK.Event], oa = c[DK.OnAction], com = c[DK.Common] + c[DK.Define], loc = c[DK.Loc];
            if (ev > 0) issues[i].Add(new ModIssue(1, Trf("{0} event ID(s) are also used by other mods (duplicate event IDs)", ev)));
            int other = sr.Unclear[i] - ev - oa; if (other < 0) other = 0;
            if (oa > 0) issues[i].Add(new ModIssue(0, Trf("{0} on_action(s) are also extended by other mods", oa)));
            if (other > 0) issues[i].Add(new ModIssue(0, Trf("{0} definition(s) are also defined by other mods (see Conflicts > By definition)", other)));
            if (com > 0 && other == 0) issues[i].Add(new ModIssue(0, Trf("{0} script definition(s) are also defined by other mods", com)));
            if (loc > 0) issues[i].Add(new ModIssue(0, Trf("{0} localization key(s) are also defined by other mods", loc)));
            Analyze.StableSortBySevDesc(issues[i]);
        }
    }
}

// ---------- playset comparison & update detection ----------
public sealed class PlaysetDiff
{
    public List<string> OnlyA = new(), OnlyB = new(), EnabledDiffers = new(), Moved = new();
    public int Shared; public bool SameOrder = true;
}

public static class Compare
{
    public static PlaysetDiff ComparePlaysets(Playset a, Playset b, IReadOnlyDictionary<string, ModInfo> info)
    {
        var d = new PlaysetDiff();
        string Nm(ModRef m) => info.TryGetValue(m.Id, out var mi) ? mi.Name : (m.Name.Length == 0 ? m.Id : m.Name);
        var ma = new Dictionary<string, ModRef>(StringComparer.Ordinal); var mb = new Dictionary<string, ModRef>(StringComparer.Ordinal);
        foreach (var m in a.Mods) ma[m.Id] = m;
        foreach (var m in b.Mods) mb[m.Id] = m;
        var oa = new List<string>(); var ob = new List<string>();
        foreach (var m in a.Mods)
        {
            bool has = mb.TryGetValue(m.Id, out var o);
            bool eb = has && o!.Enabled;
            if (m.Enabled && !eb) { if (!has) d.OnlyA.Add(Nm(m)); else d.EnabledDiffers.Add(Trf("{0} (on in {1}, off in {2})", Nm(m), a.Name, b.Name)); }
            else if (m.Enabled && eb) oa.Add(m.Id);
        }
        foreach (var m in b.Mods)
        {
            bool has = ma.TryGetValue(m.Id, out var o);
            bool ea = has && o!.Enabled;
            if (m.Enabled && !ea) { if (!has) d.OnlyB.Add(Nm(m)); else d.EnabledDiffers.Add(Trf("{0} (on in {1}, off in {2})", Nm(m), b.Name, a.Name)); }
            else if (m.Enabled && ea) ob.Add(m.Id);
        }
        d.Shared = oa.Count;
        d.SameOrder = oa.SequenceEqual(ob, StringComparer.Ordinal);
        if (!d.SameOrder)
        {
            var pb = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int i = 0; i < ob.Count; i++) pb[ob[i]] = i;
            int n = oa.Count;
            var best = new int[n]; var prev = new int[n];
            for (int i = 0; i < n; i++) { best[i] = 1; prev[i] = -1; }
            for (int i = 0; i < n; i++) for (int j = 0; j < i; j++) if (pb[oa[j]] < pb[oa[i]] && best[j] + 1 > best[i]) { best[i] = best[j] + 1; prev[i] = j; }
            int end = 0; for (int i = 0; i < n; i++) if (best[i] > best[end]) end = i;
            var keep = new HashSet<int>(); for (int i = end; i >= 0; i = prev[i]) keep.Add(i);
            for (int i = 0; i < n; i++) if (!keep.Contains(i)) d.Moved.Add(info.TryGetValue(oa[i], out var mi) ? mi.Name : oa[i]);
        }
        return d;
    }

    public static bool FingerprintChanged(string stored, string now)
    {
        static void Split(string f, out string quick, out string bas, out string deep)
        {
            int d = f.IndexOf("#d:", StringComparison.Ordinal);
            quick = d < 0 ? f : f[..d];
            deep = d < 0 ? "" : f[(d + 3)..];
            int t = quick.LastIndexOf('|');
            bas = t < 0 ? quick : quick[..t];
        }
        Split(stored, out var q1, out var b1, out var d1); Split(now, out var q2, out var b2, out var d2);
        if (d1.Length > 0 && d2.Length > 0) return b1 != b2 || d1 != d2;
        return q1 != q2;
    }

    public static void AddUpdateIssues(List<ModIssue>[] issues, Playset ps, IReadOnlyDictionary<string, string> seen, IReadOnlyDictionary<string, string> now)
    {
        if (issues.Length != ps.Mods.Count) return;
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            if (!ps.Mods[i].Enabled) continue;
            if (!seen.TryGetValue(ps.Mods[i].Id, out var s) || !now.TryGetValue(ps.Mods[i].Id, out var n) || !FingerprintChanged(s, n)) continue;
            issues[i].Add(new ModIssue(1, Tr("Updated since you last pressed Play (Workshop update or edited files)")));
        }
    }
}
