using System.Security.Cryptography;
using System.Text;
namespace RoyalCourt.Core;

public sealed class ApplyResult { public bool Ok; public string Message = ""; public int Written, Skipped; }

public static class Steam
{
    /// <summary>Steam's library files are simple text ("key" "value" pairs, nested in braces).</summary>
    public static List<string> VdfValues(string text, string key)
    {
        var tokens = new List<string>(); var outv = new List<string>();
        int i = 0;
        while (i < text.Length)
        {
            if (text[i] == '"')
            {
                var v = new StringBuilder();
                i++;
                while (i < text.Length && text[i] != '"')
                {
                    if (text[i] == '\\' && i + 1 < text.Length) { v.Append(text[i + 1]); i += 2; }
                    else v.Append(text[i++]);
                }
                i++;
                tokens.Add(v.ToString());
            }
            else i++;
        }
        for (int k = 0; k + 1 < tokens.Count; k++) if (tokens[k] == key) outv.Add(tokens[k + 1]);
        return outv;
    }
    public static List<string> LibraryPaths(string libraryfoldersVdf) => VdfValues(libraryfoldersVdf, "path");
    public static string AcfInstallDir(string acf)
    {
        var v = VdfValues(acf, "installdir");
        return v.Count == 0 || v[0].Length == 0 ? "Crusader Kings III" : v[0];
    }

    /// <summary>Workshop downloads that are still waiting for the launcher to write their ugc_ descriptor.</summary>
    public static List<ModInfo> ScanWorkshopFolders(IEnumerable<string> contentDirs, ISet<string> knownIds)
    {
        var outv = new List<ModInfo>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var cd in contentDirs)
        {
            if (!Util.DirExists(cd)) continue;
            foreach (var e in Util.ListDir(cd))
            {
                if (e is not DirectoryInfo) continue;
                string num = e.Name;
                if (num.Length == 0 || num.Length > 20 || !num.All(c => c >= '0' && c <= '9')) continue;
                string id = "ugc_" + num + ".mod";
                if (knownIds.Contains(id) || !seen.Add(id)) continue;
                if (!Util.ReadFile(Path.Combine(e.FullName, "descriptor.mod"), out var text, 1 << 20)) continue;
                var kv = ModScan.ParseDescriptor(text);
                string First(string k) => kv.TryGetValue(k, out var v) && v.Count > 0 ? v[0] : "";
                var mi = new ModInfo { Id = id, Source = "Workshop", Pending = true, Name = First("name"), Version = First("version"), Supported = First("supported_version") };
                if (kv.TryGetValue("dependencies", out var d)) mi.Deps = new List<string>(d);
                if (kv.TryGetValue("replace_path", out var rp)) mi.ReplacePaths = new List<string>(rp);
                if (kv.TryGetValue("tags", out var tg)) mi.Tags = new List<string>(tg);
                mi.Path = mi.ContentDir = e.FullName;
                mi.ContentState = 1;
                if (mi.Name.Length == 0) mi.Name = num;
                outv.Add(mi);
            }
        }
        return outv;
    }

    /// <summary>The text of the ugc_ descriptor the launcher would write: the mod's own descriptor.mod plus remote_file_id and path.</summary>
    public static string RegisteredDescriptorText(string descriptorText, string workshopId, string folder)
    {
        var o = new StringBuilder();
        var lines = descriptorText.Split('\n');
        int count = lines.Length;
        if (count > 0 && lines[count - 1].Length == 0) count--;     // std::getline gives no empty last line
        bool skipping = false;
        for (int li = 0; li < count; li++)
        {
            string line = lines[li];
            if (line.Length > 0 && line[^1] == '\r') line = line[..^1];
            string t = Util.Trimmed(line);
            if (skipping) { if (t.Contains('}')) skipping = false; continue; }
            if (t.StartsWith("path", StringComparison.Ordinal) || t.StartsWith("remote_file_id", StringComparison.Ordinal))
            {
                if (t.Contains('{') && !t.Contains('}')) skipping = true;
                continue;
            }
            o.Append(line).Append('\n');
        }
        string f = folder.Replace('\\', '/');
        return o + "remote_file_id=\"" + workshopId + "\"\npath=\"" + f + "\"\n";
    }

    /// <summary>Writes mod/ugc_(id).mod for the enabled mods of the playset that only exist as Workshop downloads. Never overwrites an existing file.</summary>
    public static int RegisterPendingMods(string dir, Playset ps, IReadOnlyDictionary<string, ModInfo> info, out string err)
    {
        err = "";
        int n = 0;
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled) continue;
            if (!info.TryGetValue(m.Id, out var mi) || !mi.Pending || !Playsets.ValidID(m.Id)) continue;
            string target = Path.Combine(dir, "mod", m.Id);
            if (File.Exists(target) || Directory.Exists(target)) continue;
            if (!Util.ReadFile(Path.Combine(mi.ContentDir, "descriptor.mod"), out var text, 1 << 20)) { err = Trf("Could not read the downloaded mod \"{0}\".", mi.Name); continue; }
            string wid = m.Id.Substring(4, m.Id.Length - 8);
            if (Util.WriteFileAtomic(target, RegisteredDescriptorText(text, wid, mi.ContentDir))) n++;
            else err = Trf("Could not write {0} into your mod folder.", m.Id);
        }
        return n;
    }

    /// <summary>The game (when started directly, without the launcher) reads which mods to load from dlc_load.json in the CK3 user folder.</summary>
    public static ApplyResult WriteGameModList(string dir, Playset ps, ISet<string> installed)
    {
        var r = new ApplyResult();
        if (dir.Length == 0) { r.Message = Tr("The CK3 folder is not set."); return r; }
        string path = Path.Combine(dir, "dlc_load.json");
        var doc = J.Obj();
        if (File.Exists(path))
        {
            if (!Util.ReadFile(path, out var old)) { r.Message = Tr("Could not read your existing dlc_load.json (it may be in use or unusually large), so it was left unchanged."); return r; }
            if (J.TryParse(old, out var parsed) && parsed.IsObj) doc = parsed;
            string keep = Path.Combine(dir, "dlc_load.json.rc-original");
            if (!File.Exists(keep) && !Util.WriteFile(keep, old)) { r.Message = Tr("Could not save a copy of your dlc_load.json, so nothing was changed."); return r; }
        }
        if (doc.Get("disabled_dlcs") == null) doc.Set("disabled_dlcs", J.Arr());
        var list = J.Arr();
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled) continue;
            if (!Playsets.ValidID(m.Id) || !installed.Contains(m.Id)) { r.Skipped++; continue; }
            list.A.Add(J.Str("mod/" + m.Id));
            r.Written++;
        }
        doc.Set("enabled_mods", list);
        if (!Util.WriteFileAtomic(path, doc.Dump())) { r.Message = Tr("Could not write dlc_load.json (is the CK3 folder writable?)."); return r; }
        r.Ok = true;
        r.Message = r.Skipped > 0 ? Trf("Loaded \"{0}\": {1} mods ({2} not installed, skipped)", ps.Name, r.Written, r.Skipped)
                                  : Trf("Loaded \"{0}\": {1} mods", ps.Name, r.Written);
        return r;
    }
}

