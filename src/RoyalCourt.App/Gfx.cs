using Avalonia;
using Avalonia.Media;
namespace RoyalCourt.Ui;

/// <summary>Drawing helpers and the animated pictures: sun, moon, pumpkin, the 3D day/night switch, the crown, the Halloween scenery.</summary>
public static class Gfx
{
    public static IBrush Br(Color c, int a = 255) => new SolidColorBrush(Color.FromArgb((byte)Math.Clamp(a, 0, 255), c.R, c.G, c.B));
    public static IBrush Br(byte r, byte g, byte b, int a = 255) => new SolidColorBrush(Color.FromArgb((byte)Math.Clamp(a, 0, 255), r, g, b));
    public static Color Rgb(uint v) => Color.FromRgb((byte)(v >> 16), (byte)(v >> 8), (byte)v);
    public static Color Lerp(Color a, Color b, double t) => Look.Mix(a, b, t * 100);
    public static Color Alpha(Color c, int a) => Color.FromArgb((byte)Math.Clamp(a, 0, 255), c.R, c.G, c.B);
    public static Pen P(Color c, double w, int a = 255, bool round = false) => new(Br(c, a), w, lineCap: round ? PenLineCap.Round : PenLineCap.Flat, lineJoin: round ? PenLineJoin.Round : PenLineJoin.Miter);

    public static float Frand(uint n)   // a fixed pseudo-random 0..1 (the same scenery on every repaint)
    {
        unchecked { n *= 2654435761u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16; }
        return (n & 0xFFFF) / 65535.0f;
    }
    public static double Ease(double p) { p = Math.Clamp(p, 0, 1); return p * p * (3.0 - 2.0 * p); }

    public static StreamGeometry Poly(params Point[] pts)
    {
        var g = new StreamGeometry();
        using (var c = g.Open())
        {
            c.BeginFigure(pts[0], true);
            for (int i = 1; i < pts.Length; i++) c.LineTo(pts[i]);
            c.EndFigure(true);
        }
        return g;
    }
    public static void Poly(DrawingContext dc, IBrush b, params Point[] pts) => dc.DrawGeometry(b, null, Poly(pts));
    public static void Oval(DrawingContext dc, IBrush? b, Pen? p, double x, double y, double w, double h) => dc.DrawEllipse(b, p, new Point(x + w / 2, y + h / 2), w / 2, h / 2);
    public static RoundedRect RR(double x, double y, double w, double h, double r) => new(new Rect(x, y, Math.Max(0, w), Math.Max(0, h)), Math.Min(r, Math.Min(w, h) / 2));
    public static void Glow(DrawingContext dc, double cx, double cy, double r, Color c, int centerAlpha)
    {
        if (centerAlpha <= 0) return;
        var br = new RadialGradientBrush { GradientStops = { new GradientStop(Alpha(c, centerAlpha), 0), new GradientStop(Alpha(c, 0), 1) } };
        dc.DrawEllipse(br, null, new Point(cx, cy), r, r);
    }
    static LinearGradientBrush Lin(double x0, double y0, double x1, double y1, Color a, Color b) => new()
    {
        StartPoint = new RelativePoint(x0, y0, RelativeUnit.Absolute), EndPoint = new RelativePoint(x1, y1, RelativeUnit.Absolute),
        GradientStops = { new GradientStop(a, 0), new GradientStop(b, 1) },
    };
    public static LinearGradientBrush LinearAbs(double x0, double y0, double x1, double y1, Color a, Color b) => Lin(x0, y0, x1, y1, a, b);

