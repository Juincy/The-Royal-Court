using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using RoyalCourt.Core;
using static RoyalCourt.Core.Lang;
namespace RoyalCourt.Ui;

/// <summary>Places its children by code, the way the Windows version's layout() did.</summary>
public sealed class Root : Panel
{
    public Action<Size>? DoLayout;
    protected override Size MeasureOverride(Size s) { foreach (var c in Children) c.Measure(Size.Infinity); return s; }
    protected override Size ArrangeOverride(Size s) { DoLayout?.Invoke(s); return s; }
}

/// <summary>"CK3 version: 1.12.3" with only the number in colour; right-aligned.</summary>
public sealed class GameVerLabel : Control
{
    public string Version = "";
    public string Text = "";
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= InvalidateVisual; }
    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur; double w = Bounds.Width, cy = Bounds.Height / 2;
        int at = Version.Length == 0 ? -1 : Text.LastIndexOf(Version, StringComparison.Ordinal);
        bool known = at >= 0;
        string lead = known ? Text[..at] : Text;
        double ns = known ? Txt.W(Version, Txt.Size, true) : 0, ls = Txt.W(lead);
        double x0 = w - ns - ls;
        Txt.Draw(dc, lead, x0, cy, t.Muted);
        if (known) Txt.Draw(dc, Version, x0 + ls, cy, t.Gold, Txt.Size, true);
    }
}

public partial class MainWindow : Window
{
    public static WindowIcon? AppIcon;
    public readonly ModManager mm = new();
    public Dialogs Dlg = null!;

    // controls
    readonly Root root = new();
    readonly Banner banner = new();
    readonly ThemePicker picker = new();
    readonly RcButton bResync = new(), bLang = new(), bUpd = new(), bAdv = new(), bLog = new();
    readonly RcLabel l1 = new("", Role.Muted), lStatus = new("", Role.Muted), lCount = new("", Role.Muted), l3 = new("", Role.Muted);
    readonly RcLabel l2 = new("⌕", Role.Gold) { Size = 24, Family = Txt.Symbols, End = false, Center = true };
    readonly GameVerLabel gameVer = new();
    readonly RcCombo combo = new();
    readonly RcButton bNew = new(), bDup = new(), bRen = new(), bDel = new(), bExport = new(), bImport = new(), bPlay = new();
    readonly RcEdit filter = new(), dir = new();
    readonly RcButton bAllOn = new(), bAllOff = new(), bConflicts = new(), bSort = new(), bUndo = new(), bUp = new(), bDown = new();
    readonly RcButton bBrowse = new(), bSaveDir = new();
    public readonly RcList list = new();
    int lastBase = Look.IDark;
    double needClientW = 1200;
    int[] shownIdx = Array.Empty<int>();
    bool populating;

    public MainWindow()
    {
        Title = "The Royal Court | CK3 Mod Manager  v" + Env.Version;
        Width = 1260; Height = 660; MinHeight = 420; MinWidth = 1200;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;
        Icon = AppIcon;
        FontFamily = new FontFamily(Txt.Family); FontSize = Txt.Size;
        Content = root;
        Dlg = new Dialogs(this);
        mm.Dlg = Dlg;
        mm.Post = a => Dispatcher.UIThread.Post(a);
        BuildControls();
        root.DoLayout = DoLayout;
        Wire();
        Skin.Changed += OnSkin;
    }

    // ---------------------------------------------------------------- build
    void Add(params Control[] cs) { foreach (var c in cs) root.Children.Add(c); }
    static RcButton G(RcButton b, string glyph, double minW, bool banner = false, bool primary = false) { b.Glyph = glyph; b.MinW = minW; b.InBanner = banner; b.Primary = primary; return b; }
    void BuildControls()
    {
        Add(banner);
        G(bResync, "↻", 36, true); bResync.Label = ""; G(bLang, "\U0001F310", 76, true); G(bUpd, "⬆", 104, true); G(bAdv, "⚙", 116, true); G(bLog, "☰", 126, true);
        Add(bResync, bLang, bUpd, bAdv, bLog, picker);
        G(bNew, "+", 76); G(bDup, "❐", 110); G(bRen, "✎", 98); G(bDel, "✕", 90); G(bExport, "⇧", 96); G(bImport, "⇩", 96); G(bPlay, "▶", 140, false, true);
        G(bAllOn, "☑", 140); G(bAllOff, "☐", 146); G(bConflicts, "⚔", 120); G(bSort, "⇅", 124); G(bUndo, "↶", 112); G(bUp, "▲", 80); G(bDown, "▼", 90);
        G(bBrowse, "…", 110); G(bSaveDir, "✓", 120);
        Add(l1, combo, bNew, bDup, bRen, bDel, bExport, bImport, bPlay, l2, filter, bAllOn, bAllOff, bConflicts, bSort, bUndo, lCount, bUp, bDown, list, lStatus, gameVer, l3, dir, bBrowse, bSaveDir);
        bUndo.IsEnabled = false;
        string[] heads = { "#", "Mod", "Version", "Game Version", "Source", "Type", "Notes" };
        double[] widths = { 64, 400, 100, 110, 90, 90, 280 };
        for (int i = 0; i < 7; i++) list.Cols.Add(new RcColumn { Title = heads[i], W = widths[i], Min = 30 });
        list.Cols[0].Sortable = true;
        list.AllowDrag = true;
        list.Cell = (r, c) => r < mm.Rows.Count ? CellText(mm.Rows[r], c) : "";
        list.RowKind = r => r < mm.Rows.Count ? (mm.Rows[r].Pending ? 2 : mm.Rows[r].Enabled ? 0 : 1) : 0;
        list.IsChecked = r => r < mm.Rows.Count && mm.Rows[r].Enabled;
        list.CellFg = (r, c) =>
        {
            if (r >= mm.Rows.Count) return null;
            var row = mm.Rows[r]; var t = Skin.Cur;
            if (c == 6 && row.NoteSev > 0) return row.NoteSev == 2 ? t.Bad : row.NoteSev == 1 ? t.Warn : t.Ok;
            if (c == 3 && row.VerState == 2) return t.Warn;
            return null;
        };
        list.Focusable = true;
        list.Icon = r => r < mm.Rows.Count ? Thumbs.Get(mm.ThumbPath(mm.Rows[r].Id)) : null;
        list.IconCol = mm.St.Pictures ? 1 : -1;
        Thumbs.Loaded += () => list.InvalidateVisual();
        filter.Width = 230;
        bPlay.Primary = true;
    }
    static string CellText(ModRow r, int c) => c switch { 0 => r.Num, 1 => r.Name, 2 => r.Version, 3 => r.GameVersion, 4 => r.Source, 5 => r.Type, _ => r.Note };

