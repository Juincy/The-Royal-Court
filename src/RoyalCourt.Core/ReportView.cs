using System.Globalization;
using System.Text;
namespace RoyalCourt.Core;

public sealed class RepRow
{
    public string[] Cells = Array.Empty<string>();
    public string Hay = "", Key = "", Copy = "", Author = "";
    public int Sev, Mod = -1;
    public double Frac = -1;      // bar length (0..1) in the Script time window, -1 = no bar
}

/// <summary>The three report windows in one: the game log helper (kind 0), "since you last pressed Play" (kind 1) and script time (kind 2).</summary>
public sealed class ReportView
{
    public const int KindLog = 0, KindSince = 1, KindTime = 2;
    readonly ModManager m;
    public int Kind;
    public int Sub;                       // the view button: log 0 by mod / 1 all messages; time 0 by mod / 1 events / 2 on_actions / 3 years / 4 history
    public string Filter = "";
    public List<RepRow> All = new();
    public List<RepRow> Shown = new();
    public int BarCol = -1;
    public string StatusText = "", ReportText = "";
    public bool WaitIndex;
    public LogReport? LogRep;
    public ReportView(ModManager mm, int kind) { m = mm; Kind = kind; }

    public sealed record Col(string Title, int Width);
    public List<Col> Columns()
    {
        string T(string s) => Tr(s);
        if (Kind == KindTime)
        {
            if (Sub == 0) return new() { new(T("Mod"), 330), new(T("Time"), 80), new(T("Share of script time"), 260), new(T("Runs"), 100), new(T("Slowest event or on_action"), 360) };
            if (Sub == 4) return new() { new(T("Mod"), 330), new(T("Share now"), 220), new(T("Share before"), 110), new(T("Change"), 100), new(T("Sessions seen"), 110), new(T("Last sessions (share %, oldest first)"), 300) };
            if (Sub == 3) return new() { new(T("Year"), 70), new(T("Living characters"), 260), new(T("Court"), 90), new(T("Rulers"), 90), new(T("Opinions"), 100), new(T("Schemes"), 90), new(T("Wars"), 70), new(T("Errors"), 90) };
            return new() { new(Sub == 1 ? T("Event") : T("On_action"), 330), new(T("Mod"), 300), new(T("Time"), 80), new(T("Share of script time"), 200), new(T("Runs"), 100) };
        }
        if (Kind == KindSince) return new() { new(T("What"), 190), new(T("Mod"), 320), new(T("Details"), 620) };
        if (Sub == 0) return new() { new(T("Mod"), 320), new(T("Errors"), 70), new(T("Warnings"), 80), new(T("Different"), 80), new(T("Most common message"), 620) };
        return new() { new(T("Mod"), 260), new(T("Level"), 70), new(T("Times"), 60), new(T("Message"), 560), new(T("File"), 360) };
    }

    public static string ReadHeadOrTail(string path, int maxBytes, bool tail)
    {
        try
        {
            using var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            long sz = f.Length;
            if (sz <= 0) return "";
            long start = (tail && sz > maxBytes) ? sz - maxBytes : 0;
            int len = (int)Math.Min(sz - start, maxBytes);
            f.Seek(start, SeekOrigin.Begin);
            var buf = new byte[len];
            int got = 0;
            while (got < len) { int r = f.Read(buf, got, len - got); if (r <= 0) break; got += r; }
            int s = 0;
            if (tail && start > 0) { int nl = Array.IndexOf(buf, (byte)'\n', 0, got); if (nl >= 0) s = nl + 1; }
            var clean = new List<byte>(got - s);
            for (int i = s; i < got; i++) if (buf[i] != 0) clean.Add(buf[i]);
            return Util.Utf8.GetString(clean.ToArray());
        }
        catch { return ""; }
    }

    static string When(long t)
    {
        try { return DateTimeOffset.FromUnixTimeSeconds(t).ToLocalTime().ToString("dd MMM yyyy HH:mm", CultureInfo.InvariantCulture); } catch { return "?"; }
    }
    static long FileWhen(string path)
    {
        try { return new DateTimeOffset(File.GetLastWriteTimeUtc(path), TimeSpan.Zero).ToUnixTimeSeconds(); } catch { return 0; }
    }

