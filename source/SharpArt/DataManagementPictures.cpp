#include "SharpArt/DataManagementPictures.h"

#include "SharpArt/SmallWords.h"
#include "SharpArt/WiiLogoPictures.h"
#include "SharpArt/WiiOptionsPictures.h"

#include <array>
#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color Gray(Formats::Byte level)
{
    return {level, level, level, 255};
}

// Each drawing is written in the places of its largest picture; a placement moves and scales it to where a smaller picture shows it.
struct Placement
{
    Point offset = {0, 0};
    float scale = 1;

    Point Place(Point point) const
    {
        return {offset.x + point.x * scale, offset.y + point.y * scale};
    }

    float Size(float size) const
    {
        return size * scale;
    }
};

void FillPolygon(VectorCanvas& canvas, const Placement& placement, const std::vector<Point>& corners, Color color)
{
    VectorPath polygon;
    for (Point corner : corners)
        polygon.LineTo(placement.Place(corner));
    polygon.Close();
    canvas.Fill(polygon, SolidPaint(color));
}

VectorPath RoundedPolygon(const Placement& placement, const std::vector<Point>& corners, float cornerCut)
{
    std::vector<Point> placedCorners;
    for (Point corner : corners)
        placedCorners.push_back(placement.Place(corner));

    VectorPath polygon;
    polygon.AddPolygonWithRoundCorners(placedCorners, placement.Size(cornerCut));
    return polygon;
}

void FillRoundedPolygon(VectorCanvas& canvas, const Placement& placement, const std::vector<Point>& corners, float cornerCut, Color color)
{
    canvas.Fill(RoundedPolygon(placement, corners, cornerCut), SolidPaint(color));
}

void FillRectangle(VectorCanvas& canvas, const Placement& placement, float left, float top, float right, float bottom, Color color)
{
    FillPolygon(canvas, placement, {{left, top}, {right, top}, {right, bottom}, {left, bottom}}, color);
}

void FillRoundedRectangle(VectorCanvas& canvas, const Placement& placement, float left, float top, float right, float bottom, float radius,
    Color color)
{
    Point topLeft = placement.Place({left, top});
    Point bottomRight = placement.Place({right, bottom});
    VectorPath rectangle;
    rectangle.AddRoundedRectangle(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y, placement.Size(radius));
    canvas.Fill(rectangle, SolidPaint(color));
}

// Lines through the points with a round pen; each line is filled on its own, so the round ends make round corners.
void DrawLines(VectorCanvas& canvas, const Placement& placement, const std::vector<Point>& points, float penWidth, Color color,
    bool isClosed)
{
    std::size_t lineCount = isClosed ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < lineCount; ++index)
    {
        VectorPath line;
        line.AddStroke(placement.Place(points[index]), placement.Place(points[(index + 1) % points.size()]), placement.Size(penWidth));
        canvas.Fill(line, SolidPaint(color));
    }
}

// ---- The console, standing a little turned: its narrow left side with the slot, its front, and the foot below both.
// The places are those of the Wii button of Save Data.

constexpr Point ConsoleTopLeft = {69.5f, 34.8f};
constexpr Point ConsoleTopRight = {134.5f, 54.2f};
constexpr Point ConsoleBottomRight = {131.3f, 111.3f};
constexpr Point ConsoleBottomLeft = {58, 118.8f};
constexpr Point FrontTopLeft = {78.5f, 37.5f};
constexpr Point FrontBottomLeft = {72, 99.4f};
constexpr Point SideBottomLeft = {60.6f, 100.2f};
constexpr Point FrontBottomRight = {132.2f, 96};
constexpr Point FootBottomOfTheEdge = {70, 117.6f};
constexpr float ConsoleOutlineWidth = 2.3f;
constexpr Color ConsoleOutline = Gray(135);
constexpr Color ConsoleSide = Gray(214);
constexpr Color ConsoleFront = Gray(231);
constexpr Color ConsoleFoot = Gray(198);
constexpr Color ConsoleFootSide = Gray(148);
constexpr Color ConsoleSlot = Gray(165);
constexpr Color ConsoleFootLine = Gray(189);

