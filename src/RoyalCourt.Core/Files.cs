using System.Text;
namespace RoyalCourt.Core;

public sealed class ModFiles
{
    public string Fingerprint = "";
    public List<string> Files = new();
    public bool Complete;
    public string Deep = "";
    public List<ulong> Mix = new();
}

public enum WalkResult { Ok = 0, Cancelled = 1, Failed = 2 }

public static class Walk
{
    public static ulong DeepMix(string rel, ulong size, long mtime)
    {
        ulong h = 1469598103934665603UL;
        foreach (byte c in Encoding.UTF8.GetBytes(rel)) { h ^= c; h *= 1099511628211UL; }
        void Put(ulong v) { for (int i = 0; i < 8; i++) { h ^= (byte)(v >> (i * 8)); h *= 1099511628211UL; } }
        Put(size); Put((ulong)mtime);
        h ^= h >> 33; h *= 0xff51afd7ed558ccdUL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53UL; h ^= h >> 33;
        return h;
    }
    public static string Hex64(ulong v) => v.ToString("x16");

    public static bool SkipFileName(string n) => n == "thumbs.db" || n == ".ds_store" || n == ".gitignore" || n == ".gitattributes" || n == "desktop.ini";
    public static bool SkipDirName(string n) => n == ".git" || n == ".github" || n == ".vscode" || n == ".idea" || n == ".svn" || n == "__macosx";

    /// <summary>ASCII-only lower case, '\' to '/': the form used for relative paths.</summary>
    public static string Norm(string s)
    {
        var a = s.ToCharArray();
        for (int i = 0; i < a.Length; i++) { char c = a[i]; if (c == '\\') a[i] = '/'; else if (c >= 'A' && c <= 'Z') a[i] = (char)(c + 32); }
        return new string(a);
    }
    static string AsciiLower(string s) => Norm(s);

    public delegate void WalkCallback(string rel, ulong size, long mtime);

    /// <summary>Calls f(relative path, size, time) for every file below root (lower case, '/' separators, no .git-like folders, no Thumbs.db-like files, links to folders not followed).</summary>
    public static WalkResult WalkFiles(string root, WalkCallback f, CancellationToken cancel = default)
    {
        while (root.Length > 1 && (root[^1] == '/' || root[^1] == '\\')) root = root[..^1];
        int rootLen = root.Length;
        var stack = new Stack<string>();
        stack.Push(root);
        bool failed = false;
        while (stack.Count > 0)
        {
            if (cancel.IsCancellationRequested) return WalkResult.Cancelled;
            string dir = stack.Pop();
            IEnumerable<FileSystemInfo> entries;
            try { entries = new DirectoryInfo(dir).EnumerateFileSystemInfos(); }
            catch { if (dir == root) return WalkResult.Failed; failed = true; continue; }
            try
            {
                foreach (var e in entries)
                {
                    if (cancel.IsCancellationRequested) return WalkResult.Cancelled;
                    string name = AsciiLower(e.Name);
                    if (e is DirectoryInfo)
                    {
                        if ((e.Attributes & FileAttributes.ReparsePoint) != 0 || SkipDirName(name)) continue;
                        stack.Push(e.FullName);
                        continue;
                    }
                    if (SkipFileName(name)) continue;
                    string full = e.FullName;
                    if (full.Length <= rootLen + 1) continue;
                    string rel = Norm(full.Substring(rootLen + 1));
                    ulong sz = 0; long mt = 0;
                    try { var fi = (FileInfo)e; sz = (ulong)fi.Length; mt = fi.LastWriteTimeUtc.Ticks; } catch { }
                    f(rel, sz, mt);
                }
            }
            catch { failed = true; }
        }
        return failed ? WalkResult.Failed : WalkResult.Ok;
    }

    /// <summary>Lists a mod folder. Files directly in the root are not game files and are skipped. If cancelled, the result is marked incomplete.</summary>
    public static ModFiles IndexModFiles(string contentDir, CancellationToken cancel = default)
    {
        var r = new ModFiles();
        if (contentDir.Length == 0) return r;
        ulong sum = 0;
        var rc = WalkFiles(contentDir, (rel, size, mtime) =>
        {
            if (rel.IndexOf('/') < 0) return;
            ulong mx = DeepMix(rel, size, mtime);
            sum += mx; r.Files.Add(rel); r.Mix.Add(mx);
        }, cancel);
        if (rc == WalkResult.Cancelled) return r;
        r.Complete = rc == WalkResult.Ok;
        r.Deep = Hex64(sum) + "-" + r.Files.Count;
        return r;
    }

    public static string ModFingerprint(ModInfo m)
    {
        string t = "?";
        try { t = Directory.GetLastWriteTimeUtc(m.ContentDir).Ticks.ToString(); } catch { }
        return m.ContentDir + "|" + m.Version + "|" + t;
    }
}

// ---------- the game's own files ----------
public sealed class VanillaIndex
{
    public string Key = "", Dir = "";
    public List<string> Files = new();
    public int DlcFiles;
    HashSet<string> set = new(StringComparer.Ordinal);
    public void Finish()
    {
        Files.Sort(StringComparer.Ordinal);
        var u = new List<string>(Files.Count);
        foreach (var f in Files) if (u.Count == 0 || u[^1] != f) u.Add(f);
        Files = u;
        set = new HashSet<string>(Files, StringComparer.Ordinal);
    }
    public bool Has(string rel) => set.Contains(rel);
    public int CountUnder(string prefix)
    {
        int lo = 0, hi = Files.Count;
        while (lo < hi) { int mid = (lo + hi) / 2; if (string.CompareOrdinal(Files[mid], prefix) < 0) lo = mid + 1; else hi = mid; }
        int n = 0;
        while (lo < Files.Count && Files[lo].StartsWith(prefix, StringComparison.Ordinal)) { n++; lo++; }
        return n;
    }
}

