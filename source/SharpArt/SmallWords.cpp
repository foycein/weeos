#include "SharpArt/SmallWords.h"

#include <vector>

namespace SharpArt
{

namespace
{

constexpr float WideLetterFactor = 1.5f;

// The box of one letter; places in it are given as shares of its width and height.
struct LetterBox
{
    float left;
    float top;
    float width;
    float height;

    Point At(float shareX, float shareY) const
    {
        return {left + width * shareX, top + height * shareY};
    }
};

// Lines drawn one after the other with a round pen. Each line is filled on its own, so where two of them cross they do not cut a hole.
void DrawLines(VectorCanvas& canvas, const std::vector<Point>& points, const SmallWordStyle& style)
{
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        VectorPath line;
        line.AddStroke(points[index - 1], points[index], style.penWidth);
        canvas.Fill(line, SolidPaint(style.color));
    }
}

void DrawRing(VectorCanvas& canvas, const LetterBox& box, const SmallWordStyle& style)
{
    Point center = box.At(0.5f, 0.5f);
    float halfPen = style.penWidth / 2;
    VectorPath ring;
    ring.AddEllipse(center, box.width / 2 + halfPen, box.height / 2 + halfPen);
    ring.AddEllipse(center, box.width / 2 - halfPen, box.height / 2 - halfPen);
    canvas.Fill(ring, SolidPaint(style.color));
}

// An arc of the ellipse that fills the box, from one angle to the other in degrees (0 to the right, 90 down).
void DrawArc(VectorCanvas& canvas, const LetterBox& box, float startDegrees, float endDegrees, const SmallWordStyle& style)
{
    VectorPath arc;
    arc.ArcTo(box.At(0.5f, 0.5f), box.width / 2, box.height / 2, startDegrees, endDegrees);
    DrawLines(canvas, arc.Outlines().front(), style);
}

void DrawLetter(VectorCanvas& canvas, char letter, const LetterBox& box, const SmallWordStyle& style)
{
    auto at = [&box](float x, float y) { return box.At(x, y); };
    switch (letter)
    {
    case 'A':
        DrawLines(canvas, {at(0, 1), at(0.5f, 0), at(1, 1)}, style);
        DrawLines(canvas, {at(0.25f, 0.65f), at(0.75f, 0.65f)}, style);
        return;
    case 'B':
        DrawLines(canvas, {at(0, 0), at(0, 1), at(0.75f, 1), at(1, 0.75f), at(0.75f, 0.5f), at(0, 0.5f)}, style);
        DrawLines(canvas, {at(0, 0), at(0.7f, 0), at(0.9f, 0.25f), at(0.7f, 0.5f)}, style);
        return;
    case 'C':
        DrawArc(canvas, box, 45, 315, style);
        return;
    case 'D':
        DrawLines(canvas, {at(0, 0), at(0, 1), at(0.55f, 1), at(1, 0.7f), at(1, 0.3f), at(0.55f, 0), at(0, 0)}, style);
        return;
    case 'E':
        DrawLines(canvas, {at(1, 0), at(0, 0), at(0, 1), at(1, 1)}, style);
        DrawLines(canvas, {at(0, 0.5f), at(0.85f, 0.5f)}, style);
        return;
    case 'G':
        DrawArc(canvas, box, 0, 315, style);
        DrawLines(canvas, {at(0.55f, 0.5f), at(1, 0.5f)}, style);
        return;
    case 'H':
        DrawLines(canvas, {at(0, 0), at(0, 1)}, style);
        DrawLines(canvas, {at(1, 0), at(1, 1)}, style);
        DrawLines(canvas, {at(0, 0.5f), at(1, 0.5f)}, style);
        return;
    case 'I':
        DrawLines(canvas, {at(0.5f, 0), at(0.5f, 1)}, style);
        return;
    case 'M':
        DrawLines(canvas, {at(0, 1), at(0, 0), at(0.5f, 0.7f), at(1, 0), at(1, 1)}, style);
        return;
    case 'N':
        DrawLines(canvas, {at(0, 1), at(0, 0), at(1, 1), at(1, 0)}, style);
        return;
    case 'O':
        DrawRing(canvas, box, style);
        return;
    case 'P':
        DrawLines(canvas, {at(0, 1), at(0, 0), at(0.75f, 0), at(1, 0.25f), at(0.75f, 0.5f), at(0, 0.5f)}, style);
        return;
    case 'R':
        DrawLines(canvas, {at(0, 1), at(0, 0), at(0.75f, 0), at(1, 0.25f), at(0.75f, 0.5f), at(0, 0.5f)}, style);
        DrawLines(canvas, {at(0.5f, 0.5f), at(1, 1)}, style);
        return;
    case 'T':
        DrawLines(canvas, {at(0, 0), at(1, 0)}, style);
        DrawLines(canvas, {at(0.5f, 0), at(0.5f, 1)}, style);
        return;
    case 'U':
        DrawLines(canvas, {at(0, 0), at(0, 0.7f), at(0.3f, 1), at(0.7f, 1), at(1, 0.7f), at(1, 0)}, style);
        return;
    case 'W':
        DrawLines(canvas, {at(0, 0), at(0.25f, 1), at(0.5f, 0.2f), at(0.75f, 1), at(1, 0)}, style);
        return;
    default:
        return;
    }
}

bool IsWide(char letter)
{
    return letter == 'M' || letter == 'W';
}

}

void DrawSmallWord(VectorCanvas& canvas, std::string_view word, const SmallWordStyle& style)
{
    float left = style.topLeft.x;
    for (char letter : word)
    {
        float width = style.letterWidth;
        float advance = style.letterPitch;
        if (IsWide(letter))
        {
            float extraWidth = width * (WideLetterFactor - 1);
            width += extraWidth;
            advance += extraWidth;
        }

        DrawLetter(canvas, letter, {left, style.topLeft.y, width, style.letterHeight}, style);
        left += advance;
    }
}

}
