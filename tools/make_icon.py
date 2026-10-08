"""Builds src/royalcourt.ico: a golden crown above a throne on a midnight-indigo tile (matches the app's header crown).
Usage: python3 tools/make_icon.py [output.ico]   (needs Pillow)"""
import sys
from PIL import Image, ImageDraw

S = 1024
GOLD, GOLD_DARK, GOLD_LIGHT = (232, 185, 58), (150, 108, 18), (255, 224, 138)
CRIMSON, CRIMSON_DARK = (176, 32, 66), (110, 18, 42)

def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

def tile():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        gd.line([(0, y), (S, y)], fill=lerp((38, 33, 58), (18, 16, 28), y / S) + (255,))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
    im.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([14, 14, S - 15, S - 15], radius=int(S * 0.22) - 10, outline=GOLD_DARK + (255,), width=10)
    return im

def crown(d):
    pts = [(330, 440), (330, 262), (421, 340), (512, 225), (603, 340), (694, 262), (694, 440)]
    d.polygon(pts, fill=GOLD, outline=GOLD_DARK)
    d.line(pts + [pts[0]], fill=GOLD_DARK, width=14, joint="curve")
    for cx, cy, r in ((330, 250, 28), (512, 205, 34), (694, 250, 28)):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=GOLD, outline=GOLD_DARK, width=10)
    d.rectangle([330, 392, 694, 440], fill=GOLD_DARK)
    d.rectangle([338, 400, 686, 432], fill=GOLD)
    d.line([(352, 380), (352, 300)], fill=GOLD_LIGHT, width=12)
    d.line([(498, 300), (512, 260)], fill=GOLD_LIGHT, width=10)
    for cx, col in ((421, (60, 110, 220)), (512, CRIMSON), (603, (60, 110, 220))):
        d.ellipse([cx - 19, 416 - 19, cx + 19, 416 + 19], fill=col, outline=GOLD_DARK, width=6)

def throne(d):
    d.polygon([(356, 760), (356, 560), (512, 462), (668, 560), (668, 760)], fill=GOLD, outline=GOLD_DARK)
    d.polygon([(392, 760), (392, 578), (512, 504), (632, 578), (632, 760)], fill=CRIMSON)
    d.line([(392, 578), (512, 504), (632, 578)], fill=CRIMSON_DARK, width=10)
    for x0 in (292, 644):
        d.rounded_rectangle([x0, 650, x0 + 88, 706], radius=18, fill=GOLD, outline=GOLD_DARK, width=8)
        d.rectangle([x0 + 22, 706, x0 + 66, 770], fill=GOLD)
    d.rounded_rectangle([384, 712, 640, 762], radius=22, fill=CRIMSON, outline=CRIMSON_DARK, width=8)
    d.rounded_rectangle([340, 758, 684, 812], radius=14, fill=GOLD, outline=GOLD_DARK, width=8)
    d.rectangle([362, 812, 420, 892], fill=GOLD_DARK)
    d.rectangle([604, 812, 662, 892], fill=GOLD_DARK)
    d.rectangle([350, 880, 432, 910], fill=GOLD)
    d.rectangle([592, 880, 674, 910], fill=GOLD)

def main(out):
    big = tile()
    d = ImageDraw.Draw(big)
    throne(d)
    crown(d)
    base = big.resize((256, 256), Image.LANCZOS)
    base.save(out, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "src/royalcourt.ico")
