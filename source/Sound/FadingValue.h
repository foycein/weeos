#pragma once

namespace Sound
{

// A number that moves in a straight line to a new value, one sound frame at a time, as the volume of a sound does when it fades.
// It starts at 1.
class FadingValue
{
public:
    // Moves from the value it has now to the target in that many sound frames. With 0 frames it is there at once.
    void MoveTo(float targetValue, int frameCount)
    {
        target = targetValue;
        framesLeft = frameCount;
        if (framesLeft <= 0)
            value = target;
    }

    // One sound frame passes. Each frame covers an equal part of the way that is left, so the steps are equal, and the last frame
    // ends exactly at the target.
    void Advance()
    {
        if (framesLeft <= 0)
            return;

        value += (target - value) / float(framesLeft);
        --framesLeft;
        if (framesLeft == 0)
            value = target;
    }

    float Value() const
    {
        return value;
    }

    bool HasArrived() const
    {
        return framesLeft <= 0;
    }

private:
    float value = 1.0f;
    float target = 1.0f;
    int framesLeft = 0;
};

}
