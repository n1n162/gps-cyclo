#!/usr/bin/env python3
"""Convertit l'image de l'ecran de demarrage en tableau C pour LVGL 9 (RGB565, 320 x 480).

Usage :
    python3 tools/splash_to_c.py "docs/ecran acceuil.jpg" ui/splash_img.c [--no-dither] [--preview apercu.png]

L'image est recadree au format de l'ecran (sans deformation), puis convertie en RGB565 avec un tramage ordonne
(les degrades du ciel restent sans bandes). Le blanc reste exactement blanc. Le fichier produit declare
`const lv_image_dsc_t ui_splash_img`, place en memoire flash (const). Il fait environ 1,8 Mo en texte pour
300 Ko dans la puce : il n'est pas publie dans le depot, les scripts de compilation le regenerent.
Necessite Pillow : pip3 install pillow
"""
import sys

W, H = 320, 480
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))


def to_rgb565(img, dither=True):
    """img : image Pillow RGB de W x H. Retourne W*H*2 octets, RGB565 petit-boutiste (format natif de LVGL)."""
    px = img.load()
    out = bytearray(W * H * 2)
    i = 0
    for y in range(H):
        row = BAYER[y & 3]
        for x in range(W):
            r, g, b = px[x, y]
            t = (row[x & 3] + 0.5) / 16.0 if dither else 0.5
            r5 = min(31, int(r * 31 / 255 + t))
            g6 = min(63, int(g * 63 / 255 + t))
            b5 = min(31, int(b * 31 / 255 + t))
            v = (r5 << 11) | (g6 << 5) | b5
            out[i] = v & 0xFF
            out[i + 1] = v >> 8
            i += 2
    return bytes(out)


def load_fitted(path):
    from PIL import Image, ImageOps
    img = Image.open(path).convert("RGB")
    return ImageOps.fit(img, (W, H), method=Image.LANCZOS, centering=(0.5, 0.5))


def to_png(data, path):
    """Relit les octets RGB565 et ecrit un PNG : permet de voir ce que l'ecran affichera."""
    from PIL import Image
    im = Image.new("RGB", (W, H))
    px = im.load()
    for y in range(H):
        for x in range(W):
            v = data[(y * W + x) * 2] | (data[(y * W + x) * 2 + 1] << 8)
            r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
            px[x, y] = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
    im.save(path)


def write_c(data, path):
    lines = [
        "/* Genere par tools/splash_to_c.py a partir de docs/ecran acceuil.jpg : ne pas modifier a la main. */",
        '#include "lvgl.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
        "static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t ui_splash_map[] = {",
    ]
    for k in range(0, len(data), 32):
        lines.append("  " + ",".join("0x%02x" % c for c in data[k:k + 32]) + ",")
    lines += [
        "};",
        "",
        "const lv_image_dsc_t ui_splash_img = {",
        "  .header.magic = LV_IMAGE_HEADER_MAGIC,",
        "  .header.cf = LV_COLOR_FORMAT_RGB565,",
        "  .header.flags = 0,",
        "  .header.w = %d," % W,
        "  .header.h = %d," % H,
        "  .header.stride = %d," % (W * 2),
        "  .data_size = sizeof(ui_splash_map),",
        "  .data = ui_splash_map,",
        "};",
        "",
    ]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    dither = "--no-dither" not in argv
    preview = None
    if "--preview" in argv:
        k = argv.index("--preview")
        if k + 1 >= len(argv):
            print(__doc__)
            return 2
        preview = argv[k + 1]
        args = [a for a in args if a != preview]
    if len(args) != 2:
        print(__doc__)
        return 2
    try:
        import PIL  # noqa: F401
    except ImportError:
        sys.stderr.write("Pillow est absent. Installer : pip3 install pillow\n")
        return 3
    data = to_rgb565(load_fitted(args[0]), dither)
    write_c(data, args[1])
    if preview:
        to_png(data, preview)
    print("%s : %d x %d, %d octets (RGB565)" % (args[1], W, H, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
