using System.Text;
namespace RoyalCourt.Core;

/// <summary>One line of the mod list, ready to show.</summary>
public sealed class ModRow
{
    public int Idx;                          // position in the active playset
    public string Num = "", Name = "", Version = "", GameVersion = "", Source = "", Type = "", Note = "", Size = "";
    public int Order;                        // 0 load order not known, 1 its page was read and gives no order, 2 the database has a rule for it
    public long SizeBytes = -1;              // -1 = not measured
    public int NoteSev;                      // 0 none, 1 warning, 2 problem, 3 checked and fine
    public int VerState;                     // 0 unknown, 1 matches the game, 2 may be outdated
    public bool Enabled, Locked, Pending, Installed;
    public string Id = "";
}

/// <summary>The dialogs the controller needs. The window implements them; tests use a scripted version.</summary>
public interface IDialogs
{
    Task Info(string text);
    Task<bool> Confirm(string title, string text);
    Task<string?> Ask(string title, string prompt, string initial);
    Task<string?> PickGameExe();
    Task<string?> SaveFile(string suggestedName);
    Task<string?> OpenFile();
    Task ShowText(string title, string text);
    Task<bool> SortPreview(SortPlan plan, Playset ps);
    Task<string> ClipboardGet();
    Task<bool> ClipboardSet(string text);
    /// <summary>A menu of choices; returns the index picked or -1.</summary>
    Task<int> PickItem(string title, IReadOnlyList<string> items);
    Task<string?> PickBackup(string initialDir);
    /// <summary>1001 = update now, 1002 = open the release page, 0 = not now.</summary>
    Task<int> UpdateChoice(string head, string body, string notes, bool canInstall, bool hasPage);
}

/// <summary>Everything the main window does, without any window: the state, the background scans, the list and all operations on playsets.
/// All members are used from the interface thread; the two workers only read the file index and report back through <see cref="Post"/>.</summary>
public sealed partial class ModManager
{
    // ---- state ----
    public Settings St = new();
    public List<Playset> Playsets = new();
    public List<ModInfo> Mods = new();
    public Dictionary<string, ModInfo> Info = new(StringComparer.Ordinal);
    public string GameVer = "", GameExeUsed = "";
    public List<ModIssue>[] Issues = Array.Empty<List<ModIssue>>();
    public List<ModRow> Rows = new();
    public List<int> Shown = new();                  // list row -> index into the active playset's mods
    public List<KnownMod> KnownDb = new();
    public int KnownRev;
    public int HiddenPresent;
    public string Filter = "";
    public int SortCol = -1;                         // -1 = load order; otherwise the column the view is sorted by (view only)
    public bool SortAsc = true;
    public List<string> UndoIds = new();
    public string UndoPlayset = "";
    public List<string> DocRoots = Platform.DocRoots();
    public string Status = "";

    // ---- the interface hooks ----
    /// <summary>Runs an action on the interface thread (the window sets this; the default runs it at once).</summary>
    public Action<Action> Post = a => a();
    public IDialogs? Dlg;
    public event Action<string>? StatusChanged;
    public event Action? ListChanged;        // the rows were rebuilt
    public event Action? NotesChanged;       // only the Notes / Type cells changed
    public event Action? CountChanged;
    public event Action? PlaysetsChanged;    // the playset combo has to be refilled
    public event Action? ConflictsChanged;
    public event Action? GameVersionChanged;
    public event Action? FolderChanged;

    public static Playset? NoPlayset => null;

    public Playset? Active() { foreach (var p in Playsets) if (p.Name == St.Active) return p; return null; }
    public string EffectiveDir() => St.Ck3Dir.Length == 0 ? ModScan.AutoFindCk3Dir(DocRoots) : St.Ck3Dir;
    public void Say(string t) { Status = t; StatusChanged?.Invoke(t); }
    static bool Lt(string a, string b) => string.CompareOrdinal(Util.Lower(a), Util.Lower(b)) < 0;

    ISet<string> LockedIds()
    {
        var ps = Active();
        if (ps != null && St.Locks.TryGetValue(ps.Name, out var s)) return s;
        return new SortedSet<string>(StringComparer.Ordinal);
    }
    SortedSet<string> LockedIdsForWrite(string playset)
    {
        if (!St.Locks.TryGetValue(playset, out var s)) St.Locks[playset] = s = new SortedSet<string>(StringComparer.Ordinal);
        return s;
    }
    public bool IsLocked(string id) => LockedIds().Contains(id);