    // ---------------------------------------------------------------- layout
    double BtnW(RcButton b) => b.WantW();
    double LabW(RcLabel l, double min) => Math.Max(min, Txt.W(l.Text, l.Size, l.Bold) + 6);
    void Place(Control c, double x, double y, double w, double h) { c.Measure(new Size(Math.Max(0, w), Math.Max(0, h))); c.Arrange(new Rect(x, y, Math.Max(0, w), Math.Max(0, h))); }
    void DoLayout(Size s)
    {
        double Wd = s.Width, Ht = s.Height;
        if (Wd <= 0 || Ht <= 0) return;
        const double m = 12, rowH = 30, gap = 6, bannerH = Banner.Height_;
        Place(banner, 0, 0, Wd, bannerH);
        double by = (bannerH - 2 - 32) / 2, rx = Wd - m;
        Place(picker, rx - Gfx.PickW, (bannerH - 2 - Gfx.PickH) / 2, Gfx.PickW, Gfx.PickH); rx -= Gfx.PickW + 14;
        double wLog = BtnW(bLog), wAdv = BtnW(bAdv), wUpd = BtnW(bUpd), wLang = BtnW(bLang);
        Place(bLog, rx - wLog, by, wLog, 32); rx -= wLog + gap;
        Place(bAdv, rx - wAdv, by, wAdv, 32); rx -= wAdv + gap;
        Place(bUpd, rx - wUpd, by, wUpd, 32); rx -= wUpd + gap;
        Place(bLang, rx - wLang, by, wLang, 32); rx -= wLang + gap;
        Place(bResync, rx - 36, by, 36, 32);
        banner.ArtR = rx - 36 - 8; banner.InvalidateVisual();

        double y = bannerH + m, x = m;
        double wL1 = LabW(l1, 52);
        double wNew = BtnW(bNew), wDup = BtnW(bDup), wRen = BtnW(bRen), wDel = BtnW(bDel), wExp = BtnW(bExport), wImp = BtnW(bImport), wPlay = BtnW(bPlay);
        double fixed1 = m * 2 + wL1 + 4 + gap * 2 + wNew + gap + wDup + gap + wRen + gap + wDel + gap * 2 + wExp + gap + wImp + gap * 2 + wPlay;
        double wCombo = Math.Max(110, Math.Min(220, Wd - fixed1));
        Place(l1, x, y + 7, wL1, 20); x += wL1 + 4;
        Place(combo, x, y + 2, wCombo, 26); x += wCombo + gap * 2;
        Place(bNew, x, y, wNew, rowH); x += wNew + gap;
        Place(bDup, x, y, wDup, rowH); x += wDup + gap;
        Place(bRen, x, y, wRen, rowH); x += wRen + gap;
        Place(bDel, x, y, wDel, rowH); x += wDel + gap * 2;
        Place(bExport, x, y, wExp, rowH); x += wExp + gap;
        Place(bImport, x, y, wImp, rowH);
        Place(bPlay, Wd - m - wPlay, y, wPlay, rowH);

        y += rowH + gap + 2; x = m;
        double wOn = BtnW(bAllOn), wOff = BtnW(bAllOff), wCf = BtnW(bConflicts), wSort = BtnW(bSort), wUndo = BtnW(bUndo), wDown = BtnW(bDown), wUp = BtnW(bUp);
        double fixed2 = m * 2 + 30 + gap * 2 + wOn + gap + wOff + gap * 2 + wCf + gap + wSort + gap + wUndo + gap * 2 + wUp + gap + wDown + gap * 2 + 190;
        double wFilter = Math.Max(90, Math.Min(170, Wd - fixed2 + 110));
        needClientW = Math.Max(fixed1 + 110, fixed2 - 190 + 90 + 190);
        Place(l2, x, y + 1, 24, 28); x += 30;
        Place(filter, x, y + 2, wFilter, rowH - 4); x += wFilter + gap * 2;
        Place(bAllOn, x, y, wOn, rowH); x += wOn + gap;
        Place(bAllOff, x, y, wOff, rowH); x += wOff + gap * 2;
        Place(bConflicts, x, y, wCf, rowH); x += wCf + gap;
        Place(bSort, x, y, wSort, rowH); x += wSort + gap;
        Place(bUndo, x, y, wUndo, rowH); x += wUndo + gap * 2;
        rx = Wd - m - wDown;
        Place(bDown, rx, y, wDown, rowH);
        rx -= gap + wUp;
        Place(bUp, rx, y, wUp, rowH);
        Place(lCount, x, y + 7, Math.Max(20, rx - gap * 2 - x), 20);

        y += rowH + gap + 2;
        double bottom = Ht - m - rowH - gap - 22 - gap;
        Place(list, m, y, Wd - 2 * m, bottom - y);
        double sy = bottom + gap;
        double wVer = Math.Max(220, Txt.W(gameVer.Text) + 24);
        Place(gameVer, Wd - m - wVer, sy, wVer, 20);
        Place(lStatus, m, sy, Wd - 2 * m - wVer - 10, 20);
        double dy = sy + 22 + gap;
        double wL3 = LabW(l3, 80), wBr = BtnW(bBrowse), wSv = BtnW(bSaveDir);
        Place(l3, m, dy + 7, wL3, 20);
        Place(dir, m + wL3 + 4, dy + 2, Math.Max(60, Wd - 2 * m - wL3 - 4 - wBr - wSv - 2 * gap), rowH - 4);
        Place(bBrowse, Wd - m - wSv - gap - wBr, dy, wBr, rowH);
        Place(bSaveDir, Wd - m - wSv, dy, wSv, rowH);
        ResizeCols(Wd - 2 * m);
    }
    void ResizeCols(double listW)
    {
        double fixedW = 0; foreach (int c in new[] { 0, 2, 3, 4, 5 }) fixedW += list.Cols[c].W;
        double notes = 280, w = listW - fixedW - notes - 12;
        if (w < 150) w = 150;
        list.Cols[1].W = w;
        double last = listW - fixedW - w - 12;
        list.Cols[6].W = last < 120 ? 120 : last;
        list.InvalidateVisual();
    }
    void EnsureWide()
    {
        MinWidth = Math.Max(1200, needClientW);
        if (WindowState == WindowState.Normal && Bounds.Width < needClientW) Width = needClientW;
    }

