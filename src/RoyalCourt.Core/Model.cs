namespace RoyalCourt.Core;

public sealed class ModRef
{
    public string Id = "";
    public bool Enabled;
    public string Name = "";   // display name from the playset file
    public ModRef() { }
    public ModRef(string id, bool enabled, string name = "") { Id = id; Enabled = enabled; Name = name; }
}

public sealed class Playset
{
    public string Name = "";
    public List<ModRef> Mods = new();
    public Playset Clone()
    {
        var p = new Playset { Name = Name };
        foreach (var m in Mods) p.Mods.Add(new ModRef(m.Id, m.Enabled, m.Name));
        return p;
    }
}

public sealed class ModInfo
{
    public string Id = "", Name = "", Version = "", Supported = "", Source = "";
    public string Path = "", Archive = "";                 // as written in the descriptor
    public List<string> Deps = new(), ReplacePaths = new(), Tags = new();
    public string ContentDir = "";                         // resolved folder with the mod's files ("" if unknown / archive)
    public int ContentState;                               // 0 unknown (archive or no path), 1 folder exists, 2 folder missing
    public bool Pending;                                   // found in Steam's Workshop folder, but the Paradox launcher has not created its ugc_ descriptor yet
}

public sealed class Settings
{
    public string Ck3Dir = "", Active = "", GameExe = "", Theme = "";                         // theme: "dark", "light", "halloween", "agot" or "" (follow Windows)
    public SortedDictionary<string, SortedSet<string>> Locks = new(StringComparer.Ordinal);   // playset name -> mod ids that Auto Sort never moves
    /// <summary>What the last finished Bisect found, per playset (the Crash helper shows it).</summary>
    public sealed class BisectFind { public List<string> Ids = new(), Names = new(); public long When; public bool TurnedOff; }
    public SortedDictionary<string, BisectFind> BisectLast = new(StringComparer.Ordinal);
    public SortedDictionary<string, SortedSet<string>> BisectKeep = new(StringComparer.Ordinal);   // playset name -> mod ids Bisect never turns off
    public SortedDictionary<string, int> Cats = new(StringComparer.Ordinal);                  // mod id -> category chosen by the user (overrides the guess)
    public SortedSet<string> Hidden = new(StringComparer.Ordinal);                            // mods the user removed from the list (never touched on disk)
    public SortedDictionary<string, long> Unsubbed = new(StringComparer.Ordinal);             // Workshop mods the user unsubscribed from -> when (seconds since 1970)
    public SortedDictionary<string, string> Launch = new(StringComparer.Ordinal);             // playset name -> extra command line options for the game
    public SortedDictionary<string, string> Seen = new(StringComparer.Ordinal);               // mod id -> fingerprint when Play was last pressed (update detection)
    public int WinX, WinY, WinW, WinH; public bool WinMax;                                    // remembered window position and size (WinW == 0: none)
    public List<int> ColW = new();                                                            // remembered widths of the list columns (0 = automatic)
    public bool Pictures = true;                                                              // show the mod pictures (thumbnail.png from the mod folder) in the list
    public bool PlayCheck = true;                                                             // run the health check when Play is pressed (on by default)
    public bool Gpu;                                                                          // draw with the graphics card (off by default: software drawing is steady on every PC and lighter on memory)
    public bool CheckUpdates;                                                                 // look for a newer release when the program starts (off by default)
    public string PlayGameVer = "", PlayName = "";                                            // game version and playset name when Play was last pressed
    public long PlayTime;                                                                     // when (seconds since 1970; 0 = never)
    public List<string> PlayMods = new();                                                     // mods that were enabled then
    public string CrashSeen = "";                                                             // newest crash folder name the user has been told about
    public bool TutorialDone;                                                                 // the first-launch tutorial has been shown
    public string Language = "auto";                                                          // "auto" (follow Windows) or a language code
    /// <summary>Settings.json exists but could not be read: saving is refused so the user's real settings survive.</summary>
    public static bool Unreadable;