    // ---------------- sun, moon, stars, pumpkin ----------------
    public static void Star4(DrawingContext dc, double cx, double cy, double r, Color c, int alpha)
    {
        if (alpha <= 0) return;
        var p = new Point[8];
        for (int i = 0; i < 8; i++)
        {
            double a = i * Math.PI / 4 - Math.PI / 2, rr = (i & 1) != 0 ? r * 0.30 : r;
            p[i] = new Point(cx + Math.Cos(a) * rr, cy + Math.Sin(a) * rr);
        }
        Poly(dc, Br(c, alpha), p);
    }
    /// <summary>sec >= 0 animates it: the rays turn slowly and breathe.</summary>
    public static void Sun(DrawingContext dc, double cx, double cy, double size, Color c, int alpha = 255, double sec = -1)
    {
        if (alpha <= 0) return;
        double breathe = sec >= 0 ? 1.0 + 0.07 * Math.Sin(sec * 2.2) : 1.0, rot = sec >= 0 ? sec * 0.55 : 0.0;
        double r0 = size * 0.15 * breathe;
        dc.DrawEllipse(Br(c, alpha), null, new Point(cx, cy), r0, r0);
        var pen = P(c, size * 0.075, alpha, true);
        for (int i = 0; i < 8; i++)
        {
            double a = i * Math.PI / 4 + rot;
            double outer = r0 * (2.6 + (sec >= 0 ? 0.28 * Math.Sin(sec * 2.2 + i * 0.8) : 0.0));
            dc.DrawLine(pen, new Point(cx + Math.Cos(a) * r0 * 1.9, cy + Math.Sin(a) * r0 * 1.9), new Point(cx + Math.Cos(a) * outer, cy + Math.Sin(a) * outer));
        }
    }
    static readonly (double x, double y, double r, double ph)[] moonStars = { (0.40, -0.20, 0.085, 0.0), (0.30, 0.24, 0.06, 2.1), (-0.02, -0.40, 0.055, 4.2) };
    /// <summary>A crescent: a disc with an offset disc cut out of it, so it works on any background. sec >= 0: stars twinkle beside it.</summary>
    public static void Moon(DrawingContext dc, double cx, double cy, double size, Color c, int alpha = 255, double sec = -1)
    {
        if (alpha <= 0) return;
        double r = size * 0.30, r2 = r * 0.82;
        var disc = new EllipseGeometry(new Rect(cx - r, cy - r, r * 2, r * 2));
        var bite = new EllipseGeometry(new Rect(cx - r2 + r * 0.62, cy - r2 - r * 0.38, r2 * 2, r2 * 2));
        dc.DrawGeometry(Br(c, alpha), null, new CombinedGeometry(GeometryCombineMode.Exclude, disc, bite));
        if (sec >= 0)
            foreach (var s in moonStars)
            {
                double tw = 0.5 + 0.5 * Math.Sin(sec * 2.6 + s.ph);
                Star4(dc, cx + size * s.x, cy + size * s.y, size * s.r * (0.7 + 0.5 * tw), c, (int)(alpha * (0.25 + 0.75 * tw)));
            }
    }
    /// <summary>A jack-o'-lantern. lit 0..1: unlit it is a dim orange pumpkin with a dark face; lit the face glows and flickers like a candle.</summary>
    public static void Pumpkin(DrawingContext dc, double cx, double cy, double size, double lit, double sec, int alpha = 255)
    {
        double r = size * 0.5;
        double fl = 0.80 + 0.20 * Math.Sin(sec * 9.0 + 1.3) * Math.Sin(sec * 3.7 + 0.4);
        double f = lit * fl;
        if (lit > 0.02) Glow(dc, cx, cy, r * 2.2, Rgb(0xFF8C14), (int)(105.0 * f * alpha / 255.0));
        Color bodyA = Lerp(Rgb(0x925022), Rgb(0xFFA030), lit), bodyB = Lerp(Rgb(0x5C2C14), Rgb(0xE85A0C), lit);
        double[] lobe = { -0.46, 0.46, 0.0 };
        for (int i = 0; i < 3; i++)
        {
            double w = r * (i == 2 ? 1.25 : 1.05), h = r * 1.62, x = cx + lobe[i] * r - w / 2, y = cy + r * 0.12 - h / 2;
            dc.DrawEllipse(Lin(x, y, x + w, y + h, Alpha(bodyA, alpha), Alpha(bodyB, alpha)), null, new Point(x + w / 2, y + h / 2), w / 2, h / 2);
        }
        {
            double sw = Math.Sin(sec * 1.3) * r * 0.06;
            dc.DrawLine(P(Rgb(0x4E7A2E), r * 0.26, alpha, true), new Point(cx, cy - r * 0.62), new Point(cx + sw + r * 0.10, cy - r * 0.98));
        }
        Color face = Lerp(Rgb(0x2A1008), Rgb(0xFFE270), Math.Min(f, 1));
        var fb = Br(face, alpha);
        Poly(dc, fb, new Point(cx - r * 0.62, cy - r * 0.06), new Point(cx - r * 0.20, cy - r * 0.06), new Point(cx - r * 0.40, cy - r * 0.44));
        Poly(dc, fb, new Point(cx + r * 0.20, cy - r * 0.06), new Point(cx + r * 0.62, cy - r * 0.06), new Point(cx + r * 0.40, cy - r * 0.44));
        Poly(dc, fb, new Point(cx - r * 0.64, cy + r * 0.28), new Point(cx - r * 0.40, cy + r * 0.20), new Point(cx - r * 0.22, cy + r * 0.42), new Point(cx, cy + r * 0.22), new Point(cx + r * 0.22, cy + r * 0.42),
            new Point(cx + r * 0.40, cy + r * 0.20), new Point(cx + r * 0.64, cy + r * 0.28), new Point(cx + r * 0.46, cy + r * 0.62), new Point(cx + r * 0.20, cy + r * 0.52), new Point(cx, cy + r * 0.66),
            new Point(cx - r * 0.20, cy + r * 0.52), new Point(cx - r * 0.46, cy + r * 0.62));
    }

