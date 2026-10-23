using System.Globalization;
using System.Text;
namespace RoyalCourt.Core;

public sealed class ImportResult { public bool Ok; public string Error = ""; public Playset Playset = new(); public int Invalid, Unmatched; }

public static class Playsets
{
    public const string LocalPrefix = "local:";

    public static bool ValidID(string id)
    {
        if (id.Length < 5 || Util.Lower(id[^4..]) != ".mod") return false;
        return id.IndexOf('/') < 0 && id.IndexOf('\\') < 0 && !id.Contains("..", StringComparison.Ordinal);
    }

    /// <summary>Writes enabled mods in load order, plus disabled mods that are not installed. Output is compact JSON identical to the launcher's own files.</summary>
    public static string ExportLauncher(Playset ps, IReadOnlyDictionary<string, ModInfo> info, string game = "ck3")
    {
        var root = J.Obj();
        root.Set("game", J.Str(game));
        root.Set("name", J.Str(ps.Name));
        var ms = J.Arr();
        int pos = 0;
        foreach (var m in ps.Mods)
        {
            bool installed = info.TryGetValue(m.Id, out var mi);
            if (!m.Enabled && installed) continue;
            var mo = J.Obj();
            string nm = installed ? mi!.Name : m.Name;
            if (nm.Length == 0) nm = m.Id;
            mo.Set("displayName", J.Str(nm));
            mo.Set("enabled", J.Bool(m.Enabled));
            mo.Set("position", J.Num(pos++));
            string sid = Names.SteamIdOf(m.Id);
            if (sid.Length > 0) mo.Set("steamId", J.Str(sid));
            else if (ValidID(m.Id)) mo.Set("gameRegistryId", J.Str("mod/" + m.Id));
            ms.A.Add(mo);
        }
        root.Set("mods", ms);
        return root.DumpMin();
    }

    static string ScalarText(J? v)
    {
        if (v == null) return "";
        if (v.IsStr) return v.S;
        if (v.IsNum) return v.N.ToString("F0", CultureInfo.InvariantCulture);
        return "";
    }
    static bool AllDigits(string s) { foreach (char c in s) if (c < '0' || c > '9') return false; return true; }

    public static ImportResult Parse(string text, IReadOnlyList<ModInfo> installed)
    {
        var r = new ImportResult();
        if (Util.Utf8.GetByteCount(text) > (5 << 20)) { r.Error = Tr("That file is too large to be a playset."); return r; }
        if (!J.TryParse(text, out var root) || !root.IsObj) { r.Error = Tr("That file is not valid JSON."); return r; }
        if (root.Get("game") is { IsStr: true } g && g.S.Length > 0 && Util.Lower(g.S) != "ck3")
        {
            r.Error = Trf("That playset is for \"{0}\", not Crusader Kings III.", g.S);
            return r;
        }
        var ms = root.Get("mods");
        if (ms == null || !ms.IsArr) { r.Error = Tr("That file is not a playset (no mod list found)."); return r; }
        if (ms.A.Count > 5000) { r.Error = Tr("The playset file lists too many mods."); return r; }
        if (root.Get("name") is { IsStr: true } nn) r.Playset.Name = nn.S.Length > 100 ? nn.S[..100] : nn.S;

        var es = new List<(long pos, int order, ModRef m)>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        int order = 0;
        Dictionary<string, string>? byName = null;
        foreach (var mo in ms.A)
        {
            if (!mo.IsObj) { r.Invalid++; continue; }
            string display = ScalarText(mo.Get("displayName"));
            string steam = ScalarText(mo.Get("steamId"));
            string reg = ScalarText(mo.Get("gameRegistryId")).Replace('\\', '/');
            if (reg.StartsWith("mod/", StringComparison.Ordinal)) reg = reg[4..];
            string id = "";
            if (reg.Length > 0 && ValidID(reg)) id = reg;
            else if (steam.Length > 0 && AllDigits(steam)) id = "ugc_" + steam + ".mod";
            else if (display.Length > 0)
            {
                if (byName == null)
                {
                    byName = new Dictionary<string, string>(StringComparer.Ordinal);
                    foreach (var m in installed) byName.TryAdd(Util.Lower(m.Name), m.Id);
                }
                if (byName.TryGetValue(Util.Lower(display), out var f)) id = f;
                if (id.Length == 0) { id = LocalPrefix + display; r.Unmatched++; }
            }
            if (id.Length == 0) { r.Invalid++; continue; }
            if (!seen.Add(id))
            {
                if (mo.Get("enabled") is { IsBool: true, B: true }) for (int x = 0; x < es.Count; x++) if (es[x].m.Id == id) es[x].m.Enabled = true;
                continue;
            }
            var mr = new ModRef { Id = id, Name = display, Enabled = true };
            if (mo.Get("enabled") is { IsBool: true } e) mr.Enabled = e.B;
            long pos = 1L << 30;
            string ps = ScalarText(mo.Get("position"));
            if (ps.Length > 0 && AllDigits(ps) && ps.Length < 9) pos = long.Parse(ps, CultureInfo.InvariantCulture);
            es.Add((pos, order++, mr));
        }
        foreach (var e in es.OrderBy(x => x.pos).ThenBy(x => x.order)) r.Playset.Mods.Add(e.m);
        r.Ok = true;
        return r;
    }