void DrawConsole(VectorCanvas& canvas, const Placement& placement)
{
    constexpr float OuterCornerCut = 4;
    constexpr float FaceCornerCut = 2;
    std::vector<Point> outline = {ConsoleTopLeft, ConsoleTopRight, ConsoleBottomRight, ConsoleBottomLeft};

    FillRoundedPolygon(canvas, placement, outline, OuterCornerCut, ConsoleSide);
    FillRoundedPolygon(canvas, placement, {FrontTopLeft, ConsoleTopRight, FrontBottomRight, FrontBottomLeft}, FaceCornerCut, ConsoleFront);
    FillRoundedPolygon(canvas, placement, {FrontBottomLeft, FrontBottomRight, ConsoleBottomRight, FootBottomOfTheEdge}, FaceCornerCut,
        ConsoleFoot);
    FillRoundedPolygon(canvas, placement, {SideBottomLeft, FrontBottomLeft, FootBottomOfTheEdge, ConsoleBottomLeft}, FaceCornerCut,
        ConsoleFootSide);

    DrawLines(canvas, placement, {{74, 40.5f}, {67.6f, 95.5f}}, 1.4f, ConsoleSlot, false);
    DrawLines(canvas, placement, {FrontBottomLeft, FrontBottomRight}, 1, ConsoleFootLine, false);

    constexpr Placement AlreadyPlaced = {{0, 0}, 1};
    VectorPath roundShape = RoundedPolygon(placement, outline, OuterCornerCut);
    for (const Outline& roundOutline : roundShape.Outlines())
        DrawLines(canvas, AlreadyPlaced, roundOutline, placement.Size(ConsoleOutlineWidth), ConsoleOutline, true);
}

// ---- The SD card: a gray card with a cut corner, a white label with the SD mark and darker bands at the bottom.
// The places are those of the Wii button of Save Data.

constexpr Color CardOutline = Gray(120);
constexpr Color CardLightEdge = Gray(198);
constexpr Color CardTopBand = Gray(189);
constexpr Color CardLabel = Gray(255);
constexpr Color CardMark = Gray(196);
constexpr Color CardLowerLine = Gray(206);
constexpr Color CardLowerBand = Gray(173);

void DrawSdCard(VectorCanvas& canvas, const Placement& placement)
{
    FillPolygon(canvas, placement, {{164, 66}, {193.5f, 66}, {199.5f, 72}, {199.5f, 111.5f}, {164, 111.5f}}, CardOutline);
    FillPolygon(canvas, placement, {{166, 68}, {192.7f, 68}, {197.5f, 72.8f}, {197.5f, 109.5f}, {166, 109.5f}}, CardLightEdge);
    FillRectangle(canvas, placement, 167.5f, 68, 193, 71, CardTopBand);
    FillRectangle(canvas, placement, 167.5f, 71, 196, 100, CardLabel);
    FillRectangle(canvas, placement, 167.5f, 100, 196, 101, CardLowerLine);
    FillRectangle(canvas, placement, 167.5f, 101, 196, 107, CardLowerBand);
    FillRectangle(canvas, placement, 169, 75, 170, 77.5f, CardMark);

    FillPolygon(canvas, placement, {{172, 84}, {182, 84}, {181.3f, 86.2f}, {171.3f, 86.2f}}, CardMark);
    DrawLines(canvas, placement, {{172.2f, 86}, {179.4f, 90.2f}}, 2.2f, CardMark, false);
    FillPolygon(canvas, placement, {{168.8f, 89.6f}, {178.8f, 89.6f}, {178.2f, 92}, {168.2f, 92}}, CardMark);
    FillPolygon(canvas, placement, {{181.5f, 84}, {188.8f, 84}, {193.2f, 86.5f}, {192.3f, 89.7f}, {187.5f, 92}, {178.6f, 92}}, CardMark);
}

