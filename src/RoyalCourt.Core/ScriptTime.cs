using System.Globalization;
using System.Text;
namespace RoyalCourt.Core;

// Which mod's scripts use the most time: reads the game's own eventstats.csv / onactionstats.csv (see scripttime.hpp for the full explanation).

public sealed class TimeRow { public string Id = ""; public double Sum, Own, Share; public long Count; }
public sealed class TimeTable { public List<TimeRow> Rows = new(); public double Total; public bool Ok; }
public sealed class TimeMod { public int Mod = -1; public double Time; public long Runs; public int Events; public string Slowest = ""; public double SlowestTime; }
public sealed class TimeReport
{
    public TimeTable Events = new(), OnActions = new();
    public int[] EventMod = Array.Empty<int>();
    public int[] ActionMod = Array.Empty<int>();
    public List<int>[] ActionDefiners = Array.Empty<List<int>>();
    public List<TimeMod> PerMod = new();
    public double BaseEvents, SharedActions, All;
    public long BaseRuns, SharedRuns;
    public int ActionsByName;
}
public sealed class SnapMod { public string Id = "", Name = ""; public double Time, Share; public long Runs; }
public sealed class TimeSnap { public long When; public double Total; public int Events; public string Playset = "", Game = ""; public List<SnapMod> Mods = new(); }
public sealed class HistRow { public string Id = "", Name = ""; public double Now, Before, Change; public bool InNow, InBefore; public int Seen; public long RunsNow; }
public sealed class YearStats { public int Year; public SortedDictionary<string, double> V = new(StringComparer.Ordinal); }

public static class ScriptTime
{
    public static List<string> SplitSemicolons(string s) => new(s.Split(';'));
    public static double ToDouble(string s)
    {
        if (s.Length == 0) return 0;
        double d = Util.StrToD(s);
        return (double.IsNaN(d) || double.IsInfinity(d) || d < 0) ? 0 : d;
    }
    static long ToLong(double d) => d >= 9.2e18 ? long.MaxValue : d <= 0 ? 0 : (long)d;

    /// <summary>events: time = Sum. on_actions: time = own part. share = time / sum of all times in the table.</summary>
    public static TimeTable ParseStatsCsv(string text, bool onAction)
    {
        var t = new TimeTable();
        var col = new Dictionary<string, int>(StringComparer.Ordinal);
        bool header = false;
        int p = 0;
        while (p < text.Length)
        {
            int e = text.IndexOf('\n', p);
            if (e < 0) e = text.Length;
            string line = text.Substring(p, e - p);
            p = e + 1;
            line = line.TrimEnd('\r', ' ');
            if (line.Length == 0) continue;
            var c = SplitSemicolons(line);
            if (!header)
            {
                if (line[0] != '#') continue;
                c[0] = c[0].Substring(1);
                for (int i = 0; i < c.Count; i++) col[c[i]] = i;
                if (!col.ContainsKey("ID") || !col.ContainsKey("Sum")) return t;
                header = true;
                continue;
            }
            double Get(string name) => col.TryGetValue(name, out int ix) && ix < c.Count ? ToDouble(c[ix]) : 0.0;
            if (c.Count == 0 || c[0].Length == 0 || c.Count + 1 < col.Count) continue;
            var r = new TimeRow { Id = c[0], Sum = Get("Sum") };
            r.Count = ToLong(Math.Max(Get("ExecuteCount"), Math.Max(Get("IsValidToTriggerCount"), Get("ImmediateEffectCount"))));
            if (onAction)
            {
                r.Own = r.Sum - Get("ExecuteEventsTotal") - Get("ExecuteOnActionsTotal");
                if (r.Own < 0) r.Own = 0;
            }
            else r.Own = r.Sum;
            t.Total += r.Own;
            t.Rows.Add(r);
        }
        t.Ok = header;
        foreach (var r in t.Rows) r.Share = t.Total > 0 ? r.Own / t.Total : 0;
        t.Rows = t.Rows.OrderByDescending(r => r.Own).ToList();   // stable
        return t;
    }

