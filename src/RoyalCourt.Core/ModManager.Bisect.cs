using System.Text;
namespace RoyalCourt.Core;

public enum BisectMode { Crash, Errors, Slow }

/// <summary>One enabled mod as the Bisect window sees it.</summary>
public sealed class BisectMod
{
    public int Pos;                    // position in the playset
    public string Id = "", Name = "";
    public int Cat;
    public double Weight = 1;          // how suspicious (relative)
    public string Why = "";            // the strongest reasons, for the user
    public bool Keep;                  // the user wants it left on
    public List<int> DependsOn = new();   // indexes into the BisectMod list
}

/// <summary>Files read for Bisect (disk only, safe to read on another thread).</summary>
public sealed class BisectEvidence
{
    public string GameLog = "", EventStats = "", OnActionStats = "";
    public HashSet<string>? BaseOnActions;
    public bool CrashLog;       // the game log is the one from the newest crash folder
}

// Bisect: find the mod behind a crash, errors or lag by turning mods off in groups and starting the game.
// The playset on screen is never touched; each test writes a temporary copy of it to the game's own mod list.
public sealed partial class ModManager
{
    /// <summary>Reads the logs Bisect needs. Touches only the disk, so it can run off the UI thread.</summary>
    public BisectEvidence ReadBisectEvidence(BisectMode mode)
    {
        var ev = new BisectEvidence();
        string dir = EffectiveDir();
        if (dir.Length == 0) return ev;
        string logs = Path.Combine(dir, "logs");
        try
        {
            if (mode == BisectMode.Slow)
            {
                ev.EventStats = ReportView.ReadHeadOrTail(Path.Combine(logs, "eventstats.csv"), 32 << 20, false);
                ev.OnActionStats = ReportView.ReadHeadOrTail(Path.Combine(logs, "onactionstats.csv"), 32 << 20, false);
                if (ev.EventStats.Length > 0 || ev.OnActionStats.Length > 0) ev.BaseOnActions = ScriptTime.BaseOnActionNames(Vanilla.GameDir(GameExeUsed));
                return ev;
            }
            if (mode == BisectMode.Crash)
            {
                var list = Crash.List(CrashesDir(), 1);
                if (list.Count > 0) { ev.GameLog = Crash.Read(list[0].Path).GameLog; ev.CrashLog = ev.GameLog.Length > 0; }
            }
            if (ev.GameLog.Length == 0)
            {
                ev.GameLog = ReportView.ReadHeadOrTail(Path.Combine(logs, "game.log"), 4 << 20, true);
                string old = ReportView.ReadHeadOrTail(Path.Combine(logs, "error.log"), 2 << 20, true);
                if (old.Length > 0) ev.GameLog += "\n" + old;
            }
        }
        catch (Exception e) { Log.Line("bisect: reading logs failed: " + e.Message); }
        return ev;
    }

