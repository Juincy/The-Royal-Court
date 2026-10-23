using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using RoyalCourt.Core;
using static RoyalCourt.Core.Lang;
namespace RoyalCourt.Ui;

/// <summary>Common frame of the conflicts and report windows: a row of buttons, a filter box, a status line and a list.</summary>
public abstract class ToolWindow : RcWindow, IRefreshable
{
    protected readonly MainWindow main;
    protected readonly ModManager mm;
    protected readonly RcList list = new() { Checkboxes = false };
    protected readonly RcEdit filter = new();
    protected readonly RcLabel status = new("", Role.Muted);
    protected readonly StackPanel left = new() { Orientation = Orientation.Horizontal, Spacing = 6 };
    protected readonly StackPanel right = new() { Orientation = Orientation.Horizontal, Spacing = 6 };
    protected double[] baseW = Array.Empty<double>();
    protected bool filling;

    protected ToolWindow(MainWindow owner, string title, double w, double h, double minW, double minH) : base(title, w, h)
    {
        main = owner; mm = owner.mm; MinWidth = minW; MinHeight = minH;
        ShowInTaskbar = true;
        var bar = new DockPanel { Margin = new Thickness(12, 12, 12, 0), Height = 30 };
        DockPanel.SetDock(left, Dock.Left); DockPanel.SetDock(right, Dock.Right);
        filter.Margin = new Thickness(6, 0); filter.VerticalAlignment = VerticalAlignment.Center; filter.MinHeight = 24; filter.Height = 24; filter.Padding = new Thickness(6, 2);
        bar.Children.Add(left); bar.Children.Add(right); bar.Children.Add(filter);
        status.Margin = new Thickness(12, 8, 12, 0); status.Height = 20;
        var frame = new Border { Margin = new Thickness(12, 6, 12, 12), BorderBrush = new SolidColorBrush(Skin.Cur.Border), BorderThickness = new Thickness(1), Child = list };
        Skin.Changed += () => frame.BorderBrush = new SolidColorBrush(Skin.Cur.Border);
        var dock = new DockPanel();
        DockPanel.SetDock(bar, Dock.Top); DockPanel.SetDock(status, Dock.Top);
        dock.Children.Add(bar); dock.Children.Add(status); dock.Children.Add(frame);
        Content = dock;
        list.SizeChanged += (_, _) => FitLast();
        list.ColumnsResized += FitLast;
        owner.SubWindows.Add(this);
        Closed += (_, _) => owner.SubWindows.Remove(this);
    }
    protected RcButton Btn(string glyph, string label, double minW, Func<bool>? primaryIf = null)
    {
        var b = new RcButton { Glyph = glyph, Label = label, MinW = minW, PrimaryIf = primaryIf };
        return b;
    }
    protected void SetColumns(IEnumerable<(string Title, int Width)> cols, bool sortable = false)
    {
        list.Cols.Clear();
        foreach (var c in cols) list.Cols.Add(new RcColumn { Title = c.Title, W = c.Width, Sortable = sortable, Min = 30 });
        baseW = list.Cols.Select(c => c.W).ToArray();
        FitLast();
    }
    void FitLast()
    {
        if (list.Cols.Count == 0 || list.Bounds.Width <= 0) return;
        double other = 0; for (int i = 0; i < list.Cols.Count - 1; i++) other += list.Cols[i].W;
        double rest = list.Bounds.Width - 14 - other;
        list.Cols[^1].W = Math.Max(baseW.Length == list.Cols.Count ? Math.Min(baseW[^1], 120) : 80, rest);
        list.InvalidateVisual();
    }
    public abstract void RefreshAll();
    protected void Invalidate(Func<Task> f) { _ = f(); }
}

