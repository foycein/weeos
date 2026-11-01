"""Draws the WeeOS logo and writes it as the icon of the program: source/WeeOS/WeeOS.ico.

A white tile with rounded corners, a ring in the blue of the Wii, and a W of the same blue inside the ring. The logo is drawn four
times larger than the largest icon and made smaller, so that its edges are smooth at every size.

Run it with a Python that has Pillow: python -I tools/MakeProgramIcon.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

WiiBlue = (52, 190, 237, 255)
TileWhite = (255, 255, 255, 255)
TileEdgeGray = (200, 205, 210, 255)
Transparent = (0, 0, 0, 0)

LargestIconSize = 256
IconSizes = [16, 24, 32, 48, 64, 128, 256]
DrawingScale = 4

# Shares of the side of the picture.
TileCornerRadius = 0.22
TileEdgeWidth = 0.02
RingRadius = 0.36
RingWidth = 0.055
LetterStrokeWidth = 0.075

# The corners of the W, as shares of the side: down, up to the middle, down, and up again.
LetterCorners = [(0.31, 0.37), (0.40, 0.64), (0.50, 0.43), (0.60, 0.64), (0.69, 0.37)]

IconFile = Path(__file__).resolve().parent.parent / "source" / "WeeOS" / "WeeOS.ico"


def DrawTile(drawing, side):
    edgeWidth = round(side * TileEdgeWidth)
    radius = round(side * TileCornerRadius)
    drawing.rounded_rectangle([0, 0, side - 1, side - 1], radius=radius, fill=TileWhite, outline=TileEdgeGray, width=edgeWidth)


def DrawRing(drawing, side):
    middle = side / 2
    radius = side * RingRadius
    drawing.ellipse([middle - radius, middle - radius, middle + radius, middle + radius], outline=WiiBlue, width=round(side * RingWidth))


# A line with round ends and round corners: every corner gets a dot as wide as the line.
def DrawLetter(drawing, side):
    strokeWidth = round(side * LetterStrokeWidth)
    corners = [(x * side, y * side) for x, y in LetterCorners]
    drawing.line(corners, fill=WiiBlue, width=strokeWidth, joint="curve")

    dotRadius = strokeWidth / 2
    for x, y in corners:
        drawing.ellipse([x - dotRadius, y - dotRadius, x + dotRadius, y + dotRadius], fill=WiiBlue)


def DrawLogo():
    side = LargestIconSize * DrawingScale
    logo = Image.new("RGBA", (side, side), Transparent)
    drawing = ImageDraw.Draw(logo)
    DrawTile(drawing, side)
    DrawRing(drawing, side)
    DrawLetter(drawing, side)
    return logo.resize((LargestIconSize, LargestIconSize), Image.LANCZOS)


def Main():
    logo = DrawLogo()
    logo.save(IconFile, format="ICO", sizes=[(size, size) for size in IconSizes])
    print(f"Wrote {IconFile}")


if __name__ == "__main__":
    Main()