    public void SaveActive()
    {
        var ps = Active();
        if (ps != null && !Core.Playsets.Save(ps, Info)) Say(Tr("Warning: could not save the playset file."));
    }
    public void SaveSettingsNow() { if (!St.Save()) Say(Tr("Warning: could not save settings.")); }
    void SortPlaysets() { Playsets = Playsets.OrderBy(p => Util.Lower(p.Name), StringComparer.Ordinal).ToList(); }

    // ---- type of a mod (guess from name and tags, refined from its files, refined again by the known-mods list, overridden by the user) ----
    readonly Dictionary<string, (string key, Known.CatGuess g)> catCache = new(StringComparer.Ordinal);
    public int CatOfMod(string id, out bool overridden)
    {
        overridden = St.Cats.TryGetValue(id, out var ov);
        if (overridden) return ov;
        if (!Info.TryGetValue(id, out var mi)) return Cat.Content;
        var g = Known.GuessCategory(mi);
        if (g.Why == "default" && FileIndex.TryGetValue(id, out var f))
        {
            // looking through a big mod's whole file list on every redraw is slow: remembered until its index changes
            string key = f.Fingerprint + "#" + f.Files.Count + (f.Complete ? "c" : "i");
            if (!catCache.TryGetValue(id, out var c) || c.key != key) catCache[id] = c = (key, Known.GuessFromFiles(f));
            g = c.g;
        }
        int k = Known.Find(KnownDb, mi);
        if (k >= 0 && KnownDb[k].Cat >= 0) g = g with { Cat = KnownDb[k].Cat };
        return g.Cat;
    }
    public int CatOfMod(string id) => CatOfMod(id, out _);

    // ---- start ----
    public void LoadSettings()
    {
        St = Settings.Load();
    }

    /// <summary>Installed game version (empty if unknown); a changed install or a game update means the base game's file list is read again.</summary>
    void RefreshGameVersion(string exe)
    {
        string ver = ModScan.ReadGameVersion(exe);
        if (exe != GameExeUsed || ver != GameVer)
        {
            ConfQuiesce();
            Van = null; VanillaFailed = false; confSig = "";
        }
        GameExeUsed = exe;
        GameVer = ver;
        GameVersionChanged?.Invoke();
    }

    long modStamp;
    public long ModDirStamp()
    {
        string dir = EffectiveDir();
        if (dir.Length == 0) return 0;
        long best = 0;
        try { best = Directory.GetLastWriteTimeUtc(Path.Combine(dir, "mod")).Ticks; } catch { }
        foreach (var wd in Platform.WorkshopDirs())
        {
            if (!Util.DirExists(wd)) continue;
            try
            {
                best = Math.Max(best, Directory.GetLastWriteTimeUtc(wd).Ticks);
                foreach (var e in Util.ListDir(wd)) { try { best = Math.Max(best, e.LastWriteTimeUtc.Ticks); } catch { } }
            }
            catch { }
        }
        return best;
    }

    /// <summary>Takes mods out of the program's own state (playsets, list, caches) after they were removed, deleted or unsubscribed from. Nothing on disk is touched here.
    /// purge: they are gone for good, so everything remembered about them is forgotten too.</summary>
    void DropMods(IReadOnlyCollection<string> done, bool purge)
    {
        if (done.Count == 0) return;
        ConfQuiesce();
        var gone = new HashSet<string>(done, StringComparer.Ordinal);
        Mods.RemoveAll(m => gone.Contains(m.Id));
        Info = Core.Playsets.InfoMap(Mods); fpNow.Clear(); confSig = "";
        foreach (var pl in Playsets) { pl.Mods.RemoveAll(m => gone.Contains(m.Id)); Core.Playsets.Save(pl, Info); }
        foreach (var mid in done)
        {
            FileIndex.Remove(mid); DefIndex.Remove(mid);
            if (purge) { St.Seen.Remove(mid); St.Hidden.Remove(mid); St.Unsubbed.Remove(mid); St.Cats.Remove(mid); foreach (var lk in St.Locks.Values) lk.Remove(mid); foreach (var bk in St.BisectKeep.Values) bk.Remove(mid); }
        }
    }