    public static string Message(LogEntry e) => Msg(e);
    static string Msg(LogEntry e)
    {
        string d = e.Detail;
        if (d.StartsWith("Error: ", StringComparison.Ordinal)) d = d[7..];
        static string Cut(string t, string tok) { int k = t.IndexOf(tok, StringComparison.Ordinal); return k > 0 ? t[..k] : t; }
        d = Cut(d, " | Script location:"); d = Cut(d, "Script location:");
        string mm = e.Msg;
        foreach (var tok in new[] { " at file: ", ", at file: ", " in 'file: ", " at location 'file: ", " in file: " }) mm = Cut(mm, tok);   // the file has its own column
        return (e.Msg == "Script system error!" && d.Length > 0) ? d : (d.Length == 0 ? mm : mm + " | " + d);
    }
    static string Where(LogEntry e) => e.File.Length == 0 ? "" : e.File + (e.Line != 0 ? ":" + e.Line : "");

    void Add(RepRow r)
    {
        var h = new StringBuilder();
        foreach (var c in r.Cells) h.Append(c).Append(' ');
        r.Hay = Util.Lower(h.ToString());
        All.Add(r);
    }

    static string TimeText(double v) => v >= 100 ? v.ToString("F0", CultureInfo.InvariantCulture) : v >= 10 ? v.ToString("F1", CultureInfo.InvariantCulture) : v.ToString("F2", CultureInfo.InvariantCulture);
    static string ShareText(double f) => f > 0 && f < 0.001 ? "<0.1%" : (f * 100.0).ToString("F1", CultureInfo.InvariantCulture) + "%";
    static string CountText(long n)
    {
        string s = n.ToString(CultureInfo.InvariantCulture);
        for (int i = s.Length - 3; i > 0; i -= 3) s = s.Insert(i, ",");
        return s;
    }
    static string ErrCountText(double v) => v >= 100000 ? Trf("{0} or more", CountText(100000)) : CountText((long)v);   // the game's counter stops at 100,000

    string NameOf(Playset ps, int pos) => m.Info.TryGetValue(ps.Mods[pos].Id, out var mi) ? mi.Name : ps.Mods[pos].Id;
    string Label(int pos)
    {
        var ps = m.Active();
        if (ps == null || pos < 0 || pos >= ps.Mods.Count) return "?";
        return "#" + (pos + 1) + "   " + NameOf(ps, pos);
    }

    /// <summary>Rebuilds the rows for the open view and applies the filter.</summary>
    public void Refresh()
    {
        if (Kind == KindLog) BuildLog(); else if (Kind == KindTime) BuildTime(); else BuildSince();
        Fill();
    }

    public void Fill()
    {
        string q = Util.Lower(Filter);
        Shown = q.Length == 0 ? new(All) : All.Where(r => r.Hay.Contains(q, StringComparison.Ordinal)).ToList();
    }
    public string StatusLine => Filter.Length > 0 ? StatusText + "   ·   " + Trf("showing {0} of {1}", Shown.Count, All.Count) : StatusText;

    string SamePlaysetText(Playset ps, string logs, bool timeView, out bool sameSet, out bool any)
    {
        sameSet = true; any = false;
        var loaded = GameLog.ParseLoadedMods(ReadHeadOrTail(Path.Combine(logs, "debug.log"), 6 << 20, false));
        if (loaded.Count == 0) return "";
        any = true;
        var was = new HashSet<string>(loaded.Where(x => x.Enabled).Select(x => x.Id), StringComparer.Ordinal);
        var cur = new HashSet<string>(ps.Mods.Where(x => x.Enabled).Select(x => x.Id), StringComparer.Ordinal);
        int onlyNow = cur.Count(id => !was.Contains(id)), onlyThen = was.Count(id => !cur.Contains(id));
        if (onlyNow == 0 && onlyThen == 0) return " · " + Tr("the game ran with the mods you have enabled now");
        sameSet = false;
        return " · " + Trf(timeView ? "NOT the playset you have now ({0} turned on, {1} turned off since); names may point at the wrong mods" : "NOT the playset you have now ({0} turned on, {1} turned off since); messages may point at the wrong mods", onlyNow, onlyThen);
    }

