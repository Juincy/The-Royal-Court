using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Text;
namespace RoyalCourt.Core;

// The Advanced menu, the crash helper and the update check.
public sealed partial class ModManager
{
    static string SizeText(long b) => b >= (1L << 30) ? ((double)b / (1L << 30)).ToString("F1", CultureInfo.InvariantCulture) + " GB" : ((double)b / (1L << 20)).ToString("F1", CultureInfo.InvariantCulture) + " MB";
    const string FolderNotSet = "The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder.";

    // ---- crash helper ----
    string CrashReport(CrashFolder cf, bool newest)
    {
        var ps = Active();
        var cd = Crash.Read(cf.Path);
        string gameLog = cd.GameLog, debugLog = cd.DebugLog;
        var t = new StringBuilder();
        t.Append(Trf("Crash from {0}", CrashWhen(cf))).Append("   (").Append(cf.Name).Append(")\r\n\r\n");
        t.Append(Tr("WHAT HAPPENED")).Append("\r\n");
        t.Append("  ").Append(Tr(Crash.KindName(cd.Kind)));
        if (cd.Code.StartsWith("EXCEPTION_", StringComparison.Ordinal)) t.Append("  [").Append(cd.Code).Append(']');
        t.Append("\r\n\r\n");
        t.Append(Tr(Crash.KindAdvice(cd.Kind))).Append("\r\n\r\n");
        // logs: the crash folder's own copy; the live log only for the newest crash
        string note = "";
        if (gameLog.Length == 0 && newest)
        {
            string dir = EffectiveDir();
            if (dir.Length > 0)
            {
                string logs = Path.Combine(dir, "logs");
                gameLog = ReportView.ReadHeadOrTail(Path.Combine(logs, "game.log"), 4 << 20, true);
                string old = ReportView.ReadHeadOrTail(Path.Combine(logs, "error.log"), 2 << 20, true);
                if (old.Length > 0) { gameLog += "\n"; gameLog += old; }
                if (debugLog.Length == 0) debugLog = ReportView.ReadHeadOrTail(Path.Combine(logs, "debug.log"), 6 << 20, false);
                if (gameLog.Length > 0) note = Tr("The crash folder had no copy of the logs, so the game's current logs are used. If you started the game again after the crash, they may not match.");
            }
        }
        // suspects
        t.Append(Tr("LIKELY SUSPECTS")).Append("\r\n");
        bool suspects = false;
        if (ps != null && gameLog.Length > 0)
        {
            int enabled = 0, ready = 0;
            foreach (var pm in ps.Mods) if (pm.Enabled) { enabled++; if (FileIndex.TryGetValue(pm.Id, out var f) && f.Complete) ready++; }
            if (ready < enabled) { if (!Scanning) MaybeScan(true); note += (note.Length == 0 ? "" : " ") + Tr("Your mods' files are still being read, so some messages could not be matched yet. Open this again in a moment."); }
            var lr = GameLog.Attribute(Crash.LastMessages(gameLog), ps, FileIndex, DefIndex, Van);
            var sc = Crash.Scores(lr);
            var order = sc.Select(kv => (score: kv.Value, mod: kv.Key)).OrderByDescending(x => x.score).ToList();   // stable, by mod index on ties
            int rank = 0;
            foreach (var o in order)
            {
                if (rank >= 6) break;
                int mod = o.mod;
                long er = 0, wa = 0; LogEntry? top = null;
                for (int k = 0; k < lr.Parse.Entries.Count; k++)
                    if (lr.ModOf[k] == mod)
                    {
                        var e = lr.Parse.Entries[k];
                        if (e.Level == 'E') er += e.Count; else wa += e.Count;
                        if (e.Level == 'E' || top == null) top = e;
                    }
                string nm = Info.TryGetValue(ps.Mods[mod].Id, out var mi) ? mi.Name : ps.Mods[mod].Id;
                t.Append("  ").Append(++rank).Append(".  ").Append(nm).Append("   (#").Append(mod + 1).Append(")\r\n");
                t.Append("      ").Append(Trf("{0} errors and {1} warnings among the last messages the game wrote", er, wa)).Append("\r\n");
                if (top != null) { string mm = ReportView.Message(top); if (mm.Length > 170) mm = mm[..167] + "..."; t.Append("      ").Append(mm).Append("\r\n"); }
                suspects = true;
            }
        }
        if (!suspects) t.Append("  ").Append(Tr("None of the last messages the game wrote could be tied to one of your mods.")).Append("\r\n");
        if (note.Length > 0) t.Append("\r\n  ").Append(note).Append("\r\n");
        t.Append("\r\n");
        // playset then and now
        if (ps != null && debugLog.Length > 0)
        {
            var loaded = GameLog.ParseLoadedMods(debugLog);
            if (loaded.Count > 0)
            {
                var was = new SortedSet<string>(loaded.Where(x => x.Enabled).Select(x => x.Id), StringComparer.Ordinal);
                var cur = new SortedSet<string>(ps.Mods.Where(x => x.Enabled).Select(x => x.Id), StringComparer.Ordinal);
                var added = new List<string>(); var removed = new List<string>();
                string NameOf(string id) => Info.TryGetValue(id, out var mi) ? mi.Name : id;
                foreach (var id in cur) if (!was.Contains(id)) added.Add(NameOf(id));
                foreach (var id in was)
                    if (!cur.Contains(id))
                    {
                        string n = ""; foreach (var lm in loaded) if (lm.Id == id) n = lm.Name;
                        removed.Add(n.Length == 0 ? id : n);
                    }
                t.Append(Tr("THE PLAYSET AT THE TIME")).Append("\r\n");
                if (added.Count == 0 && removed.Count == 0) t.Append("  ").Append(Tr("The game had the same mods turned on that you have turned on now.")).Append("\r\n");
                else
                {
                    t.Append("  ").Append(Trf("Not the playset you have now: {0} mod(s) turned on since, {1} turned off since.", added.Count, removed.Count)).Append("\r\n");
                    for (int i = 0; i < added.Count && i < 8; i++) t.Append("      + ").Append(added[i]).Append("\r\n");
                    for (int i = 0; i < removed.Count && i < 8; i++) t.Append("      - ").Append(removed[i]).Append("\r\n");
                }
                t.Append("\r\n");
            }
        }
        // what to try
        t.Append(Tr("WHAT TO TRY")).Append("\r\n");
        t.Append("  1. ").Append(Tr("Turn off the top suspect (or the mod you added or updated most recently) and start the game again.")).Append("\r\n");
        t.Append("  2. ").Append(Tr("Use Auto Sort and the Conflicts window to look for mods that overwrite each other or load in the wrong order.")).Append("\r\n");
        t.Append("  3. ").Append(Tr("Check the mod's Workshop page: is it made for your game version? Do other players report crashes?")).Append("\r\n");
        t.Append("  4. ").Append(Tr("If the crash keeps happening, turn mods off in halves to find the one that causes it.")).Append("\r\n\r\n");
        if (cd.Frames.Count > 0)
        {
            t.Append(Tr("FIRST LINES OF THE CALL STACK (for bug reports)")).Append("\r\n");
            foreach (var f in cd.Frames) t.Append("  ").Append(f).Append("\r\n");
            t.Append("\r\n");
        }
        if (cd.Meta.Count > 0) { t.Append(Tr("BUILD")).Append("\r\n"); foreach (var mm in cd.Meta) t.Append("  ").Append(mm).Append("\r\n"); t.Append("\r\n"); }
        t.Append(Trf("Crash folder: {0}", cf.Path)).Append("\r\n");
        t.Append(Tr("The crash files stay as they are; this window only reads them. Select all (Ctrl+A) and copy (Ctrl+C) to send this text to a mod author."));
        return t.ToString();
    }

