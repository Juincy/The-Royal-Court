using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using RoyalCourt.Core;
using static RoyalCourt.Core.Lang;
namespace RoyalCourt.Ui;

/// <summary>Bisect: the program turns groups of mods off in a temporary copy of the playset and starts the game; you say whether the problem still happened.</summary>
public sealed class BisectWindow : ToolWindow
{
    BisectMode mode = BisectMode.Crash;
    List<BisectMod> mods = new();
    List<int> view = new();
    BisectSession? ses;
    bool loading, launched, tested;
    string note = "";
    HashSet<int> offSet = new(), remSet = new(), groupSet = new(), newSet = new(), culpSet = new(), lockSet = new(), allCulp = new(), extraSet = new();
    readonly RcButton bCrash, bErrors, bSlow, bBegin, bStart, bBroken, bFine, bUndo;

    public static void Open(MainWindow main)
    {
        var ex = main.SubWindows.OfType<BisectWindow>().FirstOrDefault();
        if (ex != null) { if (ex.WindowState == WindowState.Minimized) ex.WindowState = WindowState.Normal; ex.Activate(); return; }
        var w = new BisectWindow(main);
        w.Show(main);
        if (!main.mm.Scanning) main.mm.MaybeScan(true);
        _ = w.Load();
    }

    BisectWindow(MainWindow owner) : base(owner, Tr("Bisect - The Royal Court"), 1100, 640, 940, 400)
    {
        bCrash = Btn("☠", Tr("It crashes"), 120, () => mode == BisectMode.Crash);
        bErrors = Btn("⚠", Tr("It misbehaves"), 130, () => mode == BisectMode.Errors);
        bSlow = Btn("⏱", Tr("It is slow"), 110, () => mode == BisectMode.Slow);
        bBegin = Btn("▶", Tr("Begin"), 90, () => ses == null);
        bStart = Btn("🎮", Tr("Start test game"), 150, () => ses != null && !ses.Done && !launched);
        bBroken = Btn("✗", Tr("It still happened"), 150);
        bFine = Btn("✓", Tr("It did not happen"), 150);
        bUndo = Btn("↶", Tr("Undo"), 80);
        left.Children.Add(bCrash); left.Children.Add(bErrors); left.Children.Add(bSlow);
        right.Children.Add(bBegin); right.Children.Add(bStart); right.Children.Add(bBroken); right.Children.Add(bFine); right.Children.Add(bUndo);
        bCrash.Click += () => SetMode(BisectMode.Crash); bErrors.Click += () => SetMode(BisectMode.Errors); bSlow.Click += () => SetMode(BisectMode.Slow);
        bBegin.Click += () => Invalidate(Begin);
        bStart.Click += () => Invalidate(StartTest);
        bBroken.Click += () => Report(true);
        bFine.Click += () => Report(false);
        bUndo.Click += () => { if (ses != null && ses.CanUndo) { ses.Undo(); launched = false; Fill(); } };
        filter.IsVisible = false;
        SetColumns(new[] { (Tr("Status"), 150), (Tr("Mod"), 330), (Tr("Type"), 90), (Tr("Suspicion"), 110), (Tr("Why"), 300) });
        list.Cell = Cell;
        list.CellBar = (r, c) => c == 3 && r < view.Count && mods.Count > 0 ? Math.Min(1, mods[view[r]].Weight / Math.Max(1, mods.Max(m => m.Weight))) : -1;
        list.CellFg = (r, c) =>
        {
            if (c != 0 || r >= view.Count) return null;
            var t = Skin.Cur; int i = view[r];
            if (ses != null && allCulp.Contains(i) && ses.Phase == BisectPhase.Found) return t.Bad;
            if (ses != null && offSet.Contains(i)) return t.Warn;
            if (mods[i].Keep || lockSet.Contains(i)) return t.Gold;
            return Cleared(i) ? t.Muted : null;
        };
        list.RowKind = r => r < view.Count && Cleared(view[r]) ? 1 : 0;
        list.Activated += r => { if (ses == null && r >= 0 && r < view.Count) { mods[view[r]].Keep = !mods[view[r]].Keep; Fill(); } };
        KeyDown += (_, e) => { if (e.Key == Key.Escape) Close(); };
        Closed += (_, _) => { if (tested) { tested = false; if (!mm.BisectRestore()) mm.Say(Tr("Bisect ended while the game was running (or the list could not be written). Press Play to start the game with your own list.")); } };
        Fill();
    }

