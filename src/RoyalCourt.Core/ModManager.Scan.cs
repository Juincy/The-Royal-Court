namespace RoyalCourt.Core;

// Background work: the file lists of the enabled mods are read on a worker thread, and the conflict report is computed from them.
public sealed partial class ModManager
{
    public Dictionary<string, ModFiles> FileIndex = new(StringComparer.Ordinal);   // mod id -> files (kept for the session; rebuilt when a mod's folder changes)
    public Dictionary<string, ModDefs> DefIndex = new(StringComparer.Ordinal);     // mod id -> script definitions (events, common/ objects, localization keys)
    public Scripts.ScriptReport Script = new();
    public ConflictReport Conf = new();
    string confSig = "";                                   // which playset state Conf was computed for
    public bool Scanning;
    public int ScanDone, ScanTotal;
    public VanillaIndex? Van;                              // the base game's file list (null: not read yet, or the game folder was not found)
    public bool VanillaFailed;                             // reading it failed or there is no game folder: not tried again until the game install or version changes
    long lastDeepVerify;                                   // when every enabled mod was last compared file by file with what we know (Environment.TickCount64)
    readonly Dictionary<string, string> scanFailed = new(StringComparer.Ordinal);   // mod id -> fingerprint of a folder that could not be read completely
    bool prunedCache;
    public bool SinceShown;
    public bool ForceAsyncConf;                            // test hook (RC_ASYNC_CONF=1): always use the worker thread
    public const int ConfAsyncFiles = 150000;              // above this many files in the enabled mods the report is computed on a worker thread

    sealed class ScanItem { public string Id = "", Dir = "", Fp = "", KnownDeep = ""; }   // KnownDeep: set when the mod is already indexed and only needs to be checked for changes
    sealed class ScanJob
    {
        public List<ScanItem> Items = new();
        public ModFiles[] Out = Array.Empty<ModFiles>();
        public ModDefs[] Defs = Array.Empty<ModDefs>();
        public bool[] Unchanged = Array.Empty<bool>(), FromCache = Array.Empty<bool>();
        public string CacheDir = "", GameExe = "", GameVer = "";
        public bool WantVanilla, Quiet;
        public VanillaIndex? Vanilla;
        public bool VanillaFromCache;
        public long Ms;
        public CancellationTokenSource Cts = new();
        public Thread? Th;
    }
    ScanJob? job;

    sealed class ConfJob
    {
        public int Id;
        public Playset Ps = new();
        public Dictionary<string, ModInfo> Info = new();
        public VanillaIndex? Van;
        public string Sig = "";
        public ConflictReport Conf = new();
        public Scripts.ScriptReport Script = new();
        public CancellationTokenSource Cts = new();
        public Thread? Th;
        public long Ms;
        public string Err = "";
    }
    ConfJob? confJob;
    int confJobSeq;

    string PlaysetSignature(Playset ps)
    {
        var s = new System.Text.StringBuilder(ps.Name);
        foreach (var m in ps.Mods) if (m.Enabled) { s.Append('\n').Append(m.Id); }
        s.Append("\n#"); if (Van != null) s.Append(Van.Key);      // the report also depends on the game's file list
        return s.ToString();
    }

    bool VanillaPending() => Van == null && !VanillaFailed && GameExeUsed.Length > 0 && Scanning;

    /// <summary>Stops the conflict worker and waits for it: it reads the file index, so everything that changes the index calls this first.</summary>
    public void ConfQuiesce()
    {
        var j = confJob;
        if (j == null) return;
        j.Cts.Cancel();
        j.Th?.Join();
        confJob = null;
        confSig = ""; Conf = new(); Script = new();
    }

