namespace RoyalCourt.Core;

// The crash helper (see crash.hpp): reads CK3's crash folders. Every file is optional and every reader tolerates garbage.

public enum CrashKind { Unknown = 0, Access, Memory, Stack, Graphics, Assert, Illegal, Divide }

public sealed class CrashFolder { public string Name = "", Path = ""; public int Y, Mo, D, H, Mi, S; }
public sealed class CrashData
{
    public CrashKind Kind = CrashKind.Unknown;
    public string Code = "";
    public List<string> Frames = new(), Meta = new(), Files = new();
    public bool HaveException, HaveMeta, LogsFromCrash;
    public string GameLog = "", DebugLog = "";
}

public static class Crash
{
    static bool Digit(char c) => c >= '0' && c <= '9';

    /// <summary>"ck3_20261009_172556" -> fields; false when the name does not have that shape</summary>
    public static bool NameParse(string name, CrashFolder f)
    {
        if (name.Length < 19 || !name.StartsWith("ck3_", StringComparison.Ordinal) || name[12] != '_') return false;
        for (int i = 4; i < 19; i++) if (i != 12 && !Digit(name[i])) return false;
        int Num(int a, int n) => int.Parse(name.AsSpan(a, n), System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture);
        f.Name = name;
        f.Y = Num(4, 4); f.Mo = Num(8, 2); f.D = Num(10, 2); f.H = Num(13, 2); f.Mi = Num(15, 2); f.S = Num(17, 2);
        return f.Mo >= 1 && f.Mo <= 12 && f.D >= 1 && f.D <= 31 && f.H < 24 && f.Mi < 60 && f.S < 61;
    }

    /// <summary>Crash folders, newest first. Folders with other names are listed after the dated ones, by name.</summary>
    public static List<CrashFolder> List(string crashesDir, int maxN = 200)
    {
        var v = new List<CrashFolder>();
        if (crashesDir.Length == 0 || !Util.DirExists(crashesDir)) return v;
        foreach (var e in Util.ListDir(crashesDir))
        {
            if (v.Count >= 20000) break;
            if (e is not DirectoryInfo) continue;
            var f = new CrashFolder();
            if (!NameParse(e.Name, f)) { f = new CrashFolder { Name = e.Name }; }
            f.Path = e.FullName;
            v.Add(f);
        }
        v.Sort((a, b) =>
        {
            bool da = a.Y > 0, db = b.Y > 0;
            if (da != db) return da ? -1 : 1;
            return string.CompareOrdinal(b.Name, a.Name);
        });
        if (v.Count > maxN) v.RemoveRange(maxN, v.Count - maxN);
        return v;
    }

    static string AsciiLower(string s) { var a = s.ToCharArray(); for (int i = 0; i < a.Length; i++) if (a[i] >= 'A' && a[i] <= 'Z') a[i] = (char)(a[i] + 32); return new string(a); }

    public static CrashKind KindOf(string text)
    {
        string t = AsciiLower(text);
        bool Has(string k) => t.Contains(k, StringComparison.Ordinal);
        if (Has("out_of_memory") || Has("out of memory") || Has("bad_alloc") || Has("std::bad_alloc") || Has("memory allocation") || Has("not enough memory")) return CrashKind.Memory;
        if (Has("stack_overflow") || Has("stack overflow")) return CrashKind.Stack;
        if (Has("device_removed") || Has("device removed") || Has("device_hung") || Has("d3d") || Has("dxgi") || Has("vulkan") || Has("nvwgf") || Has("atidxx") || Has("nvoglv")) return CrashKind.Graphics;
        if (Has("illegal_instruction") || Has("privileged_instruction")) return CrashKind.Illegal;
        if (Has("divide_by_zero") || Has("int_divide")) return CrashKind.Divide;
        if (Has("assert")) return CrashKind.Assert;
        if (Has("access_violation") || Has("in_page_error") || Has("array_bounds")) return CrashKind.Access;
        return CrashKind.Unknown;
    }