    // ---------------- the 3D day/night switch ----------------
    /// <summary>tt: 0 day (knob left) .. 1 night; hw: 0 Halloween off .. 1 on. Both are eased while the theme changes.</summary>
    public static void Switch(DrawingContext dc, double x, double y, double W, double H, double tt, double hw, bool hot, bool down, double u, double sec)
    {
        Color trackTop = Lerp(Lerp(Rgb(0xF2D8BC), Rgb(0x070B22), tt), Rgb(0x2A0C30), hw), trackBot = Lerp(Lerp(Rgb(0xFFF1DE), Rgb(0x162455), tt), Rgb(0x5A222C), hw);
        Color rimA = Lerp(Lerp(Rgb(0xFF9E3D), Rgb(0x2FB4FF), tt), Rgb(0xFF7A18), hw), rimB = Lerp(Lerp(Rgb(0xC23B66), Rgb(0x5B4BFF), tt), Rgb(0x8B4DFF), hw);
        Color knA = Lerp(Lerp(Rgb(0xFFC14A), Rgb(0x3CCBFF), tt), Rgb(0xFF9628), hw), knB = Lerp(Lerp(Rgb(0xF2452B), Rgb(0x5A3CF0), tt), Rgb(0x9A3CC8), hw);
        if (hot) { rimA = Look.Mix(rimA, Colors.White, 22); rimB = Look.Mix(rimB, Colors.White, 22); }
        double dark = Math.Max(tt, hw);
        for (int i = 3; i >= 1; i--)   // soft shadow under the whole switch
            dc.DrawRectangle(Br(0, 0, 0, (int)(22 + 4 * dark)), null, RR(x + 1 * u, y + (1 + i * 0.8) * u, W - 2 * u, H - 3 * u, H / 2));
        double tx = x + 1 * u, ty = y + 1 * u, tw = W - 2 * u, th = H - 4 * u;
        var track = RR(tx, ty, tw, th, th / 2);
        dc.DrawRectangle(Lin(tx, ty, tx, ty + th, trackTop, trackBot), null, track);
        {   // inner shadow along the top edge: the track is cut into the panel
            using (dc.PushClip(new Rect(tx, ty, tw, th)))
            {
                var ip = P(Colors.Black, 3.2 * u, (int)(70 + 50 * dark));
                dc.DrawRectangle(null, ip, RR(tx, ty + 2.2 * u, tw, th, th / 2));
            }
        }
        dc.DrawRectangle(null, new Pen(Lin(tx, ty, tx + tw, ty, rimA, rimB), 1.8 * u), track);
        dc.DrawRectangle(null, P(Colors.White, 1 * u, (int)(90 - 50 * dark)), RR(tx + 1 * u, ty + 1 * u, tw - 2 * u, th - 2 * u, (th - 2 * u) / 2));
        double k = th - 5 * u, pad = (th - k) / 2, kxL = tx + pad, kxR = tx + tw - pad - k;
        double kx = kxL + (kxR - kxL) * tt, ky = ty + pad + (down ? 0.8 * u : 0.0);
        double sunX = kxL + k / 2, moonX = kxR + k / 2, cyT = ty + th / 2;
        double ka = 1.0 - 0.72 * hw;   // the knob is ghosted while Halloween is on
        Sun(dc, sunX, cyT, k, Rgb(0xFFB25A), (int)(95 * tt), sec);
        Moon(dc, moonX, cyT, k * 1.05, Rgb(0x5A3CF0), (int)(85 * (1 - tt) * (1 - hw)), sec);
        for (int i = 3; i >= 1; i--)   // knob cast shadow
            Oval(dc, Br(0, 0, 0, (int)((36 + 10 * dark) * ka)), null, kx - (i - 1) * 0.3 * u, ky + (1 + i * 0.9) * u, k + (i - 1) * 0.6 * u, k);
        Oval(dc, Lin(kx, ky, kx + k, ky + k, Alpha(knA, (int)(255 * ka)), Alpha(knB, (int)(255 * ka))), null, kx, ky, k, k);
        {   // bevel: light edge top-left, dark edge bottom-right
            var bv = new Pen(Lin(kx, ky, kx + k, ky + k, Color.FromArgb((byte)(150 * ka), 255, 255, 255), Color.FromArgb((byte)(120 * ka), 0, 0, 0)), 1.4 * u);
            dc.DrawEllipse(null, bv, new Point(kx + k / 2, ky + k / 2), k / 2 - 0.7 * u, k / 2 - 0.7 * u);
        }
        {   // gloss on the upper half
            double gx = kx + k * 0.10, gy = ky + k * 0.05, gw = k * 0.80, gh = k * 0.52;
            Oval(dc, Lin(kx, ky + k * 0.05, kx, ky + k * 0.57, Color.FromArgb((byte)(120 * ka), 255, 255, 255), Color.FromArgb((byte)(6 * ka), 255, 255, 255)), null, gx, gy, gw, gh);
        }
        double kcx = kx + k / 2, kcy = ky + k / 2;
        if (tt < 0.999) Sun(dc, kcx, kcy, k, Colors.White, (int)(255 * (1 - tt) * ka), sec);
        if (tt > 0.001) Moon(dc, kcx, kcy, k * 1.05, Colors.White, (int)(255 * tt * ka), sec);
    }