    /// <summary>Rebuilds the conflict report from the cached file lists when the enabled mods or their order changed (no disk access).
    /// Small playsets are computed on the spot. A big one is computed on a worker thread so the window never stops answering; while it runs,
    /// the old report is dropped (its positions would be wrong for the new order).</summary>
    public void EnsureConflicts()
    {
        var ps = Active();
        if (ps == null) return;
        string sig = PlaysetSignature(ps);
        if (sig == confSig && (Conf.Valid || Scanning || confJob != null)) return;
        if (confJob != null)   // a result for an older state is useless: stop it, and ask again when it has ended
        {
            confJob.Cts.Cancel();
            confSig = ""; Conf = new(); Script = new();
            return;
        }
        bool ready = !VanillaPending();
        if (ready)
            foreach (var m in ps.Mods)
            {
                if (!m.Enabled) continue;
                if (!Info.TryGetValue(m.Id, out var mi) || mi.ContentState != 1) continue;
                if (!FileIndex.TryGetValue(m.Id, out var f) || !f.Complete)
                {
                    if (scanFailed.TryGetValue(m.Id, out var bad) && bad == Walk.ModFingerprint(mi)) continue;   // unreadable folder: skipped, not waited for
                    ready = false; break;
                }
            }
        bool defsReady = ready;
        if (ready)
            foreach (var m in ps.Mods)
            {
                if (!m.Enabled) continue;
                if (!Info.TryGetValue(m.Id, out var mi) || mi.ContentState != 1) continue;
                if (!DefIndex.TryGetValue(m.Id, out var d) || !d.Complete)
                {
                    if (scanFailed.TryGetValue(m.Id, out var bad) && bad == Walk.ModFingerprint(mi)) continue;
                    defsReady = false; break;
                }
            }
        confSig = sig;
        if (!ready) { Conf = new(); Script = new(); ConflictsChanged?.Invoke(); return; }
        long total = 0;
        foreach (var m in ps.Mods) if (m.Enabled && FileIndex.TryGetValue(m.Id, out var f)) total += f.Files.Count;
        if (total > ConfAsyncFiles || ForceAsyncConf)
        {
            var j = new ConfJob { Id = ++confJobSeq, Ps = ps.Clone(), Info = new Dictionary<string, ModInfo>(Info, StringComparer.Ordinal), Van = Van, Sig = sig };
            Log.Line("conflicts: " + total + " files, computing on a worker thread");
            var token = j.Cts.Token;
            var fileIdx = FileIndex; var defIdx = DefIndex;
            j.Th = new Thread(() =>
            {
                Platform.KeepOutOfTheWay();
                var sw = System.Diagnostics.Stopwatch.StartNew();
                try
                {
                    j.Conf = Conflicts.Find(j.Ps, j.Info, fileIdx, j.Van, token);
                    if (!token.IsCancellationRequested) j.Script = Scripts.FindScriptConflicts(j.Ps, j.Info, defIdx);
                }
                catch (Exception e) { j.Conf = new(); j.Script = new(); j.Err = e.Message; }
                j.Ms = sw.ElapsedMilliseconds;
                int id = j.Id;
                Post(() => OnConfDone(id));
            }) { IsBackground = true, Name = "conflicts" };
            confJob = j;
            j.Th.Start();
            Conf = new(); Script = new(); ConflictsChanged?.Invoke();
            return;
        }
        Conf = Conflicts.Find(ps, Info, FileIndex, Van);
        Script = defsReady ? Scripts.FindScriptConflicts(ps, Info, DefIndex) : new Scripts.ScriptReport();
        ConflictsChanged?.Invoke();
    }

    void OnConfDone(int id)
    {
        var j = confJob;
        if (j == null || j.Id != id) return;
        j.Th?.Join();
        confJob = null;
        bool use = !j.Cts.IsCancellationRequested && j.Conf.Valid;
        if (j.Err.Length > 0)   // the worker failed: compute here instead (and say so), never start another worker for this state
        {
            Log.Line("conflict worker failed (" + j.Err + "), computing on the main thread");
            var ps = Active();
            if (ps != null && j.Sig == PlaysetSignature(ps))
            {
                Conf = Conflicts.Find(ps, Info, FileIndex, Van);
                Script = Scripts.FindScriptConflicts(ps, Info, DefIndex);
                confSig = j.Sig;
            }
            else confSig = "";
            RefreshNotes(); ConflictsChanged?.Invoke();
            return;
        }
        if (use)
        {
            Conf = j.Conf; Script = j.Script; confSig = j.Sig;
            Log.Line($"conflicts: {Conf.Files.Count} files, {Script.Items.Count} script overlaps, computed in {j.Ms} ms on the worker thread");
        }
        else { confSig = ""; Log.Line("conflicts: worker result dropped (" + (j.Cts.IsCancellationRequested ? "cancelled" : "invalid") + ")"); }
        RefreshNotes();      // asks again when the result was thrown away
        ConflictsChanged?.Invoke();
    }

