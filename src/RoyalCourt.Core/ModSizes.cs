using System.Text;
namespace RoyalCourt.Core;

/// <summary>Size of each mod's files on disk. Measured on a low-priority background thread (one folder listing per mod, no file is read) and
/// remembered in the data folder against the mod's fingerprint, so only new or changed mods are measured again.</summary>
public sealed class ModSizes
{
    public readonly Dictionary<string, (string fp, long bytes)> Map = new(StringComparer.Ordinal);
    static string FilePath() => Path.Combine(Util.DataDir(), "sizes.txt");

    public void Load()
    {
        Map.Clear();
        if (!Util.ReadFile(FilePath(), out var t)) return;
        foreach (var line in t.Split('\n'))
        {
            var p = line.TrimEnd('\r').Split('\t');
            if (p.Length == 3 && long.TryParse(p[2], out var b) && b >= 0) Map[p[0]] = (p[1], b);
        }
    }

    public void Save()
    {
        var sb = new StringBuilder();
        foreach (var kv in Map.OrderBy(k => k.Key, StringComparer.Ordinal)) sb.Append(kv.Key).Append('\t').Append(kv.Value.fp).Append('\t').Append(kv.Value.bytes).Append('\n');
        Util.WriteFileAtomic(FilePath(), sb.ToString());
    }

    /// <summary>Sum of the file sizes below a folder, or -1 when it could not be listed completely.</summary>
    public static long Measure(string dir, CancellationToken cancel = default)
    {
        if (dir.Length == 0) return -1;
        ulong sum = 0;
        var rc = Walk.WalkFiles(dir, (_, size, _) => sum += size, cancel);
        return rc == WalkResult.Ok ? (long)sum : -1;
    }

    /// <summary>"1.2 GB", "340 MB", "12 KB"; empty for an unknown size.</summary>
    public static string Format(long b)
    {
        if (b < 0) return "";
        if (b >= 1L << 30) return (b / (double)(1L << 30)).ToString(b >= 10L << 30 ? "0" : "0.0", System.Globalization.CultureInfo.InvariantCulture) + " GB";
        if (b >= 1L << 20) return (b / (double)(1L << 20)).ToString(b >= 10L << 20 ? "0" : "0.0", System.Globalization.CultureInfo.InvariantCulture) + " MB";
        if (b >= 1L << 10) return (b >> 10) + " KB";
        return b + " B";
    }
}
