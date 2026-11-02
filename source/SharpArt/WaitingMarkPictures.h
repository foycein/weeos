#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The eight steps of the waiting mark (32 by 32): nine white squares in three rows, one of them missing. Step by step the gap goes
// around clockwise, starting at the top in the middle.
void DrawWaitingMarkStepOne(VectorCanvas& canvas);
void DrawWaitingMarkStepTwo(VectorCanvas& canvas);
void DrawWaitingMarkStepThree(VectorCanvas& canvas);
void DrawWaitingMarkStepFour(VectorCanvas& canvas);
void DrawWaitingMarkStepFive(VectorCanvas& canvas);
void DrawWaitingMarkStepSix(VectorCanvas& canvas);
void DrawWaitingMarkStepSeven(VectorCanvas& canvas);
void DrawWaitingMarkStepEight(VectorCanvas& canvas);

}
