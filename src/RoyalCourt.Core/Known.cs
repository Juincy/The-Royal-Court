using System.Text;
namespace RoyalCourt.Core;

public static class Cat
{
    public const int Library = 0, Overhaul = 1, Content = 2, Graphics = 3, Ui = 4, Translation = 5, Patch = 6, Count = 7;
    static readonly string[] En = { K("Library"), K("Overhaul"), K("Content"), K("Graphics"), K("Interface"), K("Translation"), K("Patch") };
    /// <summary>English name (for knownmods.json "type" values and the English sort report).</summary>
    public static string NameEn(int c) => c >= 0 && c < Count ? En[c] : "Content";
    public static string Name(int c) => Tr(NameEn(c));
    public static string SourceLabel(string s) => s == "Workshop" ? Tr("Workshop") : s == "Local" ? Tr("Local") : s;
    public static int FromName(string s)
    {
        string l = Util.Lower(s);
        for (int c = 0; c < Count; c++) if (l == Util.Lower(NameEn(c))) return c;
        return -1;
    }
}

public static class Names
{
    /// <summary>Whole-word match inside a lower-case string; prefix=true also accepts longer words.</summary>
    public static bool HasWord(string hay, string w, bool prefix = false)
    {
        static bool Alnum(char c) => (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        int pos = 0;
        while ((pos = hay.IndexOf(w, pos, StringComparison.Ordinal)) >= 0)
        {
            bool left = pos == 0 || !Alnum(hay[pos - 1]);
            bool right = pos + w.Length >= hay.Length || !Alnum(hay[pos + w.Length]) || prefix;
            if (left && right) return true;
            pos++;
        }
        return false;
    }

    /// <summary>Mod names as people write them differ ("Rise and Fall 1.17", "Rise &amp; Fall [1.17]"): lower case, "&amp;" -> "and", no brackets, punctuation as spaces, trailing version numbers removed.</summary>
    public static string Norm(string input)
    {
        var t = new StringBuilder();
        int depth = 0;
        foreach (char c in input)
        {
            if (c == '[' || c == '(') { depth++; t.Append(' '); continue; }
            if (c == ']' || c == ')') { if (depth > 0) depth--; t.Append(' '); continue; }
            if (depth > 0) continue;
            if (c == '&') { t.Append(" and "); continue; }
            if (c == '+') { t.Append(" plus "); continue; }
            char l = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
            if (l >= 0x80) l = char.ToLowerInvariant(l);
            t.Append(((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l >= 0x80) ? l : (l == '.' ? '.' : ' '));
        }
        var w = new List<string>();
        foreach (var part in t.ToString().Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            string x = part.Trim('.');
            if (x.Length > 0) w.Add(x);
        }
        static bool IsVersion(string x)
        {
            int i = (x.Length > 1 && x[0] == 'v') ? 1 : 0;
            if (i >= x.Length) return false;
            bool digit = false;
            for (; i < x.Length; i++) { if (x[i] >= '0' && x[i] <= '9') digit = true; else if (x[i] != '.') return false; }
            return digit;
        }
        while (w.Count > 1 && IsVersion(w[^1])) w.RemoveAt(w.Count - 1);
        return string.Join(' ', w);
    }

    static readonly string[] Bad = { "patch", "patches", "compat", "compatch", "compatibility", "translation", "submod", "addon", "for", "plus", "extension", "expansion", "bridge", "of", "in", "and", "with" };
    static int Spaces(string s) { int n = 0; foreach (char c in s) if (c == ' ') n++; return n; }

    /// <summary>Does a (normalised) mod name stand for the known name pat? Exact, or pat plus up to two extra words that do not say it is some other kind of mod.</summary>
    public static bool NameIs(string norm, string pat)
    {
        if (norm == pat) return true;
        if (norm.Length <= pat.Length + 1 || !norm.StartsWith(pat, StringComparison.Ordinal) || norm[pat.Length] != ' ') return false;
        if (Spaces(pat) < 2 || pat.Length < 10) return false;
        string rest = norm.Substring(pat.Length + 1);
        if (Spaces(rest) > 1) return false;
        foreach (var bad in Bad) if (HasWord(rest, bad, bad == "compat")) return false;
        return true;
    }

    public static string Acronym(string norm)
    {
        var a = new StringBuilder(); int words = 0;
        foreach (var x in norm.Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            words++;
            if (x == "and" || x == "the" || x == "of" || x == "a" || x == "for") continue;
            a.Append(x[0]);
        }
        return words >= 3 ? a.ToString() : "";
    }

    /// <summary>"ugc_2216660858.mod" -> "2216660858" (Steam Workshop id), otherwise "".</summary>
    public static string SteamIdOf(string id)
    {
        if (id.Length < 9 || !id.StartsWith("ugc_", StringComparison.Ordinal) || Util.Lower(id[^4..]) != ".mod") return "";
        string d = id.Substring(4, id.Length - 8);
        foreach (char c in d) if (c < '0' || c > '9') return "";
        return d;
    }
}

public sealed class KnownMod
{
    public List<string> Names = new();   // normalised names ("rise and fall")
    public List<string> Ids = new();     // Steam Workshop ids
    public int Cat = -1;                 // -1 = keep the guessed type
    public int Pos;                      // -2 very first (total conversions), -1 first, +1 last in the whole load order
    public int Rank;                     // order inside its type (higher = later)
    public List<string> After = new(), Before = new();
    public string Note = "";
}

public sealed class CompatPatch
{
    public string Name, Id; public string[][] Parents;
    public CompatPatch(string name, string id, string[][] parents) { Name = name; Id = id; Parents = parents; }
}

public sealed class CompatHit { public int State; public string Patch = "", Id = ""; public List<string> Parents = new(); public int PatchPos = -1; }

public sealed class KnownOnlineInfo { public bool Ok; public int Revision, Count; }

public static class Known
{
    public static List<KnownMod> Builtin() => KnownData.Builtin();

    public static string OnlineFile() => Path.Combine(Util.DataDir(), "knownmods.online.json");

    /// <summary>Adds the entries of one list file to v. Returns false (and leaves v alone) when the text is not a usable list.</summary>
    public static bool ApplyJson(List<KnownMod> v, string text, out int added)
    {
        added = 0;
        if (!J.TryParse(text, out var root) || !root.IsObj) return false;
        var arr = root.Get("mods");
        if (arr == null || !arr.IsArr) return false;
        static List<string> Strs(J? a, bool norm)
        {
            var o = new List<string>();
            if (a != null && a.IsArr) foreach (var e in a.A) if (e.IsStr && e.S.Length > 0 && Encoding.UTF8.GetByteCount(e.S) < 300) o.Add(norm ? Names.Norm(e.S) : e.S);
            return o;
        }
        int n = 0;
        foreach (var e in arr.A)
        {
            if (!e.IsObj) continue;
            var k = new KnownMod
            {
                Names = Strs(e.Get("names"), true), Ids = Strs(e.Get("ids"), false),
                After = Strs(e.Get("after"), true), Before = Strs(e.Get("before"), true)
            };
            if (k.Names.Count == 0 && k.Ids.Count == 0) continue;
            if (e.Get("type") is { IsStr: true } t) k.Cat = Cat.FromName(t.S);
            if (e.Get("position") is { IsStr: true } p) { string pl = Util.Lower(p.S); k.Pos = pl == "top" ? -2 : pl == "first" ? -1 : pl == "last" ? 1 : 0; }
            if (e.Get("rank") is { IsNum: true } r) k.Rank = (int)Math.Max(-100.0, Math.Min(100.0, r.N));
            if (e.Get("note") is { IsStr: true } nt) k.Note = Util.Utf8.GetByteCount(nt.S) > 1000 ? Util.Utf8.GetString(Util.Utf8.GetBytes(nt.S), 0, 1000) : nt.S;
            v.RemoveAll(b =>
            {
                foreach (var nm in k.Names) foreach (var m in b.Names) if (nm == m) return true;
                foreach (var id in k.Ids) foreach (var m in b.Ids) if (id == m) return true;
                return false;
            });
            v.Add(k);
            n++;
        }
        added = n;
        return true;
    }

    public static KnownOnlineInfo CheckOnline(string text)
    {
        var r = new KnownOnlineInfo();
        if (text.Length == 0 || Util.Utf8.GetByteCount(text) > (512 << 10)) return r;
        if (!J.TryParse(text, out var root) || !root.IsObj) return r;
        if (root.Get("format") is not { IsNum: true } f || (int)f.N != 1) return r;
        if (root.Get("revision") is { IsNum: true } rv) r.Revision = (int)rv.N;
        var tmp = new List<KnownMod>();
        if (!ApplyJson(tmp, text, out r.Count) || r.Count > 3000) return r;
        r.Ok = true;
        return r;
    }

    public static List<KnownMod> Load(out int onlineRevision)
    {
        var v = Builtin();
        onlineRevision = 0;
        if (Util.ReadFile(OnlineFile(), out var text))
        {
            var oi = CheckOnline(text);
            if (oi.Ok && ApplyJson(v, text, out _)) onlineRevision = oi.Revision;
        }
        if (Util.ReadFile(Path.Combine(Util.DataDir(), "knownmods.json"), out text)) ApplyJson(v, text, out _);
        return v;
    }

    /// <summary>The note of a known-mod entry for display: built-in notes are translatable (K() keys); notes from knownmods.json are shown as written.</summary>
    public static string NoteText(KnownMod k) => k.Note.Length == 0 ? "" : Tr(k.Note);

    public static int Find(IReadOnlyList<KnownMod> db, ModInfo m)
    {
        string sid = Names.SteamIdOf(m.Id), nn = Names.Norm(m.Name);
        for (int i = db.Count; i-- > 0;)
        {
            foreach (var id in db[i].Ids) if (sid.Length > 0 && id == sid) return i;
            foreach (var n in db[i].Names) if (Names.NameIs(nn, n)) return i;
        }
        return -1;
    }

    // ---------- compatch finder ----------
    public static List<CompatHit> FindCompatches(Playset ps, IReadOnlyDictionary<string, ModInfo> info)
    {
        var outv = new List<CompatHit>();
        int n = ps.Mods.Count;
        var norm = new string[n];
        for (int i = 0; i < n; i++) norm[i] = info.TryGetValue(ps.Mods[i].Id, out var mi) ? Names.Norm(mi.Name) : "";
        foreach (var cp in KnownData.Patches)
        {
            var names = new List<string>();
            bool all = true;
            foreach (var group in cp.Parents)
            {
                int hit = -1;
                for (int i = 0; i < n && hit < 0; i++)
                {
                    if (!ps.Mods[i].Enabled || norm[i].Length == 0) continue;
                    foreach (var a in group) if (Names.NameIs(norm[i], Names.Norm(a))) { hit = i; break; }
                }
                if (hit < 0) { all = false; break; }
                names.Add(info[ps.Mods[hit].Id].Name);
            }
            if (!all) continue;
            string pn = Names.Norm(cp.Name);
            int on = -1, off = -1;
            for (int i = 0; i < n; i++)
            {
                if (norm[i].Length == 0) continue;
                bool same = Names.NameIs(norm[i], pn) || (cp.Id.Length > 0 && Names.SteamIdOf(ps.Mods[i].Id) == cp.Id);
                if (!same) continue;
                if (ps.Mods[i].Enabled) on = i; else if (off < 0) off = i;
            }
            if (on >= 0) continue;
            outv.Add(new CompatHit { State = off >= 0 ? 1 : 0, Patch = cp.Name, Id = cp.Id, Parents = names, PatchPos = off });
        }
        return outv;
    }

    public static string CompatText(IReadOnlyList<CompatHit> hits, string playsetName)
    {
        var t = new StringBuilder(Trf("Compatibility patches for \"{0}\"", playsetName) + "\n\n");
        if (hits.Count == 0) return t + Tr("No known compatibility patch is missing. (This only knows patches that name the mods they join.)") + "\n";
        foreach (var h in hits)
        {
            string who = string.Join(" + ", h.Parents);
            t.Append("- ").Append(h.Patch).Append("\n    ").Append(Trf("for: {0}", who)).Append("\n    ");
            if (h.State == 1) t.Append(Trf("It is in your list but turned off (#{0}): tick it so the two mods work together.", h.PatchPos + 1)).Append('\n');
            else if (h.Id.Length > 0) t.Append(Tr("Not subscribed. Workshop page:")).Append(" https://steamcommunity.com/sharedfiles/filedetails/?id=").Append(h.Id).Append('\n');
            else t.Append(Tr("Not subscribed. Search the Steam Workshop for its name.")).Append('\n');
        }
        t.Append('\n').Append(Tr("Check each page before you subscribe: a patch is only needed when you really use both mods together.")).Append('\n');
        return t.ToString();
    }

    // ---------- category guesses ----------
    public readonly record struct CatGuess(int Cat, string Why)
    {
        public string Shown => Why == "default" ? Tr("default") : Why;
    }

    public static CatGuess GuessCategory(ModInfo m)
    {
        string n = Util.Lower(m.Name);
        var tags = new HashSet<string>(StringComparer.Ordinal);
        foreach (var t in m.Tags) tags.Add(Util.Lower(Util.Trimmed(t)));
        string? Name(string[] ws, bool prefix = false) { foreach (var w in ws) if (Names.HasWord(n, w, prefix)) return w; return null; }
        string? w;
        if ((w = Name(new[] { "patch", "patches", "hotfix", "bridge", "submod", "sub-mod", "addon", "add-on" })) != null) return new(Cat.Patch, Trf("name has \"{0}\"", w));
        if ((w = Name(new[] { "compat", "compatch", "compatibility" }, true)) != null) return new(Cat.Patch, Trf("name has \"{0}\"", w));
        if (tags.Contains("fixes")) return new(Cat.Patch, Tr("tag Fixes"));
        if (tags.Contains("translation")) return new(Cat.Translation, Tr("tag Translation"));
        if ((w = Name(new[] { "translation", "localization", "localisation", "l10n", "locale" })) != null) return new(Cat.Translation, Trf("name has \"{0}\"", w));
        if ((w = Name(new[] { "library", "framework", "api", "lib", "core", "dependency", "dependencies", "requirements" })) != null) return new(Cat.Library, Trf("name has \"{0}\"", w));
        if (tags.Contains("total conversion")) return new(Cat.Overhaul, Tr("tag Total Conversion"));
        if ((w = Name(new[] { "overhaul", "total conversion" })) != null) return new(Cat.Overhaul, Trf("name has \"{0}\"", w));
        if (m.ReplacePaths.Count >= 4) return new(Cat.Overhaul, Trf("replaces {0} vanilla folders", m.ReplacePaths.Count));
        if (tags.Contains("interface")) return new(Cat.Ui, Tr("tag Interface"));
        if ((w = Name(new[] { "ui", "gui", "hud", "interface", "tooltip", "tooltips" })) != null) return new(Cat.Ui, Trf("name has \"{0}\"", w));
        if (tags.Contains("graphics")) return new(Cat.Graphics, Tr("tag Graphics"));
        if (tags.Contains("character models")) return new(Cat.Graphics, Tr("tag Character Models"));
        if (tags.Contains("sound")) return new(Cat.Graphics, Tr("tag Sound"));
        if ((w = Name(new[] { "portrait", "portraits", "clothing", "clothes", "hair", "hairstyle", "hairstyles", "beard", "beards", "texture", "textures", "skin", "skins", "ethnicity", "ethnicities", "visual", "visuals", "3d", "gfx", "models", "music", "soundtrack", "artwork", "icons" })) != null) return new(Cat.Graphics, Trf("name has \"{0}\"", w));
        return new(Cat.Content, "default");
    }

    public static CatGuess GuessFromFiles(ModFiles mf)
    {
        int n = mf.Files.Count;
        if (!mf.Complete || n == 0) return new(Cat.Content, "default");
        long loc = 0, ui = 0, vis = 0, world = 0;
        foreach (var f in mf.Files)
        {
            bool S(string p) => f.StartsWith(p, StringComparison.Ordinal);
            if (S("localization/")) loc++;
            else if (S("gui/") || S("gfx/interface/") || S("interface/") || S("fonts/")) ui++;
            else if (S("gfx/") || S("common/genes/") || S("music/") || S("sound/") || S("sfx/") || S("common/ethnicities/") || S("common/bookmark_portraits/")) vis++;
            else if (S("map_data/") || S("history/provinces/") || S("history/titles/") || S("common/landed_titles/") || S("history/characters/") || S("history/cultures/")) world++;
        }
        bool Pct(long x) => x * 100 >= (long)n * 85;
        if (Pct(loc)) return new(Cat.Translation, Tr("files: mostly localization"));
        if (Pct(ui)) return new(Cat.Ui, Tr("files: mostly interface"));
        if (Pct(vis)) return new(Cat.Graphics, Tr("files: mostly graphics/audio"));
        if (n >= 200 && world * 100 >= (long)n * 40) return new(Cat.Overhaul, Tr("files: rewrites the map and history"));
        return new(Cat.Content, "default");
    }
}
