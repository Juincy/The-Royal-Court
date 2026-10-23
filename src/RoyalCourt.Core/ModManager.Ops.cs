using System.Text;
namespace RoyalCourt.Core;

// Everything the buttons and the right-click menu do.
public sealed partial class ModManager
{
    public static readonly string Name = "The Royal Court";

    // ---- turning mods on and off ----
    /// <summary>A Workshop mod Steam has downloaded but the launcher has not set up yet gets its descriptor written when it is first turned on.</summary>
    public bool AddPendingMod(string mid, out string err)
    {
        err = "";
        if (!Info.TryGetValue(mid, out var mi) || !mi.Pending) return true;
        string dir = EffectiveDir();
        var one = new Playset();
        one.Mods.Add(new ModRef(mid, true, ""));
        int n = Steam.RegisterPendingMods(dir, one, Info, out string e);
        if (n == 0 && !Util.Exists(Path.Combine(dir, "mod", mid)))
        {
            err = Trf("Could not add \"{0}\" to your mod folder.", mi.Name) + (e.Length == 0 ? "" : " " + e);
            Log.Line("add Workshop mod failed: " + mid + " " + e);
            return false;
        }
        Log.Line("added Workshop mod " + mid);
        mi.Pending = false;
        foreach (var m in Mods) if (m.Id == mid) m.Pending = false;
        modStamp = ModDirStamp();   // our own write must not look like a change made by someone else
        return true;
    }

    /// <summary>Mods that are turned off are not used at all, so they sit below the ones that are on (the order inside each group is kept).
    /// A mod you just turned off goes to the top of the off group; one you just turned on goes to the end of the on group.</summary>
    public static bool SinkDisabled(Playset ps)
    {
        var before = ps.Mods.Select(m => m.Id).ToList();
        var on = ps.Mods.Where(m => m.Enabled).ToList();
        var off = ps.Mods.Where(m => !m.Enabled).ToList();
        ps.Mods = on.Concat(off).ToList();
        for (int i = 0; i < ps.Mods.Count; i++) if (ps.Mods[i].Id != before[i]) return true;
        return false;
    }

    /// <summary>The checkbox of one list row. Returns true when the order of the list changed (the window redraws it).</summary>
    public bool SetEnabled(int row, bool on)
    {
        var ps = Active();
        if (ps == null || row < 0 || row >= Shown.Count) return false;
        int idx = Shown[row];
        if (on && !AddPendingMod(ps.Mods[idx].Id, out string perr)) { Say(perr); Rows[row].Enabled = false; return false; }
        ps.Mods[idx].Enabled = on;
        bool moved = SinkDisabled(ps);
        SaveActive();
        if (moved) Populate(); else { Rows[row].Enabled = on; RefreshNotes(); }
        Post(() => MaybeScan(false));
        return moved;
    }

    /// <summary>"Enable shown" / "Disable shown": every mod the filter currently shows.</summary>
    public void SetShown(bool on)
    {
        var ps = Active();
        if (ps == null) return;
        string err = "";
        foreach (int idx in Shown)
        {
            if (on && !AddPendingMod(ps.Mods[idx].Id, out string e)) { err = e; continue; }   // could not be added: stays off
            ps.Mods[idx].Enabled = on;
        }
        SinkDisabled(ps);
        SaveActive();
        Populate();
        if (err.Length > 0) Say(err);
        Post(() => MaybeScan(false));
    }

    // ---- load order ----
    /// <summary>Moves every selected mod one step up or down (a block stays together; unselected mods hop over it). Returns the new positions of the selection.</summary>
    public List<int> MoveSelection(IReadOnlyList<int> selPositions, bool up)
    {
        var res = new List<int>();
        var ps = Active();
        if (ps == null) return res;
        if (OrderLocked) { Say(Tr("Clear the filter and column sorting (click the # header) to change load order.")); return res; }
        if (selPositions.Count == 0) { Say(Tr("Select a mod first.")); return res; }
        var isSel = new bool[ps.Mods.Count];
        foreach (int i in selPositions) if (i >= 0 && i < isSel.Length) isSel[i] = true;
        int n = ps.Mods.Count;
        bool moved = false;
        if (up) { for (int i = 1; i < n; i++) if (isSel[i] && !isSel[i - 1]) { (ps.Mods[i], ps.Mods[i - 1]) = (ps.Mods[i - 1], ps.Mods[i]); (isSel[i], isSel[i - 1]) = (isSel[i - 1], isSel[i]); moved = true; } }
        else { for (int i = n - 2; i >= 0; i--) if (isSel[i] && !isSel[i + 1]) { (ps.Mods[i], ps.Mods[i + 1]) = (ps.Mods[i + 1], ps.Mods[i]); (isSel[i], isSel[i + 1]) = (isSel[i + 1], isSel[i]); moved = true; } }
        for (int i = 0; i < n; i++) if (isSel[i]) res.Add(i);
        if (!moved) return res;
        SaveActive();
        Populate();
        Post(() => RefreshAfterOrder());
        return res;
    }