    /// <summary>Ties placeholder entries to installed mods (by name), refreshes names, and lists every other installed mod as disabled.</summary>
    public static void Sync(Playset ps, IReadOnlyList<ModInfo> installed)
    {
        var byId = new Dictionary<string, ModInfo>(StringComparer.Ordinal);
        var byName = new Dictionary<string, ModInfo>(StringComparer.Ordinal);
        foreach (var m in installed) { byId[m.Id] = m; byName.TryAdd(Util.Lower(m.Name), m); }
        var have = new HashSet<string>(StringComparer.Ordinal);
        var outv = new List<ModRef>();
        foreach (var src in ps.Mods)
        {
            var m = new ModRef(src.Id, src.Enabled, src.Name);
            if (m.Id.StartsWith(LocalPrefix, StringComparison.Ordinal) && byName.TryGetValue(Util.Lower(m.Name), out var bn)) m.Id = bn.Id;
            if (byId.TryGetValue(m.Id, out var bi)) m.Name = bi.Name;
            if (!have.Add(m.Id)) continue;
            outv.Add(m);
        }
        foreach (var m in installed) if (!have.Contains(m.Id)) outv.Add(new ModRef(m.Id, false, m.Name));
        ps.Mods = outv;
    }

    public static Dictionary<string, ModInfo> InfoMap(IEnumerable<ModInfo> mods)
    {
        var info = new Dictionary<string, ModInfo>(StringComparer.Ordinal);
        foreach (var m in mods) info[m.Id] = m;
        return info;
    }

    public static string PlaysetPath(string name) => Path.Combine(Util.PlaysetsDir(), Util.SanitizeFileName(name) + ".json");

    static bool Same(string a, string b)
    {
        try { return string.Equals(Path.GetFullPath(a), Path.GetFullPath(b), OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal); }
        catch { return false; }
    }

    /// <summary>True if name would collide with another playset (also checks the real folder: Windows treats names that differ only by capitals as the same file).</summary>
    public static bool NameTaken(IEnumerable<Playset> all, string name, string except = "")
    {
        string n = Util.SanitizeFileName(name);
        foreach (var p in all)
        {
            if (except.Length > 0 && p.Name == except) continue;
            if (Util.Lower(p.Name) == Util.Lower(n)) return true;
        }
        string np = PlaysetPath(n);
        if (!Util.FileExists(np)) return false;
        if (except.Length > 0)
        {
            string ep = PlaysetPath(except);
            if (Util.FileExists(ep) && Same(np, ep)) return false;
        }
        return true;
    }
    public static string UniqueName(IReadOnlyCollection<Playset> all, string wanted)
    {
        string baseName = Util.SanitizeFileName(wanted.Length == 0 ? "Imported playset" : wanted);
        if (!NameTaken(all, baseName)) return baseName;
        for (int i = 2; i < 10000; i++)
        {
            string c = baseName + " (" + i + ")";
            if (!NameTaken(all, c)) return c;
        }
        return baseName + " (new)";
    }