    public static void ParseException(string text, CrashData o)
    {
        if (text.Length == 0) return;
        o.HaveException = true;
        o.Kind = KindOf(text);
        int pos = 0, lines = 0;
        while (pos < text.Length && lines < 400)
        {
            int nl = text.IndexOf('\n', pos);
            if (nl < 0) nl = text.Length;
            string line = text.Substring(pos, nl - pos);
            pos = nl + 1; lines++;
            line = line.TrimEnd('\r', ' ').TrimStart(' ', '\t');
            if (line.Length == 0 || line.Length > 300) continue;
            if (o.Code.Length == 0)
            {
                int k = line.IndexOf("EXCEPTION_", StringComparison.Ordinal);
                if (k >= 0)
                {
                    int e = k;
                    while (e < line.Length && ((line[e] >= 'A' && line[e] <= 'Z') || line[e] == '_' || Digit(line[e]))) e++;
                    o.Code = line.Substring(k, e - k);
                }
            }
            string low = AsciiLower(line);
            bool frame = low.Contains(".dll", StringComparison.Ordinal) || low.Contains(".exe", StringComparison.Ordinal) || low.Contains("::", StringComparison.Ordinal) || low.Contains("0x", StringComparison.Ordinal);
            if (frame && o.Frames.Count < 8 && !line.Contains("EXCEPTION_", StringComparison.Ordinal)) o.Frames.Add(line);
        }
        if (o.Code.Length == 0)
        {
            int e = text.IndexOf('\n');
            string first = text.Substring(0, e < 0 ? text.Length : e).TrimEnd('\r', ' ');
            if (first.Length > 160) first = first[..160];
            o.Code = first;
        }
    }

    public static void ParseMeta(string text, CrashData o)
    {
        if (text.Length == 0) return;
        o.HaveMeta = true;
        int pos = 0, lines = 0;
        while (pos < text.Length && lines < 300 && o.Meta.Count < 8)
        {
            int nl = text.IndexOf('\n', pos);
            if (nl < 0) nl = text.Length;
            string line = text.Substring(pos, nl - pos);
            pos = nl + 1; lines++;
            line = line.TrimEnd('\r', ' ').TrimStart(' ');
            int c = line.IndexOf(':');
            if (c <= 0 || line.Length > 200) continue;
            string key = AsciiLower(line[..c]);
            if (key.Contains("version", StringComparison.Ordinal) || key.Contains("build", StringComparison.Ordinal) || key.Contains("checksum", StringComparison.Ordinal)
                || key == "platform" || key == "os" || key == "gpu" || key == "cpu") o.Meta.Add(line);
        }
    }

    /// <summary>Reads what is in a crash folder (and one level of subfolders such as logs/). maxLog bounds each log file read.</summary>
    public static CrashData Read(string folder, int maxLog = 4 << 20)
    {
        var d = new CrashData();
        if (!Util.DirExists(folder)) return d;
        var found = new Dictionary<string, string>(StringComparer.Ordinal);
        void Scan(string dir, int depth)
        {
            int n = 0;
            foreach (var e in Util.ListDir(dir))
            {
                if (n++ >= 500) break;
                string nm = e.Name;
                if (e is DirectoryInfo) { if (depth < 2) Scan(e.FullName, depth + 1); continue; }
                string lo = AsciiLower(nm);
                found.TryAdd(lo, e.FullName);
                if (depth == 0 || nm.Length < 60) { if (d.Files.Count < 40) d.Files.Add(nm); }
            }
        }
        Scan(folder, 0);
        string ReadOne(string name, bool tail)
        {
            if (!found.TryGetValue(name, out var path)) return "";
            try
            {
                using var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                long sz = f.Length;
                if (sz <= 0) return "";
                long start = (tail && sz > maxLog) ? sz - maxLog : 0;
                int len = (int)Math.Min(sz - start, maxLog);
                f.Seek(start, SeekOrigin.Begin);
                var buf = new byte[len];
                int got = 0;
                while (got < len) { int r = f.Read(buf, got, len - got); if (r <= 0) break; got += r; }
                string s = Util.Utf8.GetString(buf, 0, got);
                if (tail && start > 0) { int nl = s.IndexOf('\n'); if (nl >= 0) s = s[(nl + 1)..]; }
                return s.Replace("\0", "");
            }
            catch { return ""; }
        }
        ParseException(ReadOne("exception.txt", false), d);
        ParseMeta(ReadOne("meta.yml", false), d);
        d.GameLog = ReadOne("game.log", true);
        string old = ReadOne("error.log", true);
        if (old.Length > 0) { if (d.GameLog.Length > 0) d.GameLog += "\n"; d.GameLog += old; }
        d.DebugLog = ReadOne("debug.log", false);
        d.LogsFromCrash = d.GameLog.Length > 0 || d.DebugLog.Length > 0;
        if (d.Kind == CrashKind.Unknown && !d.HaveException)
        {
            string tailTxt = d.GameLog.Length > 20000 ? d.GameLog[^20000..] : d.GameLog;
            var k = KindOf(tailTxt);
            if (k == CrashKind.Memory || k == CrashKind.Stack) d.Kind = k;
        }
        return d;
    }