    void RefreshAfterOrder() { }

    /// <summary>"Rescan files": forgets every file list and the base game's list, so all of it is read again.</summary>
    public void ForgetIndexes()
    {
        ConfQuiesce(); FileIndex.Clear(); DefIndex.Clear(); Van = null; VanillaFailed = false; confSig = ""; Conf = new(); Script = new();
    }
    public bool ConflictsRunning => confJob != null;

    /// <summary>Moves the mod at position from so it ends up at position to (both indices into the full playset).</summary>
    public bool MoveMod(int from, int to)
    {
        var ps = Active();
        if (ps == null || from < 0 || to < 0 || from >= ps.Mods.Count || to >= ps.Mods.Count || from == to) return false;
        var m = ps.Mods[from];
        ps.Mods.RemoveAt(from);
        ps.Mods.Insert(to, m);
        SaveActive();
        Populate();
        return true;
    }

    // ---- playsets ----
    public void SelectPlayset(int i)
    {
        if (i < 0 || i >= Playsets.Count || Playsets[i].Name == St.Active) return;
        St.Active = Playsets[i].Name; SaveSettingsNow();
        Populate();
        Post(() => MaybeScan(false));
    }

    public async Task NewPlayset()
    {
        if (Dlg == null) return;
        var v = await Dlg.Ask(Tr("New playset"), Tr("Name for the new playset:"), "");
        if (v == null) return;
        var p = new Playset { Name = Core.Playsets.UniqueName(Playsets, v) };
        Core.Playsets.Sync(p, Mods);
        if (!Core.Playsets.Save(p, Info)) { await Dlg.Info(Tr("Could not create the playset file.")); return; }
        Playsets.Add(p); SortPlaysets();
        St.Active = p.Name; SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
        Say(Trf("Created \"{0}\".", p.Name));
    }

