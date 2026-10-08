"""Builds the Nexus Mods header image (1920x500). Usage: python3 tools/make_banner.py out.png (needs Pillow)"""
import sys, importlib.util, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("mi", os.path.join(here, "make_icon.py")); mi = importlib.util.module_from_spec(spec); spec.loader.exec_module(mi)

W, H, K = 1920, 500, 2          # supersample factor K
GOLD, GOLD_D, GOLD_L = mi.GOLD, mi.GOLD_DARK, mi.GOLD_LIGHT
SER = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
SAN = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
SANR = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"

def font(p, s): return ImageFont.truetype(p, s * K)

def background():
    w, h = W * K, H * K
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for x in range(w):
        t = x / w
        d.line([(x, 0), (x, h)], fill=mi.lerp((44, 36, 70), (14, 12, 24), t))
    # soft golden glow behind the emblem
    glow = Image.new("RGB", (w, h), (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, r = 330 * K, 250 * K, 330 * K
    gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(120, 85, 20))
    glow = glow.filter(ImageFilter.GaussianBlur(120 * K))
    im = Image.blend(im, Image.eval(Image.composite(glow, im, Image.new("L", (w, h), 255)), lambda v: v), 0.0)
    from PIL import ImageChops
    im = ImageChops.add(im, glow.point(lambda v: int(v * 0.55)))
    d = ImageDraw.Draw(im)
    # faint diamond lattice
    step = 70 * K
    for i in range(-H * K, w + H * K, step):
        d.line([(i, 0), (i + h, h)], fill=(255, 255, 255), width=1)
        d.line([(i + h, 0), (i, h)], fill=(255, 255, 255), width=1)
    lat = im.copy()
    base = background_base(w, h)
    return Image.blend(base, lat, 0.0) if False else Image.blend(im_without_lattice(w, h), im, 0.06)

def im_without_lattice(w, h):
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for x in range(w):
        d.line([(x, 0), (x, h)], fill=mi.lerp((44, 36, 70), (14, 12, 24), x / w))
    glow = Image.new("RGB", (w, h), (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, r = 330 * K, 250 * K, 330 * K
    gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(120, 85, 20))
    glow = glow.filter(ImageFilter.GaussianBlur(120 * K))
    from PIL import ImageChops
    return ImageChops.add(im, glow.point(lambda v: int(v * 0.55)))

def background_base(w, h): return im_without_lattice(w, h)

def emblem(size):
    big = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    mi.throne(d); mi.crown(d)
    bb = big.getbbox()
    big = big.crop(bb)
    sc = size / big.height
    return big.resize((int(big.width * sc), size), Image.LANCZOS)

def spaced(d, xy, text, f, fill, sp):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + sp * K
    return x

def main(out):
    im = background().convert("RGBA")
    d = ImageDraw.Draw(im)
    # gold frame lines top and bottom
    for y, wd in ((0, 10), (H * K - 10 * K, 10)):
        d.rectangle([0, y, W * K, y + wd * K if y == 0 else H * K], fill=GOLD_D)
    d.rectangle([0, 10 * K, W * K, 14 * K], fill=GOLD)
    d.rectangle([0, H * K - 14 * K, W * K, H * K - 10 * K], fill=GOLD)
    # emblem
    em = emblem(360 * K)
    sh = Image.new("RGBA", im.size, (0, 0, 0, 0))
    sh.paste((0, 0, 0, 150), (int(190 * K) + 10 * K, int(70 * K) + 14 * K), em.split()[3])
    sh = sh.filter(ImageFilter.GaussianBlur(14 * K))
    im = Image.alpha_composite(im, sh)
    im.alpha_composite(em, (int(190 * K), int(70 * K)))
    d = ImageDraw.Draw(im)
    tx = 560
    # title
    ft = font(SER, 128)
    d.text((tx * K + 4 * K, 110 * K + 5 * K), "The Royal Court", font=ft, fill=(0, 0, 0, 140))
    d.text((tx * K, 110 * K), "The Royal Court", font=ft, fill=(246, 240, 226))
    tw = d.textlength("The Royal Court", font=ft)
    # gold rule with diamond
    ry = 262 * K
    d.rectangle([tx * K, ry, tx * K + tw, ry + 4 * K], fill=GOLD)
    dm = 12 * K; cxm = tx * K + tw / 2
    d.polygon([(cxm - dm, ry + 2 * K), (cxm, ry - dm + 2 * K), (cxm + dm, ry + 2 * K), (cxm, ry + dm + 2 * K)], fill=GOLD_L, outline=GOLD_D)
    # subtitle
    spaced(d, (tx * K + 4 * K, 285 * K), "CRUSADER KINGS III  MOD MANAGER", font(SAN, 40), GOLD, 9)
    # tagline
    d.text((tx * K + 4 * K, 352 * K), "Fast. Lightweight. Native. Rule your load order.", font=font(SANR, 36), fill=(200, 196, 214))
    # feature chips
    chips = ["Playsets", "Conflict Finder", "Game Version Check", "Dark & Light Themes"]
    f = font(SAN, 26); x = tx * K + 4 * K; y = 412 * K
    for c in chips:
        w = d.textlength(c, font=f) + 44 * K
        d.rounded_rectangle([x, y, x + w, y + 52 * K], radius=26 * K, outline=GOLD, width=2 * K, fill=(30, 26, 48))
        d.text((x + 22 * K, y + 11 * K), c, font=f, fill=GOLD_L)
        x += w + 18 * K
    im = im.convert("RGB").resize((W, H), Image.LANCZOS)
    im.save(out, optimize=True)

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "banner.png")