    /// <summary>The last maxLines lines of a log as plain text.</summary>
    public static string TailLines(string text, int maxLines)
    {
        int pos = text.Length, n = 0;
        while (pos > 0 && n <= maxLines)
        {
            int p = text.LastIndexOf('\n', pos - 1);
            if (p < 0) { pos = 0; break; }
            pos = p; n++;
            if (pos == 0) break;
        }
        return text.Substring(pos == 0 ? 0 : pos + 1);
    }

    /// <summary>What the game was doing at the end: parse the last lines of the log.</summary>
    public static LogParse LastMessages(string gameLog, int tailBytes = 24000)
    {
        string t = gameLog.Length > tailBytes ? gameLog[^tailBytes..] : gameLog;
        if (t.Length < gameLog.Length) { int nl = t.IndexOf('\n'); if (nl >= 0) t = t[(nl + 1)..]; }
        return GameLog.Parse(t, true, 400);
    }

    /// <summary>How many of the entries from the end (the last window) point at each mod, newest weighing more. Returns mod index -> score.</summary>
    public static SortedDictionary<int, double> Scores(LogReport r, int window = 60)
    {
        var sc = new SortedDictionary<int, double>();
        int n = r.Parse.Entries.Count;
        int from = n > window ? n - window : 0;
        for (int k = from; k < n; k++)
        {
            if (r.ModOf[k] < 0) continue;
            var e = r.Parse.Entries[k];
            double recency = 1.0 + 2.0 * (k - from) / Math.Max(1, n - from);
            sc[r.ModOf[k]] = sc.GetValueOrDefault(r.ModOf[k]) + (e.Level == 'E' ? 2.0 : 0.5) * recency;
        }
        return sc;
    }

    public static string KindName(CrashKind k) => k switch
    {
        CrashKind.Access => K("The game touched memory it should not (access violation)"),
        CrashKind.Memory => K("The game ran out of memory"),
        CrashKind.Stack => K("The game's call stack overflowed"),
        CrashKind.Graphics => K("A graphics driver or graphics-card error"),
        CrashKind.Assert => K("The game stopped itself on an internal check (assertion)"),
        CrashKind.Illegal => K("A processor instruction the computer could not run"),
        CrashKind.Divide => K("The game divided by zero"),
        _ => K("A crash with no recognised cause"),
    };
    public static string KindAdvice(CrashKind k) => k switch
    {
        CrashKind.Access => K("This is the most common crash with mods. It is usually a script or data file the game cannot handle: a mod that is out of date for this game version, two mods that overwrite the same files, or a mod loaded in the wrong order. Check the suspects below, then try turning off the most recently added or updated mods first."),
        CrashKind.Memory => K("A big playset on a long game can use more memory than the computer has. Close other programs, turn off graphics-heavy and map mods first, and restart the game now and then in a long campaign."),
        CrashKind.Stack => K("This is almost always a mod that makes events or effects call each other forever. Look at the suspects below, and at mods that changed recently."),
        CrashKind.Graphics => K("This points at the graphics driver or card rather than a script mod. Update the graphics driver, and turn off mods that add many or large textures, 3D models or shaders."),
        CrashKind.Assert => K("The game found data that does not make sense. A mod that is out of date for this game version is the usual reason."),
        CrashKind.Illegal => K("This usually means damaged game files or an unstable overclock. Verify the game files in Steam."),
        CrashKind.Divide => K("A mod gave the game a value of zero where it must not. Check the suspects below."),
        _ => K("The crash files do not say what went wrong. The suspects below come from what the game wrote to its log just before it stopped."),
    };
}