    public const int PickW = 80, PickH = 54;
    /// <summary>1 = the pumpkin (Halloween), 2 = the day/night switch.</summary>
    public static int PickerZone(double x, double y, double u) => (y < 25.0 * u && Math.Abs(x - PickW * u / 2.0) < 17.0 * u) ? 1 : 2;

    /// <summary>The theme picker: an upside-down T with the pumpkin on a short stalk above the day/night switch.</summary>
    public static void Picker(DrawingContext dc, double W, double H, double u, double togT, double hw, int zone, bool down, double sec)
    {
        double cx = W / 2, od = 20 * u, oy = 1.5 * u;
        {   // the stalk that joins the pumpkin to the switch (the stem of the T)
            Color sc = Lerp(Lerp(Rgb(0xC23B66), Rgb(0x5B4BFF), togT), Rgb(0xFF7A18), hw);
            dc.DrawRectangle(Br(sc, 200), null, RR(cx - 1.6 * u, oy + od - 1.0 * u, 3.2 * u, 5.5 * u, 1.2 * u));
        }
        {
            double oa = oy + (zone == 1 && down ? 0.8 * u : 0.0);
            if (hw > 0.02) Glow(dc, cx, oa + od / 2, od * 1.25, Rgb(0xFF7814), (int)(80.0 * hw * (0.85 + 0.15 * Math.Sin(sec * 7.0))));
            Color top = Lerp(Lerp(Rgb(0xF6EAD4), Rgb(0x2E2442), togT), Rgb(0x582410), hw), bot = Lerp(Lerp(Rgb(0xE2D0B0), Rgb(0x161226), togT), Rgb(0x2A1020), hw);
            dc.DrawEllipse(Lin(cx, oa, cx, oa + od, top, bot), null, new Point(cx, oa + od / 2), od / 2, od / 2);
            Color rimC = Lerp(Rgb(0x5A4A7A), Rgb(0xFF8A1F), hw);
            if (zone == 1) rimC = Look.Mix(rimC, Colors.White, 25);
            dc.DrawEllipse(null, P(rimC, 1.4 * u), new Point(cx, oa + od / 2), od / 2 - 0.7 * u, od / 2 - 0.7 * u);
            Pumpkin(dc, cx, oa + od / 2 + 0.5 * u, od * 0.72, hw, sec, (int)(150 + 105 * hw));
        }
        Switch(dc, 0, 24 * u, W, H - 24 * u, togT, hw, zone == 2, zone == 2 && down, u, sec);
    }