public sealed class ConflictsWindow : ToolWindow
{
    readonly ConflictView view;
    readonly RcButton bPair, bFile, bDef, bBase, bRescan;
    public static void Open(MainWindow main)
    {
        var ex = main.SubWindows.OfType<ConflictsWindow>().FirstOrDefault();
        if (ex != null) { if (ex.WindowState == WindowState.Minimized) ex.WindowState = WindowState.Normal; ex.Activate(); return; }
        var w = new ConflictsWindow(main);
        w.Show(main);
        if (!main.mm.Scanning) main.mm.MaybeScan(true);
        main.mm.EnsureConflicts();
        w.RefreshAll();
    }
    ConflictsWindow(MainWindow owner) : base(owner, Tr("Conflicts - The Royal Court"), 1080, 600, 900, 360)
    {
        view = new ConflictView(mm);
        bPair = Btn("↔", Tr("By mod pair"), 140, () => view.View == ConflictView.ByPair);
        bFile = Btn("☰", Tr("By file"), 110, () => view.View == ConflictView.ByFile);
        bDef = Btn("{ }", Tr("By definition"), 150, () => view.View == ConflictView.ByDef);
        bBase = Btn("◆", Tr("Base game"), 130, () => view.View == ConflictView.ByBase);
        bRescan = Btn("↻", Tr("Rescan files"), 150);
        left.Children.Add(bPair); left.Children.Add(bFile); left.Children.Add(bDef); left.Children.Add(bBase);
        right.Children.Add(bRescan);
        bPair.Click += () => SetView(ConflictView.ByPair); bFile.Click += () => SetView(ConflictView.ByFile);
        bDef.Click += () => SetView(ConflictView.ByDef); bBase.Click += () => SetView(ConflictView.ByBase);
        bRescan.Click += () => { view.Rescan(); RefreshAll(); };
        filter.TextChanged += (_, _) => { if (filling) return; view.Filter = filter.Text ?? ""; view.Rebuild(); Fill(); };
        list.Cell = (r, c) => view.Cell(r, c);
        list.CellFg = (r, c) =>
        {
            if (c != 0) return null;
            int sv = view.RowSev(r); var t = Skin.Cur;
            return sv >= (int)Sev.High ? t.Bad : sv == (int)Sev.Med ? t.Warn : t.Muted;
        };
        list.Activated += r => Invalidate(() => Dbl(r));
        list.RowContext += (r, p) => Invalidate(() => Menu(r, p));
        SetColumns(view.Columns().Select(c => (c.Title, c.Width)));
        KeyDown += (_, e) => { if (e.Key == Key.Escape) Close(); };
    }
    void SetView(int v) { view.SetView(v); SetColumns(view.Columns().Select(c => (c.Title, c.Width))); Fill(); foreach (var b in new[] { bPair, bFile, bDef, bBase }) b.InvalidateVisual(); }
    void Fill() { list.Count = view.Rows.Count; list.Sel.Clear(); list.Refresh(); status.Text = view.StatusText; }
    public override void RefreshAll() { view.Rebuild(); Fill(); }
    async Task Dbl(int row)
    {
        if (row < 0 || row >= view.Rows.Count) return;
        if (view.View == ConflictView.ByFile || view.View == ConflictView.ByBase)
        {
            var t = view.WinnerText(row, out string title);
            if (t != null) await main.Dlg.ShowText(title, t);
        }
        else if (view.View == ConflictView.ByPair)
        {
            view.ShowPairFiles(row);
            SetColumns(view.Columns().Select(c => (c.Title, c.Width))); Fill();
            foreach (var b in new[] { bPair, bFile, bDef, bBase }) b.InvalidateVisual();
        }
        else if (view.View == ConflictView.ByDef)
        {
            var t = view.ScriptDetail(row);
            if (t != null) await main.Dlg.ShowText(Tr("Script overlap"), t);
        }
    }
    async Task Menu(int row, Point p)
    {
        if (row < 0 || row >= view.Rows.Count) return;
        list.SelectOnly(row);
        var items = view.ContextMenu(row, out int lo, out int hi, out string what);
        if (items.Count == 0) return;
        int r = await PopupMenu.Show(this, list.PointToScreen(p), items.Select(i => new MenuDef(i.Id, i.Text)).ToList());
        if (r == ConflictView.MnBelow) { view.MoveFor(lo, hi); Fill(); }
        else if (r == ConflictView.MnAbove) { view.MoveFor(hi, lo); Fill(); }
        else if (r == ConflictView.MnFiles) { view.ShowPairFiles(row); SetColumns(view.Columns().Select(c => (c.Title, c.Width))); Fill(); foreach (var b in new[] { bPair, bFile, bDef, bBase }) b.InvalidateVisual(); }
        else if (r == ConflictView.MnCopy) { if (!await main.Dlg.ClipboardSet(what)) await main.Dlg.Info(Tr("Could not copy to the clipboard (another program may be holding it). Try again.")); }
    }
}

