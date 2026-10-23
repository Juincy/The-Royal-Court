using System.Collections.Concurrent;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
namespace RoyalCourt.Ui;

/// <summary>The mod pictures of the list: each mod's own thumbnail.png, read from disk (never from the network) one at a time off the UI thread,
/// shrunk to a few kilobytes and kept. The newest request is served first, so scrolling shows the visible rows first.</summary>
public static class Thumbs
{
    const int Px = 40;                     // twice the drawn size, sharp on high-DPI screens
    static readonly Dictionary<string, Bitmap?> cache = new(StringComparer.Ordinal);   // UI thread only
    static readonly ConcurrentStack<string> todo = new();
    static int running;
    public static event Action? Loaded;

    /// <summary>The picture if it is loaded; otherwise null, and a load is started once. Loaded fires when new pictures arrive.</summary>
    public static Bitmap? Get(string path)
    {
        if (path.Length == 0) return null;
        if (cache.TryGetValue(path, out var b)) return b;
        cache[path] = null;
        todo.Push(path);
        if (Interlocked.Exchange(ref running, 1) == 0) { var t = new Thread(Work) { IsBackground = true, Priority = ThreadPriority.BelowNormal, Name = "thumbs" }; t.Start(); }
        return null;
    }
    public static void Clear() { cache.Clear(); todo.Clear(); }

    static void Work()
    {
        var done = new List<(string, Bitmap)>();
        DateTime flush = DateTime.UtcNow;
        while (true)
        {
            if (!todo.TryPop(out var path))
            {
                if (done.Count > 0) Flush(done);
                Interlocked.Exchange(ref running, 0);
                if (todo.IsEmpty || Interlocked.Exchange(ref running, 1) == 1) return;   // something arrived while stopping: carry on
                continue;
            }
            try
            {
                var fi = new FileInfo(path);
                if (!fi.Exists || fi.Length > 12L << 20 || fi.Length < 16) continue;
                // the shrunk copy is kept on disk: a big Workshop picture is decoded once ever, not at every start (decoding big images makes garbage the UI would pay for)
                string cf = CacheFile(path, fi);
                Bitmap? bmp = null;
                if (File.Exists(cf)) { try { using var cs = File.OpenRead(cf); bmp = new Bitmap(cs); } catch { bmp = null; } }
                if (bmp == null)
                {
                    using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                    bmp = Bitmap.DecodeToWidth(fs, Px, BitmapInterpolationMode.MediumQuality);
                    try { Directory.CreateDirectory(Path.GetDirectoryName(cf)!); using var os = File.Create(cf + ".tmp"); bmp.Save(os); os.Close(); File.Move(cf + ".tmp", cf, true); } catch { }
                    Thread.Sleep(6);   // leave the processor to the window between big pictures
                }
                done.Add((path, bmp));
            }
            catch { }
            if (done.Count >= 8 || (DateTime.UtcNow - flush).TotalMilliseconds > 150 && done.Count > 0) { Flush(done); flush = DateTime.UtcNow; }
        }
    }
    static string CacheFile(string path, FileInfo fi)
    {
        var h = System.Security.Cryptography.SHA1.HashData(System.Text.Encoding.UTF8.GetBytes(path.ToLowerInvariant() + "|" + fi.Length + "|" + fi.LastWriteTimeUtc.Ticks + "|" + Px));
        return Path.Combine(RoyalCourt.Core.Util.DataDir(), "Thumbs", Convert.ToHexString(h, 0, 10) + ".png");
    }
    static void Flush(List<(string, Bitmap)> done)
    {
        var batch = done.ToList(); done.Clear();
        Dispatcher.UIThread.Post(() => { foreach (var (p, b) in batch) cache[p] = b; Loaded?.Invoke(); });
    }
}
