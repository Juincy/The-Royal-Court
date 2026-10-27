using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using RoyalCourt.Core;
using static RoyalCourt.Core.Lang;
namespace RoyalCourt.Ui;

/// <summary>The first-launch quick tour: eight pages, a page list on the left, Back / Next / Skip.</summary>
public sealed class TourWindow : RcWindow
{
    public static TourWindow? Current;
    sealed record TourPage(string Glyph, string Title, string Intro, string[] Pts, string Tip);
    static readonly TourPage[] Pages =
    {
        new TourPage("♛", K("Welcome"),
            K("The Royal Court is a fast, light mod manager for Crusader Kings III. This short tour shows the features that save you the most trouble."),
            new[] { K("Your playsets are saved as Paradox Launcher playset files, so nothing is locked in."),
                    K("No installer and no browser. The program goes online only when you press Updates."),
                    K("It never changes your mod files. It reads them to find problems."),
                    K("You can open this tour again at any time from Advanced > Show the quick tour again.") },
            K("If your mods do not appear, type your Crusader Kings III folder at the bottom (the folder that contains \"mod\") and press Save folder.")),
        new TourPage("☰", K("Playsets and load order"),
            K("A playset is a list of mods with an order. The order matters: when two mods change the same thing, the one lower in the list wins."),
            new[] { K("Tick a mod to turn it on or off. Every change is saved at once."),
                    K("Drag rows, or use Up and Down, to change the order. Hold Ctrl or Shift to select several mods and move them together."),
                    K("Use New, Duplicate, Rename and Delete for your playsets. Duplicate one before you experiment."),
                    K("Click a column header to sort the view. The Notes column flags missing mods, mods made for an older game and conflicts.") },
            K("The filter box next to the magnifier finds a mod by name as you type.")),
        new TourPage("⇅", K("Auto Sort"),
            K("Press Auto Sort and the program proposes a better load order. Nothing changes until you look at the preview and press Apply."),
            new[] { K("Dependencies go first, then libraries, overhauls, content, graphics, interface, translations and patches."),
                    K("Total conversions load first, and well-known mods such as Rise and Fall are placed where their authors say."),
                    K("Right-click a mod to lock it so Auto Sort never moves it, or to correct its type."),
                    K("Not happy with the result? Undo order puts the list back as it was.") },
            K("Sorted something wrongly? Advanced > Report a sort problem copies the details so the known-mods list can be fixed.")),
        new TourPage("⚠", K("Conflicts"),
            K("Conflicts shows where mods collide, before you spend an evening in a broken game."),
            new[] { K("By mod pair or by file: mods that ship the same file. The mod loaded last usually wins."),
                    K("By definition: mods that define the same trait, event, define or localization key."),
                    K("Base game: mods that replace a file of the game itself. The game never merges those."),
                    K("Double-click a file to see every mod that ships it and who wins. Right-click a row to let one mod win; the order is saved and checked again.") },
            K("Every conflict has a severity (High, Medium, Low). Type high, medium or low in the filter box to see only those.")),
        new TourPage("▶", K("Play and backups"),
            K("Play starts Crusader Kings III directly with the playset you selected, without the Paradox Launcher."),
            new[] { K("Steam must be running. The program writes your playset into the game's dlc_load.json first."),
                    K("Your own dlc_load.json is kept next to it as dlc_load.json.rc-original."),
                    K("Your playset is backed up automatically at startup and before Auto Sort."),
                    K("Advanced has Back up now, Restore from a backup and Open backups folder.") },
            K("Game launch options for each playset (for example -debug_mode) are under Advanced.")),
        new TourPage("⚒", K("When something goes wrong"),
            K("Three helpers read the game's own files and tell you where to look. They only read; they never change anything."),
            new[] { K("Crash helper: after a crash it says what kind of crash it was and which of your mods the last log messages point at."),
                    K("Game log: every error and warning of the game, grouped by the mod it comes from, with a copy button for the mod's author."),
                    K("What changed since I last pressed Play: mods that updated, mods made for an older game, mods turned on or off, a game update."),
                    K("The status line at the bottom tells you when the game crashed or when something changed.") },
            K("These helpers are in the Advanced menu.")),
        new TourPage("⏱", K("Speed and fit"),
            K("Tools that look at the whole playset instead of one mod at a time."),
            new[] { K("Script time shows which mod's scripts use the most game time, as measured by the game itself. History compares your game sessions, so you see whether a mod got slower."),
                    K("Right-click a mod > Explain this position tells you which rules hold it in place and what Auto Sort would do."),
                    K("Compatch finder lists compatibility patches for mods you use together that you have not subscribed to yet."),
                    K("Health check tells you before you press Play whether the playset is ready. Clear the shader cache is in Advanced too.") },
            K("All of these are in the Advanced menu or the right-click menu.")),
        new TourPage("✉", K("Share, language and look"),
            K("A few things that make the program yours."),
            new[] { K("Export > Copy a share code turns a whole playset into one line of text. Import > Paste a share code builds it again for a friend."),
                    K("The language button in the top bar switches the program between eleven languages at once."),
                    K("The sun and moon switch changes between the dark and the light look."),
                    K("Updates checks GitHub when you press it and can update the program in place.") },
            K("That is the tour. Enjoy your court!")),
    };