public sealed class ReportWindow : ToolWindow
{
    readonly ReportView rv;
    readonly RcButton[] subs;
    public static void Open(MainWindow main, int kind)
    {
        var ex = main.SubWindows.OfType<ReportWindow>().FirstOrDefault();
        if (ex != null)
        {
            if (ex.rv.Kind == kind) { if (ex.WindowState == WindowState.Minimized) ex.WindowState = WindowState.Normal; ex.Activate(); ex.RefreshAll(); return; }
            ex.Close();
        }
        var w = new ReportWindow(main, kind);
        w.Show(main);
        w.RefreshAll();
    }
    ReportWindow(MainWindow owner, int kind) : base(owner,
        kind == ReportView.KindLog ? Tr("Game log - The Royal Court") : kind == ReportView.KindTime ? Tr("Script time - The Royal Court") : Tr("Since you last pressed Play - The Royal Court"), 1080, 600, 760, 320)
    {
        rv = new ReportView(mm, kind);
        var bs = new List<RcButton>();
        if (kind == ReportView.KindLog) { bs.Add(Btn("☰", Tr("By mod"), 110)); bs.Add(Btn("≡", Tr("All messages"), 150)); }
        else if (kind == ReportView.KindTime)
        {
            bs.Add(Btn("☰", Tr("By mod"), 124)); bs.Add(Btn("≡", Tr("Slowest events"), 124)); bs.Add(Btn("↯", Tr("On_actions"), 124)); bs.Add(Btn("▁", Tr("Over the years"), 150)); bs.Add(Btn("⟲", Tr("History"), 150));
        }
        subs = bs.ToArray();
        for (int i = 0; i < subs.Length; i++)
        {
            int k = i; subs[i].PrimaryIf = () => rv.Sub == k;
            subs[i].Click += () => { rv.Sub = k; RefreshAll(); foreach (var b in subs) b.InvalidateVisual(); };
            left.Children.Add(subs[i]);
        }
        var copy = Btn("⎘", Tr("Copy report"), 150); var reload = Btn("↻", Tr("Reload"), 110);
        right.Children.Add(copy); right.Children.Add(reload);
        copy.Click += () => Invalidate(async () =>
        {
            if (!await main.Dlg.ClipboardSet(rv.ReportText.Length == 0 ? "(nothing to copy)" : rv.ReportText)) await main.Dlg.Info(Tr("Could not copy to the clipboard (another program may be holding it). Try again."));
            else status.Text = Tr("Report copied to the clipboard.");
        });
        reload.Click += RefreshAll;
        filter.TextChanged += (_, _) => { if (filling) return; rv.Filter = filter.Text ?? ""; rv.Fill(); Fill(); };
        list.Cell = (r, c) => r < rv.Shown.Count && c < rv.Shown[r].Cells.Length ? rv.Shown[r].Cells[c] : "";
        list.CellFg = (r, c) =>
        {
            if (r >= rv.Shown.Count) return null;
            int sv = rv.Shown[r].Sev; var t = Skin.Cur;
            bool level = (rv.Kind == ReportView.KindSince && c == 0) || (rv.Kind == ReportView.KindLog && rv.Sub == 1 && c == 1) || (rv.Kind == ReportView.KindLog && rv.Sub == 0 && c <= 2);
            return level ? (sv >= 2 ? t.Bad : sv == 1 ? t.Warn : null) : null;
        };
        list.CellBar = (r, c) => rv.Kind == ReportView.KindTime && c == rv.BarCol && r < rv.Shown.Count && rv.Shown[r].Frac >= 0 ? rv.Shown[r].Frac : -1;
        list.Activated += r => { if (r < rv.Shown.Count && rv.DrillDown(rv.Shown[r])) { filling = true; filter.Text = rv.Filter; filling = false; RebuildColumns(); Fill(); foreach (var b in subs) b.InvalidateVisual(); } };
        list.RowContext += (r, p) => Invalidate(() => Menu(r, p));
        RebuildColumns();
        KeyDown += (_, e) => { if (e.Key == Key.Escape) Close(); };
    }
    void RebuildColumns() => SetColumns(rv.Columns().Select(c => (c.Title, c.Width)));
    void Fill() { list.Count = rv.Shown.Count; list.Sel.Clear(); list.Refresh(); status.Text = rv.StatusLine; }
    public override void RefreshAll() { rv.Refresh(); RebuildColumns(); Fill(); }
    async Task Menu(int r, Point p)
    {
        if (r < 0 || r >= rv.Shown.Count) return;
        list.SelectOnly(r);
        var row = rv.Shown[r];
        var items = new List<MenuDef> { new(1, Tr("Copy this line")) };
        if (rv.Kind == ReportView.KindTime && row.Author.Length > 0) items.Add(new(2, Tr("Copy a script time report for this mod's author")));
        if (rv.Kind == ReportView.KindLog && row.Mod >= 0) items.Add(new(3, Tr("Copy all messages for this mod (to send to its author)")));
        int cmd = await PopupMenu.Show(this, list.PointToScreen(p), items);
        if (cmd == 1) await main.Dlg.ClipboardSet(row.Copy.Length == 0 ? (row.Cells.Length > 0 ? row.Cells[0] : "") : row.Copy);
        else if (cmd == 2) { if (await main.Dlg.ClipboardSet(row.Author)) status.Text = Tr("Report for the mod's author copied to the clipboard."); }
        else if (cmd == 3) await main.Dlg.ClipboardSet(rv.ModMessages(row));
    }
}