    static void LoadExtra(Settings st, J root)
    {
        if (root.Get("hidden") is { IsArr: true } h) foreach (var e in h.A) if (e.IsStr) st.Hidden.Add(e.S);
        if (root.Get("unsubbed") is { IsObj: true } u)
            for (int i = 0; i < u.Keys.Count && st.Unsubbed.Count < 5000; i++) if (u.Vals[i].IsNum && u.Vals[i].N >= 0 && u.Vals[i].N < 4e10) st.Unsubbed[u.Keys[i]] = (long)u.Vals[i].N;
        if (root.Get("launch") is { IsObj: true } l)
            for (int i = 0; i < l.Keys.Count; i++) if (l.Vals[i].IsStr && l.Vals[i].S.Length > 0) st.Launch[l.Keys[i]] = l.Vals[i].S;
        if (root.Get("seen") is { IsObj: true } sn)
            for (int i = 0; i < sn.Keys.Count; i++) if (sn.Vals[i].IsStr) st.Seen[sn.Keys[i]] = sn.Vals[i].S;
        if (root.Get("checkUpdates") is { IsBool: true } cu) st.CheckUpdates = cu.B;
        if (root.Get("gpu") is { IsBool: true } gp) st.Gpu = gp.B;
        if (root.Get("playCheck") is { IsBool: true } pc) st.PlayCheck = pc.B;
        if (root.Get("pictures") is { IsBool: true } pi) st.Pictures = pi.B;
        if (root.Get("lastPlay") is { IsObj: true } lp)
        {
            if (lp.Get("game") is { IsStr: true } g) st.PlayGameVer = g.S.Length > 40 ? g.S[..40] : g.S;
            if (lp.Get("playset") is { IsStr: true } p) st.PlayName = p.S.Length > 100 ? p.S[..100] : p.S;
            if (lp.Get("time") is { IsNum: true } t && t.N >= 0 && t.N < 4e10) st.PlayTime = (long)t.N;
            if (lp.Get("mods") is { IsArr: true } m) foreach (var e in m.A) if (e.IsStr && st.PlayMods.Count < 5000) st.PlayMods.Add(e.S);
        }
        if (root.Get("window") is { IsObj: true } w)
        {
            void Num(string k, ref int o) { if (w.Get(k) is { IsNum: true } v) o = Util.ClampInt(v.N); }
            Num("x", ref st.WinX); Num("y", ref st.WinY); Num("w", ref st.WinW); Num("h", ref st.WinH);
            if (w.Get("max") is { IsBool: true } mx) st.WinMax = mx.B;
            if (w.Get("cols") is { IsArr: true } c) foreach (var e in c.A) st.ColW.Add(e.IsNum ? Util.ClampInt(e.N) : 0);
        }
    }

    public static Settings Load()
    {
        var st = new Settings();
        Unreadable = false;
        string text = "";
        string path = Util.SettingsPath();
        bool there = Util.FileExists(path), ok = false;
        for (int t = 0; t < 4 && there && !ok; t++)
        {
            ok = Util.ReadFile(path, out text);
            if (!ok) Thread.Sleep(150);                    // an antivirus scan can hold the file for a moment
        }
        if (!ok) { Unreadable = there; return st; }        // present but unreadable: the real file must not be overwritten with defaults
        if (!J.TryParse(text, out var root) || !root.IsObj)
        {
            // A damaged settings file is replaced on the next save: keep what was there so it can be recovered by hand.
            string bad = path + ".bad";
            if (!Util.Exists(bad)) Util.WriteFile(bad, text);
            return st;
        }
        if (root.Get("ck3Dir") is { IsStr: true } d) st.Ck3Dir = d.S;
        if (root.Get("active") is { IsStr: true } a) st.Active = a.S;
        if (root.Get("gameExe") is { IsStr: true } g) st.GameExe = g.S;
        if (root.Get("theme") is { IsStr: true } th && (th.S == "dark" || th.S == "light" || th.S == "halloween" || th.S == "agot")) st.Theme = th.S;
        if (root.Get("crashSeen") is { IsStr: true } cs && cs.S.Length < 100) st.CrashSeen = cs.S;
        if (root.Get("tutorialDone") is { IsBool: true } td) st.TutorialDone = td.B;
        if (root.Get("language") is { IsStr: true } lg && (lg.S == "auto" || Lang.FromCode(lg.S) >= 0)) st.Language = lg.S;
        if (root.Get("locks") is { IsObj: true } lk)
            for (int i = 0; i < lk.Keys.Count; i++)
                if (lk.Vals[i].IsArr)
                    foreach (var e in lk.Vals[i].A)
                        if (e.IsStr)
                        {
                            if (!st.Locks.TryGetValue(lk.Keys[i], out var set)) st.Locks[lk.Keys[i]] = set = new SortedSet<string>(StringComparer.Ordinal);
                            set.Add(e.S);
                        }
        if (root.Get("bisectKeep") is { IsObj: true } bk)
            for (int i = 0; i < bk.Keys.Count; i++)
                if (bk.Vals[i].IsArr)
                    foreach (var e in bk.Vals[i].A)
                        if (e.IsStr)
                        {
                            if (!st.BisectKeep.TryGetValue(bk.Keys[i], out var set)) st.BisectKeep[bk.Keys[i]] = set = new SortedSet<string>(StringComparer.Ordinal);
                            set.Add(e.S);
                        }
        if (root.Get("bisectLast") is { IsObj: true } bl)
            for (int i = 0; i < bl.Keys.Count; i++)
                if (bl.Vals[i].IsObj)
                {
                    var f = new BisectFind(); var o = bl.Vals[i];
                    if (o.Get("ids") is { IsArr: true } ia) foreach (var e in ia.A) if (e.IsStr) f.Ids.Add(e.S);
                    if (o.Get("names") is { IsArr: true } na) foreach (var e in na.A) if (e.IsStr) f.Names.Add(e.S);
                    if (o.Get("when") is { IsNum: true } wn) f.When = (long)wn.N;
                    if (o.Get("off") is { IsBool: true } ob) f.TurnedOff = ob.B;
                    if (f.Ids.Count > 0) st.BisectLast[bl.Keys[i]] = f;
                }
        if (root.Get("categories") is { IsObj: true } ct)
            for (int i = 0; i < ct.Keys.Count; i++)
                if (ct.Vals[i].IsNum && ct.Vals[i].N >= 0 && ct.Vals[i].N < 7) st.Cats[ct.Keys[i]] = (int)ct.Vals[i].N;
        LoadExtra(st, root);
        return st;
    }