    public async Task CrashHelper()
    {
        if (Dlg == null) return;
        string dir = EffectiveDir();
        if (dir.Length == 0) { await Dlg.Info(Tr("The CK3 folder is not set. Enter it in the main window first.")); return; }
        var list = Crash.List(CrashesDir(), 12);
        if (list.Count == 0) { await Dlg.Info(Trf("No crash found. When the game crashes it writes a folder in {0}.", CrashesDir())); return; }
        int pick = 0;
        if (list.Count > 1)
        {
            var names = list.Select((c, i) => (i == 0 ? Tr("Newest: ") : "") + CrashWhen(c)).ToList();
            pick = await Dlg.PickItem(Tr("Crash helper"), names);
            if (pick < 0) return;
        }
        await Dlg.ShowText(Trf("Crash helper - {0}", CrashWhen(list[pick])), CrashReport(list[pick], pick == 0));
        St.CrashSeen = list[0].Name; SaveSettingsNow();
    }

    // ---- saves, shader cache ----
    public async Task DeleteAllSaves()
    {
        if (Dlg == null) return;
        string dir = EffectiveDir();
        if (dir.Length == 0) { await Dlg.Info(Tr(FolderNotSet)); return; }
        if (Platform.ProcessRunning("ck3")) { await Dlg.Info(Tr("Crusader Kings III is running. Close the game first, then try again.")); return; }
        var sc = ModScan.ScanSaves(dir);
        if (sc.Entries.Count == 0) { await Dlg.Info(Trf("No saves found in {0}", sc.Folder)); return; }
        string q = Trf("Delete ALL saved games?\n\n{0} save file(s), {1}\nin {2}\n\nThey are moved to the Recycle Bin (anything too big for it is deleted permanently). If Steam Cloud sync is on, Steam may bring cloud copies back.", sc.Files, SizeText(sc.Bytes), sc.Folder);
        if (!await Dlg.Confirm(Tr("Delete all saves"), q)) return;
        bool ok = await Task.Run(() => Platform.RecycleToBin(sc.Entries));
        var after = ModScan.ScanSaves(dir);
        if (!ok || after.Entries.Count > 0) Say(Trf("Some saves could not be deleted ({0} item(s) left in the save folder).", after.Entries.Count));
        else Say(Trf("Deleted {0} save file(s) ({1}) to the Recycle Bin.", sc.Files, SizeText(sc.Bytes)));
    }