/// <summary>One expandable branch per version, read from the CHANGELOG built into the program.</summary>
public sealed class ChangelogWindow : RcWindow
{
    sealed class Node { public string Text = ""; public Node? Parent; public List<Node> Kids = new(); public bool Open; public int Level; }
    readonly List<Node> roots = new();
    List<Node> vis = new();
    readonly RcList list = new() { Checkboxes = false, ShowHeader = false };
    public static void Open(MainWindow main)
    {
        var ex = main.SubWindows.OfType<ChangelogWindow>().FirstOrDefault();
        if (ex != null) { if (ex.WindowState == WindowState.Minimized) ex.WindowState = WindowState.Normal; ex.Activate(); return; }
        var w = new ChangelogWindow(main);
        w.Show(main);
    }
    static List<string> Wrap(string t, int width)
    {
        var lines = new List<string>(); string cur = ""; int i = 0;
        while (i < t.Length)
        {
            int sp = t.IndexOf(' ', i);
            string word = sp < 0 ? t[i..] : t[i..sp];
            if (cur.Length > 0 && cur.Length + 1 + word.Length > width) { lines.Add(cur); cur = ""; }
            if (cur.Length > 0) cur += ' ';
            cur += word;
            if (sp < 0) break;
            i = sp + 1;
        }
        if (cur.Length > 0) lines.Add(cur);
        return lines;
    }
    static string LoadText()
    {
        try
        {
            using var s = Avalonia.Platform.AssetLoader.Open(new Uri("avares://TheRoyalCourt/Assets/CHANGELOG.md"));
            using var r = new StreamReader(s, Encoding.UTF8);
            return r.ReadToEnd();
        }
        catch { return "Changelog not found."; }
    }
    ChangelogWindow(MainWindow owner) : base(Tr("Changelog - The Royal Court"), 780, 560)
    {
        ShowInTaskbar = true; MinWidth = 420; MinHeight = 260;
        owner.SubWindows.Add(this); Closed += (_, _) => owner.SubWindows.Remove(this);
        string txt = LoadText().Replace("\r\n", "\n");
        int u = txt.IndexOf("## [Unreleased]\n- Nothing yet.", StringComparison.Ordinal);
        if (u >= 0) { int e = txt.IndexOf("## [", u + 5, StringComparison.Ordinal); if (e >= 0) txt = txt.Remove(u, e - u); }
        txt = txt.Replace("**", "").Replace("`", "");
        Node? version = null, category = null, first = null; bool inFirst = false; var firstCats = new List<Node>();
        Node Add(Node? parent, string text) { var n = new Node { Text = text, Parent = parent, Level = parent == null ? 0 : parent.Level + 1 }; if (parent == null) roots.Add(n); else parent.Kids.Add(n); return n; }
        void Bullets(Node parent, string text, bool bullet)
        {
            var ls = Wrap(text, 105);
            for (int i = 0; i < ls.Count; i++) Add(parent, (i == 0 && bullet ? "• " : "    ") + ls[i]);
        }
        foreach (var line0 in txt.Split('\n'))
        {
            string line = line0.TrimEnd('\r');
            if (line.StartsWith("## ", StringComparison.Ordinal))
            {
                string head = line[3..].Replace("[", "").Replace("]", "");
                if (head.Length > 0 && char.IsDigit(head[0])) head = Trf("Version {0}", head);
                version = Add(null, head); category = null;
                if (first == null) { first = version; inFirst = true; } else inFirst = false;
            }
            else if (version == null) continue;
            else if (line.StartsWith("### ", StringComparison.Ordinal)) { category = Add(version, line[4..]); if (inFirst) firstCats.Add(category); }
            else if (line.StartsWith("- ", StringComparison.Ordinal)) Bullets(category ?? version, line[2..], true);
            else if (line.Length > 0) Bullets(category ?? version, line, false);
        }
        if (first != null) { first.Open = true; foreach (var c in firstCats) c.Open = true; }
        var ex = new RcButton { Glyph = "▾", Label = Tr("Expand all"), MinW = 120 };
        var co = new RcButton { Glyph = "▸", Label = Tr("Collapse all"), MinW = 130 };
        ex.Click += () => { SetAll(roots, true); Rebuild(); };
        co.Click += () => { SetAll(roots, false); Rebuild(); list.Off = 0; };
        var bar = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(8) };
        bar.Children.Add(ex); bar.Children.Add(co);
        var dock = new DockPanel();
        DockPanel.SetDock(bar, Dock.Top);
        dock.Children.Add(bar); dock.Children.Add(list);
        Content = dock;
        list.Cols.Add(new RcColumn { Title = "", W = 700, Sortable = false });
        list.Cell = (r, _) => r < vis.Count ? Label(vis[r]) : "";
        list.SizeChanged += (_, e) => list.Cols[0].W = Math.Max(100, e.NewSize.Width - 14);
        list.SelectionChanged += () => { if (list.Sel.Count == 1) { int r = list.Sel.Min; if (r < vis.Count && vis[r].Kids.Count > 0) { vis[r].Open = !vis[r].Open; int keep = r; Rebuild(); list.SelectOnly(keep); } } };
        Rebuild();
        if (first != null) list.EnsureVisible(0);
        KeyDown += (_, e) => { if (e.Key == Key.Escape) Close(); };
    }
    static string Label(Node n) => new string(' ', n.Level * 5) + (n.Kids.Count > 0 ? (n.Open ? "▾ " : "▸ ") : "") + n.Text;
    static void SetAll(List<Node> ns, bool open) { foreach (var n in ns) { if (n.Kids.Count > 0) n.Open = open; SetAll(n.Kids, open); } }
    void Rebuild()
    {
        vis = new();
        void Walk(Node n) { vis.Add(n); if (n.Open) foreach (var k in n.Kids) Walk(k); }
        foreach (var r in roots) Walk(r);
        list.Count = vis.Count; list.Refresh();
    }
}
