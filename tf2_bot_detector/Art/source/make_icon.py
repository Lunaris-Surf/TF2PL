# Generates the TF2PL app icon (TF2PL.ico for Windows, PNGs for Linux/AppImage).
# Original artwork: LunarisV glass hexagon + italic "PL" monogram (teal P, orange L, like the TF2PL wordmark),
# set in the app's own Chakra Petch SemiBold Italic. Sizes <= 32px use a simplified, bolder variant.
#
# usage (needs Pillow):
#   python make_icon.py ../../../staging/fonts/ChakraPetch-SemiBoldItalic.ttf <out dir>
# then copy <out dir>/TF2PL.ico -> tf2_bot_detector/Art/TF2PL.ico
#        and <out dir>/tf2pl_icon_256.png -> packaging/linux/TF2PL.png
import math, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

FONT = sys.argv[1]       # ChakraPetch-SemiBoldItalic.ttf
OUT = sys.argv[2]
N = 1024                 # master canvas

TEAL = (63, 230, 216); TEAL_HI = (185, 255, 247); TEAL_DEEP = (15, 109, 115)
ORANGE = (255, 154, 31); ORANGE_HI = (255, 207, 122)
VOID = (2, 16, 21); DEEP = (4, 25, 31)

def hexagon(cx, cy, r, rot=90):
    return [(cx + r * math.cos(math.radians(rot + 60 * i)), cy + r * math.sin(math.radians(rot + 60 * i))) for i in range(6)]

def vgrad(size, top, bottom):
    g = Image.new("RGB", (1, size[1]))
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        g.putpixel((0, y), tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return g.resize(size)

def render(simple=False):
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    c = N / 2
    R = N * 0.47

    # outer glow
    glow = Image.new("L", (N, N), 0)
    ImageDraw.Draw(glow).polygon(hexagon(c, c, R), fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(N * 0.02))
    img.paste(Image.new("RGBA", (N, N), TEAL + (110,)), (0, 0), glow)

    # rim
    rim = Image.new("L", (N, N), 0)
    ImageDraw.Draw(rim).polygon(hexagon(c, c, R), fill=255)
    img.paste(vgrad((N, N), TEAL_HI, TEAL_DEEP).convert("RGBA"), (0, 0), rim)

    # glass body
    body_r = R * (0.86 if simple else 0.90)
    body = Image.new("L", (N, N), 0)
    ImageDraw.Draw(body).polygon(hexagon(c, c, body_r), fill=255)
    img.paste(vgrad((N, N), (10, 52, 60), VOID).convert("RGBA"), (0, 0), body)

    if not simple:
        d = ImageDraw.Draw(img)
        # scanlines
        lines = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        ld = ImageDraw.Draw(lines)
        for y in range(0, N, 14):
            ld.line([(0, y), (N, y)], fill=TEAL + (14,), width=3)
        img.paste(lines, (0, 0), ImageChops.multiply(lines.split()[3], body))
        # orange HUD slashes (from the masthead)
        sl = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        sd = ImageDraw.Draw(sl)
        for i in range(4):
            x0 = c - R * 0.40 + i * N * 0.045
            y0 = c + R * 0.60
            sd.polygon([(x0, y0), (x0 + N * 0.028, y0), (x0 + N * 0.010, y0 + N * 0.040), (x0 - N * 0.018, y0 + N * 0.040)],
                       fill=ORANGE + (200 - i * 40,))
        img.paste(sl, (0, 0), ImageChops.multiply(sl.split()[3], body))
        # inner bevel line
        d.line(hexagon(c, c, body_r * 0.965) + [hexagon(c, c, body_r * 0.965)[0]], fill=TEAL + (70,), width=int(N * 0.006))

    # PL monogram
    size = int(N * (0.60 if simple else 0.54))
    font = ImageFont.truetype(FONT, size)
    txt = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    td = ImageDraw.Draw(txt)
    p_w = td.textlength("P", font=font)
    l_w = td.textlength("L", font=font)
    kern = -size * 0.05
    total = p_w + l_w + kern
    bbox = td.textbbox((0, 0), "PL", font=font)
    x = c - total / 2 - size * 0.02
    y = c - (bbox[1] + bbox[3]) / 2 - (N * 0.03 if not simple else 0)

    # letter glow
    tg = Image.new("L", (N, N), 0)
    tgd = ImageDraw.Draw(tg)
    tgd.text((x, y), "P", font=font, fill=255)
    tgd.text((x + p_w + kern, y), "L", font=font, fill=255)
    tg = tg.filter(ImageFilter.GaussianBlur(N * 0.018))
    img.paste(Image.new("RGBA", (N, N), (255, 255, 255, 90)), (0, 0), tg)

    pm = Image.new("L", (N, N), 0); ImageDraw.Draw(pm).text((x, y), "P", font=font, fill=255)
    lm = Image.new("L", (N, N), 0); ImageDraw.Draw(lm).text((x + p_w + kern, y), "L", font=font, fill=255)
    img.paste(vgrad((N, N), TEAL_HI, TEAL).convert("RGBA"), (0, 0), pm)
    img.paste(vgrad((N, N), ORANGE_HI, ORANGE).convert("RGBA"), (0, 0), lm)

    if not simple:
        # underline bar like the wordmark
        d = ImageDraw.Draw(img)
        uy = y + bbox[3] + N * 0.035
        d.rectangle([c - total * 0.52, uy, c + total * 0.40, uy + N * 0.016], fill=ORANGE)
    return img

master = render(simple=False)
small_master = render(simple=True)
master.save(OUT + "/tf2pl_icon_1024.png")

sizes = [256, 128, 64, 48, 32, 24, 16]
frames = []
for s in sizes:
    src = small_master if s <= 32 else master
    frames.append(src.resize((s, s), Image.LANCZOS))
for f, s in zip(frames, sizes):
    f.save(f"{OUT}/tf2pl_icon_{s}.png")
frames[0].save(OUT + "/TF2PL.ico", sizes=[(s, s) for s in sizes], append_images=frames[1:])

# preview sheet on dark and light backgrounds
sheet = Image.new("RGBA", (900, 380), (30, 30, 30, 255))
light = Image.new("RGBA", (900, 120), (235, 235, 235, 255))
sheet.paste(light, (0, 260))
x = 10
for f, s in zip(frames, sizes):
    sheet.alpha_composite(f, (x, 10)); sheet.alpha_composite(f, (x, 270 if s <= 64 else 270))
    x += s + 16
sheet.save(OUT + "/preview.png")
print("ok")
