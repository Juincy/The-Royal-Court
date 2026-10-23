using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;
namespace RoyalCourt.Ui;

/// <summary>One clock for every animation. While something moves (a fade, the theme slide) it is driven by the screen's own frame callback, so every step
/// lands exactly on a screen refresh (a timer cannot: on Windows it ticks in 15.6 ms steps, which looks like sticking). Otherwise a slow timer: 30 Hz while the window is in front, 4 Hz behind.</summary>
public static class Anim
{
    public static event Action<double>? Frame;
    static DispatcherTimer? timer;
    static TopLevel? host;
    static double last, lastFrameAt;
    static int holds, chk; static bool pending;
    public static bool Active = true;
    /// <summary>Re-reads whether any window of the app is in front (call after a window gains or loses focus).</summary>
    public static void Refresh()
    {
        bool any = true;
        if (Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime l)
        { any = false; foreach (var w in l.Windows) if (w.IsVisible && w.IsActive) { any = true; break; } }
        if (any == Active) return;
        Active = any;
        if (any && timer != null) { last = Skin.Sec; timer.Interval = TimeSpan.FromMilliseconds(33); }
    }
    /// <summary>True while something asked for smooth motion (Hold without Release).</summary>
    public static bool Moving => holds > 0;
    public static void Hold() { if (holds++ == 0) Kick(); }
    public static void Release() { if (holds > 0) holds--; }
    public static void Start(TopLevel top)
    {
        if (timer != null) return;
        host = top; last = Skin.Sec;
        timer = new DispatcherTimer(DispatcherPriority.Render) { Interval = TimeSpan.FromMilliseconds(33) };
        timer.Tick += (_, _) =>
        {
            // while the frame callback runs it does the work; the timer only steps in when the callback has stopped (window hidden or minimised)
            if (holds > 0 && Skin.Sec - lastFrameAt < 0.1) return;
            // nothing is moving and no window of the app is in front: stand still (the clock only checks back twice a second)
            if (!Active || (++chk & 15) == 0) Refresh();   // tool windows and other apps do not raise the main window's events
            if (holds == 0 && !Active) { last = Skin.Sec; if ((int)timer!.Interval.TotalMilliseconds != 500) timer.Interval = TimeSpan.FromMilliseconds(500); return; }
            Pulse();
            int want = holds > 0 ? 50 : 33;
            if ((int)timer!.Interval.TotalMilliseconds != want) timer.Interval = TimeSpan.FromMilliseconds(want);
        };
        timer.Start();
    }
    public static void Stop() { timer?.Stop(); timer = null; host = null; }
    static void Pulse()
    {
        double now = Skin.Sec, dt = Math.Min(0.25, now - last); last = now;
        Frame?.Invoke(dt);
    }
    static void Kick()
    {
        if (pending || host == null) return;
        pending = true;
        host.RequestAnimationFrame(OnFrame);
    }
    static void OnFrame(TimeSpan _)
    {
        pending = false; lastFrameAt = Skin.Sec;
        if (holds <= 0) return;
        Pulse(); Kick();
    }
}