    bool IndexWait(Playset ps)
    {
        int enabled = 0, ready = 0;
        foreach (var pm in ps.Mods) if (pm.Enabled) { enabled++; if (m.FileIndex.TryGetValue(pm.Id, out var f) && f.Complete) ready++; }
        return ready < enabled;
    }

    // ---------- the game log ----------
    void BuildLog()
    {
        All = new(); LogRep = null; ReportText = ""; WaitIndex = false;
        var ps = m.Active();
        string dir = m.EffectiveDir();
        if (ps == null || dir.Length == 0) { StatusText = Tr("The CK3 folder is not set. Enter it in the main window first."); return; }
        string logs = Path.Combine(dir, "logs");
        string text = ReadHeadOrTail(Path.Combine(logs, "game.log"), 32 << 20, true);
        string old = ReadHeadOrTail(Path.Combine(logs, "error.log"), 8 << 20, true);
        if (text.Length == 0 && old.Length == 0) { StatusText = Trf("There is no game log yet ({0}). Start the game once, then open this again.", logs); return; }
        text += "\n"; text += old;
        if (IndexWait(ps)) { WaitIndex = true; if (!m.Scanning) m.MaybeScan(true); }
        var lr = GameLog.Attribute(GameLog.Parse(text, true), ps, m.FileIndex, m.DefIndex, m.Van);
        LogRep = lr;
        string same = SamePlaysetText(ps, logs, false, out _, out _);
        long when = FileWhen(Path.Combine(logs, "game.log"));
        StatusText = Trf("Log from {0}", when != 0 ? When(when) : "?") + " · " + Trf("{0} errors, {1} warnings ({2} different)", lr.Parse.Errors, lr.Parse.Warnings, lr.Parse.Entries.Count) + same
                     + (WaitIndex ? " · " + Tr("still reading your mods' files, this updates by itself") : "")
                     + (m.Van == null ? " · " + Tr("base-game file list not loaded") : "");
        var rep = new StringBuilder($"Game log summary ({lr.Parse.Errors} errors, {lr.Parse.Warnings} warnings)\n");
        string TopMsg(int mod)
        {
            LogEntry? best = null;
            for (int k = 0; k < lr.Parse.Entries.Count; k++)
            {
                if (lr.ModOf[k] != mod) continue;
                var e = lr.Parse.Entries[k];
                if (best == null || (e.Level == 'E' && best.Level != 'E') || (e.Level == best.Level && e.Count > best.Count)) best = e;
            }
            return best != null ? Msg(best) : "";
        }
        void Group(int mod, string label)
        {
            var ks = new List<int>();
            for (int k = 0; k < lr.Parse.Entries.Count; k++)
            {
                bool inn = mod >= 0 ? lr.ModOf[k] == mod : mod == -2 ? (lr.ModOf[k] < 0 && lr.How[k] == LogHow.Base) : (lr.ModOf[k] < 0 && lr.How[k] != LogHow.Base);
                if (inn) ks.Add(k);
            }
            ks = ks.OrderBy(k => lr.Parse.Entries[k].Level == 'E' ? 0 : 1).ThenByDescending(k => lr.Parse.Entries[k].Count).ToList();   // stable
            foreach (int k in ks)
            {
                var e = lr.Parse.Entries[k];
                var r = new RepRow { Mod = mod, Sev = e.Level == 'E' ? 2 : 1 };
                r.Cells = new[] { label + (lr.How[k] == LogHow.Name ? "  " + Tr("(matched by name)") : ""), e.Level == 'E' ? Tr("Error") : Tr("Warning"), e.Count.ToString(), Msg(e), Where(e) };
                r.Key = label;
                r.Copy = (e.Level == 'E' ? "[error] " : "[warning] ") + Msg(e) + (e.File.Length == 0 ? "" : "  (" + Where(e) + ")") + (e.Count > 1 ? "  x" + e.Count : "");
                Add(r);
            }
        }
        if (Sub == 0)
        {
            foreach (var pm in lr.PerMod)
            {
                string lab = Label(pm.Mod);
                var r = new RepRow { Mod = pm.Mod, Sev = pm.Errors != 0 ? 2 : 1 };
                r.Cells = new[] { lab, pm.Errors.ToString(), pm.Warnings.ToString(), pm.Entries.ToString(), TopMsg(pm.Mod) };
                r.Key = pm.Mod >= 0 && pm.Mod < ps.Mods.Count ? NameOf(ps, pm.Mod) : lab;
                r.Copy = $"{lab}: {pm.Errors} errors, {pm.Warnings} warnings";
                rep.Append(lab).Append(": ").Append(pm.Errors).Append(" errors, ").Append(pm.Warnings).Append(" warnings\n");
                Add(r);
            }
            if (lr.Base.Entries != 0)
            {
                var r = new RepRow { Mod = -2, Sev = 0, Key = Tr("Base game") };
                r.Cells = new[] { Tr("Base game files (no mod replaces them)"), lr.Base.Errors.ToString(), lr.Base.Warnings.ToString(), lr.Base.Entries.ToString(), Tr("Problems in the game's own files, or caused by a mod referring to something that is missing") };
                Add(r);
                rep.Append("Base game files: ").Append(lr.Base.Errors).Append(" errors, ").Append(lr.Base.Warnings).Append(" warnings\n");
            }
            if (lr.Unplaced.Entries != 0)
            {
                var r = new RepRow { Mod = -3, Sev = 0, Key = "Unplaced" };
                r.Cells = new[] { Tr("Could not be tied to a mod"), lr.Unplaced.Errors.ToString(), lr.Unplaced.Warnings.ToString(), lr.Unplaced.Entries.ToString(), Tr("No file or name in the message matches a mod you have enabled") };
                Add(r);
                rep.Append("Could not be tied to a mod: ").Append(lr.Unplaced.Errors).Append(" errors, ").Append(lr.Unplaced.Warnings).Append(" warnings\n");
            }
        }
        else
        {
            foreach (var pm in lr.PerMod) Group(pm.Mod, Label(pm.Mod));
            Group(-2, Tr("Base game"));
            Group(-3, Tr("Not tied to a mod"));
            rep.Append("(all messages: use the By mod view for a summary)\n");
        }
        ReportText = rep.ToString();
    }