    /// <summary>The enabled mods of the playset with a suspicion weight from everything the program knows. Runs on the UI thread (it reads the program's own lists); the logs come from <see cref="ReadBisectEvidence"/>.</summary>
    public List<BisectMod> BisectMods(BisectMode mode, BisectEvidence evidence, out string note)
    {
        note = "";
        var res = new List<BisectMod>();
        var ps = Active();
        if (ps == null) return res;
        string dir = EffectiveDir();
        var byName = new Dictionary<string, int>(StringComparer.Ordinal);
        var posToIdx = new Dictionary<int, int>();
        for (int p = 0; p < ps.Mods.Count; p++)
        {
            var pm = ps.Mods[p];
            if (!pm.Enabled) continue;
            string name = Info.TryGetValue(pm.Id, out var mi) && mi.Name.Length > 0 ? mi.Name : (pm.Name.Length == 0 ? pm.Id : pm.Name);
            var b = new BisectMod { Pos = p, Id = pm.Id, Name = name, Cat = CatOfMod(pm.Id) };
            posToIdx[p] = res.Count; res.Add(b);
            string key = Util.Lower(Util.Trimmed(name));
            if (key.Length > 0 && !byName.ContainsKey(key)) byName[key] = res.Count - 1;
        }
        if (res.Count == 0) return res;
        var why = new List<(double gain, string text)>[res.Count];
        for (int i = 0; i < res.Count; i++) why[i] = new();
        void Boost(int idx, double mult, string text) { if (idx < 0 || idx >= res.Count) return; res[idx].Weight *= mult; why[idx].Add((mult, text)); }

        // what a mod needs (a mod is only turned off together with everything that needs it)
        foreach (var b in res)
            if (Info.TryGetValue(b.Id, out var mi))
                foreach (var d in mi.Deps) if (byName.TryGetValue(Util.Lower(Util.Trimmed(d)), out int di) && !ReferenceEquals(res[di], b)) b.DependsOn.Add(di);

        // what kind of mod: libraries are rarely the cause (and everything that needs them goes off with them), big content and overhauls often are
        for (int i = 0; i < res.Count; i++)
        {
            double k = res[i].Cat switch { Cat.Library => 0.5, Cat.Patch => 0.7, Cat.Translation => 0.7, Cat.Graphics => 0.9, Cat.Ui => 1.0, Cat.Overhaul => 1.5, _ => 1.2 };
            res[i].Weight *= k;
            if (FileIndex.TryGetValue(res[i].Id, out var f) && f.Files.Count > 0) res[i].Weight *= 1 + 0.12 * Math.Log2(1 + f.Files.Count / 40.0);   // more files, more ways to break
        }

        // changed since the game last ran fine
        try
        {
            var pr = Changes.BuildPlayReport(St, ps, Info, FpNow(), FileIndex, GameVer);
            var idxOf = res.Select((b, i) => (b.Id, i)).ToDictionary(x => x.Id, x => x.i, StringComparer.Ordinal);
            foreach (var u in pr.Updated) if (idxOf.TryGetValue(u.ModId, out int i)) Boost(i, 2.2, Tr("updated since you last pressed Play"));
            foreach (var u in pr.Added) if (idxOf.TryGetValue(u.ModId, out int i)) Boost(i, 2.5, Tr("added since you last pressed Play"));
            foreach (var u in pr.Outdated) if (idxOf.TryGetValue(u.ModId, out int i)) Boost(i, 2.0, Tr("made for an older game version"));
        }
        catch (Exception e) { Log.Line("bisect: changes failed: " + e.Message); }

        // the program's own checks, and mods known not to work together
        try
        {
            var iss = Analyze.AnalyzePlayset(ps, Info, GameVer);
            for (int p = 0; p < iss.Length; p++)
            {
                if (!posToIdx.TryGetValue(p, out int i)) continue;
                int sev = iss[p].Count == 0 ? -1 : iss[p].Max(x => x.Sev);
                if (sev >= 2) Boost(i, 3.0, Tr("has a problem in the checks"));
                else if (sev == 1) Boost(i, 1.5, Tr("has a warning in the checks"));
            }
            foreach (var kc in KnownConflicts.Find(ps, Info))
            {
                if (posToIdx.TryGetValue(kc.A, out int a)) Boost(a, 3.0, Tr("known not to work with another mod in this list"));
                if (posToIdx.TryGetValue(kc.B, out int bb)) Boost(bb, 3.0, Tr("known not to work with another mod in this list"));
            }
        }
        catch (Exception e) { Log.Line("bisect: checks failed: " + e.Message); }

        // the evidence for this kind of problem
        if (dir.Length > 0)
        {
            int enabled = res.Count, ready = res.Count(b => FileIndex.TryGetValue(b.Id, out var f) && f.Complete);
            if (ready < enabled) note = Tr("Your mods' files are still being read, so the log evidence is incomplete. Open this again in a moment for better suggestions.");
            try
            {
                if (mode == BisectMode.Slow)
                {
                    var ev = ScriptTime.ParseStatsCsv(evidence.EventStats, false); var oa = ScriptTime.ParseStatsCsv(evidence.OnActionStats, true);
                    if (!ev.Ok && !oa.Ok) note += (note.Length > 0 ? " " : "") + Tr("The game has not written its script statistics yet, so the suggestions use only what is known about the mods. Play for a while, quit the game, then try again.");
                    else
                    {
                        var baseOa = evidence.BaseOnActions ?? new HashSet<string>();
                        var tr = ScriptTime.AttributeTime(ev, oa, ps, DefIndex, baseOa.Count == 0 ? null : baseOa);
                        double all = tr.All > 0 ? tr.All : 1;
                        foreach (var pm in tr.PerMod)
                            if (posToIdx.TryGetValue(pm.Mod, out int i) && pm.Time / all >= 0.01) Boost(i, 1 + 10 * Math.Min(1, pm.Time / all), Trf("{0}% of the script time", (100 * pm.Time / all).ToString("F0")));
                    }
                }
                else if (evidence.GameLog.Length > 0)
                {
                    bool crash = mode == BisectMode.Crash;
                    var lr = GameLog.Attribute(crash ? Crash.LastMessages(evidence.GameLog) : GameLog.Parse(evidence.GameLog), ps, FileIndex, DefIndex, Van);
                    if (crash)
                        foreach (var kv in Crash.Scores(lr))
                        {
                            if (!posToIdx.TryGetValue(kv.Key, out int i)) continue;
                            int cnt = 0; for (int k = 0; k < lr.Parse.Entries.Count; k++) if (lr.ModOf[k] == kv.Key) cnt++;
                            Boost(i, 1 + 0.8 * Math.Min(kv.Value, 20), Trf("named in the log right before the crash ({0} message(s))", cnt));
                        }
                    else
                        foreach (var pm in lr.PerMod)
                        {
                            if (!posToIdx.TryGetValue(pm.Mod, out int i)) continue;
                            double m = 1 + 1.2 * Math.Log2(1 + pm.Errors) + 0.2 * Math.Log2(1 + pm.Warnings);
                            if (m > 1.3) Boost(i, m, Trf("{0} error(s) and {1} warning(s) in the game log", pm.Errors, pm.Warnings));
                        }
                }
                else note += (note.Length > 0 ? " " : "") + Tr("No game log was found, so the suggestions use only what is known about the mods. Start the game once, then try again.");
            }
            catch (Exception e) { Log.Line("bisect: log evidence failed: " + e.Message); }
        }
        for (int i = 0; i < res.Count; i++) res[i].Why = string.Join(", ", why[i].OrderByDescending(x => x.gain).Take(2).Select(x => x.text));
        return res;
    }

