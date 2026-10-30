#!/usr/bin/env python3
"""Renders a text file as a terminal-style PNG (CI uses it for screenshots).

Usage: render_terminal.py <text file> <output.png> <window title>

This file is identical in every WFC library (wfc-vbrun, wfc-mscomctl,
wfc-comdlg32, wfc-mswinsck); a WFC test fails when the copies differ.
"""
import sys

from PIL import Image, ImageDraw, ImageFont

FONTS = (
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "C:/Windows/Fonts/consola.ttf",
)
MAX_LINES = 46
MAX_COLUMNS = 110


def load_font(size):
    for path in FONTS:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def main():
    text_path, output, title = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(text_path, encoding="utf-8", errors="replace") as handle:
        lines = [line.rstrip("\n").expandtabs(4) for line in handle]
    if len(lines) > MAX_LINES:
        lines = ["..."] + lines[-(MAX_LINES - 1):]
    lines = [line[:MAX_COLUMNS] for line in lines] or [""]
    font = load_font(15)
    left, top, bar = 16, 44, 32
    cell = font.getbbox("M")
    line_height = cell[3] - cell[1] + 6
    columns = min(MAX_COLUMNS, max(70, max(len(line) for line in lines)))
    width = left * 2 + int(font.getlength("M" * columns))
    height = top + bar + line_height * len(lines)
    image = Image.new("RGB", (width, height), (30, 30, 36))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, width, bar), fill=(54, 54, 64))
    for index, colour in enumerate(((237, 106, 94), (245, 191, 79), (97, 197, 84))):
        draw.ellipse((14 + index * 22, 9, 28 + index * 22, 23), fill=colour)
    draw.text((width // 2 - len(title) * 4, 8), title, font=font,
              fill=(210, 210, 220))
    y = bar + 14
    for line in lines:
        colour = (230, 230, 235)
        if line.startswith("$ "):
            colour = (130, 220, 150)
        elif "Passed" in line or "tests passed" in line:
            colour = (150, 210, 255)
        elif "Failed" in line or "***" in line:
            colour = (255, 130, 120)
        draw.text((left, y), line, font=font, fill=colour)
        y += line_height
    image.save(output)


if __name__ == "__main__":
    main()