/// <summary>Text drawing and measuring with the app's fonts.</summary>
public static class Txt
{
    public static string Family = "Segoe UI, Inter, Arial";
    public const string Symbols = "Segoe UI Symbol, Segoe UI Emoji, Noto Sans Symbols2, DejaVu Sans, Inter";
    public const double Size = 12;
    public static void SetLanguage(string code)
    {
        Family = code == "zh" ? "Microsoft YaHei UI, Noto Sans CJK SC, Segoe UI, Inter" : code == "ja" ? "Yu Gothic UI, Noto Sans CJK JP, Segoe UI, Inter" : code == "ko" ? "Malgun Gothic, Noto Sans CJK KR, Segoe UI, Inter" : "Segoe UI, Inter, Arial";
    }
    public static Typeface Face(bool bold = false, string? fam = null) => new(fam ?? Family, FontStyle.Normal, bold ? FontWeight.SemiBold : FontWeight.Normal);
    public static FormattedText Ft(string s, double size, bool bold, IBrush b, string? fam = null, double maxW = double.PositiveInfinity)
    {
        var f = new FormattedText(s, CultureInfo.CurrentUICulture, FlowDirection.LeftToRight, Face(bold, fam), size, b);
        if (!double.IsInfinity(maxW)) { f.MaxTextWidth = Math.Max(1, maxW); f.MaxLineCount = 1; f.Trimming = TextTrimming.CharacterEllipsis; }
        return f;
    }
    public static double W(string s, double size = Size, bool bold = false, string? fam = null) => s.Length == 0 ? 0 : Ft(s, size, bold, Brushes.Black, fam).WidthIncludingTrailingWhitespace;
    public static double H(double size = Size, bool bold = false, string? fam = null) => Ft("Ag", size, bold, Brushes.Black, fam).Height;
    /// <summary>Draws one line, vertically centred on cy (or top-aligned at y when cy is NaN).</summary>
    public static void Draw(DrawingContext dc, string s, double x, double cy, Color c, double size = Size, bool bold = false, double maxW = double.PositiveInfinity, string? fam = null, int a = 255)
    {
        if (s.Length == 0) return;
        var f = Ft(s, size, bold, Gfx.Br(c, a), fam, maxW);
        dc.DrawText(f, new Point(x, cy - f.Height / 2));
    }
}