    public async Task ClearShaders()
    {
        if (Dlg == null) return;
        string dir = EffectiveDir();
        if (dir.Length == 0) { await Dlg.Info(Tr(FolderNotSet)); return; }
        if (Platform.ProcessRunning("ck3")) { await Dlg.Info(Tr("Crusader Kings III is running. Close the game first, then try again.")); return; }
        var sc = ModScan.ScanShaderCache(dir);
        if (sc.Folders.Count == 0) { await Dlg.Info(Tr("No shader cache folder was found in your Crusader Kings III folder. Nothing to clear.")); return; }
        if (sc.Entries.Count == 0) { await Dlg.Info(Tr("The shader cache folder is already empty.")); return; }
        string q = Trf("Clear the shader cache?\n\n{0} file(s), {1}\nin {2}\n\nThe game rebuilds it by itself, so the first start afterwards can be a little slower and stutter more for a while. Your saves, mods and settings are not touched.", sc.Files, SizeText(sc.Bytes), sc.Folders[0]);
        if (!await Dlg.Confirm(Tr("Clear shader cache"), q)) return;
        int failed = await Task.Run(() => ModScan.ClearShaderCache(sc));
        Log.Line($"shader cache cleared: {sc.Files} files, {failed} failed");
        if (failed > 0) Say(Trf("Some shader cache items could not be removed ({0}). Close the game and other programs and try again.", failed));
        else Say(Trf("Shader cache cleared ({0} file(s), {1}). The game builds it again on the next start.", sc.Files, SizeText(sc.Bytes)));
    }

    // ---- backups ----
    public void BackupNow()
    {
        var ps = Active();
        if (ps == null) return;
        string f = Core.Playsets.Backup(ps, Info, "manual");
        Say(f.Length == 0 ? Tr("This playset is already backed up exactly as it is now.") : Trf("Backup saved: {0}.", Util.FileStem(f)));
    }
    public string BackupsFolder() { var ps = Active(); return ps != null ? Core.Playsets.BackupsDir(ps.Name) : Path.Combine(Util.DataDir(), "Backups"); }