    // ---------------------------------------------------------------- texts / language
    void InitLanguage()
    {
        Pseudo = false; int idx = -1;
        string? e = Environment.GetEnvironmentVariable("RC_LANG");
        if (!string.IsNullOrEmpty(e)) { if (e == "zz") { Pseudo = true; idx = 0; } else idx = FromCode(e); }
        if (idx < 0 && mm.St.Language != "auto") idx = FromCode(mm.St.Language);
        if (idx < 0) idx = FromTag(CultureInfo.CurrentUICulture.Name);
        Set(idx);
        string code = Langs[Current].Code; code = code.Contains('-') ? code[..code.IndexOf('-')] : code;
        Txt.SetLanguage(code);
        FontFamily = new FontFamily(Txt.Family);
    }
    string LangShort() { string c = Langs[Current].Code; c = c.Contains('-') ? c[..c.IndexOf('-')] : c; return c.ToUpperInvariant(); }
    void ApplyUiText()
    {
        l1.Text = Tr("Playset"); l3.Text = Tr("CK3 folder");
        void B(RcButton b, string label, string tip) => b.SetText(label, tip);
        B(bNew, Tr("New"), Tr("Create an empty playset")); B(bDup, Tr("Duplicate"), Tr("Copy the selected playset under a new name")); B(bRen, Tr("Rename"), Tr("Rename the selected playset"));
        B(bDel, Tr("Delete"), Tr("Delete the selected playset (your mods are not touched)")); B(bExport, Tr("Export"), Tr("Save this playset as a Paradox Launcher playset file"));
        B(bImport, Tr("Import"), Tr("Load a Paradox Launcher playset file")); B(bPlay, Tr("Play"), Tr("Start Crusader Kings III with this playset (skips the launcher)"));
        B(bAdv, Tr("Advanced"), Tr("Backups, compare playsets, launch options, saves, updates and the diagnostics log")); B(bLog, Tr("Changelog"), Tr("What changed in each version"));
        B(bUpd, Tr("Updates"), Tr("Check GitHub for a newer version of The Royal Court, and for a newer known-mods list"));
        B(bAllOn, Tr("Enable shown"), Tr("Enable every mod currently shown in the list")); B(bAllOff, Tr("Disable shown"), Tr("Disable every mod currently shown in the list"));
        B(bConflicts, Tr("Conflicts"), Tr("Find mods that change the same files or definitions")); B(bSort, Tr("Auto Sort"), Tr("Propose a better load order (you see a preview first)"));
        B(bUndo, Tr("Undo order"), Tr("Go back to the load order from before the last Auto Sort or the last move made from the conflicts window"));
        B(bUp, Tr("Up"), Tr("Move the selected mod(s) up in the load order")); B(bDown, Tr("Down"), Tr("Move the selected mod(s) down in the load order"));
        B(bBrowse, Tr("Browse"), Tr("Pick your Crusader Kings III folder")); B(bSaveDir, Tr("Save folder"), Tr("Remember this folder"));
        B(bResync, "", Tr("Rescan the mod folder (use after subscribing to a mod while the app is open)")); B(bLang, LangShort(), Tr("Language"));
        ToolTip.SetTip(picker, Tr("Theme: the pumpkin turns Halloween on, the switch changes between day and night"));
        string[] heads = { "#", "Mod", "Version", "Game Version", "Source", "Type", "Notes" };
        for (int i = 0; i < 7; i++) list.Cols[i].Title = Tr(heads[i]);
        banner.Sub = Trf("CK3 MOD MANAGER  ·  v{0}", Env.Version);
        banner.Author = Trf("Author: {0}", "Juincy");
        SetGameVerText();
        banner.InvalidateVisual(); list.InvalidateVisual();
    }
    void SetGameVerText()
    {
        string v = mm.GameVer.Length == 0 ? Tr("unknown") : mm.GameVer;
        gameVer.Version = mm.GameVer.Length == 0 ? "" : mm.GameVer;
        gameVer.Text = Trf("CK3 version: {0}", v);
        gameVer.InvalidateVisual();
    }
    public async Task SetUiLanguage(string code)
    {
        mm.ConfQuiesce();
        mm.St.Language = code; mm.SaveSettingsNow();
        InitLanguage();
        bool hadTour = TourWindow.Current != null; int tourPage = TourWindow.Current?.Page ?? 0;
        TourWindow.Current?.Close();
        foreach (var w in SubWindows.ToArray()) w.Close();
        ApplyUiText(); root.InvalidateMeasure(); root.InvalidateArrange(); DoLayout(root.Bounds.Size); EnsureWide();
        FillCombo(); mm.Populate(); mm.UpdateCount();
        mm.Say(Trf("Language: {0}", Langs[Current].Native));
        Log.Line("language: " + Langs[Current].Code + (code == "auto" ? " (automatic)" : ""));
        if (hadTour) { var t = TourWindow.Open(this); t.GoTo(tourPage); }
        await Task.CompletedTask;
    }
    public readonly List<Window> SubWindows = new();