    public bool Save()
    {
        if (Unreadable) return false;
        var root = J.Obj();
        root.Set("ck3Dir", J.Str(Ck3Dir));
        root.Set("active", J.Str(Active));
        root.Set("gameExe", J.Str(GameExe));
        if (Theme.Length > 0) root.Set("theme", J.Str(Theme));
        if (CrashSeen.Length > 0) root.Set("crashSeen", J.Str(CrashSeen));
        if (TutorialDone) root.Set("tutorialDone", J.Bool(true));
        if (Language != "auto") root.Set("language", J.Str(Language));
        var lk = J.Obj();
        foreach (var kv in Locks)
        {
            if (kv.Value.Count == 0) continue;
            var a = J.Arr();
            foreach (var id in kv.Value) a.A.Add(J.Str(id));
            lk.Set(kv.Key, a);
        }
        if (lk.Keys.Count > 0) root.Set("locks", lk);
        var bkj = J.Obj();
        foreach (var kv in BisectKeep)
        {
            if (kv.Value.Count == 0) continue;
            var a = J.Arr();
            foreach (var id in kv.Value) a.A.Add(J.Str(id));
            bkj.Set(kv.Key, a);
        }
        if (bkj.Keys.Count > 0) root.Set("bisectKeep", bkj);
        var blj = J.Obj();
        foreach (var kv in BisectLast)
        {
            var o = J.Obj(); var ia = J.Arr(); var na = J.Arr();
            foreach (var id in kv.Value.Ids) ia.A.Add(J.Str(id));
            foreach (var n in kv.Value.Names) na.A.Add(J.Str(n));
            o.Set("ids", ia); o.Set("names", na); o.Set("when", J.Num(kv.Value.When)); o.Set("off", J.Bool(kv.Value.TurnedOff));
            blj.Set(kv.Key, o);
        }
        if (blj.Keys.Count > 0) root.Set("bisectLast", blj);
        var ct = J.Obj();
        foreach (var kv in Cats) ct.Set(kv.Key, J.Num(kv.Value));
        if (ct.Keys.Count > 0) root.Set("categories", ct);
        if (Hidden.Count > 0) { var a = J.Arr(); foreach (var id in Hidden) a.A.Add(J.Str(id)); root.Set("hidden", a); }
        if (Unsubbed.Count > 0) { var o = J.Obj(); foreach (var kv in Unsubbed) o.Set(kv.Key, J.Num(kv.Value)); root.Set("unsubbed", o); }
        if (Launch.Count > 0) { var o = J.Obj(); foreach (var kv in Launch) o.Set(kv.Key, J.Str(kv.Value)); root.Set("launch", o); }
        if (Seen.Count > 0) { var o = J.Obj(); foreach (var kv in Seen) o.Set(kv.Key, J.Str(kv.Value)); root.Set("seen", o); }
        if (CheckUpdates) root.Set("checkUpdates", J.Bool(true));
        if (Gpu) root.Set("gpu", J.Bool(true));
        if (!PlayCheck) root.Set("playCheck", J.Bool(false));
        if (!Pictures) root.Set("pictures", J.Bool(false));
        if (PlayTime > 0)
        {
            var lp = J.Obj();
            lp.Set("game", J.Str(PlayGameVer)); lp.Set("playset", J.Str(PlayName)); lp.Set("time", J.Num(PlayTime));
            var a = J.Arr(); foreach (var id in PlayMods) a.A.Add(J.Str(id)); lp.Set("mods", a);
            root.Set("lastPlay", lp);
        }
        if (WinW > 0)
        {
            var w = J.Obj();
            w.Set("x", J.Num(WinX)); w.Set("y", J.Num(WinY)); w.Set("w", J.Num(WinW)); w.Set("h", J.Num(WinH));
            w.Set("max", J.Bool(WinMax));
            var c = J.Arr(); foreach (int v in ColW) c.A.Add(J.Num(v)); w.Set("cols", c);
            root.Set("window", w);
        }
        return Util.WriteFileAtomic(Util.SettingsPath(), root.Dump() + "\n");
    }
}
