using System.Text;
namespace RoyalCourt.Core;

public static class Env
{
    public const string Version = "0.26.0";
    /// <summary>%APPDATA% (set by the program on start). Empty: the current folder.</summary>
    public static string AppData = "";
}

public static class Util
{
    public static readonly UTF8Encoding Utf8 = new(false);

    /// <summary>Lower-case text (full Unicode, so Cyrillic and accented names compare the way people expect).</summary>
    public static string Lower(string s) => s.ToLowerInvariant();

    /// <summary>The whole file as text, or false when it cannot be read or is larger than maxBytes (huge files are never loaded).</summary>
    public static bool ReadFile(string path, out string text, long maxBytes = 8L << 20)
    {
        text = "";
        try
        {
            var fi = new FileInfo(path);
            if (!fi.Exists || fi.Length > maxBytes) return false;
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 1 << 16);
            var buf = new byte[fs.Length];
            int got = 0;
            while (got < buf.Length) { int r = fs.Read(buf, got, buf.Length - got); if (r <= 0) break; got += r; }
            text = Utf8.GetString(buf, 0, got);
            return true;
        }
        catch { return false; }
    }
    public static bool ReadBytes(string path, out byte[] data, long maxBytes = 8L << 20)
    {
        data = Array.Empty<byte>();
        try
        {
            var fi = new FileInfo(path);
            if (!fi.Exists || fi.Length > maxBytes) return false;
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 1 << 16);
            var buf = new byte[fs.Length];
            int got = 0;
            while (got < buf.Length) { int r = fs.Read(buf, got, buf.Length - got); if (r <= 0) break; got += r; }
            data = got == buf.Length ? buf : buf[..got];
            return true;
        }
        catch { return false; }
    }

    /// <summary>The entries of a folder; a folder that vanishes or cannot be read gives what was read so far.</summary>
    public static List<FileSystemInfo> ListDir(string dir)
    {
        var v = new List<FileSystemInfo>();
        try
        {
            var di = new DirectoryInfo(dir);
            foreach (var e in di.EnumerateFileSystemInfos()) v.Add(e);
        }
        catch { }
        return v;
    }

    public static bool WriteFile(string path, string data)
    {
        try { File.WriteAllBytes(path, Utf8.GetBytes(data)); return true; } catch { return false; }
    }
    public static bool WriteFileBytes(string path, byte[] data)
    {
        try { File.WriteAllBytes(path, data); return true; } catch { return false; }
    }
    /// <summary>Writes to a temporary file and renames it over the target, so a crash never leaves a half-written file.</summary>
    public static bool WriteFileAtomic(string path, string data)
    {
        string tmp = path + ".tmp";
        if (!WriteFile(tmp, data)) return false;
        try { File.Move(tmp, path, true); return true; }
        catch { try { File.Delete(tmp); } catch { } return false; }
    }
    public static bool WriteFileAtomicBytes(string path, byte[] data)
    {
        string tmp = path + ".tmp";
        if (!WriteFileBytes(tmp, data)) return false;
        try { File.Move(tmp, path, true); return true; }
        catch { try { File.Delete(tmp); } catch { } return false; }
    }

    public static bool DirExists(string p) { try { return Directory.Exists(p); } catch { return false; } }
    public static bool FileExists(string p) { try { return File.Exists(p); } catch { return false; } }
    public static bool Exists(string p) => FileExists(p) || DirExists(p);
    public static void CreateDirs(string p) { try { Directory.CreateDirectory(p); } catch { } }
    public static string Combine(params string[] parts) => Path.Combine(parts);
    public static string FileName(string p) => Path.GetFileName(p);
    public static string Stem(string p) => Path.GetFileNameWithoutExtension(p);

    public static string Trimmed(string s)
    {
        int a = 0, b = s.Length;
        while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
        return s.Substring(a, b - a);
    }

    public static bool ReservedName(string n)
    {
        int dot = n.IndexOf('.');
        string b = Lower(dot < 0 ? n : n.Substring(0, dot));
        foreach (var r in new[] { "con", "prn", "aux", "nul" }) if (b == r) return true;
        return b.Length == 4 && (b.StartsWith("com", StringComparison.Ordinal) || b.StartsWith("lpt", StringComparison.Ordinal)) && b[3] >= '1' && b[3] <= '9';
    }

    public static string SanitizeFileName(string n)
    {
        var sb = new StringBuilder(n.Length);
        foreach (char c in n) sb.Append("\\/:*?\"<>|".IndexOf(c) >= 0 || c < 0x20 ? '_' : c);
        string s = sb.ToString();
        int st = 0;
        while (st < s.Length && s[st] == ' ') st++;
        s = s.Substring(st);
        s = s.TrimEnd(' ', '.');
        if (s.Length > 80)
        {
            int cut = 80;
            if (char.IsLowSurrogate(s[cut])) cut--;       // do not cut inside a surrogate pair
            s = s.Substring(0, cut).TrimEnd(' ', '.');
        }
        if (s.Length == 0) s = "playset";
        if (ReservedName(s)) s += "_";
        return s;
    }

    public static string FileStem(string path) => Path.GetFileNameWithoutExtension(path);

    // ---- app data ----
    public static string DataDir()
    {
        string? e = Environment.GetEnvironmentVariable("RC_DATA_DIR");
        string d = !string.IsNullOrEmpty(e) ? e : Path.Combine(string.IsNullOrEmpty(Env.AppData) ? "." : Env.AppData, "TheRoyalCourt");
        CreateDirs(d);
        return d;
    }
    public static string PlaysetsDir() { string d = Path.Combine(DataDir(), "Playsets"); CreateDirs(d); return d; }
    public static string SettingsPath() => Path.Combine(DataDir(), "settings.json");


    /// <summary>C's printf("%.{prec}g") for a finite double.</summary>
    public static string G(double d, int prec)
    {
        if (double.IsNaN(d)) return "nan";
        if (double.IsInfinity(d)) return d > 0 ? "inf" : "-inf";
        string r = d.ToString("G" + prec, System.Globalization.CultureInfo.InvariantCulture);
        int e = r.IndexOf('E');
        if (e >= 0)
        {
            string mant = r.Substring(0, e), exp = r.Substring(e + 1);
            char sign = exp[0] == '-' ? '-' : '+';
            exp = exp.TrimStart('+', '-').PadLeft(2, '0');
            r = mant + "e" + sign + exp;
        }
        return r;
    }

    /// <summary>C's atoi/atoll: optional spaces and sign, then digits; anything else ends it. Saturates instead of overflowing.</summary>
    public static long Atoll(string s)
    {
        int i = 0;
        while (i < s.Length && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == '\f' || s[i] == '\v')) i++;
        bool neg = false;
        if (i < s.Length && (s[i] == '+' || s[i] == '-')) { neg = s[i] == '-'; i++; }
        long v = 0;
        for (; i < s.Length && s[i] >= '0' && s[i] <= '9'; i++)
        {
            int dgt = s[i] - '0';
            if (v > (long.MaxValue - dgt) / 10) return neg ? long.MinValue : long.MaxValue;
            v = v * 10 + dgt;
        }
        return neg ? -v : v;
    }
    public static int Atoi(string s) { long v = Atoll(s); return (int)Math.Max(int.MinValue, Math.Min(int.MaxValue, v)); }

    /// <summary>C's strtod for plain decimal text (sign, digits, '.', exponent): the longest valid prefix; 0 when there is none. inf/nan/hex are not read.</summary>
    public static double StrToD(string s)
    {
        int i = 0, n = s.Length;
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == '\f' || s[i] == '\v')) i++;
        int st = i;
        if (i < n && (s[i] == '+' || s[i] == '-')) i++;
        int digits = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') { i++; digits++; }
        if (i < n && s[i] == '.') { i++; while (i < n && s[i] >= '0' && s[i] <= '9') { i++; digits++; } }
        if (digits == 0) return 0;
        if (i < n && (s[i] == 'e' || s[i] == 'E'))
        {
            int j = i + 1;
            if (j < n && (s[j] == '+' || s[j] == '-')) j++;
            int ed = 0;
            while (j < n && s[j] >= '0' && s[j] <= '9') { j++; ed++; }
            if (ed > 0) i = j;
        }
        return double.TryParse(s.AsSpan(st, i - st), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var d) ? d : 0;
    }

    public static int ClampInt(double d) => double.IsNaN(d) ? 0 : d > 1e8 ? 100000000 : d < -1e8 ? -100000000 : (int)d;
    public static long NowSeconds() => DateTimeOffset.UtcNow.ToUnixTimeSeconds();

}