    // ---------- script time ----------
    void BuildTime()
    {
        All = new(); ReportText = ""; BarCol = -1; WaitIndex = false;
        var ps = m.Active();
        string dir = m.EffectiveDir();
        if (ps == null || dir.Length == 0) { StatusText = Tr("The CK3 folder is not set. Enter it in the main window first."); return; }
        string logs = Path.Combine(dir, "logs");
        string evText = ReadHeadOrTail(Path.Combine(logs, "eventstats.csv"), 32 << 20, false), oaText = ReadHeadOrTail(Path.Combine(logs, "onactionstats.csv"), 32 << 20, false);
        string yrText = ReadHeadOrTail(Path.Combine(logs, "custom_automated_stats.log"), 32 << 20, false);
        var ev = ScriptTime.ParseStatsCsv(evText, false); var oa = ScriptTime.ParseStatsCsv(oaText, true);
        var years = ScriptTime.ParseYearStats(yrText);
        if (!ev.Ok && !oa.Ok && years.Count == 0) { StatusText = Trf("The game has not written its script statistics yet (eventstats.csv and onactionstats.csv in {0}). Play for a while, quit the game, then open this again.", logs); return; }
        bool waiting = IndexWait(ps);
        if (waiting) { WaitIndex = true; if (!m.Scanning) m.MaybeScan(true); }
        var baseOa = ScriptTime.BaseOnActionNames(Vanilla.GameDir(m.GameExeUsed));   // the base game's on_action names (a hundred small files, read once per window)
        var tr = ScriptTime.AttributeTime(ev, oa, ps, m.DefIndex, baseOa.Count == 0 ? null : baseOa);
        string same = SamePlaysetText(ps, logs, true, out bool sameSet, out bool anyLoaded);
        long when = FileWhen(Path.Combine(logs, oaText.Length == 0 ? "eventstats.csv" : "onactionstats.csv"));
        // one snapshot per game session (same mods as then, all files read), kept in the program's data folder
        string histFile = Path.Combine(Util.DataDir(), "scripttime_history.txt");
        var hist = new List<TimeSnap>();
        if (Util.ReadFile(histFile, out var ht, 4L << 20)) hist = ScriptTime.SnapFromText(ht);
        if (when > 0 && !waiting && tr.All > 0 && sameSet && anyLoaded)
        {
            bool known = hist.Any(x => x.When == when);
            if (!known) { ScriptTime.SnapAdd(hist, ScriptTime.MakeSnap(tr, ps, m.Info, when, m.GameVer)); Util.WriteFileAtomic(histFile, ScriptTime.SnapToText(hist)); }
        }
        double errs = years.Count == 0 ? 0 : (years[^1].V.TryGetValue("errors", out var ev0) ? ev0 : 0);
        StatusText = Trf("Statistics from {0}", when != 0 ? When(when) : "?") + " · " + Trf("{0} events and {1} on_actions measured", tr.Events.Rows.Count, tr.OnActions.Rows.Count) + same
                     + (waiting ? " · " + Tr("still reading your mods' files, this updates by itself") : "")
                     + (errs >= 1000 ? " · " + Trf("the game counted {0} script errors in that session (error messages cost time too: see the Game log)", ErrCountText(errs)) : "");
        var rep = new StringBuilder("Script time report (time as written by the game, share of all measured script time)\n");
        double all = tr.All > 0 ? tr.All : 1;
        void Push(RepRow r, string line) { r.Copy = line; rep.Append(line).Append('\n'); Add(r); }
        double Frac(double v, double mx) => mx > 0 ? v / mx : 0;
        if (Sub == 0)
        {
            BarCol = 2;
            double mx = tr.PerMod.Count == 0 ? 0 : tr.PerMod[0].Time;
            mx = Math.Max(mx, Math.Max(tr.BaseEvents, tr.SharedActions));
            foreach (var pm in tr.PerMod)
            {
                var r = new RepRow { Mod = pm.Mod, Key = pm.Mod >= 0 && pm.Mod < ps.Mods.Count ? NameOf(ps, pm.Mod) : "" };
                r.Cells = new[] { Label(pm.Mod), TimeText(pm.Time), ShareText(pm.Time / all), CountText(pm.Runs), pm.Slowest };
                r.Frac = Frac(pm.Time, mx);
                if (pm.Mod >= 0 && pm.Mod < ps.Mods.Count)
                {
                    string mid = ps.Mods[pm.Mod].Id;
                    m.Info.TryGetValue(mid, out var mi);
                    r.Author = ScriptTime.AuthorReport(tr, pm.Mod, NameOf(ps, pm.Mod), Names.SteamIdOf(mid), mi?.Version ?? "", m.GameVer, Env.Version, ScriptTime.TrendText(hist, mid), pm.Time, pm.Runs);
                }
                Push(r, Label(pm.Mod) + ": " + TimeText(pm.Time) + " (" + ShareText(pm.Time / all) + "), slowest: " + pm.Slowest);
            }
            if (tr.BaseEvents > 0)
            {
                var r = new RepRow { Mod = -2, Key = Tr("Base game") };
                r.Cells = new[] { Tr("Base game events (not found in your mods)"), TimeText(tr.BaseEvents), ShareText(tr.BaseEvents / all), CountText(tr.BaseRuns), "" };
                r.Frac = Frac(tr.BaseEvents, mx);
                Push(r, r.Cells[0] + ": " + r.Cells[1] + " (" + r.Cells[2] + ")");
            }
            if (tr.SharedActions > 0)
            {
                var r = new RepRow { Mod = -3, Key = "On_actions" };
                r.Cells = new[] { Tr("On_actions (base game and mods share these)"), TimeText(tr.SharedActions), ShareText(tr.SharedActions / all), CountText(tr.SharedRuns), Tr("see the On_actions view") };
                r.Frac = Frac(tr.SharedActions, mx);
                Push(r, r.Cells[0] + ": " + r.Cells[1] + " (" + r.Cells[2] + ")");
            }
        }
        else if (Sub == 1)
        {
            BarCol = 3;
            double mx = tr.Events.Rows.Count == 0 ? 0 : tr.Events.Rows[0].Own;
            for (int k = 0; k < tr.Events.Rows.Count && k < 600; k++)
            {
                var x = tr.Events.Rows[k];
                string who = tr.EventMod[k] >= 0 ? Label(tr.EventMod[k]) : Tr("Base game");
                var r = new RepRow { Mod = tr.EventMod[k], Key = x.Id };
                r.Cells = new[] { x.Id, who, TimeText(x.Own), ShareText(x.Own / all), CountText(x.Count) };
                r.Frac = Frac(x.Own, mx);
                Push(r, x.Id + " (" + who + "): " + TimeText(x.Own) + ", " + ShareText(x.Own / all) + ", " + CountText(x.Count) + " runs");
            }
        }
        else if (Sub == 2)
        {
            BarCol = 3;
            double mx = tr.OnActions.Rows.Count == 0 ? 0 : tr.OnActions.Rows[0].Own;
            for (int k = 0; k < tr.OnActions.Rows.Count && k < 600; k++)
            {
                var x = tr.OnActions.Rows[k];
                string who = "";
                if (tr.ActionMod[k] >= 0) who = Label(tr.ActionMod[k]);
                else if (tr.ActionDefiners[k].Count == 0) who = Tr("Base game");
                else
                {
                    int n = 0;
                    foreach (int mi in tr.ActionDefiners[k]) { if (n++ > 0) who += ", "; if (n > 2) { who += "..."; break; } who += Label(mi); }
                    who = Tr("Base game and ") + who;
                }
                var r = new RepRow { Mod = tr.ActionMod[k], Key = x.Id };
                r.Cells = new[] { x.Id, who, TimeText(x.Own), ShareText(x.Own / all), CountText(x.Count) };
                r.Frac = Frac(x.Own, mx);
                Push(r, x.Id + " (" + who + "): " + TimeText(x.Own) + ", " + ShareText(x.Own / all) + ", " + CountText(x.Count) + " runs");
            }
        }
        else if (Sub == 4)
        {
            BarCol = 1;
            var hr = ScriptTime.HistoryRows(hist, out bool haveBefore);
            StatusText = Trf("{0} game session(s) saved; the newest is compared with the one before it", hist.Count) + (hist.Count >= 2 ? " · " + Trf("total measured time {0} now, {1} before", TimeText(hist[^1].Total), TimeText(hist[^2].Total)) : "") + " · " + Tr("a session is saved when you open this window after playing");
            double mx = 0; foreach (var r in hr) mx = Math.Max(mx, r.Now);
            foreach (var r in hr)
            {
                string nm = r.Id == "*base" ? Tr("Base game events (not found in your mods)") : r.Id == "*shared" ? Tr("On_actions (base game and mods share these)") : r.Name;
                string ch;
                if (!r.InBefore) ch = Tr("new"); else if (!r.InNow) ch = Tr("gone");
                else ch = (r.Change * 100.0 >= 0 ? "+" : "") + (r.Change * 100.0).ToString("F1", CultureInfo.InvariantCulture) + " pts";
                var row = new RepRow { Key = nm };
                row.Cells = new[] { nm, r.InNow ? ShareText(r.Now) : "-", r.InBefore ? ShareText(r.Before) : "-", ch, r.Seen.ToString(), ScriptTime.TrendText(hist, r.Id) };
                row.Frac = Frac(r.Now, mx);
                Push(row, nm + ": " + row.Cells[1] + " (before " + row.Cells[2] + ", " + ch + ")");
            }
            if (!haveBefore && hr.Count > 0) StatusText += " · " + Tr("play again to see changes");
        }
        else
        {
            BarCol = 1;
            double mx = 0;
            foreach (var y in years) if (y.V.TryGetValue("living characters", out var lc)) mx = Math.Max(mx, lc);
            string Num(YearStats y, string k) => !y.V.TryGetValue(k, out var v) ? "" : k == "errors" ? ErrCountText(v) : CountText((long)v);
            for (int i = years.Count - 1; i >= 0; i--)
            {
                var y = years[i];
                var r = new RepRow { Key = y.Year.ToString() };
                r.Cells = new[] { y.Year.ToString(), Num(y, "living characters"), Num(y, "court characters"), Num(y, "ruler characters"), Num(y, "opinions"), Num(y, "schemes"), Num(y, "wars"), Num(y, "errors") };
                y.V.TryGetValue("living characters", out var lcv);
                r.Frac = Frac(lcv, mx);
                Push(r, r.Cells[0] + ": " + r.Cells[1] + " living characters, " + r.Cells[4] + " opinions, " + r.Cells[5] + " schemes");
            }
        }
        ReportText = rep.ToString();
        if (All.Count == 0) Add(new RepRow { Cells = new[] { Tr("Nothing to show for this view.") } });
    }

