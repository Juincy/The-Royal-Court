using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Styling;
namespace RoyalCourt.Ui;

public enum Role { Text, Muted, Gold, Ok, Warn, Bad }

/// <summary>A line of text in a theme colour.</summary>
public sealed class RcLabel : Control
{
    public string Text { get => text; set { if (text == value) return; text = value; InvalidateMeasure(); InvalidateVisual(); } }
    string text = "";
    public Role Role = Role.Text;
    public double Size = Txt.Size;
    public bool Bold, Wrap, End = true, Right, Center;
    public string? Family;
    public RcLabel(string t = "", Role r = Role.Text) { text = t; Role = r; IsHitTestVisible = false; }
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= InvalidateVisual; }
    public static Color ColorOf(Role r, Look t) => r switch { Role.Muted => t.Muted, Role.Gold => t.Gold, Role.Ok => t.Ok, Role.Warn => t.Warn, Role.Bad => t.Bad, _ => t.Text };
    FormattedText Make(double maxW)
    {
        var f = Txt.Ft(text, Size, Bold, Gfx.Br(ColorOf(Role, Skin.Cur)), Family, Wrap ? double.PositiveInfinity : End ? maxW : double.PositiveInfinity);
        if (Wrap) { f.MaxTextWidth = Math.Max(10, maxW); }
        if (Center) f.TextAlignment = TextAlignment.Center; else if (Right) f.TextAlignment = TextAlignment.Right;
        return f;
    }
    protected override Size MeasureOverride(Size s)
    {
        if (Wrap) { var f = Make(double.IsInfinity(s.Width) ? 4000 : s.Width); return new Size(Math.Min(f.Width, double.IsInfinity(s.Width) ? f.Width : s.Width), f.Height); }
        return new Size(Txt.W(text, Size, Bold, Family) + 2, Txt.H(Size, Bold, Family));
    }
    public override void Render(DrawingContext dc)
    {
        var f = Make(Bounds.Width);
        double y = Wrap ? 0 : (Bounds.Height - f.Height) / 2;
        dc.DrawText(f, new Point(0, y));
    }
}

/// <summary>A drop-down list that matches the theme.</summary>
public sealed class RcCombo : Control
{
    public List<string> Items = new();
    public int Index = -1;
    public event Action<int>? Changed;
    bool hot;
    public RcCombo() { Focusable = true; Cursor = new Cursor(StandardCursorType.Hand); }
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= InvalidateVisual; }
    protected override void OnPointerEntered(PointerEventArgs e) { hot = true; InvalidateVisual(); }
    protected override void OnPointerExited(PointerEventArgs e) { hot = false; InvalidateVisual(); }
    public void SetItems(IEnumerable<string> items, int index) { Items = items.ToList(); Index = index < Items.Count ? index : -1; InvalidateVisual(); }
    protected override async void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!IsEnabled || Items.Count == 0) return;
        Focus();
        var def = Items.Select((s, i) => new MenuDef(i + 1, s, i == Index)).ToList();
        var top = TopLevel.GetTopLevel(this) as Window; if (top == null) return;
        var pt = this.PointToScreen(new Point(0, Bounds.Height));
        int r = await PopupMenu.Show(top, pt, def, minWidth: (int)Bounds.Width);
        if (r > 0 && r - 1 != Index) { Index = r - 1; InvalidateVisual(); Changed?.Invoke(Index); }
    }
    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur; double w = Bounds.Width, h = Bounds.Height;
        dc.DrawRectangle(Gfx.Br(t.List), new Pen(Gfx.Br(hot ? t.Gold : t.Border), 1), Gfx.RR(0.5, 0.5, w - 1, h - 1, 4));
        string s = Index >= 0 && Index < Items.Count ? Items[Index] : "";
        Txt.Draw(dc, s, 8, h / 2, t.Text, Txt.Size, false, Math.Max(4, w - 30));
        Txt.Draw(dc, "▾", w - 18, h / 2, t.Gold, 11, false, double.PositiveInfinity, Txt.Symbols);
    }
}

