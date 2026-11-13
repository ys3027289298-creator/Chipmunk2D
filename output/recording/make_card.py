#!/usr/bin/env python3
# Render a dark title card PNG (drawtext is unavailable in the static ffmpeg).
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 1280, 720
BG = (16, 19, 26)
ACCENT = (86, 217, 255)
WHITE = (235, 235, 235)


def main(out, line1, line2=""):
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)
    font_big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 44)
    font_small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 24)

    def centered(text, font, y, color):
        box = draw.textbbox((0, 0), text, font=font)
        width = box[2] - box[0]
        draw.text(((W - width) / 2, y), text, font=font, fill=color)

    centered(line1, font_big, H // 2 - 70, ACCENT)
    if line2:
        centered(line2, font_small, H // 2 + 20, WHITE)
    img.save(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "")
