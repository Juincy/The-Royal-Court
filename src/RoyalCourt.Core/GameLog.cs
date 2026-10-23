using System.Text;
namespace RoyalCourt.Core;

// Reading the game's own log and tying each message to the mod that caused it (see gamelog.hpp).

public sealed class LogEntry
{
    public char Level = 'E';
    public string Src = "", Msg = "", Detail = "", File = "";
    public int Line, Count = 1;
    public List<string> Names = new();
}
public sealed class LogParse
{
    public List<LogEntry> Entries = new();
    public int Lines, Errors, Warnings, Dropped;
}
public sealed class LoadedMod { public string Name = "", Id = ""; public bool Enabled = true; }
public static class LogHow { public const int None = 0, File = 1, Name = 2, Base = 3; }
public sealed class LogPerMod { public int Mod = -1; public long Errors, Warnings, Entries; }
public sealed class LogReport
{
    public LogParse Parse = new();
    public int[] ModOf = Array.Empty<int>();
    public int[] How = Array.Empty<int>();
    public List<LogPerMod> PerMod = new();
    public LogPerMod Base = new(), Unplaced = new();
}

public static class GameLog
{
    public static bool HeaderParse(string s, out char lvl, out string src, out string msg)
    {
        lvl = ' '; src = ""; msg = "";
        if (s.Length < 12 || s[0] != '[') return false;
        int p = s.IndexOf(']');
        if (p < 2 || p > 12) return false;
        for (int i = 1; i < p; i++) if (!(s[i] >= '0' && s[i] <= '9') && s[i] != ':') return false;
        if (p + 4 >= s.Length || s[p + 1] != '[' || s[p + 3] != ']' || s[p + 4] != '[') return false;
        lvl = s[p + 2];
        int q = s.IndexOf(']', p + 5);
        if (q < 0) return false;
        src = s.Substring(p + 5, q - p - 5);
        msg = s.Substring(q + 1);
        if (msg.StartsWith(": ", StringComparison.Ordinal)) msg = msg[2..]; else if (msg.Length > 0 && msg[0] == ':') msg = msg[1..];
        return true;
    }

    public static void Locate(string text, LogEntry e)
    {
        int f = text.IndexOf("file: ", StringComparison.Ordinal);
        if (f < 0) return;
        int b = f + 6;
        int l = text.IndexOf(" line: ", b, StringComparison.Ordinal);
        int end = l >= 0 ? l : text.IndexOfAny(new[] { ' ', '\'', '"', ')', '(' }, b);
        if (end < 0) end = text.Length;
        if (end > b && end - b < 400)
        {
            string path = text.Substring(b, end - b).Replace('\\', '/');
            e.File = Walk.Norm(path);
            if (l >= 0)
            {
                int d = l + 7, n = 0, digits = 0;
                while (d < text.Length && text[d] >= '0' && text[d] <= '9' && digits < 9) { n = n * 10 + (text[d] - '0'); d++; digits++; }
                e.Line = n;
            }
        }
    }

    public static void Names(string text, List<string> outv)
    {
        int i = 0;
        while (i < text.Length && outv.Count < 8)
        {
            int a = text.IndexOf('\'', i);
            if (a < 0) break;
            int b = text.IndexOf('\'', a + 1);
            if (b < 0) break;
            string n = text.Substring(a + 1, b - a - 1);
            if (n.Length >= 3 && n.Length <= 80 && n.IndexOf(' ') < 0) outv.Add(n);
            i = b + 1;
        }
        int o = text.LastIndexOf(" (", StringComparison.Ordinal);
        if (o >= 0 && text[^1] == ')')
        {
            string n = text.Substring(o + 2, text.Length - o - 3);
            int c = n.IndexOf(':');
            if (c >= 0) n = n[..c];
            if (n.Length >= 3 && n.Length <= 80 && n.IndexOf(' ') < 0) outv.Add(n);
        }
    }

    public static LogParse Parse(string text, bool withWarnings = true, int maxEntries = 20000)
    {
        var r = new LogParse();
        var seen = new Dictionary<string, int>(StringComparer.Ordinal);
        LogEntry cur = new(); bool have = false;
        void Flush()
        {
            if (!have) return;
            have = false;
            if (cur.Level == 'E') r.Errors++; else r.Warnings++;
            string whole = cur.Msg + (cur.Detail.Length == 0 ? "" : " | " + cur.Detail);
            Locate(whole, cur);
            Names(cur.Msg, cur.Names);
            if (cur.Detail.Length > 0) Names(cur.Detail, cur.Names);
            string key = cur.Level + "|" + cur.Src + "|" + whole;
            if (seen.TryGetValue(key, out int ix)) { r.Entries[ix].Count++; return; }
            if (r.Entries.Count >= maxEntries) { r.Dropped++; return; }
            seen[key] = r.Entries.Count;
            r.Entries.Add(cur);
        }
        int pos = 0, n = text.Length;
        while (pos < n)
        {
            int nl = text.IndexOf('\n', pos);
            if (nl < 0) nl = n;
            int end = nl;
            if (end > pos && text[end - 1] == '\r') end--;
            string line = text.Substring(pos, end - pos);
            pos = nl + 1;
            r.Lines++;
            if (HeaderParse(line, out char lvl, out string src, out string msg))
            {
                Flush();
                if (lvl == 'E' || (withWarnings && lvl == 'W')) { cur = new LogEntry { Level = lvl, Src = src, Msg = msg }; have = true; }
            }
            else if (have && line.Length > 0 && (line[0] == ' ' || line[0] == '\t'))
            {
                int a = 0; while (a < line.Length && (line[a] == ' ' || line[a] == '\t')) a++;
                if (a < line.Length && cur.Detail.Length < 1500) { if (cur.Detail.Length > 0) cur.Detail += " | "; cur.Detail += line.Substring(a); }
            }
            else Flush();
        }
        Flush();
        return r;
    }