    /// <summary>Mods the user unsubscribed from are already out of the list. Once Steam has deleted their files, the leftover descriptor (ugc_(id).mod) in the CK3 mod folder
    /// is cleared here too, so they do not come back as "files not found" entries.</summary>
    void ClearUnsubscribed()
    {
        if (St.Unsubbed.Count == 0) return;
        string dir = EffectiveDir();
        long now = Util.NowSeconds();
        var cleared = new List<string>();
        foreach (var kv in St.Unsubbed.ToList())
        {
            string id = kv.Key;
            var m = Mods.FirstOrDefault(x => x.Id == id);
            bool fin = false, unhide = false;
            if (m == null || Names.SteamIdOf(id).Length == 0) fin = true;
            else if (!m.Pending && m.ContentState == 2 && dir.Length > 0)
            {
                string d = Path.Combine(dir, "mod", id);
                try { File.Delete(d); } catch { }
                if (!Util.Exists(d)) { Log.Line("cleared unsubscribed mod " + id); fin = true; }
            }
            else if (now - kv.Value > 900) { Log.Line("unsubscribed mod " + id + " still has files after 15 minutes: shown again"); fin = true; unhide = true; }
            if (!fin) continue;
            if (unhide) St.Hidden.Remove(id); else cleared.Add(id);
            St.Unsubbed.Remove(id);
        }
        if (cleared.Count == 0) return;
        foreach (var id in cleared) { St.Seen.Remove(id); St.Hidden.Remove(id); St.Cats.Remove(id); foreach (var lk in St.Locks.Values) lk.Remove(id); foreach (var bk in St.BisectKeep.Values) bk.Remove(id); }
        Mods.RemoveAll(m => cleared.Contains(m.Id));
    }

    bool startupBackupDone;
    /// <summary>Reads the mod folder and the playsets again.</summary>
    public void Reload()
    {
        string exe = (St.GameExe.Length > 0 && Util.FileExists(St.GameExe)) ? St.GameExe : Platform.FindGameExeInSteam();
        RefreshGameVersion(exe);
        string dir = EffectiveDir();
        Mods = ModScan.ScanMods(dir);
        {
            var have = new HashSet<string>(Mods.Select(m => m.Id), StringComparer.Ordinal);
            var pend = Steam.ScanWorkshopFolders(Platform.WorkshopDirs(), have);
            Mods.AddRange(pend);
            if (Mods.Count > 0) Mods = Mods.OrderBy(m => Util.Lower(m.Name), StringComparer.Ordinal).ToList();
        }
        ClearUnsubscribed();
        HiddenPresent = Mods.Count(m => St.Hidden.Contains(m.Id));
        if (St.Hidden.Count > 0) Mods.RemoveAll(m => St.Hidden.Contains(m.Id));
        fpNow.Clear();
        KnownDb = Known.Load(out KnownRev);
        Info = Core.Playsets.InfoMap(Mods);
        StartSizeScan();
        int migrated = Core.Playsets.MigrateLegacyStore(Mods, St);
        Playsets = Core.Playsets.LoadAll(Mods);
        if (St.Hidden.Count > 0) foreach (var pl in Playsets) pl.Mods.RemoveAll(m => St.Hidden.Contains(m.Id));
        if (Playsets.Count == 0)
        {
            var d = new Playset { Name = "Default" };
            Core.Playsets.Sync(d, Mods);
            Core.Playsets.Save(d, Info);
            Playsets.Add(d);
        }
        if (Active() == null) St.Active = Playsets[0].Name;
        SaveSettingsNow();
        if (!startupBackupDone) { startupBackupDone = true; var a = Active(); if (a != null) Core.Playsets.Backup(a, Info, "startup"); }
        modStamp = ModDirStamp();
        FolderChanged?.Invoke();
        PlaysetsChanged?.Invoke();
        Populate();
        Post(() => MaybeScan(true));
        if (migrated > 0) Say(Trf("Your {0} playset(s) from the older version were converted to launcher playset files.", migrated));
        else if (dir.Length == 0) Say(Tr("CK3 folder not found. Enter it below (the folder that contains \"mod\"), then press Save folder."));
        else if (Mods.Count == 0) Say(Trf("No mods found in {0}\\mod", dir));
    }

