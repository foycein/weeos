#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The hands of the pointer (64 by 64): a white hand with a thick black edge, pointing, grabbing or open.
void DrawPointingHand(VectorCanvas& canvas);
void DrawGrabbingHand(VectorCanvas& canvas);
void DrawOpenHand(VectorCanvas& canvas);

// The shading of the hand of each player, an intensity picture laid over the hand: white, darker towards the wrist, with the number
// of the player cut out where the palm is (64 by 64). The picture without a number is 32 by 32.
void DrawHandShadeWithoutNumber(VectorCanvas& canvas);
void DrawHandShadeOfPlayerOne(VectorCanvas& canvas);
void DrawHandShadeOfPlayerTwo(VectorCanvas& canvas);
void DrawHandShadeOfPlayerThree(VectorCanvas& canvas);
void DrawHandShadeOfPlayerFour(VectorCanvas& canvas);

}