    // ---------------------------------------------------------------- theme
    Look? fromTheme; double slideStart = -1; double togFrom, togTo, hwFrom, hwTo;
    const double SlideSec = 0.42;
    public void SetTheme(int id, bool animate = true)
    {
        if (id == Skin.Id && animate) return;
        if (Skin.Id != Look.IHalloween) lastBase = Skin.Id;
        var from = Skin.Cur;
        Skin.Id = id;
        mm.St.Theme = Skin.Name(id); if (animate) mm.SaveSettingsNow();
        Application.Current!.RequestedThemeVariant = Skin.Dark ? Avalonia.Styling.ThemeVariant.Dark : Avalonia.Styling.ThemeVariant.Light;
        RequestedThemeVariant = Skin.Dark ? Avalonia.Styling.ThemeVariant.Dark : Avalonia.Styling.ThemeVariant.Light;
        togFrom = picker.TogT; togTo = id == Look.ILight ? 0 : 1; hwFrom = picker.HwT; hwTo = id == Look.IHalloween ? 1 : 0;
        if (!animate) { Skin.Repaint(Look.Of(id)); picker.TogT = togTo; picker.HwT = hwTo; ApplyBackground(); return; }
        if (slideStart < 0) Anim.Hold();
        fromTheme = from; slideStart = Skin.Sec;
        Log.Line("theme change to " + Skin.Name(id));
    }
    void SlideStep(double dt)
    {
        if (slideStart < 0) return;
        double p = Gfx.Ease((Skin.Sec - slideStart) / SlideSec);
        Skin.Repaint(Look.Between(fromTheme!, Look.Of(Skin.Id), p));
        picker.TogT = togFrom + (togTo - togFrom) * p; picker.HwT = hwFrom + (hwTo - hwFrom) * p;
        if (p >= 1) { slideStart = -1; Anim.Release(); Skin.Repaint(Look.Of(Skin.Id)); }
    }
    void ApplyBackground() { Background = new SolidColorBrush(Skin.Cur.Bg); }
    void OnSkin()
    {
        ApplyBackground();
        lStatus.Role = Role.Muted;
    }
    void OnPick(int zone)
    {
        int target = zone == 1 ? (Skin.Id == Look.IHalloween ? lastBase : Look.IHalloween) : (Skin.Id == Look.IDark ? Look.ILight : Look.IDark);
        SetTheme(target);
    }