    public bool Resyncing;
    /// <summary>Rescans the mod folder (e.g. after subscribing to a mod while the app is open). New mods are added to the end of every playset, disabled.</summary>
    public void Resync(bool automatic)
    {
        if (Resyncing) return;
        Resyncing = true;
        try
        {
            var before = new HashSet<string>(Mods.Select(m => m.Id), StringComparer.Ordinal);
            Reload();
            var after = new HashSet<string>(Mods.Select(m => m.Id), StringComparer.Ordinal);
            int added = after.Count(a => !before.Contains(a)), gone = before.Count(b => !after.Contains(b));
            int pend = Mods.Count(m => m.Pending);
            string msg = automatic ? Trf("The mod folder changed, list refreshed: {0} mods installed", Mods.Count) : Trf("Rescanned: {0} mods installed", Mods.Count);
            if (added > 0) msg += ", " + Trf("{0} new (added at the end of your playsets, disabled)", added);
            if (gone > 0) msg += ", " + Trf("{0} removed", gone);
            msg = Trf("{0}.", msg);
            if (pend > 0) msg += " " + Trf("{0} from Steam are not registered by the Paradox launcher yet (Play registers them).", pend);
            if (HiddenPresent > 0) msg += " " + Trf("{0} hidden because you removed them (Advanced > Show removed mods).", HiddenPresent);
            if (!automatic && added == 0) msg += " " + Tr("Nothing new found: if you just subscribed, wait until Steam has finished downloading the mod, then rescan.");
            Say(msg);
            Log.Line($"rescan: {Mods.Count} mods, {added} new, {pend} pending, {HiddenPresent} hidden");
        }
        finally { Resyncing = false; }
    }

    /// <summary>The user-initiated rescan: forgets the file lists too.</summary>
    public void RescanAll() { ConfQuiesce(); FileIndex.Clear(); DefIndex.Clear(); confSig = ""; Resync(false); }

    /// <summary>The window was brought to the front: look again, unless the game is running (nothing is scanned while it runs).</summary>
    public void OnActivated(bool dialogOpen)
    {
        if (dialogOpen || Platform.ProcessRunning("ck3")) return;
        if (modStamp != 0 && ModDirStamp() != modStamp) { try { Resync(true); } catch { Resyncing = false; } }
        else if (job == null) { try { MaybeScan(true); } catch { } }
    }

    /// <summary>Called every few seconds while unsubscribed mods are still being removed by Steam.</summary>
    public bool UnsubTick(bool dialogOpen)
    {
        if (St.Unsubbed.Count == 0) return false;
        if (dialogOpen || Resyncing || Platform.ProcessRunning("ck3")) return true;
        long now = Util.NowSeconds();
        bool due = modStamp != 0 && ModDirStamp() != modStamp;
        foreach (var kv in St.Unsubbed) if (now - kv.Value > 900) due = true;
        if (due) { try { Resync(true); } catch { Resyncing = false; } }
        return St.Unsubbed.Count > 0;
    }

    // ---- the list ----
    readonly Dictionary<string, string> fpNow = new(StringComparer.Ordinal);
    /// <summary>Fingerprints of the installed mods right now (cleared when the mod folder is rescanned).</summary>
    public IReadOnlyDictionary<string, string> FpNow()
    {
        if (fpNow.Count == 0)
            foreach (var kv in Info)
                if (kv.Value.ContentDir.Length > 0)
                {
                    string fp = Walk.ModFingerprint(kv.Value);
                    if (FileIndex.TryGetValue(kv.Key, out var f) && f.Complete && f.Deep.Length > 0) fp += "#d:" + f.Deep;   // indexed: every file's name, size and time count
                    fpNow[kv.Key] = fp;
                }
        return fpNow;
    }

    void Analyse(Playset ps)
    {
        Issues = Analyze.AnalyzePlayset(ps, Info, GameVer);
        if (Conf.Valid) Analyze.AddConflictIssues(Issues, Conf, ps, Info, GameVer);
        if (Script.Valid) Scripts.AddScriptIssues(Issues, Script, ps);
        Compare.AddUpdateIssues(Issues, ps, St.Seen, FpNow());
    }