    readonly MainWindow main;
    public int Page;
    readonly TourCanvas canvas;
    public static TourWindow Open(MainWindow main)
    {
        if (Current != null) { Current.Activate(); return Current; }
        var w = new TourWindow(main);
        Current = w;
        w.ShowSeparate(main);
        return w;
    }
    TourWindow(MainWindow owner) : base(Tr("Quick tour - The Royal Court"), 900, 600, false)
    {
        main = owner; ShowInTaskbar = true;
        canvas = new TourCanvas(this);
        Content = canvas;
        Closed += (_, _) => { if (Current == this) Current = null; };
        KeyDown += (_, e) =>
        {
            int n = Pages.Length;
            if (e.Key == Key.Right || e.Key == Key.Enter || e.Key == Key.Space) { if (Page >= n - 1) { if (e.Key != Key.Right) Finish(); } else { Page++; canvas.InvalidateVisual(); } e.Handled = true; }
            else if (e.Key == Key.Left && Page > 0) { Page--; canvas.InvalidateVisual(); e.Handled = true; }
            else if (e.Key == Key.Escape) { Finish(); e.Handled = true; }
        };
        Closing += (_, _) => { if (!main.mm.St.TutorialDone) { main.mm.St.TutorialDone = true; main.mm.SaveSettingsNow(); } };
    }
    public void GoTo(int p) { Page = Math.Clamp(p, 0, Pages.Length - 1); canvas.InvalidateVisual(); }
    void Finish() { if (!main.mm.St.TutorialDone) { main.mm.St.TutorialDone = true; main.mm.SaveSettingsNow(); } Close(); }

