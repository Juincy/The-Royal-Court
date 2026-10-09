"""Builds a 16:9 preview image (1280x720 JPEG) for ModDB or Steam. Usage: python3 tools/make_preview.py out.jpg [version] (needs Pillow)"""
import sys, importlib.util, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops
here = os.path.dirname(os.path.abspath(__file__))
def load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(here, name + ".py")); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m
mi = load("make_icon"); mb = load("make_banner")

W, H, K = 1280, 720, 2
GOLD, GOLD_D, GOLD_L = mi.GOLD, mi.GOLD_DARK, mi.GOLD_LIGHT
def font(p, s): return ImageFont.truetype(p, int(s * K))

def background():
    w, h = W * K, H * K
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=mi.lerp((46, 38, 74), (13, 11, 22), y / h))
    glow = Image.new("RGB", (w, h), (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, r = w // 2, 190 * K, 330 * K
    gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(120, 85, 20))
    glow = glow.filter(ImageFilter.GaussianBlur(110 * K))
    im = ImageChops.add(im, glow.point(lambda v: int(v * 0.6)))
    lat = Image.new("RGB", (w, h), (0, 0, 0))
    ld = ImageDraw.Draw(lat)
    step = 64 * K
    for i in range(-h, w + h, step):
        ld.line([(i, 0), (i + h, h)], fill=(255, 255, 255), width=1)
        ld.line([(i + h, 0), (i, h)], fill=(255, 255, 255), width=1)
    return Image.blend(im, ImageChops.add(im, lat.point(lambda v: int(v * 0.07))), 1.0)

def centered(d, y, text, f, fill, sp=0):
    if sp:
        total = sum(d.textlength(c, font=f) + sp * K for c in text) - sp * K
        x = (W * K - total) / 2
        for c in text:
            d.text((x, y), c, font=f, fill=fill); x += d.textlength(c, font=f) + sp * K
    else:
        d.text(((W * K - d.textlength(text, font=f)) / 2, y), text, font=f, fill=fill)

def main(out, version="0.17.0"):
    im = background().convert("RGBA")
    d = ImageDraw.Draw(im)
    for y0, y1 in ((0, 10), (H - 10, H)):
        d.rectangle([0, y0 * K, W * K, y1 * K], fill=GOLD_D)
    d.rectangle([0, 10 * K, W * K, 14 * K], fill=GOLD)
    d.rectangle([0, (H - 14) * K, W * K, (H - 10) * K], fill=GOLD)
    em = mb.emblem(210 * K) if hasattr(mb, "emblem") else None
    ex = (W * K - em.width) // 2; ey = 42 * K
    sh = Image.new("RGBA", im.size, (0, 0, 0, 0))
    sh.paste((0, 0, 0, 150), (ex + 8 * K, ey + 10 * K), em.split()[3])
    im = Image.alpha_composite(im, sh.filter(ImageFilter.GaussianBlur(12 * K)))
    im.alpha_composite(em, (ex, ey))
    d = ImageDraw.Draw(im)
    ft = font(mb.SER, 112)
    title = "The Royal Court"
    tw = d.textlength(title, font=ft)
    tx = (W * K - tw) / 2
    d.text((tx + 4 * K, 272 * K + 5 * K), title, font=ft, fill=(0, 0, 0, 140))
    d.text((tx, 272 * K), title, font=ft, fill=(246, 240, 226))
    ry = 412 * K
    d.rectangle([tx, ry, tx + tw, ry + 4 * K], fill=GOLD)
    dm = 11 * K; cxm = W * K / 2
    d.polygon([(cxm - dm, ry + 2 * K), (cxm, ry - dm + 2 * K), (cxm + dm, ry + 2 * K), (cxm, ry + dm + 2 * K)], fill=GOLD_L, outline=GOLD_D)
    centered(d, 432 * K, "CRUSADER KINGS III  MOD MANAGER", font(mb.SAN, 34), GOLD, 8)
    centered(d, 494 * K, "Fast. Lightweight. Native. Rule your load order.", font(mb.SANR, 30), (200, 196, 214))
    chips = ["Auto Sort", "Conflict Finder", "Game Log Helper", "Playset Share Codes"]
    f = font(mb.SAN, 24)
    widths = [d.textlength(c, font=f) + 44 * K for c in chips]
    gap = 16 * K
    x = (W * K - (sum(widths) + gap * (len(chips) - 1))) / 2
    y = 560 * K
    for c, w in zip(chips, widths):
        d.rounded_rectangle([x, y, x + w, y + 50 * K], radius=25 * K, outline=GOLD, width=2 * K, fill=(30, 26, 48))
        d.text((x + 22 * K, y + 10 * K), c, font=f, fill=GOLD_L)
        x += w + gap
    centered(d, 634 * K, "Version " + version + "  ·  Free and open source  ·  Windows 10 / 11", font(mb.SANR, 22), (150, 146, 168))
    im = im.convert("RGB").resize((W, H), Image.LANCZOS)
    if out.lower().endswith((".jpg", ".jpeg")): im.save(out, quality=88, optimize=True, progressive=True)
    else: im.save(out, optimize=True)

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "preview.jpg", sys.argv[2] if len(sys.argv) > 2 else "0.17.0")