    // ---------------------------------------------------------------- wiring
    async void Run(Func<Task> f)
    {
        try { await f(); }
        catch (Exception e)
        {
            Log.Line("error: " + e);
            try { await Dlg.Info(Tr("Something went wrong, but your playset files are safe.") + "\n\n" + e.Message); } catch { }
        }
    }
    void Do(Action f) => Run(() => { f(); return Task.CompletedTask; });
    List<int> SelIdx() => list.Sel.Where(r => r < mm.Shown.Count).Select(r => mm.Shown[r]).ToList();

    void Wire()
    {
        picker.Pick += OnPick;
        Anim.Frame += SlideStep;
        mm.StatusChanged += s => lStatus.Text = s;
        mm.CountChanged += () => { lCount.Text = mm.CountText; bUndo.IsEnabled = mm.UndoAvailable; };
        mm.PlaysetsChanged += FillCombo;
        mm.GameVersionChanged += SetGameVerText;
        mm.FolderChanged += () => dir.Text = mm.St.Ck3Dir.Length > 0 ? mm.St.Ck3Dir : mm.EffectiveDir();
        mm.NotesChanged += () => list.InvalidateVisual();
        mm.ListChanged += OnListChanged;
        mm.RequestClose += () => Close();
        mm.UnsubWatch += StartUnsubTimer;
        mm.ConflictsChanged += () => { foreach (var w in SubWindows.OfType<IRefreshable>()) w.RefreshAll(); RefreshButtons(); };
        mm.ScanFinished += () => { foreach (var w in SubWindows.OfType<IRefreshable>()) w.RefreshAll(); };

        combo.Changed += i => { if (i >= 0 && i < mm.Playsets.Count) mm.SelectPlayset(i); };
        filter.TextChanged += (_, _) => { if (!populating) mm.SetFilter(filter.Text ?? ""); };
        bAllOn.Click += () => Do(() => mm.SetShown(true));
        bAllOff.Click += () => Do(() => mm.SetShown(false));
        bNew.Click += () => Run(mm.NewPlayset);
        bDup.Click += () => Run(mm.DuplicatePlayset);
        bRen.Click += () => Run(mm.RenamePlayset);
        bDel.Click += () => Run(mm.DeletePlayset);
        bExport.Click += () => Run(ExportMenu);
        bImport.Click += () => Run(ImportMenu);
        bUp.Click += () => Do(() => MoveSel(true));
        bDown.Click += () => Do(() => MoveSel(false));
        bPlay.Click += () => Run(mm.Play);
        bAdv.Click += () => Run(AdvancedMenu);
        bLog.Click += () => Do(() => ChangelogWindow.Open(this));
        bLang.Click += () => Run(LanguageMenu);
        bUpd.Click += () => Run(() => mm.CheckForUpdates(false));
        bResync.Click += () => Do(mm.RescanAll);
        bConflicts.Click += () => Do(() => ConflictsWindow.Open(this));
        bSort.Click += () => Run(async () => { await mm.AutoSort(); RefreshButtons(); });
        bUndo.Click += () => Do(() => { mm.UndoSort(); RefreshButtons(); });
        bBrowse.Click += () => Run(BrowseFolder);
        bSaveDir.Click += () => Do(() =>
        {
            mm.SaveFolder(dir.Text ?? "");
        });

        list.HeaderClick += c => Do(() => { mm.ClickColumn(c); list.SortCol = mm.SortCol; list.SortAsc = mm.SortAsc; });
        list.CheckChanged += (r, on) => Do(() => { mm.SetEnabled(r, on); list.InvalidateVisual(); });
        list.Activated += r => Run(() => ShowDetails(r));
        list.RowContext += (r, p) => Run(() => ShowContextMenu(r, p));
        list.DragBlocked += () => { if (mm.OrderLocked) { mm.Say(Tr("Clear the filter and column sorting (click the # header) to change load order.")); return true; } return false; };
        list.DragMoved += (f, t) => Do(() =>
        {
            if (f < mm.Shown.Count && t < mm.Shown.Count && mm.MoveMod(mm.Shown[f], mm.Shown[t])) { list.SelectOnly(t); }
        });
        list.ColumnsResized += () => ResizeCols(list.Bounds.Width);

        Opened += (_, _) => OnOpened();
        Closing += (_, _) => OnClosing();
        Activated += (_, _) => { Anim.Active = true; Dispatcher.UIThread.Post(() => mm.OnActivated(Dialogs.Open > 0)); };
        Deactivated += (_, _) => Anim.Active = false;
        PropertyChanged += (_, e) => { if (e.Property == BoundsProperty || e.Property == WindowStateProperty) SaveStateSoon(); };
        PositionChanged += (_, _) => SaveStateSoon();
        KeyDown += (_, e) => { if (e.Key == Key.F5) { mm.RescanAll(); e.Handled = true; } };
    }
    void RefreshButtons() { bUndo.IsEnabled = mm.UndoAvailable; }
    void FillCombo()
    {
        int idx = mm.Playsets.FindIndex(p => p.Name == mm.St.Active);
        combo.SetItems(mm.Playsets.Select(p => p.Name), idx);
        RefreshButtons();
    }
    void OnListChanged()
    {
        populating = true;
        var keep = new HashSet<int>(list.Sel.Where(r => r < shownIdx.Length).Select(r => shownIdx[r]));
        list.Count = mm.Rows.Count;
        list.SortCol = mm.SortCol; list.SortAsc = mm.SortAsc;
        shownIdx = mm.Shown.ToArray();
        var rows = new List<int>();
        for (int r = 0; r < shownIdx.Length; r++) if (keep.Contains(shownIdx[r])) rows.Add(r);
        list.Sel.Clear(); foreach (var r in rows) list.Sel.Add(r);
        if (rows.Count > 0) { list.FocusRow = rows[0]; list.Anchor = rows[0]; } else { list.FocusRow = -1; list.Anchor = -1; }
        list.EmptyText = mm.Rows.Count == 0 ? (mm.EffectiveDir().Length == 0 ? Tr("The CK3 folder is not set. Enter it at the bottom (the folder that contains \"mod\") and press Save folder.") : "") : "";
        list.Refresh();
        populating = false;
        RefreshButtons();
        if ((filter.Text ?? "") != mm.Filter) { populating = true; filter.Text = mm.Filter; populating = false; }
    }
    void MoveSel(bool up)
    {
        if (mm.OrderLocked) { mm.Say(Tr("Clear the filter and column sorting (click the # header) to change load order.")); return; }
        var sel = SelIdx();
        var res = mm.MoveSelection(sel, up);
        if (res.Count > 0) { list.SetSelection(res); list.EnsureVisible(res[0]); }
    }