    // ---------------- the crown ----------------
    public static void Crown(DrawingContext dc, double x0, double y0, double w, double h, Color gold, double flame, bool halloween)
    {
        Point P_(double a, double c) => new(x0 + a * w, y0 + c * h);
        var gb = Br(gold);
        Poly(dc, gb, P_(0.04, 0.86), P_(0.0, 0.18), P_(0.26, 0.50), P_(0.50, 0.0), P_(0.74, 0.50), P_(1.0, 0.18), P_(0.96, 0.86));
        dc.DrawRectangle(gb, null, new Rect(x0 + 0.04 * w, y0 + 0.86 * h, 0.92 * w, 0.14 * h));
        var jb = halloween ? Br((byte)(235 + 20 * flame), (byte)(110 + 100 * flame), 20) : Br(176, 40, 60);
        double r = 0.07 * w;
        Oval(dc, jb, null, x0 + 0.50 * w - r, y0 + 0.62 * h - r, 2 * r, 2 * r);
        Oval(dc, jb, null, x0 + 0.25 * w - r * 0.8, y0 + 0.70 * h - r * 0.8, 1.6 * r, 1.6 * r);
        Oval(dc, jb, null, x0 + 0.75 * w - r * 0.8, y0 + 0.70 * h - r * 0.8, 1.6 * r, 1.6 * r);
    }