/// <summary>The rounded button of the Windows version: a glyph and a label, gold edge and light fade on hover.</summary>
public sealed class RcButton : Control
{
    public string Glyph = "", Label = "", Tip = "";
    public bool Primary, InBanner;
    public Func<bool>? PrimaryIf;
    public double MinW = 76;
    public event Action? Click;
    double hov; bool hot, down, held;
    public RcButton() { Focusable = true; Cursor = new Cursor(StandardCursorType.Hand); }
    public void SetText(string label, string tip) { Label = label; Tip = tip; ToolTip.SetTip(this, tip.Length == 0 ? null : tip); InvalidateMeasure(); InvalidateVisual(); }
    bool IsPrimary => Primary || (PrimaryIf?.Invoke() ?? false);
    public double WantW()
    {
        double gs = Glyph.Length > 0 ? Txt.W(Glyph, Txt.Size, false, Txt.Symbols) : 0, ls = Label.Length > 0 ? Txt.W(Label, Txt.Size, IsPrimary) : 0;
        return Math.Max(MinW, gs + (Glyph.Length > 0 && Label.Length > 0 ? 8 : 0) + ls + 36);
    }
    protected override Size MeasureOverride(Size s) => new(WantW(), 30);
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Anim.Frame += Step; Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Anim.Frame -= Step; Skin.Changed -= InvalidateVisual; if (held) { held = false; Anim.Release(); } }
    void Step(double dt)
    {
        double target = hot ? 1 : 0;
        if (hov != target)
        {
            hov = hot ? Math.Min(1, hov + dt / 0.10) : Math.Max(0, hov - dt / 0.15);
            InvalidateVisual();
        }
        if (hov == target && !hot && held) { held = false; Anim.Release(); }   // the fade is over: back to the slow clock
    }
    protected override void OnPointerEntered(PointerEventArgs e) { hot = true; if (!held) { held = true; Anim.Hold(); } base.OnPointerEntered(e); }
    protected override void OnPointerExited(PointerEventArgs e) { hot = false; down = false; InvalidateVisual(); base.OnPointerExited(e); }
    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!IsEnabled || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) return;
        down = true; Focus(); InvalidateVisual(); e.Handled = true;
    }
    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        bool was = down; down = false; InvalidateVisual();
        if (was && IsEnabled && new Rect(Bounds.Size).Contains(e.GetPosition(this))) Click?.Invoke();
        e.Handled = true;
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (IsEnabled && (e.Key == Key.Enter || e.Key == Key.Space)) { Click?.Invoke(); e.Handled = true; }
        base.OnKeyDown(e);
    }
    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs c) { base.OnPropertyChanged(c); if (c.Property == IsEnabledProperty) InvalidateVisual(); }
    public void Fire() { if (IsEnabled) Click?.Invoke(); }
    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur;
        double w = Bounds.Width, h = Bounds.Height;
        bool dis = !IsEnabled, primary = IsPrimary;
        Color around = InBanner ? t.Banner : t.Bg;
        dc.DrawRectangle(Gfx.Br(around), null, new Rect(0, 0, w, h));
        Color fill = primary ? (down ? Look.Mix(t.Accent, Colors.Black, 20) : Look.Mix(t.Accent, Colors.White, 18 * hov)) : (down ? t.BtnDown : Look.Mix(t.Btn, t.BtnHot, 100 * hov));
        Color edge = primary ? Look.Mix(t.Accent, Colors.Black, 25) : Look.Mix(t.Border, t.Gold, 100 * hov);
        dc.DrawRectangle(Gfx.Br(fill), new Pen(Gfx.Br(edge), 1), Gfx.RR(0.5, 0.5, w - 1, h - 1, 5));
        Color fg = dis ? Look.Mix(t.Muted, fill, 40) : primary ? t.AccentText : t.Text;
        Color gfg = dis ? fg : primary ? t.AccentText : t.Gold;
        double gs = Glyph.Length > 0 ? Txt.W(Glyph, Txt.Size, false, Txt.Symbols) : 0, ls = Label.Length > 0 ? Txt.W(Label, Txt.Size, primary) : 0;
        double gap = Glyph.Length > 0 && Label.Length > 0 ? 7 : 0;
        double x = (w - (gs + gap + ls)) / 2, cy = h / 2;
        if (Glyph.Length > 0) Txt.Draw(dc, Glyph, x, cy, gfg, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
        if (Label.Length > 0) Txt.Draw(dc, Label, x + gs + gap, cy, fg, Txt.Size, primary);
        if (IsKeyboardFocusWithin && IsEnabled)
            dc.DrawRectangle(null, new Pen(Gfx.Br(Look.Mix(fg, fill, 35)), 1, new DashStyle(new double[] { 1, 2 }, 0)), new Rect(4.5, 4.5, Math.Max(0, w - 9), Math.Max(0, h - 9)));
    }
}

/// <summary>A column of the list.</summary>
public sealed class RcColumn
{
    public string Title = "";
    public double W = 100, Min = 30;
    public bool Right;
    public bool Sortable = true;
}