    public static bool Save(Playset ps, IReadOnlyDictionary<string, ModInfo> info) => Util.WriteFileAtomic(PlaysetPath(ps.Name), ExportLauncher(ps, info));

    // ---------- backups ----------
    public static string BackupsDir(string playsetName)
    {
        string d = Path.Combine(Util.DataDir(), "Backups", Util.SanitizeFileName(playsetName));
        Util.CreateDirs(d);
        return d;
    }
    public static string StampNow() => DateTime.Now.ToString("yyyy-MM-dd HH-mm-ss", CultureInfo.InvariantCulture);

    /// <summary>Newest first.</summary>
    public static List<string> ListBackups(string playsetName)
    {
        var v = new List<string>();
        string d = Path.Combine(Util.DataDir(), "Backups", Util.SanitizeFileName(playsetName));
        if (!Util.DirExists(d)) return v;
        foreach (var e in Util.ListDir(d)) if (e is FileInfo && Util.Lower(e.Extension) == ".json") v.Add(e.FullName);
        v.Sort((a, b) => string.CompareOrdinal(Path.GetFileName(b), Path.GetFileName(a)));
        return v;
    }

    public static string Backup(Playset ps, IReadOnlyDictionary<string, ModInfo> info, string reason, int keep = 30)
    {
        string data = ExportLauncher(ps, info);
        var existing = ListBackups(ps.Name);
        if (existing.Count > 0 && Util.ReadFile(existing[0], out var last) && last == data) return "";
        string why = Util.SanitizeFileName(reason.Length == 0 ? "backup" : reason);
        string stamp = StampNow();
        int seq = 0;
        foreach (var e in existing)
        {
            string nm = Path.GetFileName(e);
            if (!nm.StartsWith(stamp, StringComparison.Ordinal)) continue;
            int k = 1;
            if (nm.Length > stamp.Length + 3 && nm[stamp.Length] == '-' && int.TryParse(nm.AsSpan(stamp.Length + 1, 2), NumberStyles.None, CultureInfo.InvariantCulture, out int kk)) k = kk;
            seq = Math.Max(seq, k);
        }
        string suf = seq > 0 ? "-" + (seq + 1).ToString("00", CultureInfo.InvariantCulture) : "";
        string f = Path.Combine(BackupsDir(ps.Name), stamp + suf + " " + why + ".json");
        if (!Util.WriteFileAtomic(f, data)) return "";
        existing = ListBackups(ps.Name);
        for (int i = keep; i < existing.Count; i++) { try { File.Delete(existing[i]); } catch { } }
        return f;
    }

    /// <summary>Puts the mods of from (a backup) into to: backup order and enabled state first, then any mods the backup did not know about.</summary>
    public static void ApplyBackupOrder(Playset to, Playset from)
    {
        var cur = new HashSet<string>(StringComparer.Ordinal);
        foreach (var m in to.Mods) cur.Add(m.Id);
        var outv = new List<ModRef>(); var used = new HashSet<string>(StringComparer.Ordinal);
        foreach (var m in from.Mods) if (cur.Contains(m.Id) && used.Add(m.Id)) outv.Add(new ModRef(m.Id, m.Enabled, m.Name));
        foreach (var m in to.Mods) if (!used.Contains(m.Id)) outv.Add(m);
        to.Mods = outv;
    }

    public static bool Delete(string name)
    {
        try { string p = PlaysetPath(name); if (!File.Exists(p)) return false; File.Delete(p); return true; } catch { return false; }
    }