    /// <summary>Starts the game with the temporary playset: a copy of the active one, in the same order, with <paramref name="off"/> turned off. The playset on screen is not changed.</summary>
    public async Task<bool> BisectLaunch(IReadOnlyCollection<string> offIds)
    {
        var ps = Active();
        if (ps == null || Dlg == null) return false;
        var chk = await LaunchCheck();
        if (chk == null) return false;
        var off = new HashSet<string>(offIds, StringComparer.Ordinal);
        var tmp = new Playset { Name = ps.Name, Mods = ps.Mods.Select(m => new ModRef(m.Id, m.Enabled && !off.Contains(m.Id), m.Name)).ToList() };
        var r = await StartGame(tmp, chk.Value.dir, chk.Value.exe);
        if (r == null) return false;
        Log.Line($"bisect test: {off.Count} mod(s) off");
        return true;
    }

    /// <summary>Writes the playset on screen back as the game's mod list (the tests left a temporary one there). False when the game is running or it failed.</summary>
    public bool BisectRestore()
    {
        var ps = Active();
        string dir = EffectiveDir();
        if (ps == null || dir.Length == 0 || Platform.ProcessRunning("ck3")) return false;
        var inst = new HashSet<string>(Mods.Select(m => m.Id), StringComparer.Ordinal);
        foreach (var m in ps.Mods)
            if (m.Enabled && inst.Contains(m.Id) && Info.TryGetValue(m.Id, out var mi) && mi.Pending && !Util.Exists(Path.Combine(dir, "mod", m.Id))) inst.Remove(m.Id);
        var r = Steam.WriteGameModList(dir, ps, inst);
        Log.Line("bisect: own mod list written back: " + r.Ok);
        return r.Ok;
    }

    /// <summary>Turns the found mods off in the real playset (after a backup).</summary>
    public void BisectApply(IReadOnlyCollection<string> ids)
    {
        var ps = Active();
        if (ps == null || ids.Count == 0) return;
        var set = new HashSet<string>(ids, StringComparer.Ordinal);
        BackupAffected(set.ToList(), "bisect");
        foreach (var m in ps.Mods) if (set.Contains(m.Id)) m.Enabled = false;
        SinkDisabled(ps); SaveActive(); Populate();
        Post(() => MaybeScan(false));
        Say(Trf("Turned off {0} mod(s) found by Bisect.", set.Count));
    }
}