    // for the screenshot tool: runs a round without the dialogs
    public void Test_Begin() { ses = new BisectSession(mods.Count, i => mods[i].Weight, i => mods[i].DependsOn, Array.Empty<int>()); launched = true; Fill(); }
    public void Test_Report(bool still) { Report(still); launched = true; Fill(); }

    bool Cleared(int i) => ses != null && !ses.Done && !remSet.Contains(i) && !offSet.Contains(i) && !lockSet.Contains(i) && ses.Phase != BisectPhase.Baseline;

    string Cell(int r, int c)
    {
        if (r >= view.Count) return "";
        int i = view[r]; var m = mods[i];
        switch (c)
        {
            case 0:
                if (ses != null)
                {
                    if (ses.Phase == BisectPhase.Found && allCulp.Contains(i)) return Tr("Found it");
                    if (!ses.Done && groupSet.Contains(i)) return ses.Phase == BisectPhase.Confirming ? Tr("Off: last check") : (newSet.Contains(i) ? Tr("Off (new)") : Tr("Off"));
                    if (culpSet.Contains(i)) return Tr("Off: part of it");
                    if (!ses.Done && offSet.Contains(i)) return Tr("Off (needs the above)");
                    if (lockSet.Contains(i)) return Tr("Stays on");
                    if (Cleared(i)) return Tr("Cleared");
                    return ses.Done ? "" : Tr("Suspect");
                }
                return m.Keep ? Tr("Stays on") : Tr("Suspect");
            case 1: return m.Name;
            case 2: return Cat.Name(m.Cat);
            case 3: return m.Weight.ToString("F1");
            default: return m.Why;
        }
    }

    async Task Load()
    {
        if (loading) return;
        loading = true; status.Text = Tr("Looking at your mods and logs...");
        var keep = new HashSet<string>(mods.Where(m => m.Keep).Select(m => m.Id), StringComparer.Ordinal);
        var md = mode; string n = "";
        var evd = await Task.Run(() => mm.ReadBisectEvidence(md));   // the logs are read off the UI thread, the ranking runs here (it reads the program's own lists)
        var res = mm.BisectMods(md, evd, out n);
        loading = false;
        foreach (var m in res) m.Keep = keep.Contains(m.Id);
        mods = res; note = n; Fill();
    }

    void SetMode(BisectMode m)
    {
        if (ses != null || loading || mode == m) return;
        mode = m; foreach (var b in new[] { bCrash, bErrors, bSlow }) b.InvalidateVisual();
        _ = Load();
    }

    public override void RefreshAll() { if (ses == null && !loading) _ = Load(); }

    void Fill()
    {
        offSet = ses == null ? new() : new(ses.Off); remSet = ses == null ? new() : new(ses.Remaining); groupSet = ses == null ? new() : new(ses.Group); newSet = ses == null ? new() : new(ses.NewInGroup);
        culpSet = ses == null ? new() : new(ses.Culprits); lockSet = ses == null ? new() : new(ses.Locked); allCulp = ses == null ? new() : new(ses.AllCulprits);
        filling = true;
        string f = Util.Lower(filter.Text ?? "");
        view = Enumerable.Range(0, mods.Count).Where(i => f.Length == 0 || Util.Lower(mods[i].Name).Contains(f)).OrderByDescending(i => mods[i].Weight).ThenBy(i => mods[i].Pos).ToList();
        filling = false;
        list.Count = view.Count; list.Sel.Clear(); list.Refresh();
        bBegin.IsEnabled = ses == null && !loading && mods.Count > 1;
        bStart.IsEnabled = ses != null && !ses.Done && !launched;
        bBroken.IsEnabled = ses != null && !ses.Done && launched;
        bFine.IsEnabled = ses != null && !ses.Done && launched;
        bUndo.IsEnabled = ses != null && ses.CanUndo;
        bCrash.IsEnabled = bErrors.IsEnabled = bSlow.IsEnabled = ses == null && !loading;
        foreach (var b in new[] { bCrash, bErrors, bSlow, bBegin, bStart }) b.InvalidateVisual();
        if (loading) return;
        if (ses == null)
            status.Text = mods.Count < 2 ? Tr("Bisect needs a playset with at least two mods turned on.")
                        : Trf("{0} mods are on, most suspicious first. Double-click one to keep it on during the tests, then press Begin.", mods.Count) + (note.Length > 0 ? " " + note : "");
        else status.Text = Instruction();
    }

