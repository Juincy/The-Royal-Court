using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using RoyalCourt.Core;
using static RoyalCourt.Core.Lang;
namespace RoyalCourt.Ui;

/// <summary>The window-based implementation of the controller's dialogs.</summary>
public sealed class Dialogs : IDialogs
{
    public static int Open;      // modal dialogs on screen (automatic rescans wait for 0)
    readonly MainWindow main;
    public Dialogs(MainWindow w) { main = w; }

    Window Owner()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime l)
            foreach (var w in l.Windows) if (w.IsActive && w.IsVisible && w is not PopupHost && !PopupMenu.IsMenuWindow(w)) return w;
        return main;
    }
    sealed class PopupHost : Window { }

    async Task<T> Modal<T>(RcWindow w, Func<T> result)
    {
        Open++;
        try { await w.ShowDialog(Owner()); }
        finally { Open--; }
        return result();
    }
    static Control Row(params Control[] c) { var sp = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right }; foreach (var x in c) sp.Children.Add(x); return sp; }
    static RcButton Btn(string glyph, string label, double minW = 86, bool primary = false) => new() { Glyph = glyph, Label = label, MinW = minW, Primary = primary };

    // ---- messages ----
    public Task Info(string text) => Message("The Royal Court", text, false);
    public Task<bool> Confirm(string title, string text) => Message(title.Length == 0 ? "The Royal Court" : title, text, true);
    async Task<bool> Message(string title, string text, bool yesNo)
    {
        bool yes = false;
        var w = new RcWindow(title, 480, 200, false) { SizeToContent = SizeToContent.Height };
        var lab = new RcLabel(text) { Wrap = true, End = false, MaxWidth = 440 };
        var ok = yesNo ? Btn("✓", Tr("Yes"), 90, true) : Btn("✓", Tr("OK"), 90, true);
        var no = yesNo ? Btn("", Tr("No"), 90) : null;
        ok.Click += () => { yes = true; w.Close(); };
        if (no != null) no.Click += () => w.Close();
        var sp = new StackPanel { Margin = new Thickness(16), Spacing = 16 };
        sp.Children.Add(lab); sp.Children.Add(no != null ? Row(ok, no) : Row(ok));
        w.Content = sp;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); else if (e.Key == Key.Enter && !yesNo) { yes = true; w.Close(); } };
        w.Opened += (_, _) => ok.Focus();
        return await Modal(w, () => yes);
    }
    async Task Message(string title, string text) => await Message(title, text, false);

    // ---- one line of text ----
    public async Task<string?> Ask(string title, string prompt, string initial)
    {
        string? res = null;
        var w = new RcWindow(title, 400, 150, false) { SizeToContent = SizeToContent.Height };
        var edit = new RcEdit { Text = initial };
        var ok = Btn("✓", Tr("OK"), 86, true); var ca = Btn("", Tr("Cancel"), 86);
        ok.Click += () => { res = edit.Text ?? ""; w.Close(); };
        ca.Click += () => w.Close();
        var sp = new StackPanel { Margin = new Thickness(14), Spacing = 10 };
        sp.Children.Add(new RcLabel(prompt) { End = false }); sp.Children.Add(edit); sp.Children.Add(Row(ok, ca));
        w.Content = sp;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); else if (e.Key == Key.Enter) { res = edit.Text ?? ""; w.Close(); } };
        w.Opened += (_, _) => { edit.Focus(); edit.SelectAll(); };
        return await Modal(w, () => res);
    }

    // ---- files ----
    /// <summary>The classic Windows dialog first (it works on every PC); null result with handled = false means "use the other picker".</summary>
    string? Native(bool save, string title, (string, string)[] filters, string? dir, string? name, string? defExt, out bool handled)
    {
        handled = false;
        if (!OperatingSystem.IsWindows()) return null;
        var win = Owner(); IntPtr h = win.TryGetPlatformHandle()?.Handle ?? IntPtr.Zero;
        Log.Line("file dialog: opening (" + (save ? "save" : "open") + ", owner " + win.Title + ")");
        var r = NativeFileDialog.Show(h, save, title, filters, dir, name, defExt, out string path);
        Log.Line("file dialog: " + r);
        try { if (win.WindowState != WindowState.Minimized) win.Activate(); } catch { }
        if (r == NativeFileDialog.Result.Failed) { Log.Line("native file dialog failed, using the built-in picker"); return null; }
        handled = true;
        return r == NativeFileDialog.Result.Picked ? path : null;
    }
    public async Task<string?> PickGameExe()
    {
        { var n = Native(false, Tr("Choose ck3.exe"), new[] { ("ck3.exe", "ck3.exe"), (Tr("All files"), "*.*") }, null, null, null, out bool done); if (done) return n; }
        var r = await Owner().StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = Tr("Choose ck3.exe"), AllowMultiple = false,
            FileTypeFilter = new[] { new FilePickerFileType("ck3.exe") { Patterns = new[] { "ck3.exe" } }, FilePickerFileTypes.All },
        });
        return r.Count > 0 ? r[0].TryGetLocalPath() : null;
    }
    static readonly FilePickerFileType JsonType = new("Playset (*.json)") { Patterns = new[] { "*.json" } };
    public async Task<string?> SaveFile(string suggestedName)
    {
        { var n = Native(true, Tr("Export"), new[] { ("Playset (*.json)", "*.json") }, null, suggestedName, "json", out bool done); if (done) return n; }
        var r = await Owner().StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = Tr("Export"), SuggestedFileName = suggestedName, DefaultExtension = "json", FileTypeChoices = new[] { JsonType },
        });
        return r?.TryGetLocalPath();
    }
    public async Task<string?> OpenFile()
    {
        { var n = Native(false, Tr("Import"), new[] { ("Playset (*.json)", "*.json"), (Tr("All files"), "*.*") }, null, null, null, out bool done); if (done) return n; }
        var r = await Owner().StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions { Title = Tr("Import"), AllowMultiple = false, FileTypeFilter = new[] { JsonType, FilePickerFileTypes.All } });
        return r.Count > 0 ? r[0].TryGetLocalPath() : null;
    }
    public async Task<string?> PickBackup(string initialDir)
    {
        { var n = Native(false, Tr("Restore this playset from a backup..."), new[] { ("Playset (*.json)", "*.json"), (Tr("All files"), "*.*") }, initialDir, null, null, out bool done); if (done) return n; }
        var sp = Owner().StorageProvider;
        IStorageFolder? start = null;
        try { start = await sp.TryGetFolderFromPathAsync(initialDir); } catch { }
        var r = await sp.OpenFilePickerAsync(new FilePickerOpenOptions { Title = Tr("Restore this playset from a backup..."), AllowMultiple = false, SuggestedStartLocation = start, FileTypeFilter = new[] { JsonType, FilePickerFileTypes.All } });
        return r.Count > 0 ? r[0].TryGetLocalPath() : null;
    }

    // ---- clipboard ----
    public async Task<string> ClipboardGet()
    {
        try { var cb = TopLevel.GetTopLevel(Owner())?.Clipboard; return cb == null ? "" : (await cb.GetTextAsync()) ?? ""; } catch { return ""; }
    }
    public async Task<bool> ClipboardSet(string text)
    {
        try { var cb = TopLevel.GetTopLevel(Owner())?.Clipboard; if (cb == null) return false; await cb.SetTextAsync(text); return true; } catch { return false; }
    }

    // ---- long text ----
    public async Task ShowText(string title, string text)
    {
        var w = new RcWindow(title, 720, 520);
        var box = new RcEdit { Text = text, IsReadOnly = true, AcceptsReturn = true, TextWrapping = TextWrapping.Wrap, VerticalContentAlignment = VerticalAlignment.Top };
        ScrollViewer.SetVerticalScrollBarVisibility(box, Avalonia.Controls.Primitives.ScrollBarVisibility.Auto);
        var copy = Btn("❐", Tr("Copy"), 90); var close = Btn("", Tr("Close"), 90);
        copy.Click += async () => await ClipboardSet(text);
        close.Click += () => w.Close();
        var dock = new DockPanel { Margin = new Thickness(8) };
        var bar = Row(copy, close); bar.Margin = new Thickness(0, 8, 0, 0);
        DockPanel.SetDock(bar, Dock.Bottom);
        dock.Children.Add(bar); dock.Children.Add(box);
        w.Content = dock;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); };
        await Modal(w, () => 0);
    }

    // ---- choices ----
    public async Task<int> PickItem(string title, IReadOnlyList<string> items)
    {
        int res = -1;
        var w = new RcWindow(title, 420, 360);
        var list = new RcList { Checkboxes = false };
        list.Cols.Add(new RcColumn { Title = title, W = 380, Sortable = false });
        list.Count = items.Count; list.Cell = (r, _) => items[r];
        list.SelectOnly(items.Count > 0 ? 0 : -1);
        var ok = Btn("✓", Tr("OK"), 86, true); var ca = Btn("", Tr("Cancel"), 86);
        ok.Click += () => { res = list.FocusRow; w.Close(); };
        ca.Click += () => w.Close();
        list.Activated += r => { res = r; w.Close(); };
        list.SizeChanged += (_, e) => { list.Cols[0].W = Math.Max(100, e.NewSize.Width - 14); };
        var dock = new DockPanel { Margin = new Thickness(10) };
        var bar = Row(ok, ca); bar.Margin = new Thickness(0, 10, 0, 0);
        DockPanel.SetDock(bar, Dock.Bottom);
        dock.Children.Add(bar); dock.Children.Add(new Border { BorderBrush = new SolidColorBrush(Skin.Cur.Border), BorderThickness = new Thickness(1), Child = list });
        w.Content = dock;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); };
        w.Opened += (_, _) => list.Focus();
        return await Modal(w, () => res);
    }

    // ---- the Auto Sort preview ----
    public async Task<bool> SortPreview(SortPlan plan, Playset ps)
    {
        bool apply = false;
        var main = this.main;
        var w = new RcWindow(Tr("Auto Sort preview - The Royal Court"), 980, 620);
        w.MinWidth = 720; w.MinHeight = 360;
        string sum = Trf("Auto Sort will move {0} of {1} mods. Green = moves earlier, amber = moves later. Nothing is changed until you press Apply, and you can undo it afterwards.", plan.Moves.Count, ps.Mods.Count);
        if (plan.KnownCount > 0) sum += " " + Trf("{0} mod(s) recognised from the known-mods list.", plan.KnownCount);
        if (plan.PatchLinks > 0) sum += " " + Trf("{0} patch(es) placed after the mods they are for.", plan.PatchLinks);
        if (plan.ConflictChoices > 0) sum += " " + Trf("{0} file-conflict tie-break(s) applied.", plan.ConflictChoices);
        foreach (var wn in plan.Warnings) sum += "\n⚠ " + wn;
        var lab = new RcLabel(sum) { Wrap = true, End = false, Margin = new Thickness(0, 0, 0, 8) };
        var list = new RcList { Checkboxes = false };
        string[] heads = { "New #", "Mod", "Type", "Change", "Why" };
        double[] widths = { 56, 300, 100, 90, 300 };
        for (int i = 0; i < 5; i++) list.Cols.Add(new RcColumn { Title = Tr(heads[i]), W = widths[i], Sortable = false });
        var mv = new Dictionary<int, SortMove>();
        foreach (var m in plan.Moves) mv[m.From] = m;
        var lk = main.mm.St.Locks.TryGetValue(ps.Name, out var l0) ? l0 : new SortedSet<string>();
        var rows = new List<string[]>();
        var color = new List<int>();
        for (int p = 0; p < plan.Order.Length; p++)
        {
            int old = plan.Order[p];
            var mr = ps.Mods[old];
            string name = main.mm.Info.TryGetValue(mr.Id, out var inf) ? inf.Name : (mr.Name.Length == 0 ? mr.Id : mr.Name);
            string ch = "";
            if (lk.Contains(mr.Id)) ch = "\U0001F512 " + Tr("locked");
            else if (old > p) ch = "▲ " + Trf("from {0}", old + 1);
            else if (old < p) ch = "▼ " + Trf("from {0}", old + 1);
            rows.Add(new[] { (p + 1).ToString(), name, Cat.Name(plan.Cat[old]), ch, mv.TryGetValue(old, out var mm_) ? mm_.Reason : "" });
            color.Add(old > p ? 1 : old < p ? 2 : 0);
        }
        list.Count = rows.Count; list.Cell = (r, c) => rows[r][c];
        list.CellFg = (r, c) => c == 3 ? (color[r] == 1 ? Skin.Cur.Ok : color[r] == 2 ? Skin.Cur.Warn : Skin.Cur.Muted) : null;
        list.SizeChanged += (_, e) => { double rest = e.NewSize.Width - 56 - 300 - 100 - 90 - 14; list.Cols[4].W = Math.Max(160, rest); };
        var ok = Btn("✓", Tr("Apply sort"), 150, true); var ca = Btn("", Tr("Cancel"), 100);
        ok.Click += () => { apply = true; w.Close(); }; ca.Click += () => w.Close();
        var dock = new DockPanel { Margin = new Thickness(12) };
        var bar = Row(ok, ca); bar.Margin = new Thickness(0, 12, 0, 0);
        DockPanel.SetDock(bar, Dock.Bottom); DockPanel.SetDock(lab, Dock.Top);
        dock.Children.Add(bar); dock.Children.Add(lab);
        dock.Children.Add(new Border { BorderBrush = new SolidColorBrush(Skin.Cur.Border), BorderThickness = new Thickness(1), Child = list });
        w.Content = dock;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); };
        w.Opened += (_, _) => ok.Focus();
        return await Modal(w, () => apply);
    }

    // ---- an update is available ----
    public async Task<int> UpdateChoice(string head, string body, string notes, bool canInstall, bool hasPage)
    {
        int res = 0;
        var w = new RcWindow(Tr("Updates"), 560, 420);
        var sp = new StackPanel { Spacing = 10 };
        sp.Children.Add(new RcLabel(head) { Wrap = true, End = false, Bold = true, Size = 14 });
        sp.Children.Add(new RcLabel(body) { Wrap = true, End = false, Role = Role.Muted });
        var dock = new DockPanel { Margin = new Thickness(14) };
        var bar = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 10, 0, 0) };
        if (canInstall) { var b = Btn("⬆", Tr("Update now"), 160, true); b.Click += () => { res = 1001; w.Close(); }; bar.Children.Add(b); }
        if (hasPage) { var b = Btn("", Tr("Open the release page"), 180); b.Click += () => { res = 1002; w.Close(); }; bar.Children.Add(b); }
        var nb = Btn("", Tr("Not now"), 100); nb.Click += () => w.Close(); bar.Children.Add(nb);
        DockPanel.SetDock(bar, Dock.Bottom); DockPanel.SetDock(sp, Dock.Top);
        dock.Children.Add(bar); dock.Children.Add(sp);
        if (notes.Length > 0)
        {
            var box = new RcEdit { Text = notes, IsReadOnly = true, AcceptsReturn = true, TextWrapping = TextWrapping.Wrap, VerticalContentAlignment = VerticalAlignment.Top, Margin = new Thickness(0, 10, 0, 0) };
            dock.Children.Add(box);
        }
        w.Content = dock;
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) w.Close(); };
        return await Modal(w, () => res);
    }
}