    string NoteText(Playset ps, int i, out int sev)
    {
        var iss = Issues[i];
        sev = Analyze.IssueSeverity(iss);
        string sum = Analyze.IssueSummary(iss);
        if (sum.Length > 0) return sum;
        if (Info.TryGetValue(ps.Mods[i].Id, out var mi) && mi.Pending) return Tr("Downloaded from Steam. Tick the box to add it");
        if (ps.Mods[i].Enabled && Info.ContainsKey(ps.Mods[i].Id)) { sev = 3; return "✓"; }
        return "";
    }

    string ShownName(Playset ps, int i)
    {
        var m = ps.Mods[i];
        return Info.TryGetValue(m.Id, out var mi) ? mi.Name : Trf("{0}  (not installed)", m.Name.Length == 0 ? m.Id : m.Name);
    }

    ModRow MakeRow(Playset ps, int i)
    {
        var m = ps.Mods[i];
        bool inst = Info.TryGetValue(m.Id, out var mi);
        var r = new ModRow { Idx = i, Id = m.Id, Enabled = m.Enabled, Installed = inst, Pending = inst && mi!.Pending };
        r.Num = (i + 1).ToString();
        r.Locked = St.Locks.TryGetValue(ps.Name, out var lk) && lk.Contains(m.Id);
        if (r.Locked) r.Num += " \U0001F512";
        r.Name = ShownName(ps, i);
        r.Version = inst ? mi!.Version : "";
        r.GameVersion = inst ? mi!.Supported : "";
        var vm = inst ? ModScan.MatchGameVersion(mi!.Supported, GameVer) : VerMatch.Unknown;
        r.VerState = vm == VerMatch.Match ? 1 : vm == VerMatch.Mismatch ? 2 : 0;
        r.Source = inst ? Cat.SourceLabel(mi!.Source) : "";
        r.Note = NoteText(ps, i, out r.NoteSev);
        r.Type = inst ? Cat.Name(CatOfMod(m.Id, out bool ovr)) + (ovr ? " *" : "") : "";
        r.Order = inst ? OrderState(mi!) : 0;
        r.SizeBytes = inst ? SizeOf(m.Id) : -1; r.Size = ModSizes.Format(r.SizeBytes);
        return r;
    }

