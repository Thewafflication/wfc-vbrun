#!/usr/bin/env python3
"""Renders a ctest session as a result card in the WFC house style.

Usage: render_card.py <ctest output> <output.png> <title> <subtitle>

The card is the look of the WFC games (tests/vb6games/ui/modTheme.bas of
WFC): a navy background, a raised panel with a thin light edge and a
two-step soft shadow, a teal accent, amber for numbers and Segoe UI text.
Every line it shows is a result line that ctest printed:

    12/44 Test #12: TC-0013-x87 ........   Passed    0.03 sec

The picture is drawn at twice its size so that it stays sharp when a page
scales it down.
"""
import re
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

BACK = (0x11, 0x14, 0x1F)
PANEL = (0x1B, 0x20, 0x33)
PANEL2 = (0x25, 0x2B, 0x45)
EDGE = (0x40, 0x4A, 0x6B)
TEXT = (0xE8, 0xEB, 0xF5)
MUTED = (0x98, 0xA0, 0xBE)
TEAL = (0x3D, 0xD6, 0xC6)
AMBER = (0xFF, 0xB4, 0x54)
BAD = (0xFF, 0x6B, 0x81)
SHADOW_NEAR = (0x08, 0x0A, 0x0F)
SHADOW_FAR = (0x0D, 0x10, 0x17)

DEJAVU = "/usr/share/fonts/truetype/dejavu/"
SANS = ("C:/Windows/Fonts/segoeui.ttf", DEJAVU + "DejaVuSans.ttf")
SANS_SEMI = ("C:/Windows/Fonts/seguisb.ttf", DEJAVU + "DejaVuSans-Bold.ttf")
MONO = (
    "C:/Windows/Fonts/CascadiaMono.ttf",
    "C:/Windows/Fonts/consola.ttf",
    DEJAVU + "DejaVuSansMono.ttf",
)

SCALE = 2
RESULT = re.compile(
    r"^\s*(\d+)/(\d+) Test\s+#\d+:\s+(\S+)\s+\.+\s*"
    r"(\S+(?: \S+)?)\s+([\d.]+) sec"
)
SUMMARY = re.compile(r"(\d+)% tests passed")
TOTAL = re.compile(r"Total Test time \(real\) =\s+([\d.]+) sec")


def font(paths, size):
    for path in paths:
        try:
            return ImageFont.truetype(path, size * SCALE)
        except OSError:
            continue
    return ImageFont.load_default()


def px(value):
    return int(round(value * SCALE))


def main():
    text_path, output, title, subtitle = sys.argv[1:5]
    results = []
    percent = None
    total = None
    with open(text_path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            found = RESULT.match(line)
            if found:
                results.append(
                    (found.group(3), found.group(4), found.group(5))
                )
            found = SUMMARY.search(line)
            if found:
                percent = found.group(1)
            found = TOTAL.search(line)
            if found:
                total = found.group(1)
    if not results:
        sys.exit("no ctest result lines in " + text_path)
    passed = sum(1 for _, state, _ in results if state == "Passed")

    columns = 2
    rows = (len(results) + columns - 1) // columns
    margin, pad = 36, 28
    card_w = 1000
    head_h = 92
    row_h = 30
    foot_h = 64
    card_h = head_h + pad + rows * row_h + foot_h
    width = card_w + margin * 2
    height = card_h + margin * 2 + 12

    # Background and the two-step soft shadow.
    image = Image.new("RGB", (px(width), px(height)), BACK)
    for blur, drop, colour in ((26, 14, SHADOW_FAR), (9, 5, SHADOW_NEAR)):
        layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).rounded_rectangle(
            (px(margin), px(margin + drop), px(margin + card_w),
             px(margin + card_h + drop)),
            radius=px(18), fill=colour + (255,))
        layer = layer.filter(ImageFilter.GaussianBlur(px(blur) / 2))
        image.paste(layer, (0, 0), layer)
    draw = ImageDraw.Draw(image)
    x0, y0 = margin, margin
    draw.rounded_rectangle(
        (px(x0), px(y0), px(x0 + card_w), px(y0 + card_h)),
        radius=px(18), fill=PANEL, outline=EDGE, width=SCALE)

    title_font = font(SANS_SEMI, 22)
    sub_font = font(SANS, 14)
    name_font = font(MONO, 14)
    small_font = font(MONO, 13)
    foot_font = font(SANS_SEMI, 15)

    # Header: an accent bar, the title and the subtitle.
    draw.rounded_rectangle(
        (px(x0 + pad), px(y0 + 26), px(x0 + pad + 5), px(y0 + 66)),
        radius=px(2), fill=TEAL)
    draw.text((px(x0 + pad + 20), px(y0 + 20)), title, font=title_font,
              fill=TEXT)
    draw.text((px(x0 + pad + 20), px(y0 + 54)), subtitle, font=sub_font,
              fill=MUTED)
    # The count, right-aligned, in amber.
    count = "%d / %d passed" % (passed, len(results))
    count_w = draw.textlength(count, font=title_font)
    draw.text((px(x0 + card_w - pad) - count_w, px(y0 + 30)), count,
              font=title_font, fill=AMBER if passed < len(results) else TEAL)
    draw.line((px(x0 + 1), px(y0 + head_h), px(x0 + card_w - 1),
               px(y0 + head_h)), fill=EDGE, width=SCALE)

    # The results, in columns, in the order ctest ran them.
    col_w = (card_w - pad * 2) // columns
    top = y0 + head_h + pad // 2 + 4
    for index, (name, state, seconds) in enumerate(results):
        col, row = divmod(index, rows)
        x = x0 + pad + col * col_w
        y = top + row * row_h
        if row % 2 == 0:
            draw.rounded_rectangle(
                (px(x - 8), px(y - 3), px(x + col_w - 10), px(y + row_h - 5)),
                radius=px(6), fill=PANEL2)
        ok = state == "Passed"
        colour = TEAL if ok else BAD
        # A teal dot for a pass, a coral one for a failure.
        draw.ellipse((px(x + 2), px(y + 6), px(x + 10), px(y + 14)),
                     fill=colour)
        number, _, short = name.partition("-")[2].partition("-")
        draw.text((px(x + 22), px(y)), "TC-" + number, font=small_font,
                  fill=MUTED)
        draw.text((px(x + 82), px(y)), short, font=name_font,
                  fill=TEXT if ok else BAD)
        stamp = "%.2f s" % float(seconds)
        stamp_w = draw.textlength(stamp, font=small_font)
        draw.text((px(x + col_w - 26) - stamp_w, px(y)), stamp,
                  font=small_font, fill=MUTED)

    # Footer: the summary ctest printed.
    fy = y0 + card_h - foot_h
    draw.line((px(x0 + 1), px(fy), px(x0 + card_w - 1), px(fy)), fill=EDGE,
              width=SCALE)
    summary = "%d passed" % passed
    if percent:
        summary = "%s%% tests passed" % percent
    draw.text((px(x0 + pad), px(fy + 20)), summary, font=foot_font,
              fill=TEAL if passed == len(results) else AMBER)
    if total:
        label = "total test time %s s" % total
        label_w = draw.textlength(label, font=sub_font)
        draw.text((px(x0 + card_w - pad) - label_w, px(fy + 22)), label,
                  font=sub_font, fill=MUTED)
    image.save(output)


if __name__ == "__main__":
    main()
