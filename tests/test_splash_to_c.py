#!/usr/bin/env python3
"""Verifie tools/splash_to_c.py sur une image synthetique (sans l'image du depot ni LVGL) : recadrage, RGB565, blanc exact,
tableau C produit. Lancer : python3 tests/test_splash_to_c.py (necessite Pillow)."""
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import splash_to_c as s  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

fails = 0


def check(what, ok, detail=""):
    global fails
    print("  %-66s %s %s" % (what, "OK" if ok else "ECHEC", detail))
    if not ok:
        fails += 1


def px(data, x, y):
    v = data[(y * s.W + x) * 2] | (data[(y * s.W + x) * 2 + 1] << 8)
    return (v >> 11) & 31, (v >> 5) & 63, v & 31


with tempfile.TemporaryDirectory() as tmp:
    im = Image.new("RGB", (1000, 1000), (255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([450, 0, 549, 999], fill=(255, 0, 0))
    d.rectangle([200, 0, 299, 99], fill=(0, 0, 0))
    src = os.path.join(tmp, "src.png")
    im.save(src)
    data = s.to_rgb565(s.load_fitted(src))
    check("taille : 320 x 480 x 2 octets", len(data) == s.W * s.H * 2, "(%d)" % len(data))
    check("blanc exactement blanc (31, 63, 31)", px(data, 5, 470) == (31, 63, 31), str(px(data, 5, 470)))
    check("noir exactement noir", px(data, 40, 10) == (0, 0, 0), str(px(data, 40, 10)))
    r, g, b = px(data, 160, 240)
    check("le centre garde la bande rouge", r == 31 and g == 0 and b == 0, str((r, g, b)))
    a = s.to_rgb565(s.load_fitted(src))
    check("conversion deterministe", a == data)
    g_im = Image.new("RGB", (s.W, s.H))
    gp = g_im.load()
    for y in range(s.H):
        for x in range(s.W):
            gp[x, y] = (100, 100, 100 + (y * 7) // s.H)
    dd = s.to_rgb565(g_im)
    levels = set(px(dd, x, y)[2] for x in range(8) for y in range(100, 104))
    check("tramage : deux niveaux voisins melanges dans un bloc de degrade", len(levels) >= 2, str(sorted(levels)))
    out = os.path.join(tmp, "splash_img.c")
    s.write_c(data, out)
    txt = open(out).read()
    check("tableau C : descripteur LVGL RGB565 320 x 480, stride 640",
          "LV_COLOR_FORMAT_RGB565" in txt and ".header.w = 320," in txt and ".header.h = 480," in txt and ".header.stride = 640," in txt)
    n = len(re.findall(r"0x[0-9a-f]{2}", txt))
    check("tableau C : 307200 octets", n == 307200, "(%d)" % n)
    check("tableau C : symbole ui_splash_img", "const lv_image_dsc_t ui_splash_img" in txt)

print("ECHECS" if fails else "TOUS LES TESTS REUSSIS")
sys.exit(1 if fails else 0)