    /// <summary>Rebuilds the list for the active playset (filter, view sort, notes).</summary>
    public void Populate()
    {
        var ps = Active();
        if (ps == null) return;
        EnsureConflicts();
        Analyse(ps);
        string f = Util.Lower(Filter);
        var order = new List<int>();
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            if (f.Length > 0 && !Util.Lower(ShownName(ps, i)).Contains(f, StringComparison.Ordinal)) continue;
            if (Quick != QAll && !QuickMatch(ps, i)) continue;
            order.Add(i);
        }
        if (SortCol >= 1)
        {
            // View-only sort: the load order in the playset is never touched. Empty cells always go last.
            string Key(int i)
            {
                var m = ps.Mods[i];
                bool inst = Info.TryGetValue(m.Id, out var mi);
                return SortCol switch
                {
                    1 => inst ? mi!.Name : (m.Name.Length == 0 ? m.Id : m.Name),
                    2 => inst ? mi!.Version : "",
                    3 => inst ? mi!.Supported : "",
                    4 => inst ? Cat.SourceLabel(mi!.Source) : "",
                    5 => inst ? Cat.Name(CatOfMod(m.Id)) : "",
                    6 => inst ? (OrderState(mi!) == 2 ? "a" : OrderState(mi!) == 1 ? "b" : "") : "",
                    7 => inst && SizeOf(m.Id) >= 0 ? SizeOf(m.Id).ToString("D20") : "",
                    _ => Analyze.IssueSummary(Issues[i]),
                };
            }
            var keyed = order.Select(i => (key: Key(i), i)).ToList();
            bool asc = SortAsc; int col = SortCol;
            // OrderBy is stable, like std::stable_sort
            var sorted = keyed.Order(Comparer<(string key, int i)>.Create((x, y) =>
            {
                if (x.key.Length == 0 != (y.key.Length == 0)) return y.key.Length == 0 ? -1 : 1;
                if (col == 8)
                {
                    int sx = Analyze.IssueSeverity(Issues[x.i]), sy = Analyze.IssueSeverity(Issues[y.i]);
                    if (sx != sy) return asc ? (sx > sy ? -1 : 1) : (sx < sy ? -1 : 1);   // worst first when ascending
                }
                int c = Health.NaturalCompare(x.key, y.key);
                return asc ? c : -c;
            })).ToList();
            order = sorted.Select(k => k.i).ToList();
        }
        Shown = order;
        Rows = order.Select(i => MakeRow(ps, i)).ToList();
        ListChanged?.Invoke();
        UpdateCount();
    }

    /// <summary>After a single checkbox change: the problems of every mod can change (dependencies), so redo the Notes column.</summary>
    public void RefreshNotes()
    {
        var ps = Active();
        if (ps == null) return;
        EnsureConflicts();
        Analyse(ps);
        for (int r = 0; r < Shown.Count; r++)
        {
            int i = Shown[r];
            if (i >= ps.Mods.Count || r >= Rows.Count) continue;
            var nr = MakeRow(ps, i);
            var o = Rows[r];
            o.Note = nr.Note; o.NoteSev = nr.NoteSev; o.Type = nr.Type;
        }
        NotesChanged?.Invoke();
        UpdateCount();
    }

    public string CountText = "";
    public void UpdateCount()
    {
        var ps = Active();
        if (ps == null) return;
        int on = 0, old = 0;
        foreach (var m in ps.Mods)
        {
            if (!m.Enabled) continue;
            on++;
            if (Info.TryGetValue(m.Id, out var mi) && ModScan.MatchGameVersion(mi.Supported, GameVer) == VerMatch.Mismatch) old++;
        }
        int bad = 0;
        for (int i = 0; i < ps.Mods.Count && i < Issues.Length; i++) if (ps.Mods[i].Enabled && Analyze.IssueSeverity(Issues[i]) == 2) bad++;
        string t = Trf("{0} of {1} enabled", on, ps.Mods.Count);
        var es = EnabledSize();
        if (es.measured > 0) t += "  |  " + Trf("{0} on disk", ModSizes.Format(es.bytes)) + (es.measured < es.enabled ? "+" : "");
        if (bad > 0) t += "  |  " + Trf("{0} with problems", bad);
        if (Scanning) t += "  |  " + Trf("scanning mod files {0}/{1}", ScanDone, ScanTotal);
        else if (confJob != null) t += "  |  " + Tr("calculating conflicts");
        else if (Conf.Valid && Conf.Files.Count > 0)
        {
            t += "  |  " + Trf("{0} file conflicts", Conf.Files.Count);
            if (Script.Valid && Script.Items.Count > 0) t += ", " + Trf("{0} script overlaps", Script.Items.Count);
        }
        if (!Scanning && confJob == null && Conf.Valid && Conf.VanillaChecked)
        {
            int vm = Conf.VanillaCount.Count(c => c > 0);
            if (vm > 0) t += "  |  " + Trf("{0} replace base-game files", vm);
        }
        if (old > 0) t += "  |  " + Trf("{0} may be outdated", old);
        CountText = t;
        CountChanged?.Invoke();
    }

    public bool UndoAvailable { get { var ps = Active(); return ps != null && UndoIds.Count > 0 && UndoPlayset == ps.Name; } }
    /// <summary>Load order can only be changed in the playset's own order: no filter, no column sort.</summary>
    public bool OrderLocked => Filter.Length > 0 || Quick != QAll || SortCol >= 1;
    public string PlaysetName => Active()?.Name ?? "";
    public string[] Namelist(Playset ps) => ps.Mods.Select((_, i) => ShownName(ps, i)).ToArray();

    public void SetFilter(string f) { if (f == Filter) return; Filter = f; Populate(); }
    /// <summary>Click on a column header: ascending, again: descending, again (or "#"): back to load order. Never changes the playset itself.</summary>
    public void ClickColumn(int c)
    {
        if (c <= 0) SortCol = -1;
        else if (c != SortCol) { SortCol = c; SortAsc = true; }
        else if (SortAsc) SortAsc = false;
        else SortCol = -1;
        Populate();
        Say(SortCol < 0 ? Tr("Showing load order.") : Tr("Sorted view (load order is unchanged). Click # to go back to load order."));
    }
}