/// <summary>A single-line text box in the theme's colours.</summary>
public sealed class RcEdit : TextBox
{
    protected override Type StyleKeyOverride => typeof(TextBox);
    public RcEdit()
    {
        FontSize = Txt.Size; MinHeight = 26; Padding = new Thickness(6, 3); CornerRadius = new CornerRadius(4);
        VerticalContentAlignment = VerticalAlignment.Center;
        Apply();
        Skin.Changed += Apply;
    }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Skin.Changed -= Apply; }
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Skin.Changed -= Apply; Skin.Changed += Apply; Apply(); }
    void Apply()
    {
        var t = Skin.Cur;
        var bg = new SolidColorBrush(t.List); var fg = new SolidColorBrush(t.Text); var bd = new SolidColorBrush(t.Border); var gold = new SolidColorBrush(t.Gold);
        Background = bg; Foreground = fg; BorderBrush = bd; CaretBrush = fg; FontFamily = new FontFamily(Txt.Family);
        SelectionBrush = new SolidColorBrush(t.Sel); SelectionForegroundBrush = new SolidColorBrush(t.SelText);
        foreach (var k in new[] { "TextControlBackground", "TextControlBackgroundPointerOver", "TextControlBackgroundFocused", "TextControlBackgroundDisabled" }) Resources[k] = bg;
        foreach (var k in new[] { "TextControlForeground", "TextControlForegroundPointerOver", "TextControlForegroundFocused", "TextControlForegroundDisabled" }) Resources[k] = fg;
        foreach (var k in new[] { "TextControlBorderBrush", "TextControlBorderBrushDisabled" }) Resources[k] = bd;
        foreach (var k in new[] { "TextControlBorderBrushPointerOver", "TextControlBorderBrushFocused" }) Resources[k] = gold;
        Resources["TextControlPlaceholderForeground"] = new SolidColorBrush(t.Muted);
    }
}

public sealed record MenuDef(int Id, string Text, bool Check = false, bool Enabled = true, bool Sep = false, bool Header = false)
{
    public static MenuDef Separator => new(0, "", false, false, true);
    /// <summary>A small gold section title; it cannot be clicked.</summary>
    public static MenuDef Title(string text) => new(0, text, false, false, false, true);
}