    // ---------------------------------------------------------------- menus
    PixelPoint Below(Control c) => c.PointToScreen(new Point(0, c.Bounds.Height));
    async Task ExportMenu()
    {
        if (mm.Active() == null) return;
        int r = await PopupMenu.Show(this, Below(bExport), new()
        {
            new(1, Tr("Save as a playset file (.json, also opens in the Paradox launcher)...")),
            new(2, Tr("Copy a share code (short, fits in a chat message)")),
            new(3, Tr("Copy a share code with mod names (longer, shows what is missing)")),
        });
        if (r == 1) await mm.ExportFile(); else if (r == 2) await mm.ExportCode(false); else if (r == 3) await mm.ExportCode(true);
    }
    async Task ImportMenu()
    {
        int r = await PopupMenu.Show(this, Below(bImport), new() { new(1, Tr("Open a playset file (.json)...")), new(2, Tr("Paste a share code from the clipboard")) });
        if (r == 1) await mm.ImportFile(); else if (r == 2) await mm.ImportCode();
    }
    async Task LanguageMenu()
    {
        var items = new List<MenuDef> { new(1, Tr("Automatic (same as Windows)"), mm.St.Language == "auto"), MenuDef.Separator };
        for (int i = 0; i < Count; i++) items.Add(new(2 + i, Langs[i].Native, mm.St.Language == Langs[i].Code));
        int r = await PopupMenu.Show(this, Below(bLang), items);
        if (r == 1) await SetUiLanguage("auto"); else if (r >= 2) await SetUiLanguage(Langs[r - 2].Code);
    }
    async Task AdvancedMenu()
    {
        var items = new List<MenuDef>
        {
            new(1, Tr("Health check: is this playset ready to play...")), MenuDef.Separator,
            new(2, Tr("Game log: which mod causes the errors...")), new(3, Tr("Compatch finder: patches for the mods you use...")),
            new(4, Tr("Script time: which mod's scripts take the most time...")), new(5, Tr("What changed since I last pressed Play...")), new(6, Tr("Crash helper: why did the game crash...")), new(23, Tr("Bisect: find the mod causing a problem by testing...")), MenuDef.Separator,
            new(7, Tr("Open playsets folder")), MenuDef.Separator,
            new(8, Tr("Back up this playset now")), new(9, Tr("Restore this playset from a backup...")), new(10, Tr("Open backups folder")), MenuDef.Separator,
            new(11, Tr("Compare with another playset...")), new(12, Tr("Game launch options for this playset...")),
        };
        if (mm.St.Hidden.Count > 0) items.Add(new(13, Trf("Show removed mods ({0})", mm.St.Hidden.Count)));
        items.Add(MenuDef.Separator);
        items.Add(new(14, Tr("Delete all saves..."))); items.Add(MenuDef.Separator);
        items.Add(new(15, Tr("Open saves folder"))); items.Add(new(16, Tr("Open game logs folder")));
        items.Add(new(17, Tr("Clear the shader cache (the game rebuilds it)..."))); items.Add(new(18, Tr("Show the quick tour again...")));
        items.Add(MenuDef.Separator);
        items.Add(new(24, Tr("Show the mod pictures in the list"), mm.St.Pictures));
        items.Add(new(19, Tr("Run the health check when I press Play"), mm.St.PlayCheck));
        items.Add(new(20, Tr("Check for updates when the program starts"), mm.St.CheckUpdates));
        items.Add(new(21, Tr("Report a sort problem (copy details)...")));
        items.Add(new(22, Tr("Open diagnostics log (for bug reports)")));
        int r = await PopupMenu.Show(this, Below(bAdv), items);
        switch (r)
        {
            case 1: await mm.ShowHealth(); break;
            case 2: ReportWindow.Open(this, ReportView.KindLog); break;
            case 3: await mm.Compatches(); break;
            case 4: ReportWindow.Open(this, ReportView.KindTime); break;
            case 5: ReportWindow.Open(this, ReportView.KindSince); break;
            case 6: await mm.CrashHelper(); break;
            case 23: BisectWindow.Open(this); break;
            case 7: Platform.OpenShell(Util.PlaysetsDir()); break;
            case 8: mm.BackupNow(); break;
            case 9: await mm.RestoreBackup(); break;
            case 10: Platform.OpenShell(mm.BackupsFolder()); break;
            case 11: await mm.ComparePlaysets(); break;
            case 12: await mm.LaunchOptions(); break;
            case 13: mm.ShowRemovedMods(); break;
            case 14: await mm.DeleteAllSaves(); break;
            case 15: await mm.OpenUserFolder("save games"); break;
            case 16: await mm.OpenUserFolder("logs"); break;
            case 17: await mm.ClearShaders(); break;
            case 18: TourWindow.Open(this); break;
            case 24: mm.TogglePictures(); list.IconCol = mm.St.Pictures ? 1 : -1; list.InvalidateVisual(); break;
            case 19: mm.TogglePlayCheck(); break;
            case 20: mm.ToggleAutoUpdate(); break;
            case 21: await mm.SortReport(); break;
            case 22: mm.OpenLog(); break;
        }
    }
    async Task ShowContextMenu(int row, Point p)
    {
        var ps = mm.Active();
        if (ps == null || row < 0 || row >= mm.Shown.Count) return;
        int clicked = mm.Shown[row];
        var sel = SelIdx(); if (sel.Count == 0) sel.Add(clicked);
        int n = sel.Count;
        var mr = ps.Mods[clicked];
        bool locked = mm.IsLocked(mr.Id);
        int cur = mm.CatOfMod(mr.Id, out bool ovr);
        var items = new List<MenuDef>
        {
            new(1, n > 1 ? Trf("Lock position of {0} mods (Auto Sort never moves them)", n) : Tr("Lock position (Auto Sort never moves it)"), locked && n == 1), MenuDef.Separator,
        };
        for (int c = 0; c < Cat.Count; c++) items.Add(new(100 + c, n > 1 ? Trf("Type for all: {0}", Cat.Name(c)) : Trf("Type: {0}", Cat.Name(c)), n == 1 && ovr && cur == c));
        items.Add(new(100 + Cat.Count, n > 1 ? Tr("Type for all: automatic") : Tr("Type: automatic"), n == 1 && !ovr));
        items.Add(MenuDef.Separator);
        if (n == 1) { items.Add(new(2, Tr("Explain this position..."))); items.Add(MenuDef.Separator); }
        bool anyWs = sel.Any(i => i < ps.Mods.Count && mm.HasWorkshopId(ps.Mods[i].Id));
        if (anyWs) items.Add(new(3, n > 1 ? Trf("Unsubscribe from {0} mods on Steam...", n) : Tr("Unsubscribe on Steam...")));
        items.Add(new(4, n > 1 ? Trf("Remove {0} mods from list...", n) : Tr("Remove from list...")));
        bool anyDel = sel.Any(i => i < ps.Mods.Count && mm.Info.TryGetValue(ps.Mods[i].Id, out var inf) && !inf.Pending);
        if (anyDel) items.Add(new(5, n > 1 ? Trf("Delete {0} mods permanently...", n) : Tr("Delete mod permanently...")));
        var at = list.PointToScreen(p);
        int r = await PopupMenu.Show(this, at, items);
        if (r == 1) mm.ToggleLock(sel, clicked);
        else if (r >= 100) mm.SetType(sel, r - 100 == Cat.Count ? -1 : r - 100);
        else if (r == 2) { string txt = mm.ExplainPosition(clicked); await Dlg.ShowText(Tr("Why is this mod here? - The Royal Court"), txt); }
        else if (r == 3) await mm.Unsubscribe(sel);
        else if (r == 4) await mm.RemoveOrDelete(sel, false);
        else if (r == 5) await mm.RemoveOrDelete(sel, true);
    }
    async Task ShowDetails(int row)
    {
        if (row < 0 || row >= mm.Shown.Count) return;
        await Dlg.ShowText(Tr("Mod details"), mm.DetailsText(mm.Shown[row]));
    }
    async Task BrowseFolder()
    {
        var sp = StorageProvider;
        var r = await sp.OpenFolderPickerAsync(new Avalonia.Platform.Storage.FolderPickerOpenOptions { Title = Tr("Pick your Crusader Kings III folder"), AllowMultiple = false });
        if (r.Count > 0) dir.Text = r[0].TryGetLocalPath() ?? "";
    }

