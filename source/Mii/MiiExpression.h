#pragma once

#include "Mii/MiiRecord.h"

namespace Mii
{

// The faces a Mii can make. They only change the face texture: other eyes, eyebrows or mouth.
enum class MiiExpression
{
    Normal,
    Smile,
    Anger,
    Sorrow,
    Surprise,
    Blink,
    OpenMouth,
};

// The record whose face texture shows the expression. Its eye 48 or 49 and mouth 24 exist only for expressions.
MiiRecord ApplyExpression(MiiRecord record, MiiExpression expression);

}
