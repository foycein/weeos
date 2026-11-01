#pragma once

#include <array>

namespace Mii
{

// Every eye and eyebrow picture is drawn turned by its own offset plus the rotation of the record, in steps of 1/32 of a turn.
// A rotation of (32 - offset) mod 32 shows the picture upright. Eye pictures 48 and 49 belong to the smile and surprise faces.
constexpr int RotationStepsPerTurn = 32;

constexpr std::array<int, 50> EyeRotationOffset = {
    29, 28, 28, 28, 29, 28, 28, 28, 29, 28, 28, 28, 28, 29, 29, 28, 28, 28, 29, 29, 28, 29, 28, 29, 29,
    28, 29, 28, 28, 29, 28, 28, 28, 29, 29, 29, 28, 28, 29, 29, 29, 28, 28, 29, 29, 29, 29, 29, 28, 28,
};

constexpr std::array<int, 24> EyebrowRotationOffset = {
    26, 26, 27, 25, 26, 25, 26, 25, 28, 25, 26, 24, 27, 27, 26, 26, 25, 25, 26, 26, 27, 26, 25, 27,
};

}