/// <summary>The big list: drawn by hand so thousands of rows cost nothing. Zebra rows, check boxes, multi-select, column sort arrows, drag reorder.</summary>
public sealed class RcList : Control
{
    public const double RowH = 24;
    public bool ShowHeader = true;
    public double HeadH => ShowHeader ? 28 : 0;
    public List<RcColumn> Cols = new();
    public bool Checkboxes = true, AllowDrag;
    public int Count;
    public Func<int, int, string> Cell = (_, _) => "";
    public Func<int, int, Color?>? CellFg;                // null colour: the row's own
    public Func<int, int>? RowKind;                        // 0 normal, 1 muted (disabled), 2 faint (pending)
    public Func<int, bool>? IsChecked;
    public Func<int, IImage?>? Icon; public int IconCol = -1;    // a small picture before the text of this column (a grey square while it is not loaded)
    public Func<int, Color?>? RowBg;                       // extra tint (conflict severity etc.)
    public Func<int, int, double>? CellBar;                // >= 0: a bar of that length (0..1) behind the cell text
    public int SortCol = -1; public bool SortAsc = true;
    public readonly SortedSet<int> Sel = new();
    public int FocusRow = -1, Anchor = -1;
    public double Off;
    public event Action<int, bool>? CheckChanged;
    public event Action<int>? HeaderClick;
    public event Action<int>? Activated;
    public event Action<int, Point>? RowContext;     // row (or -1), point in screen units of this control
    public event Action<int, int>? DragMoved;              // from row, to row (final index)
    public event Action? SelectionChanged;
    public event Action? ColumnsResized;
    public event Func<bool>? DragBlocked;
    double vp => Math.Max(0, Bounds.Height - HeadH);
    int dragFrom = -1, dragMark = -1; bool dragging; Point pressAt; int resizeCol = -1; double resizeX, resizeW; bool sbDrag; double sbGrab;
    public RcList() { Focusable = true; ClipToBounds = true; }
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= InvalidateVisual; }
    public double TotalW() { double s = 0; foreach (var c in Cols) s += c.W; return s; }
    double Content => Count * RowH;
    double MaxOff => Math.Max(0, Content - vp);
    public void Refresh() { Off = Math.Clamp(Off, 0, MaxOff); InvalidateVisual(); }
    public void EnsureVisible(int row)
    {
        if (row < 0 || row >= Count) return;
        if (row * RowH < Off) Off = row * RowH; else if ((row + 1) * RowH > Off + vp) Off = (row + 1) * RowH - vp;
        Off = Math.Clamp(Off, 0, MaxOff); InvalidateVisual();
    }
    public void SelectOnly(int row) { Sel.Clear(); if (row >= 0 && row < Count) { Sel.Add(row); FocusRow = row; Anchor = row; EnsureVisible(row); } SelectionChanged?.Invoke(); InvalidateVisual(); }
    public void SetSelection(IEnumerable<int> rows) { Sel.Clear(); foreach (var r in rows) if (r >= 0 && r < Count) Sel.Add(r); if (Sel.Count > 0) { FocusRow = Sel.Min; Anchor = FocusRow; } SelectionChanged?.Invoke(); InvalidateVisual(); }
    public int RowAt(double y) { double v = y - HeadH + Off; if (y < HeadH || v < 0) return -1; int r = (int)(v / RowH); return r < Count ? r : -1; }
    double CheckW => Checkboxes ? 24 : 0;
    int ColAt(double x) { double a = 0; for (int i = 0; i < Cols.Count; i++) { a += Cols[i].W; if (x < a) return i; } return -1; }
    int EdgeAt(double x) { double a = 0; for (int i = 0; i < Cols.Count; i++) { a += Cols[i].W; if (Math.Abs(x - a) <= 4) return i; } return -1; }
    Rect SbTrack() => new(Bounds.Width - 12, HeadH, 12, vp);
    Rect SbThumb()
    {
        var tr = SbTrack(); if (Content <= vp || vp <= 0) return default;
        double th = Math.Max(24, tr.Height * vp / Content), y = tr.Y + (tr.Height - th) * (MaxOff <= 0 ? 0 : Off / MaxOff);
        return new Rect(tr.X + 2, y, 8, th);
    }

    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur; double W = Bounds.Width, H = Bounds.Height;
        dc.DrawRectangle(Gfx.Br(t.List), null, new Rect(0, 0, W, H));
        int first = Math.Max(0, (int)(Off / RowH)), last = Math.Min(Count - 1, (int)((Off + vp) / RowH));
        using (dc.PushClip(new Rect(0, HeadH, W, vp)))
            for (int r = first; r <= last; r++)
            {
                double y = HeadH + r * RowH - Off;
                bool sel = Sel.Contains(r);
                int kind = RowKind?.Invoke(r) ?? 0;
                Color bg = sel ? t.Sel : RowBg?.Invoke(r) ?? ((r & 1) != 0 ? t.Alt : t.List);
                Color fg = sel ? t.SelText : kind == 0 ? t.Text : t.Muted;
                if (!sel && kind == 2) fg = Look.Mix(fg, bg, 50);
                dc.DrawRectangle(Gfx.Br(bg), null, new Rect(0, y, W, RowH));
                double x = 0;
                for (int c = 0; c < Cols.Count; c++)
                {
                    double cw = Cols[c].W, tx = x + 8;
                    Color cf = fg;
                    if (!sel && CellFg?.Invoke(r, c) is Color o) cf = o;
                    if (c == 0 && Checkboxes)
                    {
                        bool on = IsChecked?.Invoke(r) ?? false;
                        var box = new Rect(x + 6, y + (RowH - 14) / 2, 14, 14);
                        dc.DrawRectangle(on ? Gfx.Br(t.Accent) : null, new Pen(Gfx.Br(on ? t.Accent : Look.Mix(t.Border, t.Muted, 40)), 1), Gfx.RR(box.X + 0.5, box.Y + 0.5, 13, 13, 3));
                        if (on)
                        {
                            var g = new StreamGeometry();
                            using (var cx = g.Open()) { cx.BeginFigure(new Point(box.X + 3, box.Y + 7.5), false); cx.LineTo(new Point(box.X + 6, box.Y + 10.5)); cx.LineTo(new Point(box.X + 11.5, box.Y + 3.8)); cx.EndFigure(false); }
                            dc.DrawGeometry(null, Gfx.P(t.AccentText, 2, 255, true), g);
                        }
                        tx = x + CheckW + 6;
                    }
                    if (c == IconCol)
                    {
                        var ir = new Rect(tx, y + 2, 20, 20);
                        var img = Icon?.Invoke(r);
                        if (img != null) { using (dc.PushClip(ir)) dc.DrawImage(img, new Rect(img.Size), ir); }
                        else dc.DrawRectangle(Gfx.Br(Look.Mix(sel ? t.Sel : bg, t.Muted, 22)), null, Gfx.RR(ir.X, ir.Y, 20, 20, 3));
                        tx += 26;
                    }
                    string s = Cell(r, c);
                    if (CellBar?.Invoke(r, c) is double fr && fr >= 0)
                    {
                        double full = cw - 6, len = Math.Round(full * Math.Min(1.0, fr)); if (fr > 0 && len < 2) len = 2;
                        dc.DrawRectangle(Gfx.Br(Look.Mix(t.Accent, sel ? t.Sel : bg, 45)), null, new Rect(x + 3, y + 3, len, RowH - 6));
                    }
                    if (c == 6 && s == "✓") Txt.Draw(dc, s, tx, y + RowH / 2, cf, Txt.Size, false, double.PositiveInfinity, Txt.Symbols);
                    else if (s.Length > 0) Txt.Draw(dc, s, tx, y + RowH / 2, cf, Txt.Size, false, Math.Max(4, cw - (tx - x) - 6));
                    x += cw;
                }
                if (sel && FocusRow == r && IsKeyboardFocusWithin) dc.DrawRectangle(null, new Pen(Gfx.Br(t.Gold, 120), 1, new DashStyle(new double[] { 1, 2 }, 0)), new Rect(0.5, y + 0.5, W - 1, RowH - 1));
            }
        if (dragging && dragMark >= 0)
        {
            double y = HeadH + dragMark * RowH - Off;
            dc.DrawLine(Gfx.P(t.Gold, 2), new Point(0, y), new Point(W - 12, y));
        }
        if (Count == 0 && EmptyText.Length > 0) Txt.Draw(dc, EmptyText, 16, HeadH + 24, t.Muted);
        if (!ShowHeader) { var th0 = SbThumb(); if (th0.Width > 0) dc.DrawRectangle(Gfx.Br(Look.Mix(t.Muted, t.List, 55)), null, Gfx.RR(th0.X, th0.Y, th0.Width, th0.Height, 4)); return; }
        // header
        dc.DrawRectangle(Gfx.Br(t.Banner), null, new Rect(0, 0, W, HeadH));
        double hx = 0;
        for (int c = 0; c < Cols.Count; c++)
        {
            bool sorted = c == SortCol;
            double cw = Cols[c].W, left = hx + (c == 0 && Checkboxes ? CheckW + 6 : 8);
            Txt.Draw(dc, Cols[c].Title, left, HeadH / 2, sorted ? t.Gold : t.Muted, Txt.Size, true, Math.Max(4, cw - (left - hx) - (sorted ? 22 : 6)));
            if (sorted) Txt.Draw(dc, SortAsc ? "▲" : "▼", hx + cw - 20, HeadH / 2, t.Gold, 9, false, double.PositiveInfinity, Txt.Symbols);
            var pn = Gfx.P(t.Border, 1);
            dc.DrawLine(pn, new Point(hx + cw - 0.5, 5), new Point(hx + cw - 0.5, HeadH - 5));
            hx += cw;
        }
        dc.DrawLine(Gfx.P(t.Border, 1), new Point(0, HeadH - 0.5), new Point(W, HeadH - 0.5));
        var th = SbThumb();
        if (th.Width > 0) dc.DrawRectangle(Gfx.Br(Look.Mix(t.Muted, t.List, 55)), null, Gfx.RR(th.X, th.Y, th.Width, th.Height, 4));
    }
    public string EmptyText = "";

    public void SelectRange(int a, int b) { Sel.Clear(); for (int i = Math.Min(a, b); i <= Math.Max(a, b); i++) Sel.Add(i); }
    protected override void OnPointerWheelChanged(PointerWheelEventArgs e) { Off = Math.Clamp(Off - e.Delta.Y * RowH * 3, 0, MaxOff); InvalidateVisual(); e.Handled = true; }
    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        Focus();
        var p = e.GetPosition(this); var pr = e.GetCurrentPoint(this).Properties;
        if (SbThumb().Contains(p) && pr.IsLeftButtonPressed) { sbDrag = true; sbGrab = p.Y - SbThumb().Y; e.Pointer.Capture(this); return; }
        if (ShowHeader && p.Y < HeadH)
        {
            int edge = EdgeAt(p.X);
            if (edge >= 0 && pr.IsLeftButtonPressed) { resizeCol = edge; resizeX = p.X; resizeW = Cols[edge].W; e.Pointer.Capture(this); return; }
            int hc = ColAt(p.X);
            if (hc >= 0 && pr.IsLeftButtonPressed && Cols[hc].Sortable) HeaderClick?.Invoke(hc);
            return;
        }
        int row = RowAt(p.Y);
        var mods = e.KeyModifiers;
        if (pr.IsRightButtonPressed)
        {
            if (row >= 0 && !Sel.Contains(row)) SelectOnly(row);
            RowContext?.Invoke(row, p);
            return;
        }
        if (!pr.IsLeftButtonPressed) return;
        if (row < 0) { if ((mods & (KeyModifiers.Control | KeyModifiers.Shift)) == 0) SelectOnly(-1); return; }
        if (Checkboxes && p.X >= 4 && p.X <= CheckW + 4 && (mods & (KeyModifiers.Control | KeyModifiers.Shift)) == 0)
        {
            CheckChanged?.Invoke(row, !(IsChecked?.Invoke(row) ?? false));
            InvalidateVisual();
            return;
        }
        if (e.ClickCount == 2) { Activated?.Invoke(row); return; }
        if ((mods & KeyModifiers.Shift) != 0 && Anchor >= 0) { SelectRange(Anchor, row); FocusRow = row; }
        else if ((mods & KeyModifiers.Control) != 0) { if (!Sel.Remove(row)) Sel.Add(row); FocusRow = row; Anchor = row; }
        else
        {
            if (!Sel.Contains(row) || Sel.Count == 1) { Sel.Clear(); Sel.Add(row); }
            FocusRow = row; Anchor = row;
            if (AllowDrag) { dragFrom = row; pressAt = p; e.Pointer.Capture(this); }
        }
        SelectionChanged?.Invoke(); InvalidateVisual();
    }
    protected override void OnPointerMoved(PointerEventArgs e)
    {
        var p = e.GetPosition(this);
        if (sbDrag)
        {
            var tr = SbTrack(); var th = SbThumb();
            double span = tr.Height - th.Height; if (span > 0) Off = Math.Clamp((p.Y - sbGrab - tr.Y) / span * MaxOff, 0, MaxOff);
            InvalidateVisual(); return;
        }
        if (resizeCol >= 0) { Cols[resizeCol].W = Math.Max(Cols[resizeCol].Min, resizeW + p.X - resizeX); ColumnsResized?.Invoke(); InvalidateVisual(); return; }
        if (dragFrom >= 0 && !dragging && Math.Abs(p.Y - pressAt.Y) > 5)
        {
            if (DragBlocked?.Invoke() == true) { dragFrom = -1; e.Pointer.Capture(null); return; }
            dragging = true;
        }
        if (dragging)
        {
            int r = (int)Math.Round((p.Y - HeadH + Off) / RowH);
            dragMark = Math.Clamp(r, 0, Count);
            if (p.Y < HeadH + 8) Off = Math.Clamp(Off - 8, 0, MaxOff); else if (p.Y > Bounds.Height - 8) Off = Math.Clamp(Off + 8, 0, MaxOff);
            InvalidateVisual(); return;
        }
        Cursor = p.Y < HeadH && EdgeAt(p.X) >= 0 ? new Cursor(StandardCursorType.SizeWestEast) : Cursor.Default;
    }
    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        e.Pointer.Capture(null);
        if (sbDrag) { sbDrag = false; return; }
        if (resizeCol >= 0) { resizeCol = -1; ColumnsResized?.Invoke(); return; }
        if (dragging && dragFrom >= 0 && dragMark >= 0)
        {
            int to = dragMark; if (to > dragFrom) to--;
            int from = dragFrom; dragging = false; dragFrom = -1; dragMark = -1; InvalidateVisual();
            if (to != from) DragMoved?.Invoke(from, to);
            return;
        }
        dragging = false; dragFrom = -1; dragMark = -1; InvalidateVisual();
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        int n = Count; if (n == 0) return;
        bool shift = (e.KeyModifiers & KeyModifiers.Shift) != 0, ctrl = (e.KeyModifiers & KeyModifiers.Control) != 0;
        int page = Math.Max(1, (int)(vp / RowH) - 1), f = FocusRow < 0 ? 0 : FocusRow, to = f;
        switch (e.Key)
        {
            case Key.Down: to = Math.Min(n - 1, f + 1); break;
            case Key.Up: to = Math.Max(0, f - 1); break;
            case Key.PageDown: to = Math.Min(n - 1, f + page); break;
            case Key.PageUp: to = Math.Max(0, f - page); break;
            case Key.Home: to = 0; break;
            case Key.End: to = n - 1; break;
            case Key.A when ctrl: SelectRange(0, n - 1); SelectionChanged?.Invoke(); InvalidateVisual(); e.Handled = true; return;
            case Key.Space:
                if (Checkboxes) { foreach (var r in Sel.ToArray()) CheckChanged?.Invoke(r, !(IsChecked?.Invoke(r) ?? false)); InvalidateVisual(); e.Handled = true; }
                return;
            case Key.Enter: if (FocusRow >= 0) Activated?.Invoke(FocusRow); e.Handled = true; return;
            default: return;
        }
        if (shift && Anchor >= 0) SelectRange(Anchor, to); else { Sel.Clear(); Sel.Add(to); Anchor = to; }
        FocusRow = to; EnsureVisible(to); SelectionChanged?.Invoke(); InvalidateVisual(); e.Handled = true;
    }
}