/// <summary>A pop-up menu drawn in the theme (the system menu would stay white). Returns the id picked, or 0.</summary>
public sealed class PopupMenu : Control
{
    readonly List<MenuDef> items; int hot = -1; readonly double width;
    readonly TaskCompletionSource<int> tcs = new();
    Window? win;
    /// <summary>The most recent menu window (the screenshot tool photographs it).</summary>
    public static Window? Last;
    PopupMenu(List<MenuDef> it, double minW)
    {
        items = it;
        double w = minW;
        foreach (var d in it) if (!d.Sep) w = Math.Max(w, Txt.W(d.Text) + (d.Header ? 30 : 52));
        width = w;
    }
    double ItemH(MenuDef d) => d.Sep ? 9 : d.Header ? 26 : 27;
    protected override Size MeasureOverride(Size s) => new(width, items.Sum(ItemH) + 8);
    public static Task<int> Show(Window owner, PixelPoint at, List<MenuDef> items, int minWidth = 0)
    {
        var m = new PopupMenu(items, minWidth);
        var t = Skin.Cur;
        var w = new Window
        {
            SystemDecorations = SystemDecorations.None, ShowInTaskbar = false, Topmost = true, CanResize = false, SizeToContent = SizeToContent.WidthAndHeight,
            WindowStartupLocation = WindowStartupLocation.Manual, Background = new SolidColorBrush(Skin.Dark ? t.Banner : t.List), Content = m,
        };
        m.win = w; Last = w;
        var scr = owner.Screens.ScreenFromPoint(at) ?? owner.Screens.Primary;
        double sc = owner.RenderScaling;
        m.Measure(Size.Infinity);
        var sz = m.DesiredSize;
        int px = at.X, py = at.Y;
        if (scr != null)
        {
            int wpx = (int)(sz.Width * sc), hpx = (int)(sz.Height * sc);
            if (px + wpx > scr.WorkingArea.Right) px = Math.Max(scr.WorkingArea.X, scr.WorkingArea.Right - wpx);
            if (py + hpx > scr.WorkingArea.Bottom) py = Math.Max(scr.WorkingArea.Y, at.Y - hpx - 4);
        }
        w.Position = new PixelPoint(px, py);
        w.Deactivated += (_, _) => m.Close(0);
        w.KeyDown += (_, e) => { if (e.Key == Key.Escape) m.Close(0); };
        w.Closed += (_, _) => m.tcs.TrySetResult(0);
        w.Show(owner);
        w.Activate();
        return m.tcs.Task;
    }
    void Close(int id) { tcs.TrySetResult(id); try { win?.Close(); } catch { } }
    int At(double y)
    {
        double a = 4;
        for (int i = 0; i < items.Count; i++) { double h = ItemH(items[i]); if (y >= a && y < a + h) return i; a += h; }
        return -1;
    }
    protected override void OnPointerMoved(PointerEventArgs e) { int i = At(e.GetPosition(this).Y); if (i >= 0 && (items[i].Sep || items[i].Header || !items[i].Enabled)) i = -1; if (i != hot) { hot = i; InvalidateVisual(); } }
    protected override void OnPointerExited(PointerEventArgs e) { hot = -1; InvalidateVisual(); }
    protected override void OnPointerReleased(PointerReleasedEventArgs e) { int i = At(e.GetPosition(this).Y); if (i >= 0 && !items[i].Sep && !items[i].Header && items[i].Enabled) Close(items[i].Id); }
    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur; double w = Bounds.Width, y = 4;
        Color back = Skin.Dark ? t.Banner : t.List;
        dc.DrawRectangle(Gfx.Br(back), new Pen(Gfx.Br(t.Border), 1), new Rect(0.5, 0.5, w - 1, Bounds.Height - 1));
        for (int i = 0; i < items.Count; i++)
        {
            var d = items[i]; double h = ItemH(d);
            if (d.Sep) { dc.DrawLine(Gfx.P(t.Border, 1), new Point(10, y + h / 2), new Point(w - 10, y + h / 2)); y += h; continue; }
            if (d.Header) { Txt.Draw(dc, d.Text.ToUpperInvariant(), 14, y + h / 2 + 3, t.Gold, Txt.Size - 2, true, Math.Max(4, w - 28)); y += h; continue; }
            if (i == hot) dc.DrawRectangle(Gfx.Br(t.BtnHot), Gfx.P(t.Gold, 1, 160), Gfx.RR(y > 0 ? 3.5 : 3.5, y + 1.5, w - 7, h - 3, 5));
            if (d.Check)
            {
                var g = new StreamGeometry(); double cx = 16, cy = y + h / 2, s = 4;
                using (var c = g.Open()) { c.BeginFigure(new Point(cx - s, cy), false); c.LineTo(new Point(cx - s * 0.2, cy + s * 0.8)); c.LineTo(new Point(cx + s, cy - s * 0.8)); c.EndFigure(false); }
                dc.DrawGeometry(null, Gfx.P(d.Enabled ? t.Gold : t.Muted, 2, 255, true), g);
            }
            Txt.Draw(dc, d.Text, 34, y + h / 2, d.Enabled ? t.Text : Look.Mix(t.Muted, back, 30), Txt.Size, false, Math.Max(4, w - 46));
            y += h;
        }
    }
}

/// <summary>The base of every window of the app: themed background, centred over the owner, optional keyboard shortcuts.</summary>
public class RcWindow : Window
{
    public RcWindow(string title, double w, double h, bool resizable = true)
    {
        Title = title; Width = w; Height = h; CanResize = resizable; ShowInTaskbar = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Icon = MainWindow.AppIcon;
        Apply();
        Skin.Changed += Apply;
        Closed += (_, _) => Skin.Changed -= Apply;
        // a trace for the diagnostics log: if a window ever takes the program down, the last lines say what it was doing
        PropertyChanged += (_, e) => { if (e.Property == WindowStateProperty) RoyalCourt.Core.Log.Line("window \"" + Title + "\": " + WindowState); };
        Opened += (_, _) => RoyalCourt.Core.Log.Line("window opened: " + Title);
        Closed += (_, _) => RoyalCourt.Core.Log.Line("window closed: " + Title);
        FontFamily = new FontFamily(Txt.Family); FontSize = Txt.Size;
    }
    void Apply() { Background = new SolidColorBrush(Skin.Cur.Bg); RequestedThemeVariant = Skin.Dark ? ThemeVariant.Dark : ThemeVariant.Light; }
}