    // ---------------------------------------------------------------- unsubscribe watcher, old copy, window state
    DispatcherTimer? unsubTimer, oldTimer, stateTimer;
    void StartUnsubTimer()
    {
        if (unsubTimer != null) return;
        unsubTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3) };
        unsubTimer.Tick += (_, _) => { try { if (!mm.UnsubTick(Dialogs.Open > 0)) { unsubTimer!.Stop(); unsubTimer = null; } } catch (Exception e) { Log.Line("unsub tick: " + e.Message); } };
        unsubTimer.Start();
    }
    bool ready;
    void SaveStateSoon()
    {
        if (!ready) return;
        stateTimer ??= new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(600) };
        stateTimer.Stop(); stateTimer.Tick -= StateTick; stateTimer.Tick += StateTick; stateTimer.Start();
    }
    void StateTick(object? s, EventArgs e) { stateTimer?.Stop(); SaveState(); }
    void SaveState()
    {
        try
        {
            var st = mm.St; double sc = RenderScaling;
            st.WinMax = WindowState == WindowState.Maximized;
            if (WindowState == WindowState.Normal)
            {
                st.WinX = Position.X; st.WinY = Position.Y; st.WinW = (int)Math.Round(Bounds.Width * sc); st.WinH = (int)Math.Round(Bounds.Height * sc);
            }
            st.ColW = list.Cols.Select(c => (int)Math.Round(c.W * sc)).ToList();
            st.Save();
        }
        catch { }
    }
    void RestoreState()
    {
        var st = mm.St; double sc = RenderScaling;
        if (st.WinW >= 1200 && st.WinH >= 420)
        {
            var rect = new PixelRect(st.WinX, st.WinY, st.WinW, st.WinH);
            if (Screens.All.Any(s => s.WorkingArea.Intersects(rect)))
            {
                WindowStartupLocation = WindowStartupLocation.Manual; Position = new PixelPoint(st.WinX, st.WinY);
                Width = st.WinW / sc; Height = st.WinH / sc;
            }
        }
        if (st.WinMax) WindowState = WindowState.Maximized;
        if (st.ColW.Count == 7) foreach (int c in new[] { 0, 2, 3, 4, 5 }) if (st.ColW[c] >= 30 && st.ColW[c] <= 800) list.Cols[c].W = st.ColW[c] / sc;
    }

    public void Boot()
    {
        mm.LoadSettings();
        InitLanguage();
        string t = mm.St.Theme;
        int id = Skin.FromName(t);
        if (id < 0) id = (Application.Current?.PlatformSettings?.GetColorValues().ThemeVariant ?? PlatformThemeVariant.Dark) == PlatformThemeVariant.Light ? Look.ILight : Look.IDark;
        lastBase = id == Look.ILight ? Look.ILight : Look.IDark;
        Skin.Id = id; Skin.Cur = Look.Of(id);
        picker.TogT = id == Look.ILight ? 0 : 1; picker.HwT = id == Look.IHalloween ? 1 : 0;
        Application.Current!.RequestedThemeVariant = Skin.Dark ? Avalonia.Styling.ThemeVariant.Dark : Avalonia.Styling.ThemeVariant.Light;
        RequestedThemeVariant = Application.Current.RequestedThemeVariant;
        ApplyBackground();
        ApplyUiText();
    }
    void OnOpened()
    {
        RestoreState();
        try { mm.Reload(); } catch (Exception e) { Log.Line("reload failed: " + e); _ = Dlg.Info(Tr("Something went wrong, but your playset files are safe.") + "\n\n" + e.Message); }
        FillCombo();
        dir.Text = mm.St.Ck3Dir.Length > 0 ? mm.St.Ck3Dir : mm.EffectiveDir();
        Log.Line($"start v{Env.Version}, {mm.Mods.Count} mods, {mm.Playsets.Count} playsets, game {(mm.GameVer.Length == 0 ? "?" : mm.GameVer)}");
        root.InvalidateArrange();
        EnsureWide();
        ready = true;
        Anim.Start(this);
        if (!mm.St.TutorialDone) Dispatcher.UIThread.Post(() => { if (!mm.St.TutorialDone) TourWindow.Open(this); });
        oldTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(20) };
        oldTimer.Tick += (_, _) => { oldTimer!.Stop(); ModManager.RemoveOldCopy(); };
        oldTimer.Start();
        if (mm.St.CheckUpdates) Run(() => mm.CheckForUpdates(true));
    }
    void OnClosing()
    {
        ready = false;
        SaveState();
        mm.Shutdown();
        foreach (var w in SubWindows.ToArray()) w.Close();
        if (Environment.GetEnvironmentVariable("RC_LANG_MISSING") is { Length: > 0 } mf)
        {
            try { File.WriteAllLines(mf, Missing); } catch { }
        }
    }
}

public interface IRefreshable { void RefreshAll(); }