    /// <summary>Starts a background scan for enabled mods that have no file list yet, and (when verify is set) checks the others for changes.
    /// A mod that is already indexed is only listed again, not read, unless something in it changed.</summary>
    public void MaybeScan(bool verify)
    {
        if (job != null) return;
        var ps = Active();
        if (ps == null) return;
        long now = Environment.TickCount64;
        bool deepDue = verify && now - lastDeepVerify > 120000;   // Steam can replace files deep inside a mod without the mod's folder showing it
        var jb = new ScanJob();
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled) continue;
            if (!Info.TryGetValue(m.Id, out var mi) || mi.ContentState != 1) continue;
            string fp = Walk.ModFingerprint(mi);
            if (scanFailed.TryGetValue(m.Id, out var bad) && bad == fp) continue;   // failed before and nothing changed since
            FileIndex.TryGetValue(m.Id, out var f);
            DefIndex.TryGetValue(m.Id, out var d);
            bool have = f != null && f.Complete && d != null && d.Complete;
            if (have)
            {
                if (!verify || (f!.Fingerprint == fp && !deepDue)) continue;
                jb.Items.Add(new ScanItem { Id = m.Id, Dir = mi.ContentDir, Fp = fp, KnownDeep = f.Deep });
            }
            else jb.Items.Add(new ScanItem { Id = m.Id, Dir = mi.ContentDir, Fp = fp });
        }
        bool wantVan = Van == null && !VanillaFailed && GameExeUsed.Length > 0;
        if (jb.Items.Count == 0 && !wantVan) { EnsureConflicts(); return; }
        bool quiet = !wantVan;
        foreach (var it in jb.Items) if (it.KnownDeep.Length == 0) quiet = false;   // pure change checks run silently
        jb.CacheDir = Path.Combine(Util.DataDir(), "cache");
        jb.GameExe = GameExeUsed; jb.GameVer = GameVer;
        jb.WantVanilla = wantVan; jb.Quiet = quiet;
        job = jb;
        if (deepDue) lastDeepVerify = now;
        Scanning = !quiet; ScanDone = 0; ScanTotal = jb.Items.Count;
        var token = jb.Cts.Token;
        jb.Th = new Thread(() => ScanWorker(jb, token)) { IsBackground = true, Name = "scan" };
        jb.Th.Start();
        ConflictsChanged?.Invoke();
        UpdateCount();
    }

    void ScanWorker(ScanJob j, CancellationToken token)
    {
        Platform.KeepOutOfTheWay();
        var sw = System.Diagnostics.Stopwatch.StartNew();
        int n = j.Items.Count;
        j.Out = new ModFiles[n]; j.Defs = new ModDefs[n];
        for (int i = 0; i < n; i++) { j.Out[i] = new ModFiles(); j.Defs[i] = new ModDefs(); }
        j.Unchanged = new bool[n]; j.FromCache = new bool[n];
        try
        {
            if (j.WantVanilla && !token.IsCancellationRequested)
            {
                var fc = new bool[1];
                j.Vanilla = Core.Vanilla.LoadOrIndex(j.CacheDir, j.GameExe, j.GameVer, token, fc);
                j.VanillaFromCache = fc[0];
            }
            for (int i = 0; i < n && !token.IsCancellationRequested; i++)
            {
                var it = j.Items[i];
                bool done = false;
                if (it.KnownDeep.Length > 0)   // already indexed: only look at whether anything changed (list the folder, no file is read)
                {
                    var mf = Walk.IndexModFiles(it.Dir, token);
                    if (mf.Complete && mf.Deep == it.KnownDeep) { j.Unchanged[i] = true; done = true; }
                }
                if (!done)
                {
                    var im = Scripts.IndexModCached(j.CacheDir, it.Id, it.Dir, token);
                    j.Out[i] = im.Files;
                    j.Out[i].Fingerprint = it.Fp;
                    if (j.Out[i].Complete) { j.Defs[i] = im.Defs; j.Defs[i].Fingerprint = it.Fp; j.FromCache[i] = im.FromCache; }
                }
                int done1 = i + 1;
                Post(() => { if (job == j) { ScanDone = done1; UpdateCount(); ConflictsChanged?.Invoke(); } });
            }
        }
        catch (Exception e)   // never let an exception end the program from a worker thread; the mods not finished simply stay unread
        {
            Log.Line("scan worker failed: " + e.Message);
            foreach (var o in j.Out) if (!o.Complete) o.Files.Clear();
        }
        j.Ms = sw.ElapsedMilliseconds;
        Post(() => OnScanDone(j));
    }

    void OnScanDone(ScanJob j)
    {
        if (job != j) return;
        j.Th?.Join();
        ConfQuiesce();   // the index is about to change
        bool cancelled = j.Cts.IsCancellationRequested;
        int hits = 0, read = 0, same = 0;
        for (int i = 0; i < j.Items.Count && i < j.Out.Length; i++)
        {
            var it = j.Items[i];
            if (j.Unchanged[i])   // checked file by file: nothing changed
            {
                if (FileIndex.TryGetValue(it.Id, out var f0)) f0.Fingerprint = it.Fp;
                if (DefIndex.TryGetValue(it.Id, out var d0)) d0.Fingerprint = it.Fp;
                same++;
                continue;
            }
            if (!j.Out[i].Complete)
            {
                if (!cancelled) { scanFailed[it.Id] = it.Fp; Log.Line("could not read all files of mod " + it.Id + ", skipped in conflict checks"); }
                continue;
            }
            scanFailed.Remove(it.Id);
            FileIndex[it.Id] = j.Out[i];
            if (j.Defs[i].Complete) DefIndex[it.Id] = j.Defs[i];
            if (j.FromCache[i]) hits++; else read++;
        }
        if (j.WantVanilla && !cancelled)
        {
            if (j.Vanilla != null)
            {
                Van = j.Vanilla;
                Log.Line($"game files: {Van.Files.Count} in {Van.Dir}" + (Van.DlcFiles > 0 ? $" ({Van.DlcFiles} from DLC folders)" : "") + (j.VanillaFromCache ? ", from the saved list" : ", read from disk"));
            }
            else { VanillaFailed = true; Log.Line("game files: could not read the game folder, base-game checks are off"); }
        }
        if (j.Items.Count > 0 && (read + hits > 0 || !j.Quiet))
            Log.Line($"scan: {j.Items.Count} mods in {j.Ms} ms ({hits} from the saved index, {read} read" + (same > 0 ? $", {same} unchanged" : "") + ")");
        lastDeepVerify = Environment.TickCount64;
        fpNow.Clear();   // the update check now knows each indexed mod's deep fingerprint
        if (!prunedCache && !cancelled && Info.Count > 0)
        {
            prunedCache = true;
            Scripts.PruneModCache(Path.Combine(Util.DataDir(), "cache"), Info.Keys);
        }
        job = null; Scanning = false;
        if (!cancelled)
        {
            confSig = ""; RefreshNotes(); MaybeScan(false);
            ScanFinished?.Invoke();
            if (!SinceShown && !Scanning) { SinceShown = true; SinceHint(); CrashHint(); }
        }
    }

    public event Action? ScanFinished;

    public void StopScan()
    {
        var j = job;
        if (j == null) return;
        j.Cts.Cancel();
        j.Th?.Join(5000);
        job = null; Scanning = false;
    }

    /// <summary>Everything is shut down in order: the workers are stopped before the window closes.</summary>
    public void Shutdown() { StopScan(); ConfQuiesce(); }

    // ---- hints after the first scan ----
    /// <summary>Once per start, after the mods have been read: say in the status line when something changed since Play was last pressed.</summary>
    void SinceHint()
    {
        var ps = Active();
        if (ps == null || St.PlayTime <= 0) return;
        var pr = Changes.BuildPlayReport(St, ps, Info, FpNow(), FileIndex, GameVer);
        if (pr.Empty) return;
        string t = Tr("Since you last played:");
        bool any = false;
        void Part(int n, string tmpl) { if (n == 0) return; t += (any ? ", " : " ") + Trf(tmpl, n); any = true; }
        if (pr.GameChanged) { t += " " + Trf("the game updated to {0}", pr.NowGame); any = true; }
        Part(pr.Updated.Count, K("{0} mod(s) updated"));
        Part(pr.Outdated.Count, K("{0} made for an older game"));
        Part(pr.Added.Count + pr.Removed.Count, K("{0} turned on or off"));
        t += ". ";
        t += Tr("Details: Advanced > What changed since I last pressed Play.");
        Say(t);
    }

    public string CrashesDir() { string d = EffectiveDir(); return d.Length == 0 ? "" : Path.Combine(d, "crashes"); }
    public static string CrashWhen(CrashFolder c) => c.Y <= 0 ? c.Name : $"{c.Y:D4}-{c.Mo:D2}-{c.D:D2} {c.H:D2}:{c.Mi:D2}";
    /// <summary>Once per start: a crash folder that is newer than the last one the user was told about.</summary>
    void CrashHint()
    {
        var list = Crash.List(CrashesDir(), 5);
        if (list.Count == 0) return;
        string newest = list[0].Name;
        if (St.CrashSeen.Length == 0) { St.CrashSeen = newest; SaveSettingsNow(); return; }   // crashes from before this program was used are not news
        if (string.CompareOrdinal(newest, St.CrashSeen) <= 0) return;
        Say(Trf("The game crashed on {0}. Advanced > Crash helper shows the likely cause.", CrashWhen(list[0])));
    }
}