    public static string FirstWord(string s)
    {
        int u = s.IndexOf('_');
        string w = u < 0 ? s : s.Substring(0, u);
        int d = w.IndexOf('.');
        if (d >= 0) w = w.Substring(0, d);
        var a = w.ToCharArray();
        for (int i = 0; i < a.Length; i++) if (a[i] >= 'A' && a[i] <= 'Z') a[i] = (char)(a[i] + 32);
        return new string(a);
    }

    /// <summary>The names of the base game's on_actions (game/common/on_action and the same folder in every DLC).</summary>
    public static HashSet<string> BaseOnActionNames(string gameDir)
    {
        var names = new HashSet<string>(StringComparer.Ordinal);
        if (gameDir.Length == 0) return names;
        var roots = new List<string> { Path.Combine(gameDir, "common", "on_action") };
        foreach (var dd in Vanilla.DlcDirs(gameDir))
            foreach (var e in Util.ListDir(dd)) roots.Add(Path.Combine(e.FullName, "common", "on_action"));
        var keys = new List<ScriptKey>();
        foreach (var r in roots)
        {
            if (!Util.DirExists(r)) continue;
            var stack = new Stack<string>(); stack.Push(r);
            while (stack.Count > 0)
            {
                string d = stack.Pop();
                foreach (var e in Util.ListDir(d))
                {
                    if (e is DirectoryInfo) { if ((e.Attributes & FileAttributes.ReparsePoint) == 0) stack.Push(e.FullName); continue; }
                    if (Util.Lower(e.Extension) != ".txt") continue;
                    if (!Util.ReadFile(e.FullName, out var text, 8 << 20)) continue;
                    keys.Clear();
                    Scripts.ScanScriptKeys(text, 0, keys);
                    foreach (var k in keys) names.Add(k.Key);
                }
            }
        }
        return names;
    }

