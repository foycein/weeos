#include "Mii/MiiExpression.h"

#include "Mii/MiiRotationOffsets.h"

#include <algorithm>

namespace Mii
{

namespace
{

constexpr int SmileEyes = 48;
constexpr int SurpriseEyes = 49;
constexpr int ClosedEyes = 26;
constexpr int AngryMouth = 10;
constexpr int SadMouth = 12;
constexpr int OpenMouth = 24;

constexpr int ExpressionRotationSteps = 2;
constexpr int SurpriseEyebrowRise = 2;

constexpr int HighestEyeRotation = EyeRotationCount - 1;
constexpr int HighestEyebrowRotation = EyebrowRotationCount - 1;

// Other eyes keep the angle at which the old ones were drawn.
void ChangeEyes(MiiEyes& eyes, int newType)
{
    int rotation = EyeRotationOffset[eyes.type] - EyeRotationOffset[newType] + eyes.rotation;
    eyes.rotation = std::clamp(rotation, 0, HighestEyeRotation);
    eyes.type = newType;
}

}

MiiRecord ApplyExpression(MiiRecord record, MiiExpression expression)
{
    switch (expression)
    {
    case MiiExpression::Normal:
        break;
    case MiiExpression::Smile:
        ChangeEyes(record.eyes, SmileEyes);
        break;
    case MiiExpression::Anger:
        record.eyebrows.rotation = std::min(record.eyebrows.rotation + ExpressionRotationSteps, HighestEyebrowRotation);
        record.eyes.rotation = std::min(record.eyes.rotation + ExpressionRotationSteps, HighestEyeRotation);
        record.mouth.type = AngryMouth;
        break;
    case MiiExpression::Sorrow:
        record.eyebrows.rotation = std::max(record.eyebrows.rotation - ExpressionRotationSteps, 0);
        record.eyes.rotation = std::max(record.eyes.rotation - ExpressionRotationSteps, 0);
        record.mouth.type = SadMouth;
        break;
    case MiiExpression::Surprise:
        ChangeEyes(record.eyes, SurpriseEyes);
        record.eyebrows.vertical -= SurpriseEyebrowRise;
        break;
    case MiiExpression::Blink:
        ChangeEyes(record.eyes, ClosedEyes);
        break;
    case MiiExpression::OpenMouth:
        record.mouth.type = OpenMouth;
        break;
    }
    return record;
}

}