public static class Vanilla
{
    public static string GameDir(string gameExe)
    {
        if (gameExe.Length == 0) return "";
        string? p = Path.GetDirectoryName(Path.GetDirectoryName(gameExe));
        if (p == null) return "";
        string d = Path.Combine(p, "game");
        return Directory.Exists(d) ? d : "";
    }
    public static List<string> DlcDirs(string gameDir)
    {
        var v = new List<string>();
        string g = gameDir.TrimEnd('/', '\\');
        string? par = Path.GetDirectoryName(g);
        foreach (var d in new[] { Path.Combine(g, "dlc"), par == null ? "" : Path.Combine(par, "dlc") })
            if (d.Length > 0 && Directory.Exists(d)) v.Add(d);
        return v;
    }
    public static string Key(string gameDir, string gameVer)
    {
        ulong sum = 0; int n = 0;
        void Add(string d)
        {
            try
            {
                foreach (var e in new DirectoryInfo(d).EnumerateDirectories())
                { sum += Walk.DeepMix(Walk.Norm(e.Name), 0, e.LastWriteTimeUtc.Ticks); n++; }
            }
            catch { }
        }
        Add(gameDir);
        foreach (var d in DlcDirs(gameDir)) Add(d);
        return gameDir + "|" + gameVer + "|" + Walk.Hex64(sum) + "-" + n;
    }
    public static VanillaIndex? Index(string gameDir, CancellationToken cancel = default)
    {
        var idx = new VanillaIndex { Dir = gameDir };
        bool WalkOne(string root, bool isDlc)
        {
            var rc = Walk.WalkFiles(root, (rel, _, _) =>
            {
                if (rel.IndexOf('/') < 0 || rel.IndexOf('\n') >= 0) return;
                if (isDlc) idx.DlcFiles++;
                idx.Files.Add(rel);
            }, cancel);
            return rc != WalkResult.Cancelled;
        }
        if (!WalkOne(gameDir, false)) return null;
        foreach (var d in DlcDirs(gameDir))
        {
            IEnumerable<DirectoryInfo> subs;
            try { subs = new DirectoryInfo(d).EnumerateDirectories().ToList(); } catch { continue; }
            foreach (var s in subs)
            {
                if (cancel.IsCancellationRequested) return null;
                if (!WalkOne(s.FullName, true)) return null;
            }
        }
        idx.Finish();
        return idx;
    }
    static string CacheFile(string cacheDir) => Path.Combine(cacheDir, "vanilla.txt");
    public static bool SaveCache(string cacheDir, VanillaIndex v)
    {
        if (cacheDir.Length == 0) return false;
        Util.CreateDirs(cacheDir);
        var sb = new StringBuilder();
        sb.Append("RCVAN1\n").Append(v.Key).Append('\n').Append(v.Dir).Append('\n').Append(v.DlcFiles).Append('\n').Append(v.Files.Count).Append('\n');
        foreach (var f in v.Files) sb.Append(f).Append('\n');
        return Util.WriteFileAtomic(CacheFile(cacheDir), sb.ToString());
    }
    public static VanillaIndex? LoadCache(string cacheDir, string key)
    {
        if (cacheDir.Length == 0) return null;
        if (!Util.ReadFile(CacheFile(cacheDir), out var t, 256 << 20)) return null;
        int pos = 0;
        bool Line(out string o)
        {
            int e = t.IndexOf('\n', pos);
            if (e < 0) { o = ""; return false; }
            o = t.Substring(pos, e - pos); pos = e + 1; return true;
        }
        if (!Line(out var magic) || magic != "RCVAN1" || !Line(out var k) || k != key || !Line(out var dir) || !Line(out var dlc) || !Line(out var cnt)) return null;
        if (!ulong.TryParse(dlc, out var d) || !ulong.TryParse(cnt, out var n) || n > 5000000UL) return null;
        var v = new VanillaIndex { Key = k, Dir = dir, DlcFiles = (int)d };
        while ((ulong)v.Files.Count < n && Line(out var f)) { if (f.Length == 0 || f.IndexOf('/') < 0) return null; v.Files.Add(f); }
        if ((ulong)v.Files.Count != n || pos != t.Length) return null;
        v.Finish();
        if ((ulong)v.Files.Count != n) return null;
        return v;
    }
    public static VanillaIndex? LoadOrIndex(string cacheDir, string gameExe, string gameVer, CancellationToken cancel = default, bool[]? fromCache = null)
    {
        if (fromCache != null && fromCache.Length > 0) fromCache[0] = false;
        string dir = GameDir(gameExe);
        if (dir.Length == 0) return null;
        string key = Key(dir, gameVer);
        var c = LoadCache(cacheDir, key);
        if (c != null) { if (fromCache != null && fromCache.Length > 0) fromCache[0] = true; return c; }
        var v = Index(dir, cancel);
        if (v == null || v.Files.Count == 0) return null;
        v.Key = key;
        SaveCache(cacheDir, v);
        return v;
    }
}