    /// <summary>The mods the game says it loaded: debug.log has one "Name|mod/ugc_123.mod|Enabled" line per mod.</summary>
    public static List<LoadedMod> ParseLoadedMods(string text)
    {
        var v = new List<LoadedMod>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        int pos = 0;
        while (pos < text.Length && v.Count < 5000)
        {
            int nl = text.IndexOf('\n', pos);
            if (nl < 0) nl = text.Length;
            string line = text.Substring(pos, nl - pos).TrimEnd('\r', ' ');
            pos = nl + 1;
            int a = line.IndexOf("|mod/", StringComparison.Ordinal);
            if (a <= 0 || line.Length > 600) continue;
            int b = line.IndexOf('|', a + 5);
            if (b < 0) continue;
            string id = line.Substring(a + 5, b - a - 5), state = line.Substring(b + 1);
            if (!Playsets.ValidID(id) || (state != "Enabled" && state != "Disabled")) continue;
            if (!seen.Add(id)) continue;
            v.Add(new LoadedMod { Name = line[..a], Id = id, Enabled = state == "Enabled" });
        }
        return v;
    }

    public static LogReport Attribute(LogParse parse, Playset ps, IReadOnlyDictionary<string, ModFiles> files, IReadOnlyDictionary<string, ModDefs> defs, VanillaIndex? van)
    {
        var r = new LogReport { Parse = parse };
        var es = r.Parse.Entries;
        r.ModOf = new int[es.Count]; Array.Fill(r.ModOf, -1);
        r.How = new int[es.Count];
        var wantFiles = new HashSet<string>(StringComparer.Ordinal);
        foreach (var e in es) if (e.File.Length > 0) wantFiles.Add(e.File);
        var fileOwner = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < ps.Mods.Count && wantFiles.Count > 0; i++)
        {
            if (!ps.Mods[i].Enabled || !files.TryGetValue(ps.Mods[i].Id, out var f)) continue;
            foreach (var rel in f.Files) if (wantFiles.Contains(rel)) fileOwner[rel] = i;
        }
        var wantNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (var e in es)
        {
            bool placed = e.File.Length > 0 && (fileOwner.ContainsKey(e.File) || (van != null && van.Has(e.File)));
            if (!placed) foreach (var n in e.Names) wantNames.Add(n);
        }
        var nameOwner = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < ps.Mods.Count && wantNames.Count > 0; i++)
        {
            if (!ps.Mods[i].Enabled || !defs.TryGetValue(ps.Mods[i].Id, out var d)) continue;
            foreach (var de in d.Defs)
            {
                string nm = d.NameOf(de);
                if (wantNames.Contains(nm)) nameOwner[nm] = i;
            }
        }
        var pm = new SortedDictionary<int, LogPerMod>();
        static void Add(LogPerMod m, LogEntry e) { if (e.Level == 'E') m.Errors += e.Count; else m.Warnings += e.Count; m.Entries++; }
        for (int k = 0; k < es.Count; k++)
        {
            var e = es[k];
            if (e.File.Length > 0)
            {
                if (fileOwner.TryGetValue(e.File, out int o)) { r.ModOf[k] = o; r.How[k] = LogHow.File; }
                else if (van != null && van.Has(e.File)) r.How[k] = LogHow.Base;
            }
            if (r.How[k] == LogHow.None)
                foreach (var n in e.Names) if (nameOwner.TryGetValue(n, out int o)) { r.ModOf[k] = o; r.How[k] = LogHow.Name; break; }
            if (r.ModOf[k] >= 0) { if (!pm.TryGetValue(r.ModOf[k], out var m)) pm[r.ModOf[k]] = m = new LogPerMod { Mod = r.ModOf[k] }; Add(m, e); }
            else if (r.How[k] == LogHow.Base) Add(r.Base, e);
            else Add(r.Unplaced, e);
        }
        r.PerMod = pm.Values.OrderByDescending(m => m.Errors).ThenByDescending(m => m.Warnings).ToList();
        return r;
    }

    /// <summary>A short, shareable text for a mod author: the messages that point at one mod.</summary>
    public static string ModReport(LogReport r, int mod, string modName, int maxLines = 60)
    {
        var o = new StringBuilder("Messages from the game log that point at \"" + modName + "\":\n");
        int shown = 0, total = 0;
        for (int k = 0; k < r.Parse.Entries.Count; k++)
        {
            if (r.ModOf[k] != mod) continue;
            total++;
            if (shown >= maxLines) continue;
            var e = r.Parse.Entries[k];
            o.Append(e.Level == 'E' ? "[error] " : "[warning] ").Append(e.Msg);
            if (e.Detail.Length > 0) o.Append(" | ").Append(e.Detail);
            if (e.Count > 1) o.Append("  (x").Append(e.Count).Append(')');
            o.Append('\n');
            shown++;
        }
        if (total > shown) o.Append("... and ").Append(total - shown).Append(" more\n");
        return o.ToString();
    }
}
