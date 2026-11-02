#include "SharpArt/WaitingMarkPictures.h"

#include <array>

namespace SharpArt
{

namespace
{

constexpr Color White = {255, 255, 255, 255};
constexpr float SquareSize = 6;

// The left or top edges of the three columns and of the three rows.
constexpr std::array<float, 3> SquareStarts = {3, 13, 23};

struct GridPlace
{
    std::size_t column = 0;
    std::size_t row = 0;
};

// The place of the gap in each step, clockwise from the top in the middle.
constexpr std::array<GridPlace, 8> GapOfEachStep = {
    GridPlace{1, 0},
    GridPlace{2, 0},
    GridPlace{2, 1},
    GridPlace{2, 2},
    GridPlace{1, 2},
    GridPlace{0, 2},
    GridPlace{0, 1},
    GridPlace{0, 0},
};

void DrawWaitingMark(VectorCanvas& canvas, std::size_t stepIndex)
{
    const GridPlace& gap = GapOfEachStep[stepIndex];

    VectorPath squares;
    for (std::size_t row = 0; row < SquareStarts.size(); ++row)
    {
        for (std::size_t column = 0; column < SquareStarts.size(); ++column)
        {
            if (column == gap.column && row == gap.row)
                continue;

            squares.AddRectangle(SquareStarts[column], SquareStarts[row], SquareSize, SquareSize);
        }
    }
    canvas.Fill(squares, SolidPaint(White));
}

}

void DrawWaitingMarkStepOne(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 0);
}

void DrawWaitingMarkStepTwo(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 1);
}

void DrawWaitingMarkStepThree(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 2);
}

void DrawWaitingMarkStepFour(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 3);
}

void DrawWaitingMarkStepFive(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 4);
}

void DrawWaitingMarkStepSix(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 5);
}

void DrawWaitingMarkStepSeven(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 6);
}

void DrawWaitingMarkStepEight(VectorCanvas& canvas)
{
    DrawWaitingMark(canvas, 7);
}

}