// ---- The memory card: a gray card with a label (a white field over a dark band), a small arrow above it and its words below it.
// The places are those of the button of Save Data.

constexpr Color MemoryCardOutline = Gray(125);
constexpr Color MemoryCardBody = Gray(210);
constexpr Color MemoryCardLabelEdge = Gray(132);
constexpr Color MemoryCardLabelTop = Gray(189);
constexpr Color MemoryCardLabelBand = Gray(123);
constexpr Color MemoryCardDots = Gray(200);
constexpr Color MemoryCardMakerWord = Gray(200);
constexpr Color MemoryCardNameWord = Gray(160);

void DrawMemoryCard(VectorCanvas& canvas, const Placement& placement)
{
    FillRoundedRectangle(canvas, placement, 125, 62, 170.5f, 111.5f, 2, MemoryCardOutline);
    FillRoundedRectangle(canvas, placement, 127, 64, 168.5f, 109.5f, 1, MemoryCardBody);
    FillPolygon(canvas, placement, {{146.5f, 68.2f}, {148.5f, 65}, {150.5f, 68.2f}}, MemoryCardLabelEdge);

    FillRectangle(canvas, placement, 128.5f, 69.5f, 164.5f, 96.5f, MemoryCardLabelEdge);
    FillRectangle(canvas, placement, 130, 70.5f, 163.5f, 71.5f, MemoryCardLabelTop);
    FillRectangle(canvas, placement, 130, 71.5f, 163.5f, 89.3f, CardLabel);
    FillRectangle(canvas, placement, 129.5f, 89.5f, 164, 96, MemoryCardLabelBand);

    constexpr float FirstDotX = 143.5f;
    constexpr float DotSpacing = 3;
    constexpr int DotCount = 7;
    for (int dot = 0; dot < DotCount; ++dot)
    {
        Point center = placement.Place({FirstDotX + DotSpacing * static_cast<float>(dot), 72.8f});
        VectorPath dotPath;
        dotPath.AddCircle(center, placement.Size(0.5f));
        canvas.Fill(dotPath, SolidPaint(MemoryCardDots));
    }

    SmallWordStyle maker{placement.Place({129.5f, 99}), placement.Size(2.6f), placement.Size(2), placement.Size(4.6f), placement.Size(0.6f),
        MemoryCardMakerWord};
    DrawSmallWord(canvas, "NINTENDO", maker);
    SmallWordStyle name{placement.Place({128.6f, 103}), placement.Size(3.2f), placement.Size(3.6f), placement.Size(4.55f),
        placement.Size(1), MemoryCardNameWord};
    DrawSmallWord(canvas, "GAMECUBE", name);
}

// ---- The channel: a tile with a dark rim, white lines across it and the word Wii.
// The places are those of the button of Channels.

constexpr Color ChannelRim = Gray(132);
constexpr Color ChannelInside = Gray(189);
constexpr Color ChannelLine = Gray(240);
constexpr Color ChannelWord = Gray(107);
constexpr float ChannelLeft = 90;
constexpr float ChannelTop = 59;
constexpr float ChannelRight = 163;
constexpr float ChannelBottom = 113.5f;
constexpr float ChannelRimWidth = 2.5f;
constexpr float FirstChannelLineY = 63;
constexpr float ChannelLineSpacing = 4;

void DrawChannel(VectorCanvas& canvas, const Placement& placement)
{
    FillRoundedRectangle(canvas, placement, ChannelLeft, ChannelTop, ChannelRight, ChannelBottom, 4.5f, ChannelRim);

    float insideLeft = ChannelLeft + ChannelRimWidth;
    float insideRight = ChannelRight - ChannelRimWidth;
    float insideBottom = ChannelBottom - ChannelRimWidth;
    FillRoundedRectangle(canvas, placement, insideLeft, ChannelTop + 3.5f, insideRight, insideBottom, 2, ChannelInside);
    for (float y = FirstChannelLineY; y < insideBottom; y += ChannelLineSpacing)
        FillRectangle(canvas, placement, insideLeft, y, insideRight, y + 1, ChannelLine);

    VectorPath word;
    AddWiiWord(word, placement.Place({99.5f, 72.4f}), placement.Size(0.8f));
    canvas.Fill(word, SolidPaint(ChannelWord));
}

