namespace RoyalCourt.Core;

public sealed partial class ModManager
{
    // ---- size on disk (measured in the background, remembered between runs) ----
    readonly ModSizes sizes = new();
    bool sizesLoaded;
    Thread? sizeTh; CancellationTokenSource? sizeCts;
    public const long LargeMod = 1L << 30;

    /// <summary>Bytes of the mod's files, or -1 when not measured yet (or unknown: archive, missing folder).</summary>
    public long SizeOf(string id) => sizes.Map.TryGetValue(id, out var s) ? s.bytes : -1;

    /// <summary>Total size of the enabled mods of the active playset that have been measured, and how many of the enabled mods that is.</summary>
    public (long bytes, int measured, int enabled) EnabledSize()
    {
        long sum = 0; int m = 0, e = 0;
        var ps = Active();
        if (ps == null) return (0, 0, 0);
        foreach (var pm in ps.Mods)
        {
            if (!pm.Enabled || !Info.ContainsKey(pm.Id)) continue;
            e++;
            long b = SizeOf(pm.Id);
            if (b >= 0) { sum += b; m++; }
        }
        return (sum, m, e);
    }

    /// <summary>Measures the mods that are new or changed, on a low-priority thread, and updates the Size column when done.</summary>
    void StartSizeScan()
    {
        if (!sizesLoaded) { sizesLoaded = true; sizes.Load(); }
        sizeCts?.Cancel();
        var todo = Info.Values.Where(m => m.ContentState == 1 && m.ContentDir.Length > 0).Select(m => (id: m.Id, dir: m.ContentDir, mi: m)).ToList();
        if (todo.Count == 0) return;
        var known = sizes.Map.ToDictionary(k => k.Key, k => k.Value.fp, StringComparer.Ordinal);
        var cts = sizeCts = new CancellationTokenSource();
        var token = cts.Token;
        var th = sizeTh = new Thread(() =>
        {
            Platform.KeepOutOfTheWay();
            var got = new Dictionary<string, (string fp, long bytes)>(StringComparer.Ordinal);
            int sinceFlush = 0;
            foreach (var (id, dir, mi) in todo)
            {
                if (token.IsCancellationRequested) return;
                string fp = Walk.ModFingerprint(mi);
                if (known.TryGetValue(id, out var kf) && kf == fp) continue;
                long b = ModSizes.Measure(dir, token);
                if (token.IsCancellationRequested) return;
                if (b < 0) continue;
                got[id] = (fp, b);
                if (++sinceFlush >= 12) { var part = new Dictionary<string, (string, long)>(got); got.Clear(); sinceFlush = 0; Post(() => ApplySizes(part, token)); }
            }
            if (got.Count > 0) Post(() => ApplySizes(got, token));
        }) { IsBackground = true, Name = "sizes" };
        th.Start();
    }

    void ApplySizes(Dictionary<string, (string fp, long bytes)> part, CancellationToken token)
    {
        if (token.IsCancellationRequested) return;
        foreach (var kv in part) sizes.Map[kv.Key] = kv.Value;
        sizes.Save();
        RefreshSizes();
    }

    /// <summary>Fills the Size cells of the rows from what has been measured so far.</summary>
    public void RefreshSizes()
    {
        foreach (var r in Rows) { r.SizeBytes = r.Installed ? SizeOf(r.Id) : -1; r.Size = ModSizes.Format(r.SizeBytes); }
        NotesChanged?.Invoke();
        UpdateCount();
    }

    // ---- how much is known about a mod's load order ----
    /// <summary>2 = the database has an order rule for it, 1 = its Workshop page was read and states no order (or only its type is known), 0 = not known.</summary>
    public int OrderState(ModInfo mi)
    {
        int k = Known.Find(KnownDb, mi);
        if (k >= 0)
        {
            var d = KnownDb[k];
            return d.After.Count > 0 || d.Before.Count > 0 || d.Pos != 0 || d.Rank != 0 ? 2 : 1;
        }
        return KnownChecked.Ids.Contains(Names.SteamIdOf(mi.Id)) ? 1 : 0;
    }

    // ---- quick filter ("Show") ----
    public const int QAll = 0, QOff = 1, QOn = 2, QIssues = 3, QOutdated = 4, QUnknownOrder = 5, QLarge = 6;
    public int Quick;
    public static string QuickName(int q) => q switch
    {
        QOff => Tr("Turned off"), QOn => Tr("Turned on"), QIssues => Tr("With problems or warnings"), QOutdated => Tr("May be outdated"),
        QUnknownOrder => Tr("Load order not known"), QLarge => Tr("Large (1 GB or more)"), _ => Tr("All mods")
    };
    public void SetQuick(int q) { Quick = q; Populate(); Say(q == QAll ? Tr("Showing all mods.") : Trf("Showing: {0}", QuickName(q))); }

    bool QuickMatch(Playset ps, int i)
    {
        var m = ps.Mods[i];
        bool inst = Info.TryGetValue(m.Id, out var mi);
        switch (Quick)
        {
            case QOff: return !m.Enabled;
            case QOn: return m.Enabled;
            case QIssues: return i < Issues.Length && Analyze.IssueSeverity(Issues[i]) >= 1;
            case QOutdated: return inst && ModScan.MatchGameVersion(mi!.Supported, GameVer) == VerMatch.Mismatch;
            case QUnknownOrder: return inst && OrderState(mi!) == 0;
            case QLarge: return inst && SizeOf(m.Id) >= LargeMod;
            default: return true;
        }
    }
}
