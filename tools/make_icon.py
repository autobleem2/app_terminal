#!/usr/bin/env python3
# Draws resources/icon.png, the Terminal App's picture in the launcher's Apps set: a terminal window - a dark
# screen in a light frame with a title bar - with a green prompt and a cursor. Generated, so nothing
# proprietary ships (the launcher's rule for payload artwork). Needs Pillow.
import os

from PIL import Image, ImageDraw

W, H = 256, 219
S = 4  # drawn 4x and scaled down, for smooth edges


def main():
    im = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    def r(x0, y0, x1, y1, radius, fill, outline=None, width=0):
        d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=radius * S, fill=fill, outline=outline,
                            width=width * S)

    # the window: frame, title bar, screen
    r(8, 14, 248, 205, 16, (214, 220, 228, 255))
    r(8, 14, 248, 46, 16, (150, 160, 176, 255))
    d.rectangle([8 * S, 32 * S, 248 * S, 46 * S], fill=(150, 160, 176, 255))
    for i, color in enumerate([(236, 95, 86), (244, 190, 80), (98, 196, 98)]):
        cx, cy = 28 + i * 20, 30
        d.ellipse([(cx - 6) * S, (cy - 6) * S, (cx + 6) * S, (cy + 6) * S], fill=color + (255,))
    r(18, 54, 238, 195, 8, (14, 17, 22, 255))

    # the prompt ">_" in green, and two lines of "output"
    green = (80, 220, 110, 255)
    w = 10
    d.line([(38 * S, 82 * S), (62 * S, 100 * S), (38 * S, 118 * S)], fill=green, width=w * S, joint="curve")
    d.rectangle([74 * S, 110 * S, 112 * S, 120 * S], fill=green)
    grey = (120, 132, 150, 255)
    d.rectangle([38 * S, 140 * S, 190 * S, 148 * S], fill=grey)
    d.rectangle([38 * S, 160 * S, 150 * S, 168 * S], fill=grey)
    d.rectangle([38 * S, 176 * S, 52 * S, 186 * S], fill=(230, 230, 230, 255))  # the cursor

    im = im.resize((W, H), Image.LANCZOS)
    out = os.path.join(os.path.dirname(__file__), "..", "resources", "icon.png")
    im.save(out)
    print("wrote", os.path.normpath(out))


if __name__ == "__main__":
    main()