constexpr Placement AtItsSize = {{0, 0}, 1};

// ---- The outlines that float around the channel on the button of Channels: thin rounded frames, drawn in a black so faint that the
// background shows through. The places are the middles of their lines.

struct FloatingFrame
{
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    Formats::Byte alpha = 0;
};

constexpr float FloatingFrameLineWidth = 1.3f;
constexpr float FloatingFrameCornerRadius = 4.5f;

// Behind the channel, then in front of it.
constexpr std::array FramesBehindTheChannel = {
    FloatingFrame{44, 26.1f, 72.5f, 46.5f, 31},
    FloatingFrame{82.3f, 20.8f, 128, 53.3f, 46},
    FloatingFrame{63.8f, 37.5f, 109.7f, 69.3f, 51},
    FloatingFrame{177.2f, 33.8f, 205.8f, 53.8f, 41},
    FloatingFrame{23.7f, 82.8f, 52.4f, 103.9f, 56},
    FloatingFrame{66.3f, 104, 95.6f, 124.2f, 38},
    FloatingFrame{179.3f, 77.3f, 225.2f, 110.7f, 66},
};
constexpr FloatingFrame FrameInFrontOfTheChannel = {148, 97.6f, 193.8f, 130, 66};

void DrawFloatingFrame(VectorCanvas& canvas, const FloatingFrame& frame)
{
    constexpr float HalfLine = FloatingFrameLineWidth / 2;
    float outerWidth = frame.right - frame.left + FloatingFrameLineWidth;
    float outerHeight = frame.bottom - frame.top + FloatingFrameLineWidth;

    VectorPath outline;
    outline.AddRoundedRectangle(frame.left - HalfLine, frame.top - HalfLine, outerWidth, outerHeight, FloatingFrameCornerRadius + HalfLine);
    outline.AddRoundedRectangle(frame.left + HalfLine, frame.top + HalfLine, outerWidth - 2 * FloatingFrameLineWidth,
        outerHeight - 2 * FloatingFrameLineWidth, FloatingFrameCornerRadius - HalfLine);
    canvas.Fill(outline, SolidPaint({0, 0, 0, frame.alpha}));
}

}

void DrawDataManagementButton(VectorCanvas& canvas)
{
    DrawBigOptionButtonFrame(canvas);
    DrawConsole(canvas, {{-12, 25}, 0.7273f});
    DrawMemoryCard(canvas, {{7.5f, 24.1f}, 0.74f});
    DrawSdCard(canvas, {{24, 22.5f}, 0.75f});
    DrawChannel(canvas, {{131.8f, 38.8f}, 0.58f});
}

void DrawSaveDataButton(VectorCanvas& canvas)
{
    constexpr float ConsoleShift = -28;
    constexpr float SdCardShift = 25;
    DrawBigOptionButtonFrame(canvas);
    DrawConsole(canvas, {{ConsoleShift, 0}, 1});
    DrawMemoryCard(canvas, AtItsSize);
    DrawSdCard(canvas, {{SdCardShift, 0}, 1});
}

void DrawWiiSaveDataButton(VectorCanvas& canvas)
{
    DrawBigOptionButtonFrame(canvas);
    DrawConsole(canvas, AtItsSize);
    DrawSdCard(canvas, AtItsSize);
}

void DrawChannelsButton(VectorCanvas& canvas)
{
    DrawBigOptionButtonFrame(canvas);
    for (const FloatingFrame& frame : FramesBehindTheChannel)
        DrawFloatingFrame(canvas, frame);
    DrawChannel(canvas, AtItsSize);
    DrawFloatingFrame(canvas, FrameInFrontOfTheChannel);
}

}