    /// <summary>Renames a playset: the file gets the new name and the name written inside it matches.</summary>
    public static bool Rename(Playset ps, string newName, IReadOnlyDictionary<string, ModInfo> info)
    {
        string old = ps.Name;
        string op = PlaysetPath(old), np = PlaysetPath(newName);
        bool differs = !string.Equals(op, np, StringComparison.Ordinal);
        bool same = differs && File.Exists(op) && File.Exists(np) && Same(op, np);
        if (same)
        {
            string mid = op + ".rename.tmp";
            try { File.Move(op, mid); } catch { return false; }
            try { File.Move(mid, np); } catch { try { File.Move(mid, op); } catch { } return false; }
        }
        ps.Name = newName;
        if (!Save(ps, info)) { ps.Name = old; return false; }
        if (!same && differs) Delete(old);
        string ob = Path.Combine(Util.DataDir(), "Backups", Util.SanitizeFileName(old)), nb = Path.Combine(Util.DataDir(), "Backups", Util.SanitizeFileName(newName));
        if (ob != nb && Util.DirExists(ob))
        {
            try
            {
                if (!Directory.Exists(nb)) Directory.Move(ob, nb);
                else
                {
                    Directory.CreateDirectory(nb);
                    foreach (var f in Util.ListDir(ob)) { string t = Path.Combine(nb, f.Name); if (!File.Exists(t) && f is FileInfo) try { File.Move(f.FullName, t); } catch { } }
                    Directory.Delete(ob, true);
                }
            }
            catch { }
        }
        return true;
    }

    public static List<Playset> LoadAll(IReadOnlyList<ModInfo> installed)
    {
        var outv = new List<Playset>();
        foreach (var e in Util.ListDir(Util.PlaysetsDir()))
        {
            if (e is not FileInfo || Util.Lower(e.Extension) != ".json") continue;
            if (!Util.ReadFile(e.FullName, out var text, 5 << 20)) continue;
            var r = Parse(text, installed);
            if (!r.Ok) continue;
            var p = r.Playset;
            p.Name = Path.GetFileNameWithoutExtension(e.Name);
            string clean = Util.SanitizeFileName(p.Name);
            if (clean != p.Name && clean.Length > 0)
            {
                string target = PlaysetPath(clean);
                if (!File.Exists(target)) { try { File.Move(e.FullName, target); p.Name = clean; } catch { } }
                else
                {
                    for (int k = 2; k < 100; k++)
                    {
                        string alt = (clean.Length > 70 ? clean[..70] : clean) + " (" + k + ")";
                        string t2 = PlaysetPath(alt);
                        if (File.Exists(t2)) continue;
                        try { File.Move(e.FullName, t2); p.Name = alt; } catch { }
                        break;
                    }
                }
            }
            Sync(p, installed);
            outv.Add(p);
        }
        return outv.OrderBy(p => Util.Lower(p.Name), StringComparer.Ordinal).ToList();
    }

    /// <summary>One-time upgrade from the old app database (playsets.json, v0.1-0.3). Returns the number of playsets converted.</summary>
    public static int MigrateLegacyStore(IReadOnlyList<ModInfo> installed, Settings settings)
    {
        string old = Path.Combine(Util.DataDir(), "playsets.json");
        if (!File.Exists(old)) return 0;
        foreach (var e in Util.ListDir(Util.PlaysetsDir())) if (Util.Lower(e.Extension) == ".json") return 0;
        if (!Util.ReadFile(old, out var text) || !J.TryParse(text, out var root) || !root.IsObj) return 0;
        var info = InfoMap(installed);
        var done = new List<Playset>();
        if (root.Get("playsets") is { IsArr: true } pl)
        {
            foreach (var po in pl.A)
            {
                if (!po.IsObj) continue;
                var p = new Playset();
                string nm = po.Get("name") is { IsStr: true } n ? n.S : "";
                p.Name = UniqueName(done, nm);
                if (po.Get("mods") is { IsArr: true } ms)
                {
                    foreach (var mo in ms.A)
                    {
                        if (!mo.IsObj) continue;
                        var m = new ModRef();
                        if (mo.Get("id") is { IsStr: true } i) m.Id = i.S;
                        if (mo.Get("enabled") is { IsBool: true } e) m.Enabled = e.B;
                        if (mo.Get("name") is { IsStr: true } n2) m.Name = n2.S;
                        if (m.Id.Length > 0) p.Mods.Add(m);
                    }
                }
                if (Save(p, info)) done.Add(p);
            }
        }
        if (settings.Ck3Dir.Length == 0 && root.Get("ck3Dir") is { IsStr: true } d) settings.Ck3Dir = d.S;
        if (root.Get("active") is { IsStr: true } a) settings.Active = Util.SanitizeFileName(a.S);
        try { File.Move(old, old + ".migrated"); } catch { }
        return done.Count;
    }
}
