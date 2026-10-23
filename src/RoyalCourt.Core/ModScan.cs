using System.Text;
namespace RoyalCourt.Core;

public enum VerMatch { Unknown, Match, Mismatch }

public static class ModScan
{
    public static string AutoFindCk3Dir(IEnumerable<string> docRoots)
    {
        string? e = Environment.GetEnvironmentVariable("RC_CK3_DIR");
        if (!string.IsNullOrEmpty(e)) return e;
        foreach (var r in docRoots)
        {
            string c = Path.Combine(r, "Paradox Interactive", "Crusader Kings III");
            if (Util.DirExists(c)) return c;
        }
        return "";
    }

    // ---------- .mod descriptor (Paradox script) ----------
    // key="value"   key={ "a" "b" }   repeated keys (replace_path), comments (#), CRLF, BOM. Nested braces are flattened.
    readonly record struct Tok(char Kind, string S);   // 'w' word, '=' '{' '}'

    public static Dictionary<string, List<string>> ParseDescriptor(string text)
    {
        var kv = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        var t = new List<Tok>();
        int i = 0, n = text.Length;
        if (n >= 1 && text[0] == '\uFEFF') i = 1;
        const string stop = " \t\r\n={}\"#";
        while (i < n && t.Count < 200000)
        {
            char c = text[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
            if (c == '#') { while (i < n && text[i] != '\n') i++; continue; }
            if (c == '=' || c == '{' || c == '}') { t.Add(new Tok(c, "")); i++; continue; }
            if (c == '"')
            {
                var v = new StringBuilder(); i++;
                while (i < n && text[i] != '"' && text[i] != '\n')
                {
                    // \" is an escaped quote only if the string goes on after it; a path that ends in a backslash ("C:\\mods\\x\\") closes there, and \\ stays as written (UNC paths)
                    if (text[i] == '\\' && i + 1 < n && text[i + 1] == '"' && QuoteBeforeNewline(text, i + 2)) { v.Append('"'); i += 2; }
                    else v.Append(text[i++]);
                }
                if (i < n && text[i] == '"') i++;
                t.Add(new Tok('w', v.ToString()));
                continue;
            }
            var w = new StringBuilder();
            while (i < n && stop.IndexOf(text[i]) < 0) w.Append(text[i++]);
            if (w.Length == 0) { i++; continue; }
            t.Add(new Tok('w', w.ToString()));
        }
        int p = 0;
        while (p < t.Count)
        {
            if (t[p].Kind != 'w') { p++; continue; }
            string key = t[p].S;
            if (p + 1 >= t.Count || t[p + 1].Kind != '=') { p++; continue; }
            p += 2;
            if (p >= t.Count) break;
            if (!kv.TryGetValue(key, out var vals)) kv[key] = vals = new List<string>();
            if (t[p].Kind == 'w') { vals.Add(t[p].S); p++; }
            else if (t[p].Kind == '{')
            {
                int depth = 1; p++;
                while (p < t.Count && depth > 0)
                {
                    if (t[p].Kind == '{') depth++;
                    else if (t[p].Kind == '}') depth--;
                    else if (t[p].Kind == 'w' && depth == 1) vals.Add(t[p].S);
                    p++;
                }
            }
        }
        return kv;
    }
    // text.find('"', from) < text.find('\n', from)  (npos counts as "very far")
    static bool QuoteBeforeNewline(string text, int from)
    {
        int q = from <= text.Length ? text.IndexOf('"', from) : -1;
        int nl = from <= text.Length ? text.IndexOf('\n', from) : -1;
        if (q < 0) return false;
        return nl < 0 || q < nl;
    }

    public static List<ModInfo> ScanMods(string dir)
    {
        var outv = new List<ModInfo>();
        if (dir.Length == 0) return outv;
        string md = Path.Combine(dir, "mod");
        if (!Util.DirExists(md)) return outv;
        foreach (var e in Util.ListDir(md))
        {
            if (e is not FileInfo) continue;
            string ext = Util.Lower(e.Extension);
            if (ext != ".mod") continue;
            if (!Util.ReadFile(e.FullName, out var text)) continue;
            var kv = ParseDescriptor(text);
            string First(string k) => kv.TryGetValue(k, out var v) && v.Count > 0 ? v[0] : "";
            var mi = new ModInfo { Id = e.Name };
            mi.Source = mi.Id.StartsWith("ugc_", StringComparison.Ordinal) ? "Workshop" : "Local";
            mi.Name = First("name"); mi.Version = First("version"); mi.Supported = First("supported_version");
            mi.Path = First("path"); mi.Archive = First("archive");
            if (kv.TryGetValue("dependencies", out var deps)) mi.Deps = new List<string>(deps);
            if (kv.TryGetValue("replace_path", out var rp)) mi.ReplacePaths = new List<string>(rp);
            if (kv.TryGetValue("tags", out var tg)) mi.Tags = new List<string>(tg);
            if (mi.Path.Length > 0)
            {
                string cp = mi.Path;
                if (!Path.IsPathRooted(cp)) cp = Path.Combine(dir, cp);
                mi.ContentDir = cp;
                mi.ContentState = Util.DirExists(cp) ? 1 : 2;
            }
            if (mi.Name.Length == 0) mi.Name = mi.Id;
            outv.Add(mi);
        }
        // std::sort with lower(name) < lower(name); names that compare equal keep no particular order: use a stable order for determinism
        return outv.OrderBy(m => Util.Lower(m.Name), StringComparer.Ordinal).ToList();
    }

    // ---------- game version ----------
    // "v1.14.*" / "1.14.1 (Scythe)" / "\"1.14\"" -> segments {"1","14","*"}; stops at the first character that is not a digit, dot or '*'.
    public static List<string> VersionSegments(string s)
    {
        var seg = new List<string>();
        int i = 0;
        while (i < s.Length && !((s[i] >= '0' && s[i] <= '9') || s[i] == '*')) i++;
        var cur = new StringBuilder();
        for (; i < s.Length; i++)
        {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '*') cur.Append(c);
            else if (c == '.') { if (cur.Length > 0) seg.Add(cur.ToString()); cur.Clear(); }
            else break;
        }
        if (cur.Length > 0) seg.Add(cur.ToString());
        return seg;
    }

