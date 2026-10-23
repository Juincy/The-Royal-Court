using Avalonia.Media;
namespace RoyalCourt.Ui;

/// <summary>The three looks: Parchment (light), Midnight Court (dark) and Halloween. Same colour roles as the Windows version.</summary>
public sealed class Look
{
    public Color Bg, Banner, List, Alt, Border, Text, Muted, Accent, AccentText, Gold, Btn, BtnHot, BtnDown, Sel, SelText, Ok, Warn, Bad;
    static Color C(uint rgb) => Color.FromRgb((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb);
    static Look Make(uint[] v) => new()
    {
        Bg = C(v[0]), Banner = C(v[1]), List = C(v[2]), Alt = C(v[3]), Border = C(v[4]), Text = C(v[5]), Muted = C(v[6]), Accent = C(v[7]), AccentText = C(v[8]),
        Gold = C(v[9]), Btn = C(v[10]), BtnHot = C(v[11]), BtnDown = C(v[12]), Sel = C(v[13]), SelText = C(v[14]), Ok = C(v[15]), Warn = C(v[16]), Bad = C(v[17]),
    };
    public static readonly Look Dark = Make(new uint[] { 0x14121C, 0x1C1928, 0x191723, 0x1F1C2C, 0x3A3550, 0xEEE9DC, 0x9C96B0, 0xD4AF37, 0x1A1400, 0xD4AF37, 0x2A2640, 0x3A345C, 0x221F35, 0x3A3366, 0xFFFFFF, 0x6FCF97, 0xE8A838, 0xFF6B6B });
    public static readonly Look Light = Make(new uint[] { 0xF4EFE4, 0xE9E1CF, 0xFCFAF4, 0xF6F1E6, 0xCFC4A8, 0x2A2433, 0x776E88, 0x4B2E83, 0xFFFFFF, 0xB08410, 0xE2D9C4, 0xD6CBB0, 0xCBBEA0, 0xDDD1F0, 0x1E1630, 0x2E7D4F, 0xB06206, 0xB3261E });
    public static readonly Look Halloween = Make(new uint[] { 0x0F0A13, 0x1A0E1D, 0x140C17, 0x1C111F, 0x4A2846, 0xF6E8D6, 0xB096AA, 0xFF7A18, 0x1A0A00, 0xFF8A1F, 0x2C182E, 0x462442, 0x241426, 0x4E245C, 0xFFFFFF, 0x7CE65A, 0xFFB02E, 0xFF5C5C });

    public const int ILight = 0, IDark = 1, IHalloween = 2;
    public static Look Of(int id) => id == ILight ? Light : id == IHalloween ? Halloween : Dark;

    public static Color Mix(Color a, Color b, double pctB)
    {
        double p = Math.Clamp(pctB, 0, 100) / 100.0;
        return Color.FromArgb(255, (byte)(a.R + (b.R - a.R) * p), (byte)(a.G + (b.G - a.G) * p), (byte)(a.B + (b.B - a.B) * p));
    }
    public static Color Blend(Look a, Look b, double t, Func<Look, Color> role) => Mix(role(a), role(b), t * 100);

    /// <summary>A theme between two others (the 420 ms slide).</summary>
    public static Look Between(Look a, Look b, double t)
    {
        if (t <= 0) return a;
        if (t >= 1) return b;
        var r = new Look();
        r.Bg = Mix(a.Bg, b.Bg, t * 100); r.Banner = Mix(a.Banner, b.Banner, t * 100); r.List = Mix(a.List, b.List, t * 100); r.Alt = Mix(a.Alt, b.Alt, t * 100);
        r.Border = Mix(a.Border, b.Border, t * 100); r.Text = Mix(a.Text, b.Text, t * 100); r.Muted = Mix(a.Muted, b.Muted, t * 100); r.Accent = Mix(a.Accent, b.Accent, t * 100);
        r.AccentText = Mix(a.AccentText, b.AccentText, t * 100); r.Gold = Mix(a.Gold, b.Gold, t * 100); r.Btn = Mix(a.Btn, b.Btn, t * 100); r.BtnHot = Mix(a.BtnHot, b.BtnHot, t * 100);
        r.BtnDown = Mix(a.BtnDown, b.BtnDown, t * 100); r.Sel = Mix(a.Sel, b.Sel, t * 100); r.SelText = Mix(a.SelText, b.SelText, t * 100);
        r.Ok = Mix(a.Ok, b.Ok, t * 100); r.Warn = Mix(a.Warn, b.Warn, t * 100); r.Bad = Mix(a.Bad, b.Bad, t * 100);
        return r;
    }
}

/// <summary>The theme on screen right now, the animation clock, and the "repaint everything" signal.</summary>
public static class Skin
{
    public static int Id = Look.IDark;
    public static Look Cur = Look.Dark;
    public static bool Dark => Id != Look.ILight;
    public static bool Halloween => Id == Look.IHalloween;
    public static event Action? Changed;
    public static void Set(int id, Look shown) { Id = id; Cur = shown; Changed?.Invoke(); }
    public static void Repaint(Look shown) { Cur = shown; Changed?.Invoke(); }

    static readonly System.Diagnostics.Stopwatch clock = System.Diagnostics.Stopwatch.StartNew();
    public static double Sec => clock.Elapsed.TotalSeconds;

    public static string Name(int id) => id == Look.ILight ? "light" : id == Look.IHalloween ? "halloween" : "dark";
    public static int FromName(string n) => n == "light" ? Look.ILight : n == "halloween" ? Look.IHalloween : n == "dark" ? Look.IDark : -1;
}