public static class Log
{
    public static string LogPath() => Path.Combine(Util.DataDir(), "royalcourt.log");
    /// <summary>A small text file next to settings.json (royalcourt.log, rotated at 256 KB).</summary>
    public static void Line(string msg)
    {
        try
        {
            string p = LogPath();
            var fi = new FileInfo(p);
            if (fi.Exists && fi.Length > 256 * 1024) { string old = p + ".1"; try { File.Delete(old); } catch { } try { File.Move(p, old); } catch { } }
            Util.CreateDirs(Path.GetDirectoryName(p) ?? ".");
            File.AppendAllText(p, DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "  " + msg + "\n", Util.Utf8);
        }
        catch { }
    }
}

public sealed class ReleaseAsset { public string Name = "", Url = ""; public long Size; }
public sealed class ReleaseInfo
{
    public bool Ok; public string Tag = "", Name = "", Body = "", PageUrl = "";
    public List<ReleaseAsset> Assets = new();
    public ReleaseAsset? Asset(string n) { foreach (var a in Assets) if (a.Name == n) return a; return null; }
}

public static class Release
{
    public const string UpdateRepo = "Juincy/the-royal-court", UpdateExeAsset = "TheRoyalCourt.exe", UpdateSumsAsset = "SHA256SUMS.txt";

    public static string Sha256Hex(byte[] data) => Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
    public static string Sha256Hex(string data) => Sha256Hex(Encoding.UTF8.GetBytes(data));