    public async Task DuplicatePlayset()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var v = await Dlg.Ask(Tr("Duplicate playset"), Tr("Name for the copy:"), Trf("{0} copy", ps.Name));
        if (v == null) return;
        var p = ps.Clone();
        p.Name = Core.Playsets.UniqueName(Playsets, v);
        if (!Core.Playsets.Save(p, Info)) { await Dlg.Info(Tr("Could not create the playset file.")); return; }
        Playsets.Add(p); SortPlaysets();
        St.Active = p.Name; SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
    }

    public async Task RenamePlayset()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var v = await Dlg.Ask(Tr("Rename playset"), Tr("New name:"), ps.Name);
        if (v == null) return;
        string n = Util.SanitizeFileName(v);
        if (n == ps.Name) return;
        if (Core.Playsets.NameTaken(Playsets, n, ps.Name)) { await Dlg.Info(Tr("A playset with that name already exists.")); return; }
        string oldName = ps.Name;
        if (!Core.Playsets.Rename(ps, n, Info)) { await Dlg.Info(Tr("Could not rename the playset file.")); return; }
        if (St.Locks.TryGetValue(oldName, out var lk)) { St.Locks[ps.Name] = lk; St.Locks.Remove(oldName); }
        if (St.Launch.TryGetValue(oldName, out var lo)) { St.Launch[ps.Name] = lo; St.Launch.Remove(oldName); }
        if (UndoPlayset == oldName) UndoPlayset = ps.Name;
        St.Active = ps.Name;
        SortPlaysets(); SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
    }

    public async Task DeletePlayset()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        if (Playsets.Count < 2) { await Dlg.Info(Tr("You need at least one playset.")); return; }
        if (!await Dlg.Confirm(Name, Trf("Delete playset \"{0}\"? Its file will be removed. A copy is kept in the Backups folder (Advanced > Open backups folder).", ps.Name))) return;
        string gone = ps.Name;
        Core.Playsets.Backup(ps, Info, "before deleting the playset");
        Core.Playsets.Delete(gone);
        St.Locks.Remove(gone);
        St.Launch.Remove(gone);
        Playsets.RemoveAll(p => p.Name == gone);
        St.Active = Playsets[0].Name; SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
    }

    // ---- export and import ----
    int EnabledCount(Playset ps) => ps.Mods.Count(m => m.Enabled);

    public async Task ExportFile()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var path = await Dlg.SaveFile(Util.SanitizeFileName(ps.Name) + ".json");
        if (path == null) return;
        // The name written inside the file is the file name you chose, so both always match.
        var outPs = ps.Clone();
        string stem = Util.FileStem(path);
        if (stem.Length > 0) outPs.Name = stem;
        if (!Util.WriteFile(path, Core.Playsets.ExportLauncher(outPs, Info))) { await Dlg.Info(Tr("Could not write that file.")); return; }
        Say(Trf("Exported \"{0}\" ({1} enabled mods) to {2}", outPs.Name, EnabledCount(ps), path));
    }

    public async Task ExportCode(bool withNames)
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        string code = Share.Make(ps, Info, withNames);
        if (!await Dlg.ClipboardSet(code)) { await Dlg.Info(Tr("Could not copy to the clipboard (another program may be holding it). Try again.")); return; }
        Say(Trf("Share code for \"{0}\" copied ({1} enabled mods, {2} characters). Paste it in a chat; the other person uses Import > Paste a share code.", ps.Name, EnabledCount(ps), code.Length));
    }

    public async Task ImportFile()
    {
        if (Dlg == null) return;
        var path = await Dlg.OpenFile();
        if (path == null) return;
        if (!Util.ReadFile(path, out var text, 5L << 20)) { await Dlg.Info(Tr("Could not read that file (is it larger than 5 MB?).")); return; }
        var r = Core.Playsets.Parse(text, Mods);
        if (!r.Ok) { await Dlg.Info(r.Error); return; }
        var p = r.Playset;
        string stem = Util.FileStem(path);  // the imported playset keeps the file's name
        p.Name = Core.Playsets.UniqueName(Playsets, stem.Length == 0 ? p.Name : stem);
        Core.Playsets.Sync(p, Mods);
        int missing = p.Mods.Count(m => m.Enabled && !Info.ContainsKey(m.Id));
        if (!Core.Playsets.Save(p, Info)) { await Dlg.Info(Tr("Could not save the imported playset.")); return; }
        Playsets.Add(p); SortPlaysets();
        St.Active = p.Name; SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
        string msg = Trf("Imported \"{0}\"", p.Name);
        if (missing > 0) msg += ": " + Trf("{0} mod(s) not installed (shown in the list)", missing);
        if (r.Invalid > 0) msg += "; " + Trf("{0} invalid entries ignored", r.Invalid);
        Say(msg + ".");
    }

    /// <summary>Share code from the clipboard -> a new playset. Mods you do not have are kept in the list (shown as not installed).</summary>
    public async Task ImportCode()
    {
        if (Dlg == null) return;
        string text = await Dlg.ClipboardGet();
        if (Share.Find(text).Length == 0) { await Dlg.Info(Tr("There is no share code on the clipboard. Copy one (it starts with RC1:) and try again.")); return; }
        var d = Share.Decode(text);
        if (!d.Ok) { await Dlg.Info(d.Error); return; }
        var p = Share.ToPlayset(d, Mods);
        p.Name = Core.Playsets.UniqueName(Playsets, p.Name.Length == 0 ? "Shared playset" : Util.SanitizeFileName(p.Name));
        Core.Playsets.Sync(p, Mods);
        var missing = new List<(string id, string name)>();
        int missingLocal = 0;
        foreach (var m in p.Mods)
        {
            if (!m.Enabled || Info.ContainsKey(m.Id)) continue;
            string sid = Names.SteamIdOf(m.Id);
            if (sid.Length > 0) missing.Add((sid, m.Name)); else missingLocal++;
        }
        if (!Core.Playsets.Save(p, Info)) { await Dlg.Info(Tr("Could not save the imported playset.")); return; }
        Playsets.Add(p); SortPlaysets();
        St.Active = p.Name; SaveSettingsNow(); PlaysetsChanged?.Invoke(); Populate();
        Log.Line($"imported a share code: {p.Mods.Count} mods, {missing.Count} Workshop mods not installed");
        string msg = Trf("Imported \"{0}\" from the share code ({1} mods)", p.Name, p.Mods.Count);
        if (missing.Count > 0 || missingLocal > 0) msg += "; " + Trf("{0} not installed", missing.Count + missingLocal);
        Say(msg + ".");
        if (missing.Count == 0 && missingLocal == 0) return;
        var t = new StringBuilder(Trf("The playset was imported, but {0} enabled mod(s) are not installed on this computer:", missing.Count + missingLocal) + "\n\n");
        int shown = 0;
        foreach (var mm in missing) { if (shown++ >= 14) break; t.Append("  • ").Append(mm.name.Length == 0 ? Trf("Workshop mod {0}", mm.id) : mm.name).Append('\n'); }
        if (missing.Count > 14) t.Append("  ").Append(Trf("... and {0} more", missing.Count - 14)).Append('\n');
        if (missingLocal > 0) t.Append("  • ").Append(Trf("{0} local mod(s) that are not Workshop mods (ask the sender for them)", missingLocal)).Append('\n');
        if (missing.Count == 0) { await Dlg.Info(t.ToString()); return; }
        t.Append('\n').Append(Tr("Subscribe to them on the Steam Workshop, wait for Steam to download them, then press Rescan.\n\nCopy the Workshop links of the missing mods to the clipboard?"));
        if (await Dlg.Confirm(Tr("Share code imported"), t.ToString()))
        {
            var links = new StringBuilder();
            foreach (var mm in missing) links.Append("https://steamcommunity.com/sharedfiles/filedetails/?id=").Append(mm.id).Append(mm.name.Length == 0 ? "" : "   " + mm.name).Append('\n');
            if (!await Dlg.ClipboardSet(links.ToString())) await Dlg.Info(Tr("Could not copy to the clipboard (another program may be holding it). Try again."));
            else Say(Tr("Workshop links copied to the clipboard."));
        }
    }

    // ---- Auto Sort ----
    public SortPlan PlanSort()
    {
        var ps = Active()!;
        return Sorter.Plan(ps, Info, LockedIds(), St.Cats, Conf.Valid ? Conf : null, FileIndex, KnownDb);
    }

    public async Task AutoSort()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        if (ps.Mods.Count < 2) { Say(Tr("Nothing to sort.")); return; }
        EnsureConflicts();
        var plan = PlanSort();
        if (!plan.Changed)
        {
            string m = Tr("Already in a good order, nothing to move.");
            foreach (var wn in plan.Warnings) m += "  ⚠ " + wn;
            Say(m); return;
        }
        if (!await Dlg.SortPreview(plan, ps)) { Say(Tr("Auto Sort cancelled, nothing changed.")); return; }
        ps = Active()!;
        Core.Playsets.Backup(ps, Info, "before auto sort");
        UndoIds = ps.Mods.Select(m => m.Id).ToList();
        UndoPlayset = ps.Name;
        ps.Mods = plan.Order.Select(o => ps.Mods[o]).ToList();
        SaveActive(); Populate();
        Say(Trf("Auto Sort moved {0} mods. Press Undo order to go back.", plan.Moves.Count));
    }

    public void UndoSort()
    {
        var ps = Active();
        if (ps == null || UndoIds.Count == 0 || UndoPlayset != ps.Name) return;
        var byId = new Dictionary<string, ModRef>(StringComparer.Ordinal);
        foreach (var m in ps.Mods) byId.TryAdd(m.Id, m);
        var nm = new List<ModRef>(); var used = new HashSet<string>(StringComparer.Ordinal);
        foreach (var uid in UndoIds) if (byId.TryGetValue(uid, out var f) && used.Add(uid)) nm.Add(f);
        foreach (var m in ps.Mods) if (!used.Contains(m.Id)) nm.Add(m);
        ps.Mods = nm;
        UndoIds.Clear();
        SaveActive(); Populate();
        Say(Tr("Load order restored to how it was before the last change."));
    }

    public string ExplainPosition(int idx)
    {
        var ps = Active()!;
        var plan = PlanSort();
        return Sorter.ExplainPosition(plan, ps, Info, idx, IsLockedAt(idx));
    }
    bool IsLockedAt(int idx) { var ps = Active(); return ps != null && idx >= 0 && idx < ps.Mods.Count && LockedIds().Contains(ps.Mods[idx].Id); }

    // ---- right-click menu actions (positions are indices into the active playset) ----
    public void ToggleLock(IReadOnlyList<int> sel, int clicked)
    {
        var ps = Active();
        if (ps == null || clicked < 0 || clicked >= ps.Mods.Count) return;
        var lk = LockedIdsForWrite(ps.Name);
        bool unlock = lk.Contains(ps.Mods[clicked].Id);   // the row that was clicked decides for the whole selection
        foreach (int i in sel)
        {
            if (i < 0 || i >= ps.Mods.Count) continue;
            if (unlock) lk.Remove(ps.Mods[i].Id); else lk.Add(ps.Mods[i].Id);
        }
        SaveSettingsNow(); Populate();
    }

    /// <summary>cat = -1: automatic again.</summary>
    public void SetType(IReadOnlyList<int> sel, int cat)
    {
        var ps = Active();
        if (ps == null) return;
        foreach (int i in sel)
        {
            if (i < 0 || i >= ps.Mods.Count) continue;
            string mid = ps.Mods[i].Id;
            if (cat < 0) St.Cats.Remove(mid); else St.Cats[mid] = cat;
        }
        SaveSettingsNow(); Populate();
    }

    public bool HasWorkshopId(string modId) => Names.SteamIdOf(modId).Length > 0;

    /// <summary>A mod that is removed or deleted disappears from every playset, so every playset that has it is backed up first.</summary>
    void BackupAffected(IEnumerable<string> ids, string reason)
    {
        var want = new HashSet<string>(ids, StringComparer.Ordinal);
        foreach (var pl in Playsets)
            if (pl.Mods.Any(m => want.Contains(m.Id))) Core.Playsets.Backup(pl, Info, reason);
    }

    public async Task Unsubscribe(IReadOnlyList<int> sel)
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var wids = new List<string>(); var names = new List<string>(); var uids = new List<string>();
        foreach (int i in sel)
        {
            if (i < 0 || i >= ps.Mods.Count) continue;
            string sid = Names.SteamIdOf(ps.Mods[i].Id);
            if (sid.Length == 0) continue;
            wids.Add(sid); uids.Add(ps.Mods[i].Id);
            names.Add(Info.TryGetValue(ps.Mods[i].Id, out var mi) ? mi.Name : ps.Mods[i].Id);
        }
        if (wids.Count == 0) return;
        var q = new StringBuilder();
        if (wids.Count == 1) q.Append(Trf("Unsubscribe from \"{0}\" on Steam?", names[0]));
        else
        {
            q.Append(Trf("Unsubscribe from these {0} Workshop mods on Steam?", wids.Count));
            for (int k = 0; k < names.Count && k < 8; k++) q.Append("\n  • ").Append(names[k]);
            if (names.Count > 8) q.Append("\n  ").Append(Trf("... and {0} more", names.Count - 8));
        }
        q.Append("\n\n").Append(Tr("Steam removes the mods' files from your computer and they stop loading in every playset. You can subscribe again on the Steam Workshop at any time."));
        if (!await Dlg.Confirm(Name, q.ToString())) return;
        if (Platform.ProcessRunning("ck3")) { await Dlg.Info(Tr("Crusader Kings III is running. Close the game first, then try again.")); return; }
        if (!Platform.ProcessRunning("steam")) { await Dlg.Info(Tr("Steam is not running. Start Steam and sign in, then try again.")); return; }
        BackupAffected(uids, "before unsubscribing");
        string exe = St.GameExe.Length > 0 ? St.GameExe : GameExeUsed;
        int sent = await Task.Run(() => { int s = Platform.SteamUnsubscribe(exe, wids, out string why); if (s <= 0) unsubWhy = why; return s; });
        if (sent > 0)
        {
            Log.Line("unsubscribe requested for " + sent + " mod(s)");
            long now = Util.NowSeconds();
            foreach (var u in uids) { St.Unsubbed[u] = now; St.Hidden.Add(u); }   // out of the list now; the leftover entry is cleared once Steam has deleted the files
            SaveSettingsNow();
            DropMods(uids, false);
            UnsubWatch?.Invoke();
            Populate();
            Say(Trf("Asked Steam to unsubscribe from {0} mod(s). They are out of your list and playsets now; Steam deletes the files itself.", sent));
        }
        else
        {
            Log.Line("unsubscribe through Steamworks failed: " + unsubWhy);
            for (int k = 0; k < wids.Count && k < 5; k++) Platform.OpenShell("steam://url/CommunityFilePage/" + wids[k]);
            await Dlg.Info(Tr("The program could not unsubscribe through Steam directly, so the Workshop page of the mod is opened in Steam instead. Press Unsubscribe there."));
        }
    }
    string unsubWhy = "";
    public event Action? UnsubWatch;      // start the 3 second watcher for Steam finishing the removal

    static bool IsReparse(string p)
    {
        try { return (File.GetAttributes(p) & FileAttributes.ReparsePoint) != 0; } catch { return true; }
    }

    /// <summary>Remove from the list only (nothing on disk is touched), or delete the mod's descriptor and folder for good.</summary>
    public async Task RemoveOrDelete(IReadOnlyList<int> sel, bool files)
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var ids = new List<string>();
        foreach (int i in sel)
        {
            if (i < 0 || i >= ps.Mods.Count) continue;
            string mid = ps.Mods[i].Id;
            if (files && (!Info.TryGetValue(mid, out var gi) || gi.Pending)) continue;   // nothing of ours on disk to delete
            ids.Add(mid);
        }
        if (ids.Count == 0) return;
        ConfQuiesce();   // the conflict worker reads the file index, which is about to change
        string NameOf(string mid) => Info.TryGetValue(mid, out var mi) ? mi.Name : mid;
        string q, firstName = NameOf(ids[0]);
        if (ids.Count == 1)
        {
            Info.TryGetValue(ids[0], out var mit);
            string nm = NameOf(ids[0]);
            bool ws = mit != null && mit.Source == "Workshop";
            if (files && ws) q = Trf("Permanently delete \"{0}\"?\n\nThis erases the mod's entry (its descriptor file in your CK3 mod folder) from your disk and from every playset. This cannot be undone.", nm) + "\n\n" +
                (mit!.ContentState == 2 ? Tr("The mod's files are already gone (you unsubscribed on Steam), so this just clears the leftover entry.") : Tr("The mod is still installed through Steam. Unsubscribe on Steam first, otherwise Steam will bring it back."));
            else if (files) q = Trf("Permanently delete \"{0}\"?\n\nIts descriptor file and its folder inside your CK3 mod folder are erased from your disk. This cannot be undone.", nm);
            else q = Trf("Remove \"{0}\" from the list?\n\nNothing is deleted from your disk. The mod disappears from every playset and from this list (Advanced > Show removed mods brings it back).", nm);
        }
        else
        {
            var list = new StringBuilder();
            for (int k = 0; k < ids.Count && k < 8; k++) list.Append("\n  • ").Append(NameOf(ids[k]));
            if (ids.Count > 8) list.Append("\n  ").Append(Trf("... and {0} more", ids.Count - 8));
            if (files) q = Trf("Permanently delete these {0} mods?", ids.Count) + list + "\n\n" + Tr("Their descriptor files and folders inside your CK3 mod folder are erased from your disk and from every playset. This cannot be undone.\n\nMods that are still installed through Steam come back unless you unsubscribe on Steam first.");
            else q = Trf("Remove these {0} mods from the list?", ids.Count) + list + "\n\n" + Tr("Nothing is deleted from your disk. They disappear from every playset and from this list (Advanced > Show removed mods brings them back).");
        }
        if (!await Dlg.Confirm(Name, q)) return;
        string dir = EffectiveDir();
        var done = new List<string>();
        int failed = 0;
        if (files)
        {
            if (dir.Length == 0) return;
            BackupAffected(ids, "before deleting mods");
            string modDir = Path.GetFullPath(Path.Combine(dir, "mod"));
            foreach (var mid in ids)
            {
                if (!Info.TryGetValue(mid, out var mit)) continue;
                bool bad = false, refused = false;
                string desc = Path.Combine(modDir, mid);
                // the mod's own folder is erased only if it really lies inside the CK3 mod folder (never the folder itself, never a link)
                if (mit.ContentDir.Length > 0)
                {
                    string content = Path.GetFullPath(mit.ContentDir);
                    string rel = Path.GetRelativePath(modDir, content);
                    bool inside = rel != "." && !rel.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(rel)
                                  && Util.DirExists(content) && !IsReparse(mit.ContentDir);
                    if (inside && Safe.HasLinkInside(content)) { bad = refused = true; Log.Line("delete refused for " + mid + ": the folder contains a link"); }
                    else if (inside) { try { Directory.Delete(content, true); } catch { } if (Util.Exists(content)) bad = true; }
                }
                if (!refused) { try { File.Delete(desc); } catch { } }   // a refused folder keeps its descriptor, so nothing is left half deleted
                if (Util.Exists(desc)) bad = true;
                if (bad) { failed++; FileIndex.Remove(mid); Log.Line("delete failed for " + mid); }
                else { done.Add(mid); Log.Line("deleted mod " + mid); }
            }
        }
        else
        {
            BackupAffected(ids, ids.Count == 1 ? "before removing a mod" : "before removing mods");
            foreach (var mid in ids) { St.Hidden.Add(mid); done.Add(mid); Log.Line("removed mod from list " + mid); }
            SaveSettingsNow();
        }
        if (files) modStamp = ModDirStamp();   // our own deletions are not a change made by someone else
        if (done.Count > 0)
        {
            DropMods(done, files);
            if (files) SaveSettingsNow();
            Populate();
            Say(files ? (done.Count == 1 ? Trf("Deleted \"{0}\".", firstName) : Trf("Deleted {0} mods.", done.Count))
                      : (done.Count == 1 ? Trf("Removed \"{0}\" from the list.", firstName) : Trf("Removed {0} mods from the list.", done.Count)));
        }
        if (failed > 0) await Dlg.Info(Trf("{0} mod(s) could not be fully deleted (are their files open in another program?). They are left in the list; close the other program and try again.", failed));
    }

    // ---- details ----
    public string DetailsText(int idx)
    {
        var ps = Active();
        if (ps == null || idx < 0 || idx >= ps.Mods.Count) return "";
        var m = ps.Mods[idx];
        string t;
        if (!Info.TryGetValue(m.Id, out var mi)) t = (m.Name.Length == 0 ? m.Id : m.Name) + "\n" + Trf("Not installed ({0})", m.Id) + "\n";
        else
        {
            static string Join(List<string> v) => v.Count == 0 ? "-" : string.Join(", ", v);
            string files = mi.Path.Length == 0 ? (mi.Archive.Length == 0 ? Trf("Files: {0}", "-") : Trf("Files: archive {0}", mi.Archive))
                                               : (mi.ContentState == 2 ? Trf("Files: {0}  (NOT FOUND)", mi.ContentDir) : Trf("Files: {0}", mi.ContentDir));
            int kn = Known.Find(KnownDb, mi);
            t = mi.Name + "\n\n" + (m.Enabled ? Trf("Position in playset: {0} (enabled)", idx + 1) : Trf("Position in playset: {0} (disabled)", idx + 1)) +
                "\n" + Trf("Descriptor: {0}", mi.Id) + "\n" + Trf("Source: {0}", Cat.SourceLabel(mi.Source)) + "\n" + Trf("Mod version: {0}", mi.Version.Length == 0 ? "-" : mi.Version) +
                "\n" + Trf("Game version: {0}", mi.Supported.Length == 0 ? "-" : mi.Supported) +
                "\n" + files +
                "\n" + (St.Cats.ContainsKey(mi.Id) ? Trf("Type: {0} (set by you)", Cat.Name(CatOfMod(mi.Id))) : Trf("Type: {0} ({1})", Cat.Name(CatOfMod(mi.Id)), Known.GuessCategory(mi).Shown)) +
                (LockedIds().Contains(mi.Id) ? "\n" + Tr("Position: locked (Auto Sort will not move it)") : "") +
                (kn >= 0 ? "\n" + Trf("Known mod: {0}", Known.NoteText(KnownDb[kn])) : "") +
                "\n" + Trf("Dependencies: {0}", Join(mi.Deps)) + "\n" + Trf("Replaces vanilla folders: {0}", Join(mi.ReplacePaths)) + "\n" + Trf("Tags: {0}", Join(mi.Tags)) + "\n";
        }
        if (idx < Issues.Length)
        {
            var iss = Issues[idx];
            if (iss.Count > 0)
            {
                t += "\n" + Tr("Checks:") + "\n";
                foreach (var i in iss) t += (i.Sev == 2 ? Trf("  [problem] {0}", i.Text) : i.Sev == 1 ? Trf("  [warning] {0}", i.Text) : Trf("  [info] {0}", i.Text)) + "\n";
            }
            else if (m.Enabled) t += "\n" + Tr("Checks: no problems found.") + "\n";
        }
        return t;
    }

    // ---- health, reports ----
    public HealthReport RunHealth()
    {
        var ps = Active();
        if (ps == null) return new HealthReport();
        return Health.Check(ps, Info, GameVer, LockedIds(), St.Cats, Conf.Valid ? Conf : null, FileIndex, KnownDb);
    }

    public async Task ShowHealth()
    {
        var ps = Active();
        if (Dlg == null) return;
        if (ps == null) { await Dlg.Info(Tr("Choose or create a playset first.")); return; }
        var hr = RunHealth();
        await Dlg.ShowText(Tr("Health check - The Royal Court"), Health.Text(hr, ps.Name));
    }

    public async Task SortReport()
    {
        var ps = Active();
        if (Dlg == null) return;
        if (ps == null || ps.Mods.Count == 0) { await Dlg.Info(Tr("There is no load order to report yet.")); return; }
        var cats = ps.Mods.Select(m => CatOfMod(m.Id)).ToList();
        string text = Release.BuildSortReport(ps, Info, KnownDb, cats, LockedIds(), GameVer, KnownRev);
        if (!await Dlg.ClipboardSet(text)) { await Dlg.Info(Tr("Could not copy to the clipboard (another program may be holding it). Try again.")); return; }
        Log.Line($"sort report copied ({ps.Mods.Count} mods)");
        Say(Tr("Sort report copied to the clipboard."));
        if (await Dlg.Confirm(Tr("Report a sort problem"), Trf("The load order of \"{0}\" is on your clipboard: mod names, Steam ids, types and which mods the program recognises. No file paths or personal information.\n\nOpen the GitHub issues page now? Write what you expected at the bottom and paste (Ctrl+V).", ps.Name)))
            Platform.OpenShell("https://github.com/" + Release.UpdateRepo + "/issues/new");
    }

    // ---- the CK3 folder ----
    public void SaveFolder(string dir)
    {
        St.Ck3Dir = dir;
        Reload();
        if (EffectiveDir().Length > 0) Say(Trf("Folder saved. Found {0} installed mods.", Mods.Count));
    }

    // ---- Play ----
    public async Task Play()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        string dir = EffectiveDir();
        if (dir.Length == 0) { await Dlg.Info(Tr("The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder.")); return; }
        string exe = (St.GameExe.Length > 0 && Util.FileExists(St.GameExe)) ? St.GameExe : Platform.FindGameExeInSteam();
        if (exe.Length == 0)
        {
            var chosen = await Dlg.PickGameExe();
            if (chosen == null) return;
            if (!string.Equals(Path.GetFileName(chosen), "ck3.exe", StringComparison.OrdinalIgnoreCase)) { await Dlg.Info(Tr("Please choose ck3.exe (it is in the \"binaries\" folder of your Crusader Kings III install).")); return; }
            exe = chosen;
        }
        if (exe != St.GameExe) { St.GameExe = exe; SaveSettingsNow(); }
        if (GameVer.Length == 0) { RefreshGameVersion(exe); Populate(); }
        if (Platform.ProcessRunning("ck3")) { await Dlg.Info(Tr("Crusader Kings III is already running. Close the game first, then press Play again.")); return; }
        if (!Platform.ProcessRunning("steam")) { await Dlg.Info(Tr("Steam needs to be running to start the game without the launcher. Start Steam, then press Play again.")); return; }
        if (St.PlayCheck)
        {
            var hr = RunHealth();
            if (!hr.Ok)
            {
                var list = new StringBuilder();
                int shown = 0;
                foreach (var h in hr.Items)
                {
                    if (h.Sev < 1) continue;
                    if (shown++ >= 4) break;
                    list.Append("  • ").Append(h.Pos >= 0 ? h.Name + ": " : "").Append(h.Text).Append('\n');
                }
                int total = hr.Problems + hr.Warnings;
                if (total > shown) list.Append("  ").Append(Trf("... and {0} more", total - shown)).Append('\n');
                string q = Trf("The health check found {0} problem(s) and {1} warning(s) in this playset:", hr.Problems, hr.Warnings) + "\n\n" + list + "\n" + Tr("Start the game anyway?\nYes: start the game. No: show me the details first.");
                if (!await Dlg.Confirm(Tr("Health check"), q)) { await Dlg.ShowText(Tr("Health check - The Royal Court"), Health.Text(hr, ps.Name)); return; }
            }
        }
        ps = Active()!;
        var inst = new HashSet<string>(Mods.Select(m => m.Id), StringComparer.Ordinal);
        int registered = Steam.RegisterPendingMods(dir, ps, Info, out string regErr);
        if (registered > 0) Log.Line($"registered {registered} Workshop mod(s) that the launcher had not set up yet");
        if (regErr.Length > 0) Log.Line("register failed: " + regErr);
        int notAdded = 0;
        foreach (var m in ps.Mods)   // a Steam download that could not be written into the mod folder cannot be loaded: leave it out and say so
            if (m.Enabled && inst.Contains(m.Id) && Info.TryGetValue(m.Id, out var mi) && mi.Pending && !Util.Exists(Path.Combine(dir, "mod", m.Id))) { inst.Remove(m.Id); notAdded++; }
        var r = Steam.WriteGameModList(dir, ps, inst);
        if (!r.Ok) { await Dlg.Info(r.Message); return; }
        St.Launch.TryGetValue(ps.Name, out var args);
        if (!Platform.StartProgram(exe, args ?? "", out string err)) { await Dlg.Info(Trf("Could not start the game ({0}).", err)); return; }
        foreach (var k in St.Seen.Keys.ToList()) if (!Info.ContainsKey(k)) St.Seen.Remove(k);   // forget mods that are gone
        var fp = FpNow();
        foreach (var m in ps.Mods) if (m.Enabled && fp.TryGetValue(m.Id, out var f)) St.Seen[m.Id] = f;
        Changes.RecordPlay(St, ps, FileIndex, GameVer, Util.NowSeconds());   // what "since you last pressed Play" compares against
        SaveSettingsNow(); RefreshNotes();
        Say(Trf("{0}. Starting Crusader Kings III directly (launcher skipped).", r.Message) + (registered > 0 ? " " + Trf("Registered {0} new Steam mod(s) for you.", registered) : "") + (notAdded > 0 ? " " + Trf("WARNING: {0} Steam mod(s) could not be added to your mod folder and were left out.", notAdded) : ""));
    }
}
