using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
namespace RoyalCourt.Ui;

/// <summary>The 64 px banner: crown, name, version, author, the gold line and (Halloween) the whole scene.</summary>
public sealed class Banner : Control
{
    public const double Height_ = 64;
    public string Sub = "", Author = "";
    public double ArtR;
    protected override Size MeasureOverride(Size s) => new(double.IsInfinity(s.Width) ? 800 : s.Width, Height_);
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Anim.Frame += Step; Skin.Changed += InvalidateVisual; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Anim.Frame -= Step; Skin.Changed -= InvalidateVisual; }
    int skip;
    void Step(double dt) { if (Skin.Halloween && (Anim.Active || (++skip & 7) == 0)) InvalidateVisual(); }

    static void Spaced(DrawingContext dc, string s, double x, double cy, Color c, double size, bool bold, double spacing, out double width)
    {
        double xx = x;
        foreach (var r in s.EnumerateRunes())
        {
            string ch = r.ToString();
            Txt.Draw(dc, ch, xx, cy, c, size, bold);
            xx += Txt.W(ch, size, bold) + spacing;
        }
        width = xx - x;
    }
    public double SubWidth()
    {
        double w = 0; foreach (var r in Sub.EnumerateRunes()) w += Txt.W(r.ToString(), 11, true) + 2;
        return w;
    }
    public override void Render(DrawingContext dc)
    {
        var t = Skin.Cur; double W = Bounds.Width, H = Bounds.Height, cy = (H - 2) / 2;
        bool hw = Skin.Halloween; double sec = Skin.Sec;
        dc.DrawRectangle(Gfx.Br(t.Banner), null, new Rect(0, 0, W, H));
        string title = "The Royal Court";
        double tw = Txt.W(title, 24, true, "Georgia, Times New Roman, serif");
        double subX = 58 + tw + 4 + 10;
        if (hw) Gfx.HalloweenScene(dc, W, H, 1.0, sec, subX + SubWidth() + 40, ArtR > 0 ? ArtR : W);
        double bas = H - 2;
        dc.DrawRectangle(Gfx.Br(t.Gold), null, new Rect(0, bas, W, 2));
        if (hw)
        {
            double sw = 190, x = (sec * 170) % (W + sw * 2) - sw;
            var sh = new LinearGradientBrush
            {
                StartPoint = new RelativePoint(x, 0, RelativeUnit.Absolute), EndPoint = new RelativePoint(x + sw, 0, RelativeUnit.Absolute),
                GradientStops = { new GradientStop(Color.FromArgb(0, 255, 232, 160), 0), new GradientStop(Color.FromArgb(240, 255, 232, 160), 0.5), new GradientStop(Color.FromArgb(0, 255, 232, 160), 1) },
            };
            dc.DrawRectangle(sh, null, new Rect(x, bas, sw, 2));
        }
        double fl = hw ? 0.5 + 0.5 * Math.Sin(sec * 8.0) * Math.Sin(sec * 3.1 + 0.7) : 0.0;
        Gfx.Crown(dc, 10, cy - 15, 40, 30, t.Gold, fl, hw);
        Txt.Draw(dc, title, 58, cy, t.Text, 24, true, double.PositiveInfinity, "Georgia, Times New Roman, serif");
        Spaced(dc, Sub, subX, cy + 6, t.Muted, 11, true, 2, out _);
        Txt.Draw(dc, Author, subX, cy + 19, Look.Mix(t.Muted, t.Banner, 30), 9);
    }
}

/// <summary>The theme picker: the pumpkin on a stalk above the sun/moon switch (an upside-down T).</summary>
public sealed class ThemePicker : Control
{
    public double TogT = 1, HwT;           // eased toward the target while the theme changes
    public event Action<int>? Pick;        // 1 = pumpkin, 2 = switch
    int zone; bool down;
    public ThemePicker() { Cursor = new Cursor(StandardCursorType.Hand); }
    protected override Size MeasureOverride(Size s) => new(Gfx.PickW, Gfx.PickH);
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) { base.OnAttachedToVisualTree(e); Anim.Frame += Step; }
    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) { base.OnDetachedFromVisualTree(e); Anim.Frame -= Step; }
    int skip;
    void Step(double dt) { if (Anim.Active || (++skip & 7) == 0) InvalidateVisual(); }
    protected override void OnPointerMoved(PointerEventArgs e) { zone = Gfx.PickerZone(e.GetPosition(this).X, e.GetPosition(this).Y, 1); }
    protected override void OnPointerEntered(PointerEventArgs e) { zone = Gfx.PickerZone(e.GetPosition(this).X, e.GetPosition(this).Y, 1); }
    protected override void OnPointerExited(PointerEventArgs e) { zone = 0; down = false; }
    protected override void OnPointerPressed(PointerPressedEventArgs e) { if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) { down = true; zone = Gfx.PickerZone(e.GetPosition(this).X, e.GetPosition(this).Y, 1); e.Handled = true; } }
    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        bool was = down; down = false;
        var p = e.GetPosition(this);
        if (was && new Rect(Bounds.Size).Contains(p)) Pick?.Invoke(Gfx.PickerZone(p.X, p.Y, 1));
    }
    public override void Render(DrawingContext dc) => Gfx.Picker(dc, Bounds.Width, Bounds.Height, 1.0, TogT, HwT, zone, down, Skin.Sec);
}