    string Instruction()
    {
        var s = ses!;
        if (s.Done) return s.Phase == BisectPhase.Found ? Tr("Done.") : Tr("Finished without finding a single mod.");
        int off = s.Off.Count;
        string left = Trf("about {0} more test(s)", s.RoundsLeft);
        string what = s.Phase == BisectPhase.Baseline ? Trf("Test: all {0} suspect mods are off.", s.Group.Count)
                    : s.Phase == BisectPhase.Confirming ? Tr("Last check: only the main suspect is off.")
                    : Trf("Test: {0} mods are off ({1} of them new).", off, s.NewInGroup.Count);
        string how = !launched ? Tr("Press Start test game.") : Tr("Play until the problem would show up (or you are sure it will not), close the game, then say what happened.");
        return what + " " + how + "   (" + left + ")";
    }

    async Task Begin()
    {
        if (ses != null || mods.Count < 2) return;
        var keep = mods.Select((m, i) => (m, i)).Where(x => x.m.Keep).Select(x => x.i).ToList();
        string q = Trf("Bisect finds the mod behind a problem by starting the game several times with different mods turned off.\n\nIt will take about {0} tests. Each one: press Start test game, play until the problem would show up, close the game, then tell me whether it happened.\n\nYour real playset is NOT changed. The tests use a temporary copy, and your own list is written back when Bisect ends.\n\nBegin?", (int)Math.Ceiling(Math.Log2(Math.Max(2, mods.Count - keep.Count))) + 2);
        if (!await main.Dlg.Confirm(Tr("Bisect"), q)) return;
        var ms = mods;
        ses = new BisectSession(ms.Count, i => ms[i].Weight, i => ms[i].DependsOn, keep);
        launched = false; Fill();
    }

    async Task StartTest()
    {
        if (ses == null || ses.Done) return;
        var ids = ses.Off.Select(i => mods[i].Id).ToList();
        bStart.IsEnabled = false;
        bool ok = await mm.BisectLaunch(ids);
        launched = ok; if (ok) tested = true; Fill();
        if (ok) Activate();
    }

    void Report(bool still)
    {
        if (ses == null || ses.Done) return;
        ses.Report(still); launched = false; Fill();
        if (ses.Done) _ = Finish();
    }

    async Task Finish()
    {
        var s = ses!;
        if (s.Phase == BisectPhase.Found)
        {
            var ids = s.AllCulprits.Select(i => mods[i].Id).ToList();
            string names = string.Join("\n", s.AllCulprits.Select(i => "  • " + mods[i].Name));
            // mods that need a culprit cannot work without it: they go off with it
            var off = new HashSet<int>(s.AllCulprits); bool grew = true;
            while (grew) { grew = false; for (int i = 0; i < mods.Count; i++) if (!off.Contains(i) && mods[i].DependsOn.Any(off.Contains)) { off.Add(i); grew = true; } }
            var dep = off.Except(s.AllCulprits).ToList();
            ids = off.Select(i => mods[i].Id).ToList();
            string q = Trf("Found it:\n\n{0}\n\nThe problem goes away when it is off.", names) + (dep.Count > 0 ? "\n\n" + Trf("{0} other mod(s) need it and go off with it:", dep.Count) + "\n" + string.Join("\n", dep.Take(8).Select(i => "  • " + mods[i].Name)) + (dep.Count > 8 ? "\n  ..." : "") : "") + "\n\n" + Tr("Turn it off in your playset now? (A backup is made first, you can turn it on again at any time.)\n\nYes: turn it off. No: leave the playset as it is.");
            if (await main.Dlg.Confirm(Tr("Bisect"), q)) mm.BisectApply(ids);
            else mm.Say(Tr("Bisect finished. Your playset was not changed."));
        }
        else if (s.NotMods)
            await main.Dlg.Info(Tr("The problem happened even with every suspect mod turned off.\n\nSo it is probably not caused by these mods: the base game, a DLC, your graphics driver, or a mod you told Bisect to keep on. Verify the game files in Steam, or try the Crash helper in the Advanced menu."));
        else
            await main.Dlg.Info(Tr("Bisect could not narrow it down to one mod. The problem probably needs more than one mod together, or it does not always happen. Try again, and play longer in each test."));
        if (tested && mm.BisectRestore()) tested = false;   // the game's mod list is your own again
    }
}
