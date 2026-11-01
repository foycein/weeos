"""Draws the WeeOS logo as the icon of WeeOS.exe and weeossetup.exe: source/WeeOS/WeeOS.ico.

python -I tools/Release/MakeIcon.py

A tile in the calm white and blue of the Wii (the colors of the welcome screen, source/SystemMenu/WelcomeScreen.cpp) with "WeeOS" in
the blue of the Wii and the short line under it. The small sizes show only the "W", which stays readable. Needs Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

IconPath = Path(__file__).resolve().parents[2] / "source" / "WeeOS" / "WeeOS.ico"
FontPath = Path("C:/Windows/Fonts/seguisb.ttf")

IconSizes = [16, 24, 32, 48, 64, 128, 256]
LargestSmallSize = 32

# Every size is drawn four times larger and then made smaller, which gives smooth edges.
DrawingScale = 4

BackgroundTop = (255, 255, 255)
BackgroundBottom = (222, 236, 247)
FrameColor = (170, 200, 222)
WiiBlue = (52, 190, 237)

CornerRadiusPart = 0.2
FramePart = 0.03
MarginPart = 0.04


def Mix(firstColor, secondColor, shareOfSecond):
    return tuple(round(first + (second - first) * shareOfSecond) for first, second in zip(firstColor, secondColor))


def BackgroundColorAt(row, side):
    return Mix(BackgroundTop, BackgroundBottom, row / (side - 1))


def DrawBackground(side):
    background = Image.new("RGBA", (side, side))
    draw = ImageDraw.Draw(background)
    for row in range(side):
        draw.line([(0, row), (side, row)], fill=BackgroundColorAt(row, side) + (255,))
    return background


def DrawTile(side):
    margin = round(side * MarginPart)
    tileBox = [margin, margin, side - margin - 1, side - margin - 1]
    radius = round(side * CornerRadiusPart)

    shape = Image.new("L", (side, side), 0)
    ImageDraw.Draw(shape).rounded_rectangle(tileBox, radius=radius, fill=255)

    tile = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    tile.paste(DrawBackground(side), (0, 0), shape)
    frameWidth = max(1, round(side * FramePart))
    ImageDraw.Draw(tile).rounded_rectangle(tileBox, radius=radius, outline=FrameColor + (255,), width=frameWidth)
    return tile


def DrawCenteredText(tile, text, fontSize, middleY):
    draw = ImageDraw.Draw(tile)
    font = ImageFont.truetype(str(FontPath), fontSize)
    draw.text((tile.width / 2, middleY), text, font=font, fill=WiiBlue + (255,), anchor="mm")


def DrawLogoLine(tile, middleY):
    side = tile.width
    lineWidth = side * 0.42
    lineHeight = max(1, round(side * 0.02))
    left = (side - lineWidth) / 2
    background = BackgroundColorAt(round(middleY), side)
    draw = ImageDraw.Draw(tile)
    steps = round(lineWidth)
    for step in range(steps):
        distanceFromMiddle = abs(step - steps / 2) / (steps / 2)
        color = Mix(WiiBlue, background, distanceFromMiddle)
        x = left + step
        draw.line([(x, middleY), (x, middleY + lineHeight)], fill=color + (255,))


def DrawIcon(size):
    side = size * DrawingScale
    tile = DrawTile(side)
    if size <= LargestSmallSize:
        DrawCenteredText(tile, "W", round(side * 0.66), side * 0.47)
    else:
        DrawCenteredText(tile, "WeeOS", round(side * 0.3), side * 0.45)
        DrawLogoLine(tile, side * 0.64)
    return tile.resize((size, size), Image.LANCZOS)


def Main():
    pictures = [DrawIcon(size) for size in IconSizes]
    largest = pictures[-1]
    largest.save(IconPath, format="ICO", sizes=[(size, size) for size in IconSizes], append_images=pictures[:-1])
    print(f"Wrote {IconPath}")


Main()
