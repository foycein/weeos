#pragma once

#include "SharpArt/VectorCanvas.h"

#include <string_view>

namespace SharpArt
{

// How a word of capital letters is written: where its first letter begins, how large every letter is, the room from one letter to
// the next, and the pen.
struct SmallWordStyle
{
    Point topLeft;
    float letterWidth = 0;
    float letterHeight = 0;
    float letterPitch = 0;
    float penWidth = 0;
    Color color = {0, 0, 0, 255};
};

// Writes a short word of capital letters a few units high with a round pen, as the tiny words printed on the pictures of the Wii
// (POWER, HOME, NINTENDO, GAMECUBE). M and W are half again as wide as the other letters. Knows A B C D E G H I M N O P R T U W.
void DrawSmallWord(VectorCanvas& canvas, std::string_view word, const SmallWordStyle& style);

}