    /// <summary>Installed game version from launcher-settings.json ("rawVersion", else "version"). This file belongs to the game install, not to the Paradox Launcher's data.</summary>
    public static string GameVersionFromSettings(string json)
    {
        if (!J.TryParse(json, out var root) || !root.IsObj) return "";
        foreach (var key in new[] { "rawVersion", "version" })
        {
            var v = root.Get(key);
            if (v is { IsStr: true })
            {
                var seg = VersionSegments(v.S);
                if (seg.Count == 0 || seg[0] == "*") continue;
                return string.Join('.', seg);
            }
        }
        return "";
    }

    /// <summary>exePath = ...\Crusader Kings III\binaries\ck3.exe -> ...\Crusader Kings III\launcher\launcher-settings.json</summary>
    public static string ReadGameVersion(string exePath)
    {
        if (exePath.Length == 0) return "";
        string? root = Path.GetDirectoryName(Path.GetDirectoryName(exePath));
        if (root == null) return "";
        if (!Util.ReadFile(Path.Combine(root, "launcher", "launcher-settings.json"), out var text, 1 << 20)) return "";
        return GameVersionFromSettings(text);
    }

    /// <summary>Does a mod's supported_version ("1.14.*", "1.14", "1.14.2") fit the installed game version? Compares major.minor only.</summary>
    public static VerMatch MatchGameVersion(string supported, string installed)
    {
        var want = VersionSegments(supported); var have = VersionSegments(installed);
        if (want.Count == 0 || have.Count == 0) return VerMatch.Unknown;
        // Only major.minor (1.20) decides: patch releases (1.20.0.3 vs 1.20.0.4) keep working mods working.
        for (int i = 0; i < want.Count && i < 2; i++)
        {
            if (want[i] == "*") return VerMatch.Match;
            if (i >= have.Count) return VerMatch.Unknown;
            if (want[i].Contains('*')) return VerMatch.Unknown;
            if (want[i].Length > 9 || have[i].Length > 9) return want[i] == have[i] ? VerMatch.Match : VerMatch.Mismatch;
            static bool Digits(string x) => x.Length > 0 && x.All(ch => ch >= '0' && ch <= '9');
            if (!Digits(want[i]) || !Digits(have[i])) return VerMatch.Unknown;
            if (long.Parse(want[i]) != long.Parse(have[i])) return VerMatch.Mismatch;
        }
        return VerMatch.Match;
    }