    // ---------------- Halloween scenery ----------------
    static void Bat(DrawingContext dc, double cx, double cy, double s, double flap, int alpha)
    {
        var br = Br(8, 3, 12, alpha);
        Oval(dc, br, null, cx - s * 0.15, cy - s * 0.18, s * 0.30, s * 0.40);
        Poly(dc, br, new Point(cx - s * 0.13, cy - s * 0.10), new Point(cx - s * 0.10, cy - s * 0.32), new Point(cx - s * 0.01, cy - s * 0.16));
        Poly(dc, br, new Point(cx + s * 0.13, cy - s * 0.10), new Point(cx + s * 0.10, cy - s * 0.32), new Point(cx + s * 0.01, cy - s * 0.16));
        for (int side = -1; side <= 1; side += 2)
        {
            double sd = side;
            Poly(dc, br, new Point(cx + sd * s * 0.10, cy - s * 0.12), new Point(cx + sd * s * 0.55, cy - s * 0.30 - flap * s * 0.35), new Point(cx + sd * s * 1.00, cy - flap * s * 0.62),
                new Point(cx + sd * s * 0.80, cy + s * 0.10 - flap * s * 0.22), new Point(cx + sd * s * 0.58, cy + s * 0.02 - flap * s * 0.10), new Point(cx + sd * s * 0.42, cy + s * 0.22 - flap * s * 0.06),
                new Point(cx + sd * s * 0.24, cy + s * 0.10), new Point(cx + sd * s * 0.10, cy + s * 0.18));
        }
    }
    static void Spider(DrawingContext dc, double sx, double len, double u, double sec)
    {
        dc.DrawLine(P(Color.FromRgb(226, 216, 238), 0.8 * u, 120), new Point(sx, 0), new Point(sx, len));
        var body = Br(14, 8, 22);
        var leg = P(Color.FromRgb(14, 8, 22), 1.0 * u);
        double wig = Math.Sin(sec * 6.0) * 0.8 * u;
        for (int i = 0; i < 4; i++)
        {
            double dy = (i - 1.5) * 1.5 * u;
            dc.DrawLine(leg, new Point(sx - 1.5 * u, len + 2.8 * u + dy * 0.4), new Point(sx - 5.4 * u, len + 1.2 * u + dy + wig));
            dc.DrawLine(leg, new Point(sx + 1.5 * u, len + 2.8 * u + dy * 0.4), new Point(sx + 5.4 * u, len + 1.2 * u + dy - wig));
        }
        Oval(dc, body, null, sx - 2.6 * u, len + 1.0 * u, 5.2 * u, 5.6 * u);
        Oval(dc, body, null, sx - 1.7 * u, len - 0.8 * u, 3.4 * u, 3.0 * u);
        var eye = Br(255, 70, 70);
        Oval(dc, eye, null, sx - 1.2 * u, len - 0.2 * u, 0.9 * u, 0.9 * u);
        Oval(dc, eye, null, sx + 0.3 * u, len - 0.2 * u, 0.9 * u, 0.9 * u);
    }
    static void Cobweb(DrawingContext dc, double u)
    {
        var wp = P(Color.FromRgb(210, 198, 226), 0.8 * u, 70);
        const int N = 5;
        var ang = new double[N];
        for (int i = 0; i < N; i++) ang[i] = i * 1.5707963 / (N - 1);
        double R = 26.0 * u;
        for (int i = 0; i < N; i++) dc.DrawLine(wp, new Point(0, 0), new Point(Math.Cos(ang[i]) * R, Math.Sin(ang[i]) * R));
        for (int ring = 1; ring <= 3; ring++)
        {
            double rr = R * ring / 3.0;
            for (int i = 0; i + 1 < N; i++)
            {
                double am = (ang[i] + ang[i + 1]) / 2;
                var g = new StreamGeometry();
                using (var c = g.Open())
                {
                    c.BeginFigure(new Point(Math.Cos(ang[i]) * rr, Math.Sin(ang[i]) * rr), false);
                    c.QuadraticBezierTo(new Point(Math.Cos(am) * rr * 0.72, Math.Sin(am) * rr * 0.72), new Point(Math.Cos(ang[i + 1]) * rr, Math.Sin(ang[i + 1]) * rr));
                    c.EndFigure(false);
                }
                dc.DrawGeometry(null, wp, g);
            }
        }
    }
    /// <summary>Everything alive in the banner, drawn behind the controls: haze, embers, the moon and its clouds, the graveyard, bats, a spider on its thread.</summary>
    public static void HalloweenScene(DrawingContext dc, double W, double H, double u, double sec, double artL, double artR)
    {
        double bas = H - 2 * u;
        {   // a warm haze rising from the gold line
            var hz = Lin(0, H * 0.30, 0, bas, Color.FromArgb(0, 255, 110, 20), Color.FromArgb(80, 255, 96, 20));
            dc.DrawRectangle(hz, null, new Rect(0, H * 0.30, W, bas - H * 0.30));
        }
        double zoneW = artR - artL;
        if (zoneW > 110 * u)   // the moon, with clouds sliding over it
        {
            double mx = artL + zoneW * 0.58, my = H * 0.46, r = 14 * u;
            Glow(dc, mx, my, r * 3.6, Rgb(0xFFAA3C), 125);
            dc.DrawEllipse(Br(255, 228, 164), null, new Point(mx, my), r, r);
            var cr = Br(196, 146, 78, 48);
            Oval(dc, cr, null, mx - r * 0.55, my - r * 0.45, r * 0.55, r * 0.5);
            Oval(dc, cr, null, mx + r * 0.10, my + r * 0.15, r * 0.62, r * 0.55);
            Oval(dc, cr, null, mx - r * 0.20, my + r * 0.40, r * 0.30, r * 0.28);
            for (int k = 0; k < 2; k++)
            {
                double cw = (52.0 + 26.0 * k) * u;
                double x = artL - cw + ((sec * (5.0 + 3.5 * k) * u + k * 80.0 * u) % (zoneW + cw * 2));
                var cb = Br(26, 12, 34, 170);
                Oval(dc, cb, null, x, my + (k != 0 ? 5.0 : -9.0) * u, cw, 7.0 * u);
                Oval(dc, cb, null, x + cw * 0.2, my + (k != 0 ? 2.0 : -12.0) * u, cw * 0.5, 6.0 * u);
            }
        }
        for (int i = 0; i < 16; i++)   // embers drifting up
        {
            double sp = 7.0 + 9.0 * Frand((uint)(i * 3 + 1)), ph = Frand((uint)(i * 3 + 2)), cyc = H * 1.5;
            double pos = (sec * sp * u + ph * cyc) % cyc;
            double x = Frand((uint)(i * 3)) * W + Math.Sin(sec * 0.8 + i) * 5.0 * u, y = bas - pos;
            double tw = 0.55 + 0.45 * Math.Sin(sec * 5.0 + i * 1.7);
            int a = (int)(190.0 * (1.0 - pos / cyc) * tw);
            if (a <= 4) continue;
            double rr = (0.8 + 1.3 * Frand((uint)(i * 7 + 5))) * u;
            dc.DrawEllipse(Br(255, (byte)(150 + 80 * Frand((uint)(i + 40))), 40, a), null, new Point(x, y), rr, rr);
        }
        {   // the graveyard along the bottom
            var sil = Br(11, 5, 15);
            var tp = P(Color.FromRgb(11, 5, 15), 1.3 * u, 255, true);
            double step = 15.0 * u;
            for (int i = 0; i * step < W + step; i++)
            {
                double x = i * step + Frand((uint)(i + 900)) * 6.0 * u, h0 = Frand((uint)(i + 500));
                double lim = (x < artL ? 6.5 : 10.5) * u * (0.65 + 0.35 * Frand((uint)(i + 700)));
                if (h0 < 0.28)
                {
                    dc.DrawRectangle(sil, null, new Rect(x - 2.6 * u, bas - lim * 0.75, 5.2 * u, lim * 0.75));
                    Oval(dc, sil, null, x - 2.6 * u, bas - lim, 5.2 * u, lim * 0.5);
                }
                else if (h0 < 0.42)
                {
                    dc.DrawRectangle(sil, null, new Rect(x - 0.9 * u, bas - lim, 1.8 * u, lim));
                    dc.DrawRectangle(sil, null, new Rect(x - 2.6 * u, bas - lim * 0.72, 5.2 * u, 1.7 * u));
                }
                else if (h0 < 0.56)
                {
                    dc.DrawLine(tp, new Point(x, bas), new Point(x, bas - lim));
                    dc.DrawLine(tp, new Point(x, bas - lim * 0.55), new Point(x - 3.6 * u, bas - lim * 0.95));
                    dc.DrawLine(tp, new Point(x, bas - lim * 0.72), new Point(x + 3.4 * u, bas - lim * 1.05));
                }
                else if (h0 < 0.68 && (i & 1) == 0)
                {
                    double fl = 0.6 + 0.4 * Math.Sin(sec * 8.0 + i * 2.3) * Math.Sin(sec * 2.9 + i);
                    Oval(dc, Br(150, 60, 10), null, x - 3.2 * u, bas - 5.4 * u, 6.4 * u, 5.4 * u);
                    var fb = Br(255, 220, 90, (int)(120 + 135 * fl));
                    Oval(dc, fb, null, x - 1.9 * u, bas - 3.9 * u, 1.3 * u, 1.3 * u);
                    Oval(dc, fb, null, x + 0.6 * u, bas - 3.9 * u, 1.3 * u, 1.3 * u);
                }
            }
        }
        for (int k = 0; k < 3; k++)   // bats crossing the banner
        {
            double speed = (36.0 + 15.0 * k) * u, s = (8.5 + 2.0 * k) * u;
            double x = ((sec * speed + k * W * 0.37) % (W + 70 * u)) - 35 * u;
            double y = H * (0.26 + 0.17 * k) + Math.Sin(sec * 1.7 + k * 2.0) * 4.5 * u;
            Bat(dc, x, y, s, Math.Sin(sec * 13.0 + k * 1.7), 240);
        }
        if (zoneW > 150 * u)   // a spider bobbing on its thread
        {
            double len = 9.0 * u + 11.0 * u * (0.5 + 0.5 * Math.Sin(sec * 0.8));
            Spider(dc, artR - 40.0 * u, len, u, sec);
        }
        Cobweb(dc, u);
    }
}