    public static TimeReport AttributeTime(TimeTable events, TimeTable actions, Playset ps, IReadOnlyDictionary<string, ModDefs> defs, ISet<string>? baseActions = null)
    {
        var r = new TimeReport { Events = events, OnActions = actions };
        var wantEv = new HashSet<string>(StringComparer.Ordinal); var wantOa = new HashSet<string>(StringComparer.Ordinal);
        foreach (var x in r.Events.Rows) wantEv.Add(x.Id);
        foreach (var x in r.OnActions.Rows) wantOa.Add(x.Id);
        var evOwner = new Dictionary<string, int>(StringComparer.Ordinal);
        var oaDef = new Dictionary<string, List<int>>(StringComparer.Ordinal);
        var words = new HashSet<string>[ps.Mods.Count];
        for (int i = 0; i < words.Length; i++) words[i] = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            if (!ps.Mods[i].Enabled || !defs.TryGetValue(ps.Mods[i].Id, out var d)) continue;
            foreach (var de in d.Defs)
            {
                if (de.Kind == DK.Event)
                {
                    string nm = d.NameOf(de);
                    if (nm.IndexOf('.') >= 0) { string w = FirstWord(nm); if (w.Length >= 2) words[i].Add(w); }
                    if (wantEv.Contains(nm)) evOwner[nm] = i;
                }
                else if (de.Kind == DK.OnAction)
                {
                    string nm = d.NameOf(de);
                    if (wantOa.Contains(nm)) { if (!oaDef.TryGetValue(nm, out var v)) oaDef[nm] = v = new List<int>(); if (v.Count == 0 || v[^1] != i) v.Add(i); }
                }
            }
        }
        var pm = new SortedDictionary<int, TimeMod>();
        TimeMod Pm(int k) { if (!pm.TryGetValue(k, out var m)) pm[k] = m = new TimeMod(); return m; }
        r.EventMod = new int[r.Events.Rows.Count]; Array.Fill(r.EventMod, -1);
        for (int k = 0; k < r.Events.Rows.Count; k++)
        {
            var x = r.Events.Rows[k];
            if (!evOwner.TryGetValue(x.Id, out int owner)) { r.BaseEvents += x.Own; r.BaseRuns += x.Count; continue; }
            r.EventMod[k] = owner;
            var m = Pm(owner);
            m.Mod = owner; m.Time += x.Own; m.Runs += x.Count; m.Events++;
            if (x.Own > m.SlowestTime) { m.SlowestTime = x.Own; m.Slowest = x.Id; }
        }
        r.ActionMod = new int[r.OnActions.Rows.Count]; Array.Fill(r.ActionMod, -1);
        r.ActionDefiners = new List<int>[r.OnActions.Rows.Count];
        for (int k = 0; k < r.OnActions.Rows.Count; k++)
        {
            var x = r.OnActions.Rows[k];
            r.ActionDefiners[k] = new List<int>();
            bool has = oaDef.TryGetValue(x.Id, out var it);
            if (has) r.ActionDefiners[k] = it!;
            string w = FirstWord(x.Id);
            bool ownName = baseActions != null && baseActions.Count > 0 && has && it!.Count == 1 && !baseActions.Contains(x.Id);
            if (ownName || (has && it!.Count == 1 && w.Length >= 2 && w != "on" && words[it[0]].Contains(w)))
            {
                int mi = it![0];
                if (ownName) r.ActionsByName++;
                r.ActionMod[k] = mi;
                var m = Pm(mi);
                m.Mod = mi; m.Time += x.Own; m.Runs += x.Count;
                if (x.Own > m.SlowestTime) { m.SlowestTime = x.Own; m.Slowest = x.Id; }
            }
            else { r.SharedActions += x.Own; r.SharedRuns += x.Count; }
        }
        r.PerMod = pm.Values.OrderByDescending(m => m.Time).ToList();   // stable
        r.All = r.Events.Total + r.OnActions.Total;
        return r;
    }

    // ---------- history ----------
    static string SnapClean(string s) { var a = s.ToCharArray(); for (int i = 0; i < a.Length; i++) if (a[i] == '\t' || a[i] == '\n' || a[i] == '\r') a[i] = ' '; return new string(a); }

    public static string SnapToText(IEnumerable<TimeSnap> v)
    {
        var o = new StringBuilder();
        foreach (var s in v)
        {
            o.Append("S\t").Append(s.When).Append('\t').Append(Util.G(s.Total, 6)).Append('\t').Append(s.Events).Append('\t').Append(SnapClean(s.Playset)).Append('\t').Append(SnapClean(s.Game)).Append('\n');
            foreach (var m in s.Mods)
                o.Append("M\t").Append(SnapClean(m.Id)).Append('\t').Append(Util.G(m.Time, 6)).Append('\t').Append(Util.G(m.Share, 6)).Append('\t').Append(m.Runs).Append('\t').Append(SnapClean(m.Name)).Append('\n');
        }
        return o.ToString();
    }

    public static List<TimeSnap> SnapFromText(string text)
    {
        var v = new List<TimeSnap>();
        int p = 0;
        while (p < text.Length)
        {
            int e = text.IndexOf('\n', p);
            if (e < 0) e = text.Length;
            string line = text.Substring(p, e - p);
            p = e + 1;
            var c = line.Split('\t');
            if (c.Length >= 6 && c[0] == "S") v.Add(new TimeSnap { When = Util.Atoll(c[1]), Total = ToDouble(c[2]), Events = (int)Math.Min(int.MaxValue, Math.Max(0, Util.Atoll(c[3]))), Playset = c[4], Game = c[5] });
            else if (c.Length >= 6 && c[0] == "M" && v.Count > 0) v[^1].Mods.Add(new SnapMod { Id = c[1], Time = ToDouble(c[2]), Share = ToDouble(c[3]), Runs = Util.Atoll(c[4]), Name = c[5] });
        }
        return v;
    }

    /// <summary>Adds a snapshot of this report (replaces one with the same time), keeps the newest keep, oldest first.</summary>
    public static void SnapAdd(List<TimeSnap> v, TimeSnap s, int keep = 30)
    {
        for (int i = 0; i < v.Count; i++) if (v[i].When == s.When) { v[i] = s; return; }
        v.Add(s);
        var sorted = v.OrderBy(x => x.When).ToList();   // stable
        v.Clear(); v.AddRange(sorted);
        while (v.Count > keep) v.RemoveAt(0);
    }

    public static TimeSnap MakeSnap(TimeReport r, Playset ps, IReadOnlyDictionary<string, ModInfo> info, long when, string game)
    {
        var s = new TimeSnap { When = when, Total = r.All, Events = r.Events.Rows.Count, Playset = ps.Name, Game = game };
        double all = r.All > 0 ? r.All : 1;
        foreach (var m in r.PerMod)
        {
            if (m.Mod < 0 || m.Mod >= ps.Mods.Count) continue;
            var sm = new SnapMod { Id = ps.Mods[m.Mod].Id };
            sm.Name = info.TryGetValue(sm.Id, out var mi) ? mi.Name : sm.Id;
            sm.Time = m.Time; sm.Share = m.Time / all; sm.Runs = m.Runs;
            s.Mods.Add(sm);
        }
        if (r.BaseEvents > 0) s.Mods.Add(new SnapMod { Id = "*base", Name = "", Time = r.BaseEvents, Share = r.BaseEvents / all, Runs = r.BaseRuns });
        if (r.SharedActions > 0) s.Mods.Add(new SnapMod { Id = "*shared", Name = "", Time = r.SharedActions, Share = r.SharedActions / all, Runs = r.SharedRuns });
        return s;
    }

    /// <summary>Compares the newest snapshot with the one before it (same playset name preferred).</summary>
    public static List<HistRow> HistoryRows(IReadOnlyList<TimeSnap> v, out bool haveBefore)
    {
        var rows = new List<HistRow>();
        haveBefore = false;
        if (v.Count == 0) return rows;
        var now = v[^1];
        TimeSnap? before = null;
        for (int i = v.Count - 2; i >= 0; i--) if (v[i].Playset == now.Playset) { before = v[i]; break; }
        if (before == null && v.Count >= 2) before = v[^2];
        haveBefore = before != null;
        var at = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (var m in now.Mods) { at[m.Id] = rows.Count; rows.Add(new HistRow { Id = m.Id, Name = m.Name, Now = m.Share, InNow = true, RunsNow = m.Runs }); }
        if (before != null)
            foreach (var m in before.Mods)
            {
                if (!at.TryGetValue(m.Id, out int f)) { at[m.Id] = rows.Count; rows.Add(new HistRow { Id = m.Id, Name = m.Name, Before = m.Share, InBefore = true }); }
                else { rows[f].Before = m.Share; rows[f].InBefore = true; }
            }
        foreach (var r in rows)
        {
            r.Change = r.Now - r.Before;
            foreach (var s in v) foreach (var m in s.Mods) if (m.Id == r.Id) { r.Seen++; break; }
        }
        return rows.OrderByDescending(r => r.Now).ThenByDescending(r => r.Before).ToList();   // stable
    }

    /// <summary>One mod's share of script time over the last sessions of the same playset, oldest first, in percent: "3.8 · 4.1 · -".</summary>
    public static string TrendText(IReadOnlyList<TimeSnap> v, string id, int maxN = 6)
    {
        if (v.Count == 0) return "";
        var pick = new List<TimeSnap>();
        for (int i = v.Count - 1; i >= 0 && pick.Count < maxN; i--) if (v[i].Playset == v[^1].Playset) pick.Add(v[i]);
        var o = new StringBuilder();
        for (int k = pick.Count - 1; k >= 0; k--)
        {
            string cell = "-";
            foreach (var m in pick[k].Mods) if (m.Id == id) { cell = (m.Share * 100.0).ToString("F1", CultureInfo.InvariantCulture); break; }
            if (o.Length > 0) o.Append(" · ");
            o.Append(cell);
        }
        return o.ToString();
    }

    /// <summary>A plain-English note a player can send to a mod's author.</summary>
    public static string AuthorReport(TimeReport tr, int mod, string modName, string workshopId, string modVersion, string gameVer, string toolVer, string trend, double modTime, long modRuns)
    {
        var o = new StringBuilder("Script time report for \"" + modName + "\"" + (workshopId.Length == 0 ? "" : " (Steam Workshop id " + workshopId + ")") + "\n");
        if (modVersion.Length > 0) o.Append("Mod version: ").Append(modVersion).Append('\n');
        if (gameVer.Length > 0) o.Append("Game version: ").Append(gameVer).Append('\n');
        o.Append("Measured with The Royal Court ").Append(toolVer).Append(", from the game's own statistics files (eventstats.csv and onactionstats.csv).\n");
        double all = tr.All > 0 ? tr.All : 1;
        o.Append("Share of all measured script time in this session: ").Append((modTime / all * 100.0).ToString("F1", CultureInfo.InvariantCulture)).Append("% (").Append(modRuns).Append(" runs)\n");
        if (trend.Length > 0) o.Append("Share over the last sessions with the same playset (percent, oldest first): ").Append(trend).Append('\n');
        string List(TimeTable t, int[] owner, string head)
        {
            var s = new StringBuilder(); int n = 0;
            for (int i = 0; i < t.Rows.Count && i < owner.Length && n < 5; i++)
            {
                if (owner[i] != mod) continue;
                s.Append("  ").Append(++n).Append(". ").Append(t.Rows[i].Id).Append(": ").Append((t.Rows[i].Own / all * 100.0).ToString("F1", CultureInfo.InvariantCulture)).Append("% of all script time, ").Append(t.Rows[i].Count).Append(" runs\n");
            }
            return s.Length == 0 ? "" : head + "\n" + s;
        }
        o.Append(List(tr.Events, tr.EventMod, "Slowest events:"));
        o.Append(List(tr.OnActions, tr.ActionMod, "Slowest on_actions (counted for this mod only because no other mod and not the base game defines them):"));
        o.Append("A high share only means these scripts take a large part of the measured time; it is not a fault by itself. The time unit is the game's own and is not stated by it.\n");
        return o.ToString();
    }

    public static List<YearStats> ParseYearStats(string text)
    {
        var by = new SortedDictionary<int, SortedDictionary<string, double>>();
        int p = 0;
        const string tag = "[AUTOMATION_STATS][";
        while (p < text.Length)
        {
            int e = text.IndexOf('\n', p);
            if (e < 0) e = text.Length;
            int a = text.IndexOf(tag, p, e - p, StringComparison.Ordinal);
            if (a >= 0)
            {
                int n0 = a + 19, n1 = text.IndexOf(']', n0);
                int colon = n1 < 0 ? -1 : text.IndexOf("]:[", n1, StringComparison.Ordinal);
                if (colon >= 0 && colon < e)
                {
                    int v0 = colon + 3, v1 = text.IndexOf(']', v0);
                    int y0 = v1 < 0 ? -1 : text.IndexOf('[', v1);
                    int y1 = y0 < 0 ? -1 : text.IndexOf(']', y0);
                    if (y1 >= 0 && y1 <= e)
                    {
                        int year = Util.Atoi(text.Substring(y0 + 1, y1 - y0 - 1));
                        if (year > 0)
                        {
                            if (!by.TryGetValue(year, out var d)) by[year] = d = new SortedDictionary<string, double>(StringComparer.Ordinal);
                            d[text.Substring(n0, n1 - n0)] = ToDouble(text.Substring(v0, v1 - v0));
                        }
                    }
                }
            }
            p = e + 1;
        }
        var outv = new List<YearStats>();
        foreach (var kv in by) outv.Add(new YearStats { Year = kv.Key, V = kv.Value });
        return outv;
    }
}