    // ---------- since you last pressed Play ----------
    void BuildSince()
    {
        All = new(); ReportText = "";
        var ps = m.Active();
        if (ps == null) { StatusText = Tr("No playset."); return; }
        var pr = Changes.BuildPlayReport(m.St, ps, m.Info, m.FpNow(), m.FileIndex, m.GameVer);
        StatusText = pr.HasPrev ? Trf("Since you last pressed Play: {0}", When(pr.Time)) + (pr.PrevGame.Length == 0 ? "" : ", " + Trf("game {0}", pr.PrevGame)) + (pr.SamePlayset ? "" : " " + Tr("(a different playset, so turned-on and turned-off mods are not compared)"))
                                 : Tr("Play has not been pressed with this version yet. After you press Play, this window shows what changed since.");
        var rep = new StringBuilder();
        void AddRow(string what, string name, string text, int sev)
        {
            var r = new RepRow { Sev = sev };
            r.Cells = new[] { Tr(what), name, text };
            r.Copy = what + ": " + name + " - " + text;
            rep.Append(r.Copy).Append('\n');
            Add(r);
        }
        if (pr.GameChanged) AddRow(K("Game updated"), "Crusader Kings III", pr.PrevGame + " -> " + pr.NowGame, 1);
        foreach (var i in pr.Outdated) AddRow(K("Made for an older game"), i.Name, i.Text, 1);
        foreach (var i in pr.Updated) AddRow(K("Updated"), i.Name, i.Text, 0);
        foreach (var i in pr.Added) AddRow(K("Turned on"), i.Name, i.Text, 0);
        foreach (var i in pr.Removed) AddRow(K("Gone"), i.Name, i.Text, i.Sev);
        ReportText = rep.ToString();
        if (All.Count == 0) Add(new RepRow { Cells = new[] { pr.HasPrev ? Tr("Nothing has changed since you last pressed Play.") : Tr("Nothing to compare yet."), "", "" } });
    }

    // ---------- row actions ----------
    /// <summary>Double-click: a mod row jumps to its detail view with the filter set to its name. Returns true when the view changed.</summary>
    public bool DrillDown(RepRow row)
    {
        if (Kind == KindTime && Sub == 0 && row.Mod >= 0) { Sub = 1; Filter = row.Cells[0]; Refresh(); return true; }
        if (Kind == KindLog && Sub == 0) { Sub = 1; Filter = row.Key; Refresh(); return true; }
        return false;
    }

    public string ModMessages(RepRow row)
    {
        var ps = m.Active();
        if (LogRep == null || ps == null || row.Mod < 0 || row.Mod >= ps.Mods.Count) return "";
        return GameLog.ModReport(LogRep, row.Mod, NameOf(ps, row.Mod));
    }
}