    /// <summary>"v0.14.1" / "0.14" -> {0,14,1}; anything after the first non-numeric part is ignored</summary>
    public static List<int> ParseVersion(string s)
    {
        var v = new List<int>();
        int i = 0;
        while (i < s.Length && (s[i] == 'v' || s[i] == 'V' || s[i] == ' ')) i++;
        int cur = 0; bool any = false;
        for (; i < s.Length; i++)
        {
            if (s[i] >= '0' && s[i] <= '9') { cur = Math.Min(cur * 10 + (s[i] - '0'), 1000000); any = true; }
            else if (s[i] == '.' && any) { v.Add(cur); cur = 0; any = false; }
            else break;
        }
        if (any) v.Add(cur);
        return v;
    }
    /// <summary>&lt;0 older, 0 same, &gt;0 newer (a compared with b)</summary>
    public static int CompareVersions(string a, string b)
    {
        var x = ParseVersion(a); var y = ParseVersion(b);
        int n = Math.Max(x.Count, y.Count);
        for (int i = 0; i < n; i++)
        {
            int p = i < x.Count ? x[i] : 0, q = i < y.Count ? y[i] : 0;
            if (p != q) return p < q ? -1 : 1;
        }
        return 0;
    }
    /// <summary>Only downloads from this project's own GitHub releases are ever followed.</summary>
    public static bool TrustedUrl(string url)
    {
        string pre = Util.Lower("https://github.com/" + UpdateRepo + "/releases/");
        return Util.Lower(url).StartsWith(pre, StringComparison.Ordinal);
    }
    public static ReleaseInfo Parse(string json)
    {
        var r = new ReleaseInfo();
        if (!J.TryParse(json, out var root) || !root.IsObj) return r;
        static string Str(J o, string k) => o.Get(k) is { IsStr: true } v ? v.S : "";
        r.Tag = Str(root, "tag_name"); r.Name = Str(root, "name"); r.Body = Str(root, "body"); r.PageUrl = Str(root, "html_url");
        if (r.Tag.Length == 0 || ParseVersion(r.Tag).Count == 0) return r;
        if (r.PageUrl.Length > 0 && !TrustedUrl(r.PageUrl)) r.PageUrl = "";
        if (root.Get("assets") is { IsArr: true } assets)
            foreach (var a in assets.A)
            {
                if (!a.IsObj) continue;
                var x = new ReleaseAsset { Name = Str(a, "name"), Url = Str(a, "browser_download_url") };
                if (a.Get("size") is { IsNum: true } sz) x.Size = (long)sz.N;
                if (x.Name.Length > 0 && TrustedUrl(x.Url)) r.Assets.Add(x);
            }
        r.Ok = true;
        return r;
    }
    /// <summary>SHA256SUMS.txt lines look like "(64 hex)  name" or "(64 hex) *name"; returns "" when not listed</summary>
    public static string FindSumFor(string sums, string file)
    {
        foreach (var raw in sums.Split('\n'))
        {
            string line = raw.Length > 0 && raw[^1] == '\r' ? raw[..^1] : raw;
            if (line.Length < 66) continue;
            string hash = Util.Lower(line[..64]);
            if (!hash.All(c => (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) continue;
            int p = 64;
            while (p < line.Length && (line[p] == ' ' || line[p] == '*' || line[p] == '\t')) p++;
            if (line[p..] == file) return hash;
        }
        return "";
    }

    /// <summary>Plain text a user can paste into a GitHub issue: the current load order with each mod's type. Only mod names, Workshop ids and flags are included.</summary>
    public static string BuildSortReport(Playset ps, IReadOnlyDictionary<string, ModInfo> info, IReadOnlyList<KnownMod> db,
                                         IReadOnlyList<int> cats, ISet<string> locked, string gameVer, int onlineRevision)
    {
        var o = new StringBuilder();
        o.Append("The Royal Court sort report\n");
        o.Append("Program ").Append(Env.Version).Append(", Crusader Kings III ").Append(gameVer.Length == 0 ? "unknown" : gameVer)
         .Append(", known-mods list: built-in").Append(onlineRevision > 0 ? " + online revision " + onlineRevision : "").Append('\n');
        o.Append("Playset: ").Append(ps.Name).Append(" (").Append(ps.Mods.Count).Append(" mods)\n\n");
        o.Append("Load order, first line loads first:\n");
        int n = 0;
        for (int i = 0; i < ps.Mods.Count && n < 300; i++, n++)
        {
            var m = ps.Mods[i];
            bool has = info.TryGetValue(m.Id, out var mi);
            string name = has && mi!.Name.Length > 0 ? mi.Name : (m.Name.Length == 0 ? m.Id : m.Name);
            o.Append(i + 1).Append(". ").Append(m.Enabled ? "[on] " : "[off] ").Append(name);
            string sid = Names.SteamIdOf(m.Id);
            if (sid.Length > 0) o.Append(" (Steam ").Append(sid).Append(')');
            if (i < cats.Count) o.Append(" | type ").Append(Cat.NameEn(cats[i]));
            if (locked.Contains(m.Id)) o.Append(" | locked");
            if (has && Known.Find(db, mi!) >= 0) o.Append(" | known mod");
            if (!has) o.Append(" | not installed");
            o.Append('\n');
        }
        if (ps.Mods.Count > n) o.Append("... and ").Append(ps.Mods.Count - n).Append(" more\n");
        o.Append("\nWhat I expected (which mod should be where, and why):\n\n");
        return o.ToString();
    }
}