    // ---------- saves ----------
    public sealed class SaveScan { public List<string> Entries = new(); public int Files; public long Bytes; public string Folder = ""; }

    static void CountFiles(string dir, ref int files, ref long bytes)
    {
        var stack = new Stack<string>(); stack.Push(dir);
        while (stack.Count > 0)
        {
            string d = stack.Pop();
            foreach (var e in Util.ListDir(d))
            {
                if ((e.Attributes & FileAttributes.ReparsePoint) != 0) continue;
                if (e is FileInfo fi) { files++; try { bytes += fi.Length; } catch { } }
                else if (e is DirectoryInfo) stack.Push(e.FullName);
            }
        }
    }

    /// <summary>Everything inside "save games" of the CK3 user folder (the folder itself is kept). Links are never followed.</summary>
    public static SaveScan ScanSaves(string ck3Dir)
    {
        var r = new SaveScan();
        if (ck3Dir.Length == 0) return r;
        r.Folder = Path.Combine(ck3Dir, "save games");
        if (!Util.DirExists(r.Folder)) return r;
        foreach (var e in Util.ListDir(r.Folder))
        {
            r.Entries.Add(e.FullName);
            if (e is FileInfo fi) { r.Files++; try { r.Bytes += fi.Length; } catch { } }
            else if (e is DirectoryInfo && (e.Attributes & FileAttributes.ReparsePoint) == 0) CountFiles(e.FullName, ref r.Files, ref r.Bytes);
        }
        return r;
    }

    // ---------- shader cache ----------
    public sealed class ShaderScan { public List<string> Folders = new(), Entries = new(); public int Files; public long Bytes; }
    /// <summary>The game keeps a shader cache in a folder of its user folder whose name contains "shader". Only such folders directly inside the CK3 user folder are looked at; links are never followed.</summary>
    public static ShaderScan ScanShaderCache(string ck3Dir)
    {
        var r = new ShaderScan();
        if (ck3Dir.Length == 0 || !Util.DirExists(ck3Dir)) return r;
        foreach (var d in Util.ListDir(ck3Dir))
        {
            if (d is not DirectoryInfo || (d.Attributes & FileAttributes.ReparsePoint) != 0) continue;
            if (!d.Name.ToLowerInvariant().Contains("shader")) continue;
            r.Folders.Add(d.FullName);
            foreach (var e in Util.ListDir(d.FullName))
            {
                r.Entries.Add(e.FullName);
                if (e is FileInfo fi) { r.Files++; try { r.Bytes += fi.Length; } catch { } }
                else if (e is DirectoryInfo && (e.Attributes & FileAttributes.ReparsePoint) == 0) CountFiles(e.FullName, ref r.Files, ref r.Bytes);
            }
        }
        return r;
    }
    /// <summary>Removes the contents found by ScanShaderCache; returns how many entries could not be removed. A link anywhere inside an entry makes the whole entry be skipped.</summary>
    public static int ClearShaderCache(ShaderScan sc)
    {
        int failed = 0;
        foreach (var e in sc.Entries)
        {
            try
            {
                if (Directory.Exists(e))
                {
                    var di = new DirectoryInfo(e);
                    if ((di.Attributes & FileAttributes.ReparsePoint) != 0) di.Delete();
                    else if (Safe.HasLinkInside(e)) failed++;
                    else Directory.Delete(e, true);
                }
                else File.Delete(e);
            }
            catch { failed++; }
        }
        return failed;
    }
}

public static class Safe
{
    /// <summary>True when anything inside the folder is a link or junction (those could point outside it, so a recursive delete must not touch such a folder), or it cannot be read.</summary>
    public static bool HasLinkInside(string root)
    {
        var todo = new Stack<string>(); todo.Push(root);
        while (todo.Count > 0)
        {
            string d = todo.Pop();
            try
            {
                foreach (var e in new DirectoryInfo(d).EnumerateFileSystemInfos())
                {
                    if ((e.Attributes & FileAttributes.ReparsePoint) != 0) return true;
                    if (e is DirectoryInfo) todo.Push(e.FullName);
                }
            }
            catch { return true; }
        }
        return false;
    }
}