    sealed class TourCanvas : Control
    {
        readonly TourWindow w;
        readonly List<(Rect r, int id)> hits = new();
        int hot = -1;
        public TourCanvas(TourWindow t) { w = t; Focusable = true; }
        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed += InvalidateVisual; }
        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= InvalidateVisual; }
        int HitAt(Point p) { foreach (var h in hits) if (h.r.Contains(p)) return h.id; return -1; }
        protected override void OnPointerMoved(PointerEventArgs e)
        {
            int id = HitAt(e.GetPosition(this));
            if (id != hot) { hot = id; Cursor = id >= 0 ? new Cursor(StandardCursorType.Hand) : Cursor.Default; InvalidateVisual(); }
        }
        protected override void OnPointerExited(PointerEventArgs e) { if (hot != -1) { hot = -1; InvalidateVisual(); } }
        protected override async void OnPointerReleased(PointerReleasedEventArgs e)
        {
            int id = HitAt(e.GetPosition(this)), n = Pages.Length;
            if (id >= 0 && id < n) w.Page = id;
            else if (id == 100 && w.Page > 0) w.Page--;
            else if (id == 101) { if (w.Page >= n - 1) { w.Finish(); return; } w.Page++; }
            else if (id == 102) { w.Finish(); return; }
            else if (id == 103)
            {
                var items = new List<MenuDef> { new(1, Tr("Automatic (same as Windows)"), w.main.mm.St.Language == "auto"), MenuDef.Separator };
                for (int i = 0; i < Count; i++) items.Add(new(2 + i, Langs[i].Native, w.main.mm.St.Language == Langs[i].Code));
                var r = hits.First(h => h.id == 103).r;
                int pick = await PopupMenu.Show(w, this.PointToScreen(new Point(r.X, r.Bottom + 2)), items);
                if (pick == 1) await w.main.SetUiLanguage("auto"); else if (pick >= 2) await w.main.SetUiLanguage(Langs[pick - 2].Code);
                return;
            }
            else return;
            InvalidateVisual();
        }
        static double TextH(string s, double size, bool bold, double width) { var f = Txt.Ft(s, size, bold, Brushes.Black); f.MaxTextWidth = Math.Max(10, width); return f.Height; }
        static void Wrapped(DrawingContext dc, string s, double x, double y, double width, Color c, double size = 16, bool bold = false)
        {
            var f = Txt.Ft(s, size, bold, Gfx.Br(c)); f.MaxTextWidth = Math.Max(10, width); dc.DrawText(f, new Point(x, y));
        }
        public override void Render(DrawingContext dc)
        {
            var t = Skin.Cur; double W = Bounds.Width, H = Bounds.Height;
            int n = Pages.Length; w.Page = Math.Clamp(w.Page, 0, n - 1);
            var pg = Pages[w.Page];
            hits.Clear();
            dc.DrawRectangle(Gfx.Br(t.Bg), null, new Rect(0, 0, W, H));
            const double sideW = 230;
            dc.DrawRectangle(Gfx.Br(t.Banner), null, new Rect(0, 0, sideW, H));
            dc.DrawRectangle(Gfx.Br(t.Gold), null, new Rect(sideW - 2, 0, 2, H));
            Txt.Draw(dc, "♛", 22, 38, t.Gold, 24, false, double.PositiveInfinity, Txt.Symbols);
            Txt.Draw(dc, "The Royal Court", 56, 38, t.Text, Txt.Size, true, sideW - 66);
            double y = 84;
            var langR = new Rect(14, H - 64 + 14, sideW - 18 - 14, 36);
            for (int i = 0; i < n; i++)
            {
                var row = new Rect(10, y, sideW - 24, 40);
                bool cur = i == w.Page, isHot = i == hot;
                if (cur || isHot) dc.DrawRectangle(Gfx.Br(cur ? t.BtnHot : t.Btn, cur ? 255 : 160), null, Gfx.RR(row.X, row.Y, row.Width, row.Height, 8));
                double cx = row.X + 22, cy = row.Y + 20;
                dc.DrawEllipse(Gfx.Br(i < w.Page ? t.Ok : cur ? t.Gold : t.Border), null, new Point(cx, cy), 11, 11);
                hits.Add((row, i));
                string num = i < w.Page ? "✓" : (i + 1).ToString();
                var nc = i == w.Page ? t.AccentText : i < w.Page ? t.Text : t.Muted;
                var f0 = Txt.Ft(num, i < w.Page ? Txt.Size : 12, false, Gfx.Br(nc), i < w.Page ? Txt.Symbols : null); f0.TextAlignment = TextAlignment.Center; f0.MaxTextWidth = 22;
                dc.DrawText(f0, new Point(row.X + 11, cy - f0.Height / 2));
                Txt.Draw(dc, Tr(Pages[i].Title), row.X + 40, cy, i == w.Page ? t.Text : t.Muted, 16, i == w.Page, row.Width - 46);
                y += 44;
            }
            {   // language chip
                dc.DrawRectangle(Gfx.Br(hot == 103 ? t.BtnHot : t.Btn), new Pen(Gfx.Br(t.Border), 1), Gfx.RR(langR.X + 0.5, langR.Y + 0.5, langR.Width - 1, langR.Height - 1, 9));
                Txt.Draw(dc, "◎", langR.X + 10, langR.Center.Y, t.Gold, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
                Txt.Draw(dc, Langs[Lang.Current].Native, langR.X + 36, langR.Center.Y, t.Text, 16, false, langR.Width - 62);
                Txt.Draw(dc, "▾", langR.Right - 22, langR.Center.Y, t.Muted, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
                hits.Add((langR, 103));
            }
            double rx = sideW + 34, rw = W - rx - 34, barY = H - 64;
            void Button(string label, int id, double x, double wd, bool primary)
            {
                var r = new Rect(x, barY + 14, wd, 36);
                bool h = hot == id;
                Color fill = primary ? (h ? Look.Mix(t.Accent, t.Text, 12) : t.Accent) : (h ? t.BtnHot : t.Btn);
                dc.DrawRectangle(Gfx.Br(fill), new Pen(Gfx.Br(primary ? fill : t.Border), 1), Gfx.RR(r.X + 0.5, r.Y + 0.5, r.Width - 1, r.Height - 1, 9));
                var f = Txt.Ft(label, 16, primary, Gfx.Br(primary ? t.AccentText : t.Text)); f.TextAlignment = TextAlignment.Center; f.MaxTextWidth = wd;
                dc.DrawText(f, new Point(r.X, r.Center.Y - f.Height / 2));
                hits.Add((r, id));
            }
            double BW(string label, bool primary) => Math.Max(96, Txt.W(label, 16, primary) + 36);
            bool last = w.Page == n - 1;
            string lNext = last ? Tr("Start") : Tr("Next") + "  ›", lBack = "‹  " + Tr("Back"), lSkip = Tr("Skip the tour");
            double wNext = BW(lNext, true), wBack = BW(lBack, false), wSkip = BW(lSkip, false), xr = W - 34;
            Button(lNext, 101, xr - wNext, wNext, true); xr -= wNext + 10;
            if (w.Page > 0) { Button(lBack, 100, xr - wBack, wBack, false); xr -= wBack + 10; }
            if (!last) Button(lSkip, 102, rx, wSkip, false);
            double top = 34;
            Txt.Draw(dc, pg.Glyph, rx, top + 22, t.Gold, 24, false, double.PositiveInfinity, Txt.Symbols);
            Txt.Draw(dc, Tr(pg.Title), rx + 48, top + 22, t.Text, 28, true, rw - 48);
            dc.DrawRectangle(Gfx.Br(t.Gold), null, new Rect(rx, top + 52, 64, 3));
            double y0 = top + 72;
            string intro = Tr(pg.Intro);
            double ih = TextH(intro, 16, false, rw);
            Wrapped(dc, intro, rx, y0, rw, t.Muted);
            double yb = y0 + ih + 16;
            foreach (var ptx in pg.Pts)
            {
                string txt = Tr(ptx);
                double bh = TextH(txt, 16, false, rw - 26);
                Txt.Draw(dc, "◆", rx, yb + 11, t.Gold, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
                Wrapped(dc, txt, rx + 26, yb, rw - 26, t.Text);
                yb += bh + 14;
            }
            {
                string tip = Tr(pg.Tip);
                double th = TextH(tip, 16, false, rw - 34), boxH = th + 24, boxY = Math.Max(yb + 6, barY - boxH - 12);
                dc.DrawRectangle(Gfx.Br(t.List), new Pen(Gfx.Br(t.Gold, 150), 1), Gfx.RR(rx + 0.5, boxY + 0.5, rw - 1, boxH - 1, 10));
                Txt.Draw(dc, "★", rx + 12, boxY + 22, t.Gold, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
                Wrapped(dc, tip, rx + 34, boxY + 12, rw - 44, t.Text);
            }
        }
    }
}
