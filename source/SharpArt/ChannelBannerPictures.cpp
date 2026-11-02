#include "SharpArt/ChannelBannerPictures.h"

#include <vector>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};

constexpr float LetterTop = 3.8f;
constexpr float LetterBottom = 45.2f;
constexpr float HumpCut = 7;
constexpr float NotchCut = 5;
constexpr float SmallCut = 0.6f;
constexpr float ValleyCut = 2.5f;

void FillLetterM(VectorCanvas& canvas)
{
    VectorPath letter;
    letter.AddPolygonWithRoundCorners({{{0, LetterTop}, HumpCut}, {{13.4f, LetterTop}, NotchCut}, {{20.3f, 23.8f}, SmallCut},
        {{27.6f, LetterTop}, NotchCut}, {{40.9f, LetterTop}, HumpCut}, {{40.9f, LetterBottom}, SmallCut}, {{30.1f, LetterBottom}, SmallCut},
        {{30.1f, 31}, 0}, {{29.6f, 31}, SmallCut}, {{25, 42.5f}, 1}, {{22.4f, LetterBottom}, ValleyCut}, {{18.2f, LetterBottom}, ValleyCut},
        {{15.7f, 42.5f}, 1}, {{10.6f, 30.3f}, SmallCut}, {{10.1f, 30.3f}, 0}, {{10.1f, LetterBottom}, SmallCut}, {{0, LetterBottom}, SmallCut}});
    canvas.Fill(letter, SolidPaint(White));
}

void FillLetterI(VectorCanvas& canvas, float stemLeft, float stemRight, Point dotCenter, float dotRadiusX, float dotRadiusY)
{
    constexpr float StemTop = 16.9f;
    constexpr float StemCornerRadius = 0.8f;

    VectorPath letter;
    letter.AddRoundedRectangle(stemLeft, StemTop, stemRight - stemLeft, LetterBottom - StemTop, StemCornerRadius);
    canvas.Fill(letter, SolidPaint(White));

    VectorPath dot;
    dot.AddEllipse(dotCenter, dotRadiusX, dotRadiusY);
    canvas.Fill(dot, SolidPaint(White));
}

// ---- The colors of the word Mii

constexpr Color LogoGround = {127, 156, 156, 255};
constexpr Color FaceColor = {123, 154, 156, 255};
constexpr Color LetterWhite = {252, 253, 255, 255};
constexpr Color LetterPaleGreen = {244, 249, 252, 255};
constexpr Color LetterLightGreen = {225, 241, 239, 255};
constexpr Color LetterGreen = {189, 225, 222, 255};
constexpr float ColoredLetterTop = 6;
constexpr float ColoredLetterBottom = 42.6f;
constexpr float ColoredStemTop = 18.3f;

Paint LetterGradient()
{
    Paint paint;
    paint.kind = PaintKind::LinearGradient;
    paint.start = {0, 18};
    paint.end = {0, 41};
    paint.stops = {{0, LetterWhite}, {0.26f, LetterPaleGreen}, {0.61f, LetterLightGreen}, {1, LetterGreen}};
    return paint;
}

// The legs and the slanted strokes of the M are parted by thin lines that open into the notches at the bottom.
void FillColoredLetterM(VectorCanvas& canvas)
{
    constexpr float SlitTop = 17.5f;

    VectorPath letter;
    letter.AddPolygonWithRoundCorners({{{1.5f, ColoredLetterTop}, 6}, {{13, ColoredLetterTop}, 5}, {{20, 29.6f}, 0.5f},
        {{27.2f, ColoredLetterTop}, 5}, {{38.6f, ColoredLetterTop}, 6}, {{38.6f, ColoredLetterBottom}, 0.5f}, {{31.3f, ColoredLetterBottom}, 0},
        {{31.3f, SlitTop}, 0}, {{31, SlitTop}, 0}, {{21.6f, ColoredLetterBottom}, 1.2f}, {{18.5f, ColoredLetterBottom}, 1.2f},
        {{9.2f, SlitTop}, 0}, {{8.9f, SlitTop}, 0}, {{8.9f, ColoredLetterBottom}, 0}, {{1.5f, ColoredLetterBottom}, 0.5f}});
    canvas.Fill(letter, LetterGradient());
}

void FillColoredLetterI(VectorCanvas& canvas, float stemLeft, float stemRight, Point dotCenter)
{
    constexpr float DotRadius = 5;

    VectorPath stem;
    stem.AddRoundedRectangle(stemLeft, ColoredStemTop, stemRight - stemLeft, ColoredLetterBottom - ColoredStemTop, 0.5f);
    canvas.Fill(stem, LetterGradient());

    VectorPath dot;
    dot.AddCircle(dotCenter, DotRadius);
    canvas.Fill(dot, SolidPaint(LetterWhite));
}

void FillEye(VectorCanvas& canvas, Point center)
{
    constexpr float EyeRadiusX = 0.6f;
    constexpr float EyeRadiusY = 1;

    VectorPath eye;
    eye.AddEllipse(center, EyeRadiusX, EyeRadiusY);
    canvas.Fill(eye, SolidPaint(FaceColor));
}

// The smile is the lower half of a small oval.
void FillSmile(VectorCanvas& canvas, Point center, float radiusX, float radiusY)
{
    constexpr float RightDegrees = 0;
    constexpr float LeftDegrees = 180;

    VectorPath smile;
    smile.ArcTo(center, radiusX, radiusY, RightDegrees, LeftDegrees);
    smile.Close();
    canvas.Fill(smile, SolidPaint(FaceColor));
}

}

void DrawMiiLogoShape(VectorCanvas& canvas)
{
    FillLetterM(canvas);
    FillLetterI(canvas, 45, 55.1f, {50.1f, 8.55f}, 7, 6.55f);
    FillLetterI(canvas, 59.9f, 70, {64.95f, 8.8f}, 7.05f, 6.8f);
}

void DrawMiiLogoColors(VectorCanvas& canvas)
{
    VectorPath ground;
    ground.AddRectangle(0, 0, static_cast<float>(canvas.UnitWidth()), static_cast<float>(canvas.UnitHeight()));
    canvas.Fill(ground, SolidPaint(LogoGround));

    FillColoredLetterM(canvas);
    FillColoredLetterI(canvas, 47, 53.3f, {50.5f, 8.6f});
    FillColoredLetterI(canvas, 61.6f, 68, {65, 8.6f});

    FillEye(canvas, {48.3f, 7.3f});
    FillEye(canvas, {51.6f, 7.3f});
    FillSmile(canvas, {50.6f, 10.1f}, 1.3f, 1.3f);
    FillEye(canvas, {63.3f, 6.8f});
    FillEye(canvas, {66.6f, 6.8f});
    FillSmile(canvas, {65, 10.8f}, 2, 1.4f);
}

}