    public async Task RestoreBackup()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        if (Core.Playsets.ListBackups(ps.Name).Count == 0) { await Dlg.Info(Tr("This playset has no backups yet. One is made automatically before every Auto Sort, and when the app starts if the playset changed.")); return; }
        var path = await Dlg.PickBackup(Core.Playsets.BackupsDir(ps.Name));
        if (path == null) return;
        if (!Util.ReadFile(path, out var text, 5L << 20)) { await Dlg.Info(Tr("Could not read that backup.")); return; }
        var r = Core.Playsets.Parse(text, Mods);
        if (!r.Ok) { await Dlg.Info(r.Error); return; }
        if (!await Dlg.Confirm(Name, Trf("Restore \"{0}\" to the load order and enabled mods saved in:\n{1}\n\nYour current state is backed up first, so you can come back.", ps.Name, Util.FileStem(path)))) return;
        ps = Active()!;
        Core.Playsets.Backup(ps, Info, "before restore");
        var from = r.Playset;
        Core.Playsets.Sync(from, Mods);
        Core.Playsets.ApplyBackupOrder(ps, from);
        UndoIds.Clear();
        SaveActive(); Populate();
        Say(Tr("Playset restored from the backup."));
    }

    // ---- other Advanced entries ----
    public void ShowRemovedMods() { St.Hidden.Clear(); SaveSettingsNow(); Resync(false); }

    public async Task LaunchOptions()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        St.Launch.TryGetValue(ps.Name, out var cur);
        var v = await Dlg.Ask(Tr("Game launch options"), Tr("Extra options for the game, e.g. -debug_mode (empty = none):"), cur ?? "");
        if (v == null) return;
        string t = v.Trim(' ');
        if (t.Length == 0) St.Launch.Remove(ps.Name); else St.Launch[ps.Name] = t;
        SaveSettingsNow();
        Say(t.Length == 0 ? Tr("Launch options cleared.") : Trf("Launch options saved for \"{0}\".", ps.Name));
    }

    public async Task ComparePlaysets()
    {
        var ps = Active();
        if (ps == null || Dlg == null) return;
        var others = Playsets.Where(p => p.Name != ps.Name).ToList();
        if (others.Count == 0) { await Dlg.Info(Tr("You need a second playset to compare with.")); return; }
        int pick = await Dlg.PickItem(Tr("Compare playsets"), others.Select(p => p.Name).ToList());
        if (pick < 0) return;
        var other = others[pick];
        var d = Compare.ComparePlaysets(ps, other, Info);
        static string Sect(string head, List<string> v)
        {
            var o = new StringBuilder(Trf("{0} ({1})", head, v.Count) + "\r\n");
            if (v.Count == 0) o.Append("   -\r\n");
            foreach (var x in v) o.Append("   ").Append(x).Append("\r\n");
            return o.Append("\r\n").ToString();
        }
        var t = new StringBuilder(Trf("\"{0}\"  vs  \"{1}\"", ps.Name, other.Name) + "\r\n\r\n" + Trf("{0} mods are enabled in both.", d.Shared) + "  " + (d.SameOrder ? Tr("Load order of those: identical.") : Tr("Load order of those: different.")) + "\r\n\r\n");
        t.Append(Sect(Trf("Only enabled in {0}", ps.Name), d.OnlyA)).Append(Sect(Trf("Only enabled in {0}", other.Name), d.OnlyB)).Append(Sect(Tr("Enabled in one, disabled in the other"), d.EnabledDiffers));
        if (!d.SameOrder) t.Append(Sect(Tr("Mods at a different place in the shared load order"), d.Moved));
        await Dlg.ShowText(Tr("Compare playsets"), t.ToString());
    }

    public async Task Compatches()
    {
        var ps = Active();
        if (Dlg == null) return;
        if (ps == null) { await Dlg.Info(Tr("Choose or create a playset first.")); return; }
        await Dlg.ShowText(Tr("Compatch finder - The Royal Court"), Known.CompatText(Known.FindCompatches(ps, Info), ps.Name));
    }

    /// <summary>The mod's own picture: thumbnail.png in its folder ("" when unknown or in an archive). Nothing is downloaded.</summary>
    public string ThumbPath(string id) => Info.TryGetValue(id, out var mi) && mi.ContentState == 1 && mi.ContentDir.Length > 0 ? Path.Combine(mi.ContentDir, "thumbnail.png") : "";
    public void TogglePictures()
    {
        St.Pictures = !St.Pictures; SaveSettingsNow();
        Say(St.Pictures ? Tr("Mod pictures are shown in the list.") : Tr("Mod pictures are hidden."));
    }
    public void TogglePlayCheck()
    {
        St.PlayCheck = !St.PlayCheck; SaveSettingsNow();
        Say(St.PlayCheck ? Tr("The health check will run when you press Play.") : Tr("The health check will no longer run when you press Play. You can still start it from Advanced."));
    }
    public void ToggleAutoUpdate()
    {
        St.CheckUpdates = !St.CheckUpdates; SaveSettingsNow();
        Say(St.CheckUpdates ? Tr("The program will look for updates when it starts.") : Tr("The program will no longer look for updates by itself."));
    }

    /// <summary>Opens the diagnostics log (created first if it does not exist yet).</summary>
    public void OpenLog() { if (!File.Exists(Log.LogPath())) Log.Line("log opened by user"); Platform.OpenShell(Log.LogPath()); }
    public async Task OpenUserFolder(string sub)
    {
        string dir = EffectiveDir();
        string p = Path.Combine(dir, sub);
        if (dir.Length == 0 || !Directory.Exists(p)) { if (Dlg != null) await Dlg.Info(Tr("That folder does not exist yet.")); return; }
        Platform.OpenShell(p);
    }

    // ---- updates (GitHub Releases) ----
    // Nothing is contacted until the Updates button is pressed (or, if switched on in Advanced, once at startup).
    // Only https://github.com/(this project)/releases/... is ever downloaded, and the exe must match SHA256SUMS.txt from the same release.
    bool updBusy;
    ReleaseInfo updRel = new();
    public event Action? RequestClose;     // the update is installed: the window closes (saving its state) and the new copy takes over

    static readonly HttpClient Http = MakeHttp();
    static HttpClient MakeHttp()
    {
        var h = new HttpClient(new SocketsHttpHandler { AutomaticDecompression = DecompressionMethods.All, ConnectTimeout = TimeSpan.FromSeconds(10) }) { Timeout = TimeSpan.FromSeconds(60) };
        h.DefaultRequestHeaders.UserAgent.ParseAdd("TheRoyalCourt/" + Env.Version);
        h.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
        return h;
    }

    /// <summary>Error texts stay in English until they reach the screen (the text table is looked up on the interface thread).</summary>
    static async Task<(bool ok, byte[] data, string err)> HttpGet(string url, long maxBytes)
    {
#if RC_TEST_HOOKS
        string? testDir = Environment.GetEnvironmentVariable("RC_UPDATE_DIR");
        if (!string.IsNullOrEmpty(testDir))
        {
            string leaf = url.Contains("/releases/latest", StringComparison.Ordinal) ? "latest.json" : url[(url.LastIndexOf('/') + 1)..];
            try { return (true, await File.ReadAllBytesAsync(Path.Combine(testDir, leaf)), ""); } catch { return (false, Array.Empty<byte>(), "Test release file missing: " + leaf); }
        }
#endif
        if (!Uri.TryCreate(url, UriKind.Absolute, out var u) || u.Scheme != Uri.UriSchemeHttps) return (false, Array.Empty<byte>(), "Bad update address.");
        try
        {
            using var rsp = await Http.GetAsync(u, HttpCompletionOption.ResponseHeadersRead);
            int status = (int)rsp.StatusCode;
            if (status == 404) return (false, Array.Empty<byte>(), "No release was found on GitHub yet.");
            if (status == 403 || status == 429) return (false, Array.Empty<byte>(), "GitHub is limiting requests right now. Try again in a little while.");
            if (status != 200) return (false, Array.Empty<byte>(), "GitHub answered with error " + status + ".");
            await using var s = await rsp.Content.ReadAsStreamAsync();
            using var ms = new MemoryStream();
            var buf = new byte[65536];
            for (;;)
            {
                int n = await s.ReadAsync(buf);
                if (n <= 0) break;
                if (ms.Length + n > maxBytes) return (false, Array.Empty<byte>(), "The download is larger than expected.");
                ms.Write(buf, 0, n);
            }
            return (true, ms.ToArray(), "");
        }
        catch (HttpRequestException) { return (false, Array.Empty<byte>(), "Could not reach GitHub (are you online?)."); }
        catch (TaskCanceledException) { return (false, Array.Empty<byte>(), "Could not reach GitHub (are you online?)."); }
        catch (IOException) { return (false, Array.Empty<byte>(), "The connection was interrupted."); }
    }

    static string TrErr(string e)
    {
        string[] known = {
            "Bad update address.", "Could not start the Windows network library.", "Could not reach GitHub.", "Could not reach GitHub (are you online?).",
            "No release was found on GitHub yet.", "GitHub is limiting requests right now. Try again in a little while.", "The connection was interrupted.",
            "The download is larger than expected.", "GitHub's answer could not be read.", "This release has no update file attached.",
            "The release's checksum list does not include the program.", "The downloaded file is not a program.",
            "The downloaded file does not match its checksum, so it was thrown away." };
        foreach (var k in known) if (e == k) return Tr(k);
        const string pre = "GitHub answered with error ";
        if (e.StartsWith(pre, StringComparison.Ordinal) && e.Length > pre.Length + 1) return Trf("GitHub answered with error {0}.", e[pre.Length..^1]);
        return e;
    }

    public async Task CheckForUpdates(bool silent)
    {
        if (updBusy) { if (!silent) Say(Tr("Already checking...")); return; }
        if (!silent && updRel.Tag.Length > 0 && Release.CompareVersions(updRel.Tag, Env.Version) > 0) { await ShowUpdateDialog(updRel); return; }   // already found by the startup check
        updBusy = true;
        try
        {
            if (!silent) Say(Tr("Checking GitHub for a newer version..."));
            string err = ""; ReleaseInfo rel = new(); bool ok = false;
            var (gok, body, gerr) = await HttpGet("https://api.github.com/repos/" + Release.UpdateRepo + "/releases/latest", 1 << 20);
            if (gok) { rel = Release.Parse(Util.Utf8.GetString(body)); ok = rel.Ok; if (!ok) err = "GitHub's answer could not be read."; } else err = gerr;
            // The shared known-mods list comes from the same place (fixed address in this project's repo); it is validated before it is kept.
            var (kok, kbody, _) = await HttpGet("https://raw.githubusercontent.com/" + Release.UpdateRepo + "/main/knownmods.json", 512 << 10);
            string knownMsg = kok ? TakeOnlineKnown(Util.Utf8.GetString(kbody)) : "";
            if (!ok)
            {
                Log.Line("update check failed: " + err);
                if (silent) return;
                Say(knownMsg.Length == 0 ? Tr("Update check failed.") : knownMsg);
                if (Dlg != null) await Dlg.Info(TrErr(err) + "\n\n" + Tr("You can always get the latest version from the GitHub page.") + (knownMsg.Length == 0 ? "" : "\n\n" + knownMsg));
                return;
            }
            bool newer = Release.CompareVersions(rel.Tag, Env.Version) > 0;
            Log.Line("update check: latest " + rel.Tag + (newer ? " (newer)" : " (up to date)"));
            if (!newer)
            {
                if (!silent) { Say(knownMsg.Length == 0 ? Tr("You have the latest version.") : knownMsg); if (Dlg != null) await Dlg.Info(Trf("You have the latest version ({0}).", Env.Version) + (knownMsg.Length == 0 ? "" : "\n\n" + knownMsg)); }
                else if (knownMsg.Length > 0) Say(knownMsg);
                return;
            }
            updRel = rel;
            if (silent) { Say(Trf("A newer version ({0}) is available - press Updates.", rel.Tag)); return; }
            Say(Trf("A newer version is available: {0}.", rel.Tag));
            updBusy = false;
            await ShowUpdateDialog(rel);
        }
        finally { updBusy = false; }
    }

    static string PlainNotes(string md)
    {
        var o = new StringBuilder();
        for (int i = 0; i < md.Length && o.Length < 3000; i++)
        {
            char c = md[i];
            if (c == '*' || c == '`' || c == '\r') continue;
            if (c == '#') { while (i + 1 < md.Length && (md[i + 1] == '#' || md[i + 1] == ' ')) i++; continue; }
            o.Append(c);
        }
        return o.ToString();
    }

    async Task ShowUpdateDialog(ReleaseInfo r)
    {
        if (Dlg == null) return;
        bool canInstall = r.Asset(Release.UpdateExeAsset) != null && r.Asset(Release.UpdateSumsAsset) != null;
        string head = Trf("Version {0} is available", r.Tag);
        string body = Trf("You have version {0}.", Env.Version) + " " + (canInstall ? Tr("\"Update now\" downloads the new program, checks it against the release's checksum, replaces this one and restarts. Your playsets and settings are not touched.") : Tr("Open the release page to download it."));
        int choice = await Dlg.UpdateChoice(head, body, PlainNotes(r.Body), canInstall, r.PageUrl.Length > 0);
        if (choice == 1002 && r.PageUrl.Length > 0) Platform.OpenShell(r.PageUrl);
        else if (choice == 1001) await DownloadAndInstall(r);
    }

    /// <summary>Downloads the exe and SHA256SUMS.txt of the release, and keeps the exe only if its checksum matches.</summary>
    async Task DownloadAndInstall(ReleaseInfo r)
    {
        updBusy = true;
        try
        {
            Say(Trf("Downloading version {0}...", r.Tag));
            string err = ""; byte[] exe = Array.Empty<byte>(); bool ok = false;
            var ea = r.Asset(Release.UpdateExeAsset); var sa = r.Asset(Release.UpdateSumsAsset);
            if (ea == null || sa == null) err = "This release has no update file attached.";
            else
            {
                var (s1, sums, e1) = await HttpGet(sa.Url, 1 << 16);
                if (!s1) err = e1;
                else
                {
                    var (s2, data, e2) = await HttpGet(ea.Url, 64L << 20);
                    if (!s2) err = e2;
                    else
                    {
                        string want = Release.FindSumFor(Util.Utf8.GetString(sums), Release.UpdateExeAsset);
                        if (want.Length == 0) err = "The release's checksum list does not include the program.";
                        else if (data.Length < 100000 || data[0] != (byte)'M' || data[1] != (byte)'Z') err = "The downloaded file is not a program.";
                        else if (Release.Sha256Hex(data) != want) err = "The downloaded file does not match its checksum, so it was thrown away.";
                        else { ok = true; exe = data; }
                    }
                }
            }
            string shown = TrErr(err);
            if (ok && InstallUpdate(exe, out shown))
            {
                Log.Line("updated to " + r.Tag + ", restarting");
                RequestClose?.Invoke();   // saves the window state, then the new copy takes over
                return;
            }
            Log.Line("update failed: " + shown);
            Say(Tr("Update failed."));
            if (Dlg != null) await Dlg.Info(shown + "\n\n" + Tr("Your current version was left as it is."));
        }
        finally { updBusy = false; }
    }

    /// <summary>Swaps the running exe for the downloaded one (Windows lets a running exe be renamed, not overwritten) and starts the new copy.</summary>
    static bool InstallUpdate(byte[] data, out string err)
    {
        err = "";
        string self = Environment.ProcessPath ?? "";
        if (self.Length == 0) { err = Tr("Could not replace the program file (Windows error {0}).").Replace("{0}", "?"); return false; }
        string neu = self + ".new", old = self + ".old";
        if (!Util.WriteFileBytes(neu, data)) { err = Tr("Could not write next to the program (is it in a protected folder like Program Files?). Download the new version from GitHub instead."); return false; }
        try { File.Delete(old); } catch { }
        try { File.Move(self, old, true); }
        catch (Exception e) { try { File.Delete(neu); } catch { } err = Trf("Could not replace the program file (Windows error {0}).", e.HResult & 0xFFFF); return false; }
        try { File.Move(neu, self, true); }
        catch
        {
            try { File.Move(old, self, true); } catch { }   // put the old one back
            try { File.Delete(neu); } catch { }
            err = Tr("Could not put the new program in place.");
            return false;
        }
        if (!Platform.StartProgram(self, "--wait-pid " + Environment.ProcessId, out string e2)) { err = Trf("The update was installed but the new copy could not be started ({0}). Start the program again.", e2); return false; }
        return true;
    }

    /// <summary>The old program file from an update is removed once this version has run for a while (until then it is the way back).</summary>
    public static void RemoveOldCopy()
    {
        try { string old = (Environment.ProcessPath ?? "") + ".old"; if (old.Length > 4) File.Delete(old); } catch { }
    }

    /// <summary>Keeps a downloaded known-mods list if it is valid and newer than the one in use. Returns a sentence for the user ("" = nothing changed).</summary>
    string TakeOnlineKnown(string text)
    {
        if (text.Length == 0) return "";
        var oi = Known.CheckOnline(text);
        if (!oi.Ok) { Log.Line("known-mods list from GitHub was not usable, ignored"); return ""; }
        if (oi.Revision <= KnownRev) { Log.Line("known-mods list is up to date (revision " + KnownRev + ")"); return ""; }
        if (!Util.WriteFileAtomic(Known.OnlineFile(), text)) { Log.Line("could not save the known-mods list"); return ""; }
        KnownDb = Known.Load(out KnownRev);
        Log.Line($"known-mods list updated to revision {KnownRev} ({oi.Count} entries)");
        return Trf("Known-mods list updated (revision {0}). Press Auto Sort to use it.", KnownRev);
    }
}
